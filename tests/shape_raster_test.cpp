// Shape fill rasterizer (Tomodachi #329, ADR-0016): rings -> 1-bpp row-word
// mask with majority coverage (4x4 sub-samples, >= 8/16), pixel-centre +
// top-left rule on the sub-samples, integer edge stepping.
//
// Expected masks are hand-worked pictures ('#' = set) or, for the Star, the
// prototype's own 4x4 majority output (shape_star_golden.hpp).

#include <enjin2/shape/shape_raster.hpp>

#include "shape_star_golden.hpp"
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace enjin2;
using namespace enjin2::shape;

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

// A point in quarter pixels: q(9, 2) = (2.25, 0.5).
static FixedVec2 q(int32_t x4, int32_t y4)
{
    return {Fixed::fromRatio(x4, 4), Fixed::fromRatio(y4, 4)};
}
static FixedVec2 px(int32_t x, int32_t y) { return {Fixed::fromInt(x), Fixed::fromInt(y)}; }

// The mask over a canvas window as rows of '#' / '.', '\n'-separated.
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
static uint32_t storage[2048];

static RowMask fill(const Ring* rings, uint16_t n, FillRule rule, Rect clip = Rect(0, 0, 16, 16))
{
    RowMask m(storage, sizeof(storage) / sizeof(storage[0]));
    Shape s;
    s.rings = rings;
    s.ringCount = n;
    s.paint.rule = rule;
    const bool ok = raster.fill(s, clip, m);
    ASSERT(ok, "fill succeeds");
    return m;
}

// A rect whose sides lie on pixel edges covers exactly its pixels (16/16
// each) and nothing outside it.
static void testRectOnPixelEdges()
{
    const FixedVec2 pts[] = {px(2, 3), px(6, 3), px(6, 5), px(2, 5)};
    const Ring ring{pts, 4, true};
    const RowMask m = fill(&ring, 1, FillRule::NonZero);
    ASSERT(expectPicture(m, 0, 0, 8, 7,
                         "........\n"
                         "........\n"
                         "........\n"
                         "..####..\n"
                         "..####..\n"
                         "........\n"
                         "........",
                         "rect on pixel edges"),
           "rect on pixel edges");
    ASSERT(m.x() == 2 && m.y() == 3 && m.width() == 4 && m.height() == 2, "mask is bbox-local");
}

static FixedVec2 raw(int32_t x, int32_t y) { return {Fixed::fromRaw(x), Fixed::fromRaw(y)}; }

// One-row rect [2, right) x [3, 4): how much of pixel 4 it takes decides
// whether pixel 4 is set.
static bool pixel4Set(Fixed right)
{
    const FixedVec2 pts[] = {px(2, 3), {right, Fixed::fromInt(3)}, {right, Fixed::fromInt(4)}, px(2, 4)};
    const Ring ring{pts, 4, true};
    const RowMask m = fill(&ring, 1, FillRule::NonZero);
    return m.test(4, 3);
}

