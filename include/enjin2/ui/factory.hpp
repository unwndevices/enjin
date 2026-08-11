#pragma once

#include "json.hpp"
#include "reflect.hpp"
#include "widgets/list.hpp"
#include "widgets/popup.hpp"
#include "world.hpp"
#include <cstring>

/**
 * @file factory.hpp
 * @brief Name factory over the ui ECS: components and widget verbs by string (G3, unwn #183)
 *
 * The G1/G2 reflection layer gave every serializable component a permanent
 * name; this header is the indirection that *uses* those names at runtime:
 *
 *  - @ref addComponentByName — attach a default-constructed component to an
 *    entity from its scene-file name ("label", "gauge", …). The scene loader
 *    resolves the same reflected names (with the typed context it needs to
 *    populate fields in one pass); the editor palette enumerates the set via
 *    @ref forEachComponentName.
 *  - @ref callWidgetVerb — invoke compiled widget behavior by name
 *    ("moveUp", "setItems", …), the `call` action of the ratified behavior
 *    schema. Verbs are the C++/data boundary for *imperative* widget state:
 *    the data model may sequence them but never define them (ADR-0005).
 *
 * Everything is templated on the world type: the factory covers exactly the
 * component set a world composes, nothing global to drift.
 */

namespace enjin2 {

/**
 * @brief Attach a default-constructed component to @p e by its reflected name
 * @return true if the name matched a reflected component type of @p TWorld
 *         and the add succeeded
 */
template<typename TWorld>
bool addComponentByName(TWorld& world, Entity e, const char* name) {
    bool added = false;
    TWorld::forEachComponentType([&](auto tag) {
        using C = typename decltype(tag)::type;
        if constexpr (IsReflected<C>::value) {
            if (!added && std::strcmp(name, ComponentTraits<C>::kName) == 0)
                added = world.template add<C>(e) != nullptr;
        }
    });
    return added;
}

/**
 * @brief Enumerate the reflected component names @p TWorld composes
 * @param f Visitor `void(const char* name, ComponentTypeId id)`, called in
 *          pack order — the editor palette and pickers build from this
 */
template<typename TWorld, typename F>
constexpr void forEachComponentName(F&& f) {
    TWorld::forEachComponentType([&](auto tag) {
        using C = typename decltype(tag)::type;
        if constexpr (IsReflected<C>::value)
            f(ComponentTraits<C>::kName, ComponentTraits<C>::kTypeId);
    });
}

namespace detail {

/// @brief JSON array items → vector of strings (non-strings skipped, tolerant).
inline std::vector<std::string> stringList(const JsonValue& args, size_t at) {
    std::vector<std::string> out;
    if (args.type != JsonValue::Type::Array || at >= args.array.size()) return out;
    const JsonValue& v = args.array[at];
    if (v.type != JsonValue::Type::Array) return out;
    for (const JsonValue& item : v.array)
        if (item.type == JsonValue::Type::String) out.push_back(item.str);
    return out;
}

/// @brief args[at] as a number, or @p fallback.
inline double numberAt(const JsonValue& args, size_t at, double fallback = 0.0) {
    if (args.type != JsonValue::Type::Array || at >= args.array.size()) return fallback;
    const JsonValue& v = args.array[at];
    return v.type == JsonValue::Type::Number ? v.number : fallback;
}

/// @brief The per-widget verb tables. Unknown verbs return false.
inline bool callVerbOn(ListComponent& c, const char* verb, const JsonValue& args) {
    if (std::strcmp(verb, "moveUp") == 0) { c.moveUp(); return true; }
    if (std::strcmp(verb, "moveDown") == 0) { c.moveDown(); return true; }
    if (std::strcmp(verb, "setItems") == 0) { c.updateItems(stringList(args, 0)); return true; }
    if (std::strcmp(verb, "setSelection") == 0) {
        // Ratified interpreter spec (scene_vm.py): an out-of-range selection
        // is a no-op, not a clamp — the cursor keeps its position.
        const int idx = static_cast<int>(numberAt(args, 0, -1.0));
        if (idx >= 0 && idx < c.itemCount()) c.setCurrentSelection(idx);
        return true;
    }
    return false;
}

inline bool callVerbOn(PopUpComponent& c, const char* verb, const JsonValue& args) {
    if (std::strcmp(verb, "show") == 0) {
        c.show(static_cast<uint16_t>(numberAt(args, 0)));
        return true;
    }
    if (std::strcmp(verb, "hide") == 0) { c.hide(); return true; }
    return false;
}

/// @brief Fallback: component types without a verb table.
template<typename C>
bool callVerbOn(C&, const char*, const JsonValue&) {
    return false;
}

} // namespace detail

/**
 * @brief Invoke a widget verb on an entity by name (the `call` action)
 * @param args JSON array of resolved action arguments
 * @return true if some component on @p e handled @p verb
 *
 * Each component the entity holds is offered the verb in pack order; the
 * first taker wins. Verbs route through the widgets' public API, so the same
 * invariants hold as for C++ callers (selection clamps against items, a
 * popup's show() arms its countdown).
 */
template<typename TWorld>
bool callWidgetVerb(TWorld& world, Entity e, const char* verb, const JsonValue& args) {
    bool handled = false;
    TWorld::forEachComponentType([&](auto tag) {
        using C = typename decltype(tag)::type;
        if (!handled) {
            if (C* c = world.template get<C>(e))
                handled = detail::callVerbOn(*c, verb, args);
        }
    });
    return handled;
}

} // namespace enjin2
