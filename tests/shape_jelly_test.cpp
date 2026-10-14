// A Shape group bent by its Lattice (Tomodachi #334, ADR-0016): Bindings,
// the tick seam, wake on impulse / pose / move, and the Star kicked and left
// to settle, frame by frame against goldens.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include "shape_jelly_golden.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static ShapeDrawScratch scratch;

// ---------------------------------------------------------------------------
// Bindings, at the generic group: one diamond, field or rigid.
// ---------------------------------------------------------------------------

static const FixedVec2 kDiamond[] = {px(0, -9), px(9, 0), px(0, 9), px(-9, 0)};

struct OneItem : ShapeItemSource {
    Binding binding = Binding::Field;
    FixedVec2 at = px(10, -20);
    bool mirror = false;
    bool forEach(ShapeDrawScratch& s, ShapeEmit emit, void* ctx) const override {
        s.restRings[0] = Ring{kDiamond, 4, true};
        Shape shape{s.restRings, 1, Paint{}};
        return emit(ctx, shape, at, mirror, binding);
    }
};

// The oracle: fill `pts` (already in layer px) with the rasterizer directly.
static void drawExpected(Layer& out, const FixedVec2* pts) {
    static uint32_t words[RowMask::wordsFor(256, 256)];
    Ring ring{pts, 4, true};
    Shape shape{&ring, 1, Paint{}};
    Rasterizer r;
    RowMask mask(words, sizeof words / sizeof words[0]);
    out.clear(Pixel4(15));
    r.fill(shape, Rect(0, 0, 160, 160), mask);
    paintFill(out, mask, ColourRole::Body, Remap::identity());
}

static void testBindings() {
    for (Binding b : {Binding::Field, Binding::Rigid, Binding::Sway})
        for (bool mirror : {false, true}) {
            ShapeGroup g;
            g.setOrigin(px(80, 100));
            g.lattice().configure(px(-30, -50), px(30, 10));
            OneItem item;
            item.binding = b;
            item.mirror = mirror;
            // A sway item swings on the arm hung at its anchor (a 9 px lever, down).
            if (b == Binding::Sway) g.lattice().addSway(item.at, px(0, 9));
            g.impulse(px(80 + 12, 100 - 26), px(-900, 700));  // layer px
            for (int f = 0; f < 6; ++f) ASSERT(g.tick(33334), "a kicked group's field moves every tick");
            ASSERT(g.changed(), "a moving field marks the group for a redraw");
            const double th = g.lattice().swayAngleAt(item.at).raw / 65536.0;
            ASSERT(b != Binding::Sway || std::fabs(th) > 0.05, "the kick swings the sway arm");

            static Layer got, want;
            got.clear(Pixel4(15));
            g.redraw(got, scratch, item);

            const Lattice& lat = g.lattice();
            const FixedVec2 at = item.at;
            FixedVec2 pts[4];
            for (int i = 0; i < 4; ++i) {
                const FixedVec2 p = kDiamond[i];
                const FixedVec2 local = {mirror ? at.x - p.x : at.x + p.x, at.y + p.y};
                // field: every point moved by the field there; rigid: the rest
                // Shape moved by the field at its anchor; sway: that, turned
                // about the anchor by the arm's angle.
                const FixedVec2 d = b == Binding::Field ? lat.displacementAt(local) : lat.displacementAt(at);
                FixedVec2 q = local;
                if (b == Binding::Sway) {
                    const double rx = (local.x - at.x).raw, ry = (local.y - at.y).raw;
                    q = {at.x + Fixed::fromRaw(int32_t(std::lround(rx * std::cos(th) - ry * std::sin(th)))),
                         at.y + Fixed::fromRaw(int32_t(std::lround(rx * std::sin(th) + ry * std::cos(th))))};
                }
                pts[i] = g.origin() + q + d;
            }
            drawExpected(want, pts);
            ASSERT(sameLayer(got, want), b == Binding::Field  ? "a field Shape is bent point by point"
                                         : b == Binding::Rigid ? "a rigid Shape is carried at its anchor, unbent"
                                                               : "a sway Shape is carried and turned about its anchor");
            if (b == Binding::Sway) {
                static Layer rigid;
                OneItem r = item;
                r.binding = Binding::Rigid;
                ShapeGroup copy = g;
                rigid.clear(Pixel4(15));
                copy.redraw(rigid, scratch, r);
                ASSERT(!sameLayer(got, rigid), "swung, a sway Shape draws unlike a rigid one");
            }
        }

    // The two differ once the field is uneven over the Shape.
    ShapeGroup g;
    g.lattice().configure(px(-30, -50), px(30, 10));
    g.setOrigin(px(80, 100));
    g.impulse(px(90, 80), px(0, 400));
    g.tick(33334);
    g.tick(33334);
    static Layer field, rigid;
    OneItem item;
    field.clear(Pixel4(15));
    g.redraw(field, scratch, item);
    item.binding = Binding::Rigid;
    rigid.clear(Pixel4(15));
    g.redraw(rigid, scratch, item);
    ASSERT(!sameLayer(field, rigid), "bent and carried draw differently under an uneven field");
}

