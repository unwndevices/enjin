// blitCanvasOpacity contract test (M5 adjudication, unwn #168).
//
// The visual parity bench RETIRED the blit.canvasOpacity pair: BASE keyed
// transparency on an out-of-band 8-bit matte (Eisei widgets composited with
// matte=16, one past the 4-bit range), and a Pixel4 source has no
// out-of-band value to carry that sentinel — the divergence is depth-
// inherent, not fixable. Retirement's exit question is "what now guards the
// ratified HEAD behavior?"; this test is the answer. It pins the ratified
// contract of blitCanvasOpacity:
//
//   * source pixels equal to `transparent` (default Pixel4(0)) are skipped,
//     leaving the destination untouched;
//   * every other pixel lands as source / divisor (absolute source fade,
//     not a destination lerp);
//   * divisor < 1 is clamped to 1;
//   * placement clips at the destination edges via setPixel.

#include <enjin2/graphics/blit.hpp>
#include <enjin2/graphics/canvas.hpp>

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

using Dst = Canvas4<32, 32>;
using Src = Canvas4<8, 8>;

static void test_transparent_skip()
{
    printf("--- transparent skip ---\n");
    Dst dst;
    dst.clear(Pixel4(5));
    Src src;
    src.setPixel(0, 0, Pixel4(0));  // transparent
    src.setPixel(1, 0, Pixel4(12)); // opaque

    blitCanvasOpacity(dst, src, 4, 4, 1);
    ASSERT(dst.getPixel(4, 4).value == 5,
           "source Pixel4(0) is skipped, destination shows through");
    ASSERT(dst.getPixel(5, 4).value == 12,
           "opaque source pixel lands at divisor 1");

    Dst dst2;
    dst2.clear(Pixel4(5));
    blitCanvasOpacity(dst2, src, 4, 4, 1, Pixel4(12));
    ASSERT(dst2.getPixel(5, 4).value == 5,
           "a custom transparent value is honored");
    ASSERT(dst2.getPixel(4, 4).value == 0,
           "Pixel4(0) is opaque when transparent is remapped");
}

static void test_divisor_fade()
{
    printf("--- divisor fade ---\n");
    Dst dst;
    Src src;
    src.setPixel(0, 0, Pixel4(12));
    src.setPixel(1, 0, Pixel4(15));

    blitCanvasOpacity(dst, src, 0, 0, 2);
    ASSERT(dst.getPixel(0, 0).value == 6, "12 / 2 = 6");
    ASSERT(dst.getPixel(1, 0).value == 7, "15 / 2 = 7 (integer fade)");

    Dst dst4;
    blitCanvasOpacity(dst4, src, 0, 0, 4);
    ASSERT(dst4.getPixel(0, 0).value == 3, "12 / 4 = 3");

    Dst dst0;
    blitCanvasOpacity(dst0, src, 0, 0, 0);
    ASSERT(dst0.getPixel(0, 0).value == 12, "divisor 0 clamps to 1");
}

static void test_clipping()
{
    printf("--- clipping ---\n");
    Dst dst;
    dst.clear(Pixel4(3));
    Src src;
    for (int16_t y = 0; y < 8; ++y)
        for (int16_t x = 0; x < 8; ++x)
            src.setPixel(x, y, Pixel4(9));

    blitCanvasOpacity(dst, src, -4, 28, 1);
    ASSERT(dst.getPixel(0, 28).value == 9,
           "on-canvas part of a clipped placement lands");
    ASSERT(dst.getPixel(4, 28).value == 3,
           "columns past the source stay untouched");
    ASSERT(dst.getPixel(0, 27).value == 3,
           "rows above the placement stay untouched");
    // Rows below the canvas edge simply clip; nothing to observe but the
    // absence of a crash and the untouched interior.
    ASSERT(dst.getPixel(15, 15).value == 3, "interior stays untouched");
}

// fillRectWithPattern samples at ABSOLUTE canvas coords (Tomodachi #36 ratified
// HEAD; the rect-relative BASE `blit.pattern` bench pair is retired). This pins
// that ratified behaviour: the sample index depends on where the rect sits, not
// on its local origin, so abutting rects tile seamlessly.
static void test_pattern_absolute_phase()
{
    printf("--- fillRectWithPattern absolute phase ---\n");
    // 2x2 checker: pattern[(y%2)*2 + (x%2)] over {A,B,B,A}.
    static const uint8_t checker[4] = {5, 6, 6, 5};
    Dst dst;
    dst.clear(Pixel4(0));

    // A 1x1 rect at an even/even absolute coord samples cell (0,0) = 5.
    fillRectWithPattern(dst, 4, 4, 1, 1, checker, 2, 2);
    ASSERT(dst.getPixel(4, 4).value == 5, "even/even absolute -> cell(0,0)=5");

    // A 1x1 rect at an odd x samples cell (1,0) = 6 — absolute, not rect-local
    // (a rect-relative fill would sample cell(0,0) here since px==0).
    fillRectWithPattern(dst, 5, 4, 1, 1, checker, 2, 2);
    ASSERT(dst.getPixel(5, 4).value == 6, "odd-x absolute -> cell(1,0)=6");

    // Seam check: a 2-wide rect at x=4 and two 1-wide rects at x=4 and x=5 must
    // produce the same two pixels (absolute phase makes the split invisible).
    Dst whole;
    whole.clear(Pixel4(0));
    fillRectWithPattern(whole, 4, 7, 2, 1, checker, 2, 2);
    Dst split;
    split.clear(Pixel4(0));
    fillRectWithPattern(split, 4, 7, 1, 1, checker, 2, 2);
    fillRectWithPattern(split, 5, 7, 1, 1, checker, 2, 2);
    ASSERT(whole.getPixel(4, 7).value == split.getPixel(4, 7).value &&
               whole.getPixel(5, 7).value == split.getPixel(5, 7).value,
           "split fill matches whole fill (seamless)");
}

int main()
{
    printf("=== blit_semantics_test (unwn #168) ===\n");
    test_transparent_skip();
    test_divisor_fade();
    test_clipping();
    test_pattern_absolute_phase();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
