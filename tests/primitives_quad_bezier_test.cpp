// Quadratic Bezier primitive test (Tomodachi #226): drawQuadBezier and the
// sagged-line helper on Primitives<TPixel>, the curve the Tape-deck Now Playing
// draws each Tape Span with (control point = chord midpoint + 2·Sag down).
//
// The seams under test are the pixels a real Canvas4 ends up holding:
//   - a control point on the chord gives exactly drawLine's pixels, in every
//     octant, and Sag 0 is drawLine, so a Span doesn't flicker by a pixel as
//     its Sag reaches 0;
//   - every curve is a thin 8-connected path from end to end: no gaps, no
//     doubled "L" corners, no side-by-side steps, both endpoints drawn;
//   - a curve mirrored about the vertical comes out mirrored;
//   - a Span's middle droops by exactly its Sag, each column holds the pixel
//     nearest the curve, and more Sag only ever moves pixels down;
//   - dirty tiles and canvas-edge clipping behave like every other primitive.
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <numeric>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { passes++; }                                                    \
    } while (0)

static constexpr int16_t N = 64;
using C = Canvas4<N, N>;
using P = Primitives<Pixel4>;

static bool sameCanvas(const C& a, const C& b) {
    for (int16_t y = 0; y < N; y++)
        for (int16_t x = 0; x < N; x++)
            if (a.getPixel(x, y) != b.getPixel(x, y)) return false;
    return true;
}

// Chords that cover every octant from a common centre, plus the axis-aligned
// and 45° cases and a degenerate point.
struct Chord { int16_t x0, y0, x1, y1; };
static const Chord kOctantChords[] = {
    {32, 32, 52, 37}, {32, 32, 37, 52}, {32, 32, 27, 52}, {32, 32, 12, 37},
    {32, 32, 12, 27}, {32, 32, 27, 12}, {32, 32, 37, 12}, {32, 32, 52, 27},
    {32, 32, 52, 32}, {32, 32, 12, 32}, {32, 32, 32, 52}, {32, 32, 32, 12},
    {32, 32, 50, 50}, {32, 32, 14, 50}, {32, 32, 14, 14}, {32, 32, 50, 14},
    {32, 32, 55, 41}, {32, 32, 41, 55}, {32, 32, 9, 23},  {32, 32, 23, 9},
    {5, 7, 58, 9},    {58, 9, 5, 7},    {6, 60, 8, 3},    {32, 32, 32, 32},
};

// Every lattice point on the chord (ends included) is a control point on the
// chord: the curve is then the straight segment, and must be drawLine's pixels.
static void test_control_on_chord_matches_drawLine() {
    char msg[160];
    for (const Chord& c : kOctantChords) {
        const int16_t dx = c.x1 - c.x0, dy = c.y1 - c.y0;
        const int16_t steps = static_cast<int16_t>(std::gcd(abs(dx), abs(dy)));
        C line;
        line.clear(Colors::BLACK);
        P::drawLine(line, c.x0, c.y0, c.x1, c.y1, Colors::WHITE);
        for (int16_t i = 0; i <= steps; i++) {
            const int16_t cx = steps ? c.x0 + dx / steps * i : c.x0;
            const int16_t cy = steps ? c.y0 + dy / steps * i : c.y0;
            C curve;
            curve.clear(Colors::BLACK);
            P::drawQuadBezier(curve, c.x0, c.y0, cx, cy, c.x1, c.y1, Colors::WHITE);
            snprintf(msg, sizeof msg,
                     "control (%d,%d) on chord (%d,%d)-(%d,%d) draws drawLine's pixels",
                     cx, cy, c.x0, c.y0, c.x1, c.y1);
            ASSERT(sameCanvas(curve, line), msg);
        }
    }
}

