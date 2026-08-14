// Scene-file playback rig (unwn #184, M2 exit).
//
// The one place the editor-scene World, the 127x127 authoring canvas
// (unwn ADR-0006) and the widget-system wiring live. Both parity tracks
// compile this header — eisei_preview links it natively, the WASM editor
// surface links it under Emscripten — so the CI byte-compare of their goldens
// proves toolchain parity, never wiring drift: there is no second copy of the
// rig to drift.
//
// A loaded scene is a running scene: `scene.activate` is dispatched inside
// loadText(), so enter animations arm on frame 0. Host effects the scene
// emits are logged to stderr on both tracks (the session's `event` directive
// plays the host's other half); they never reach the rendered frame.
#pragma once

#include "../graphics/canvas.hpp"
#include "../graphics/sprite_asset.hpp"
#include "asset_manifest.hpp"
#include "scene_json.hpp"
#include "scene_stream.hpp"
#include "schema_json.hpp"
#include "scene_vm.hpp"
#include "systems.hpp"
#include "theme.hpp"
#include "widgets/bar.hpp"
#include "widgets/gauge.hpp"
#include "widgets/icon.hpp"
#include "widgets/label.hpp"
#include "widgets/list.hpp"
#include "widgets/overlay.hpp"
#include "widgets/popup.hpp"
#include "widgets/shape.hpp"
#include "widgets/sprite.hpp"
#include "world.hpp"

#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace enjin2 {

class ScenePlayer {
public:
    // One wide world for editor-authored scenes (spec: G5 stays deferred via a
    // single World reserved for them) — every reflected component type.
    //
    // Entity capacity (unwn #204, spec §Widget set §4): raised 16 → 32. Now that
    // `shape` is a placeable widget (unwn #206) each rect/line/frame is its own
    // entity, so decorative-heavy scenes cross 16 fast. 32 is provisional — the
    // final N is gated on the #199 hardware free-heap measurement; changing it
    // is a one-integer edit here.
    static constexpr size_t kEntityCapacity = 32;
    using World = enjin2::World<kEntityCapacity, IdComponent, PositionComponent, SizeComponent,
                                LabelComponent, IconComponent, SpriteComponent, GaugeComponent,
                                ShapeComponent, BarComponent, OverlayComponent, PopUpComponent,
                                ListComponent, SlotComponent, BindingsComponent>;
    // The ratified authoring surface: the firmware's 127x127 logical region,
    // packed rows at (127+1)/2 = 64 bytes — the exact bytes the goldens hold.
    using Canvas = Canvas4<127, 127>;

    // The session clock's fixed frame. Drivers that keep their own virtual
    // clock must tick at this same period for the two tracks to stay in step.
    static constexpr int kFrameMs = 16;

    bool active() const { return vm_ != nullptr; }
    Canvas* canvas() { return &canvas_; }

    /// The loaded scene's world (nullptr when no scene is active) — read-only
    /// access for inspection and tests.
    const World* world() const { return world_.get(); }

    /// Install the content-addressed asset byte source (unwn #204). The source
    /// must outlive the player. With no source the player loads no owned
    /// assets; icons then resolve only against compiled-in bitmaps, as before.
    void setAssetSource(AssetSource* source) { assetSource_ = source; }

    /// The player's asset registry — owned scene assets plus any compiled-in
    /// bitmaps/fonts a host registers before load.
    AssetRegistry& assets() { return assets_; }

    /// Load a scene document from JSON text and start it: `scene.activate` is
    /// dispatched here, so a loaded scene is a running scene.
    bool loadText(const std::string& text) {
        return loadWith(
            [&](SceneDoc& doc, World& world, Theme* theme, bool* themePresent) {
                return readSceneDocJson(text, doc, world, assets_, theme, themePresent,
                                        [this] { loadManifestAssets(); });
            },
            /*captureSaveText=*/true);
    }

    /// Stream-load a scene document (the firmware/M5 path): same activation
    /// contract as loadText, but the document text is never resident and no
    /// canonical-save snapshot is captured — saveText() returns "" for a
    /// stream-loaded scene (firmware never saves).
    bool loadStream(SceneStreamSource& src) {
        return loadWith(
            [&](SceneDoc& doc, World& world, Theme* theme, bool* themePresent) {
                return readSceneDocStream(src, doc, world, assets_, theme, themePresent,
                                          [this] { loadManifestAssets(); });
            },
            /*captureSaveText=*/false);
    }

    /// The loaded document in canonical scene JSON (the round-trip writer's
    /// fixed point), snapshotted at load before scene.activate runs — the
    /// authored scene, never runtime state. A theme section survives only
    /// when the document authored one. "" when no scene is loaded.
    const std::string& saveText() const {
        static const std::string kEmpty;
        return vm_ ? savedText_ : kEmpty;
    }

