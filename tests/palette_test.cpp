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
// a. The 8 Palette roles (ADR-0022): job-named indices 0-7, 15 transparent
// ============================================================
static void test_role_indices()
{
    printf("--- Palette roles ---\n");
    ASSERT(ROLE_BG == 0, "bg is index 0");
    ASSERT(ROLE_SURFACE == 1, "surface is index 1");
    ASSERT(ROLE_MUTED == 2, "muted is index 2");
    ASSERT(ROLE_TEXT == 3, "text is index 3");
    ASSERT(ROLE_ACCENT == 4, "accent is index 4");
    ASSERT(ROLE_ACCENT2 == 5, "accent2 is index 5");
    ASSERT(ROLE_ACCENT3 == 6, "accent3 is index 6");
    ASSERT(ROLE_ACCENT4 == 7, "accent4 is index 7");
    ASSERT(PALETTE_TRANSPARENT == 15, "transparent stays index 15");
}

// ============================================================
// b. The house default (placeholder until the designer picks)
// ============================================================
static void test_house_default()
{
    printf("--- House default ---\n");

    ASSERT(g_palette.getColor(ROLE_BG) == RGB(0x08, 0x1a, 0x1c), "bg is #081a1c");
    ASSERT(g_palette.getColor(ROLE_SURFACE) == RGB(0x16, 0x46, 0x32), "surface is #164632");
    ASSERT(g_palette.getColor(ROLE_MUTED) == RGB(0x52, 0xa8, 0x2e), "muted is #52a82e");
    ASSERT(g_palette.getColor(ROLE_TEXT) == RGB(0xe8, 0xf4, 0xcc), "text is #e8f4cc");
    ASSERT(g_palette.getColor(ROLE_ACCENT) == RGB(0xb2, 0xda, 0x63), "accent is #b2da63");

    ASSERT(g_palette.getColor(ROLE_ACCENT2) == RGB(0xf0, 0xa8, 0x30), "accent2 is #f0a830");
    ASSERT(g_palette.getColor(ROLE_ACCENT3) == RGB(0x48, 0xc8, 0xd8), "accent3 is #48c8d8");
    ASSERT(g_palette.getColor(ROLE_ACCENT4) == RGB(0xe8, 0x48, 0x40), "accent4 is #e84840");
    ASSERT(sizeof(g_palette.colors) / sizeof(g_palette.colors[0]) == ROLE_COUNT,
           "the palette holds exactly the 8 roles");
}

// ============================================================
// b2. The seal (ADR-0022): 8-14 are no role
// ============================================================
static void test_sealed_indices()
{
    printf("--- 8-14 are invalid ---\n");
    for (unsigned i = 0; i < 16; ++i) {
        const bool valid = i < ROLE_COUNT || i == PALETTE_TRANSPARENT;
        ASSERT(isPaletteIndex(i) == valid, "isPaletteIndex: roles 0-7 and 15 only");
    }
    ASSERT(!isPaletteIndex(16) && !isPaletteIndex(255), "isPaletteIndex: above 15 is no index");

    // Unused LUT slots: black, and setColor cannot give them a colour.
    for (uint8_t i = ROLE_COUNT; i < PALETTE_TRANSPARENT; ++i) {
        ASSERT(g_palette.getColor(i) == RGB(0, 0, 0), "8-14 resolve to black (getColor)");
        ASSERT(g_palette.resolve(i) == RGB(0, 0, 0), "8-14 resolve to black (resolve)");
    }
    Palette p;
    p.setColor(9, 255, 0, 0);
    p.setColor(PALETTE_TRANSPARENT, 255, 0, 0);
    ASSERT(p.getColor(9) == RGB(0, 0, 0), "setColor(9) is ignored");
    ASSERT(p.getColor(PALETTE_TRANSPARENT) == RGB(0, 0, 0), "setColor(15) is ignored");
    for (uint8_t i = 0; i < ROLE_COUNT; ++i)
        ASSERT(p.getColor(i) == g_palette.getColor(i), "setColor(8+) leaves the roles alone");
}

// ============================================================
// c. Transparency
// ============================================================
static void test_transparency()
{
    printf("--- Transparency ---\n");

    ASSERT(g_palette.isTransparent(15) == true,
           "Index 15 should be transparent");
    ASSERT(g_palette.isTransparent(ROLE_BG) == false,
           "bg should not be transparent");
    ASSERT(g_palette.isTransparent(ROLE_ACCENT4) == false,
           "accent4 should not be transparent");
    ASSERT(g_palette.getColor(15) == RGB(0, 0, 0),
           "getColor(15) is the transparent sentinel, black");
}

