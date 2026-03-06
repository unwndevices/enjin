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

// Dummy sprite data: 4x4 pixels, 1 frame (all zeros)
static uint8_t g_dummyPixels[16] = {0};

// ============================================================
// Test fixture: LuaEngine + LuaBindings + 4 Canvas4 layers
// Reuses the same pattern as layer_binding_test.cpp
// ============================================================
struct HotReloadFixture {
    LayerCompositor<16, 16> compositor;
    LuaEngine engine;
    LuaBindings bindings;

    LuaCanvas layer0;
    LuaCanvas layer1;
    LuaCanvas layer2;
    LuaCanvas layer3;

    LuaCanvas* layerPtrs[4];

    HotReloadFixture()
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

        // Expose dummy sprite data as lightuserdata for Lua sprite tests
        lua_pushlightuserdata(engine.getState(), g_dummyPixels);
        lua_setglobal(engine.getState(), "testSpriteData");
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }
};

// ============================================================
// test_resetSpritePool_clears_all_slots
// After creating sprites, resetSpritePool() should free all
// slots so newSprite allocates from slot 0 again.
// ============================================================
static void test_resetSpritePool_clears_all_slots()
{
    printf("--- resetSpritePool clears all slots ---\n");

    HotReloadFixture f;

    // Allocate first sprite — should get slot 0
    LuaResult r1 = f.exec("handle1 = gfx.newSprite(testSpriteData, 4, 4, 1, 1)");
    ASSERT(r1.success, "newSprite should succeed");
    ASSERT(f.getNum("handle1") == 0.0,
           "first newSprite should return handle 0");

    // Allocate second sprite — should get slot 1
    LuaResult r2 = f.exec("handle2 = gfx.newSprite(testSpriteData, 4, 4, 1, 1)");
    ASSERT(r2.success, "second newSprite should succeed");
    ASSERT(f.getNum("handle2") == 1.0,
           "second newSprite should return handle 1");

    // Reset the pool
    f.bindings.resetSpritePool();

    // Allocate again — should get slot 0 (pool was cleared)
    LuaResult r3 = f.exec("handle3 = gfx.newSprite(testSpriteData, 4, 4, 1, 1)");
    ASSERT(r3.success, "newSprite after reset should succeed");
    ASSERT(f.getNum("handle3") == 0.0,
           "after resetSpritePool, newSprite should return handle 0 again");
}

// ============================================================
// test_registerAll_resets_drawing_state
// After changing color and lineWidth, registerAll() should
// reset them to defaults (color=15, lineWidth=1).
// ============================================================
static void test_registerAll_resets_drawing_state()
{
    printf("--- registerAll resets drawing state ---\n");

    HotReloadFixture f;

    // Change drawing state from defaults
    f.exec("gfx.setColor(5)");
    f.exec("gfx.setLineWidth(3)");

    // Verify state was changed
    LuaResult rc = f.exec("c = gfx.getColor(); w = gfx.getLineWidth()");
    ASSERT(rc.success, "gfx.getColor/gfx.getLineWidth should succeed");
    ASSERT(f.getNum("c") == 5.0, "color should be 5 after setColor(5)");
    ASSERT(f.getNum("w") == 3.0, "lineWidth should be 3 after setLineWidth(3)");

    // Re-register all bindings (simulates what happens on reload)
    f.bindings.registerAll();
    // Re-wire layers since registerAll re-registers the bindings pointer
    f.bindings.setLayers(f.layerPtrs, 4, f.compositor.visible);

    // Verify defaults are restored
    LuaResult rd = f.exec("c2 = gfx.getColor(); w2 = gfx.getLineWidth()");
    ASSERT(rd.success, "gfx.getColor/gfx.getLineWidth after registerAll should succeed");
    ASSERT(f.getNum("c2") == 15.0,
           "after registerAll, color should be reset to 15");
    ASSERT(f.getNum("w2") == 1.0,
           "after registerAll, lineWidth should be reset to 1");
}

