// enjin2::ScenePlayer (unwn #184, M2 exit): the one scene-playback rig both
// parity tracks compile — eisei_preview links it natively, the WASM editor
// surface links it under Emscripten. Parity is by construction only if this
// header is the single source of the World/Canvas/system wiring, so the test
// pins the contract the goldens depend on:
//
//   - loadText() on the shared fixture activates the scene (enter animation
//     armed on frame 0, status label set) and renders into a 127x127 Canvas4
//     with the padded-row packed layout the golden .bin files capture verbatim
//   - dispatch()/stepFrame() drive the same behavior the SceneVM test proves,
//     observable through the rendered buffer
//   - two players fed the same script produce byte-identical buffers — the
//     determinism claim the CI byte-compare stands on
//   - a malformed document leaves the player inactive, never half-swapped
#include <enjin2/ui/scene_player.hpp>

#include "scene_fixture.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

static bool bufferAllZero(const ScenePlayer::Canvas* cv) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(cv->getBuffer());
    for (size_t i = 0; i < cv->getBufferSize(); ++i)
        if (b[i] != 0) return false;
    return true;
}

// The scripted drive both players replay in the determinism test: activate,
// settle the enter animation, deliver the list, scroll once.
static void drive(ScenePlayer& p) {
    p.loadText(kDatumManagerScene);
    for (int i = 0; i < 16; ++i) p.stepFrame();
    p.dispatch("host.listArrived",
               R"({"names": ["OddEven", "SineMove", "Pulsar", "Mirror", "Comb"],
                   "loadedIndex": 2})");
    p.stepFrame();
    p.dispatch("input.encoder.cw", "");
    p.stepFrame();
}

static void test_load_activates_and_renders() {
    ScenePlayer p;
    ASSERT(!p.active(), "load: player starts inactive");
    ASSERT(p.loadText(kDatumManagerScene), "load: fixture document loads");
    ASSERT(p.active(), "load: player active after load");

    // The golden contract: 127x127 packed at a 64-byte row stride.
    ASSERT(p.canvas()->getBufferSize() == 64u * 127u,
           "load: canvas buffer is the 8128-byte packed 127x127 layout");

    p.stepFrame();
    ASSERT(!bufferAllZero(p.canvas()),
           "load: scene.activate armed the enter animation — frame 1 not blank");
}

static void test_dispatch_reaches_the_frame() {
    ScenePlayer p;
    p.loadText(kDatumManagerScene);
    for (int i = 0; i < 16; ++i) p.stepFrame();   // enter anim done (250ms)

    ScenePlayer::Canvas before = *p.canvas();
    p.dispatch("host.listArrived",
               R"({"names": ["OddEven", "SineMove", "Pulsar", "Mirror", "Comb"],
                   "loadedIndex": 2})");
    p.stepFrame();
    ASSERT(memcmp(before.getBuffer(), p.canvas()->getBuffer(),
                  p.canvas()->getBufferSize()) != 0,
           "dispatch: list arrival changes the rendered frame");
}

// renderFrame() (unwn #226 follow-up) repaints the current scene state without
// advancing time: the editor's paused preview reflects an input without
// stepping the enter animation forward. Byte-identical to the last stepFrame,
// and stable across repeats, where a real stepFrame moves the animation on.
static void test_render_frame_repaints_without_advancing() {
    ScenePlayer p;
    p.loadText(kDatumManagerScene);
    for (int i = 0; i < 4; ++i) p.stepFrame(); // mid enter-animation (64ms)

    ScenePlayer::Canvas frozen = *p.canvas();
    p.renderFrame();
    ASSERT(memcmp(frozen.getBuffer(), p.canvas()->getBuffer(),
                  p.canvas()->getBufferSize()) == 0,
           "render: repaints the current frame byte-identically (no advance)");
    p.renderFrame();
    p.renderFrame();
    ASSERT(memcmp(frozen.getBuffer(), p.canvas()->getBuffer(),
                  p.canvas()->getBufferSize()) == 0,
           "render: repeated renders never step the animation forward");

    for (int i = 0; i < 8; ++i) p.stepFrame(); // real time DOES advance it
    ASSERT(memcmp(frozen.getBuffer(), p.canvas()->getBuffer(),
                  p.canvas()->getBufferSize()) != 0,
           "render: a real stepFrame advances where renderFrame did not");
}

static void test_two_players_render_identical_bytes() {
    ScenePlayer a, b;
    drive(a);
    drive(b);
    ASSERT(memcmp(a.canvas()->getBuffer(), b.canvas()->getBuffer(),
                  a.canvas()->getBufferSize()) == 0,
           "determinism: same script, byte-identical buffers");
}

static void test_reload_replaces_the_scene() {
    ScenePlayer p;
    drive(p);
    ASSERT(p.loadText(kDatumManagerScene), "reload: second load succeeds");
    p.stepFrame();

    ScenePlayer fresh;
    fresh.loadText(kDatumManagerScene);
    fresh.stepFrame();
    ASSERT(memcmp(p.canvas()->getBuffer(), fresh.canvas()->getBuffer(),
                  p.canvas()->getBufferSize()) == 0,
           "reload: no state leaks from the torn-down scene");
}

static void test_malformed_load_leaves_inactive() {
    ScenePlayer p;
    ASSERT(!p.loadText("{ not json"), "malformed: load fails");
    ASSERT(!p.active(), "malformed: player inactive after failed load");

    drive(p);
    ASSERT(!p.loadText("{ not json"), "malformed: reload over a live scene fails");
    ASSERT(!p.active(), "malformed: failed reload deactivates, never half-swaps");
}

