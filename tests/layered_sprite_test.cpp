/**
 * @file layered_sprite_test.cpp
 * @brief Retained layered-sprite instance: reconstruction + animation control
 *        (ADR-0004, Tomodachi #96).
 *
 * Test IDs:
 *   LAYSP-01  Reconstruction paints overlapping parts in painter order.
 *   LAYSP-02  Transparent pixels inside a part image stay transparent.
 *   LAYSP-03  Parts are clipped to the target canvas (negative offset).
 *   LAYSP-04  Intermittent parts disappear/appear per frame.
 *   LAYSP-05  Authored width/height are stable on every frame.
 *   LAYSP-06  Horizontal flip mirrors parts around the authored canvas extent.
 *   LAYSP-07  Vertical flip mirrors parts around the authored canvas extent.
 *   LAYSP-08  Combined flip mirrors on both axes, still around the extent.
 *   LAYSP-09  Position offsets the whole reconstruction.
 *   LAYSP-10  Clips expose names, indices and frame counts.
 *   LAYSP-11  Timed loop playback honours unequal durations.
 *   LAYSP-12  Timed once playback freezes on the terminal frame.
 *   LAYSP-13  Timed ping-pong reverses at both endpoints.
 *   LAYSP-14  Pause holds the frame; play resumes without losing elapsed time.
 *   LAYSP-15  Explicit frame selection sets a pose and pauses timed playback.
 *   LAYSP-16  Normalized progress is endpoint-exact.
 *   LAYSP-17  Normalized progress is duration-weighted at intermediate boundaries.
 *   LAYSP-18  Increasing and decreasing scrub traverse in either direction.
 *   LAYSP-19  Frame events are suppressed while scrubbing.
 *   LAYSP-20  Timed play resumes from the scrubbed frame.
 *   LAYSP-21  Clip direction (reversed frame order) is respected.
 *   LAYSP-22  An untagged asset gets an enumerable looping default clip.
 *   LAYSP-23  Two instances share one asset's pixels; position/state independent.
 *   LAYSP-24  A store-retained asset survives free() while a sprite is bound.
 */

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/layered_asset.hpp>
#include <enjin2/graphics/layered_sprite.hpp>
#include <enjin2/graphics/njn2.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                             \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            ++failures;                                               \
        } else {                                                      \
            ++passes;                                                 \
        }                                                             \
    } while (0)

// ---------------------------------------------------------------------------
// Direct-view fixture builder (isolates the retained instance from the loader,
// which has its own LAYLA tests).
// ---------------------------------------------------------------------------

struct Fixture {
    std::vector<LayeredImage> images;
    std::vector<LayeredPart> parts;
    std::vector<NjnFramePartRef> refs;
    std::vector<uint16_t> durations;
    std::vector<LayeredClip> clips;
    std::vector<std::vector<NjnFrameEntry>> clipFrames;
    LayeredAsset asset{};

    void addImage(uint16_t w, uint16_t h, std::vector<uint8_t> px) {
        LayeredImage img;
        img.w = w;
        img.h = h;
        img.pixels = nullptr;
        pixels.push_back(std::move(px));
        images.push_back(img);
    }

    void addPart(const char* name) {
        LayeredPart p;
        std::memset(p.name, 0, sizeof(p.name));
        std::strncpy(p.name, name, sizeof(p.name) - 1);
        parts.push_back(p);
    }

    void addClip(const char* name, NjnLoopMode mode, std::vector<NjnFrameEntry> frames) {
        clipFrames.push_back(std::move(frames));
        LayeredClip c;
        std::memset(c.name, 0, sizeof(c.name));
        std::strncpy(c.name, name, sizeof(c.name) - 1);
        c.loopMode = mode;
        c.numFrames = static_cast<uint16_t>(clipFrames.back().size());
        c.frames = clipFrames.back().data();
        clips.push_back(c);
    }

