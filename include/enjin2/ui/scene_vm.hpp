#pragma once

#include "components.hpp"
#include "easing.hpp"
#include "factory.hpp"
#include "json.hpp"
#include "scene_json.hpp"
#include "slot.hpp"
#include "widgets/sprite.hpp" // SpriteComponent::frameCount for the map value->frame slot (#220)
#include "world.hpp"
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

/**
 * @file scene_vm.hpp
 * @brief Behavior-as-data interpreter for scene documents (unwn #183, M2)
 *
 * The C++ implementation of the ratified interpreter spec
 * (`eisei/tools/scene_data_prototype/scene_vm.py`, unwn #173): event→action
 * tables with guards, timers, animation tracks — deliberately **not** a
 * language. Everything Turing-complete stays in C++ behind two seams the VM
 * only *sequences*:
 *
 *  - **named host effects** — `{"host": "datum.load", "args": {...}}` emits a
 *    @ref SceneEffect the caller (UIManager, the preview session runner, the
 *    editor) executes; host completions come back as `dispatch()` events with
 *    prepared payloads (`host.listArrived` carrying `names` + `loadedIndex`).
 *  - **widget verbs** — `{"call": "presetList.moveUp"}` routes through
 *    @ref callWidgetVerb into compiled widget API.
 *
 * The VM interprets the SceneDoc's DOM subtrees directly (the Python spec
 * interprets the document dict the same way) and is tolerant like the loader:
 * unknown action verbs, missing timers/animations, dangling entity ids and
 * malformed rules are skipped, never fatal.
 *
 * Runtime state (state vars, armed timers, playing animations, virtual time)
 * lives in the VM, not the document — reloading the same document always
 * yields a fresh scene, the re-activation reset the prototype proved by
 * construction.
 *
 * Paths: `name` is a state var; `entity.prop` is a property on the entity
 * with that @ref IdComponent id. On a widget entity `prop` matches a reflected
 * field name, searched across the entity's components in world pack order
 * (first match wins); on a `cpp:` slot entity it addresses the
 * @ref SlotComponent property bag.
 */

namespace enjin2 {

/// @brief One host command (or scene switch) emitted by the scene.
struct SceneEffect {
    enum class Kind { Host, SceneSwitch };
    Kind kind = Kind::Host;
    std::string name; ///< Host effect name, or the target scene name
    JsonValue args;   ///< Resolved args object (Host only)
};

using SceneEffects = std::vector<SceneEffect>;

/**
 * @brief App-supplied *resolve-raw* for the `param.` namespace (unwn #202, #218).
 *
 * The engine stays ignorant of the concrete ParamRegistry + formatter catalog
 * (which live app-side, in unwnlib): given the key *after* the `param.` prefix
 * (e.g. `daisy.key`), the resolver reads the live-value cell and returns the
 * **raw value plus its normalization domain** — a `{number, min, max}` object
 * @ref JsonValue — or Null for an unknown key (the tolerant miss: the bound
 * property keeps its default). The engine owns the pipeline (`raw → ease → map
 * → format`); the app stays schema owner of the number and its domain.
 *
 * Split from the pre-#218 single `resolveParamBinding` (which returned a
 * pre-formatted String): resolve-raw yields the domain the value chain needs
 * (P3 value→frame, P4 easing), and @ref ParamFormatter renders the terminal
 * string separately.
 *
 * Same app-agnostic-hook shape as the schema `extraSections` callback (#201):
 * the engine only knows a resolver *may* be installed. Unset ⇒ `param.` reads
 * Null everywhere (guards, `@`-refs and bindings alike stay tolerant).
 */
using ParamResolver = std::function<JsonValue(const std::string& key)>;

/**
 * @brief App-supplied *formatter* — the value chain's terminal stage (unwn #218).
 *
 * Invoked only at the `format` terminal, for a textual target: given the key
 * (after `param.`), the binding's optional `format` override ("" = the
 * descriptor default) and the raw @p number the pipeline carried end-to-end,
 * returns the display String. The effective formatter is the app's business:
 * `binding.format ?? descriptor.format ?? raw`. Unset ⇒ the format terminal
 * yields "" (an empty display), matching the tolerant miss.
 */
using ParamFormatter =
    std::function<std::string(const std::string& key, const std::string& format, double number)>;

namespace detail {

/// @brief Schema easing names → engine curves. Unknown names fall back to linear.
inline EasingFunction easingByName(const std::string& name) {
    struct Entry {
        const char* name;
        EasingFunction fn;
    };
    static constexpr Entry kTable[] = {
        {"linear", &Easing::Linear},
        {"step", &Easing::Step},
        {"inQuad", &Easing::EaseInQuad},
        {"outQuad", &Easing::EaseOutQuad},
        {"inOutQuad", &Easing::EaseInOutQuad},
        {"inCubic", &Easing::EaseInCubic},
        {"outCubic", &Easing::EaseOutCubic},
        {"inOutCubic", &Easing::EaseInOutCubic},
        {"inQuart", &Easing::EaseInQuart},
        {"outQuart", &Easing::EaseOutQuart},
        {"inSine", &Easing::EaseInSine},
        {"inOutSine", &Easing::EaseInOutSine},
        {"inOutQuint", &Easing::EaseInOutQuint},
        {"inOutCirc", &Easing::EaseInOutCirc},
        {"inOutElastic", &Easing::EaseInOutElastic},
    };
    for (const Entry& e : kTable)
        if (name == e.name) return e.fn;
    return &Easing::Linear;
}

/// @brief Set (or insert) @p key on a DOM object, preserving member order.
inline void objectSet(JsonValue& obj, const std::string& key, JsonValue value) {
    if (obj.type != JsonValue::Type::Object) {
        obj = JsonValue{};
        obj.type = JsonValue::Type::Object;
    }
    for (auto& kv : obj.object) {
        if (kv.first == key) {
            kv.second = std::move(value);
            return;
        }
    }
    obj.object.emplace_back(key, std::move(value));
}

} // namespace detail

/**
 * @brief Owns runtime state for one loaded scene document
 * @tparam TWorld A World composing at least IdComponent (BindingsComponent
 *                and SlotComponent are used when composed, skipped when not)
 *
 * dispatch()/tick() return the host effects the scene emitted; the caller
 * executes them. The document and world are borrowed, not owned: the world is
 * the same one the widget systems render, so behavior writes are visible the
 * next frame with no sync step.
 */
template<typename TWorld>
class SceneVM {
public:
    /**
     * @brief Bind a VM to a loaded document + world
     *
     * Copies the document's initial state vars into VM runtime, indexes
     * entities by IdComponent id, and applies bindings once so bound
     * properties are consistent before the first event.
     */
    SceneVM(const SceneDoc* doc, TWorld* world, const AssetRegistry* assets,
            ParamResolver paramResolver = {}, ParamFormatter paramFormatter = {})
        : doc_(doc), world_(world), assets_(assets),
          paramResolver_(std::move(paramResolver)),
          paramFormatter_(std::move(paramFormatter)) {
        if (doc_ && doc_->state.type == JsonValue::Type::Object) vars_ = doc_->state;
        else vars_.type = JsonValue::Type::Object;
        if constexpr (TWorld::template composes<IdComponent>()) {
            if (world_) {
                const auto& ids = world_->template components<IdComponent>();
                for (size_t i = 0; i < ids.size(); ++i) {
                    const Entity e = ids.entityAt(i);
                    if (const IdComponent* c = world_->template get<IdComponent>(e))
                        entities_.emplace_back(c->id, e);
                }
            }
        }
        applyBindings();
    }