static bool lit(const ICanvas<Pixel4>& c, int16_t x, int16_t y) {
    return x >= 0 && y >= 0 && x < static_cast<int16_t>(c.getWidth()) &&
           y < static_cast<int16_t>(c.getHeight()) && c.getPixel(x, y) != 0;
}

static int litNeighbours(const ICanvas<Pixel4>& c, int16_t x, int16_t y) {
    int n = 0;
    for (int16_t j = -1; j <= 1; j++)
        for (int16_t i = -1; i <= 1; i++)
            if ((i || j) && lit(c, x + i, y + j)) n++;
    return n;
}

// The pixel rule: the lit pixels are one thin 8-connected path from (x0,y0) to
// (x1,y1). Both ends are lit and touch exactly one other pixel; every other
// pixel touches exactly two (a gap breaks the walk, an "L" corner or a
// side-by-side step gives some pixel a third neighbour); walking the path from
// one end reaches the other end and visits every lit pixel.
static bool isThinPath(const ICanvas<Pixel4>& c, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if (!lit(c, x0, y0) || !lit(c, x1, y1)) return false;
    const int16_t w = static_cast<int16_t>(c.getWidth()), h = static_cast<int16_t>(c.getHeight());
    int total = 0;
    for (int16_t y = 0; y < h; y++)
        for (int16_t x = 0; x < w; x++)
            if (lit(c, x, y)) {
                total++;
                const bool end = (x == x0 && y == y0) || (x == x1 && y == y1);
                if (litNeighbours(c, x, y) != (end ? 1 : 2)) return false;
            }
    int16_t px = x0, py = y0, x = x0, y = y0;
    int walked = 1;
    while (!(x == x1 && y == y1)) {
        int16_t nx = x, ny = y;
        for (int16_t j = -1; j <= 1; j++)
            for (int16_t i = -1; i <= 1; i++)
                if ((i || j) && lit(c, x + i, y + j) && !(x + i == px && y + j == py)) {
                    nx = x + i;
                    ny = y + j;
                }
        if (nx == x && ny == y) return false;
        px = x; py = y; x = nx; y = ny;
        walked++;
    }
    return walked == total;
}

// Chords for the droop sweep: shallow, steep, rising, falling, both widths
// parities, long and short, the way Tape Spans run between Reel and Guides.
static const Chord kSweepChords[] = {
    {4, 10, 60, 10},  {4, 10, 59, 10},  {4, 10, 60, 18},  {60, 10, 4, 18},
    {10, 4, 40, 30},  {40, 4, 10, 30},  {20, 4, 26, 40},  {26, 4, 20, 40},
    {8, 20, 24, 20},  {8, 20, 23, 22},  {30, 6, 31, 30},  {12, 8, 52, 9},
    {6, 30, 58, 4},   {58, 30, 6, 4},   {10, 10, 30, 30}, {30, 10, 10, 30},
};

// No gaps, no L corners and both endpoints drawn, for every droop from barely
// off the chord up to about half the chord's length. A steep chord only droops
// until its control point reaches the lower end: past that, pulling straight
// down folds the curve back over its own end (a hairpin), which is not a path.
static void test_sag_sweep_is_thin_path() {
    char msg[160];
    for (const Chord& c : kSweepChords) {
        const int16_t w = static_cast<int16_t>(abs(c.x1 - c.x0));
        const int16_t h = static_cast<int16_t>(abs(c.y1 - c.y0));
        const int16_t lowEnd = std::max(c.y0, c.y1);
        for (int16_t sag = 1; sag <= std::max(w, h) / 2; sag++) {
            const int16_t cx = (c.x0 + c.x1) / 2;
            const int16_t cy = (c.y0 + c.y1) / 2 + 2 * sag;
            if (h > w && cy > lowEnd) break;
            C curve;
            curve.clear(Colors::BLACK);
            P::drawQuadBezier(curve, c.x0, c.y0, cx, cy, c.x1, c.y1, Colors::WHITE);
            snprintf(msg, sizeof msg,
                     "(%d,%d)-(%d,%d) control (%d,%d): thin 8-connected path, ends drawn",
                     c.x0, c.y0, c.x1, c.y1, cx, cy);
            ASSERT(isThinPath(curve, c.x0, c.y0, c.x1, c.y1), msg);
        }
    }
}