    const LayeredAsset* finish(uint16_t w, uint16_t h) {
        for (size_t i = 0; i < images.size(); ++i) images[i].pixels = pixels[i].data();
        asset.canvasW = w;
        asset.canvasH = h;
        asset.numFrames = static_cast<uint16_t>(durations.size());
        asset.numParts = static_cast<uint16_t>(parts.size());
        asset.numImages = static_cast<uint16_t>(images.size());
        asset.images = images.data();
        asset.parts = parts.data();
        asset.refs = refs.data();
        asset.durations = durations.data();
        asset.clips = clips.data();
        asset.numClips = static_cast<uint16_t>(clips.size());
        return &asset;
    }

private:
    std::vector<std::vector<uint8_t>> pixels;
};

// ---------------------------------------------------------------------------
// Rendering fixture: 6x6 canvas, 3 parts, 3 frames.
//
//   image0 = [1 2 / 3 4]   (2x2)
//   image1 = [5 6 / 7 8]   (2x2)
//   image2 = [9 . / . 9]   (2x2 ring, 15 = transparent)
//   image3 = [10]          (1x1)
//
//   frame 0: base image0@(0,0), over image1@(1,1), spark invisible
//   frame 1: base image0@(0,0), over image1@(1,1), spark image2@(4,2)
//   frame 2: base image0@(-1,2), over invisible, spark invisible
// ---------------------------------------------------------------------------
static Fixture makeRenderAsset() {
    Fixture f;
    f.addImage(2, 2, {1, 2, 3, 4});
    f.addImage(2, 2, {5, 6, 7, 8});
    f.addImage(2, 2, {9, 15, 15, 9});
    f.addImage(1, 1, {10});
    f.addPart("base");
    f.addPart("over");
    f.addPart("spark");
    f.refs = {
        {0, 0, 0}, {1, 1, 1}, {NJN2_LAYERED_INVISIBLE, 0, 0},                        // frame 0
        {0, 0, 0}, {1, 1, 1}, {2, 4, 2},                                             // frame 1
        {0, -1, 2}, {NJN2_LAYERED_INVISIBLE, 0, 0}, {NJN2_LAYERED_INVISIBLE, 0, 0},  // frame 2
    };
    f.durations = {100, 100, 100};
    f.addClip("default", NjnLoopMode::Loop, {{0, 100, 0}, {1, 100, 0}, {2, 100, 0}});
    return f;
}

static constexpr uint16_t CW = 8;
static constexpr uint16_t CH = 8;

static void checkCanvas(const Canvas4<CW, CH>& c, const uint8_t (&expected)[CW * CH],
                        const char* msg) {
    for (uint16_t y = 0; y < CH; ++y) {
        for (uint16_t x = 0; x < CW; ++x) {
            const uint8_t got =
                c.getPixel(static_cast<int16_t>(x), static_cast<int16_t>(y)).value;
            if (got != expected[y * CW + x]) {
                fprintf(stderr, "FAIL: %s at (%u,%u): got %u expected %u\n", msg, x, y, got,
                        expected[y * CW + x]);
                ++failures;
                return;
            }
        }
    }
    ++passes;
}

static void expectRect(uint8_t (&dst)[CW * CH], uint8_t v) {
    for (size_t i = 0; i < CW * CH; ++i) dst[i] = v;
}

