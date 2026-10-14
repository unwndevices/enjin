// Shape paint (Tomodachi #332, ADR-0016): word operations on 1-bpp row-word
// masks. Inner stroke = w rounds of 4-neighbour erosion of the fill, open
// rings = pixel-perfect lines of the stroke width, clip = a word AND, and the
// fill painted through the screen-anchored Bayer dither span.
//
// Expected masks are hand-worked pictures ('#' = set).

#include <enjin2/shape/shape_raster.hpp>

#include <cstdio>
#include <string>

using namespace enjin2;
using namespace enjin2::shape;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

static FixedVec2 px(int32_t x, int32_t y) { return {Fixed::fromInt(x), Fixed::fromInt(y)}; }

static std::string picture(const RowMask& m, int x0, int y0, int w, int h)
{
    std::string s;
    for (int y = y0; y < y0 + h; ++y)
    {
        for (int x = x0; x < x0 + w; ++x)
            s += m.test(x, y) ? '#' : '.';
        if (y + 1 < y0 + h)
            s += '\n';
    }
    return s;
}

static bool expectPicture(const RowMask& m, int x0, int y0, int w, int h, const char* want, const char* what)
{
    const std::string got = picture(m, x0, y0, w, h);
    if (got == want)
        return true;
    fprintf(stderr, "  %s\n  want:\n%s\n  got:\n%s\n", what, want, got.c_str());
    return false;
}

static Rasterizer raster;
static uint32_t fillWords[2048], strokeWords[2048], clipWords[2048];

static Shape shapeOf(const Ring* rings, uint16_t n, FillRule rule = FillRule::NonZero)
{
    Shape s;
    s.rings = rings;
    s.ringCount = n;
    s.paint.rule = rule;
    return s;
}

static RowMask fillInto(uint32_t* words, const Ring* rings, uint16_t n, Rect clip = Rect(0, 0, 32, 32))
{
    RowMask m(words, 2048);
    ASSERT(raster.fill(shapeOf(rings, n), clip, m), "fill succeeds");
    return m;
}

// An inner stroke of width w keeps the fill's pixels within w 4-neighbour
// steps of its outside (the prototype's erodeEdge, w times).
static void testInnerStroke()
{
    const FixedVec2 rect[] = {px(1, 1), px(9, 1), px(9, 8), px(1, 8)};  // 8 × 7
    const Ring ring{rect, 4, true};
    const RowMask f = fillInto(fillWords, &ring, 1);

    RowMask s(strokeWords, 2048);
    ASSERT(innerStroke(f, 1, s), "width 1");
    ASSERT(expectPicture(s, 0, 0, 10, 9,
                         "..........\n"
                         ".########.\n"
                         ".#......#.\n"
                         ".#......#.\n"
                         ".#......#.\n"
                         ".#......#.\n"
                         ".#......#.\n"
                         ".########.\n"
                         "..........",
                         "width 1"),
           "width 1: the outline ring");

    ASSERT(innerStroke(f, 2, s), "width 2");
    ASSERT(expectPicture(s, 0, 0, 10, 9,
                         "..........\n"
                         ".########.\n"
                         ".########.\n"
                         ".##....##.\n"
                         ".##....##.\n"
                         ".##....##.\n"
                         ".########.\n"
                         ".########.\n"
                         "..........",
                         "width 2"),
           "width 2: two rings");

    ASSERT(innerStroke(f, 4, s), "width 4");
    ASSERT(expectPicture(s, 0, 0, 10, 9,
                         "..........\n"
                         ".########.\n"
                         ".########.\n"
                         ".########.\n"
                         ".########.\n"
                         ".########.\n"
                         ".########.\n"
                         ".########.\n"
                         "..........",
                         "width 4"),
           "width past the half-height: the whole fill");

    ASSERT(innerStroke(f, 0, s) && s.empty(), "width 0: no stroke");

    // A diamond: erosion is 4-neighbour, so a pixel is stroke iff a 4-neighbour is outside.
    const FixedVec2 dia[] = {px(5, 0), px(10, 5), px(5, 10), px(0, 5)};
    const Ring dr{dia, 4, true};
    const RowMask df = fillInto(fillWords, &dr, 1);
    ASSERT(innerStroke(df, 1, s), "diamond");
    bool ok = true;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
        {
            const bool in = df.test(x, y);
            const bool edge = in && (!df.test(x - 1, y) || !df.test(x + 1, y) || !df.test(x, y - 1) || !df.test(x, y + 1));
            if (s.test(x, y) != edge)
                ok = false;
        }
    ASSERT(ok, "diamond: a fill pixel is stroke iff a 4-neighbour is outside");

    // Across mask words: a 70 px bar; the end columns' neighbours carry over words.
    const FixedVec2 bar[] = {px(0, 0), px(70, 0), px(70, 3), px(0, 3)};
    const Ring br{bar, 4, true};
    RowMask bf(fillWords, 2048);
    ASSERT(raster.fill(shapeOf(&br, 1), Rect(0, 0, 100, 10), bf), "bar fill");
    ASSERT(innerStroke(bf, 1, s), "bar");
    bool barOk = true;
    for (int x = 0; x < 70; ++x)
        barOk = barOk && s.test(x, 0) && s.test(x, 2) && (s.test(x, 1) == (x == 0 || x == 69));
    ASSERT(barOk && !s.test(70, 1), "bar: the middle row is stroke only at its two ends");
}