// A drooping curve is not its chord: the middle of the curve is drawn below it.
static void test_sagged_curve_leaves_the_chord() {
    C curve;
    curve.clear(Colors::BLACK);
    P::drawQuadBezier(curve, 4, 10, 32, 30, 60, 10, Colors::WHITE);
    ASSERT(curve.getPixel(32, 20) != 0, "control 20 below a flat chord: middle droops by 10");
    ASSERT(curve.getPixel(32, 10) == 0, "control 20 below a flat chord: chord middle not drawn");
}

// Is b the mirror image of a about column m (x <-> 2m - x)?
static bool isMirrorAbout(const C& a, const C& b, int16_t m) {
    for (int16_t y = 0; y < N; y++)
        for (int16_t x = 0; x < N; x++) {
            const int16_t mx = 2 * m - x;
            const bool other = mx >= 0 && mx < N && b.getPixel(mx, y) != 0;
            if ((a.getPixel(x, y) != 0) != other) return false;
        }
    return true;
}

// A Span hung level between two Guides droops symmetrically: its left half is
// the mirror of its right half, for every width and droop.
static void test_symmetric_curve_is_symmetric() {
    char msg[160];
    for (int16_t half = 1; half <= 30; half++) {
        for (int16_t sag = 1; sag <= 24; sag++) {
            C curve;
            curve.clear(Colors::BLACK);
            P::drawQuadBezier(curve, 32 - half, 8, 32, 8 + 2 * sag, 32 + half, 8, Colors::WHITE);
            snprintf(msg, sizeof msg, "level chord half-width %d, sag %d: symmetric about its middle",
                     half, sag);
            ASSERT(isMirrorAbout(curve, curve, 32), msg);
        }
    }
}

// Mirroring the inputs mirrors the pixels, for sloped Spans too: a Span doesn't
// rasterise differently depending on which side of the screen it hangs.
static void test_mirrored_inputs_mirror_the_pixels() {
    char msg[160];
    for (const Chord& c : kSweepChords) {
        for (int16_t sag = 1; sag <= 12; sag++) {
            const int16_t cx = (c.x0 + c.x1) / 2;
            const int16_t cy = (c.y0 + c.y1) / 2 + 2 * sag;
            C curve, mirror;
            curve.clear(Colors::BLACK);
            mirror.clear(Colors::BLACK);
            P::drawQuadBezier(curve, c.x0, c.y0, cx, cy, c.x1, c.y1, Colors::WHITE);
            P::drawQuadBezier(mirror, 64 - c.x0, c.y0, 64 - cx, cy, 64 - c.x1, c.y1, Colors::WHITE);
            snprintf(msg, sizeof msg, "(%d,%d)-(%d,%d) control (%d,%d): mirrored inputs, mirrored pixels",
                     c.x0, c.y0, c.x1, c.y1, cx, cy);
            ASSERT(isMirrorAbout(curve, mirror, 32), msg);
        }
    }
}

// Sag 0 is taut: drawLine's pixels, for every chord (odd-length chords too,
// whose midpoint isn't a pixel).
static void test_sag_zero_is_drawLine() {
    char msg[160];
    for (const Chord& c : kOctantChords) {
        C line, sagged;
        line.clear(Colors::BLACK);
        sagged.clear(Colors::BLACK);
        P::drawLine(line, c.x0, c.y0, c.x1, c.y1, Colors::WHITE);
        P::drawSaggedLine(sagged, c.x0, c.y0, c.x1, c.y1, 0, Colors::WHITE);
        snprintf(msg, sizeof msg, "(%d,%d)-(%d,%d) sag 0: drawLine's pixels", c.x0, c.y0, c.x1, c.y1);
        ASSERT(sameCanvas(sagged, line), msg);
    }
}

