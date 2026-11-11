// The span paths behind gfx.rectangle, the wide gfx.line and gfx.drawSprite
// (Tomodachi #488): each paints exactly the pixels and dirty tiles of the
// per-pixel walk it replaced. The references here are those walks, written
// against a plain Canvas4 through setPixel:
// - a fill: every pixel of the clipped rect;
// - a wide line: a width x width square stamped on each Bresenham step;
// - a sprite: the source cell walked, flipped and rotated, index 15 skipped,
//   an effect shading each pixel at its destination.
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/effect_lua.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    } while(0)

// An odd width, so spans end on either nibble.
constexpr uint16_t W = 61, H = 47;
using Canvas = Canvas4<W, H>;

// A busy background: a span that over- or under-writes a nibble shows.
static void paintBackground(Canvas& c)
{
    static constexpr uint8_t kValid[9] = {0, 1, 2, 3, 4, 5, 6, 7, 15};
    for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x)
            c.setPixel(x, y, Pixel4(kValid[(x * 7 + y * 3 + 1) % 9]));
    c.clearDirty();
}

static bool same(const Canvas& a, const Canvas& b)
{
    for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x)
            if (a.getPixel(x, y).value != b.getPixel(x, y).value) return false;
    return memcmp(a.dirtyBitmap(), b.dirtyBitmap(), Canvas::dirtyBitmapBytes()) == 0;
}

static void refFill(Canvas& c, int x, int y, int w, int h, uint8_t color)
{
    for (int py = y; py < y + h; ++py)
        for (int px = x; px < x + w; ++px)
            c.setPixel(static_cast<int16_t>(px), static_cast<int16_t>(py), Pixel4(color));
}

