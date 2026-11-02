// Span-walker border stroke test (#18 / Tomodachi #39): the computed selection
// frame primitive. Solid draws a hollow ring with rounded corners; bevel splits
// into top/left one step toward text + bottom/right one step toward bg (the
// shade chain); drop-shadow paints an offset filled rounded rect behind the ring. All three walk rows via the
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
    canvas.clear(Pixel4(ROLE_BG));
    BorderStyle st;
    st.color = Pixel4(ROLE_TEXT);
    st.thickness = 1;
    st.radius = 6;
    st.kind = BorderKind::Solid;
    strokeBorder(canvas, Rect(2, 2, 20, 20), st);

    ASSERT(canvas.getPixel(12, 2) == ROLE_TEXT, "solid: top edge midpoint inked");
    ASSERT(canvas.getPixel(12, 21) == ROLE_TEXT, "solid: bottom edge midpoint inked");
    ASSERT(canvas.getPixel(2, 12) == ROLE_TEXT, "solid: left edge midpoint inked");
    ASSERT(canvas.getPixel(21, 12) == ROLE_TEXT, "solid: right edge midpoint inked");
    ASSERT(canvas.getPixel(12, 12) == 0, "solid: interior hollow");
    ASSERT(canvas.getPixel(2, 2) == 0, "solid: extreme corner rounded away");
}

// Canvas8 border colours are byte-wide palette indices, not Canvas4 roles.
static void test_canvas8_solid_preserves_full_width_color() {
    Canvas8<32, 32> canvas;
    canvas.clear(0);
    BorderStyle st;
    st.color = 200;
    st.radius = 0;
    st.kind = BorderKind::Solid;
    strokeBorder(canvas, Rect(4, 4, 12, 12), st);

    ASSERT(canvas.getPixel(10, 4) == 200,
           "Canvas8 solid: full-width border colour is preserved");
}

// A thicker stroke inks `thickness` rows/columns of ring (t=3 -> 3 px deep).
static void test_thickness() {
    Canvas4<32, 32> canvas;
    canvas.clear(Pixel4(ROLE_BG));
    BorderStyle st;
    st.color = Pixel4(ROLE_TEXT);
    st.thickness = 3;
    st.radius = 0;
    strokeBorder(canvas, Rect(4, 4, 16, 16), st);

    // Left edge: 3 px inked (x = 4,5,6), then hollow at x = 7.
    ASSERT(canvas.getPixel(4, 12) == ROLE_TEXT, "thick: outer column inked");
    ASSERT(canvas.getPixel(6, 12) == ROLE_TEXT, "thick: inner-most stroke column inked");
    ASSERT(canvas.getPixel(7, 12) == 0, "thick: interior past thickness is hollow");
    // Top band 3 rows deep.
    ASSERT(canvas.getPixel(12, 4) == ROLE_TEXT, "thick: top band inked");
    ASSERT(canvas.getPixel(12, 6) == ROLE_TEXT, "thick: top band inked to depth");
    ASSERT(canvas.getPixel(12, 7) == 0, "thick: below top band is hollow");
}

// A bevel is a shade-chain two-tone: top/left one step toward text,
// bottom/right one step toward bg, both derived from the base colour (never
// stored).
static void test_bevel_two_tone() {
    Canvas4<32, 32> canvas;
    canvas.clear(Pixel4(ROLE_BG));
    BorderStyle st;
    st.color = Pixel4(ROLE_MUTED);
    st.thickness = 2;
    st.radius = 0;
    st.kind = BorderKind::Bevel;
    strokeBorder(canvas, Rect(4, 4, 16, 16), st);

    ASSERT(canvas.getPixel(12, 4) == ROLE_TEXT, "bevel: top edge of muted is text");
    ASSERT(canvas.getPixel(4, 12) == ROLE_TEXT, "bevel: left edge of muted is text");
    ASSERT(canvas.getPixel(12, 19) == ROLE_SURFACE, "bevel: bottom edge of muted is surface");
    ASSERT(canvas.getPixel(19, 12) == ROLE_SURFACE, "bevel: right edge of muted is surface");
}