// The middle of a level Span droops by exactly its Sag, and nothing hangs lower.
static void test_sag_droops_the_middle_by_sag() {
    char msg[160];
    for (int16_t sag = 1; sag <= 20; sag++) {
        C curve;
        curve.clear(Colors::BLACK);
        P::drawSaggedLine(curve, 6, 10, 58, 10, sag, Colors::WHITE);
        int16_t lowest = -1;
        for (int16_t y = 0; y < N; y++)
            for (int16_t x = 0; x < N; x++)
                if (curve.getPixel(x, y) != 0) lowest = y;
        snprintf(msg, sizeof msg, "level chord, sag %d: middle pixel droops by %d", sag, sag);
        ASSERT(curve.getPixel(32, 10 + sag) != 0, msg);
        snprintf(msg, sizeof msg, "level chord, sag %d: lowest pixel is %d below", sag, sag);
        ASSERT(lowest == 10 + sag, msg);
    }
}

// Topmost and bottommost lit row in column x, or false if the column is empty.
static bool columnSpan(const C& c, int16_t x, int16_t& top, int16_t& bottom) {
    top = -1;
    for (int16_t y = 0; y < N; y++)
        if (c.getPixel(x, y) != 0) {
            if (top < 0) top = y;
            bottom = y;
        }
    return top >= 0;
}

// Easing a Span from taut to drooping never flickers upward: each extra pixel of
// Sag only ever moves a column's pixels down (or leaves them), starting from
// drawLine at Sag 0. Shallow chords, where the Tape's droop reads per column.
static void test_more_sag_only_moves_pixels_down() {
    char msg[160];
    for (const Chord& c : kSweepChords) {
        if (abs(c.y1 - c.y0) > abs(c.x1 - c.x0)) continue;
        C prev;
        prev.clear(Colors::BLACK);
        P::drawSaggedLine(prev, c.x0, c.y0, c.x1, c.y1, 0, Colors::WHITE);
        for (int16_t sag = 1; sag <= 12; sag++) {
            C next;
            next.clear(Colors::BLACK);
            P::drawSaggedLine(next, c.x0, c.y0, c.x1, c.y1, sag, Colors::WHITE);
            bool down = true;
            for (int16_t x = 0; x < N; x++) {
                int16_t pt = 0, pb = 0, nt = 0, nb = 0;
                const bool had = columnSpan(prev, x, pt, pb);
                const bool has = columnSpan(next, x, nt, nb);
                if (had != has || (had && (nt < pt || nb < pb))) down = false;
            }
            snprintf(msg, sizeof msg, "(%d,%d)-(%d,%d) sag %d -> %d: pixels only move down",
                     c.x0, c.y0, c.x1, c.y1, sag - 1, sag);
            ASSERT(down, msg);
            prev = next;
        }
    }
}

