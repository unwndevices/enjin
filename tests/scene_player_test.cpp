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

int main() {
    test_load_activates_and_renders();
    test_dispatch_reaches_the_frame();
    test_two_players_render_identical_bytes();
    test_reload_replaces_the_scene();
    test_malformed_load_leaves_inactive();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
