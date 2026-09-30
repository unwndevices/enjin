// Draw-state bindings (Tomodachi #255): both gfx.setPaletteColor forms, and
// gfx.setLineWidth feeding the line and outline primitives.
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/graphics/palette.hpp>
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
    } while(0)

// LuaEngine + LuaBindings drawing onto one 32x32 compositor layer.
struct DrawFixture {
    LayerCompositor<32, 32> compositor;
    LuaEngine engine;
    LuaBindings bindings;
    LuaCanvas layer0;
    LuaCanvas* layerPtrs[1];

    DrawFixture()
        : bindings(&engine)
        , layer0(&compositor.layers[0])
    {
        layerPtrs[0] = &layer0;
        engine.initialize();
        bindings.registerAll();
        bindings.setLayers(layerPtrs, 1, compositor.visible);
        compositor.clearAll();
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }

    uint8_t px(int16_t x, int16_t y) {
        return compositor.layers[0].getPixel(x, y).value;
    }
};

// ============================================================
// setPaletteColor: (i, r, g, b) and (i, "#rrggbb") set the same entry
// ============================================================
static void test_setPaletteColor_forms()
{
    printf("--- setPaletteColor forms ---\n");

    DrawFixture f;
    const RGB saved = g_palette.getColor(5);

    LuaResult r1 = f.exec("gfx.setPaletteColor(5, 18, 52, 86)");
    ASSERT(r1.success, "setPaletteColor(i, r, g, b) should succeed");
    const RGB a = g_palette.getColor(5);

    g_palette.setColor(5, 0, 0, 0);
    LuaResult r2 = f.exec("gfx.setPaletteColor(5, '#123456')");
    ASSERT(r2.success, "setPaletteColor(i, '#rrggbb') should succeed");
    const RGB b = g_palette.getColor(5);

    ASSERT(a.r == 18 && a.g == 52 && a.b == 86,
           "setPaletteColor(i, r, g, b) should set entry to (18, 52, 86)");
    ASSERT(a.r == b.r && a.g == b.g && a.b == b.b,
           "both setPaletteColor forms should set the same entry to the same colour");

    g_palette.setColor(5, saved.r, saved.g, saved.b);
}

// ============================================================
// setLineWidth: gfx.line draws a band that many px wide
// ============================================================
static void test_line_width()
{
    printf("--- line width ---\n");

    DrawFixture f;
    LuaResult r = f.exec("gfx.setColor(9); gfx.setLineWidth(3); gfx.line(4, 10, 20, 10)");
    ASSERT(r.success, "setLineWidth + line should succeed");

    ASSERT(f.px(12, 8) == 0,  "line w=3: row above the band stays clear");
    ASSERT(f.px(12, 9) == 9,  "line w=3: first row inked");
    ASSERT(f.px(12, 10) == 9, "line w=3: centre row inked");
    ASSERT(f.px(12, 11) == 9, "line w=3: last row inked");
    ASSERT(f.px(12, 12) == 0, "line w=3: row below the band stays clear");

    // Width 1 is the plain 1 px line.
    DrawFixture g;
    g.exec("gfx.setColor(9); gfx.line(4, 10, 20, 10)");
    ASSERT(g.px(12, 10) == 9 && g.px(12, 9) == 0 && g.px(12, 11) == 0,
           "line w=1: exactly one row inked");
}

