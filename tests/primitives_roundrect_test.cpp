// Rounded-rect primitives test (Phase 3a, #121): drawRoundRect / fillRoundRect
// added to Primitives<TPixel> so the Pixel4 widget layer (Label/PopUp/Gauge) can
// draw rounded panels instead of settling for a square bar (list.hpp Gate-2 note).
//
// The seams under test are pure geometry on a real Canvas4: a filled rounded rect
// must ink its interior but clip its extreme corners, an outline must stroke its
// edges while leaving the center hollow, and a zero radius must degrade to the
// square fill/draw exactly.
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// A filled rounded rect inks its interior but rounds off the corners: the exact
// corner pixel is clipped away while the center stays solid.
static void test_fill_rounds_corners() {
    Canvas4<32, 32> canvas;
    canvas.clear(Colors::BLACK);
    Primitives<Pixel4>::fillRoundRect(canvas, Rect(2, 2, 20, 20), 6, Colors::WHITE);

    ASSERT(canvas.getPixel(12, 12) == 15, "fillRoundRect: center is filled");
    ASSERT(canvas.getPixel(2, 2) == 0, "fillRoundRect: extreme corner is clipped");
    ASSERT(canvas.getPixel(12, 2) == 15, "fillRoundRect: top edge midpoint is filled");
    ASSERT(canvas.getPixel(2, 12) == 15, "fillRoundRect: left edge midpoint is filled");
    ASSERT(canvas.getPixel(21, 21) == 0, "fillRoundRect: opposite corner is clipped");
}

// An outline strokes its straight edges (edge midpoints are ink) but leaves the
// interior hollow and rounds the corners off.
static void test_draw_outlines_and_hollow() {
    Canvas4<32, 32> canvas;
    canvas.clear(Colors::BLACK);
    Primitives<Pixel4>::drawRoundRect(canvas, Rect(2, 2, 20, 20), 6, Colors::WHITE);

    ASSERT(canvas.getPixel(12, 2) == 15, "drawRoundRect: top edge is stroked");
    ASSERT(canvas.getPixel(12, 21) == 15, "drawRoundRect: bottom edge is stroked");
    ASSERT(canvas.getPixel(2, 12) == 15, "drawRoundRect: left edge is stroked");
    ASSERT(canvas.getPixel(21, 12) == 15, "drawRoundRect: right edge is stroked");
    ASSERT(canvas.getPixel(12, 12) == 0, "drawRoundRect: interior stays hollow");
    ASSERT(canvas.getPixel(2, 2) == 0, "drawRoundRect: corner is rounded away");
}

// A zero radius is not a special-case bug: it must paint exactly the square rect,
// corners included, matching fillRect/drawRect.
static void test_zero_radius_degrades_to_square() {
    Canvas4<32, 32> fill;
    fill.clear(Colors::BLACK);
    Primitives<Pixel4>::fillRoundRect(fill, Rect(4, 4, 10, 10), 0, Colors::WHITE);
    ASSERT(fill.getPixel(4, 4) == 15, "fillRoundRect(r=0): square corner is filled");
    ASSERT(fill.getPixel(13, 13) == 15, "fillRoundRect(r=0): far square corner is filled");

    Canvas4<32, 32> draw;
    draw.clear(Colors::BLACK);
    Primitives<Pixel4>::drawRoundRect(draw, Rect(4, 4, 10, 10), 0, Colors::WHITE);
    ASSERT(draw.getPixel(4, 4) == 15, "drawRoundRect(r=0): square corner is stroked");
    ASSERT(draw.getPixel(8, 8) == 0, "drawRoundRect(r=0): interior stays hollow");
}

int main() {
    test_fill_rounds_corners();
    test_draw_outlines_and_hollow();
    test_zero_radius_degrades_to_square();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
