#include <enjin2/graphics/palette.hpp>
#include <cstdio>

using namespace enjin2;

// Simple pass/fail tracking
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
// a. Default palette initialization
// ============================================================
static void test_default_init()
{
    printf("--- Default palette initialization ---\n");

    ASSERT(g_palette.getSize() == 15,
           "Default palette size should be 15");

    RGB c0 = g_palette.getColor(0);
    ASSERT(c0 == RGB(0x1a, 0x1c, 0x2c),
           "Color[0] should be dark navy #1a1c2c");

    RGB c12 = g_palette.getColor(12);
    ASSERT(c12 == RGB(0xf4, 0xf4, 0xf4),
           "Color[12] should be near-white #f4f4f4");

    RGB c14 = g_palette.getColor(14);
    ASSERT(c14 == RGB(0x33, 0x3c, 0x57),
           "Color[14] should be dark slate #333c57");
}

// ============================================================
// b. Transparency
// ============================================================
static void test_transparency()
{
    printf("--- Transparency ---\n");

    ASSERT(g_palette.isTransparent(15) == true,
           "Index 15 should be transparent");
    ASSERT(g_palette.isTransparent(0) == false,
           "Index 0 should not be transparent");
    ASSERT(g_palette.isTransparent(14) == false,
           "Index 14 should not be transparent");
}

// ============================================================
// c. setColor / getColor
// ============================================================
static void test_set_get_color()
{
    printf("--- setColor / getColor ---\n");

    g_palette.setColor(0, 255, 0, 0);
    RGB c = g_palette.getColor(0);
    ASSERT(c == RGB(255, 0, 0),
           "After setColor(0, 255,0,0) getColor(0) should return {255,0,0}");

    // Restore default palette
    bool ok = g_palette.loadPreset("default");
    ASSERT(ok, "loadPreset('default') should succeed after setColor test");

    RGB restored = g_palette.getColor(0);
    ASSERT(restored == RGB(0x1a, 0x1c, 0x2c),
           "Color[0] should be restored to dark navy after loadPreset('default')");
}

// ============================================================
// d. Index wrapping
// ============================================================
static void test_wrapping()
{
    printf("--- Index wrapping ---\n");

    bool ok = g_palette.loadPreset("gameboy");
    ASSERT(ok, "loadPreset('gameboy') should succeed");
    ASSERT(g_palette.getSize() == 4, "Gameboy preset size should be 4");

    // 5 % 4 = 1, so getColor(5) == getColor(1)
    RGB c5 = g_palette.getColor(5);
    RGB c1 = g_palette.getColor(1);
    ASSERT(c5 == c1,
           "With gameboy (size=4), getColor(5) should equal getColor(1) via 5%4");

    // Transparency survives wrapping check
    ASSERT(g_palette.isTransparent(15) == true,
           "isTransparent(15) should still be true with gameboy preset loaded");

    // Restore for subsequent tests
    g_palette.loadPreset("default");
}

// ============================================================
// e. Transparency check before modulo (critical pitfall)
// ============================================================
static void test_transparency_before_modulo()
{
    printf("--- Transparency before modulo ---\n");

    bool ok = g_palette.loadPreset("gameboy"); // size = 4
    ASSERT(ok, "loadPreset('gameboy') should succeed");

    // 15 % 4 = 3, but index 15 MUST be transparent, not resolved to index 3
    ASSERT(g_palette.isTransparent(15) == true,
           "Index 15 must be transparent even with size=4 (not 15%4=3)");

    RGB via_resolve = g_palette.getColor(15);
    RGB via_index3  = g_palette.getColor(3);
    ASSERT(via_resolve != via_index3 || via_resolve == RGB(0, 0, 0),
           "getColor(15) should return black (transparent sentinel), not color at index 3");

    // Restore for subsequent tests
    g_palette.loadPreset("default");
}

// ============================================================
// f. loadPreset
// ============================================================
static void test_load_preset()
{
    printf("--- loadPreset ---\n");

    bool ok_gameboy = g_palette.loadPreset("gameboy");
    ASSERT(ok_gameboy, "loadPreset('gameboy') should return true");
    ASSERT(g_palette.getSize() == 4, "After loading gameboy, size should be 4");

    bool ok_default = g_palette.loadPreset("default");
    ASSERT(ok_default, "loadPreset('default') should return true");
    ASSERT(g_palette.getSize() == 15, "After loading default, size should be 15");

    bool ok_bad = g_palette.loadPreset("nonexistent");
    ASSERT(ok_bad == false, "loadPreset('nonexistent') should return false");
}

// ============================================================
// g. parseHexColor
// ============================================================
static void test_parse_hex_color()
{
    printf("--- parseHexColor ---\n");

    uint8_t r = 0, g = 0, b = 0;

    bool ok1 = parseHexColor("#ff0000", r, g, b);
    ASSERT(ok1, "parseHexColor('#ff0000') should succeed");
    ASSERT(r == 255 && g == 0 && b == 0,
           "parseHexColor('#ff0000') should set r=255 g=0 b=0");

    bool ok2 = parseHexColor("00ff00", r, g, b);
    ASSERT(ok2, "parseHexColor('00ff00') should succeed (no '#')");
    ASSERT(r == 0 && g == 255 && b == 0,
           "parseHexColor('00ff00') should set r=0 g=255 b=0");

    bool ok3 = parseHexColor("#FF00ff", r, g, b);
    ASSERT(ok3, "parseHexColor('#FF00ff') should succeed (mixed case)");
    ASSERT(r == 255 && g == 0 && b == 255,
           "parseHexColor('#FF00ff') should set r=255 g=0 b=255");

    bool ok4 = parseHexColor(nullptr, r, g, b);
    ASSERT(ok4 == false,
           "parseHexColor(nullptr, ...) should return false");
}

// ============================================================
// main
// ============================================================
int main()
{
    printf("=== palette_test ===\n");

    test_default_init();
    test_transparency();
    test_set_get_color();
    test_wrapping();
    test_transparency_before_modulo();
    test_load_preset();
    test_parse_hex_color();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);

    return (failures == 0) ? 0 : 1;
}