static void refLine(Canvas& c, int x1, int y1, int x2, int y2, uint8_t color, int width)
{
    const int off = (width - 1) / 2;
    const int dx = abs(x2 - x1), dy = abs(y2 - y1);
    const int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;
    while (true) {
        refFill(c, x - off, y - off, width, width, color);
        if (x == x2 && y == y2) break;
        const int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

struct Fixture {
    LuaEngine engine;
    LuaBindings bindings;
    Canvas canvas;
    LuaCanvas luaCanvas;

    Fixture() : bindings(&engine), luaCanvas(&canvas)
    {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);
    }
    bool exec(const char* code) { return engine.executeString(code).success; }
};

static uint32_t rng = 0x488u;
static int rnd(int lo, int hi)  // inclusive
{
    rng = rng * 1664525u + 1013904223u;
    return lo + static_cast<int>((rng >> 8) % static_cast<uint32_t>(hi - lo + 1));
}

static void test_fill_matches_per_pixel()
{
    printf("--- gfx.rectangle fill = the per-pixel fill ---\n");
    Fixture f;
    static Canvas ref;
    int bad = 0;
    for (int i = 0; i < 400; ++i) {
        const int x = rnd(-20, W + 5), y = rnd(-20, H + 5), w = rnd(0, 40), h = rnd(0, 30);
        const uint8_t color = static_cast<uint8_t>(rnd(0, 7));
        paintBackground(f.canvas);
        paintBackground(ref);
        char code[128];
        snprintf(code, sizeof code, "gfx.setColor(%d) gfx.rectangle('fill', %d, %d, %d, %d)", color, x, y, w, h);
        f.exec(code);
        refFill(ref, x, y, w, h, color);
        bad += !same(f.canvas, ref);
    }
    ASSERT(bad == 0, "400 rects, on and off every edge: same pixels and dirty tiles");
}

static void test_wide_lines_match_the_stamps()
{
    printf("--- wide gfx.line = the stamped pen ---\n");
    Fixture f;
    static Canvas ref;
    int bad = 0, lines = 0;
    const int widths[] = {2, 3, 4, 5, 6, 7, 9, 12, 32, 33};
    for (int width : widths) {
        for (int i = 0; i < 300; ++i) {
            // Short and long, steep and shallow, in every direction, off the edges.
            const int x1 = rnd(-15, W + 15), y1 = rnd(-15, H + 15);
            const int span = i % 3 == 0 ? 4 : 70;
            const int x2 = x1 + rnd(-span, span), y2 = y1 + rnd(-span, span);
            const uint8_t color = static_cast<uint8_t>(rnd(0, 7));
            paintBackground(f.canvas);
            paintBackground(ref);
            char code[160];
            snprintf(code, sizeof code, "gfx.setLineWidth(%d) gfx.setColor(%d) gfx.line(%d, %d, %d, %d)",
                     width, color, x1, y1, x2, y2);
            f.exec(code);
            refLine(ref, x1, y1, x2, y2, color, width);
            if (!same(f.canvas, ref)) {
                if (bad < 5) fprintf(stderr, "  w=%d (%d,%d)-(%d,%d)\n", width, x1, y1, x2, y2);
                ++bad;
            }
            ++lines;
        }
    }
    ASSERT(bad == 0, "3000 wide lines, widths 2..33: same pixels and dirty tiles");
    printf("  %d lines\n", lines);
}

static void test_polylines_match_their_lines()
{
    printf("--- gfx.polyline = gfx.line point to point ---\n");
    Fixture f;
    static Canvas ref;
    int bad = 0, strokes = 0;
    const int widths[] = {1, 2, 3, 4, 5, 7, 9, 32, 33};
    for (int width : widths) {
        for (int i = 0; i < 200; ++i) {
            // A string's ripple (y one way, x swinging), or any zigzag.
            const int n = rnd(1, 22);
            const bool oneWay = i % 2 == 0;
            const int step = rnd(0, 9) * (i % 4 == 0 ? -1 : 1);
            int xs[22], ys[22];
            xs[0] = rnd(-10, W + 10);
            ys[0] = rnd(-10, H + 10);
            for (int k = 1; k < n; ++k) {
                xs[k] = oneWay ? xs[0] + rnd(-9, 9) : rnd(-10, W + 10);
                ys[k] = oneWay ? ys[k - 1] + step + (k % 5 == 0 ? 0 : rnd(0, 2) * (step < 0 ? -1 : 1))
                               : rnd(-10, H + 10);
            }
            const uint8_t color = static_cast<uint8_t>(rnd(0, 7));
            paintBackground(f.canvas);
            paintBackground(ref);
            char code[1024];
            int len = snprintf(code, sizeof code, "gfx.setLineWidth(%d) gfx.setColor(%d) gfx.polyline({", width, color);
            for (int k = 0; k < n; ++k) len += snprintf(code + len, sizeof code - len, "%d, %d, ", xs[k], ys[k]);
            snprintf(code + len, sizeof code - len, "})");
            f.exec(code);
            if (n == 1) refLine(ref, xs[0], ys[0], xs[0], ys[0], color, width);
            for (int k = 1; k < n; ++k) refLine(ref, xs[k - 1], ys[k - 1], xs[k], ys[k], color, width);
            if (!same(f.canvas, ref)) {
                if (bad < 5) fprintf(stderr, "  w=%d n=%d oneWay=%d\n", width, n, oneWay);
                ++bad;
            }
            ++strokes;
        }
    }
    ASSERT(bad == 0, "1800 polylines, one way in y and zigzag: the pixels of their gfx.line calls");
    // Fractions round as gfx.line's do.
    paintBackground(f.canvas);
    paintBackground(ref);
    f.exec("gfx.setLineWidth(5) gfx.setColor(3) gfx.polyline({10.4, 2.5, 13.6, 10, 11.5, 18.49})");
    f.exec("gfx.setLineWidth(5) gfx.setColor(3) gfx.line(10.4, 2.5, 13.6, 10) gfx.line(13.6, 10, 11.5, 18.49)");
    paintBackground(ref);
    refLine(ref, 10, 3, 14, 10, 3, 5);
    refLine(ref, 14, 10, 12, 18, 3, 5);
    ASSERT(same(f.canvas, ref), "fractional points round as gfx.line's");
    printf("  %d polylines\n", strokes);
}

// A 13 x 9 cell: every index, a run of 15s and a lone 15.
static uint8_t spriteData[13 * 9];

static void refSprite(Canvas& c, int x, int y, bool flipH, bool flipV, bool rotate90,
                      const Effect* effect)
{
    const int cellW = 13, cellH = 9;
    for (int fy = 0; fy < cellH; ++fy) {
        for (int fx = 0; fx < cellW; ++fx) {
            const int srcX = flipH ? (cellW - 1 - fx) : fx;
            const int srcY = flipV ? (cellH - 1 - fy) : fy;
            const uint8_t px = spriteData[srcY * cellW + srcX] & 0x0F;
            if (px == 15) continue;
            const int dstX = rotate90 ? (cellH - 1 - fy) : fx;
            const int dstY = rotate90 ? fx : fy;
            const int16_t ax = static_cast<int16_t>(x + dstX), ay = static_cast<int16_t>(y + dstY);
            c.setPixel(ax, ay, Pixel4(effect ? effect->shadePixel(px, ax, ay) : px));
        }
    }
}

static void test_sprites_match_the_source_walk()
{
    printf("--- gfx.drawSprite = the per-pixel source walk ---\n");
    for (int i = 0; i < 13 * 9; ++i) {
        const int v = (i * 5 + i / 13) % 11;
        spriteData[i] = static_cast<uint8_t>(v >= 8 ? 15 : v);
    }
    spriteData[13 * 4 + 6] = 15;
    Fixture f;
    lua_State* L = f.engine.getState();
    lua_pushlightuserdata(L, spriteData);
    lua_setglobal(L, "_d");
    ASSERT(f.exec("_s = gfx.newSprite(_d, 13, 9, 1, 1) "
                  "_fx = gfx.effect(gfx.mask('stripe', 3, 1), gfx.remap('solid', 6), 1, 2)"),
           "the sprite and the effect");
    lua_getglobal(L, "_fx");
    const Effect effect = lua::checkEffect(L, -1);
    lua_pop(L, 1);

    static Canvas ref;
    int bad = 0;
    for (int i = 0; i < 240; ++i) {
        const int x = rnd(-16, W + 2), y = rnd(-16, H + 2);
        const bool flipH = i & 1, flipV = i & 2, rot = i & 4, withFx = i & 8;
        paintBackground(f.canvas);
        paintBackground(ref);
        char code[160];
        snprintf(code, sizeof code, "gfx.drawSprite(_s, %d, %d, %s, %s, %s%s)", x, y,
                 flipH ? "true" : "false", flipV ? "true" : "false", rot ? "true" : "false",
                 withFx ? ", _fx" : "");
        f.exec(code);
        refSprite(ref, x, y, flipH, flipV, rot, withFx ? &effect : nullptr);
        if (!same(f.canvas, ref)) {
            if (bad < 5) fprintf(stderr, "  (%d,%d) h=%d v=%d r=%d fx=%d\n", x, y, flipH, flipV, rot, withFx);
            ++bad;
        }
    }
    ASSERT(bad == 0, "240 sprites, every flip, rotation and edge, with and without an effect");
}

int main()
{
    printf("=== draw_spans_test ===\n\n");
    test_fill_matches_per_pixel();
    test_wide_lines_match_the_stamps();
    test_polylines_match_their_lines();
    test_sprites_match_the_source_walk();
    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