static void testGroupWakes() {
    ShapeGroup g;
    g.lattice().configure(px(-30, -50), px(30, 10));
    ASSERT(g.lattice().asleep(), "a configured group's Lattice starts asleep");
    g.setOrigin(px(50, 60));
    ASSERT(!g.lattice().asleep(), "a group move wakes it");
    g.lattice().snap();
    g.impulse(px(50, 40), px(10, 10));
    ASSERT(!g.lattice().asleep(), "an impulse wakes it");
}

// ---------------------------------------------------------------------------
// The Star: kick → settle, frame by frame.
// ---------------------------------------------------------------------------

// star.svg: one antenna (top), two arms, two feet (legs).
static const uint8_t kStarSwaySockets = 5;

// SHAPE_GOLDEN=1 shape_jelly_test > tests/shape_jelly_golden.hpp regenerates the goldens.
static const bool gGolden = getenv("SHAPE_GOLDEN") != nullptr;
static uint32_t gRestCrc = 0;
static std::vector<uint32_t> gKickCrcs, gBreathCrcs;

static void printGoldens() {
    printf("// Generated by SHAPE_GOLDEN=1 shape_jelly_test (Tomodachi #334, #335): the Star kicked\n");
    printf("// at (92, 104) with (-120, 420) px/s, one CRC-32 of the layer per redraw at 30 fps;\n");
    printf("// and its bare Body (body, eyes, mouth) breathing, one CRC-32 per breath step.\n");
    printf("#pragma once\n#include <cstdint>\nstatic const uint32_t kStarRestCrc = 0x%08Xu;\n", gRestCrc);
    auto list = [](const char* name, const std::vector<uint32_t>& v) {
        printf("static const uint32_t %s[] = {", name);
        for (size_t i = 0; i < v.size(); ++i) printf("%s0x%08Xu", i == 0 ? "\n    " : i % 6 ? ", " : ",\n    ", v[i]);
        printf("\n};\n");
    };
    list("kStarKickCrcs", gKickCrcs);
    list("kStarBreathCrcs", gBreathCrcs);
}

struct Star {
    ShapeBundleResult bundle;
    std::unique_ptr<ShapePackMemory> src;
    ShapePack pack;
    AssetArena arena;
    ShapePackGroup pet;
    ShapeLook look;
};

static bool openStar(Star& s) {
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
    return true;
}

