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
#include "scene_json.hpp"
#include "schema_json.hpp"
#include "scene_vm.hpp"
#include "systems.hpp"
#include "theme.hpp"
#include "widgets/gauge.hpp"
#include "widgets/icon.hpp"
#include "widgets/label.hpp"
#include "widgets/list.hpp"
#include "widgets/overlay.hpp"
#include "widgets/popup.hpp"
#include "world.hpp"

#include <cstdio>
#include <memory>
#include <string>

namespace enjin2 {

class ScenePlayer {
public:
    // One wide world for editor-authored scenes (spec: G5 stays deferred via a
    // single World reserved for them) — every reflected component type.
    using World = enjin2::World<16, IdComponent, PositionComponent, SizeComponent,
                                LabelComponent, IconComponent, GaugeComponent,
                                OverlayComponent, PopUpComponent, ListComponent,
                                SlotComponent, BindingsComponent>;
    // The ratified authoring surface: the firmware's 127x127 logical region,
    // packed rows at (127+1)/2 = 64 bytes — the exact bytes the goldens hold.
    using Canvas = Canvas4<127, 127>;

    // The session clock's fixed frame. Drivers that keep their own virtual
    // clock must tick at this same period for the two tracks to stay in step.
    static constexpr int kFrameMs = 16;

    bool active() const { return vm_ != nullptr; }
    Canvas* canvas() { return &canvas_; }

    /// Load a scene document from JSON text and start it: `scene.activate` is
    /// dispatched here, so a loaded scene is a running scene.
    bool loadText(const std::string& text) {
        // Tear down any previous scene before its world is destroyed — the
        // rig's systems and the VM hold raw pointers into it. A failed load
        // leaves the player inactive, never half-swapped.
        vm_.reset();
        rig_.reset();
        world_ = std::make_unique<World>();
        doc_ = SceneDoc{};
        theme_ = kDefaultTheme;
        themePresent_ = false;
        if (!readSceneDocJson(text, doc_, *world_, assets_, &theme_, &themePresent_)) {
            fprintf(stderr, "[scene] malformed scene document\n");
            world_.reset();
            return false;
        }
        rig_ = std::make_unique<Rig>(*world_, canvas_, theme_);
        vm_ = std::make_unique<SceneVM<World>>(&doc_, world_.get(), &assets_);
        // Canonical form of the *authored* document, captured before
        // scene.activate below mutates the world (state sets, enter
        // animations) — saveText() must never leak runtime state into a file.
        savedText_ = writeSceneDocJson(doc_, *world_, assets_,
                                       themePresent_ ? &theme_ : nullptr);
        fprintf(stderr, "[scene] loaded scene '%s' (%zu entities)\n",
                doc_.scene.c_str(), world_->entityCount());
        dispatch("scene.activate", "");
        return true;
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
    std::string schemaText() const { return writeSchemaJson<World>(assets_); }

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

    /// One frame: advance behavior by the fixed frame period, then render.
    void stepFrame() {
        if (!vm_) return;
        logEffects(vm_->tick(static_cast<double>(kFrameMs)));
        canvas_.clear(Pixel4(0));
        const float dt = kFrameMs / 1000.0f;
        rig_->overlaySys.update(dt);
        rig_->labelSys.update(dt);
        rig_->iconSys.update(dt);
        rig_->listSys.update(dt);
        rig_->gaugeSys.update(dt);
        rig_->popupSys.update(dt);
    }

private:
    // Widget systems in ascending priority order (overlay 800 < label/icon/
    // list 900 < gauge 950 < popup 1000) — the same rig as the engine tests.
    struct Rig {
        OverlaySystem<World, Canvas> overlaySys;
        LabelSystem<World, Canvas> labelSys;
        IconSystem<World, Canvas> iconSys;
        ListSystem<World, Canvas> listSys;
        GaugeSystem<World, Canvas> gaugeSys;
        PopUpSystem<World, Canvas> popupSys;

        Rig(World& w, Canvas& c, const Theme& theme)
            : overlaySys(&w, &c), labelSys(&w, &c), iconSys(&w, &c), listSys(&w, &c, theme),
              gaugeSys(&w, &c), popupSys(&w, &c) {}
    };

    void logEffects(const SceneEffects& effects) {
        for (const SceneEffect& fx : effects) {
            if (fx.kind == SceneEffect::Kind::SceneSwitch) {
                fprintf(stderr, "[scene] sceneSwitch -> %s\n", fx.name.c_str());
            } else {
                JsonWriter w;
                writeJson(w, fx.args);
                fprintf(stderr, "[scene] host -> %s %s\n", fx.name.c_str(), w.str().c_str());
            }
        }
    }

    SceneDoc doc_;
    std::unique_ptr<World> world_;
    AssetRegistry assets_;
    Theme theme_ = kDefaultTheme;
    bool themePresent_ = false;
    std::string savedText_;
    std::unique_ptr<Rig> rig_;
    std::unique_ptr<SceneVM<World>> vm_;
    Canvas canvas_;
};

} // namespace enjin2