    /// @brief Feed one event (input, lifecycle, or host completion) through the table.
    SceneEffects dispatch(const std::string& event, const JsonValue* payload = nullptr) {
        SceneEffects effects;
        if (doc_) {
            if (const JsonValue* rules = doc_->on.find(event.c_str()))
                runRules(*rules, payload, effects);
        }
        applyBindings();
        return effects;
    }

    /// @brief Advance virtual time: expire timers (running their rules), step animations.
    SceneEffects tick(double dtMs) {
        SceneEffects effects;
        timeMs_ += dtMs;

        // Per the interpreter spec: walk a snapshot of the armed names in arm
        // order, decrementing lazily — so an earlier onExpire that (re)starts
        // a not-yet-visited timer leaves it aged by this tick's dt, and one
        // that cancels it removes it entirely (a skip here; the Python spec
        // never exercises that case). Timers armed mid-walk aren't in the
        // snapshot and first age next tick.
        std::vector<std::string> armed;
        armed.reserve(timers_.size());
        for (const auto& t : timers_) armed.push_back(t.first);
        for (const std::string& name : armed) {
            const int idx = timerIndex(name);
            if (idx < 0) continue; // cancelled by an earlier onExpire
            double& remaining = timers_[static_cast<size_t>(idx)].second;
            remaining -= dtMs;
            if (remaining > 0.0) continue;
            const JsonValue* spec = doc_ ? doc_->timers.find(name.c_str()) : nullptr;
            const bool repeat = spec && spec->find("repeat") && spec->find("repeat")->truthy();
            if (repeat) {
                const JsonValue* ms = spec->find("ms");
                setTimer(name, ms && ms->type == JsonValue::Type::Number ? ms->number : 0.0);
            } else {
                cancelTimer(name);
            }
            if (spec) {
                if (const JsonValue* rules = spec->find("onExpire"))
                    runRules(*rules, nullptr, effects);
            }
        }

        // Animations: piecewise tracks, each easing from→to over its own ms.
        std::vector<std::string> finished;
        for (auto& a : anims_) {
            a.second += dtMs;
            const JsonValue* spec =
                doc_ ? doc_->animations.find(a.first.c_str()) : nullptr;
            const JsonValue* tracks = spec ? spec->find("tracks") : nullptr;
            if (!tracks || tracks->type != JsonValue::Type::Array) {
                finished.push_back(a.first);
                continue;
            }
            bool done = true;
            for (const JsonValue& track : tracks->array) {
                const JsonValue* target = track.find("target");
                const JsonValue* from = track.find("from");
                const JsonValue* to = track.find("to");
                const JsonValue* ms = track.find("ms");
                if (!target || target->type != JsonValue::Type::String || !from || !to ||
                    !ms || ms->type != JsonValue::Type::Number || ms->number <= 0.0)
                    continue;
                const double t = std::min(1.0, a.second / ms->number);
                const double eased = static_cast<double>(
                    detail::easingByName(easingOf(track))(static_cast<float>(t)));
                JsonValue v;
                v.type = JsonValue::Type::Number;
                // round(value, 2) per the interpreter spec — quantizing also
                // keeps sub-cent libm easing differences out of the fields the
                // CI-parity goldens (#184) will compare across native/WASM.
                v.number = std::round(
                               (from->number + (to->number - from->number) * eased) * 100.0) /
                           100.0;
                setPath(target->str, v);
                if (t < 1.0) done = false;
            }
            if (done) finished.push_back(a.first);
        }
        for (const std::string& name : finished) stopAnim(name);

        applyBindings();
        return effects;
    }

