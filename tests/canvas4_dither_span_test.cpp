// Packed Bayer-dithered span fill (Tomodachi #324): Canvas4::drawHLineDither
// must paint exactly the pixels the per-pixel Mask::bayer(level).sample path
// paints (screen-anchored: sampled at absolute x/y plus phase), leave every
// other pixel untouched, and mark dirty tiles like drawHLine.

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/effect.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// A non-uniform background so "untouched" is observable on both nibbles.
template <uint16_t W, uint16_t H>
static void fillNoise(Canvas4<W, H> &c)
{
    for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x)
            c.setPixel(x, y, Pixel4(static_cast<uint8_t>((x * 7 + y * 3 + 1) % 15)));
}

// Reference: the per-pixel path, one Mask::sample per pixel.
template <uint16_t W, uint16_t H>
static void referenceSpan(Canvas4<W, H> &c, int16_t x, int16_t y, int16_t w,
                          uint8_t color, uint8_t level, int16_t px, int16_t py)
{
    const Mask m = Mask::bayer(level);
    for (int16_t i = 0; i < w; ++i)
    {
        const int16_t ax = static_cast<int16_t>(x + i);
        if (m.sample(static_cast<int16_t>(ax + px), static_cast<int16_t>(y + py)))
            c.setPixel(ax, y, Pixel4(color));
    }
}

template <uint16_t W, uint16_t H>
static bool samePixels(const Canvas4<W, H> &a, const Canvas4<W, H> &b)
{
    for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x)
            if (a.getPixel(x, y).value != b.getPixel(x, y).value)
                return false;
    return true;
}

// Every start parity, width, level 0..16 and a spread of phases, on an
// odd-width canvas (padded stride) including spans clipped at both edges.
static void test_matches_per_pixel_mask()
{
    printf("--- Span matches per-pixel Mask::bayer sample ---\n");

    static Canvas4<37, 9> got;
    static Canvas4<37, 9> want;
    static const int16_t phases[] = {0, 1, 2, 3, -1, -6, 5, 1001};
    static const int16_t rows[] = {0, 3, 8};

    size_t bad = 0, cases = 0;
    for (int16_t y : rows)
        for (int16_t x = -5; x < 42; x += (x >= 0 && x < 8) ? 1 : 3)
            for (int16_t w = 0; w <= 21; ++w)
                for (uint8_t level = 0; level <= 16; ++level)
                    for (int16_t px : phases)
                        for (int16_t py : phases)
                        {
                            fillNoise(got);
                            fillNoise(want);
                            const uint8_t color = static_cast<uint8_t>(level % 16 == 0 ? 14 : level);
                            got.drawHLineDither(x, y, w, Pixel4(color), level, px, py);
                            referenceSpan(want, x, y, w, color, level, px, py);
                            ++cases;
                            if (!samePixels(got, want))
                            {
                                if (bad < 5)
                                    fprintf(stderr, "  mismatch x=%d y=%d w=%d level=%u px=%d py=%d\n",
                                            x, y, w, level, px, py);
                                ++bad;
                            }
                        }
    printf("  %zu cases\n", cases);
    ASSERT(bad == 0, "drawHLineDither == per-pixel Mask::bayer(level).sample for all cases");
}

// Default phase is 0: the pattern is anchored to the screen, not the span.
static void test_screen_anchored_default_phase()
{
    printf("--- Screen-anchored, default phase ---\n");

    Canvas4<16, 4> c;
    c.clear(Pixel4(0));
    // Level 8 row 0 of the Bayer matrix {0,8,2,10} sets columns 0 and 2.
    c.drawHLineDither(1, 0, 4, Pixel4(9), 8);
    ASSERT(c.getPixel(0, 0).value == 0, "x=0 outside the span");
    ASSERT(c.getPixel(1, 0).value == 0, "x=1 (column 1, threshold 8) not set at level 8");
    ASSERT(c.getPixel(2, 0).value == 9, "x=2 (column 2, threshold 2) set at level 8");
    ASSERT(c.getPixel(3, 0).value == 0, "x=3 (column 3, threshold 10) not set");
    ASSERT(c.getPixel(4, 0).value == 9, "x=4 wraps to column 0 (threshold 0), set");
    ASSERT(c.getPixel(5, 0).value == 0, "x=5 outside the span");
}

// Dirty tiles: a painting span marks the same tiles drawHLine would; a
// level-0 span paints nothing and marks nothing.
static void test_dirty_tiles()
{
    printf("--- Dirty tiles ---\n");

    Canvas4<64, 32> a;
    Canvas4<64, 32> b;
    a.clearDirty();
    b.clearDirty();
    a.drawHLineDither(13, 17, 22, Pixel4(5), 8);
    b.drawHLine(13, 17, 22, Pixel4(5));
    bool same = true;
    for (uint16_t ty = 0; ty < Canvas4<64, 32>::TILES_Y; ++ty)
        for (uint16_t tx = 0; tx < Canvas4<64, 32>::TILES_X; ++tx)
            if (a.isTileDirty(tx, ty) != b.isTileDirty(tx, ty))
                same = false;
    ASSERT(same, "dither span marks the same tiles as drawHLine");
    ASSERT(a.isTileDirty(0, 1) && a.isTileDirty(1, 1) && a.isTileDirty(2, 1),
           "span x 13..34 on row 17 marks tiles (0..2, 1)");

    Canvas4<64, 32> z;
    z.clearDirty();
    z.drawHLineDither(0, 0, 64, Pixel4(5), 0);
    ASSERT(z.dirtyTileCount() == 0, "level 0 marks no tiles");
}

int main()
{
    test_matches_per_pixel_mask();
    test_screen_anchored_default_phase();
    test_dirty_tiles();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