// A clip is a word AND in canvas space: the masks' bounds differ and the
// shift crosses words; a union is the word OR into the target's bounds.
static void testClipAndUnion()
{
    const FixedVec2 a[] = {px(3, 1), px(75, 1), px(75, 5), px(3, 5)};    // x 3..74, y 1..4
    const FixedVec2 b[] = {px(40, 3), px(90, 3), px(90, 9), px(40, 9)};  // x 40..89, y 3..8
    const Ring ra{a, 4, true}, rb{b, 4, true};
    RowMask fa(fillWords, 2048), fb(clipWords, 2048);
    ASSERT(raster.fill(shapeOf(&ra, 1), Rect(0, 0, 100, 12), fa), "fill a");
    ASSERT(raster.fill(shapeOf(&rb, 1), Rect(0, 0, 100, 12), fb), "fill b");
    clipMask(fa, fb);
    bool ok = true;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 100; ++x)
            ok = ok && fa.test(x, y) == (x >= 40 && x < 75 && y >= 3 && y < 5);
    ASSERT(ok, "a AND b: only the overlap, x 40..74, y 3..4");
    ASSERT(fa.x() == 3 && fa.width() == 72, "the clipped mask keeps its bounds");

    RowMask none(strokeWords, 2048);
    none.reset(0, 0, 0, 0);
    ASSERT(raster.fill(shapeOf(&ra, 1), Rect(0, 0, 100, 12), fa), "fill a again");
    clipMask(fa, none);
    bool cleared = true;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 100; ++x)
            cleared = cleared && !fa.test(x, y);
    ASSERT(cleared, "clipped by an empty mask: nothing left");

    RowMask canvasMask(strokeWords, 2048);
    canvasMask.reset(0, 0, 100, 12);
    for (uint16_t r = 0; r < 12; ++r)
        for (uint16_t wi = 0; wi < canvasMask.wordsPerRow(); ++wi)
            canvasMask.row(r)[wi] = 0;
    ASSERT(raster.fill(shapeOf(&ra, 1), Rect(0, 0, 100, 12), fa), "fill a");
    uniteMask(canvasMask, fa);
    uniteMask(canvasMask, fb);
    bool united = true;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 100; ++x)
            united = united && canvasMask.test(x, y) ==
                                   ((x >= 3 && x < 75 && y >= 1 && y < 5) || (x >= 40 && x < 90 && y >= 3 && y < 9));
    ASSERT(united, "a OR b into a canvas-sized mask");
    ASSERT((canvasMask.row(0)[3] >> 4) == 0, "bits past the width stay zero");
}

