// Grab, pull, drag and hit on a Shape group (Tomodachi #336, ADR-0016): the
// Star pulled by its Body and by an arm, dragged by moveTo(), each released
// and left to settle, frame by frame against goldens; and hit tests on a
// deformed frame.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include "shape_grab_golden.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace enjin2;
using namespace enjin2::shape;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

using Layer = Canvas4<160, 160>;

static FixedVec2 px(int x, int y) { return {Fixed::fromInt(x), Fixed::fromInt(y)}; }

static uint32_t layerCrc(const Layer& l) {
    uint32_t c = 0xFFFFFFFFu;
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x) {
            c ^= l.getPixel(int16_t(x), int16_t(y)).value;
            for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    return ~c;
}

static bool sameLayer(const Layer& a, const Layer& b) {
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x)
            if (a.getPixel(int16_t(x), int16_t(y)).value != b.getPixel(int16_t(x), int16_t(y)).value) return false;
    return true;
}

// SHAPE_DUMP=1: the layer as text (debugging the look; '.' is transparent).
static void dump(const char* what, const Layer& l) {
    if (!getenv("SHAPE_DUMP")) return;
    fprintf(stderr, "%s\n", what);
    for (int y = 60; y < 160; ++y) {
        for (int x = 20; x < 150; ++x) {
            const uint8_t v = l.getPixel(int16_t(x), int16_t(y)).value;
            fputc(v == 15 ? '.' : "0123456789ABCDE"[v], stderr);
        }
        fputc('\n', stderr);
    }
}

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static ShapeDrawScratch scratch;

struct Star {
    ShapeBundleResult bundle;
    std::unique_ptr<ShapePackMemory> src;
    ShapePack pack;
    AssetArena arena;
    ShapePackGroup pet;
    ShapeLook look;
};

// The full Star, feet line at (80, 150), drawn and settled asleep at rest.
static bool openStar(Star& s, Layer& layer) {
    const std::string dir = std::string(ENJIN2_SOURCE_DIR) + "/tools/testdata/shapes/";
    const std::vector<uint8_t> ledger = readFile(dir + "ledger.json");
    s.bundle = bundleShapes({{"star.svg", readFile(dir + "star.svg"), readFile(dir + "star.svg.shape.json")}},
                            std::string(reinterpret_cast<const char*>(ledger.data()), ledger.size()));
    if (!s.bundle.ok()) return false;
    s.src.reset(new ShapePackMemory(s.bundle.pack.data(), s.bundle.pack.size()));
    ShapeError e;
    if (!s.pack.open(*s.src, &e)) return false;
    s.arena.allocateBacking(64 * 1024);
    for (uint8_t k = 0; k < SHAPE_SLOT_COUNT; ++k) s.look.set(ShapeSlot(k), 1);
    s.look.colour[0] = 9;
    s.look.colour[1] = 4;
    s.look.colour[2] = 0;
    if (!s.pet.build(s.pack, s.look, s.arena, &e)) return false;
    s.pet.setPosition(px(80, 150));
    layer.clear(Pixel4(15));
    s.pet.flush(layer, scratch);
    for (int f = 0; f < 10; ++f) {
        s.pet.tick(33334);
        s.pet.flush(layer, scratch);
    }
    return s.pet.asleep();
}

// A pixel the last redraw drew topmost for (slot, member), nearest (x, y).
static bool findHit(const Star& s, ShapeSlot slot, uint8_t member, int x, int y, int& ox, int& oy) {
    int best = 1 << 30;
    for (int py = 0; py < 160; ++py)
        for (int qx = 0; qx < 160; ++qx) {
            ShapeSlot hs;
            uint8_t hm;
            if (!s.pet.hit(qx, py, hs, hm) || hs != slot || hm != member) continue;
            const int d = (qx - x) * (qx - x) + (py - y) * (py - y);
            if (d < best) {
                best = d;
                ox = qx;
                oy = py;
            }
        }
    return best != (1 << 30);
}

// SHAPE_GOLDEN=1 shape_grab_test > tests/shape_grab_golden.hpp regenerates the goldens.
static const bool gGolden = getenv("SHAPE_GOLDEN") != nullptr;
static std::vector<uint32_t> gBodyCrcs, gArmCrcs, gDragCrcs;

