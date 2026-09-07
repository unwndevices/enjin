// Bilinear quad keystone warp test (Tomodachi #44): warpCanvas re-blits a cached
// panel Canvas4 into an arbitrary destination quad (the banner->panel IMU shear).
// The warp is nearest-neighbour with no per-pixel divide, so these assertions
// pin the identity blit (quad == the source rect), a shrunk quad (the opening
// panel collapses many texels onto fewer pixels), a keystone (opposite edges
// differ in length) and transparent skipping.
#include <enjin2/core/types.hpp>
#include <enjin2/graphics/blit.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// Source: 8x8, value = column+1 (1..8) so a horizontal position maps to a known
// index — lets us assert the warp preserves left-to-right order.
static void paintSource(Canvas4<8, 8>& src) {
    src.clear(Pixel4(0));
    for (int16_t sy = 0; sy < 8; ++sy)
        for (int16_t sx = 0; sx < 8; ++sx)
            src.setPixel(sx, sy, Pixel4(static_cast<uint8_t>(sx + 1)));
}

// quad == the exact source rect at (x,y): warp is an identity copy.
static void test_identity_quad_copies_1to1() {
    Canvas4<48, 48> dst;
    dst.clear(Pixel4(0));
    Canvas4<8, 8> src;
    paintSource(src);

    // TL, TR, BR, BL of the 8x8 rect placed at (10,10).
    const Point quad[4] = {Point(10, 10), Point(18, 10), Point(18, 18), Point(10, 18)};
    warpCanvas(dst, src, quad, Pixel4(0));

    ASSERT(dst.getPixel(10, 10) == 1, "warp identity: TL texel lands at the origin");
    ASSERT(dst.getPixel(17, 10) == 8, "warp identity: last column lands at the far edge");
    ASSERT(dst.getPixel(13, 13) == dst.getPixel(13, 15),
           "warp identity: a column holds one value down its height");
}

// A destination quad wider than the source stretches without leaving a hole
// (the overlapping footprints cover the gaps) and keeps left-to-right order.
static void test_stretched_quad_has_no_holes() {
    Canvas4<64, 64> dst;
    dst.clear(Pixel4(0));
    Canvas4<8, 8> src;
    paintSource(src);

    const Point quad[4] = {Point(4, 4), Point(36, 4), Point(36, 20), Point(4, 20)};
    warpCanvas(dst, src, quad, Pixel4(0));

    // No pixel inside the destination span stayed background (0) -> no seams.
    bool anyHole = false;
    for (int16_t y = 5; y < 19; ++y)
        for (int16_t x = 5; x < 35; ++x)
            if (dst.getPixel(x, y) == 0) anyHole = true;
    ASSERT(!anyHole, "warp stretch: interior fully covered (no hairline seams)");
    ASSERT(dst.getPixel(5, 12) < dst.getPixel(34, 12),
           "warp stretch: left-to-right column order preserved");
}

// The opening panel is a quad SMALLER than the cached source: many texels
// collapse onto one pixel (nearest-neighbour downscale), the last write wins.
static void test_shrunk_quad_downscales() {
    Canvas4<32, 32> dst;
    dst.clear(Pixel4(0));
    Canvas4<8, 8> src;
    paintSource(src);

    const Point quad[4] = {Point(4, 4), Point(8, 4), Point(8, 8), Point(4, 8)};  // 4x4
    warpCanvas(dst, src, quad, Pixel4(0));

    ASSERT(dst.getPixel(4, 5) != 0, "warp shrink: downscaled panel drew");
    ASSERT(dst.getPixel(4, 5) < dst.getPixel(7, 5),
           "warp shrink: order survives the downscale");
    ASSERT(dst.getPixel(9, 5) == 0, "warp shrink: nothing drawn past the small quad");
}

// A keystone: the top edge is shorter than the bottom (pitch taper). The quad is
// still filled and the shape is a trapezoid (top row narrower than bottom row).
static void test_keystone_trapezoid() {
    Canvas4<64, 64> dst;
    dst.clear(Pixel4(0));
    Canvas4<8, 8> src;
    paintSource(src);

    // Top edge x in [18,30] (width 12), bottom edge x in [10,38] (width 28).
    const Point quad[4] = {Point(18, 8), Point(30, 8), Point(38, 36), Point(10, 36)};
    warpCanvas(dst, src, quad, Pixel4(0));

    auto rowSpan = [&](int16_t y) {
        int16_t lo = -1, hi = -1;
        for (int16_t x = 0; x < 64; ++x)
            if (dst.getPixel(x, y) != 0) { if (lo < 0) lo = x; hi = x; }
        return hi - lo;
    };
    ASSERT(rowSpan(10) < rowSpan(34),
           "warp keystone: top row narrower than bottom row (trapezoid)");
}

// Transparent source pixels are skipped, leaving the destination untouched.
static void test_transparent_skipped() {
    Canvas4<32, 32> dst;
    dst.clear(Pixel4(9));
    Canvas4<8, 8> src;
    src.clear(Pixel4(PALETTE_TRANSPARENT));
    src.setPixel(0, 0, Pixel4(3));  // one opaque texel at TL

    const Point quad[4] = {Point(4, 4), Point(12, 4), Point(12, 12), Point(4, 12)};
    warpCanvas(dst, src, quad);  // default transparent = PALETTE_TRANSPARENT

    ASSERT(dst.getPixel(4, 4) == 3, "warp transparent: opaque texel drawn");
    ASSERT(dst.getPixel(10, 10) == 9, "warp transparent: transparent texels keep dst");
}

int main() {
    test_identity_quad_copies_1to1();
    test_stretched_quad_has_no_holes();
    test_shrunk_quad_downscales();
    test_keystone_trapezoid();
    test_transparent_skipped();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
