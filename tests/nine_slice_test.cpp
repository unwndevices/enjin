// Nine-slice blit test (Tomodachi #39): a Playdate-style panel draw whose edges
// TILE (never stretch, R4) and whose corners copy 1:1. Because it copies palette
// indices verbatim, the assertions double as the "art re-authorable on palette
// swap" guarantee — the indices hold their roles, so a swapped palette re-skins
// the same bytes.
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

// Source: 6x6, value = column+1 (1..6), so adjacent columns differ and the
// middle strip (columns 2,3) tiles visibly across a wider destination.
static void paintSource(Canvas4<8, 8>& src) {
    src.clear(Pixel4(0));
    for (int16_t sy = 0; sy < 6; ++sy)
        for (int16_t sx = 0; sx < 6; ++sx)
            src.setPixel(sx, sy, Pixel4(static_cast<uint8_t>(sx + 1)));
}

// Corners copy 1:1; the horizontal middle tiles with the source mid-strip's
// period (2), and neighbouring tiled columns keep their distinct values — proof
// the edge tiles rather than stretches.
static void test_edges_tile_never_stretch() {
    Canvas4<48, 48> dst;
    dst.clear(Pixel4(0));
    Canvas4<8, 8> src;
    paintSource(src);

    // srcW=8 here (canvas is 8 wide) but the painted art is 6 wide; pass the
    // painted extent as the corner geometry against a src view sized to it.
    // Use cornerW/H = 2 against the full 8-wide canvas so srcMid = 4.
    nineSlice(dst, 10, 10, 12, 8, src, 2, 2);

    // Leading corner columns copied 1:1.
    ASSERT(dst.getPixel(10, 10) == 1, "nineSlice: top-left corner copied 1:1");
    ASSERT(dst.getPixel(11, 10) == 2, "nineSlice: corner column 2 copied 1:1");

    // Middle tiles: srcMid = srcW(8) - 2*2 = 4 -> columns 2,3,4,5 repeat.
    const uint8_t mid0 = dst.getPixel(12, 10);
    const uint8_t mid1 = dst.getPixel(13, 10);
    ASSERT(mid0 != mid1, "nineSlice: neighbouring middle columns differ (not stretched)");
    ASSERT(dst.getPixel(12, 10) == dst.getPixel(16, 10),
           "nineSlice: middle repeats with the source period (tiled)");
}

// Source pixels equal to the transparent index are skipped, leaving the
// destination background untouched.
static void test_transparent_skipped() {
    Canvas4<48, 48> dst;
    dst.clear(Pixel4(9));
    Canvas4<8, 8> src;
    src.clear(Pixel4(PALETTE_TRANSPARENT));  // fully transparent source
    src.setPixel(0, 0, Pixel4(4));           // one opaque corner pixel

    nineSlice(dst, 5, 5, 10, 10, src, 2, 2);

    ASSERT(dst.getPixel(5, 5) == 4, "nineSlice: opaque corner pixel drawn");
    ASSERT(dst.getPixel(6, 6) == 9, "nineSlice: transparent source keeps dst background");
}

int main() {
    test_edges_tile_never_stretch();
    test_transparent_skipped();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