// Majority is >= 8/16, and the sub-samples follow pixel-centre + top-left:
// a sub-sample centre exactly on a right edge is out, on a left edge in.
static void testTies()
{
    // An edge through pixel 4's centre (x = 4.5) leaves it 8/16: set.
    ASSERT(pixel4Set(Fixed::fromRatio(9, 2)), "edge through the pixel centre: 8/16 is set");
    // The other side of that edge also has 8/16, so it claims pixel 4 too.
    {
        const FixedVec2 pts[] = {q(18, 12), px(7, 3), px(7, 4), q(18, 16)};
        const Ring ring{pts, 4, true};
        const RowMask m = fill(&ring, 1, FillRule::NonZero);
        ASSERT(m.test(4, 3) && m.test(6, 3) && !m.test(7, 3), "right half of pixel 4 is 8/16 too");
    }
    // Right edge on the sub-sample centre x = 4.375: 4.125 in, 4.375 out = 4/16.
    ASSERT(!pixel4Set(Fixed::fromRatio(35, 8)), "right edge on a sub-sample centre leaves it out");
    // One raw step further and 4.375 is in: 8/16.
    ASSERT(pixel4Set(Fixed::fromRaw(Fixed::fromRatio(35, 8).raw + 1)), "one ulp further takes the sample");
    // Left edge on the sub-sample centre x = 4.625: 4.625 and 4.875 in = 8/16.
    {
        const Fixed left = Fixed::fromRatio(37, 8);
        const FixedVec2 pts[] = {{left, Fixed::fromInt(3)}, px(7, 3), px(7, 4), {left, Fixed::fromInt(4)}};
        const Ring ring{pts, 4, true};
        ASSERT(fill(&ring, 1, FillRule::NonZero).test(4, 3), "left edge on a sub-sample centre takes it");
        const FixedVec2 late[] = {raw(left.raw + 1, 3 << 16), px(7, 3), px(7, 4), raw(left.raw + 1, 4 << 16)};
        const Ring lateRing{late, 4, true};
        ASSERT(!fill(&lateRing, 1, FillRule::NonZero).test(4, 3), "one ulp later drops it to 4/16");
    }
    // Same rule vertically: a top edge on the sub-row centre y = 3.625 takes
    // it (.625 and .875: 8/16); a bottom edge on 3.375 leaves it (.125 only:
    // 4/16), and one ulp lower takes it (8/16).
    {
        const Fixed y = Fixed::fromRatio(29, 8);
        const FixedVec2 pts[] = {{Fixed::fromInt(2), y}, {Fixed::fromInt(6), y}, px(6, 5), px(2, 5)};
        const Ring ring{pts, 4, true};
        const RowMask m = fill(&ring, 1, FillRule::NonZero);
        ASSERT(m.test(3, 3) && m.test(3, 4), "top edge on a sub-row centre takes it (8/16)");
        const Fixed b = Fixed::fromRatio(27, 8);
        const FixedVec2 up[] = {px(2, 1), px(6, 1), {Fixed::fromInt(6), b}, {Fixed::fromInt(2), b}};
        const Ring upRing{up, 4, true};
        const RowMask u = fill(&upRing, 1, FillRule::NonZero);
        ASSERT(u.test(3, 2) && !u.test(3, 3), "bottom edge on a sub-row centre leaves it (4/16)");
        const FixedVec2 low[] = {px(2, 1), px(6, 1), raw(6 << 16, b.raw + 1), raw(2 << 16, b.raw + 1)};
        const Ring lowRing{low, 4, true};
        ASSERT(fill(&lowRing, 1, FillRule::NonZero).test(3, 3), "one ulp lower takes the sub-row (8/16)");
    }
    // A diagonal through the pixel centres is the right edge of each row's
    // span: the 4 sub-samples on it are out, so a diagonal pixel is 6/16.
    {
        const FixedVec2 pts[] = {px(0, 0), px(8, 8), px(0, 8)};
        const Ring ring{pts, 3, true};
        ASSERT(expectPicture(fill(&ring, 1, FillRule::NonZero), 0, 0, 8, 8,
                             "........\n"
                             "#.......\n"
                             "##......\n"
                             "###.....\n"
                             "####....\n"
                             "#####...\n"
                             "######..\n"
                             "#######.",
                             "lower-left triangle on the diagonal"),
               "diagonal: 6/16 on the diagonal pixels is not set");
    }
}

// Outer square 1..9 and inner square 3..7: a hole under even-odd always, under
// non-zero only when the inner ring winds the other way.
static void testHoles()
{
    const FixedVec2 outer[] = {px(1, 1), px(9, 1), px(9, 9), px(1, 9)};
    const FixedVec2 innerSame[] = {px(3, 3), px(7, 3), px(7, 7), px(3, 7)};
    const FixedVec2 innerOpp[] = {px(3, 3), px(3, 7), px(7, 7), px(7, 3)};
    const char* holed =
        "..........\n"
        ".########.\n"
        ".########.\n"
        ".##....##.\n"
        ".##....##.\n"
        ".##....##.\n"
        ".##....##.\n"
        ".########.\n"
        ".########.\n"
        "..........";
    const char* solid =
        "..........\n"
        ".########.\n"
        ".########.\n"
        ".########.\n"
        ".########.\n"
        ".########.\n"
        ".########.\n"
        ".########.\n"
        ".########.\n"
        "..........";
    const Ring same[] = {{outer, 4, true}, {innerSame, 4, true}};
    const Ring opp[] = {{outer, 4, true}, {innerOpp, 4, true}};
    ASSERT(expectPicture(fill(same, 2, FillRule::EvenOdd), 0, 0, 10, 10, holed, "even-odd, same winding"),
           "even-odd: hole (same winding)");
    ASSERT(expectPicture(fill(opp, 2, FillRule::EvenOdd), 0, 0, 10, 10, holed, "even-odd, opposite winding"),
           "even-odd: hole (opposite winding)");
    ASSERT(expectPicture(fill(same, 2, FillRule::NonZero), 0, 0, 10, 10, solid, "non-zero, same winding"),
           "non-zero: no hole when both rings wind the same way");
    ASSERT(expectPicture(fill(opp, 2, FillRule::NonZero), 0, 0, 10, 10, holed, "non-zero, opposite winding"),
           "non-zero: hole when the inner ring winds the other way");
}

