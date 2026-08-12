// Sprite widget unit test (unwn #205): animated sprite-sheet on the ui ECS —
// an IconComponent sibling that carries a frame cursor driven two ways.
//
// Three seams are pinned: (1) the frame-major sampleAt read (each frame is a
// width*height span, out-of-range/matte reads are clear), (2) the advanceFrame
// state machine for Once/Loop/PingPong, and (3) the SpriteSystem render — an
// autonomous sprite (fps>0) advances under a delta clock, while a frame-driven
// sprite (fps<=0) blits exactly the authored/bound frame with matte skipped.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/widgets/sprite.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdint>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// A 2-frame sheet of 1x1 cells (cols=2, rows=1): frame 0 = value 5, frame 1 = 9.
static const uint8_t kTwoFrame[2] = {5, 9};

// A 2-frame sheet of 2x1 cells: frame 0 opaque-then-matte, frame 1 matte-then-opaque.
static const uint8_t kMatteSheet[4] = {
    7, 16, // frame 0: (0,0)=7 opaque, (1,0)=16 matte
    16, 8, // frame 1: (0,0)=16 matte, (1,0)=8 opaque
};

using SpriteWorld = World<32, SpriteComponent, PositionComponent>;

static void test_sample_frame_major() {
    SpriteComponent s;
    s.load(kMatteSheet, 2, 1, 2, 1);
    ASSERT(s.frameCount() == 2, "sprite: frameCount is cols*rows");
    ASSERT(s.sampleAt(0, 0) == 7, "sprite: frame 0 samples its own span");
    ASSERT(!s.isOpaqueAt(1, 0), "sprite: frame 0 matte pixel is transparent");
    s.frame = 1;
    ASSERT(s.sampleAt(1, 0) == 8, "sprite: frame 1 samples the next span");
    ASSERT(!s.isOpaqueAt(0, 0), "sprite: frame 1 matte pixel is transparent");
    s.frame = 99; // past the end clamps to the last frame
    ASSERT(s.sampleAt(1, 0) == 8, "sprite: an over-range frame clamps to the last");
    ASSERT(s.sampleAt(-1, 0) == s.matte, "sprite: out-of-range sample returns the matte");
}

static void test_advance_loop() {
    SpriteComponent s;
    s.load(kTwoFrame, 1, 1, 2, 1);
    s.mode = AnimMode::Loop;
    s.advanceFrame();
    ASSERT(s.frame == 1, "sprite/loop: 0 -> 1");
    s.advanceFrame();
    ASSERT(s.frame == 0, "sprite/loop: wraps 1 -> 0");
}

static void test_advance_once_freezes() {
    SpriteComponent s;
    s.load(kTwoFrame, 1, 1, 2, 1);
    s.mode = AnimMode::Once;
    s.advanceFrame();
    ASSERT(s.frame == 1 && !s.done, "sprite/once: 0 -> 1, not yet done");
    s.advanceFrame();
    ASSERT(s.frame == 1 && s.done, "sprite/once: freezes on the last frame + done");
}

static void test_advance_pingpong() {
    // A 3-frame sheet so the bounce is visible: 0->1->2->1->0->1...
    static const uint8_t kThree[3] = {1, 2, 3};
    SpriteComponent s;
    s.load(kThree, 1, 1, 3, 1);
    s.mode = AnimMode::PingPong;
    s.advanceFrame(); ASSERT(s.frame == 1, "sprite/pingpong: 0 -> 1");
    s.advanceFrame(); ASSERT(s.frame == 2, "sprite/pingpong: 1 -> 2");
    s.advanceFrame(); ASSERT(s.frame == 1, "sprite/pingpong: reverses 2 -> 1");
    s.advanceFrame(); ASSERT(s.frame == 0, "sprite/pingpong: 1 -> 0");
    s.advanceFrame(); ASSERT(s.frame == 1, "sprite/pingpong: reverses 0 -> 1");
}

// Autonomous playback: fps>0 advances the frame once a full period accumulates.
static void test_system_autonomous_advances() {
    SpriteWorld world;
    Canvas4<16, 16> canvas;
    SpriteSystem<SpriteWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    auto* s = world.add<SpriteComponent>(e);
    s->load(kTwoFrame, 1, 1, 2, 1);
    s->fps = 10.0f; // one frame per 0.1s
    world.add<PositionComponent>(e, Point(2, 2));

    system.update(0.05f); // half a period — no advance yet
    ASSERT(s->frame == 0, "sprite/system: sub-period tick does not advance");
    system.update(0.06f); // accumulates past 0.1s
    ASSERT(s->frame == 1, "sprite/system: a full period advances one frame");
    ASSERT(canvas.getPixel(2, 2) == 9, "sprite/system: blits the current frame value");
}

// Frame-driven: fps<=0 never advances, so the authored/bound frame stands.
static void test_system_frame_driven_no_autoadvance() {
    SpriteWorld world;
    Canvas4<16, 16> canvas;
    SpriteSystem<SpriteWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    auto* s = world.add<SpriteComponent>(e);
    s->load(kTwoFrame, 1, 1, 2, 1);
    s->fps = 0.0f;  // externally driven
    s->frame = 1;   // as if written by a `frame` binding this tick
    world.add<PositionComponent>(e, Point(5, 5));

    canvas.clear(Pixel4(2));
    system.update(1.0f); // a whole second — must NOT advance
    ASSERT(s->frame == 1, "sprite/frame-driven: fps<=0 never auto-advances");
    ASSERT(canvas.getPixel(5, 5) == 9, "sprite/frame-driven: blits the bound frame");
}

// A hidden sprite draws nothing; a null sheet is a safe no-op.
static void test_system_hidden_and_null_safe() {
    SpriteWorld world;
    Canvas4<16, 16> canvas;
    SpriteSystem<SpriteWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity hidden = world.create();
    auto* hs = world.add<SpriteComponent>(hidden);
    hs->load(kTwoFrame, 1, 1, 2, 1);
    hs->setVisible(false);
    world.add<PositionComponent>(hidden, Point(0, 0));

    Entity nul = world.create();
    world.add<SpriteComponent>(nul); // no sheet
    world.add<PositionComponent>(nul, Point(1, 1));

    canvas.clear(Colors::BLACK);
    system.update(0.5f); // must not draw and must not deref null

    bool anyInk = false;
    for (int16_t y = 0; y < 16; ++y)
        for (int16_t x = 0; x < 16; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "sprite/system: hidden + null sheets draw nothing");
}

int main() {
    test_sample_frame_major();
    test_advance_loop();
    test_advance_once_freezes();
    test_advance_pingpong();
    test_system_autonomous_advances();
    test_system_frame_driven_no_autoadvance();
    test_system_hidden_and_null_safe();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
