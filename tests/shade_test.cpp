#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/effect.hpp>
#include <enjin2/graphics/sprite.hpp>
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

// ============================================================
// Canvas4::shade — the rect apply site
// ============================================================
static void test_shade_rect_remaps_masked_pixels()
{
    printf("--- shade(rect) remaps masked pixels ---\n");
    Canvas4<32, 32> canvas;
    canvas.clear(Pixel4(1));  // base shade of ramp 0 everywhere

    // Darken every even row inside a rect (scanline over the region).
    canvas.shade(Rect(4, 4, 8, 8), Effect::scanline());

    ASSERT(canvas.getPixel(4, 4).value == 2, "even row inside rect darkened 1->2");
    ASSERT(canvas.getPixel(4, 5).value == 1, "odd row inside rect untouched");
    ASSERT(canvas.getPixel(0, 4).value == 1, "pixel outside rect untouched");
    ASSERT(canvas.getPixel(4, 12).value == 1, "pixel below rect untouched");
}

static void test_shade_is_seamless_across_tiles()
{
    printf("--- shade(rect) samples at absolute coords (no seam) ---\n");
    // Two adjacent rects on opposite sides of the 16px tile boundary must land
    // the same scanline phase, because the mask keys on absolute y.
    Canvas4<32, 32> a;
    a.clear(Pixel4(1));
    // Shade one big rect crossing the tile boundary at y=16.
    a.shade(Rect(0, 12, 4, 8), Effect::scanline());  // rows 12..19

    Canvas4<32, 32> b;
    b.clear(Pixel4(1));
    // Shade the two halves separately (as two dirty tiles would).
    b.shade(Rect(0, 12, 4, 4), Effect::scanline());  // rows 12..15
    b.shade(Rect(0, 16, 4, 4), Effect::scanline());  // rows 16..19

    for (int16_t y = 12; y < 20; ++y) {
        ASSERT(a.getPixel(0, y).value == b.getPixel(0, y).value,
               "split shade matches whole shade at every row (seamless)");
    }
    // Sanity: row 16 (even, absolute) is darkened in both.
    ASSERT(a.getPixel(0, 16).value == 2, "absolute even row 16 darkened");
}

static void test_shade_marks_dirty_tiles()
{
    printf("--- shade(rect) marks dirty tiles ---\n");
    Canvas4<32, 32> canvas;
    canvas.clear(Pixel4(1));
    canvas.clearDirty();
    ASSERT(!canvas.hasDirty(), "clean after clearDirty()");
    canvas.shade(Rect(0, 0, 8, 8), Effect::dim());
    ASSERT(canvas.isTileDirty(0, 0), "shade dirtied the touched tile (0,0)");
}

// ============================================================
// SpriteSheet::draw(..., Effect) — the sprite apply site
// ============================================================
static void test_sprite_shade_shares_path()
{
    printf("--- sprite draw with effect shares the shader path ---\n");
    // 4x4 frame: a hole at (0,0), base(1) elsewhere.
    static const uint8_t data[16] = {
        15, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    };
    SpriteSheet sheet(data, 4, 4, 1, 1);
    Canvas4<16, 16> canvas;
    canvas.clear(Pixel4(0));

    // dim: full mask + darken. Every opaque pixel 1 -> 2; the hole stays 0.
    sheet.draw(canvas, 0, 2, 2, Effect::dim());

    ASSERT(canvas.getPixel(2, 2).value == 0, "silhouette clip: hole never drawn");
    ASSERT(canvas.getPixel(3, 2).value == 2, "opaque pixel shaded 1->2");
    ASSERT(canvas.getPixel(5, 5).value == 2, "opaque corner shaded 1->2");
}

static void test_sprite_shade_absolute_position()
{
    printf("--- sprite effect samples at absolute dest coords ---\n");
    // A scanline effect through the sprite: the darkened rows depend on the
    // absolute destination y, not the sprite's local row.
    static const uint8_t data[16] = {
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    };
    SpriteSheet sheet(data, 4, 4, 1, 1);
    Canvas4<16, 16> canvas;
    canvas.clear(Pixel4(0));

    sheet.draw(canvas, 0, 0, 3, Effect::scanline());  // top-left at y=3 (odd)
    ASSERT(canvas.getPixel(0, 3).value == 1, "absolute odd row 3 unmasked (stays 1)");
    ASSERT(canvas.getPixel(0, 4).value == 2, "absolute even row 4 darkened to 2");
}

int main()
{
    printf("shade_test\n");
    printf("==========\n");

    test_shade_rect_remaps_masked_pixels();
    test_shade_is_seamless_across_tiles();
    test_shade_marks_dirty_tiles();
    test_sprite_shade_shares_path();
    test_sprite_shade_absolute_position();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