static void checkGolden(const char* what, const std::vector<uint32_t>& crcs, const uint32_t* golden, size_t n) {
    ASSERT(crcs.size() == n, what);
    for (size_t i = 0; i < n && i < crcs.size(); ++i)
        if (crcs[i] != golden[i]) {
            fprintf(stderr, "  %s frame %zu: 0x%08X, golden 0x%08X\n", what, i, crcs[i], golden[i]);
            ASSERT(false, "every frame matches its golden");
            return;
        }
}

// Run 30 fps frames until the pet sleeps (at most `max`), a CRC per redraw.
static int settle(Star& s, Layer& layer, std::vector<uint32_t>& crcs, int max = 300) {
    int frames = 0;
    while (!s.pet.asleep() && frames < max) {
        s.pet.tick(33334);
        if (s.pet.flush(layer, scratch)) crcs.push_back(layerCrc(layer));
        ++frames;
    }
    return frames;
}

// ---------------------------------------------------------------------------
// Pull the Body: grab → pull → release → settle.
// ---------------------------------------------------------------------------

static void testPullTheBody() {
    Star s;
    static Layer layer, rest;
    ASSERT(openStar(s, layer), "the Star builds and settles");
    rest = layer;
    ShapeSlot slot;
    uint8_t member;
    ASSERT(!s.pet.hit(5, 5, slot, member), "nothing drawn in the corner");
    ASSERT(s.pet.grab(px(5, 5)) < 0, "a grab off the pet grabs nothing");
    ASSERT(s.pet.asleep(), "and wakes nothing");

    // The Body's upper right, clear of the face.
    int bx = 0, by = 0;
    ASSERT(findHit(s, ShapeSlot::Body, 0, 98, 112, bx, by), "the Body is drawn");
    const int h = s.pet.grab(px(bx, by));
    ASSERT(h >= 0 && !s.pet.asleep(), "a grab on the Body takes a node and wakes the pet");

    // Pull it up and to the right over 10 frames, hold 10, release with a throw.
    std::vector<uint32_t> crcs;
    for (int f = 1; f <= 20; ++f) {
        const int k = f < 10 ? f : 10;
        s.pet.pull(h, px(bx + k, by - 2 * k));
        s.pet.tick(33334);
        if (s.pet.flush(layer, scratch)) crcs.push_back(layerCrc(layer));
    }
    dump("rest", rest);
    dump("the Body pulled", layer);
    ASSERT(!s.pet.asleep(), "held, the pet stays awake");
    ASSERT(!sameLayer(layer, rest), "pulled, it stretches");
    for (int x = 0; x < 160; ++x)
        ASSERT(layer.getPixel(int16_t(x), 151).value == rest.getPixel(int16_t(x), 151).value, "the feet stay put");
    s.pet.release(h, px(-60, 240));
    const int frames = settle(s, layer, crcs);
    ASSERT(s.pet.asleep() && frames < 120, "released, it settles within 4 s");
    ASSERT(sameLayer(layer, rest), "back exactly at rest");
    if (gGolden) gBodyCrcs = crcs;
    else checkGolden("body pull", crcs, kBodyPullCrcs, sizeof kBodyPullCrcs / sizeof kBodyPullCrcs[0]);
}

// ---------------------------------------------------------------------------
// Pull an arm (a sway Feature): its angle spring, not the grid.
// ---------------------------------------------------------------------------