    // ---- inspection (tests, editor state pane) ----------------------------

    /// @brief Resolve a path to a value; Null when absent.
    ///
    /// Three sources, in order: the `param.` namespace (a live-value cell via the
    /// installed @ref ParamResolver, formatted with the descriptor's default —
    /// bindings pass an override through applyBindings, unwn #202), then an
    /// `entity.prop` reflected field, then a bare state var.
    JsonValue lookup(const std::string& path) const {
        if (isParamPath(path))
            return resolveParamFormatted(path.substr(kParamPrefixLen), std::string());
        Entity e;
        std::string prop;
        if (splitEntityPath(path, e, prop)) return getEntityProp(e, prop);
        if (const JsonValue* v = vars_.find(path.c_str())) return *v;
        return JsonValue{};
    }

    /// @brief Milliseconds remaining on an armed timer; < 0 when not armed.
    double timerRemaining(const std::string& name) const {
        const int idx = timerIndex(name);
        return idx < 0 ? -1.0 : timers_[static_cast<size_t>(idx)].second;
    }

    /// @brief Whether an animation is still playing.
    bool animPlaying(const std::string& name) const {
        for (const auto& a : anims_)
            if (a.first == name) return true;
        return false;
    }

    /// @brief Virtual time accumulated by tick(), in ms.
    double timeMs() const { return timeMs_; }

    /// @brief Count of seeded transient easing anchors (unwn #218 seam; tests).
    size_t easingStateCount() const { return easingState_.size(); }

private:
    const SceneDoc* doc_;
    TWorld* world_;
    const AssetRegistry* assets_;
    ParamResolver paramResolver_;                           ///< `param.` resolve-raw (unwn #202/#218); may be unset
    ParamFormatter paramFormatter_;                         ///< format terminal (unwn #218); may be unset
    JsonValue vars_;                                        ///< Runtime state vars (object)
    std::vector<std::pair<std::string, Entity>> entities_;  ///< id → entity, file order
    std::vector<std::pair<std::string, double>> timers_;    ///< name → remaining ms, arm order
    std::vector<std::pair<std::string, double>> anims_;     ///< name → elapsed ms, play order
    double timeMs_ = 0.0;

    /// @brief Transient tween anchor for a bound property (unwn #214/#218/#221, P4).
    ///
    /// The bound-value easing (P4) tweens on change from the *last resolved*
    /// value; this side table holds that anchor, keyed by `(entity, property)`,
    /// seeded from the first resolved numeric value. Runtime-only — lives beside
    /// @ref timers_ / @ref anims_, **never** serialized (a save reflects the
    /// world, not the VM), so a reload always yields a fresh scene.
    ///
    /// `value` is the current eased output (also the P1 seed); a tween-on-change
    /// glides `from → target` over `startMs .. startMs+ms` on the VM clock
    /// (@ref timeMs_), re-anchoring `from` to the current `value` whenever
    /// `target` moves. Time-based off the dt fed to @ref tick, so the two hosts
    /// (16 ms / 33 ms) share one trajectory.
    struct EasingAnchor {
        Entity entity;
        std::string prop;
        double value;   ///< current eased output
        double from;    ///< tween start value
        double target;  ///< tween destination (last resolved raw)
        double startMs; ///< timeMs_ at the last (re-)anchor
    };
    std::vector<EasingAnchor> easingState_;

    // ---- entity property access (the reflection seam) ----------------------

    int timerIndex(const std::string& name) const {
        for (size_t i = 0; i < timers_.size(); ++i)
            if (timers_[i].first == name) return static_cast<int>(i);
        return -1;
    }

    // ---- the `param.` binding source (unwn #202) ---------------------------

    static constexpr char kParamPrefix[] = "param.";
    static constexpr size_t kParamPrefixLen = sizeof(kParamPrefix) - 1;

    /// @brief Default ease duration (ms) — the shorthand curve form and an
    /// `{curve}` with no `ms` both seed it (unwn #221). Must match the editor's
    /// `DEFAULT_EASE_MS` so a shorthand round-trips identically across the seam.
    static constexpr double kDefaultEaseMs = 200.0;

    /// @brief Whether @p path addresses the `param.` namespace.
    static bool isParamPath(const std::string& path) {
        return path.compare(0, kParamPrefixLen, kParamPrefix) == 0;
    }

    /// @brief Resolve-raw `param.<key>` → `{number, min, max}` (Null if unset /
    /// unknown key — the tolerant miss). The value chain's first stage; carries
    /// the raw Number end-to-end plus the normalization domain P3/P4 consume.
    JsonValue resolveParamRaw(const std::string& key) const {
        if (paramResolver_) return paramResolver_(key);
        return JsonValue{};
    }