// ---------------------------------------------------------------------------
// LAYSP-01..05,09: reconstruction
// ---------------------------------------------------------------------------
static void test_reconstruction() {
    printf("--- LAYSP-01..05,09: reconstruction ---\n");
    Fixture f = makeRenderAsset();
    const LayeredAsset* a = f.finish(6, 6);

    LayeredSprite s;
    s.bind(a);
    ASSERT(s.isBound(), "bound");
    ASSERT(s.width() == 6 && s.height() == 6, "authored extent");
    ASSERT(s.frame() == 0, "starts on the first frame");

    Canvas4<CW, CH> c;

    // Frame 0.
    uint8_t e0[CW * CH];
    expectRect(e0, 15);
    e0[0 * CW + 0] = 1; e0[0 * CW + 1] = 2;
    e0[1 * CW + 0] = 3; e0[1 * CW + 1] = 5; e0[1 * CW + 2] = 6;
    e0[2 * CW + 1] = 7; e0[2 * CW + 2] = 8;
    c.clear(Pixel4(15));
    s.draw(c);
    checkCanvas(c, e0, "LAYSP-01 painter order / overlap");

    // Frame 1: ring appears at (4,2); only its opaque corners draw.
    s.setFrame(1);
    uint8_t e1[CW * CH];
    expectRect(e1, 15);
    e1[0 * CW + 0] = 1; e1[0 * CW + 1] = 2;
    e1[1 * CW + 0] = 3; e1[1 * CW + 1] = 5; e1[1 * CW + 2] = 6;
    e1[2 * CW + 1] = 7; e1[2 * CW + 2] = 8;
    e1[2 * CW + 4] = 9; e1[3 * CW + 5] = 9;
    c.clear(Pixel4(15));
    s.draw(c);
    checkCanvas(c, e1, "LAYSP-02 transparency + LAYSP-04 intermittent");

    // Frame 2: negative offset clips the left column.
    s.setFrame(2);
    uint8_t e2[CW * CH];
    expectRect(e2, 15);
    e2[2 * CW + 0] = 2;
    e2[3 * CW + 0] = 4;
    c.clear(Pixel4(15));
    s.draw(c);
    checkCanvas(c, e2, "LAYSP-03 clipping at negative offset");

    // Stable extent across every frame.
    for (uint16_t fr = 0; fr < 3; ++fr) {
        s.setFrame(fr);
        ASSERT(s.width() == 6 && s.height() == 6, "LAYSP-05 stable authored extent");
    }

    // Position offsets the whole reconstruction.
    s.setFrame(0);
    s.setPosition(2, 1);
    uint8_t e9[CW * CH];
    expectRect(e9, 15);
    e9[1 * CW + 2] = 1; e9[1 * CW + 3] = 2;
    e9[2 * CW + 2] = 3; e9[2 * CW + 3] = 5; e9[2 * CW + 4] = 6;
    e9[3 * CW + 3] = 7; e9[3 * CW + 4] = 8;
    c.clear(Pixel4(15));
    s.draw(c);
    checkCanvas(c, e9, "LAYSP-09 position offset");
}

// ---------------------------------------------------------------------------
// LAYSP-06..08: whole-object flips around the authored extent
// ---------------------------------------------------------------------------
static void test_flips() {
    printf("--- LAYSP-06..08: whole-object flips ---\n");
    Fixture f = makeRenderAsset();
    const LayeredAsset* a = f.finish(6, 6);

    LayeredSprite s;
    s.bind(a);
    s.setFrame(1);  // ring image2@(4,2)
    Canvas4<CW, CH> c;

    // H flip: authored x=4,5 mirror to x=1,0 around the 6-wide extent.
    s.setHFlip(true);
    uint8_t eh[CW * CH];
    expectRect(eh, 15);
    eh[0 * CW + 4] = 2; eh[0 * CW + 5] = 1;
    eh[1 * CW + 3] = 6; eh[1 * CW + 4] = 5; eh[1 * CW + 5] = 3;
    eh[2 * CW + 1] = 9; eh[2 * CW + 3] = 8; eh[2 * CW + 4] = 7;
    eh[3 * CW + 0] = 9;
    c.clear(Pixel4(15));
    s.draw(c);
    checkCanvas(c, eh, "LAYSP-06 horizontal flip");

    // V flip: authored y=2,3 mirror to y=3,2 around the 6-tall extent.
    s.setHFlip(false);
    s.setVFlip(true);
    uint8_t ev[CW * CH];
    expectRect(ev, 15);
    ev[2 * CW + 5] = 9;
    ev[3 * CW + 1] = 7; ev[3 * CW + 2] = 8; ev[3 * CW + 4] = 9;
    ev[4 * CW + 0] = 3; ev[4 * CW + 1] = 5; ev[4 * CW + 2] = 6;
    ev[5 * CW + 0] = 1; ev[5 * CW + 1] = 2;
    c.clear(Pixel4(15));
    s.draw(c);
    checkCanvas(c, ev, "LAYSP-07 vertical flip");

    // Both axes.
    s.setHFlip(true);
    uint8_t eb[CW * CH];
    expectRect(eb, 15);
    eb[2 * CW + 0] = 9;
    eb[3 * CW + 1] = 9; eb[3 * CW + 3] = 8; eb[3 * CW + 4] = 7;
    eb[4 * CW + 3] = 6; eb[4 * CW + 4] = 5; eb[4 * CW + 5] = 3;
    eb[5 * CW + 4] = 2; eb[5 * CW + 5] = 1;
    c.clear(Pixel4(15));
    s.draw(c);
    checkCanvas(c, eb, "LAYSP-08 combined flip");

    ASSERT(s.width() == 6 && s.height() == 6, "flips keep the authored extent");
}