// The mask is the bbox cut to the clip; refused Shapes leave it empty.
static void testClipAndRefusals()
{
    const FixedVec2 pts[] = {px(-3, -2), px(5, -2), px(5, 4), px(-3, 4)};
    const Ring ring{pts, 4, true};
    const RowMask m = fill(&ring, 1, FillRule::NonZero, Rect(0, 0, 16, 16));
    ASSERT(m.x() == 0 && m.y() == 0 && m.width() == 5 && m.height() == 4, "bbox cut to the clip");
    ASSERT(m.test(0, 0) && m.test(4, 3) && !m.test(5, 3) && !m.test(0, 4), "clipped fill");

    ASSERT(fill(&ring, 1, FillRule::NonZero, Rect(20, 20, 8, 8)).empty(), "outside the clip: empty mask");

    const Ring open{pts, 4, false};
    ASSERT(fill(&open, 1, FillRule::NonZero).empty(), "an open ring is not filled");

    Shape s;
    s.rings = &ring;
    s.ringCount = 1;
    uint32_t tiny[3];
    RowMask small(tiny, 3);
    ASSERT(!raster.fill(s, Rect(0, 0, 16, 16), small) && small.empty(), "storage too small: refused");

    const FixedVec2 far[] = {px(0, 0), px(5000, 0), px(0, 5)};
    const Ring farRing{far, 3, true};
    s.rings = &farRing;
    RowMask big(storage, sizeof(storage) / sizeof(storage[0]));
    ASSERT(!raster.fill(s, Rect(0, 0, 16, 16), big) && big.empty(), "coordinate past kMaxCoordPx: refused");
}

// Independent reference: count every sub-sample with an exact point-in-ring
// winding test (a crossing at or left of the sample centre is left of it).
static void referenceFill(const Ring* rings, uint16_t n, FillRule rule, RowMask& m, int x0, int y0, int w, int h)
{
    m.reset(int16_t(x0), int16_t(y0), uint16_t(w), uint16_t(h));
    for (int r = 0; r < h; ++r)
        for (int wi = 0; wi < m.wordsPerRow(); ++wi)
            m.row(uint16_t(r))[wi] = 0;
    for (int py = y0; py < y0 + h; ++py)
        for (int pxl = x0; pxl < x0 + w; ++pxl)
        {
            int count = 0;
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 4; ++i)
                {
                    const int64_t ys = int64_t(py) * 65536 + (2 * j + 1) * 8192;
                    const int64_t xs = int64_t(pxl) * 65536 + (2 * i + 1) * 8192;
                    int wind = 0;
                    for (uint16_t ri = 0; ri < n; ++ri)
                    {
                        const Ring& rg = rings[ri];
                        for (uint16_t k = 0; k < rg.count; ++k)
                        {
                            FixedVec2 a = rg.points[k], b = rg.points[(k + 1) % rg.count];
                            int dir = 1;
                            if (a.y.raw > b.y.raw)
                            {
                                std::swap(a, b);
                                dir = -1;
                            }
                            if (!(a.y.raw <= ys && ys < b.y.raw))
                                continue;
                            // crossing x <= xs  <=>  (ax - xs)·D + (ys - ay)·dX <= 0
                            const int64_t D = int64_t(b.y.raw) - a.y.raw;
                            const int64_t lhs = (int64_t(a.x.raw) - xs) * D +
                                                (ys - a.y.raw) * (int64_t(b.x.raw) - a.x.raw);
                            if (lhs <= 0)
                                wind += dir;
                        }
                    }
                    if (rule == FillRule::EvenOdd ? (wind & 1) != 0 : wind != 0)
                        ++count;
                }
            if (count >= 8)
                m.row(uint16_t(py - y0))[(pxl - x0) >> 5] |= 1u << ((pxl - x0) & 31);
        }
}

