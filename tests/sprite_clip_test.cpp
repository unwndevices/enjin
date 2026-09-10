// sprite_clip_test.cpp — C_Sprite clip playback, polled events, flips, and
// scrub-by-angle (ADR-0003 §5, Tomodachi #81).
#include <enjin2/graphics/sprite.hpp>
#include <enjin2/graphics/njn2.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/sprite.hpp>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                       \
    do {                                        \
        if (!(cond)) {                          \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++;                         \
        } else {                                \
            passes++;                           \
        }                                       \
    } while (0)

// Build a clip with a name, loop mode, and {frameIndex, durationMs, eventId} rows.
static NjnClip makeClip(const char* name, NjnLoopMode mode,
                        std::vector<NjnFrameEntry> frames) {
    NjnClip c{};
    std::memset(c.name, 0, sizeof(c.name));
    std::strncpy(c.name, name, sizeof(c.name) - 1);
    c.loopMode = mode;
    c.frames = std::move(frames);
    return c;
}

// A dummy 32-cell 1×1 sheet so any frameIndex 0..31 is addressable.
static const uint8_t kSheetData[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0, 1,
};
static SpriteSheet dummySheet() { return SpriteSheet(kSheetData, 1, 1, 32, 1); }

// ============================================================
// Loop clip advances by per-frame durations and wraps
// ============================================================
static void test_clip_loop() {
    printf("--- clip: loop advances by duration + wraps ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());
    s.setClips({ makeClip("run", NjnLoopMode::Loop, {
        {10, 100, 0}, {20, 100, 0}, {30, 100, 0} }) });

    ASSERT(s.play("run"), "loop: play('run') succeeds");
    ASSERT(s.getFrame() == 10, "loop: starts on first frame cell (10)");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 20, "loop: cell 20 after 100ms");
    ASSERT(s.justAdvanced(), "loop: justAdvanced true on the change tick");
    ASSERT(!s.justCompleted(), "loop: not completed mid-clip");

    s.lateUpdate(0.05f);
    ASSERT(s.getFrame() == 20, "loop: still cell 20 before duration elapses");
    ASSERT(!s.justAdvanced(), "loop: justAdvanced false when frame unchanged");

    s.lateUpdate(0.05f);
    ASSERT(s.getFrame() == 30, "loop: cell 30 after another 50ms (carry-over)");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 10, "loop: wraps back to cell 10");
    ASSERT(s.justCompleted(), "loop: justCompleted true on wrap");
    ASSERT(!s.isDone(), "loop: never done");
}

// ============================================================
// Once clip freezes on the last frame and reports completion
// ============================================================
static void test_clip_once() {
    printf("--- clip: once freezes + completes ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());
    s.setClips({ makeClip("hit", NjnLoopMode::Once, {
        {1, 100, 0}, {2, 100, 0}, {3, 100, 0} }) });
    s.play("hit");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 2, "once: cell 2 after 100ms");
    ASSERT(!s.isDone(), "once: not done at middle frame");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 3, "once: reaches last cell 3");
    ASSERT(!s.isDone(), "once: last frame still displays before completing");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 3, "once: frozen on last cell");
    ASSERT(s.isDone(), "once: isDone after the last duration elapses");
    ASSERT(s.justCompleted(), "once: justCompleted on the freeze tick");

    s.lateUpdate(0.1f);
    ASSERT(!s.justCompleted(), "once: completion flag resets next tick");
}