// ---------------------------------------------------------------------------
// Playback fixture: 1x1 canvas, one part, four 1x1 frames, four clips.
//   images: frame f -> image f
//   clip "linear" Loop     [{0,100},{1,200},{2,300,event 7},{3,400}]
//   clip "once"   Once     [{0,100},{1,100},{2,100}]
//   clip "rev"    Loop     [{3,100},{2,100},{1,100},{0,100}]
//   clip "ping"   PingPong [{0,100},{1,100},{2,100}]
// ---------------------------------------------------------------------------
static Fixture makePlaybackAsset(bool withClips) {
    Fixture f;
    for (uint8_t i = 0; i < 4; ++i) f.addImage(1, 1, {static_cast<uint8_t>(i + 1)});
    f.addPart("body");
    for (uint16_t i = 0; i < 4; ++i) f.refs.push_back({i, 0, 0});
    f.durations = {100, 200, 300, 400};
    if (withClips) {
        f.addClip("linear", NjnLoopMode::Loop,
                  {{0, 100, 0}, {1, 200, 0}, {2, 300, 7}, {3, 400, 0}});
        f.addClip("once", NjnLoopMode::Once, {{0, 100, 0}, {1, 100, 0}, {2, 100, 0}});
        f.addClip("rev", NjnLoopMode::Loop,
                  {{3, 100, 0}, {2, 100, 0}, {1, 100, 0}, {0, 100, 0}});
        f.addClip("ping", NjnLoopMode::PingPong, {{0, 100, 0}, {1, 100, 0}, {2, 100, 0}});
    }
    return f;
}

static void test_clip_selection() {
    printf("--- LAYSP-10: clip selection ---\n");
    Fixture f = makePlaybackAsset(true);
    const LayeredAsset* a = f.finish(1, 1);

    LayeredSprite s;
    s.bind(a);
    ASSERT(s.clipCount() == 4, "four clips");
    ASSERT(std::strcmp(s.clipName(0), "linear") == 0, "clip 0 name");
    ASSERT(std::strcmp(s.clipName(3), "ping") == 0, "clip 3 name");
    ASSERT(s.clipName(4) == nullptr, "out-of-range clip name");
    ASSERT(s.findClip("once") == 1, "findClip by name");
    ASSERT(s.findClip("nope") == -1, "findClip missing");
    ASSERT(s.selectedClip() == 0, "first clip selected on bind");
    ASSERT(std::strcmp(s.selectedClipName(), "linear") == 0, "selected name");
    ASSERT(s.clipFrameCount() == 4, "clip frame count");
    ASSERT(s.selectClip("ping"), "select by name");
    ASSERT(s.selectedClip() == 3, "selected index updated");
    ASSERT(s.selectClip(static_cast<uint16_t>(1)), "select by index");
    ASSERT(s.selectedClip() == 1, "selected index by index");
    ASSERT(s.frame() == 0, "select resets to first frame");
}