// Random rings (on the raw grid, and on the sub-sample grid so exact ties
// happen) fill exactly like the brute-force reference.
static void testMatchesReference()
{
    static uint32_t refWords[2048];
    uint32_t seed = 12345;
    auto rnd = [&seed](uint32_t n) {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8) % n;
    };
    int bad = 0;
    for (int trial = 0; trial < 400; ++trial)
    {
        FixedVec2 pts[2][9];
        Ring rings[2];
        const uint16_t nr = uint16_t(1 + rnd(2));
        for (uint16_t ri = 0; ri < nr; ++ri)
        {
            const uint16_t cnt = uint16_t(3 + rnd(7));
            for (uint16_t k = 0; k < cnt; ++k)
            {
                if (trial % 2 == 0)
                    pts[ri][k] = raw(int32_t(rnd(80 * 8)) * 8192 - 8 * 65536,
                                     int32_t(rnd(50 * 8)) * 8192 - 4 * 65536);
                else
                    pts[ri][k] = raw(int32_t(rnd(80u << 16)) - (8 << 16), int32_t(rnd(50u << 16)) - (4 << 16));
            }
            rings[ri] = Ring{pts[ri], cnt, true};
        }
        const FillRule rule = (trial / 2) % 2 ? FillRule::EvenOdd : FillRule::NonZero;
        const RowMask got = fill(rings, nr, rule, Rect(0, 0, 64, 40));
        RowMask want(refWords, 2048);
        referenceFill(rings, nr, rule, want, 0, 0, 64, 40);
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 64; ++x)
                if (got.test(x, y) != want.test(x, y))
                {
                    if (bad < 5)
                        fprintf(stderr, "  trial %d: pixel (%d, %d) got %d want %d\n", trial, x, y,
                                int(got.test(x, y)), int(want.test(x, y)));
                    ++bad;
                }
    }
    ASSERT(bad == 0, "random rings match the brute-force sub-sample reference");
}

// Rings of a golden Shape, widened from its Q8.8 points.
struct GoldenRings {
    FixedVec2 points[256];
    Ring      rings[4];
    uint16_t  count = 0;
};
static void loadGolden(const star_golden::GoldenShape& g, GoldenRings& out)
{
    uint16_t at = 0;
    out.count = g.ringCount;
    for (uint16_t r = 0; r < g.ringCount; ++r)
    {
        out.rings[r] = Ring{out.points + at, g.ringSizes[r], true};
        for (uint16_t i = 0; i < g.ringSizes[r]; ++i, ++at)
            out.points[at] = {Fixed::fromQ8_8(g.points[2 * at]), Fixed::fromQ8_8(g.points[2 * at + 1])};
    }
}

// The prototype Star at rest (and the Chomper's even-odd Body): every Shape's
// mask is the prototype's 4x4 majority output, pixel for pixel, on 160x160.
static void testStarGolden()
{
    static GoldenRings gr;
    for (unsigned si = 0; si < star_golden::kShapeCount; ++si)
    {
        const star_golden::GoldenShape& g = star_golden::kShapes[si];
        loadGolden(g, gr);
        const RowMask m = fill(gr.rings, gr.count, g.evenOdd ? FillRule::EvenOdd : FillRule::NonZero,
                               Rect(0, 0, 160, 160));
        std::string want;
        for (uint16_t r = 0; r < g.h; ++r)
        {
            want += g.rows[r];
            if (r + 1 < g.h)
                want += '\n';
        }
        bool ok = expectPicture(m, g.x0, g.y0, g.w, g.h, want.c_str(), g.name);
        // Nothing set outside the golden window.
        for (int y = m.y(); y < m.y() + m.height(); ++y)
            for (int x = m.x(); x < m.x() + m.width(); ++x)
                if (m.test(x, y) && (x < g.x0 || y < g.y0 || x >= g.x0 + g.w || y >= g.y0 + g.h))
                    ok = false;
        if (!ok)
            fprintf(stderr, "  golden mismatch: %s\n", g.name);
        ASSERT(ok, "Star golden Shape matches the prototype's majority mask");
    }
}