static void testPullAnArm() {
    Star s;
    static Layer layer, rest;
    ASSERT(openStar(s, layer), "the Star builds and settles");
    rest = layer;
    int ax = 0, ay = 0;
    ASSERT(findHit(s, ShapeSlot::Arms, 1, 150, 120, ax, ay), "the right arm is drawn");
    const int h = s.pet.grab(px(ax, ay));
    ASSERT(h >= 0, "a grab on the arm");
    const Lattice& lat = s.pet.lattice();
    std::vector<uint32_t> crcs;
    double swing = 0;
    for (int f = 1; f <= 20; ++f) {
        const int k = f < 8 ? f : 8;
        s.pet.pull(h, px(ax + k / 2, ay - 2 * k));  // up: the arm swings up
        s.pet.tick(33334);
        for (uint8_t a = 0; a < lat.swayCount(); ++a) swing = std::fmax(swing, std::fabs(lat.swayAngle(a).raw / 65536.0));
        ASSERT(lat.maxDisplacement() == Fixed{}, "pulling an arm leaves the Lattice home");
        if (s.pet.flush(layer, scratch)) crcs.push_back(layerCrc(layer));
    }
    ASSERT(swing > 0.3, "the arm swings toward the pull");
    dump("the right arm pulled", layer);
    ASSERT(!sameLayer(layer, rest), "the drawn arm moved");
    s.pet.release(h, px(0, 0));
    const int frames = settle(s, layer, crcs);
    ASSERT(s.pet.asleep() && frames < 150, "released, it swings back and settles");
    ASSERT(sameLayer(layer, rest), "back exactly at rest");
    if (gGolden) gArmCrcs = crcs;
    else checkGolden("arm pull", crcs, kArmPullCrcs, sizeof kArmPullCrcs / sizeof kArmPullCrcs[0]);
}

// ---------------------------------------------------------------------------
// Drag: moveTo() every frame, stop, settle where it was left.
// ---------------------------------------------------------------------------

static void testDrag() {
    Star s;
    static Layer layer, there;
    ASSERT(openStar(s, layer), "the Star builds and settles");
    // The oracle: a Star simply placed at the end of the drag.
    Star t;
    ASSERT(openStar(t, there), "a second Star builds");
    t.pet.setPosition(px(110, 140));
    t.pet.flush(there, scratch);

    std::vector<uint32_t> crcs;
    bool lagged = false;
    for (int f = 1; f <= 15; ++f) {
        s.pet.moveTo(px(80 + 2 * f, 150 - (f * 10) / 15));
        s.pet.tick(33334);
        lagged = lagged || s.pet.lattice().maxDisplacement() > Fixed::fromInt(1);
        if (s.pet.flush(layer, scratch)) crcs.push_back(layerCrc(layer));
    }
    dump("dragged", layer);
    ASSERT(lagged, "dragged, the jelly lags behind");
    ASSERT(s.pet.position().x > Fixed::fromInt(95), "the feet follow the drag");
    const int frames = settle(s, layer, crcs);
    ASSERT(s.pet.asleep() && frames < 120, "stopped, it settles within 4 s");
    ASSERT(s.pet.position() == px(110, 140), "exactly where the drag left it");
    ASSERT(sameLayer(layer, there), "drawn as if placed there");
    if (gGolden) gDragCrcs = crcs;
    else checkGolden("drag", crcs, kDragCrcs, sizeof kDragCrcs / sizeof kDragCrcs[0]);

    // setPosition snaps and ends a drag.
    s.pet.moveTo(px(60, 150));
    s.pet.tick(33334);
    s.pet.setPosition(px(70, 150));
    ASSERT(s.pet.position() == px(70, 150), "a snap lands at once");
    for (int f = 0; f < 90; ++f) s.pet.tick(33334);
    if (getenv("SHAPE_VERBOSE"))
        fprintf(stderr, "  after the snap: %.3f, %.3f asleep %d\n", s.pet.position().x.raw / 65536.0,
                s.pet.position().y.raw / 65536.0, int(s.pet.asleep()));
    ASSERT(s.pet.position() == px(70, 150) && s.pet.asleep(), "and the drag is over");
}

// ---------------------------------------------------------------------------
// Hit on a deformed frame.
// ---------------------------------------------------------------------------