// (A Canvas4 style colour above 15 is no Palette index: Pixel4 asserts on it in
// debug builds (ADR-0022), so Canvas4 no longer narrows byte-wide colours.)

static void test_canvas8_bevel_does_not_apply_pixel4_chain() {
    Canvas8<32, 32> canvas;
    canvas.clear(0);
    BorderStyle st;
    st.color = 200;
    st.thickness = 2;
    st.radius = 0;
    st.kind = BorderKind::Bevel;
    strokeBorder(canvas, Rect(4, 4, 16, 16), st);

    ASSERT(canvas.getPixel(12, 4) == 200,
           "Canvas8 bevel: top edge keeps the byte-wide colour");
    ASSERT(canvas.getPixel(12, 19) == 200,
           "Canvas8 bevel: bottom edge keeps the byte-wide colour");
}

// A drop shadow paints an offset filled rounded rect behind the ring one step
// toward bg; the ring itself is drawn on top at the origin.
static void test_drop_shadow() {
    Canvas4<32, 32> canvas;
    canvas.clear(Pixel4(ROLE_BG));
    BorderStyle st;
    st.color = Pixel4(ROLE_TEXT);
    st.thickness = 1;
    st.radius = 0;
    st.kind = BorderKind::DropShadow;
    st.shadowDx = 3;
    st.shadowDy = 3;
    strokeBorder(canvas, Rect(4, 4, 12, 12), st);

    // Shadow fills the offset interior — a point past the ring's own bottom-right
    // edge but inside the shifted shadow rect.
    ASSERT(canvas.getPixel(17, 17) == ROLE_MUTED, "shadow of text: offset fill is muted");
    // The ring edge still inks at the origin position.
    ASSERT(canvas.getPixel(4, 10) == ROLE_TEXT, "shadow: ring drawn on top at origin");
}

// A default BorderStyle strokes in the muted role (borders and dividers).
static void test_default_colour_is_muted() {
    Canvas4<32, 32> canvas;
    canvas.clear(Pixel4(ROLE_BG));
    BorderStyle st;
    strokeBorder(canvas, Rect(4, 4, 10, 10), st);
    ASSERT(canvas.getPixel(4, 8) == ROLE_MUTED, "default border colour is muted");
}

static void test_canvas8_shadow_does_not_apply_pixel4_chain() {
    Canvas8<32, 32> canvas;
    canvas.clear(0);
    BorderStyle st;
    st.color = 199;
    st.radius = 0;
    st.kind = BorderKind::DropShadow;
    st.shadowDx = 3;
    st.shadowDy = 3;
    strokeBorder(canvas, Rect(4, 4, 12, 12), st);

    ASSERT(canvas.getPixel(17, 17) == 199,
           "Canvas8 shadow: offset fill keeps the byte-wide colour");
}

// A zero-radius stroke is exactly the square outline — no rounding, no gap.
static void test_zero_radius_square() {
    Canvas4<32, 32> canvas;
    canvas.clear(Pixel4(ROLE_BG));
    BorderStyle st;
    st.color = Pixel4(ROLE_TEXT);
    st.thickness = 1;
    st.radius = 0;
    strokeBorder(canvas, Rect(4, 4, 10, 10), st);

    ASSERT(canvas.getPixel(4, 4) == ROLE_TEXT, "r=0: square corner inked");
    ASSERT(canvas.getPixel(13, 13) == ROLE_TEXT, "r=0: far square corner inked");
    ASSERT(canvas.getPixel(8, 8) == 0, "r=0: interior hollow");
}

int main() {
    test_solid_ring();
    test_canvas8_solid_preserves_full_width_color();
    test_thickness();
    test_bevel_two_tone();
    test_canvas8_bevel_does_not_apply_pixel4_chain();
    test_drop_shadow();
    test_canvas8_shadow_does_not_apply_pixel4_chain();
    test_zero_radius_square();
    test_default_colour_is_muted();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
