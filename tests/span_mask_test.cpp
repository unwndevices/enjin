// Span masks, the compositor's masked layers and cover, and Transition (#455).
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/graphics/span_mask.hpp>
#include <enjin2/ui/transition.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace enjin2;

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
    } while(0)

// Pixel (x, y) is in the disc when its centre is within r of (cx, cy).
static bool inDisc(int x, int y, int cx, int cy, int r) {
    const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
    return r > 0 && dx * dx + dy * dy <= double(r) * r;
}

template <uint16_t W, uint16_t H>
static bool discMatches(int cx, int cy, int r) {
    SpanMask<W, H> m;
    m.disc(cx, cy, r);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (m.contains(x, y) != inDisc(x, y, cx, cy, r)) return false;
    return true;
}

static void testDiscSpans() {
    bool ok = true;
    const int centres[][2] = {{0, 0}, {10, 7}, {-5, -3}, {60, 70}, {32, 32}, {63, 0}};
    for (const auto& c : centres)
        for (int r = 0; r <= 100; r += 3) ok = ok && discMatches<64, 64>(c[0], c[1], r);
    ASSERT(ok, "disc spans match the pixel-centre test (64x64)");
    ok = true;
    for (int r = 0; r <= 80; ++r) ok = ok && discMatches<49, 40>(0, 0, r);
    ASSERT(ok, "disc spans match the pixel-centre test (odd width)");

    SpanMask<64, 64> m;
    m.fill();
    ASSERT(m.area() == 64u * 64u, "fill covers the canvas");
    m.clear();
    ASSERT(m.area() == 0u, "clear covers nothing");
    m.disc(0, 0, 0);
    ASSERT(m.area() == 0u, "radius 0 is empty");
}

static void testCoverage() {
    DiscCoverage<160, 160> lut;
    lut.build(0, 0);
    ASSERT(lut.fullRadius() == 226, "a corner disc covers 160x160 at r = 226 (the far pixel centre is 225.6 away)");
    SpanMask<160, 160> m;
    m.disc(0, 0, lut.fullRadius());
    ASSERT(m.area() == 160u * 160u, "fullRadius covers everything");
    m.disc(0, 0, lut.fullRadius() - 1);
    ASSERT(m.area() < 160u * 160u, "and is the smallest that does");
    ASSERT(lut.radiusFor(0.0f) == 0 && lut.radiusFor(1.0f) == lut.fullRadius(), "the ends");

    bool monotonic = true, enough = true, tight = true;
    int prev = 0;
    for (int i = 1; i < 100; ++i) {
        const float s = i / 100.0f;
        const int r = lut.radiusFor(s);
        if (r < prev) monotonic = false;
        prev = r;
        m.disc(0, 0, r);
        if (m.area() + 0.5f < s * 160 * 160) enough = false;
        m.disc(0, 0, r - 1);
        if (m.area() + 0.5f >= s * 160 * 160 && r > 0) tight = false;
    }
    ASSERT(monotonic, "radiusFor grows with the share");
    ASSERT(enough, "radiusFor's disc covers at least the share");
    ASSERT(tight, "radiusFor is the smallest such radius");
    ASSERT(std::fabs(lut.shareAt(lut.radiusFor(0.5f)) - 0.5f) < 0.02f, "shareAt inverts radiusFor");
}

using Comp = LayerCompositor<64, 64>;

static uint8_t outAt(const Comp& c, int x, int y) {
    return c.output.getPixel(static_cast<int16_t>(x), static_cast<int16_t>(y)).value;
}

static void settle(Comp& c) {
    c.beginFrame();
    c.compositeDirty();
    c.endFrame();
}

static bool tileDirty(const Comp& c, int tx, int ty) {
    const int t = ty * Comp::TILES_X + tx;
    return (c.mergedDirty[t >> 3] & (1u << (t & 7))) != 0;
}

static void testMaskedLayer() {
    static Comp c;
    c.resetDirtyState();
    c.layers[0].clear(Pixel4(1));
    c.layers[1].clear(Pixel4(3));
    settle(c);
    ASSERT(outAt(c, 40, 40) == 3, "unmasked: the top layer wins");

    Comp::Mask m;
    m.disc(0, 0, 30);
    c.setMask(1, m);
    settle(c);
    bool ok = true;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            if (outAt(c, x, y) != (inDisc(x, y, 0, 0, 30) ? 3 : 1)) ok = false;
    ASSERT(ok, "masked: the layer shows only inside its disc");

    // Growing the disc dirties the tiles its ring crosses, no others.
    Comp::Mask grown;
    grown.disc(0, 0, 45);
    c.setMask(1, grown);
    c.beginFrame();
    c.compositeDirty();
    bool ringOnly = true;
    for (int ty = 0; ty < 4; ++ty)
        for (int tx = 0; tx < 4; ++tx) {
            bool changes = false;
            for (int y = ty * 16; y < ty * 16 + 16; ++y)
                for (int x = tx * 16; x < tx * 16 + 16; ++x)
                    if (inDisc(x, y, 0, 0, 30) != inDisc(x, y, 0, 0, 45)) changes = true;
            if (tileDirty(c, tx, ty) != changes) ringOnly = false;
        }
    c.endFrame();
    ASSERT(ringOnly, "a growing mask recomposites its ring's tiles only");
    ASSERT(!tileDirty(c, 0, 0), "the tile inside both discs stays clean");

    c.clearMask(1);
    settle(c);
    ok = true;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            if (outAt(c, x, y) != 3) ok = false;
    ASSERT(ok && !c.isMasked(1), "clearMask: the layer shows everywhere again");

    Comp::Mask none;
    none.clear();
    c.setMask(1, none);
    settle(c);
    ASSERT(outAt(c, 0, 0) == 1 && outAt(c, 63, 63) == 1, "an empty mask hides the layer");
    c.resetDirtyState();
    ASSERT(!c.isMasked(1), "resetDirtyState drops masks");
}

