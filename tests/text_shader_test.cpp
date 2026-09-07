// Type system (#40): the 1-bit GFX glyph driven through the index shader.
//
// The glyph bitmap is the clip silhouette — its set bits decide *where* to
// draw, and every drawn pixel is funnelled through an Effect (Mask x Remap x
// phase). Colour/outline/holo/dim are Remap compositions; the 1-bit glyph
// format is unchanged. These cases pin:
//   * default (no effect / no outline) writes exactly the glyph silhouette in
//     the plain text colour — the byte-parity contract the waiver table
//     re-pins text_renderer.hpp against;
//   * a solid Remap recolours only the silhouette (the glyph is the clip);
//   * dim / holo compose as Remaps over the fill;
//   * the legibility outline stamps a +/-1 px dark silhouette *behind* the
//     fill (4-connected, so diagonal corners stay clear);
//   * integer-scale (R4) grows the silhouette by whole factors.

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/effect.hpp>
#include <enjin2/graphics/palette.hpp>
#include <enjin2/graphics/text_renderer.hpp>
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
    } while (0)

// A minimal 3x3 solid-block glyph for char 'A' (0x41): every bit set, so the
// silhouette is a filled square and pixel assertions are exact. 9 bits packed
// MSB-first: 0xFF (bits 0..7) + 0x80 (bit 8).
static const uint8_t kBlockBitmaps[] = {0xFF, 0x80};
static const GFXglyph kBlockGlyphs[] = {
    // bitmapOffset, width, height, xAdvance, xOffset, yOffset
    {0, 3, 3, 4, 0, 0},
};
static const GFXfont kBlockFont = {
    (uint8_t*)kBlockBitmaps, (GFXglyph*)kBlockGlyphs, 0x41, 0x41, 5};

static constexpr uint8_t kTransparent = 15;

// Draw the block glyph 'A' with its upper-left corner at (5,5) so both the
// outline offsets and the surrounding transparent region stay on-canvas.
template <typename Setup>
static Canvas4<24, 24> renderBlock(uint8_t color, uint8_t scale, Setup&& setup) {
    Canvas4<24, 24> c;
    c.clear(Pixel4(kTransparent));
    TextRenderer<Pixel4> tr;
    tr.setFont(&kBlockFont);
    tr.setTextColor(Pixel4(color));
    tr.setTextSize(scale);
    setup(tr);
    tr.drawChar(c, 5, 5, 'A');
    return c;
}

static void test_default_is_plain_silhouette() {
    printf("--- default fill: plain colour, glyph is the clip ---\n");
    auto c = renderBlock(4, 1, [](TextRenderer<Pixel4>&) {});
    ASSERT(c.getPixel(5, 5).value == 4, "top-left glyph pixel is the fill colour");
    ASSERT(c.getPixel(6, 6).value == 4, "interior glyph pixel is the fill colour");
    ASSERT(c.getPixel(7, 7).value == 4, "bottom-right glyph pixel is the fill colour");
    ASSERT(c.getPixel(4, 5).value == kTransparent, "left of glyph untouched");
    ASSERT(c.getPixel(8, 5).value == kTransparent, "right of glyph untouched");
    ASSERT(c.getPixel(0, 0).value == kTransparent, "far corner untouched");
}

static void test_solid_remap_recolours_only_silhouette() {
    printf("--- solid Remap recolours the silhouette (clip) ---\n");
    // Effect(full mask, solid(9)) maps every drawn glyph pixel -> 9.
    auto c = renderBlock(4, 1, [](TextRenderer<Pixel4>& tr) {
        tr.setEffect(Effect(Mask::full(), Remap::solid(9)));
    });
    ASSERT(c.getPixel(6, 6).value == 9, "glyph pixel remapped to 9");
    ASSERT(c.getPixel(5, 5).value == 9, "glyph corner remapped to 9");
    ASSERT(c.getPixel(4, 6).value == kTransparent, "outside silhouette untouched (clip)");
}