static void testStarKickSettles() {
    Star s;
    ASSERT(openStar(s), "the Star bundles and builds");
    if (!s.pet.built()) return;
    static Layer layer, rest;
    layer.clear(Pixel4(15));
    ASSERT(s.pet.flush(layer, scratch), "the first flush draws the pet at rest");
    rest = layer;
    const uint32_t restCrc = layerCrc(rest);

    const Lattice& lat = s.pet.lattice();
    ASSERT(lat.configured(), "the pet's Lattice is laid over its Look");
    // The Star spans x ±36 and y −39 (the antenna's tip) .. +28 (the feet);
    // the box adds 3 px.
    ASSERT(lat.boxMin() == px(-39, -42) && lat.boxMax() == px(39, 31), "the box is the Look's extent plus 3 px");
    // The Star's sway Features (the bundle's defaults: top, arms, legs) hang
    // an arm on every socket they sit on.
    ASSERT(lat.swayCount() == kStarSwaySockets, "a sway arm per sway socket");
    // Placing it was a move, so it is awake, but at rest nothing changes and
    // it falls asleep after K ticks.
    for (int f = 0; f < 10; ++f) {
        ASSERT(!s.pet.tick(33334), "at rest: no change");
        ASSERT(!s.pet.flush(layer, scratch), "at rest: no redraw");
    }
    ASSERT(s.pet.asleep(), "a new pet at rest falls asleep");

    // The kick: up and to the right of the Body's centre, down and a little left.
    s.pet.impulse(px(92, 104), px(-120, 420));
    ASSERT(!s.pet.asleep(), "a kick wakes the pet");
    std::vector<uint32_t> crcs;
    int frames = 0, redraws = 0;
    bool snapRedrew = false;
    double swing = 0;
    while (!s.pet.asleep() && frames < 300) {
        s.pet.tick(33334);
        for (uint8_t k = 0; k < lat.swayCount(); ++k)
            swing = std::fmax(swing, std::fabs(lat.swayAngle(k).raw / 65536.0));
        if (s.pet.flush(layer, scratch)) {
            ++redraws;
            crcs.push_back(layerCrc(layer));
            snapRedrew = s.pet.asleep();
        }
        ++frames;
    }
    ASSERT(s.pet.asleep(), "the Star settles and sleeps");
    ASSERT(swing > 0.02, "the kick swings its sway Features");
    ASSERT(frames < 90, "within 3 s");
    ASSERT(redraws > 10, "and wobbles visibly first");
    // The last frame drawn while it moved is already the rest frame, so the
    // snap has nothing left to draw: no pixel pop.
    ASSERT(sameLayer(layer, rest) && crcs.back() == restCrc, "the last frame is the rest frame");
    ASSERT(!snapRedrew, "the snap redraws nothing (no pop)");

    if (gGolden) {
        gRestCrc = restCrc;
        gKickCrcs = crcs;
    } else {
        ASSERT(restCrc == kStarRestCrc, "the rest frame matches its golden");
        const size_t n = sizeof kStarKickCrcs / sizeof kStarKickCrcs[0];
        ASSERT(crcs.size() == n, "as many redraws as the golden");
        for (size_t i = 0; i < n && i < crcs.size(); ++i)
            if (crcs[i] != kStarKickCrcs[i]) {
                fprintf(stderr, "  frame %zu: 0x%08X, golden 0x%08X\n", i, crcs[i], kStarKickCrcs[i]);
                ASSERT(false, "every frame matches its golden");
                break;
            }
    }

    // Asleep: zero sim steps, zero redraws.
    const uint32_t steps = lat.steps();
    for (int f = 0; f < 120; ++f) {
        s.pet.tick(16667);
        ASSERT(!s.pet.flush(layer, scratch), "asleep: zero redraws");
    }
    ASSERT(lat.steps() == steps, "asleep: zero sim steps");

    // A pose swap and a move wake it; a pose swap back to the same pose does not.
    s.pet.setPose(ShapeSlot::Eyes, "open", nullptr);
    ASSERT(s.pet.asleep(), "the pose it already has: still asleep");
    s.pet.setPose(ShapeSlot::Eyes, "closed", nullptr);
    ASSERT(!s.pet.asleep(), "a pose swap wakes it");
    for (int f = 0; f < 10; ++f) s.pet.tick(33334);
    ASSERT(s.pet.asleep(), "and with nothing pushing it, it sleeps again");
    s.pet.setPosition(px(70, 150));
    ASSERT(!s.pet.asleep(), "a group move wakes it");

    // A new Look lays a new Lattice, at rest.
    s.pet.impulse(px(80, 110), px(0, 300));
    s.pet.tick(33334);
    const Fixed topY = lat.boxMin().y;
    ShapeLook other = s.look;
    other.clear(ShapeSlot::Top);
    ShapeError e;
    ASSERT(s.pet.setLook(other, &e), e.text);
    ASSERT(lat.boxMin().y > topY, "a Look without its antenna: a shorter box");
    ASSERT(s.pet.asleep() && s.pet.lattice().maxDisplacement() == Fixed{}, "a new box starts at rest");
}