    /// @brief The raw `number` field out of a resolve-raw `{number,min,max}`.
    static double paramNumber(const JsonValue& raw) {
        const JsonValue* n = raw.find("number");
        return n && n->type == JsonValue::Type::Number ? n->number : 0.0;
    }

    /// @brief The format terminal: render `param.<key>`'s raw @p number to its
    /// display String via the injected formatter (empty if unset).
    std::string formatParam(const std::string& key, const std::string& format,
                            double number) const {
        if (paramFormatter_) return paramFormatter_(key, format, number);
        return std::string();
    }

    /// @brief Compose resolve-raw + format into the pre-#218 formatted value: a
    /// String @ref JsonValue for a known key, Null for the tolerant miss. Used by
    /// @ref lookup so guards / `@`-refs read a param exactly as before the split.
    JsonValue resolveParamFormatted(const std::string& key, const std::string& format) const {
        const JsonValue raw = resolveParamRaw(key);
        if (raw.type == JsonValue::Type::Null) return JsonValue{};
        JsonValue out;
        out.type = JsonValue::Type::String;
        out.str = formatParam(key, format, paramNumber(raw));
        return out;
    }

    /// @brief Read a **value-chain** binding's source (unwn #202/#218): a bare
    /// string is `{from: "<string>"}` shorthand; an object supplies `from`
    /// (required string) plus the reserved `map` / `ease` / `format` slots
    /// (P3/P4/P5) — only `format` is read here, `map`/`ease` are reserved and
    /// pass the raw value straight through until their leaf tickets fill them.
    /// False for a malformed binding (skipped, the tolerant-reader contract
    /// extends to bindings). The condition form (`op` present) never reaches here.
    static bool bindingSource(const JsonValue& v, std::string& from, std::string& format) {
        format.clear();
        if (v.type == JsonValue::Type::String) {
            from = v.str;
            return true;
        }
        if (v.type == JsonValue::Type::Object) {
            const JsonValue* f = v.find("from");
            if (!f || f->type != JsonValue::Type::String) return false;
            from = f->str;
            const JsonValue* fmt = v.find("format");
            if (fmt && fmt->type == JsonValue::Type::String) format = fmt->str;
            return true;
        }
        return false;
    }

    bool findEntity(const std::string& id, Entity& out) const {
        for (const auto& kv : entities_) {
            if (kv.first == id) {
                out = kv.second;
                return true;
            }
        }
        return false;
    }

    /// @brief `entity.prop` → (entity, prop). False for bare names / unknown ids.
    bool splitEntityPath(const std::string& path, Entity& e, std::string& prop) const {
        const size_t dot = path.find('.');
        if (dot == std::string::npos) return false;
        if (!findEntity(path.substr(0, dot), e)) return false;
        prop = path.substr(dot + 1);
        return true;
    }

    JsonValue getEntityProp(Entity e, const std::string& prop) const {
        JsonValue out;
        if constexpr (TWorld::template composes<SlotComponent>()) {
            if (const SlotComponent* slot = world_->template get<SlotComponent>(e)) {
                if (const JsonValue* v = slot->props.find(prop.c_str())) return *v;
                return out; // slot props are the bag, not reflected fields
            }
        }
        bool found = false;
        TWorld::forEachComponentType([&](auto tag) {
            using C = typename decltype(tag)::type;
            if constexpr (IsReflected<C>::value) {
                if (found) return;
                if (const C* c = world_->template get<C>(e)) {
                    ComponentTraits<C>::visitFields(*c, [&](const char* name, auto acc) {
                        if (!found && prop == name) {
                            out = detail::fieldToJson(acc.get(), *assets_);
                            found = true;
                        }
                    });
                }
            }
        });
        return out;
    }

    void setEntityProp(Entity e, const std::string& prop, const JsonValue& value) {
        if constexpr (TWorld::template composes<SlotComponent>()) {
            if (SlotComponent* slot = world_->template get<SlotComponent>(e)) {
                detail::objectSet(slot->props, prop, value);
                return;
            }
        }
        bool done = false;
        TWorld::forEachComponentType([&](auto tag) {
            using C = typename decltype(tag)::type;
            if constexpr (IsReflected<C>::value) {
                if (done) return;
                if (C* c = world_->template get<C>(e)) {
                    ComponentTraits<C>::visitFields(*c, [&](const char* name, auto acc) {
                        if (!done && prop == name) {
                            using ValueT = typename decltype(acc)::value_type;
                            ValueT tmp = acc.get();
                            if (detail::readFieldValue(value, tmp, *assets_)) acc.set(tmp);
                            done = true;
                        }
                    });
                }
            }
        });
    }

    void setPath(const std::string& path, const JsonValue& value) {
        Entity e;
        std::string prop;
        if (splitEntityPath(path, e, prop)) {
            setEntityProp(e, prop, value);
            return;
        }
        detail::objectSet(vars_, path, value);
    }

    // ---- reading -----------------------------------------------------------

