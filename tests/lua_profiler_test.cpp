/**
 * @file lua_profiler_test.cpp
 * @brief Unit tests for LuaProfiler singleton (Phase 63: PROF-01, PROF-02, PROF-03, PROF-06)
 *
 * Tests:
 *   - hook install/uninstall lifecycle and active flag
 *   - zero-overhead disabled path via lua_sethook(L, NULL, 0, 0) (PROF-03)
 *   - per-function call count accuracy (PROF-01)
 *   - GC memory query returns non-zero after Lua init (PROF-02 foundation)
 *   - null-safety of all engine.* subtable calls in headless mode (PROF-06)
 *   - sortByCount puts highest call count first
 */
#include <enjin2/scripting/lua_profiler.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <cstdio>

using namespace enjin2;

static int passes   = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// test_hook_install_uninstall
// PROF-01: install() sets active=true; Lua code increments entryCount;
//          uninstall() sets active=false.
// ============================================================
static void test_hook_install_uninstall() {
    printf("--- hook install/uninstall ---\n");

    LuaEngine eng;
    eng.initialize();
    lua_State* L = eng.getState();

    LuaProfiler& p = LuaProfiler::get();
    p.reset();

    // Before install: active should be false
    ASSERT(!p.active, "profiler inactive before install");

    p.install(L);
    ASSERT(p.active, "profiler active after install");

    // Execute a function — should record at least one call
    eng.executeString("function hello() end hello()");
    ASSERT(p.entryCount > 0, "entryCount > 0 after Lua function call");

    p.uninstall(L);
    ASSERT(!p.active, "profiler inactive after uninstall");

    eng.shutdown();
}

// ============================================================
// test_zero_overhead_disabled
// PROF-03: lua_sethook(L, NULL, 0, 0) is the disabled path.
//          Executing Lua code leaves entryCount == 0.
// ============================================================
static void test_zero_overhead_disabled() {
    printf("--- zero overhead disabled (PROF-03) ---\n");

    LuaEngine eng;
    eng.initialize();
    lua_State* L = eng.getState();

    // Explicitly disable hook (PROF-03 zero-overhead path)
    lua_sethook(L, NULL, 0, 0);

    LuaProfiler& p = LuaProfiler::get();
    p.reset();  // clears entryCount, does NOT install hook

    // Run Lua code — hook should not fire
    eng.executeString("function x() end x() x() x()");
    ASSERT(p.entryCount == 0, "entryCount == 0 when hook disabled (PROF-03)");

    eng.shutdown();
}

// ============================================================
// test_call_count_accuracy
// PROF-01: calling function f() five times produces callCount >= 5.
// ============================================================
static void test_call_count_accuracy() {
    printf("--- call count accuracy (PROF-01) ---\n");

    LuaEngine eng;
    eng.initialize();
    lua_State* L = eng.getState();

    LuaProfiler& p = LuaProfiler::get();
    p.reset();
    p.install(L);

    // Call f() exactly 5 times
    eng.executeString("function f() end f() f() f() f() f()");

    // Find the entry for "f"
    bool found = false;
    for (int i = 0; i < p.entryCount; ++i) {
        if (strcmp(p.entries[i].name, "f") == 0) {
            ASSERT(p.entries[i].callCount >= 5,
                   "callCount >= 5 for function called 5 times");
            found = true;
            break;
        }
    }
    ASSERT(found, "entry for function 'f' found in profiler table");

    p.uninstall(L);
    eng.shutdown();
}

// ============================================================
// test_gc_memory_query
// PROF-02 foundation: lua_gc(L, LUA_GCCOUNT, 0) returns non-zero after init.
// ============================================================
static void test_gc_memory_query() {
    printf("--- GC memory query (PROF-02) ---\n");

    LuaEngine eng;
    eng.initialize();
    lua_State* L = eng.getState();

    // Lua always uses some memory after initialization
    int kb    = lua_gc(L, LUA_GCCOUNT,  0);
    int bytes = lua_gc(L, LUA_GCCOUNTB, 0);
    int total = kb * 1024 + bytes;
    ASSERT(total > 0, "GC memory usage is non-zero after Lua init (PROF-02)");

    eng.shutdown();
}

