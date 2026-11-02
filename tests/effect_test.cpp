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

static void test_mask_full()
{
    printf("--- Mask::full ---\n");
    Mask m = Mask::full();
    ASSERT(m.sample(0, 0), "full: origin set");
    ASSERT(m.sample(31, 47), "full: arbitrary set");
    ASSERT(m.sample(-5, -9), "full: negative coords set");
}

static void test_mask_hlines()
{
    printf("--- Mask::hlines ---\n");
    Mask m = Mask::hlines(2, 1);  // row 0 set, row 1 clear, repeating
    ASSERT(m.sample(0, 0), "hlines(2): row 0 set");
    ASSERT(!m.sample(0, 1), "hlines(2): row 1 clear");
    ASSERT(m.sample(9, 2), "hlines(2): row 2 set (wraps)");
    ASSERT(!m.sample(9, 3), "hlines(2): row 3 clear");
    ASSERT(m.sample(5, -2), "hlines(2): negative even row set");
    ASSERT(!m.sample(5, -1), "hlines(2): negative odd row clear");
}

static void test_mask_bayer()
{
    printf("--- Mask::bayer ---\n");
    // level 0: nothing set; level 16: everything set.
    Mask none = Mask::bayer(0);
    Mask all = Mask::bayer(16);
    int onNone = 0, onAll = 0, onHalf = 0;
    Mask half = Mask::bayer(8);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            onNone += none.sample(x, y) ? 1 : 0;
            onAll += all.sample(x, y) ? 1 : 0;
            onHalf += half.sample(x, y) ? 1 : 0;
        }
    }
    ASSERT(onNone == 0, "bayer(0): no pixels set");
    ASSERT(onAll == 16, "bayer(16): all pixels set");
    ASSERT(onHalf == 8, "bayer(8): exactly half set");
}

static void test_mask_stripe_tiles_absolute()
{
    printf("--- Mask::stripe tiles at absolute coords ---\n");
    Mask m = Mask::stripe(6, 2);
    // The pattern depends only on absolute (x+y), so it repeats every 6 cells
    // and is continuous across any tile boundary (no origin dependence).
    for (int16_t x = 0; x < 32; ++x) {
        ASSERT(m.sample(x, 0) == m.sample(static_cast<int16_t>(x + 6), 0),
               "stripe: period-6 repeat along x");
        ASSERT(m.sample(x, 0) == m.sample(static_cast<int16_t>(x - 1), 1),
               "stripe: diagonal continuity across a row step");
    }
}

static void test_shade_pixel_masked_vs_unmasked()
{
    printf("--- Effect::shadePixel gates by mask ---\n");
    // Mask: only even rows; remap: solid(3).
    Effect e(Mask::hlines(2, 1), Remap::solid(3));
    ASSERT(e.shadePixel(5, 0, 0) == 3, "row 0 masked -> remapped to 3");
    ASSERT(e.shadePixel(5, 0, 1) == 5, "row 1 unmasked -> source passes through");
    ASSERT(e.shadePixel(15, 0, 0) == 15, "solid keeps transparency on masked pixel");
}

static void test_shade_pixel_absolute_and_phase()
{
    printf("--- Effect::shadePixel is absolute + phased ---\n");
    // hlines even-row mask; the choice depends on absolute y, not a rect origin.
    Effect e(Mask::hlines(2, 1), Remap::solid(7));
    ASSERT(e.shadePixel(2, 0, 16) == 7, "absolute row 16 is even -> masked");
    ASSERT(e.shadePixel(2, 0, 17) == 2, "absolute row 17 is odd -> unmasked");
    // A phase of +1 in y flips even/odd.
    Effect ph(Mask::hlines(2, 1), Remap::solid(7), 0, 1);
    ASSERT(ph.shadePixel(2, 0, 16) == 2, "phaseY=1: row 16 becomes odd -> unmasked");
    ASSERT(ph.shadePixel(2, 0, 17) == 7, "phaseY=1: row 17 becomes even -> masked");
}