    /// @brief Resolve @-references in action args: @event.x, @state.x, @entity.prop.
    JsonValue resolve(const JsonValue& value, const JsonValue* payload) const {
        if (value.type == JsonValue::Type::Object) {
            JsonValue out;
            out.type = JsonValue::Type::Object;
            for (const auto& kv : value.object)
                out.object.emplace_back(kv.first, resolve(kv.second, payload));
            return out;
        }
        if (value.type == JsonValue::Type::Array) {
            JsonValue out;
            out.type = JsonValue::Type::Array;
            for (const JsonValue& item : value.array)
                out.array.push_back(resolve(item, payload));
            return out;
        }
        if (value.type == JsonValue::Type::String && !value.str.empty() &&
            value.str[0] == '@') {
            const std::string path = value.str.substr(1);
            if (path.rfind("event.", 0) == 0) {
                if (payload)
                    if (const JsonValue* v = payload->find(path.c_str() + 6)) return *v;
                return JsonValue{};
            }
            if (path.rfind("state.", 0) == 0) {
                if (const JsonValue* v = vars_.find(path.c_str() + 6)) return *v;
                return JsonValue{};
            }
            return lookup(path);
        }
        return value;
    }

    /**
     * @brief Tiny guard grammar: atoms (state vars / entity.prop), '!', '&&', '||'
     *
     * Deliberately not a language — if a guard can't be said here, it's C++'s
     * job (the ratified C++/data boundary).
     */
    bool evalCond(const std::string& expr) const {
        size_t orStart = 0;
        while (orStart <= expr.size()) {
            size_t orEnd = expr.find("||", orStart);
            if (orEnd == std::string::npos) orEnd = expr.size();
            const std::string orTerm = expr.substr(orStart, orEnd - orStart);

            bool ok = true;
            size_t andStart = 0;
            while (andStart <= orTerm.size()) {
                size_t andEnd = orTerm.find("&&", andStart);
                if (andEnd == std::string::npos) andEnd = orTerm.size();
                std::string atom = trim(orTerm.substr(andStart, andEnd - andStart));
                bool neg = false;
                while (!atom.empty() && atom[0] == '!') {
                    neg = !neg;
                    atom = trim(atom.substr(1));
                }
                bool val = lookup(atom).truthy();
                if (neg) val = !val;
                if (!val) {
                    ok = false;
                    break;
                }
                andStart = andEnd + 2;
            }
            if (ok) return true;
            orStart = orEnd + 2;
        }
        return false;
    }

    static std::string trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
        return s.substr(a, b - a);
    }

    // ---- writing -----------------------------------------------------------

    void applyBindings() {
        if (!world_) return;
        if constexpr (TWorld::template composes<BindingsComponent>()) {
            const auto& storage = world_->template components<BindingsComponent>();
            for (size_t i = 0; i < storage.size(); ++i) {
                const Entity e = storage.entityAt(i);
                const BindingsComponent* b = world_->template get<BindingsComponent>(e);
                if (!b || b->bindings.type != JsonValue::Type::Object) continue;
                for (const auto& kv : b->bindings.object) applyBinding(e, kv.first, kv.second);
            }
        }
    }

    /// @brief One binding on one property, dispatched on the `op` discriminant
    /// (unwn #218). An object carrying `op` is the **condition form**
    /// `{from, op, threshold, then?, else?}` (unwn #213, P5); everything else —
    /// a bare string or `{from, map?, ease?, format?}` — is the **value chain**
    /// (unwn #212/#214, P3/P4). The two are mutually exclusive. Uniform
    /// tolerant-miss: a miss at any stage leaves the property at its default.
    ///
    /// The pre-#218 hardcoded `visible` property-name fork retired here: `visible`
    /// is now an ordinary bool value-chain target (a bare `visible: someVar`
    /// passes the resolved bool straight through), and a genuine single comparison
    /// is written as the condition form. Compound `&&`/`||`/`!` guards stay in the
    /// behavior-rule `if` grammar (@ref evalCond), untouched by bindings.
    void applyBinding(Entity e, const std::string& prop, const JsonValue& binding) {
        if (binding.type == JsonValue::Type::Object && binding.find("op"))
            applyConditionBinding(e, prop, binding);
        else
            applyValueChain(e, prop, binding);
    }