static void test_timed_playback() {
    printf("--- LAYSP-11..14,21: timed playback ---\n");
    Fixture f = makePlaybackAsset(true);
    const LayeredAsset* a = f.finish(1, 1);

    // LAYSP-11: unequal durations + Loop wrap.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        s.play();
        ASSERT(s.isPlaying(), "LAYSP-11 playing");
        s.update(0.05f);
        ASSERT(s.clipFrame() == 0 && s.frame() == 0, "LAYSP-11 inside first duration");
        s.update(0.06f);
        ASSERT(s.clipFrame() == 1 && s.frame() == 1, "LAYSP-11 advance to unequal second");
        s.update(0.19f);
        ASSERT(s.clipFrame() == 2 && s.frame() == 2, "LAYSP-11 200ms frame elapsed");
        ASSERT(s.justAdvanced() && s.frameEvent() == 7, "LAYSP-11 event latched");
        s.update(0.29f);
        ASSERT(s.clipFrame() == 2, "LAYSP-11 300ms frame holds");
        s.update(0.02f);
        ASSERT(s.clipFrame() == 3 && s.frame() == 3, "LAYSP-11 advance to fourth");
        s.update(0.39f);
        ASSERT(s.clipFrame() == 0 && s.frame() == 0, "LAYSP-11 loop wrap");
        ASSERT(s.justCompleted(), "LAYSP-11 wrap completed");
    }

    // LAYSP-12: Once freezes on the terminal frame.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("once");
        s.play();
        s.update(0.35f);
        ASSERT(s.clipFrame() == 2 && s.frame() == 2, "LAYSP-12 frozen on last");
        ASSERT(s.isDone() && !s.isPlaying(), "LAYSP-12 done");
        ASSERT(s.justCompleted(), "LAYSP-12 completion latched");
        s.update(1.0f);
        ASSERT(s.clipFrame() == 2, "LAYSP-12 stays frozen");
    }

    // LAYSP-13: PingPong reverses at both endpoints.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("ping");
        s.play();
        s.update(0.15f);
        ASSERT(s.clipFrame() == 1, "LAYSP-13 forward to 1");
        s.update(0.1f);
        ASSERT(s.clipFrame() == 2, "LAYSP-13 forward to 2");
        s.update(0.1f);
        ASSERT(s.clipFrame() == 1, "LAYSP-13 reversed off the end");
        s.update(0.1f);
        ASSERT(s.clipFrame() == 0, "LAYSP-13 back to start");
        ASSERT(s.justCompleted(), "LAYSP-13 round trip completed");
        s.update(0.1f);
        ASSERT(s.clipFrame() == 1, "LAYSP-13 forward again");
    }

    // LAYSP-14: pause holds; play resumes without losing elapsed time.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        s.play();
        s.update(0.05f);  // accum 50ms
        s.pause();
        s.update(1.0f);
        ASSERT(s.clipFrame() == 0 && s.isPaused(), "LAYSP-14 paused holds");
        s.play();
        s.update(0.06f);  // 50 + 60 = 110ms
        ASSERT(s.clipFrame() == 1, "LAYSP-14 resumed with retained elapsed time");
    }

    // LAYSP-21: reversed clip direction.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("rev");
        ASSERT(s.clipFrame() == 0 && s.frame() == 3, "LAYSP-21 first frame is authored 3");
        s.play();
        s.update(0.1f);
        ASSERT(s.clipFrame() == 1 && s.frame() == 2, "LAYSP-21 walks backwards");
    }
}