// An open ring is a pixel-perfect line: each point is the pixel it lies in,
// Bresenham between them, the corner of every "L" dropped; a stroke w wide
// stamps a w × w square, its top-left floor((w - 1) / 2) up and left.
static void testOpenStroke()
{
    // Points mid-pixel, so the pixels are (1,1) → (6,1) → (6,4).
    const FixedVec2 pts[] = {{Fixed::fromRatio(3, 2), Fixed::fromRatio(3, 2)},
                             {Fixed::fromRatio(13, 2), Fixed::fromRatio(3, 2)},
                             {Fixed::fromRatio(13, 2), Fixed::fromRatio(9, 2)}};
    const Ring open{pts, 3, false};
    Shape s = shapeOf(&open, 1);
    s.paint.strokeWidth = 1;
    RowMask m(strokeWords, 2048);
    ASSERT(raster.stroke(s, Rect(0, 0, 32, 32), m), "width 1");
    ASSERT(expectPicture(m, 0, 0, 9, 6,
                         ".........\n"
                         ".#####...\n"
                         "......#..\n"
                         "......#..\n"
                         "......#..\n"
                         ".........",
                         "L, width 1"),
           "an L: the corner pixel is dropped");

    s.paint.strokeWidth = 2;
    ASSERT(raster.stroke(s, Rect(0, 0, 32, 32), m), "width 2");
    ASSERT(expectPicture(m, 0, 0, 9, 7,
                         ".........\n"
                         ".######..\n"
                         ".#######.\n"
                         "......##.\n"
                         "......##.\n"
                         "......##.\n"
                         ".........",
                         "L, width 2"),
           "width 2: a 2 × 2 square per pixel, down and right");

    s.paint.strokeWidth = 3;
    ASSERT(raster.stroke(s, Rect(0, 0, 32, 32), m), "width 3");
    ASSERT(expectPicture(m, 0, 0, 9, 7,
                         "#######..\n"
                         "########.\n"
                         "########.\n"
                         ".....###.\n"
                         ".....###.\n"
                         ".....###.\n"
                         ".........",
                         "L, width 3"),
           "width 3: centred on the line");

    // Closed rings are not walked (they take the inner stroke); width 0 strokes nothing.
    const FixedVec2 sq[] = {px(1, 1), px(5, 1), px(5, 5), px(1, 5)};
    const Ring closed{sq, 4, true};
    Shape c = shapeOf(&closed, 1);
    c.paint.strokeWidth = 1;
    ASSERT(raster.stroke(c, Rect(0, 0, 32, 32), m) && m.empty(), "a closed ring: no line");
    s.paint.strokeWidth = 0;
    ASSERT(raster.stroke(s, Rect(0, 0, 32, 32), m) && m.empty(), "width 0: no line");

    // Clipped to the canvas rect.
    s.paint.strokeWidth = 1;
    ASSERT(raster.stroke(s, Rect(3, 0, 32, 3), m), "clipped");
    ASSERT(m.x() >= 3 && !m.test(2, 1) && m.test(3, 1) && m.test(6, 2) && !m.test(6, 3), "the line is cut to the clip");
}

// A dithered fill paints a pixel where the 4×4 Bayer matrix at its canvas
// coordinates is below the level, and leaves the rest as it was: moving the
// Shape does not move the pattern (screen-anchored).
static void testDither()
{
    static const int kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    static Canvas4<64, 32> canvas;
    for (int shift = 0; shift < 2; ++shift)
        for (uint8_t level : {uint8_t(4), uint8_t(8), uint8_t(12), uint8_t(16)})
        {
            canvas.clear(Pixel4(9));
            const FixedVec2 r[] = {px(3 + shift, 2), px(40 + shift, 2), px(40 + shift, 9), px(3 + shift, 9)};
            const Ring ring{r, 4, true};
            const RowMask m = fillInto(fillWords, &ring, 1, Rect(0, 0, 64, 32));
            paintFill(canvas, m, ColourRole::Accent, Remap::solid(5), level);
            bool ok = true;
            int painted = 0;
            for (int y = 0; y < 32; ++y)
                for (int x = 0; x < 64; ++x)
                {
                    const bool in = x >= 3 + shift && x < 40 + shift && y >= 2 && y < 9;
                    const bool on = in && kBayer[y & 3][x & 3] < level;
                    painted += on;
                    ok = ok && canvas.getPixel(int16_t(x), int16_t(y)).value == (on ? 5 : 9);
                }
            ASSERT(ok, "dither: Bayer below the level at canvas coordinates, the rest untouched");
            ASSERT(level < 16 ? painted < 37 * 7 : painted == 37 * 7, "dither: only solid paints every pixel");
        }
}

int main()
{
    testDither();
    testInnerStroke();
    testOpenStroke();
    testClipAndUnion();
    printf("shape_paint_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