    /// @brief Value chain: `raw → ease → map → format` (hard order, unwn #218).
    /// The `ease` slot (P4, #221) tweens the raw number on change; the `map` slot
    /// (P3, #220) quantizes it to a sprite frame; the `format` terminal (P5)
    /// renders a display String — the path the pre-#218 `param.` source took.
    /// Empty slots pass the raw value straight through. Any miss at any stage
    /// (malformed binding, unknown `from`, no sprite for `map`) is a no-op: the
    /// property keeps its authored default.
    ///
    /// The `ease`/`map` transforms are numeric, so they engage only when the
    /// resolved value is a Number; a bool/enum/string source (a state var) passes
    /// through un-transformed. Adding an `ease` or `map` slot also opts the
    /// terminal into a **numeric** write — a bare `param.` binding with neither
    /// slot still formats to a String, byte-identically to before.
    void applyValueChain(Entity e, const std::string& prop, const JsonValue& binding) {
        std::string from, format;
        if (!bindingSource(binding, from, format)) return; // malformed → tolerant miss

        std::string curve;
        double easeMs = kDefaultEaseMs;
        const bool hasEase = parseEaseSlot(binding, curve, easeMs);
        const bool toFrame = parseMapToFrame(binding);

        double number;
        double lo = 0.0, hi = 0.0;
        bool isParam;
        std::string key;
        {
            JsonValue raw;
            if (isParamPath(from)) {
                key = from.substr(kParamPrefixLen);
                raw = resolveParamRaw(key);
                if (raw.type == JsonValue::Type::Null) return; // unknown key → keep default
                isParam = true;
                number = paramNumber(raw);
                paramDomain(raw, lo, hi); // the map slot's normalization domain
            } else {
                raw = lookup(from);
                if (raw.type == JsonValue::Type::Null) return; // unknown source → keep default
                if (raw.type != JsonValue::Type::Number) {
                    // Non-numeric source: ease/map don't apply; pass the value through.
                    setEntityProp(e, prop, raw);
                    return;
                }
                isParam = false;
                number = raw.number;
            }
        }

        // ease slot: tween on change (transient anchor); else just seed it (P1 seam).
        double value = number;
        if (hasEase) value = applyEase(e, prop, curve, easeMs, number);
        else seedEasingState(e, prop, number);

        // map slot: quantize the (eased) value to a sprite frame index.
        if (toFrame) {
            int frame = 0;
            if (!applyFrameMap(e, lo, hi, value, frame)) return; // no sprite → keep default
            JsonValue out;
            out.type = JsonValue::Type::Number;
            out.number = static_cast<double>(frame);
            setEntityProp(e, prop, out);
            return;
        }

        // format terminal: a bare `param.` binding (no ease/map) renders the
        // display String exactly as pre-#218; an eased/mapped or non-param value
        // writes the raw Number.
        JsonValue out;
        if (isParam && !hasEase) {
            out.type = JsonValue::Type::String;
            out.str = formatParam(key, format, number);
        } else {
            out.type = JsonValue::Type::Number;
            out.number = value;
        }
        setEntityProp(e, prop, out);
    }

    /// @brief The `{min,max}` normalization domain out of a resolve-raw object
    /// (unwn #220): the map slot needs it to place the value in `[0,1]`. Missing
    /// fields default to `[0,0]` (a degenerate domain → the map yields frame 0).
    static void paramDomain(const JsonValue& raw, double& lo, double& hi) {
        const JsonValue* mn = raw.find("min");
        const JsonValue* mx = raw.find("max");
        lo = mn && mn->type == JsonValue::Type::Number ? mn->number : 0.0;
        hi = mx && mx->type == JsonValue::Type::Number ? mx->number : 0.0;
    }

    /// @brief Parse the `ease` slot (unwn #221): a shorthand curve string
    /// (`"inOutCubic"` → `{curve, ms:200}`) or an explicit `{curve, ms}`. Returns
    /// false when absent. `ms` defaults to 200; an unknown @p curve resolves to
    /// Linear later (in @ref detail::easingByName), so it is carried verbatim.
    static bool parseEaseSlot(const JsonValue& binding, std::string& curve, double& ms) {
        curve.clear();
        ms = kDefaultEaseMs;
        if (binding.type != JsonValue::Type::Object) return false;
        const JsonValue* e = binding.find("ease");
        if (!e) return false;
        if (e->type == JsonValue::Type::String) {
            curve = e->str;
            return true;
        }
        if (e->type == JsonValue::Type::Object) {
            const JsonValue* c = e->find("curve");
            if (c && c->type == JsonValue::Type::String) curve = c->str;
            const JsonValue* m = e->find("ms");
            if (m && m->type == JsonValue::Type::Number) ms = m->number;
            return true;
        }
        return false;
    }

    /// @brief Whether the binding carries `map:{to:"frame"}` (unwn #220) — the one
    /// target the value→frame slot fills. No general remap; any other shape misses.
    static bool parseMapToFrame(const JsonValue& binding) {
        if (binding.type != JsonValue::Type::Object) return false;
        const JsonValue* m = binding.find("map");
        if (!m || m->type != JsonValue::Type::Object) return false;
        const JsonValue* to = m->find("to");
        return to && to->type == JsonValue::Type::String && to->str == "frame";
    }

    /// @brief Ease slot transform (unwn #221): tween the resolved @p target over
    /// @p ms on the VM clock (@ref timeMs_), re-anchoring `from` to the current
    /// eased value whenever the target moves — so a mid-tween change glides on
    /// with no snap, and a first appearance seeds the target (no tween-from-zero).
    /// Rounded to 2dp, matching the animation tracks for native↔WASM golden parity.
    double applyEase(Entity e, const std::string& prop, const std::string& curve, double ms,
                     double target) {
        EasingAnchor* a = seedEasingState(e, prop, target);
        if (a->target != target) { // target moved → re-anchor from the current value
            a->from = a->value;
            a->target = target;
            a->startMs = timeMs_;
        }
        const double t = ms > 0.0 ? std::min(1.0, (timeMs_ - a->startMs) / ms) : 1.0;
        const double eased =
            static_cast<double>(detail::easingByName(curve)(static_cast<float>(t)));
        a->value = std::round((a->from + (a->target - a->from) * eased) * 100.0) / 100.0;
        return a->value;
    }