// ============================================================
// setLineWidth: outline rectangle is that many px deep, inward
// ============================================================
static void test_rectangle_line_width()
{
    printf("--- rectangle line width ---\n");

    DrawFixture f;
    LuaResult r = f.exec("gfx.setColor(9); gfx.setLineWidth(3); gfx.rectangle('line', 4, 4, 16, 16)");
    ASSERT(r.success, "setLineWidth + rectangle('line') should succeed");

    ASSERT(f.px(3, 10) == 0,  "rect w=3: nothing outside the box");
    ASSERT(f.px(4, 10) == 9,  "rect w=3: outer column inked");
    ASSERT(f.px(6, 10) == 9,  "rect w=3: 3 px deep");
    ASSERT(f.px(7, 10) == 0,  "rect w=3: hollow past the width");
    ASSERT(f.px(19, 10) == 9, "rect w=3: right edge inked");
    ASSERT(f.px(17, 10) == 9, "rect w=3: right edge 3 px deep");
    ASSERT(f.px(16, 10) == 0, "rect w=3: right edge hollow past the width");
    ASSERT(f.px(10, 6) == 9 && f.px(10, 7) == 0, "rect w=3: top edge 3 px deep");
}

// ============================================================
// setLineWidth: outline circle is a ring that many px deep, inward
// ============================================================
static void test_circle_line_width()
{
    printf("--- circle line width ---\n");

    DrawFixture f;
    LuaResult r = f.exec("gfx.setColor(9); gfx.setLineWidth(3); gfx.circle('line', 16, 16, 10)");
    ASSERT(r.success, "setLineWidth + circle('line') should succeed");

    ASSERT(f.px(5, 16) == 0,  "circle w=3: nothing outside the radius");
    ASSERT(f.px(6, 16) == 9,  "circle w=3: outer edge inked");
    ASSERT(f.px(8, 16) == 9,  "circle w=3: 3 px deep");
    ASSERT(f.px(9, 16) == 0,  "circle w=3: hollow past the width");
    ASSERT(f.px(16, 16) == 0, "circle w=3: centre hollow");
    ASSERT(f.px(16, 24) == 9 && f.px(16, 23) == 0, "circle w=3: bottom 3 px deep");
}

// ============================================================
// setLineWidth: outline triangle edges are thick
// ============================================================
static void test_triangle_line_width()
{
    printf("--- triangle line width ---\n");

    DrawFixture f;
    LuaResult r = f.exec("gfx.setColor(9); gfx.setLineWidth(3); gfx.triangle('line', 4, 20, 28, 20, 16, 4)");
    ASSERT(r.success, "setLineWidth + triangle('line') should succeed");

    // Bottom edge y=20 is horizontal: a 3 px band centred on it.
    ASSERT(f.px(16, 19) == 9 && f.px(16, 20) == 9 && f.px(16, 21) == 9,
           "triangle w=3: bottom edge 3 px wide");
    ASSERT(f.px(16, 22) == 0, "triangle w=3: nothing below the band");
    ASSERT(f.px(16, 14) == 0, "triangle w=3: interior hollow");
}

// ============================================================
// setLineWidth: a ring as wide as its radius is a full disc
// ============================================================
static void test_circle_width_equals_radius()
{
    printf("--- circle width == radius ---\n");

    DrawFixture f;
    f.exec("gfx.setColor(9); gfx.setLineWidth(4); gfx.circle('line', 16, 16, 4)");
    ASSERT(f.px(16, 16) == 9, "circle w=r: centre inked, no hole");
}

// ============================================================
// setLineWidth: the width reads back as drawn (clamped to 1..255)
// ============================================================
static void test_line_width_clamped()
{
    printf("--- line width clamp ---\n");

    DrawFixture f;
    f.exec("gfx.setLineWidth(1000); hi = gfx.getLineWidth()");
    f.exec("gfx.setLineWidth(0); lo = gfx.getLineWidth()");
    ASSERT(f.engine.getGlobalNumber("hi") == 255.0, "setLineWidth(1000) reads back 255");
    ASSERT(f.engine.getGlobalNumber("lo") == 1.0, "setLineWidth(0) reads back 1");
}

int main()
{
    printf("draw_state_binding_test\n");
    printf("=======================\n");

    test_setPaletteColor_forms();
    test_line_width();
    test_rectangle_line_width();
    test_circle_line_width();
    test_triangle_line_width();
    test_circle_width_equals_radius();
    test_line_width_clamped();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
