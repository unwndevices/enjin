// Span-walker border stroke test (#18 / Tomodachi #39): the computed selection
// frame primitive. Solid draws a hollow ring with rounded corners; bevel splits
// into top/left light + bottom/right dark ramp tones; drop-shadow paints an
// offset filled rounded rect behind the ring. All three walk rows via the
// canvas row-fill — no per-pixel virtual dispatch, no cache.
#include <enjin2/graphics/border.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/palette.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// A solid rounded stroke inks its edges, hollows its interior and rounds off the
// extreme corner — the same contract drawRoundRect had, now via strokeBorder.
static void test_solid_ring() {
    Canvas4<32, 32> canvas;
    canvas.clear(Colors::BLACK);
    BorderStyle st;
    st.color = Pixel4(15);
    st.thickness = 1;
    st.radius = 6;
    st.kind = BorderKind::Solid;
    strokeBorder(canvas, Rect(2, 2, 20, 20), st);

    ASSERT(canvas.getPixel(12, 2) == 15, "solid: top edge midpoint inked");
    ASSERT(canvas.getPixel(12, 21) == 15, "solid: bottom edge midpoint inked");
    ASSERT(canvas.getPixel(2, 12) == 15, "solid: left edge midpoint inked");
    ASSERT(canvas.getPixel(21, 12) == 15, "solid: right edge midpoint inked");
    ASSERT(canvas.getPixel(12, 12) == 0, "solid: interior hollow");
    ASSERT(canvas.getPixel(2, 2) == 0, "solid: extreme corner rounded away");
}

// A thicker stroke inks `thickness` rows/columns of ring (t=3 -> 3 px deep).
static void test_thickness() {
    Canvas4<32, 32> canvas;
    canvas.clear(Colors::BLACK);
    BorderStyle st;
    st.color = Pixel4(15);
    st.thickness = 3;
    st.radius = 0;
    strokeBorder(canvas, Rect(4, 4, 16, 16), st);

    // Left edge: 3 px inked (x = 4,5,6), then hollow at x = 7.
    ASSERT(canvas.getPixel(4, 12) == 15, "thick: outer column inked");
    ASSERT(canvas.getPixel(6, 12) == 15, "thick: inner-most stroke column inked");
    ASSERT(canvas.getPixel(7, 12) == 0, "thick: interior past thickness is hollow");
    // Top band 3 rows deep.
    ASSERT(canvas.getPixel(12, 4) == 15, "thick: top band inked");
    ASSERT(canvas.getPixel(12, 6) == 15, "thick: top band inked to depth");
    ASSERT(canvas.getPixel(12, 7) == 0, "thick: below top band is hollow");
}

// A bevel is a ramp two-tone: top/left take the light shade, bottom/right the
// dark shade, both derived from the base colour's ramp (never stored).
static void test_bevel_two_tone() {
    Canvas4<32, 32> canvas;
    canvas.clear(Colors::BLACK);
    const uint8_t base = Palette::ramp(3, SHADE_BASE);  // scenery ramp base
    BorderStyle st;
    st.color = Pixel4(base);
    st.thickness = 2;
    st.radius = 0;
    st.kind = BorderKind::Bevel;
    strokeBorder(canvas, Rect(4, 4, 16, 16), st);

    const uint8_t light = Palette::lighten(base);
    const uint8_t dark = Palette::darken(base);
    ASSERT(light != dark, "bevel: base ramp has distinct light/dark tones");
    ASSERT(canvas.getPixel(12, 4) == light, "bevel: top edge is the light tone");
    ASSERT(canvas.getPixel(4, 12) == light, "bevel: left edge is the light tone");
    ASSERT(canvas.getPixel(12, 19) == dark, "bevel: bottom edge is the dark tone");
    ASSERT(canvas.getPixel(19, 12) == dark, "bevel: right edge is the dark tone");
}

// A drop shadow paints an offset filled rounded rect behind the ring in the
// one-step-darker tone; the ring itself is drawn on top at the origin.
static void test_drop_shadow() {
    Canvas4<32, 32> canvas;
    canvas.clear(Colors::BLACK);
    const uint8_t base = Palette::ramp(2, SHADE_BASE);
    BorderStyle st;
    st.color = Pixel4(base);
    st.thickness = 1;
    st.radius = 0;
    st.kind = BorderKind::DropShadow;
    st.shadowDx = 3;
    st.shadowDy = 3;
    strokeBorder(canvas, Rect(4, 4, 12, 12), st);

    const uint8_t shadowTone = Palette::darken(base);
    // Shadow fills the offset interior — a point past the ring's own bottom-right
    // edge but inside the shifted shadow rect.
    ASSERT(canvas.getPixel(17, 17) == shadowTone, "shadow: offset fill present");
    // The ring edge still inks at the origin position.
    ASSERT(canvas.getPixel(4, 10) == base, "shadow: ring drawn on top at origin");
}

// A zero-radius stroke is exactly the square outline — no rounding, no gap.
static void test_zero_radius_square() {
    Canvas4<32, 32> canvas;
    canvas.clear(Colors::BLACK);
    BorderStyle st;
    st.color = Pixel4(15);
    st.thickness = 1;
    st.radius = 0;
    strokeBorder(canvas, Rect(4, 4, 10, 10), st);

    ASSERT(canvas.getPixel(4, 4) == 15, "r=0: square corner inked");
    ASSERT(canvas.getPixel(13, 13) == 15, "r=0: far square corner inked");
    ASSERT(canvas.getPixel(8, 8) == 0, "r=0: interior hollow");
}

int main() {
    test_solid_ring();
    test_thickness();
    test_bevel_two_tone();
    test_drop_shadow();
    test_zero_radius_square();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