// The v2 clean break (unwn #202): a document with an explicit pre-v2 version is
// rejected outright — no migration — and the player stays inactive, exactly as
// for malformed JSON. A v2 document with the same body loads.
static void test_pre_v2_version_is_rejected() {
    ScenePlayer p;
    ASSERT(!p.loadText(R"({"version":1,"scene":"old","entities":[]})"),
           "version: a v1 document is rejected");
    ASSERT(!p.active(), "version: player inactive after a rejected v1 load");

    ASSERT(!p.loadText(R"({"version":0,"scene":"older","entities":[]})"),
           "version: version 0 is rejected too");

    ASSERT(p.loadText(R"({"version":2,"scene":"new","entities":[]})"),
           "version: the same body at v2 loads");
    ASSERT(p.active(), "version: player active after the v2 load");
}

// --- Authorable layering: per-entity z + single z-sorted pass (unwn #243) ---

// Two filled rects stacked on the same 20x20 box, colors 5 then 10. `%Z1%` is
// spliced into the first shape's entity to give it an explicit draw order.
static const char* const kStackScene = R"json({
  "version": 2, "scene": "stack",
  "entities": [
    { "components": { %Z1%
        "shape": { "type": 0, "filled": true, "color": 5 },
        "position": { "position": { "x": 10, "y": 10 } },
        "size": { "size": { "width": 20, "height": 20 } } } },
    { "components": {
        "shape": { "type": 0, "filled": true, "color": 10 },
        "position": { "position": { "x": 10, "y": 10 } },
        "size": { "size": { "width": 20, "height": 20 } } } }
  ]
})json";

static std::string stackScene(const char* z1) {
    std::string s = kStackScene;
    s.replace(s.find("%Z1%"), 4, z1);
    return s;
}

static void test_z_order_reorders_the_pass() {
    // Default z = legacy system order, ties broken by file order: the second
    // shape draws last, so the overlap reads its color (10).
    ScenePlayer def;
    ASSERT(def.loadText(stackScene("")), "z: default-order scene loads");
    def.stepFrame();
    ASSERT(def.canvas()->getPixel(15, 15) == 10,
           "z: with no authored z, later-in-file wins (byte-compatible order)");

    // Lift the first shape's z above the second's default (850): it now draws
    // last and wins the same pixel — a real per-entity, cross-default reorder.
    ScenePlayer lifted;
    ASSERT(lifted.loadText(stackScene(R"("z": { "z": 900 },)")),
           "z: z-authored scene loads");
    lifted.stepFrame();
    ASSERT(lifted.canvas()->getPixel(15, 15) == 5,
           "z: an authored z reorders the merged pass (first shape now on top)");
}

// A bright backdrop, a full-canvas dim, and a separate chrome patch. `%OZ%` and
// `%CZ%` splice draw orders onto the overlay and the chrome so the same three
// entities test both the legacy run-first no-op and the authored mid-z dim.
static const char* const kDimScene = R"json({
  "version": 2, "scene": "dim",
  "entities": [
    { "components": {
        "shape": { "type": 0, "filled": true, "color": 15 },
        "position": { "position": { "x": 0, "y": 0 } },
        "size": { "size": { "width": 40, "height": 40 } } } },
    { "components": { "id": { "id": "dim" }, "overlay": { "opacity": 5 }%OZ% } },
    { "components": {
        "shape": { "type": 0, "filled": true, "color": 12 },
        "position": { "position": { "x": 50, "y": 50 } },
        "size": { "size": { "width": 10, "height": 10 } }%CZ% } }
  ]
})json";

static std::string dimScene(const char* oz, const char* cz) {
    std::string s = kDimScene;
    s.replace(s.find("%OZ%"), 4, oz);
    s.replace(s.find("%CZ%"), 4, cz);
    return s;
}

static void test_overlay_runs_first_noop_by_default() {
    // With the overlay at its default z (800) it still sorts first, dimming the
    // just-cleared black canvas — the harmless historical no-op, so the backdrop
    // stays at full brightness. This is what keeps pre-#243 scenes byte-identical.
    ScenePlayer p;
    ASSERT(p.loadText(dimScene("", "")), "overlay: default-z scene loads");
    p.stepFrame();
    ASSERT(p.canvas()->getPixel(5, 5) == 15,
           "overlay: a default-z overlay dims nothing (run-first no-op preserved)");
}

static void test_overlay_mid_z_dims_below_not_above() {
    // Overlay lifted to z=900 (above the backdrop's 850) and the chrome to 950:
    // the sorted pass draws the backdrop, dims the canvas as-drawn, then draws
    // the chrome on top undimmed. The run-first no-op is gone (ADR-0014).
    ScenePlayer p;
    ASSERT(p.loadText(dimScene(R"(, "z": { "z": 900 })", R"(, "z": { "z": 950 })")),
           "overlay: mid-z scene loads");
    p.stepFrame();
    ASSERT(p.canvas()->getPixel(5, 5) == 10,
           "overlay: content below the overlay is dimmed (15 - 5)");
    ASSERT(p.canvas()->getPixel(52, 52) == 12,
           "overlay: chrome above the overlay is left undimmed");
}

int main() {
    test_z_order_reorders_the_pass();
    test_overlay_runs_first_noop_by_default();
    test_overlay_mid_z_dims_below_not_above();
    test_load_activates_and_renders();
    test_dispatch_reaches_the_frame();
    test_render_frame_repaints_without_advancing();
    test_two_players_render_identical_bytes();
    test_reload_replaces_the_scene();
    test_malformed_load_leaves_inactive();
    test_pre_v2_version_is_rejected();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