// Painting the Star's Shapes in document order, each in its role through the
// purple Look (body 2, accent 3, ink 0), gives the canvas the golden masks
// predict: last writer wins, untouched pixels keep the background, and only
// the tiles a run touched go dirty.
static void testPaintStar()
{
    static Canvas4<160, 160> canvas;
    static uint8_t want[160][160];
    static GoldenRings gr;
    Remap look;
    look.lut[uint8_t(ColourRole::Body)] = 2;
    look.lut[uint8_t(ColourRole::Accent)] = 3;
    look.lut[uint8_t(ColourRole::Ink)] = 0;
    const ColourRole roles[] = {ColourRole::Body,   ColourRole::Body,   ColourRole::Body,   ColourRole::Body,
                                ColourRole::Accent, ColourRole::Accent, ColourRole::Ink,    ColourRole::Accent,
                                ColourRole::Accent, ColourRole::Accent, ColourRole::Accent, ColourRole::Ink,
                                ColourRole::Ink};
    const unsigned starShapes = sizeof(roles) / sizeof(roles[0]);
    ASSERT(starShapes + 1 == star_golden::kShapeCount, "the golden is the Star's 13 Shapes + the Chomper Body");

    canvas.clear(Pixel4(15));
    canvas.clearDirty();
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x)
            want[y][x] = 15;
    bool tileWant[10][10] = {};
    for (unsigned si = 0; si < starShapes; ++si)
    {
        const star_golden::GoldenShape& g = star_golden::kShapes[si];
        loadGolden(g, gr);
        const RowMask m = fill(gr.rings, gr.count, FillRule::NonZero, Rect(0, 0, 160, 160));
        paintFill(canvas, m, roles[si], look);
        for (int r = 0; r < g.h; ++r)
            for (int c = 0; c < g.w; ++c)
                if (g.rows[r][c] == '#')
                {
                    want[g.y0 + r][g.x0 + c] = look.apply(uint8_t(roles[si]));
                    tileWant[(g.y0 + r) / 16][(g.x0 + c) / 16] = true;
                }
    }
    int bad = 0;
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x)
            if (canvas.getPixel(int16_t(x), int16_t(y)).value != want[y][x])
                ++bad;
    ASSERT(bad == 0, "painted Star matches the golden masks in role colours");
    bool tilesOk = true;
    for (int ty = 0; ty < 10; ++ty)
        for (int tx = 0; tx < 10; ++tx)
            if (canvas.isTileDirty(uint8_t(tx), uint8_t(ty)) != tileWant[ty][tx])
                tilesOk = false;
    ASSERT(tilesOk, "exactly the tiles under painted runs are dirty");
}

// ColourRole::None paints nothing; a run that spans mask words and ends on
// the mask's last column is one unbroken span.
static void testPaintEdges()
{
    static Canvas4<160, 160> canvas;
    canvas.clear(Pixel4(9));
    canvas.clearDirty();
    const FixedVec2 pts[] = {px(3, 2), px(103, 2), px(103, 4), px(3, 4)};  // 100 px wide: 4 words
    const Ring ring{pts, 4, true};
    const RowMask m = fill(&ring, 1, FillRule::NonZero, Rect(0, 0, 160, 160));
    paintFill(canvas, m, ColourRole::None, Remap::identity());
    bool untouched = canvas.getPixel(50, 3).value == 9 && !canvas.isTileDirty(3, 0);
    ASSERT(untouched, "role None paints nothing");

    paintFill(canvas, m, ColourRole::Ink, Remap::solid(5));
    bool ok = true;
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 110; ++x)
        {
            const bool in = x >= 3 && x < 103 && y >= 2 && y < 4;
            if (canvas.getPixel(int16_t(x), int16_t(y)).value != (in ? 5 : 9))
                ok = false;
        }
    ASSERT(ok, "a 100 px run across mask words paints exactly its pixels");
}

int main()
{
    testRectOnPixelEdges();
    testTies();
    testHoles();
    testClipAndRefusals();
    testMatchesReference();
    testStarGolden();
    testPaintStar();
    testPaintEdges();
    printf("shape_raster_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