    /// @brief Map slot transform (unwn #220): quantize @p value in domain
    /// `[lo,hi]` to a frame index off the bound sprite's live frameCount().
    /// `t=(v-lo)/(hi-lo)`, `frame=clamp(round(t·(N-1)),0,N-1)` — v=lo→0, v=hi→N-1.
    /// N is read here, never authored, so a sheet re-import needs no binding edit.
    /// Returns false (tolerant miss → keep default) when the entity carries no
    /// SpriteComponent or an empty sheet.
    bool applyFrameMap(Entity e, double lo, double hi, double value, int& frame) const {
        if constexpr (TWorld::template composes<SpriteComponent>()) {
            const SpriteComponent* s = world_->template get<SpriteComponent>(e);
            if (!s || s->frameCount() == 0) return false;
            const int n = static_cast<int>(s->frameCount());
            const double t = hi > lo ? (value - lo) / (hi - lo) : 0.0;
            int f = static_cast<int>(std::lround(t * (n - 1)));
            if (f < 0) f = 0;
            if (f > n - 1) f = n - 1;
            frame = f;
            return true;
        } else {
            (void)e;
            (void)lo;
            (void)hi;
            (void)value;
            (void)frame;
            return false;
        }
    }

    /// @brief Condition form `{from, op, threshold, then?, else?}` (unwn #213, P5).
    /// A single numeric comparison of the resolved `from` against a literal
    /// `threshold` selects one of two literal branches, each of the target
    /// property's own kind (no nesting, no format/ease/map). `op` ∈
    /// `> >= < <= == !=`. The chosen literal is passed verbatim through
    /// @ref setEntityProp, so @ref detail::readFieldValue coerces it to the
    /// target field's C++ type (and strictly rejects a wrong-kind literal, which
    /// then keeps the authored default — the uniform tolerant-miss).
    ///
    /// Fallbacks (spec #213): an absent branch synthesizes a Bool — `then`→true,
    /// `else`→false — so a **bool** target defaults its branches and a non-bool
    /// target simply keeps its authored default (the wrong-kind literal is
    /// rejected). `else` omitted with a false condition therefore writes nothing
    /// on any non-bool target. With `op`/`threshold` absent the condition is the
    /// plain truthy test of `from` (the `visible: someVar` shorthand). An
    /// unresolvable `from` (unknown var / param key) is a no-op: keep the default.
    void applyConditionBinding(Entity e, const std::string& prop, const JsonValue& binding) {
        const JsonValue* fromV = binding.find("from");
        if (!fromV || fromV->type != JsonValue::Type::String) return; // malformed → miss

        // Resolve `from` exactly as the value chain does: a `param.` source
        // resolve-raws to its Number; anything else is a lookup. A miss (unknown
        // key / var) keeps the authored default.
        JsonValue resolved;
        if (isParamPath(fromV->str)) {
            const JsonValue raw = resolveParamRaw(fromV->str.substr(kParamPrefixLen));
            if (raw.type == JsonValue::Type::Null) return; // unknown key → keep default
            resolved.type = JsonValue::Type::Number;
            resolved.number = paramNumber(raw);
        } else {
            resolved = lookup(fromV->str);
            if (resolved.type == JsonValue::Type::Null) return; // unknown source → keep default
        }

        const JsonValue* opV = binding.find("op");
        const JsonValue* thV = binding.find("threshold");

        // A genuine comparison when both `op` and a numeric `threshold` are
        // present; otherwise the truthy test of `from`.
        bool cond;
        if (opV && opV->type == JsonValue::Type::String && thV &&
            thV->type == JsonValue::Type::Number) {
            if (resolved.type != JsonValue::Type::Number) return; // non-numeric compare → miss
            const double v = resolved.number;
            const double t = thV->number;
            const std::string& op = opV->str;
            if (op == ">") cond = v > t;
            else if (op == ">=") cond = v >= t;
            else if (op == "<") cond = v < t;
            else if (op == "<=") cond = v <= t;
            else if (op == "==") cond = v == t;
            else if (op == "!=") cond = v != t;
            else return; // unknown operator → tolerant miss
        } else {
            cond = resolved.truthy();
        }

        // Select the branch literal; an absent branch defaults to a Bool of the
        // condition (then→true / else→false). setEntityProp coerces it to the
        // target kind and rejects a mismatch, so a non-bool target with an absent
        // branch keeps its authored default.
        const JsonValue* branch = binding.find(cond ? "then" : "else");
        if (branch) {
            setEntityProp(e, prop, *branch);
        } else {
            JsonValue b;
            b.type = JsonValue::Type::Bool;
            b.boolean = cond;
            setEntityProp(e, prop, b);
        }
    }

    /// @brief Seed the transient `(entity, property)` easing anchor from the first
    /// resolved value (unwn #218). Idempotent: only the *first* resolution seeds;
    /// @ref applyEase re-anchors on change. Never serialized — a VM-local side
    /// table. Returns the anchor so a caller can read/step it.
    EasingAnchor* seedEasingState(Entity e, const std::string& prop, double value) {
        for (EasingAnchor& a : easingState_)
            if (a.entity == e && a.prop == prop) return &a;
        easingState_.push_back(EasingAnchor{e, prop, value, value, value, timeMs_});
        return &easingState_.back();
    }

