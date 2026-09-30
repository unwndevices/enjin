#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
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
    } while(0)

// ============================================================
// Test fixture: LuaEngine + LuaBindings + 4 Canvas4 layers
// ============================================================
struct LayerBindingFixture {
    LayerCompositor<16, 16> compositor;
    LuaEngine engine;
    LuaBindings bindings;

    LuaCanvas layer0;
    LuaCanvas layer1;
    LuaCanvas layer2;

    LuaCanvas* layerPtrs[3];

    LayerBindingFixture()
        : bindings(&engine)
        , layer0(&compositor.layers[0])
        , layer1(&compositor.layers[1])
        , layer2(&compositor.layers[2])
    {
        layerPtrs[0] = &layer0;
        layerPtrs[1] = &layer1;
        layerPtrs[2] = &layer2;

        engine.initialize();
        bindings.registerAll();
        bindings.setLayers(layerPtrs, 3, compositor.visible);
        compositor.clearAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }
};

// ============================================================
// test_setLayer_getLayer_roundtrip
// ============================================================
static void test_setLayer_getLayer_roundtrip()
{
    printf("--- setLayer/getLayer roundtrip ---\n");

    LayerBindingFixture f;
    LuaResult r = f.exec("gfx.setLayer(3); result = gfx.getLayer()");
    ASSERT(r.success, "setLayer/getLayer script should succeed");
    ASSERT(f.getNum("result") == 3.0,
           "roundtrip: getLayer() should return 3 after setLayer(3)");
    ASSERT(f.exec("gfx.setLayer(gfx.LAYER_BG); gfx.setPixel(0,0,5); "
                  "gfx.setLayer(gfx.LAYER_MID); gfx.setPixel(0,0,6); "
                  "gfx.setLayer(gfx.LAYER_FG); gfx.setPixel(0,0,7)").success,
           "each public layer constant draws successfully");
    for (int i = 0; i < 3; ++i)
        ASSERT(f.compositor.layers[i].getPixel(0, 0).value == i + 5,
               "each public layer constant addresses its own compositor slot");
}

// ============================================================
// test_setLayer_reject_low
// ============================================================
static void test_setLayer_reject_low()
{
    printf("--- setLayer reject low ---\n");

    LayerBindingFixture f;
    LuaResult r = f.exec("gfx.setLayer(0); result = gfx.getLayer()");
    ASSERT(!r.success, "gfx.setLayer(0) must error");
}

// ============================================================
// test_setLayer_reject_high
// ============================================================
static void test_setLayer_reject_high()
{
    printf("--- setLayer reject high ---\n");

    LayerBindingFixture f;
    LuaResult r = f.exec("gfx.setLayer(99); result = gfx.getLayer()");
    ASSERT(!r.success, "gfx.setLayer(99) must error");
    ASSERT(!f.exec("gfx.setLayer(4)").success, "UI layer must error");
    ASSERT(f.exec("for _, n in ipairs({0, 4, 256, -1}) do "
                  "assert(not pcall(gfx.clearLayer, n, 5)); "
                  "assert(not pcall(gfx.setLayerVisible, n, false)); "
                  "assert(not pcall(gfx.isLayerVisible, n)) end").success,
           "all layer-taking enjin bindings reject non-applet indices");
    ASSERT(f.compositor.visible[3], "UI layer visibility stays unchanged");
}

// ============================================================
// test_clearLayer_specific
// ============================================================
static void test_clearLayer_specific()
{
    printf("--- clearLayer specific ---\n");

    LayerBindingFixture f;

    // Draw something on layer 1 (Lua index 1, cpp index 0)
    f.exec("gfx.setLayer(1); gfx.setPixel(0, 0, 7)");
    // Draw something on layer 2 (Lua index 2, cpp index 1)
    f.exec("gfx.setLayer(2); gfx.setPixel(0, 0, 3)");

    // Clear only layer 2 to color 5
    f.exec("gfx.clearLayer(2, 5)");

    // Layer 1 pixel should be untouched (7)
    uint8_t layer1_val = f.compositor.layers[0].getPixel(0, 0).value;
    ASSERT(layer1_val == 7,
           "clearLayer: layer 1 pixel (0,0) should still be 7 (untouched)");

    // Layer 2 should be cleared to 5
    uint8_t layer2_val = f.compositor.layers[1].getPixel(0, 0).value;
    ASSERT(layer2_val == 5,
           "clearLayer: layer 2 pixel (0,0) should be 5 (cleared)");
}