// ---------------------------------------------------------------------------
// Stepped breathing: whole-pixel steps on a sleeping sim.
// ---------------------------------------------------------------------------

struct InkBox { int x0 = 999, y0 = 999, x1 = -1, y1 = -1; };

static InkBox inkBox(const Layer& l) {
    InkBox b;
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x)
            if (l.getPixel(int16_t(x), int16_t(y)).value != 15) {
                if (x < b.x0) b.x0 = x;
                if (x > b.x1) b.x1 = x;
                if (y < b.y0) b.y0 = y;
                if (y > b.y1) b.y1 = y;
            }
    return b;
}

static void testStarBreathes() {
    Star s;
    ASSERT(openStar(s), "the Star bundles and builds");
    if (!s.pet.built()) return;
    // The bare Body (body, eyes, mouth), so the drawing's extent is the Body's.
    ShapeLook bare;
    bare.set(ShapeSlot::Body, 1);
    bare.set(ShapeSlot::Eyes, 1);
    bare.set(ShapeSlot::Mouth, 1);
    bare.colour[0] = 9;
    bare.colour[1] = 4;
    ShapeError e;
    ASSERT(s.pet.setLook(bare, &e), e.text);
    static Layer layer, rest;
    layer.clear(Pixel4(15));
    s.pet.flush(layer, scratch);
    for (int f = 0; f < 10; ++f) {
        s.pet.tick(33334);
        s.pet.flush(layer, scratch);
    }
    ASSERT(s.pet.asleep(), "the bare Star settles first");
    rest = layer;
    const uint32_t restCrc = layerCrc(rest);
    const InkBox r = inkBox(rest);
    const Lattice& lat = s.pet.lattice();

    ASSERT(!s.pet.breathing(), "a built pet does not breathe");
    Breath b;  // period 3 s, 4 steps, 1 px, about the feet
    ASSERT(b.periodMicros == 3000000 && b.steps == 4 && b.amplitude == 1 && b.pivot == BreathPivot::Feet,
           "the default breath: 3 s, 4 steps, 1 px, feet");
    ASSERT(s.pet.breathe(b, &e), e.text);
    ASSERT(s.pet.breathing(), "breathing");
    ASSERT(!s.pet.changed(), "breath step 0 is the rest shape: nothing to redraw yet");

    const uint32_t steps = lat.steps();
    std::vector<uint32_t> crcs;
    std::vector<InkBox> boxes;
    bool awake = false;
    for (int f = 0; f < 360; ++f) {  // 6 s at 60 fps: two breaths
        s.pet.tick(16667);
        if (s.pet.flush(layer, scratch)) {
            crcs.push_back(layerCrc(layer));
            boxes.push_back(inkBox(layer));
        }
        awake = awake || !s.pet.asleep();
    }
    ASSERT(!awake && lat.steps() == steps, "breathing never wakes the sim: zero sim steps");
    ASSERT(crcs.size() == 8, "two breaths of 4 steps: one redraw per step change, no more");
    if (crcs.size() == 8) {
        // Step 1 (in) is a pixel taller over the same feet line, step 3 (out)
        // a pixel shorter; steps 2 and 0 are the rest frame.
        ASSERT(boxes[0].y0 == r.y0 - 1 && boxes[0].y1 == r.y1, "breathing in: the top rises 1 px, the feet stay");
        ASSERT(boxes[2].y0 == r.y0 + 1 && boxes[2].y1 == r.y1, "breathing out: the top sinks 1 px, the feet stay");
        ASSERT(boxes[0].x0 - r.x0 == r.x1 - boxes[0].x1 && boxes[0].x0 >= r.x0, "in: narrower, both sides alike");
        ASSERT(r.x0 - boxes[2].x0 == boxes[2].x1 - r.x1 && boxes[2].x0 <= r.x0, "out: wider, both sides alike");
        if (getenv("SHAPE_VERBOSE"))
            fprintf(stderr, "  rest %d,%d..%d,%d  in %d,%d..%d,%d  out %d,%d..%d,%d\n", r.x0, r.y0, r.x1, r.y1,
                    boxes[0].x0, boxes[0].y0, boxes[0].x1, boxes[0].y1, boxes[2].x0, boxes[2].y0, boxes[2].x1,
                    boxes[2].y1);
        ASSERT(crcs[1] == restCrc && crcs[3] == restCrc, "between breaths: the rest frame");
        ASSERT(crcs[4] == crcs[0] && crcs[6] == crcs[2] && crcs[7] == restCrc, "the second breath repeats the first");
        if (gGolden) {
            gBreathCrcs = {crcs[0], crcs[2]};
        } else {
            ASSERT(crcs[0] == kStarBreathCrcs[0] && crcs[2] == kStarBreathCrcs[1], "both breath steps match their goldens");
        }
    }

    // About the centre, 1 px moves the top and the feet line both.
    b.pivot = BreathPivot::Centre;
    ASSERT(s.pet.breathe(b, &e), e.text);
    for (int f = 0; f < 45; ++f) s.pet.tick(16667);  // 0.75 s: step 1
    ASSERT(s.pet.flush(layer, scratch), "the first step redraws");
    const InkBox c = inkBox(layer);
    ASSERT(c.y0 == r.y0 - 1 && c.y1 == r.y1 + 1, "about the centre: 1 px up and 1 px down");

    // Off: back to rest at once, then nothing.
    s.pet.stopBreathing();
    ASSERT(!s.pet.breathing(), "stopped");
    ASSERT(s.pet.flush(layer, scratch) && layerCrc(layer) == restCrc, "stopping redraws the rest shape once");
    int later = 0;
    for (int f = 0; f < 180; ++f) {
        s.pet.tick(16667);
        if (s.pet.flush(layer, scratch)) ++later;
    }
    ASSERT(later == 0 && s.pet.asleep(), "and then nothing at all");

    // Bad breaths are refused.
    Breath bad;
    bad.steps = 1;
    ASSERT(!s.pet.breathe(bad, &e) && !s.pet.breathing(), "fewer than 2 steps: refused");
    bad = Breath{};
    bad.steps = 17;
    ASSERT(!s.pet.breathe(bad, &e), "more than 16 steps: refused");
    bad = Breath{};
    bad.periodMicros = 0;
    ASSERT(!s.pet.breathe(bad, &e), "a zero period: refused");

    // Breathing and kicked: the sim wakes for the kick, settles back on the
    // breath's current step and sleeps again.
    ASSERT(s.pet.breathe(Breath{}, &e), e.text);
    s.pet.impulse(px(92, 120), px(-120, 420));
    int frames = 0;
    while (!s.pet.asleep() && frames < 300) {
        s.pet.tick(16667);
        s.pet.flush(layer, scratch);
        ++frames;
    }
    ASSERT(s.pet.asleep() && frames < 180, "a breathing pet still settles after a kick");
}

int main() {
    testBindings();
    testGroupWakes();
    testStarKickSettles();
    testStarBreathes();
    if (gGolden) {
        printGoldens();
        return 0;
    }
    printf("shape_jelly_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