static void test_dim_composes_as_darken() {
    printf("--- dim() darkens the fill (Remap composition) ---\n");
    const uint8_t base = 4;                  // ramp1 base
    const uint8_t dark = Palette::darken(base);
    ASSERT(dark != base, "fixture: darken(base) differs from base");
    auto c = renderBlock(base, 1, [](TextRenderer<Pixel4>& tr) {
        tr.setEffect(Effect::dim());
    });
    ASSERT(c.getPixel(6, 6).value == dark, "glyph fill darkened one shade");
}

static void test_holo_clips_to_silhouette() {
    printf("--- holo() lightens the diagonal band within the silhouette ---\n");
    // holo is Mask::stripe x Remap::lighten: masked pixels lighten, the rest
    // pass through, and nothing outside the glyph is touched.
    const uint8_t base = 4;
    auto c = renderBlock(base, 1, [](TextRenderer<Pixel4>& tr) {
        tr.setEffect(Effect::holo(0));
    });
    bool anyLight = false, anyBase = false;
    for (int16_t y = 5; y <= 7; ++y)
        for (int16_t x = 5; x <= 7; ++x) {
            uint8_t v = c.getPixel(x, y).value;
            if (v == Palette::lighten(base)) anyLight = true;
            if (v == base) anyBase = true;
            ASSERT(v == base || v == Palette::lighten(base), "glyph pixel is base or lightened");
        }
    ASSERT(anyLight, "at least one glyph pixel lands on the lighten band");
    ASSERT(anyBase, "at least one glyph pixel passes through unmasked");
    ASSERT(c.getPixel(4, 6).value == kTransparent, "outside silhouette untouched");
}

static void test_outline_is_dark_silhouette_behind_fill() {
    printf("--- outline: +/-1 px dark silhouette behind the fill ---\n");
    const uint8_t base = 4;
    const uint8_t dark = Palette::darken(base);
    auto c = renderBlock(base, 1, [](TextRenderer<Pixel4>& tr) {
        tr.setOutline(true);
    });
    // Fill still wins on the interior.
    ASSERT(c.getPixel(6, 6).value == base, "interior fill unchanged by outline");
    // 4-connected outline: one pixel beyond each edge is the dark shade.
    ASSERT(c.getPixel(4, 6).value == dark, "left outline is darken(fill)");
    ASSERT(c.getPixel(8, 6).value == dark, "right outline is darken(fill)");
    ASSERT(c.getPixel(6, 4).value == dark, "top outline is darken(fill)");
    ASSERT(c.getPixel(6, 8).value == dark, "bottom outline is darken(fill)");
    // Diagonal corner is never written (outline is 4-connected, not 8).
    ASSERT(c.getPixel(4, 4).value == kTransparent, "diagonal corner stays clear");
}

static void test_outline_explicit_colour() {
    printf("--- outline: explicit colour overrides darken(fill) ---\n");
    auto c = renderBlock(4, 1, [](TextRenderer<Pixel4>& tr) {
        tr.setOutline(true, Pixel4(12));
    });
    ASSERT(c.getPixel(4, 6).value == 12, "left outline uses the explicit colour");
    ASSERT(c.getPixel(6, 6).value == 4, "interior fill unchanged");
}

static void test_integer_scale_grows_silhouette() {
    printf("--- R4: integer scale grows the silhouette by whole factors ---\n");
    // scale 2 -> a 3x3 glyph becomes a 6x6 block anchored at (5,5).
    auto c = renderBlock(4, 2, [](TextRenderer<Pixel4>&) {});
    ASSERT(c.getPixel(5, 5).value == 4, "scaled block top-left set");
    ASSERT(c.getPixel(10, 10).value == 4, "scaled block bottom-right set (6x6)");
    ASSERT(c.getPixel(11, 11).value == kTransparent, "one past the scaled block untouched");
}

int main() {
    test_default_is_plain_silhouette();
    test_solid_remap_recolours_only_silhouette();
    test_dim_composes_as_darken();
    test_holo_clips_to_silhouette();
    test_outline_is_dark_silhouette_behind_fill();
    test_outline_explicit_colour();
    test_integer_scale_grows_silhouette();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
