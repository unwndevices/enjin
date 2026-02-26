#include <enjin2/graphics/sprite.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/sprite.hpp>
#include <cstdio>

// Test sprite data from Aseprite export
#include "pikachu.h"

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
// SpriteSheet: draw writes non-transparent pixels correctly
// ============================================================
static void test_draw_writes_pixels()
{
    printf("--- draw writes pixels ---\n");

    // 4x4 sprite, 1 frame (1 col, 1 row)
    // All pixels = palette index 3 (non-transparent)
    static const uint8_t data[16] = {
        3, 3, 3, 3,
        3, 3, 3, 3,
        3, 3, 3, 3,
        3, 3, 3, 3,
    };

    SpriteSheet sheet(data, 4, 4, 1, 1);
    Canvas4<16, 16> canvas;
    canvas.clear(Pixel4(0));

    sheet.draw(canvas, 0, 2, 2);

    // Check that pixels at (2,2)-(5,5) are now 3
    ASSERT(canvas.getPixel(2, 2).value == 3,
           "Pixel (2,2) should be 3 after draw");
    ASSERT(canvas.getPixel(5, 5).value == 3,
           "Pixel (5,5) should be 3 after draw");

    // Check that pixel outside the sprite area is still 0
    ASSERT(canvas.getPixel(0, 0).value == 0,
           "Pixel (0,0) should still be 0 (outside sprite)");
    ASSERT(canvas.getPixel(6, 6).value == 0,
           "Pixel (6,6) should still be 0 (outside sprite)");
}

// ============================================================
// SpriteSheet: draw skips transparent pixels (index 15)
// ============================================================
static void test_draw_skips_transparent()
{
    printf("--- draw skips transparent ---\n");

    // 4x4 sprite: first row transparent (15), rest palette index 5
    static const uint8_t data[16] = {
        15, 15, 15, 15,
         5,  5,  5,  5,
         5,  5,  5,  5,
         5,  5,  5,  5,
    };

    SpriteSheet sheet(data, 4, 4, 1, 1);
    Canvas4<16, 16> canvas;
    canvas.clear(Pixel4(7));  // Fill with 7 so we can verify transparent pixels stay 7

    sheet.draw(canvas, 0, 0, 0);

    // Transparent pixels (row 0) should remain as background color 7
    ASSERT(canvas.getPixel(0, 0).value == 7,
           "Pixel (0,0) should be 7 (transparent, unchanged)");
    ASSERT(canvas.getPixel(3, 0).value == 7,
           "Pixel (3,0) should be 7 (transparent, unchanged)");

    // Non-transparent pixels (rows 1-3) should be 5
    ASSERT(canvas.getPixel(0, 1).value == 5,
           "Pixel (0,1) should be 5 (drawn)");
    ASSERT(canvas.getPixel(3, 3).value == 5,
           "Pixel (3,3) should be 5 (drawn)");
}

// ============================================================
// SpriteSheet: draw applies 0x0F mask (upper nibble ignored)
// ============================================================
static void test_draw_pixel_masking()
{
    printf("--- draw pixel masking ---\n");

    // Data with upper nibble set: 0xA3 should be masked to 0x03
    static const uint8_t data[4] = { 0xA3, 0xF5, 0x70, 0xBE };

    SpriteSheet sheet(data, 2, 2, 1, 1);
    Canvas4<16, 16> canvas;
    canvas.clear(Pixel4(0));

    sheet.draw(canvas, 0, 0, 0);

    ASSERT(canvas.getPixel(0, 0).value == 3,
           "0xA3 masked to 3");
    ASSERT(canvas.getPixel(1, 0).value == 5,
           "0xF5 masked to 5");
    ASSERT(canvas.getPixel(0, 1).value == 0,
           "0x70 masked to 0");
    // 0xBE & 0x0F = 14
    ASSERT(canvas.getPixel(1, 1).value == 14,
           "0xBE masked to 14");
}