static void testCover() {
    static Comp c;
    c.resetDirtyState();
    c.layers[0].clear(Pixel4(1));
    c.layers[2].clear(Pixel4(4));
    c.tint[2] = Remap::solid(5);
    settle(c);
    ASSERT(outAt(c, 10, 10) == 5, "the tinted top layer");

    Comp::Mask m;
    m.disc(0, 0, 20);
    c.setCover(m, Pixel4(0));
    settle(c);
    bool ok = true;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            if (outAt(c, x, y) != (inDisc(x, y, 0, 0, 20) ? 0 : 5)) ok = false;
    ASSERT(ok, "the cover paints its colour inside its disc, over every layer, untinted");

    c.setCover(m, Pixel4(2));
    settle(c);
    ASSERT(outAt(c, 0, 0) == 2, "recolouring the cover repaints it");

    c.clearCover();
    settle(c);
    ASSERT(outAt(c, 0, 0) == 5 && !c.hasCover(), "clearCover gives the layers back");
}

// A full mask on every layer composites exactly as no mask (random content,
// transparent holes, tints), odd width included.
static void testFullMaskParity() {
    using C = LayerCompositor<49, 40>;
    static C a, b;
    a.resetDirtyState();
    b.resetDirtyState();
    srand(455);
    for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l)
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 49; ++x) {
                const int v = rand() % 9;
                const Pixel4 p(static_cast<uint8_t>(v == 8 ? 15 : v));
                a.layers[l].setPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), p);
                b.layers[l].setPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), p);
            }
    a.tint[1] = b.tint[1] = Remap::towardBg();
    C::Mask full;
    full.fill();
    for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) b.setMask(l, full);
    a.compositeDirty();
    b.compositeDirty();
    bool same = true;
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 49; ++x)
            if (a.output.getPixel(x, y).value != b.output.getPixel(x, y).value) same = false;
    ASSERT(same, "a full mask composites as no mask");

    // A disc mask on layer 1 equals the per-pixel reference.
    C::Mask d;
    d.disc(3, 5, 23);
    b.setMask(1, d);
    for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) b.layers[l].invalidateAll();
    b.compositeDirty();
    bool ref = true;
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 49; ++x) {
            uint8_t v = 0;
            for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
                if (l == 1 && !d.contains(x, y)) continue;
                const uint8_t s = b.layers[l].getPixel(x, y).value;
                if (s != 15) v = b.tint[l].lut[s];
            }
            if (b.output.getPixel(x, y).value != v) ref = false;
        }
    ASSERT(ref, "a disc mask composites as the per-pixel reference");
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static void testTransition() {
    Transition t;
    ASSERT(t.atStart() && !t.running(), "starts at rest at 0");
    t.animateTo(1.0f, 0.1f);
    ASSERT(t.update(0.05f) && near(t.progress(), 0.5f), "the clock: half the time, half way");
    t.update(0.05f);
    ASSERT(t.atEnd(), "and there");
    ASSERT(!t.update(0.05f), "at rest: no movement");

    t.animateTo(0.0f, 0.1f);
    t.update(0.03f);
    t.animateTo(1.0f, 0.1f);  // re-targeted mid-run: no restart
    ASSERT(near(t.progress(), 0.7f), "re-targeting keeps the progress");
    t.update(0.03f);
    ASSERT(near(t.progress(), 1.0f), "and finishes in its share of the time");

    t.seek(0.25f);
    ASSERT(near(t.progress(), 0.25f) && !t.running(), "seek: the input puts it there, clock stopped");
    t.update(1.0f);
    ASSERT(near(t.progress(), 0.25f), "the clock leaves a seek alone");

    Transition e(&Easing::EaseInQuad);
    e.seek(0.5f);
    ASSERT(near(e.value(), 0.25f), "value is the eased progress");
    e.animateTo(2.0f, 0.0f);
    ASSERT(e.atEnd(), "targets clamp; 0 s jumps");
}

int main() {
    testDiscSpans();
    testCoverage();
    testMaskedLayer();
    testCover();
    testFullMaskParity();
    testTransition();
    printf("span_mask_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