    /// The reflected component schema for this player's world (unwn #186):
    /// palette + inspector metadata from the same field lists that drive
    /// save/load, plus this player's compiled-in asset enumeration. Constant
    /// across loads — safe to fetch once, before any load.
    ///
    /// @p extraSections optionally appends app-owned top-level schema (e.g. the
    /// ParamRegistry `params`/`formatters` sections, unwn #201).
    std::string schemaText(void (*extraSections)(JsonWriter&) = nullptr) const {
        return writeSchemaJson<World>(assets_, extraSections);
    }

    /// Install the app's `param.` *resolve-raw* (unwn #202/#218). Bindings from
    /// the `param.` namespace resolve their live-value cells to `{number,min,max}`
    /// through this; unset ⇒ those bindings read Null (tolerant). Applies to the
    /// next load. Pair it with @ref setParamFormatter for the display terminal.
    void setParamResolver(ParamResolver resolver) { paramResolver_ = std::move(resolver); }

    /// Install the app's `param.` *formatter* — the value chain's format terminal
    /// (unwn #218). Renders a resolved raw number to its display String for a
    /// textual target; unset ⇒ the terminal yields "". Applies to the next load.
    void setParamFormatter(ParamFormatter formatter) { paramFormatter_ = std::move(formatter); }

    /// Optional host-effect sink. The preview/editor tracks read effects off
    /// stderr; a hosting firmware registers a handler instead (e.g. to honor
    /// `ui.exitScene`). Effects are logged either way.
    using EffectHandler = std::function<void(const SceneEffect&)>;
    void setEffectHandler(EffectHandler handler) { effectHandler_ = std::move(handler); }

    /// Dispatch one event into the tables; payload is inline JSON ("" = none).
    void dispatch(const std::string& event, const std::string& payloadText) {
        if (!vm_) return;
        JsonValue payload;
        const bool hasPayload = !payloadText.empty() && parseJson(payloadText, payload);
        if (!payloadText.empty() && !hasPayload)
            fprintf(stderr, "[scene] bad payload for %s: %s\n", event.c_str(),
                    payloadText.c_str());
        logEffects(vm_->dispatch(event, hasPayload ? &payload : nullptr));
    }

    /// One frame: advance behavior by @p dtMs (the fixed frame period by
    /// default — parity drivers must not pass anything else), then render.
    /// Firmware runs its own frame clock and passes the real delta.
    void stepFrame(int dtMs = kFrameMs) {
        if (!vm_) return;
        logEffects(vm_->tick(static_cast<double>(dtMs)));
        renderInto(dtMs);
    }

    /// Re-render the current scene state *without advancing time* (unwn #226
    /// follow-up): applies bindings at the current clock (`tick(0)` reflects a
    /// just-changed var/param, but leaves `timeMs_`, timers and eased tweens
    /// untouched) and repaints with a zero widget-animation delta. The editor's
    /// paused preview calls this to show an input's effect without stepping the
    /// scene forward — a "frozen" repaint.
    void renderFrame() {
        if (!vm_) return;
        logEffects(vm_->tick(0.0));
        renderInto(0);
    }

    /// Set a runtime scene variable (unwn #227): the editor's swept-variable
    /// preview drive forwards here each frame before @ref stepFrame, so the
    /// sweep wins over the last behavior write. No-op when no scene is loaded.
    void setVar(const std::string& name, double value) {
        if (vm_) vm_->setVar(name, value);
    }

private:
    /// Clear the canvas and run every widget system with animation delta
    /// @p dtMs (0 = repaint current state without advancing sprite/overlay
    /// animation). Shared by @ref stepFrame and @ref renderFrame.
    void renderInto(int dtMs) {
        canvas_.clear(Pixel4(0));
        const float dt = dtMs / 1000.0f;
        rig_->overlaySys.update(dt);
        rig_->shapeSys.update(dt);
        rig_->labelSys.update(dt);
        rig_->iconSys.update(dt);
        rig_->spriteSys.update(dt);
        rig_->listSys.update(dt);
        rig_->gaugeSys.update(dt);
        rig_->barSys.update(dt);
        rig_->popupSys.update(dt);
    }

    // Widget systems in ascending priority order (overlay 800 < shape 850 <
    // label/icon/list 900 < gauge 950 < bar 955 < popup 1000) — the same rig as the
    // engine tests. Shapes draw as decorative backdrop beneath content; bars
    // draw with the gauges as level indicators above it.
    struct Rig {
        OverlaySystem<World, Canvas> overlaySys;
        ShapeSystem<World, Canvas> shapeSys;
        LabelSystem<World, Canvas> labelSys;
        IconSystem<World, Canvas> iconSys;
        SpriteSystem<World, Canvas> spriteSys;
        ListSystem<World, Canvas> listSys;
        GaugeSystem<World, Canvas> gaugeSys;
        BarSystem<World, Canvas> barSys;
        PopUpSystem<World, Canvas> popupSys;

        Rig(World& w, Canvas& c, const Theme& theme)
            : overlaySys(&w, &c), shapeSys(&w, &c), labelSys(&w, &c), iconSys(&w, &c),
              spriteSys(&w, &c), listSys(&w, &c, theme), gaugeSys(&w, &c), barSys(&w, &c),
              popupSys(&w, &c) {}
    };

