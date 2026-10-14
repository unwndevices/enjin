// A Shape group bent by its Lattice (Tomodachi #334, ADR-0016): Bindings,
// the tick seam, wake on impulse / pose / move, and the Star kicked and left
// to settle, frame by frame against goldens.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include "shape_jelly_golden.hpp"

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
            g.impulse(px(80 + 12, 100 - 26), px(-150, 220));  // layer px
            for (int f = 0; f < 3; ++f) ASSERT(g.tick(33334), "a kicked group's field moves every tick");
            ASSERT(g.changed(), "a moving field marks the group for a redraw");

            static Layer got, want;
            got.clear(Pixel4(15));
            g.redraw(got, scratch, item);

            const Lattice& lat = g.lattice();
            const FixedVec2 at = item.at;
            FixedVec2 pts[4];
            for (int i = 0; i < 4; ++i) {
                const FixedVec2 p = kDiamond[i];
                const FixedVec2 local = {mirror ? at.x - p.x : at.x + p.x, at.y + p.y};
                // field: every point moved by the field there; rigid (and sway,
                // until #335): the rest Shape moved by the field at its anchor.
                const FixedVec2 d = b == Binding::Field ? lat.displacementAt(local) : lat.displacementAt(at);
                pts[i] = g.origin() + local + d;
            }
            drawExpected(want, pts);
            ASSERT(sameLayer(got, want), b == Binding::Field ? "a field Shape is bent point by point"
                                                             : "a rigid Shape is carried at its anchor, unbent");
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
    while (!s.pet.asleep() && frames < 300) {
        s.pet.tick(33334);
        if (s.pet.flush(layer, scratch)) {
            ++redraws;
            crcs.push_back(layerCrc(layer));
            snapRedrew = s.pet.asleep();
        }
        ++frames;
    }
    ASSERT(s.pet.asleep(), "the Star settles and sleeps");
    ASSERT(frames < 90, "within 3 s");
    ASSERT(redraws > 10, "and wobbles visibly first");
    // The last frame drawn while it moved is already the rest frame, so the
    // snap has nothing left to draw: no pixel pop.
    ASSERT(sameLayer(layer, rest) && crcs.back() == restCrc, "the last frame is the rest frame");
    ASSERT(!snapRedrew, "the snap redraws nothing (no pop)");

    if (getenv("SHAPE_GOLDEN")) {  // SHAPE_GOLDEN=1 shape_jelly_test > tests/shape_jelly_golden.hpp
        printf("// Generated by SHAPE_GOLDEN=1 shape_jelly_test (Tomodachi #334): the Star kicked\n");
        printf("// at (92, 104) with (-120, 420) px/s, one CRC-32 of the layer per redraw at 30 fps.\n");
        printf("#pragma once\n#include <cstdint>\nstatic const uint32_t kStarRestCrc = 0x%08Xu;\n", restCrc);
        printf("static const uint32_t kStarKickCrcs[] = {");
        for (size_t i = 0; i < crcs.size(); ++i) printf("%s0x%08Xu", i == 0 ? "\n    " : i % 6 ? ", " : ",\n    ", crcs[i]);
        printf("\n};\n");
        exit(0);
    }
    ASSERT(restCrc == kStarRestCrc, "the rest frame matches its golden");
    const size_t n = sizeof kStarKickCrcs / sizeof kStarKickCrcs[0];
    ASSERT(crcs.size() == n, "as many redraws as the golden");
    for (size_t i = 0; i < n && i < crcs.size(); ++i)
        if (crcs[i] != kStarKickCrcs[i]) {
            fprintf(stderr, "  frame %zu: 0x%08X, golden 0x%08X\n", i, crcs[i], kStarKickCrcs[i]);
            ASSERT(false, "every frame matches its golden");
            break;
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

int main() {
    testBindings();
    testGroupWakes();
    testStarKickSettles();
    printf("shape_jelly_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