// Each column of a shallow sagged Span holds the pixel nearest the true curve,
// all the way along, including next to the bottom of the droop. With the
// control point at the chord's midpoint x moves evenly with t, so the curve's
// height over column x is an exact fraction: y = Y / w² with k = x - x0 and
// Y = (w-k)²·y0 + 2k(w-k)·cy + k²·y1. Only columns flatter than 1:2 count:
// nearer 45° the walk's pick is nearest across the curve, not straight down.
// Near-ties (within 1/16 px of half a pixel) are skipped for the same reason;
// the error this guards against, re-fitting past a rounded turn, is ~0.4 px.
static void test_sagged_columns_are_nearest_pixel() {
    char msg[160];
    for (const Chord& c : kSweepChords) {
        const int32_t w = c.x1 - c.x0;
        if (w % 2 != 0 || abs(c.y1 - c.y0) > abs(w)) continue;
        for (int16_t sag = 1; sag <= 16; sag++) {
            const int32_t cy = c.y0 + (c.y1 - c.y0) / 2 + 2 * sag;
            C curve;
            curve.clear(Colors::BLACK);
            P::drawSaggedLine(curve, c.x0, c.y0, c.x1, c.y1, sag, Colors::WHITE);
            const int32_t w2 = w * w;
            bool nearest = true;
            const int32_t step = w > 0 ? 1 : -1;
            for (int32_t k = step; k != w; k += step) {
                const int32_t Y = (w - k) * (w - k) * c.y0 + 2 * k * (w - k) * cy + k * k * c.y1;
                const int32_t slope = 2 * (k - w) * c.y0 + 2 * (w - 2 * k) * cy + 2 * k * c.y1;
                const int32_t half = (2 * Y) % (2 * w2) - w2; // 0 = exactly half a pixel
                if (2 * abs(slope) > w2 || 8 * abs(half) < w2) continue;
                const int16_t want = static_cast<int16_t>((2 * Y + w2) / (2 * w2));
                if (curve.getPixel(static_cast<int16_t>(c.x0 + k), want) == 0) nearest = false;
            }
            snprintf(msg, sizeof msg, "(%d,%d)-(%d,%d) sag %d: every column is the nearest pixel",
                     c.x0, c.y0, c.x1, c.y1, sag);
            ASSERT(nearest, msg);
        }
    }
}

// Curves that bow any way, not just down: turning back in y, in x (vertical
// chords bulging sideways), and in both at once. Each is still a thin path.
static void test_general_curves_are_thin_paths() {
    struct Curve { int16_t x0, y0, cx, cy, x1, y1; };
    static const Curve kCurves[] = {
        {8, 50, 32, 10, 56, 50},  {10, 10, 50, 32, 10, 54}, {54, 10, 14, 32, 54, 54},
        {8, 40, 56, 56, 40, 8},   {10, 20, 60, 60, 50, 30}, {5, 5, 20, 40, 60, 20},
        {60, 60, 2, 40, 30, 4},   {4, 30, 30, 2, 58, 58},   {20, 60, 62, 30, 4, 4},
    };
    char msg[160];
    for (const Curve& k : kCurves) {
        C curve;
        curve.clear(Colors::BLACK);
        P::drawQuadBezier(curve, k.x0, k.y0, k.cx, k.cy, k.x1, k.y1, Colors::WHITE);
        snprintf(msg, sizeof msg, "(%d,%d) control (%d,%d) (%d,%d): thin 8-connected path",
                 k.x0, k.y0, k.cx, k.cy, k.x1, k.y1);
        ASSERT(isThinPath(curve, k.x0, k.y0, k.x1, k.y1), msg);
    }
}

// Like every primitive, a curve marks the dirty tiles it draws into: exactly
// the tiles holding its pixels, and no others.
static void test_marks_exactly_its_dirty_tiles() {
    C curve;
    curve.clear(Colors::BLACK);
    curve.clearDirty();
    P::drawSaggedLine(curve, 4, 6, 60, 20, 9, Colors::WHITE);
    bool exact = true;
    for (uint16_t ty = 0; ty < C::TILES_Y; ty++)
        for (uint16_t tx = 0; tx < C::TILES_X; tx++) {
            bool inked = false;
            for (int16_t y = ty * C::TILE_SIZE; y < (ty + 1) * C::TILE_SIZE; y++)
                for (int16_t x = tx * C::TILE_SIZE; x < (tx + 1) * C::TILE_SIZE; x++)
                    if (curve.getPixel(x, y) != 0) inked = true;
            if (inked != curve.isTileDirty(tx, ty)) exact = false;
        }
    ASSERT(curve.hasDirty(), "sagged line marks dirty tiles");
    ASSERT(exact, "sagged line marks exactly the tiles it inks");
}

