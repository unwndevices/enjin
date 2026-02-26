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
    LuaCanvas layer3;

    LuaCanvas* layerPtrs[4];

    LayerBindingFixture()
        : bindings(&engine)
        , layer0(&compositor.layers[0])
        , layer1(&compositor.layers[1])
        , layer2(&compositor.layers[2])
        , layer3(&compositor.layers[3])
    {
        layerPtrs[0] = &layer0;
        layerPtrs[1] = &layer1;
        layerPtrs[2] = &layer2;
        layerPtrs[3] = &layer3;

        engine.initialize();
        bindings.registerAll();
        bindings.setLayers(layerPtrs, 4, compositor.visible);
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
    LuaResult r = f.exec("setLayer(3); result = getLayer()");
    ASSERT(r.success, "setLayer/getLayer script should succeed");
    ASSERT(f.getNum("result") == 3.0,
           "roundtrip: getLayer() should return 3 after setLayer(3)");
}

// ============================================================
// test_setLayer_clamp_low
// ============================================================
static void test_setLayer_clamp_low()
{
    printf("--- setLayer clamp low ---\n");

    LayerBindingFixture f;
    LuaResult r = f.exec("setLayer(0); result = getLayer()");
    ASSERT(r.success, "setLayer(0) script should succeed");
    ASSERT(f.getNum("result") == 1.0,
           "clamp low: setLayer(0) should clamp to layer 1");
}

// ============================================================
// test_setLayer_clamp_high
// ============================================================
static void test_setLayer_clamp_high()
{
    printf("--- setLayer clamp high ---\n");

    LayerBindingFixture f;
    LuaResult r = f.exec("setLayer(99); result = getLayer()");
    ASSERT(r.success, "setLayer(99) script should succeed");
    ASSERT(f.getNum("result") == 4.0,
           "clamp high: setLayer(99) should clamp to layer 4");
}

// ============================================================
// test_clearLayer_specific
// ============================================================
static void test_clearLayer_specific()
{
    printf("--- clearLayer specific ---\n");

    LayerBindingFixture f;

    // Draw something on layer 1 (Lua index 1, cpp index 0)
    f.exec("setLayer(1); setPixel(0, 0, 7)");
    // Draw something on layer 2 (Lua index 2, cpp index 1)
    f.exec("setLayer(2); setPixel(0, 0, 3)");

    // Clear only layer 2 to color 5
    f.exec("clearLayer(2, 5)");

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
// test_getLayerCount
// ============================================================
static void test_getLayerCount()
{
    printf("--- getLayerCount ---\n");

    LayerBindingFixture f;
    LuaResult r = f.exec("result = getLayerCount()");
    ASSERT(r.success, "getLayerCount script should succeed");
    ASSERT(f.getNum("result") == 4.0,
           "getLayerCount: should return 4");
}

// ============================================================
// test_setLayerVisible_isLayerVisible
// ============================================================
static void test_setLayerVisible_isLayerVisible()
{
    printf("--- setLayerVisible/isLayerVisible ---\n");

    LayerBindingFixture f;

    // All layers start visible
    LuaResult r1 = f.exec("result = isLayerVisible(2) and 1 or 0");
    ASSERT(r1.success, "isLayerVisible(2) initial check should succeed");
    ASSERT(f.getNum("result") == 1.0,
           "visibility: layer 2 should start visible");

    // Hide layer 2
    LuaResult r2 = f.exec("setLayerVisible(2, false); result = isLayerVisible(2) and 1 or 0");
    ASSERT(r2.success, "setLayerVisible(2, false) should succeed");
    ASSERT(f.getNum("result") == 0.0,
           "visibility: layer 2 should be hidden after setLayerVisible(2, false)");

    // Other layers still visible
    LuaResult r3 = f.exec("result = isLayerVisible(1) and 1 or 0");
    ASSERT(r3.success, "isLayerVisible(1) should succeed");
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
    test_setLayer_clamp_low();
    test_setLayer_clamp_high();
    test_clearLayer_specific();
    test_getLayerCount();
    test_setLayerVisible_isLayerVisible();

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