// ============================================================
// SpriteSheet: pikachu data loads and draws correctly
// ============================================================
static void test_pikachu_data()
{
    printf("--- pikachu data ---\n");

    SpriteSheet sheet(pikachu_data, 38, 38, 1, 1);

    ASSERT(sheet.frameCount() == 1,
           "Pikachu should have 1 frame (1x1 grid)");
    ASSERT(sheet.cellW == 38, "Pikachu cellW should be 38");
    ASSERT(sheet.cellH == 38, "Pikachu cellH should be 38");

    // Draw on a canvas large enough to hold the sprite
    Canvas4<128, 128> canvas;
    canvas.clear(Pixel4(0));

    sheet.draw(canvas, 0, 10, 10);

    // Verify some known pixels from the pikachu data:
    // Row 0 of pikachu is all 0x00 (transparent index 0 after mask = 0)
    // The first non-zero pixel in the data is at row 2, around col 5 (data[2*38+5] = 0x03)
    // Let's check that the canvas has data where we expect it

    // pikachu_data[0] = 0x00 → palette 0 (not transparent, should be drawn)
    ASSERT(canvas.getPixel(10, 10).value == 0,
           "Pikachu top-left corner pixel should be 0");

    // Verify a non-zero pixel exists somewhere in the sprite area
    bool found_nonzero = false;
    for (int y = 10; y < 48; y++) {
        for (int x = 10; x < 48; x++) {
            if (canvas.getPixel(x, y).value != 0) {
                found_nonzero = true;
                break;
            }
        }
        if (found_nonzero) break;
    }
    ASSERT(found_nonzero, "Pikachu should have non-zero pixels after draw");
}

// ============================================================
// C_Sprite: Loop mode wraps around
// ============================================================
static void test_animation_loop()
{
    printf("--- animation loop ---\n");

    // 4-frame sprite (2x2 grid), 1x1 pixel cells
    static const uint8_t data[4] = { 1, 2, 3, 4 };

    Object obj;
    C_Sprite sprite(&obj, 1, 1);

    SpriteSheet sheet(data, 1, 1, 2, 2);  // 2 cols, 2 rows = 4 frames
    sprite.setSheet(sheet);
    sprite.setMode(AnimMode::Loop);
    sprite.setFPS(10.0f);  // 100ms per frame

    ASSERT(sprite.getFrame() == 0, "Loop: starts at frame 0");

    sprite.lateUpdate(0.1f);  // advance 1 frame (100ms = 0.1s)
    ASSERT(sprite.getFrame() == 1, "Loop: frame 1 after 0.1s");

    sprite.lateUpdate(0.1f);  // advance 1 frame
    ASSERT(sprite.getFrame() == 2, "Loop: frame 2 after 0.2s");

    sprite.lateUpdate(0.1f);  // advance 1 frame
    ASSERT(sprite.getFrame() == 3, "Loop: frame 3 after 0.3s");

    sprite.lateUpdate(0.1f);  // wrap around
    ASSERT(sprite.getFrame() == 0, "Loop: wraps to frame 0 after 0.4s");

    ASSERT(!sprite.isDone(), "Loop: isDone should be false");
}

// ============================================================
// C_Sprite: Once mode freezes on last frame
// ============================================================
static void test_animation_once()
{
    printf("--- animation once ---\n");

    static const uint8_t data[3] = { 1, 2, 3 };

    Object obj;
    C_Sprite sprite(&obj, 1, 1);

    SpriteSheet sheet(data, 1, 1, 3, 1);  // 3 cols, 1 row = 3 frames
    sprite.setSheet(sheet);
    sprite.setMode(AnimMode::Once);
    sprite.setFPS(10.0f);  // 100ms per frame

    ASSERT(sprite.getFrame() == 0, "Once: starts at frame 0");
    ASSERT(!sprite.isDone(), "Once: not done at start");

    sprite.lateUpdate(0.1f);  // → frame 1 (100ms = 0.1s)
    ASSERT(sprite.getFrame() == 1, "Once: frame 1 after 0.1s");
    ASSERT(!sprite.isDone(), "Once: not done at frame 1");

    sprite.lateUpdate(0.1f);  // → frame 2 (last), but not done yet (frame must display)
    ASSERT(sprite.getFrame() == 2, "Once: frame 2 after 0.2s");
    ASSERT(!sprite.isDone(), "Once: not done yet at last frame (needs one more tick)");

    sprite.lateUpdate(0.1f);  // attempt to advance past last frame → done
    ASSERT(sprite.getFrame() == 2, "Once: stays at frame 2 after done");
    ASSERT(sprite.isDone(), "Once: isDone after attempting to advance past last frame");
}

// ============================================================
// main
// ============================================================
int main()
{
    printf("=== sprite_test ===\n");

    test_draw_writes_pixels();
    test_draw_skips_transparent();
    test_draw_pixel_masking();
    test_pikachu_data();
    test_animation_loop();
    test_animation_once();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);

    return (failures == 0) ? 0 : 1;
}