static void test_frame_and_scrub() {
    printf("--- LAYSP-15..20: explicit frame + scrubbing ---\n");
    Fixture f = makePlaybackAsset(true);
    const LayeredAsset* a = f.finish(1, 1);

    // LAYSP-15: explicit frame selection.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        s.play();
        s.setFrame(2);
        ASSERT(s.frame() == 2 && s.clipFrame() == 2, "LAYSP-15 pose selected");
        ASSERT(s.isPaused() && !s.isScrubbing(), "LAYSP-15 pauses timed playback");
    }

    // LAYSP-16: endpoint-exact normalized progress.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        s.setProgress(0.0f);
        ASSERT(s.clipFrame() == 0 && s.frame() == 0, "LAYSP-16 zero -> first");
        ASSERT(s.isScrubbing(), "LAYSP-16 enters scrub");
        s.setProgress(1.0f);
        ASSERT(s.clipFrame() == 3 && s.frame() == 3, "LAYSP-16 one -> last");
    }

    // LAYSP-17: duration-weighted intermediate boundaries.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        const float t[] = {0.05f, 0.1f, 0.29f, 0.3f, 0.59f, 0.6f, 0.99f};
        const uint16_t want[] = {0, 1, 1, 2, 2, 3, 3};
        for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); ++i) {
            s.setProgress(t[i]);
            if (s.clipFrame() != want[i]) {
                fprintf(stderr, "FAIL: LAYSP-17 at t=%.3f got %u want %u\n", t[i], s.clipFrame(),
                        want[i]);
                ++failures;
            } else {
                ++passes;
            }
        }
    }

    // LAYSP-18: increasing and decreasing both traverse without interpolation.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        s.setProgress(0.05f);
        ASSERT(s.clipFrame() == 0, "LAYSP-18 low -> 0");
        s.setProgress(0.5f);
        ASSERT(s.clipFrame() == 2, "LAYSP-18 mid -> 2");
        s.setProgress(0.2f);
        ASSERT(s.clipFrame() == 1, "LAYSP-18 decreasing -> 1");
        s.setProgress(0.9f);
        ASSERT(s.clipFrame() == 3, "LAYSP-18 increasing -> 3");
        s.setProgress(0.0f);
        ASSERT(s.clipFrame() == 0, "LAYSP-18 back to start");
    }

    // LAYSP-19: events suppressed while scrubbing.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        s.setProgress(0.3f);  // clip frame 2 carries event 7
        ASSERT(s.clipFrame() == 2, "LAYSP-19 scrub lands on event frame");
        ASSERT(s.frameEvent() == 0 && !s.justAdvanced(), "LAYSP-19 event suppressed");
        s.update(1.0f);
        ASSERT(s.frameEvent() == 0 && s.clipFrame() == 2, "LAYSP-19 no timed advance while scrub");
    }

    // LAYSP-20: timed play resumes from the scrubbed frame.
    {
        LayeredSprite s;
        s.bind(a);
        s.selectClip("linear");
        s.setProgress(0.35f);
        ASSERT(s.clipFrame() == 2, "LAYSP-20 scrubbed to frame 2");
        s.play();
        ASSERT(!s.isScrubbing() && s.isPlaying(), "LAYSP-20 left scrub");
        s.update(0.29f);
        ASSERT(s.clipFrame() == 2, "LAYSP-20 holds remaining frame duration");
        s.update(0.02f);
        ASSERT(s.clipFrame() == 3, "LAYSP-20 resumes forward from scrubbed frame");
    }
}

static void test_default_clip() {
    printf("--- LAYSP-22: synthesized default clip ---\n");
    Fixture f = makePlaybackAsset(false);  // no CLIP chunk
    const LayeredAsset* a = f.finish(1, 1);

    LayeredSprite s;
    s.bind(a);
    ASSERT(s.clipCount() == 1, "one addressable default clip");
    ASSERT(std::strcmp(s.clipName(0), "default") == 0, "default clip enumerable");
    ASSERT(s.clipName(1) == nullptr, "no second clip");
    ASSERT(s.findClip("default") == 0, "findClip resolves default");
    ASSERT(s.selectedClip() == 0, "default clip selected");
    ASSERT(std::strcmp(s.selectedClipName(), "default") == 0, "default clip name");
    ASSERT(s.clipFrameCount() == 4, "default spans all frames");
    ASSERT(s.playMode() == NjnLoopMode::Loop, "default loops");

    s.play();
    s.update(0.05f);
    ASSERT(s.frame() == 0, "default timed first");
    s.update(0.06f);
    ASSERT(s.frame() == 1, "default timed advance");

    s.setProgress(0.0f);
    ASSERT(s.frame() == 0, "default endpoint zero");
    s.setProgress(1.0f);
    ASSERT(s.frame() == 3, "default endpoint one");
    s.setProgress(0.5f);  // total 1000ms; frame 2 owns [300,600)
    ASSERT(s.frame() == 2, "default duration-weighted midpoint");
}