// ============================================================
// test_null_safety
// PROF-06: Scripts calling all engine.* subtables run without crash
//          when input, scene, and SSM are not wired.
//
// Canvas must be wired (setLayers + setDebugCanvas) to avoid null
// dereference in bindings_draw.cpp (no null guard on currentCanvas).
// All other engine.* subtables null-guard safely per the null-safety map.
// ============================================================
static void test_null_safety() {
    printf("--- null safety engine.* headless (PROF-06) ---\n");

    // Static compositor and canvas wrappers — zero heap allocation
    static LayerCompositor<128, 128> compositor;
    static LuaCanvas lua_layer0(&compositor.layers[0]);
    static LuaCanvas lua_layer1(&compositor.layers[1]);
    static LuaCanvas lua_layer2(&compositor.layers[2]);
    static LuaCanvas lua_layer3(&compositor.layers[3]);
    static LuaCanvas lua_layer_debug(&compositor.layers[4]);
    static LuaCanvas* lua_layers[4] = {
        &lua_layer0, &lua_layer1, &lua_layer2, &lua_layer3
    };

    LuaScriptSystem sys;
    bool ok = sys.initialize();
    ASSERT(ok, "LuaScriptSystem initializes successfully");
    if (!ok) return;

    // Wire layers (required — currentCanvas must not be null in bindings_draw.cpp)
    sys.getBindings().setLayers(lua_layers, 4, compositor.visible);
    sys.getBindings().setDebugCanvas(&lua_layer_debug);

    // Wire time state so engine.time.* return valid values
    sys.getBindings().setTimeState(1.0f / 60.0f, 0.016f, 1u);

    // NOTE: setInput() intentionally NOT called — currentInput remains nullptr.
    //       All engine.input.* bindings null-guard safely.
    // NOTE: setActiveScene() / setSceneStateMachine() NOT called.
    //       All engine.scene.* bindings null-guard safely.

    // Execute script exercising all engine.* subtables
    LuaResult r = sys.getEngine().executeString(
        "local dt = engine.time.delta()\n"
        "local now = engine.time.now()\n"
        "local mem = engine.lua.memory()\n"
        "engine.log('null safety test')\n"
        "local held = engine.input.held(0)\n"
        "local found = engine.scene.find('x')\n"
    );
    ASSERT(r.success, "engine.* subtable calls do not crash in headless mode (PROF-06)");

    sys.shutdown();
}

// ============================================================
// test_sort_by_count
// sortByCount() puts the highest call count first.
// ============================================================
static void test_sort_by_count() {
    printf("--- sort by count ---\n");

    LuaEngine eng;
    eng.initialize();
    lua_State* L = eng.getState();

    LuaProfiler& p = LuaProfiler::get();
    p.reset();
    p.install(L);

    // Call a() once, b() three times — after sort b should come first
    eng.executeString(
        "function a() end\n"
        "function b() end\n"
        "a()\n"
        "b() b() b()\n"
    );

    ASSERT(p.entryCount >= 2, "at least two entries after defining a() and b()");

    p.sortByCount();

    // After sort, entries[0] must have the highest call count
    if (p.entryCount >= 2) {
        ASSERT(p.entries[0].callCount >= p.entries[1].callCount,
               "entries[0].callCount >= entries[1].callCount after sortByCount");
    }

    p.uninstall(L);
    eng.shutdown();
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== lua_profiler_test ===\n");

    test_hook_install_uninstall();
    test_zero_overhead_disabled();
    test_call_count_accuracy();
    test_gc_memory_query();
    test_null_safety();
    test_sort_by_count();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