// ============================================================
// Per-frame events are latched for the advance tick only
// ============================================================
static void test_clip_frame_event() {
    printf("--- clip: per-frame event polling ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());
    s.setClips({ makeClip("blink", NjnLoopMode::Loop, {
        {0, 100, 0}, {1, 100, 7}, {2, 100, 0} }) });
    s.play("blink");

    ASSERT(s.frameEvent() == 0, "event: none before any advance");

    s.lateUpdate(0.1f);  // → frame with eventId 7
    ASSERT(s.frameEvent() == 7, "event: eventId 7 latched on the tick it is entered");

    s.lateUpdate(0.1f);  // → frame with eventId 0
    ASSERT(s.frameEvent() == 0, "event: back to 0 on the next frame");
}

// ============================================================
// hflip / vflip route through the flip-aware SpriteSheet::draw
// ============================================================
static void test_flip_draw() {
    printf("--- flip: hflip/vflip mirror the blit ---\n");
    // 4×4 single frame, distinct non-transparent indices (no 15).
    static const uint8_t px[16] = {
        1,  2,  3,  4,
        5,  6,  7,  8,
        9, 10, 11, 12,
       13, 14,  1,  2,
    };
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(SpriteSheet(px, 4, 4, 1, 1));  // owner C_Position defaults to (0,0)

    Canvas4<16, 16> canvas;
    canvas.clear(Pixel4(0));
    s.draw(canvas);  // no flip
    ASSERT(canvas.getPixel(0, 0).value == 1, "flip: unflipped top-left is 1");
    ASSERT(canvas.getPixel(3, 0).value == 4, "flip: unflipped top-right is 4");

    canvas.clear(Pixel4(0));
    s.setHFlip(true);
    s.draw(canvas);
    ASSERT(canvas.getPixel(0, 0).value == 4, "flip: hflip puts 4 at top-left");
    ASSERT(canvas.getPixel(3, 0).value == 1, "flip: hflip puts 1 at top-right");

    canvas.clear(Pixel4(0));
    s.setHFlip(false);
    s.setVFlip(true);
    s.draw(canvas);
    ASSERT(canvas.getPixel(0, 0).value == 13, "flip: vflip puts bottom row on top");
}

// ============================================================
// setFrameForAngle scrubs the clip and disables auto-advance
// ============================================================
static void test_scrub_by_angle() {
    printf("--- scrub: setFrameForAngle maps angle to frame ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());
    // 5 authored rotation frames on cells 20..24.
    s.setClips({ makeClip("flip", NjnLoopMode::Once, {
        {20, 100, 0}, {21, 100, 0}, {22, 100, 0}, {23, 100, 0}, {24, 100, 0} }) });
    s.play("flip");

    s.setFrameForAngle(0.0f, 0.0f, 90.0f);
    ASSERT(s.getFrame() == 20, "scrub: min angle → first frame cell 20");

    s.setFrameForAngle(90.0f, 0.0f, 90.0f);
    ASSERT(s.getFrame() == 24, "scrub: max angle → last frame cell 24");

    s.setFrameForAngle(45.0f, 0.0f, 90.0f);
    ASSERT(s.getFrame() == 22, "scrub: mid angle → middle frame cell 22");

    s.setFrameForAngle(-30.0f, 0.0f, 90.0f);
    ASSERT(s.getFrame() == 20, "scrub: below min clamps to first frame");

    s.setFrameForAngle(120.0f, 0.0f, 90.0f);
    ASSERT(s.getFrame() == 24, "scrub: above max clamps to last frame");

    // Scrub mode freezes time-based advance.
    s.lateUpdate(1.0f);
    ASSERT(s.getFrame() == 24, "scrub: lateUpdate does not auto-advance in scrub mode");

    // play() returns to timed playback.
    s.play("flip");
    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 21, "scrub: play() re-enables auto-advance");
}

// ============================================================
// A single tick that advances several frames keeps the event
// ============================================================
static void test_multi_advance_event() {
    printf("--- clip: event survives a multi-frame tick ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());
    s.setClips({ makeClip("burst", NjnLoopMode::Loop, {
        {0, 100, 0}, {1, 100, 5}, {2, 100, 0} }) });
    s.play("burst");

    // 250ms in one tick advances 0→1(evt5)→2(evt0); the event must not be lost.
    s.lateUpdate(0.25f);
    ASSERT(s.getFrame() == 2, "multi: landed on cell 2 after 250ms");
    ASSERT(s.frameEvent() == 5, "multi: eventId 5 survives even though the last frame has no event");
}

// ============================================================
// The opening frame's event is pollable right after play()
// ============================================================
static void test_play_frame0_event() {
    printf("--- clip: frame-0 event on play ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());
    s.setClips({ makeClip("spawn", NjnLoopMode::Once, {
        {9, 100, 3}, {10, 100, 0} }) });
    s.play("spawn");

    ASSERT(s.frameEvent() == 3, "play: opening frame's eventId 3 is reported");
    s.lateUpdate(0.0f);  // a zero-dt tick resets one-shot events
    ASSERT(s.frameEvent() == 0, "play: frame-0 event clears on the next lateUpdate");
}

// ============================================================
// A 2-frame PingPong latches justCompleted on return to frame 0
// ============================================================
static void test_pingpong_two_frame() {
    printf("--- clip: 2-frame pingpong completion ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());
    s.setClips({ makeClip("swing", NjnLoopMode::PingPong, {
        {5, 100, 0}, {6, 100, 0} }) });
    s.play("swing");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 6, "pingpong: forward to cell 6");
    ASSERT(!s.justCompleted(), "pingpong: not complete mid-swing");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 5, "pingpong: turns back to cell 5");
    ASSERT(s.justCompleted(), "pingpong: justCompleted on return to the first frame");

    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 6, "pingpong: forward again");
    ASSERT(!s.justCompleted(), "pingpong: completion flag cleared leaving frame 0");
}

// ============================================================
// A no-clip sprite can leave scrub mode via setMode/setFPS
// ============================================================
static void test_legacy_scrub_resume() {
    printf("--- scrub: legacy sprite resumes timed playback ---\n");
    Object obj;
    C_Sprite s(&obj);
    s.setSheet(dummySheet());  // 32 cells, no clips
    s.setFPS(10.0f);
    s.setMode(AnimMode::Loop);

    s.setFrameForAngle(90.0f, 0.0f, 90.0f);  // scrub over the whole sheet → last cell
    ASSERT(s.getFrame() == 31, "scrub: max angle → last sheet cell 31");
    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 31, "scrub: frozen while scrubbing");

    s.setMode(AnimMode::Loop);  // re-arm timed playback
    s.lateUpdate(0.1f);
    ASSERT(s.getFrame() == 0, "scrub: setMode leaves scrub → legacy advance wraps 31→0");
}

int main() {
    test_clip_loop();
    test_clip_once();
    test_clip_frame_event();
    test_flip_draw();
    test_scrub_by_angle();
    test_multi_advance_event();
    test_play_frame0_event();
    test_pingpong_two_frame();
    test_legacy_scrub_resume();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