static void test_sharing() {
    printf("--- LAYSP-23: shared asset, independent instances ---\n");
    Fixture f = makeRenderAsset();
    const LayeredAsset* a = f.finish(6, 6);

    LayeredSprite s1;
    LayeredSprite s2;
    s1.bind(a);
    s2.bind(a);
    ASSERT(s1.asset() == a && s2.asset() == a, "one shared immutable view");
    ASSERT(s1.asset()->images[0].pixels == s2.asset()->images[0].pixels, "shared pixel pointer");

    s1.setPosition(0, 0);
    s2.setPosition(2, 1);
    s1.setFrame(0);
    s2.setFrame(1);
    ASSERT(s1.frame() == 0 && s2.frame() == 1, "independent frames");
    ASSERT(s1.positionX() == 0 && s1.positionY() == 0, "independent position 1");
    ASSERT(s2.positionX() == 2 && s2.positionY() == 1, "independent position 2");

    Canvas4<CW, CH> c1;
    Canvas4<CW, CH> c2;
    c1.clear(Pixel4(15));
    c2.clear(Pixel4(15));
    s1.draw(c1);
    s2.draw(c2);

    uint8_t e1[CW * CH];
    expectRect(e1, 15);
    e1[0 * CW + 0] = 1; e1[0 * CW + 1] = 2;
    e1[1 * CW + 0] = 3; e1[1 * CW + 1] = 5; e1[1 * CW + 2] = 6;
    e1[2 * CW + 1] = 7; e1[2 * CW + 2] = 8;
    checkCanvas(c1, e1, "instance 1 renders its frame");

    uint8_t e2[CW * CH];
    expectRect(e2, 15);
    e2[1 * CW + 2] = 1; e2[1 * CW + 3] = 2;
    e2[2 * CW + 2] = 3; e2[2 * CW + 3] = 5; e2[2 * CW + 4] = 6;
    e2[3 * CW + 3] = 7; e2[3 * CW + 4] = 8;
    e2[3 * CW + 6] = 9; e2[4 * CW + 7] = 9;  // spark at offset (4,2)+(2,1)
    checkCanvas(c2, e2, "instance 2 renders its own frame/position");
}

// ---------------------------------------------------------------------------
// LAYSP-24: store-retained lifetime across free()
// ---------------------------------------------------------------------------
static void test_store_lifetime() {
    printf("--- LAYSP-24: store-retained lifetime ---\n");

    NjnLayered l;
    l.canvasW = 2;
    l.canvasH = 1;
    NjnPartImage img;
    img.w = 1;
    img.h = 1;
    img.pixels = {7};
    l.images.push_back(std::move(img));
    NjnPart part;
    std::memset(part.name, 0, sizeof(part.name));
    part.name[0] = 'p';
    l.parts.push_back(part);
    l.refs.push_back({0, 0, 0});
    l.durations = {100};
    NjnClip clip;
    std::memset(clip.name, 0, sizeof(clip.name));
    std::memcpy(clip.name, "default", 7);
    clip.loopMode = NjnLoopMode::Loop;
    clip.frames.push_back({0, 100, 0});
    l.clips.push_back(std::move(clip));

    NjnV2Writer writer;
    njn2WriteLayered(writer, l);
    std::vector<uint8_t> bytes;
    writer.finalise(bytes);

    AssetArena arena;
    ASSERT(arena.allocateBacking(64 * 1024), "LAYSP-24: arena backing");
    LayeredAssetStore store(arena);
    std::string err;
    const LayeredAssetStore::Handle h = store.loadFromMemory(bytes.data(), bytes.size(), &err);
    ASSERT(h != LayeredAssetStore::INVALID_HANDLE, "LAYSP-24: load via store");

    const LayeredAsset* view = store.retain(h);
    ASSERT(view != nullptr, "LAYSP-24: retain");
    LayeredSprite s;
    s.bind(view);

    store.free(h);  // public handle invalidated; retained view must stay valid
    ASSERT(store.get(h) == nullptr, "LAYSP-24: public handle freed");
    ASSERT(s.isBound() && s.width() == 2, "LAYSP-24: bound sprite survives free");
    Canvas4<4, 4> c;
    c.clear(Pixel4(15));
    s.draw(c);
    ASSERT(c.getPixel(0, 0).value == 7, "LAYSP-24: retained pixels still render");

    store.release(h);
    ASSERT(store.assetCount() == 0, "LAYSP-24: slot reclaimed after last release");
}

int main() {
    printf("=== layered_sprite_test: retained layered sprite (#96) ===\n");
    test_reconstruction();
    test_flips();
    test_clip_selection();
    test_timed_playback();
    test_frame_and_scrub();
    test_default_clip();
    test_sharing();
    test_store_lifetime();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