// Clipping is per pixel at the canvas edge: a curve running off a small canvas
// leaves exactly the on-canvas part of the same curve on a bigger one.
static void test_clips_per_pixel_at_the_canvas_edge() {
    Canvas4<32, 32> small;
    small.clear(Colors::BLACK);
    P::drawQuadBezier(small, -10, 4, 20, 60, 50, 10, Colors::WHITE);
    C big;
    big.clear(Colors::BLACK);
    P::drawQuadBezier(big, -10, 4, 20, 60, 50, 10, Colors::WHITE);
    bool same = true;
    for (int16_t y = 0; y < 32; y++)
        for (int16_t x = 0; x < 32; x++)
            if (small.getPixel(x, y) != big.getPixel(x, y)) same = false;
    ASSERT(same, "curve off the canvas edge: the on-canvas pixels are unchanged");
}

// Tape scale (160×160, Canvas4 like the player): where a Span's slope crosses
// 45° the exact curve can pass through a pixel corner, and a walk would take
// an x step and a y step in turn. The corner pixel is dropped, so these stay
// thin paths; the first four are cases a full sweep once caught.
static void test_tape_scale_spans_have_no_L_corners() {
    static const struct { Chord c; int16_t sag; } kCases[] = {
        {{2, 10, 32, 18}, 12}, {{2, 10, 14, 42}, 7}, {{2, 10, 56, 26}, 20}, {{2, 19, 14, 50}, 7},
    };
    char msg[160];
    static Canvas4<160, 160> tape;
    for (const auto& k : kCases) {
        tape.clear(Colors::BLACK);
        P::drawSaggedLine(tape, k.c.x0, k.c.y0, k.c.x1, k.c.y1, k.sag, Colors::WHITE);
        snprintf(msg, sizeof msg, "tape (%d,%d)-(%d,%d) sag %d: no L corners",
                 k.c.x0, k.c.y0, k.c.x1, k.c.y1, k.sag);
        ASSERT(isThinPath(tape, k.c.x0, k.c.y0, k.c.x1, k.c.y1), msg);
    }
    static const int16_t kX0[] = {2, 37}, kY0[] = {12, 47};
    static const int16_t kX1[] = {23, 71, 118, 157}, kY1[] = {9, 38, 70, 101};
    int bad = 0, total = 0;
    for (int16_t x0 : kX0) for (int16_t y0 : kY0) for (int16_t x1 : kX1) for (int16_t y1 : kY1) {
        const int16_t w = static_cast<int16_t>(abs(x1 - x0)), h = static_cast<int16_t>(abs(y1 - y0));
        for (int16_t sag = 1; sag <= std::min<int16_t>(std::max(w, h) / 2, 24); sag++) {
            if (h > w && y0 + (y1 - y0) / 2 + 2 * sag > std::max(y0, y1)) break;
            tape.clear(Colors::BLACK);
            P::drawSaggedLine(tape, x0, y0, x1, y1, sag, Colors::WHITE);
            total++;
            if (!isThinPath(tape, x0, y0, x1, y1)) bad++;
        }
    }
    snprintf(msg, sizeof msg, "tape-scale sweep: %d of %d sagged Spans are thin paths", total - bad, total);
    ASSERT(bad == 0, msg);
}

int main() {
    test_control_on_chord_matches_drawLine();
    test_sagged_curve_leaves_the_chord();
    test_sag_sweep_is_thin_path();
    test_symmetric_curve_is_symmetric();
    test_mirrored_inputs_mirror_the_pixels();
    test_sag_zero_is_drawLine();
    test_sag_droops_the_middle_by_sag();
    test_more_sag_only_moves_pixels_down();
    test_sagged_columns_are_nearest_pixel();
    test_general_curves_are_thin_paths();
    test_marks_exactly_its_dirty_tiles();
    test_clips_per_pixel_at_the_canvas_edge();
    test_tape_scale_spans_have_no_L_corners();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