// ============================================================
// test_registerAll_resets_sprite_pool
// registerAll() calls resetSpritePool() internally, so
// sprites created before should be gone after re-registration.
// ============================================================
static void test_registerAll_resets_sprite_pool()
{
    printf("--- registerAll resets sprite pool ---\n");

    HotReloadFixture f;

    // Allocate 3 sprites
    f.exec("gfx.newSprite(testSpriteData, 4, 4, 1, 1)");
    f.exec("gfx.newSprite(testSpriteData, 4, 4, 1, 1)");
    f.exec("gfx.newSprite(testSpriteData, 4, 4, 1, 1)");

    // Next allocation should be slot 3
    f.exec("pre_handle = gfx.newSprite(testSpriteData, 4, 4, 1, 1)");
    ASSERT(f.getNum("pre_handle") == 3.0,
           "4th sprite should get handle 3");

    // Re-register (which calls resetSpritePool internally)
    f.bindings.registerAll();
    f.bindings.setLayers(f.layerPtrs, 4, f.compositor.visible);

    // Re-expose lightuserdata (registerAll creates fresh Lua globals)
    lua_pushlightuserdata(f.engine.getState(), g_dummyPixels);
    lua_setglobal(f.engine.getState(), "testSpriteData");

    // First allocation after registerAll should be slot 0 again
    f.exec("post_handle = gfx.newSprite(testSpriteData, 4, 4, 1, 1)");
    ASSERT(f.getNum("post_handle") == 0.0,
           "after registerAll, first newSprite should return handle 0");
}

// ============================================================
// test_reload_cycle_preserves_bindings
// Full shutdown+initialize+registerAll+setLayers cycle
// (simulating performReload), then verify bindings still work.
// ============================================================
static void test_reload_cycle_preserves_bindings()
{
    printf("--- reload cycle preserves bindings ---\n");

    HotReloadFixture f;

    // Draw on layer 2 before reload
    f.exec("gfx.setLayer(2); gfx.setColor(7); gfx.setPixel(5, 5, 7)");
    uint8_t pre = f.compositor.layers[1].getPixel(5, 5).value;
    ASSERT(pre == 7, "pre-reload: pixel (5,5) on layer 2 should be 7");

    // --- Simulate performReload() ---
    f.engine.shutdown();
    ASSERT(f.engine.initialize(), "re-initialize should succeed");
    f.bindings.registerAll();
    f.bindings.setLayers(f.layerPtrs, 4, f.compositor.visible);

    // Clear canvas for fresh start (like performReload does)
    f.compositor.clearAll();

    // Verify bindings still work after reload cycle
    LuaResult r1 = f.exec("gfx.setLayer(3); gfx.setColor(9); gfx.setPixel(7, 7, 9)");
    ASSERT(r1.success, "post-reload gfx.setLayer/gfx.setPixel should succeed");

    uint8_t post = f.compositor.layers[2].getPixel(7, 7).value;
    ASSERT(post == 9,
           "post-reload: pixel (7,7) on layer 3 should be 9");

    // Verify gfx.getLayer reports correct layer
    f.exec("result = gfx.getLayer()");
    ASSERT(f.getNum("result") == 3.0,
           "post-reload: gfx.getLayer should return 3");

    // Verify gfx.getLayerCount still works
    f.exec("count = gfx.getLayerCount()");
    ASSERT(f.getNum("count") == 4.0,
           "post-reload: gfx.getLayerCount should return 4");
}

// ============================================================
// test_script_error_returns_failure
// Executing a script with error() should return success=false
// with the error message intact.
// ============================================================
static void test_script_error_returns_failure()
{
    printf("--- script error returns failure ---\n");

    HotReloadFixture f;

    // Define a function that will error
    f.exec("function explode() error('boom') end");

    // Call the function — should fail gracefully
    LuaResult r = f.engine.callFunction("explode");
    ASSERT(!r.success,
           "callFunction('explode') should return success=false");
    ASSERT(r.error.find("boom") != std::string::npos,
           "error message should contain 'boom'");

    // Verify engine is still alive — we can still run scripts
    LuaResult r2 = f.exec("alive = 42");
    ASSERT(r2.success, "engine should still work after caught error");
    ASSERT(f.getNum("alive") == 42.0,
           "post-error: Lua globals should still be accessible");
}

// ============================================================
// main
// ============================================================
int main()
{
    printf("hot_reload_test\n");
    printf("===============\n");

    test_resetSpritePool_clears_all_slots();
    test_registerAll_resets_drawing_state();
    test_registerAll_resets_sprite_pool();
    test_reload_cycle_preserves_bindings();
    test_script_error_returns_failure();

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