    void setTimer(const std::string& name, double ms) {
        for (auto& t : timers_) {
            if (t.first == name) {
                t.second = ms; // (re-)arm resets remaining — the debounce semantics
                return;
            }
        }
        timers_.emplace_back(name, ms);
    }

    void cancelTimer(const std::string& name) {
        for (size_t i = 0; i < timers_.size(); ++i) {
            if (timers_[i].first == name) {
                timers_.erase(timers_.begin() + static_cast<long>(i));
                return;
            }
        }
    }

    void stopAnim(const std::string& name) {
        for (size_t i = 0; i < anims_.size(); ++i) {
            if (anims_[i].first == name) {
                anims_.erase(anims_.begin() + static_cast<long>(i));
                return;
            }
        }
    }

    static std::string easingOf(const JsonValue& track) {
        const JsonValue* e = track.find("easing");
        return e && e->type == JsonValue::Type::String ? e->str : std::string("linear");
    }

    // ---- execution ----------------------------------------------------------

    void runRules(const JsonValue& rules, const JsonValue* payload, SceneEffects& effects) {
        if (rules.type != JsonValue::Type::Array) return;
        for (const JsonValue& rule : rules.array) {
            runRule(rule, payload, effects);
        }
    }

    void runRule(const JsonValue& rule, const JsonValue* payload, SceneEffects& effects) {
        if (rule.type != JsonValue::Type::Object) return;
        const JsonValue* guard = rule.find("if");
        const JsonValue* actions = rule.find("do");
        if (!guard && !actions) {
            // bare action = rule with no guard; rules and actions are one shape
            runAction(rule, payload, effects);
            return;
        }
        if (guard && guard->type == JsonValue::Type::String && !evalCond(guard->str)) return;
        if (actions && actions->type == JsonValue::Type::Array)
            for (const JsonValue& action : actions->array) runAction(action, payload, effects);
    }

    void runAction(const JsonValue& action, const JsonValue* payload, SceneEffects& effects) {
        if (action.type != JsonValue::Type::Object) return;
        if (action.find("if")) { // nested guarded block — same shape as a rule
            runRule(action, payload, effects);
        } else if (const JsonValue* set = action.find("set")) {
            if (set->type != JsonValue::Type::Array || set->array.size() != 2 ||
                set->array[0].type != JsonValue::Type::String)
                return;
            setPath(set->array[0].str, resolve(set->array[1], payload));
        } else if (const JsonValue* call = action.find("call")) {
            if (call->type != JsonValue::Type::String) return;
            Entity e;
            std::string verb;
            if (!splitEntityPath(call->str, e, verb)) return;
            JsonValue args;
            args.type = JsonValue::Type::Array;
            if (const JsonValue* a = action.find("args")) args = resolve(*a, payload);
            callWidgetVerb(*world_, e, verb.c_str(), args);
        } else if (const JsonValue* host = action.find("host")) {
            if (host->type != JsonValue::Type::String) return;
            SceneEffect fx;
            fx.kind = SceneEffect::Kind::Host;
            fx.name = host->str;
            fx.args.type = JsonValue::Type::Object;
            if (const JsonValue* a = action.find("args")) fx.args = resolve(*a, payload);
            effects.push_back(std::move(fx));
        } else if (const JsonValue* timer = action.find("timer")) {
            if (timer->type != JsonValue::Type::Array || timer->array.size() != 2 ||
                timer->array[0].type != JsonValue::Type::String ||
                timer->array[1].type != JsonValue::Type::String)
                return;
            const std::string& op = timer->array[0].str;
            const std::string& name = timer->array[1].str;
            if (op == "start") {
                const JsonValue* spec = doc_ ? doc_->timers.find(name.c_str()) : nullptr;
                const JsonValue* ms = spec ? spec->find("ms") : nullptr;
                if (ms && ms->type == JsonValue::Type::Number) setTimer(name, ms->number);
            } else if (op == "cancel") {
                cancelTimer(name);
            }
        } else if (const JsonValue* anim = action.find("anim")) {
            if (anim->type != JsonValue::Type::Array || anim->array.size() != 2 ||
                anim->array[0].type != JsonValue::Type::String ||
                anim->array[1].type != JsonValue::Type::String)
                return;
            const std::string& op = anim->array[0].str;
            const std::string& name = anim->array[1].str;
            if (op == "play") {
                const JsonValue* spec = doc_ ? doc_->animations.find(name.c_str()) : nullptr;
                const JsonValue* tracks = spec ? spec->find("tracks") : nullptr;
                if (!tracks || tracks->type != JsonValue::Type::Array) return;
                stopAnim(name);
                anims_.emplace_back(name, 0.0);
                for (const JsonValue& track : tracks->array) {
                    const JsonValue* target = track.find("target");
                    const JsonValue* from = track.find("from");
                    if (target && target->type == JsonValue::Type::String && from)
                        setPath(target->str, *from);
                }
            }
        } else if (const JsonValue* sw = action.find("sceneSwitch")) {
            if (sw->type != JsonValue::Type::String) return;
            SceneEffect fx;
            fx.kind = SceneEffect::Kind::SceneSwitch;
            fx.name = sw->str;
            effects.push_back(std::move(fx));
        }
        // Unknown action verbs are skipped — the tolerant contract extends to behavior.
    }
};

} // namespace enjin2