// ============================================================
// d. setColor / getColor / loadPreset
// ============================================================
static void test_set_get_color()
{
    printf("--- setColor / getColor ---\n");

    g_palette.setColor(ROLE_BG, 255, 0, 0);
    ASSERT(g_palette.getColor(ROLE_BG) == RGB(255, 0, 0),
           "After setColor(bg, 255,0,0) getColor(bg) should return {255,0,0}");

    bool ok = g_palette.loadPreset("default");
    ASSERT(ok, "loadPreset('default') should succeed after setColor test");
    ASSERT(g_palette.getColor(ROLE_BG) == RGB(0x08, 0x1a, 0x1c),
           "bg should be restored to #081a1c after loadPreset('default')");
}

static void test_load_preset()
{
    printf("--- loadPreset ---\n");
    // The tomo / pico8 / gameboy presets are gone (ADR-0022): only the house
    // default is built in.
    ASSERT(g_palette.loadPreset("tomo") == false, "loadPreset('tomo') is gone");
    ASSERT(g_palette.loadPreset("pico8") == false, "loadPreset('pico8') is gone");
    ASSERT(g_palette.loadPreset("gameboy") == false, "loadPreset('gameboy') is gone");
    ASSERT(g_palette.loadPreset("nonexistent") == false, "loadPreset('nonexistent') fails");
    ASSERT(g_palette.loadPreset(nullptr) == false, "loadPreset(nullptr) fails");
    ASSERT(g_palette.getColor(ROLE_BG) == RGB(0x08, 0x1a, 0x1c), "a failed load leaves the palette");
}

// ============================================================
// e. The shade chain: bg <-> surface <-> muted <-> text
// ============================================================
static void test_toward_bg()
{
    printf("--- towardBg ---\n");
    ASSERT(Palette::towardBg(ROLE_TEXT) == ROLE_MUTED, "text -> muted");
    ASSERT(Palette::towardBg(ROLE_MUTED) == ROLE_SURFACE, "muted -> surface");
    ASSERT(Palette::towardBg(ROLE_SURFACE) == ROLE_BG, "surface -> bg");
    ASSERT(Palette::towardBg(ROLE_BG) == ROLE_BG, "bg stays (end of the chain)");
    ASSERT(Palette::towardBg(ROLE_ACCENT) == ROLE_MUTED, "accent -> muted");
    ASSERT(Palette::towardBg(ROLE_ACCENT2) == ROLE_MUTED, "accent2 -> muted");
    ASSERT(Palette::towardBg(ROLE_ACCENT3) == ROLE_MUTED, "accent3 -> muted");
    ASSERT(Palette::towardBg(ROLE_ACCENT4) == ROLE_MUTED, "accent4 -> muted");
    ASSERT(Palette::towardBg(PALETTE_TRANSPARENT) == PALETTE_TRANSPARENT, "transparency untouched");
}

static void test_toward_text()
{
    printf("--- towardText ---\n");
    ASSERT(Palette::towardText(ROLE_BG) == ROLE_SURFACE, "bg -> surface");
    ASSERT(Palette::towardText(ROLE_SURFACE) == ROLE_MUTED, "surface -> muted");
    ASSERT(Palette::towardText(ROLE_MUTED) == ROLE_TEXT, "muted -> text");
    ASSERT(Palette::towardText(ROLE_TEXT) == ROLE_TEXT, "text stays (end of the chain)");
    ASSERT(Palette::towardText(ROLE_ACCENT) == ROLE_TEXT, "accent -> text");
    ASSERT(Palette::towardText(ROLE_ACCENT4) == ROLE_TEXT, "accent4 -> text");
    ASSERT(Palette::towardText(PALETTE_TRANSPARENT) == PALETTE_TRANSPARENT, "transparency untouched");
}

// Three steps reach the end of the chain from anywhere (the transition dim's
// maximum).
static void test_chain_reaches_bg_in_three()
{
    printf("--- chain length ---\n");
    for (uint8_t i = 0; i < ROLE_COUNT; ++i) {
        uint8_t v = i;
        for (int s = 0; s < 3; ++s) v = Palette::towardBg(v);
        ASSERT(v == ROLE_BG, "three towardBg steps reach bg");
    }
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

    test_role_indices();
    test_house_default();
    test_sealed_indices();
    test_transparency();
    test_set_get_color();
    test_load_preset();
    test_toward_bg();
    test_toward_text();
    test_chain_reaches_bg_in_three();
    test_parse_hex_color();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);

    return (failures == 0) ? 0 : 1;
}
