// Parity oracle for LayerCompositor::recompositeTile.
//
// recompositeTile is the compositor's hot inner loop (#66). This test pins its
// exact per-pixel semantics against an INDEPENDENT naive reference so a perf
// rewrite of the loop (packed-buffer iteration, hoisted bounds/visibility,
// two-pixels-per-byte) can be proven visually identical. The reference here is
// deliberately the simple, slow definition and must not change.
//
// Semantics pinned (painter's order, back layer 0 -> front layer N-1):
//   out defaults to background index 0;
//   each VISIBLE layer whose source index != 15 overwrites out with
//   tint[l].apply(source); source index 15 is transparent regardless of tint.

#include <enjin2/graphics/layer_compositor.hpp>
#include <cstdio>
#include <cstdint>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { passes++; } \
    } while (0)

// Tiny deterministic PRNG so runs are reproducible across machines.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x1234567u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    uint8_t nibble() { return static_cast<uint8_t>(next() & 0x0F); }
};

// Independent reference: naive full-canvas composite, per pixel, all layers.
template <uint16_t W, uint16_t H>
static uint8_t referencePixel(const LayerCompositor<W, H>& c, int16_t x, int16_t y) {
    uint8_t out = 0;
    for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
        if (!c.visible[l]) continue;
        const uint8_t s = c.layers[l].getPixel(x, y).value;
        if (s == 0x0F) continue;
        out = c.tint[l].apply(s);
    }
    return out;
}

// Paint each layer with random indices, apply an assortment of tints/visibility,
// force a full recomposite, and compare every output pixel to the reference.
template <uint16_t W, uint16_t H>
static void checkSize(const char* label, uint32_t seed) {
    printf("--- parity %s (%ux%u, %u layers) ---\n", label, W, H, ENJIN_LAYER_COUNT);
    auto c = new LayerCompositor<W, H>();
    Rng rng(seed);

    // Fill every layer with random content (index 15 sprinkled in as transparency).
    for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
        for (int16_t y = 0; y < static_cast<int16_t>(H); ++y) {
            for (int16_t x = 0; x < static_cast<int16_t>(W); ++x) {
                c->layers[l].setPixel(x, y, Pixel4(rng.nibble()));
            }
        }
    }

    // A mix of tints across the layers, plus one hidden layer if there is room.
    if (ENJIN_LAYER_COUNT >= 2) c->setTint(1, Remap::solid(3));
    if (ENJIN_LAYER_COUNT >= 3) c->setTint(2, Remap::darken());
    if (ENJIN_LAYER_COUNT >= 4) c->setVisible(2, false);

    // Force a full-frame recomposite through the real path.
    c->invalidateLayer(0);
    for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) c->invalidateLayer(l);
    c->compositeDirty();

    int mism = 0;
    for (int16_t y = 0; y < static_cast<int16_t>(H) && mism < 5; ++y) {
        for (int16_t x = 0; x < static_cast<int16_t>(W) && mism < 5; ++x) {
            const uint8_t got = c->output.getPixel(x, y).value;
            const uint8_t ref = referencePixel(*c, x, y);
            if (got != ref) {
                fprintf(stderr, "  mismatch at (%d,%d): got %u want %u\n", x, y, got, ref);
                ++mism;
            }
        }
    }
    ASSERT(mism == 0, "every composited pixel matches the naive reference");
    delete c;
}

int main() {
    printf("recomposite_parity_test\n");
    printf("=======================\n");

    // Even width, exact tile grid.
    checkSize<64, 64>("even-exact", 0xA11CE);
    // Odd width: exercises the trailing single-pixel byte at the row end and
    // partial right/bottom tiles.
    checkSize<65, 63>("odd-partial", 0xBEEF1);
    // Non-square, width not a tile multiple (48=3 tiles, 30 -> partial bottom).
    checkSize<48, 30>("nonsquare-partial", 0xC0FFEE);
    // Larger even canvas near the on-device 160x160 logical size.
    checkSize<160, 160>("device-logical", 0xD00D);

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