    // Shared load path: tear down any previous scene before its world is
    // destroyed — the rig's systems and the VM hold raw pointers into it. A
    // failed load leaves the player inactive, never half-swapped.
    template<typename Loader>
    bool loadWith(Loader&& loadDoc, bool captureSaveText) {
        vm_.reset();
        rig_.reset();
        // Free the previous scene's owned assets — this is the only teardown
        // site, symmetric with loadManifestAssets() below (unwn #204).
        assets_.clearOwned();
        world_ = std::make_unique<World>();
        doc_ = SceneDoc{};
        theme_ = kDefaultTheme;
        themePresent_ = false;
        savedText_.clear();
        if (!loadDoc(doc_, *world_, &theme_, &themePresent_)) {
            // A pre-v2 version is rejected distinctly from a parse failure; the
            // reader sets doc_.version before it returns (unwn #202).
            if (doc_.version < kSceneMinReadVersion)
                fprintf(stderr,
                        "[scene] unsupported scene version %lld (need >= %lld); "
                        "v1 scenes are not migrated\n",
                        static_cast<long long>(doc_.version),
                        static_cast<long long>(kSceneMinReadVersion));
            else
                fprintf(stderr, "[scene] malformed scene document\n");
            world_.reset();
            return false;
        }
        rig_ = std::make_unique<Rig>(*world_, canvas_, theme_);
        vm_ = std::make_unique<SceneVM<World>>(&doc_, world_.get(), &assets_, paramResolver_,
                                               paramFormatter_);
        // Canonical form of the *authored* document, captured before
        // scene.activate below mutates the world (state sets, enter
        // animations) — saveText() must never leak runtime state into a file.
        // The streaming path skips the snapshot: no document text is resident
        // and the firmware consumer never saves.
        if (captureSaveText)
            savedText_ = writeSceneDocJson(doc_, *world_, assets_,
                                           themePresent_ ? &theme_ : nullptr);
        fprintf(stderr, "[scene] loaded scene '%s' (%zu entities)\n",
                doc_.scene.c_str(), world_->entityCount());
        dispatch("scene.activate", "");
        return true;
    }

    // Read the scene's asset manifest and register each resident asset as an
    // owned bitmap under its content hash. Missing assets and non-v2/malformed
    // bytes are skipped (the icon then resolves to nothing, as before a load).
    // Palette index 15 (the .njn v2 transparent marker) is remapped to the icon
    // blit-skip sentinel, unifying the three transparency notions on one rule.
    void loadManifestAssets() {
        if (!assetSource_ || doc_.manifest.type != JsonValue::Type::Array) return;
        const std::vector<AssetManifestEntry> entries = parseAssetManifest(doc_.manifest);
        std::vector<uint8_t> bytes;
        for (const AssetManifestEntry& e : entries) {
            bytes.clear();
            if (!assetSource_->read(e.hash, bytes)) continue;
            NjnHeader h{};
            if (!parseNjnHeader(bytes.data(), bytes.size(), h)) continue;
            if (h.version != NJN_VERSION_V2) continue;
            const uint32_t px = njnPixelCount(h);
            std::vector<uint8_t> pixels(px);
            njnUnpackNibbles(bytes.data() + sizeof(NjnHeader), px, pixels.data());
            for (uint8_t& p : pixels)
                if (njnIsTransparent(p)) p = IconComponent::kTransparent;
            const uint16_t w = e.w ? e.w : static_cast<uint16_t>(h.cellW * h.cols);
            const uint16_t hgt = e.h ? e.h : static_cast<uint16_t>(h.cellH * h.rows);
            assets_.registerOwnedBitmap(e.hash.c_str(), pixels.data(), pixels.size(), w, hgt);
        }
    }

    void logEffects(const SceneEffects& effects) {
        for (const SceneEffect& fx : effects) {
            if (fx.kind == SceneEffect::Kind::SceneSwitch) {
                fprintf(stderr, "[scene] sceneSwitch -> %s\n", fx.name.c_str());
            } else {
                JsonWriter w;
                writeJson(w, fx.args);
                fprintf(stderr, "[scene] host -> %s %s\n", fx.name.c_str(), w.str().c_str());
            }
            if (effectHandler_) effectHandler_(fx);
        }
    }

    SceneDoc doc_;
    ParamResolver paramResolver_;
    ParamFormatter paramFormatter_;
    EffectHandler effectHandler_;
    std::unique_ptr<World> world_;
    AssetRegistry assets_;
    AssetSource* assetSource_ = nullptr; ///< Content-addressed byte source (borrowed)
    Theme theme_ = kDefaultTheme;
    bool themePresent_ = false;
    std::string savedText_;
    std::unique_ptr<Rig> rig_;
    std::unique_ptr<SceneVM<World>> vm_;
    Canvas canvas_;
};

} // namespace enjin2