static void test_shade_pixel_shader_paints_hole()
{
    printf("--- shader may paint the transparent slot ---\n");
    // Unlike a compositor tint, a shader honours remap entry 15.
    Effect e(Mask::full(), Remap::recolor(15, 4));
    ASSERT(e.shadePixel(15, 3, 3) == 4, "recolor(15,4) paints the hole under a shader");
}

static void test_presets()
{
    printf("--- presets ---\n");
    // dim: full mask, one step toward bg -> text(3) dims to muted(2), everywhere.
    Effect d = Effect::dim();
    ASSERT(d.shadePixel(ROLE_TEXT, 0, 0) == ROLE_MUTED, "dim: text -> muted at origin");
    ASSERT(d.shadePixel(ROLE_TEXT, 9, 5) == ROLE_MUTED, "dim: text -> muted everywhere (full mask)");
    ASSERT(d.shadePixel(ROLE_SURFACE, 0, 0) == ROLE_BG, "dim: surface -> bg");
    ASSERT(d.shadePixel(ROLE_ACCENT, 0, 0) == ROLE_MUTED, "dim: accent -> muted");
    ASSERT(d.shadePixel(15, 0, 0) == 15, "dim: transparency held");

    // scanline: only every other row steps toward bg.
    Effect s = Effect::scanline();
    ASSERT(s.shadePixel(ROLE_TEXT, 0, 0) == ROLE_MUTED, "scanline: even row dims");
    ASSERT(s.shadePixel(ROLE_TEXT, 0, 1) == ROLE_TEXT, "scanline: odd row untouched");

    // fade(0) is a no-op; fade(16) dims the whole region toward bg.
    Effect f0 = Effect::fade(0);
    ASSERT(f0.shadePixel(ROLE_TEXT, 2, 2) == ROLE_TEXT, "fade(0): no coverage, source held");
    Effect f16 = Effect::fade(16);
    ASSERT(f16.shadePixel(ROLE_TEXT, 2, 2) == ROLE_MUTED, "fade(16): full coverage, toward bg");

    // holo/ghost step toward text somewhere in their period, and only there.
    Effect h = Effect::holo(0);
    bool holoActive = false;
    for (int16_t x = 0; x < 12; ++x) {
        const uint8_t v = h.shadePixel(ROLE_SURFACE, x, 0);
        ASSERT(v == ROLE_SURFACE || v == ROLE_MUTED, "holo: surface stays or steps toward text");
        if (v == ROLE_MUTED) holoActive = true;
    }
    ASSERT(holoActive, "holo: steps toward text somewhere along its stripe");

    Effect g = Effect::ghost();
    bool ghostActive = false;
    for (int16_t y = 0; y < 4; ++y)
        for (int16_t x = 0; x < 4; ++x) {
            const uint8_t v = g.shadePixel(ROLE_BG, x, y);
            ASSERT(v == ROLE_BG || v == ROLE_SURFACE, "ghost: bg stays or steps toward text");
            if (v == ROLE_SURFACE) ghostActive = true;
        }
    ASSERT(ghostActive, "ghost: steps toward text somewhere in its dither");
}

static void test_constexpr_usable()
{
    printf("--- constexpr usability ---\n");
    constexpr Effect e = Effect::dim();
    static_assert(e.shadePixel(ROLE_TEXT, 0, 0) == ROLE_MUTED, "dim constexpr steps text toward bg");
    static_assert(Mask::bayer(0).sample(0, 0) == false, "bayer(0) constexpr empty");
    static_assert(Mask::full().sample(-3, 7) == true, "full constexpr set on negatives");
    ASSERT(true, "constexpr usage compiled");
}

int main()
{
    printf("effect_test\n");
    printf("===========\n");

    test_mask_full();
    test_mask_hlines();
    test_mask_bayer();
    test_mask_stripe_tiles_absolute();
    test_shade_pixel_masked_vs_unmasked();
    test_shade_pixel_absolute_and_phase();
    test_shade_pixel_shader_paints_hole();
    test_presets();
    test_constexpr_usable();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