static void testHitFollowsTheJelly() {
    Star s;
    static Layer layer, rest;
    ASSERT(openStar(s, layer), "the Star builds and settles");
    rest = layer;
    // Hit on rest, for comparison.
    static int8_t restHit[160][160];
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x) {
            ShapeSlot hs;
            uint8_t hm;
            restHit[y][x] = s.pet.hit(x, y, hs, hm) ? int8_t(hs) : int8_t(-1);
        }

    // Kick it and stop on a bent frame.
    s.pet.impulse(px(92, 104), px(-120, 420));
    for (int f = 0; f < 3; ++f) {
        s.pet.tick(33334);
        s.pet.flush(layer, scratch);
    }
    ASSERT(!sameLayer(layer, rest), "a deformed frame");

    int differs = 0, eyes[2] = {0, 0}, eyeX[2] = {0, 0};
    bool covered = true, inkEyes = true, onlyDrawn = true;
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x) {
            ShapeSlot hs;
            uint8_t hm;
            const bool hitIt = s.pet.hit(x, y, hs, hm);
            const uint8_t px = layer.getPixel(int16_t(x), int16_t(y)).value;
            if (px != 15 && !hitIt) covered = false;
            // A dithered Feature (the antenna, the feet) leaves gaps it still owns.
            if (hitIt && px == 15 && hs != ShapeSlot::Top && hs != ShapeSlot::Legs) onlyDrawn = false;
            if (hitIt && hs == ShapeSlot::Eyes) {
                ++eyes[hm & 1];
                eyeX[hm & 1] += x;
                if (px != 0) inkEyes = false;  // eyes are ink and always on top
            }
            if ((hitIt ? int8_t(hs) : int8_t(-1)) != restHit[y][x]) ++differs;
        }
    ASSERT(covered, "every drawn pixel hits the pet");
    ASSERT(onlyDrawn, "and nothing undrawn does, but a dither's gaps");
    ASSERT(differs > 0, "the hits follow the jelly");
    ASSERT(eyes[0] > 0 && eyes[1] > 0, "both eyes hit");
    ASSERT(inkEyes, "an eye's hit is where the eye is drawn: topmost first");
    ASSERT(eyes[0] && eyes[1] && eyeX[0] / eyes[0] < 80 && eyeX[1] / eyes[1] > 80,
           "member 0 is the left (mirrored) eye, member 1 the right");

    // The Star's eyes are one member drawn twice: the left socket mirrors it.
    const ShapePackEntry* e = s.pack.index().find(ShapeSlot::Eyes, 1);
    std::vector<uint8_t> rec(e ? e->size : 0);
    ShapeRecordView view;
    ASSERT(e && s.src->readAt(e->offset, rec.data(), e->size) && view.parse(rec.data(), rec.size(), nullptr) &&
               view.memberCount() == 1,
           "the Star's eyes are a single member, mirrored on the left");

    // A hit answers for the last drawn frame, not for state since.
    ShapeSlot before;
    uint8_t m0;
    int bx = 0, by = 0;
    findHit(s, ShapeSlot::Body, 0, 80, 120, bx, by);
    s.pet.setPosition(px(20, 150));
    ASSERT(s.pet.hit(bx, by, before, m0) && before == ShapeSlot::Body, "moved but not redrawn: the old masks");
    s.pet.flush(layer, scratch);
    ASSERT(!s.pet.hit(bx, by, before, m0), "redrawn: the new ones");
}

static void printGoldens() {
    printf("// Generated by SHAPE_GOLDEN=1 shape_grab_test (Tomodachi #336): the Star at (80, 150),\n");
    printf("// one CRC-32 of the layer per redraw at 30 fps, from the grab to the settle: its Body\n");
    printf("// pulled and thrown, its right arm pulled up, and a moveTo() drag to (110, 140).\n");
    printf("#pragma once\n#include <cstdint>\n");
    auto list = [](const char* name, const std::vector<uint32_t>& v) {
        printf("static const uint32_t %s[] = {", name);
        for (size_t i = 0; i < v.size(); ++i) printf("%s0x%08Xu", i == 0 ? "\n    " : i % 6 ? ", " : ",\n    ", v[i]);
        printf("\n};\n");
    };
    list("kBodyPullCrcs", gBodyCrcs);
    list("kArmPullCrcs", gArmCrcs);
    list("kDragCrcs", gDragCrcs);
}

int main() {
    testPullTheBody();
    testPullAnArm();
    testDrag();
    testHitFollowsTheJelly();
    if (gGolden) {
        printGoldens();
        return 0;
    }
    printf("shape_grab_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