// ============================================================
// test_clearLayer_current — the layer argument is optional (#255)
// ============================================================
static void test_clearLayer_current()
{
    printf("--- clearLayer current ---\n");

    LayerBindingFixture f;

    f.exec("gfx.setLayer(1); gfx.setPixel(0, 0, 7)");
    f.exec("gfx.setLayer(2); gfx.setPixel(0, 0, 3)");

    // No layer argument: clears the current layer (2) only
    LuaResult r = f.exec("gfx.clearLayer()");
    ASSERT(r.success, "gfx.clearLayer() without a layer should succeed");
    ASSERT(f.compositor.layers[0].getPixel(0, 0).value == 7,
           "clearLayer(): layer 1 pixel (0,0) should still be 7 (untouched)");
    ASSERT(f.compositor.layers[1].getPixel(0, 0).value == 0,
           "clearLayer(): current layer 2 pixel (0,0) should be cleared to 0");
}

// ============================================================
// test_getLayerCount
// ============================================================
static void test_getLayerCount()
{
    printf("--- getLayerCount ---\n");

    LayerBindingFixture f;
    LuaResult r = f.exec("result = gfx.getLayerCount()");
    ASSERT(r.success, "gfx.getLayerCount script should succeed");
    ASSERT(f.getNum("result") == 3.0,
           "getLayerCount: should return 3");
    ASSERT(f.exec("assert(gfx.LAYER_UI == nil and gfx.LAYER_DEBUG == nil)").success,
           "only the three applet constants are exposed");
}

// ============================================================
// test_setLayerVisible_isLayerVisible
// ============================================================
static void test_setLayerVisible_isLayerVisible()
{
    printf("--- setLayerVisible/isLayerVisible ---\n");

    LayerBindingFixture f;

    // All layers start visible
    LuaResult r1 = f.exec("result = gfx.isLayerVisible(2) and 1 or 0");
    ASSERT(r1.success, "gfx.isLayerVisible(2) initial check should succeed");
    ASSERT(f.getNum("result") == 1.0,
           "visibility: layer 2 should start visible");

    // Hide layer 2
    LuaResult r2 = f.exec("gfx.setLayerVisible(2, false); result = gfx.isLayerVisible(2) and 1 or 0");
    ASSERT(r2.success, "gfx.setLayerVisible(2, false) should succeed");
    ASSERT(f.getNum("result") == 0.0,
           "visibility: layer 2 should be hidden after gfx.setLayerVisible(2, false)");

    // Other layers still visible
    LuaResult r3 = f.exec("result = gfx.isLayerVisible(1) and 1 or 0");
    ASSERT(r3.success, "gfx.isLayerVisible(1) should succeed");
    ASSERT(f.getNum("result") == 1.0,
           "visibility: layer 1 should still be visible");

    // Verify compositor visibility array matches
    ASSERT(f.compositor.visible[1] == false,
           "visibility: compositor.visible[1] should be false (layer 2 in Lua = index 1 in C++)");
    ASSERT(f.compositor.visible[0] == true,
           "visibility: compositor.visible[0] should be true (layer 1)");
}

// ============================================================
// main
// ============================================================
int main()
{
    printf("layer_binding_test\n");
    printf("==================\n");

    test_setLayer_getLayer_roundtrip();
    test_setLayer_reject_low();
    test_setLayer_reject_high();
    test_clearLayer_specific();
    test_clearLayer_current();
    test_getLayerCount();
    test_setLayerVisible_isLayerVisible();

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
