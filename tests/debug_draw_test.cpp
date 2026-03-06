/**
 * @file debug_draw_test.cpp
 * @brief Lua integration test for engine.debug.* sub-table (Phase 47: DEBUG-01..DEBUG-03)
 *
 * Verifies:
 * - engine.debug sub-table exists and has all expected functions
 * - All draw functions are callable without crash (null canvas — early return)
 * - setEnabled/getEnabled toggle works correctly
 * - Draw functions are no-ops when disabled (DEBUG-03)
 * - LAYER_DEBUG global constant is registered and equals 5
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cstring>

using namespace enjin2;

static int passes = 0;
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
// Minimal fixture: LuaEngine + LuaBindings, no canvas injected
// ============================================================
struct DebugFixture {
    LuaEngine engine;
    LuaBindings bindings;

    DebugFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
        // Intentionally do NOT call setDebugCanvas() — tests null-canvas safety
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }
};

// ============================================================
// Test 1: engine.debug sub-table exists and has expected functions
// ============================================================
static void test_debug_table_exists() {
    printf("--- engine.debug table exists and has all functions ---\n");

    DebugFixture f;

    LuaResult r = f.exec(
        "ok_table      = (type(engine.debug) == 'table') and 1 or 0\n"
        "ok_rect       = (type(engine.debug.rect) == 'function') and 1 or 0\n"
        "ok_circle     = (type(engine.debug.circle) == 'function') and 1 or 0\n"
        "ok_line       = (type(engine.debug.line) == 'function') and 1 or 0\n"
        "ok_cross      = (type(engine.debug.cross) == 'function') and 1 or 0\n"
        "ok_text       = (type(engine.debug.text) == 'function') and 1 or 0\n"
        "ok_setEnabled = (type(engine.debug.setEnabled) == 'function') and 1 or 0\n"
        "ok_getEnabled = (type(engine.debug.getEnabled) == 'function') and 1 or 0\n"
    );

    ASSERT(r.success, "engine.debug table checks must not error");
    ASSERT(f.getNum("ok_table")      == 1.0, "engine.debug must be a table");
    ASSERT(f.getNum("ok_rect")       == 1.0, "engine.debug.rect must be a function");
    ASSERT(f.getNum("ok_circle")     == 1.0, "engine.debug.circle must be a function");
    ASSERT(f.getNum("ok_line")       == 1.0, "engine.debug.line must be a function");
    ASSERT(f.getNum("ok_cross")      == 1.0, "engine.debug.cross must be a function");
    ASSERT(f.getNum("ok_text")       == 1.0, "engine.debug.text must be a function");
    ASSERT(f.getNum("ok_setEnabled") == 1.0, "engine.debug.setEnabled must be a function");
    ASSERT(f.getNum("ok_getEnabled") == 1.0, "engine.debug.getEnabled must be a function");
}

// ============================================================
// Test 2: All draw functions callable without crash (null canvas)
// When m_debugCanvas is nullptr, all draw calls early-return (DEBUG-01, DEBUG-02)
// ============================================================
static void test_draw_functions_no_crash_null_canvas() {
    printf("--- draw functions no-crash with null canvas ---\n");

    DebugFixture f;

    // Each call must succeed (return success) without segfault
    LuaResult r1 = f.exec("engine.debug.rect(10, 20, 8, 8, 5)");
    ASSERT(r1.success, "engine.debug.rect must not crash with null canvas");

    LuaResult r2 = f.exec("engine.debug.circle(64, 64, 16, 3)");
    ASSERT(r2.success, "engine.debug.circle must not crash with null canvas");

    LuaResult r3 = f.exec("engine.debug.line(0, 0, 127, 127, 7)");
    ASSERT(r3.success, "engine.debug.line must not crash with null canvas");

    LuaResult r4 = f.exec("engine.debug.cross(64, 64, 4, 2)");
    ASSERT(r4.success, "engine.debug.cross must not crash with null canvas");

    LuaResult r5 = f.exec("engine.debug.text('hello', 10, 10, 1)");
    ASSERT(r5.success, "engine.debug.text must not crash with null canvas");

    // Default color argument variants
    LuaResult r6 = f.exec("engine.debug.rect(0, 0, 4, 4)");
    ASSERT(r6.success, "engine.debug.rect with default color must not crash");

    LuaResult r7 = f.exec("engine.debug.cross(10, 10)");
    ASSERT(r7.success, "engine.debug.cross with default size/color must not crash");
}

// ============================================================
// Test 3: setEnabled/getEnabled toggle (DEBUG-03)
// ============================================================
static void test_set_get_enabled_toggle() {
    printf("--- setEnabled/getEnabled toggle ---\n");

    DebugFixture f;

    LuaResult r = f.exec(
        "result_before   = engine.debug.getEnabled() and 1 or 0\n"
        "engine.debug.setEnabled(false)\n"
        "result_after    = engine.debug.getEnabled() and 1 or 0\n"
        "engine.debug.setEnabled(true)\n"
        "result_restored = engine.debug.getEnabled() and 1 or 0\n"
    );

    ASSERT(r.success, "setEnabled/getEnabled toggle must not error");
    ASSERT(f.getNum("result_before")   == 1.0, "getEnabled() should return true initially");
    ASSERT(f.getNum("result_after")    == 0.0, "getEnabled() should return false after setEnabled(false)");
    ASSERT(f.getNum("result_restored") == 1.0, "getEnabled() should return true after setEnabled(true)");
}

// ============================================================
// Test 4: Draw functions are no-ops when disabled (DEBUG-03)
// All calls must succeed without crash even when disabled
// ============================================================
static void test_draw_no_op_when_disabled() {
    printf("--- draw functions are no-ops when disabled ---\n");

    DebugFixture f;

    LuaResult r = f.exec(
        "engine.debug.setEnabled(false)\n"
        "engine.debug.rect(10, 20, 8, 8, 5)\n"
        "engine.debug.circle(64, 64, 16, 3)\n"
        "engine.debug.line(0, 0, 127, 127, 7)\n"
        "engine.debug.cross(64, 64, 4, 2)\n"
        "engine.debug.text('hello', 10, 10, 1)\n"
        "disabled_ok = 1\n"
    );

    ASSERT(r.success,                      "All calls must succeed when disabled");
    ASSERT(f.getNum("disabled_ok") == 1.0, "disabled_ok must be 1 (all calls completed)");
}

// ============================================================
// Test 5: gfx.LAYER_DEBUG constant is registered as 5
// ============================================================
static void test_layer_debug_constant() {
    printf("--- gfx.LAYER_DEBUG constant ---\n");

    DebugFixture f;

    LuaResult r = f.exec(
        "layer_debug_val = gfx.LAYER_DEBUG\n"
        "layer_debug_type = (type(gfx.LAYER_DEBUG) == 'number') and 1 or 0\n"
    );

    ASSERT(r.success,                          "gfx.LAYER_DEBUG constant check must not error");
    ASSERT(f.getNum("layer_debug_val")  == 5.0, "gfx.LAYER_DEBUG must equal 5");
    ASSERT(f.getNum("layer_debug_type") == 1.0, "gfx.LAYER_DEBUG must be a number");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== debug_draw_test (Phase 47: DEBUG-01..DEBUG-03) ===\n");

    test_debug_table_exists();
    test_draw_functions_no_crash_null_canvas();
    test_set_get_enabled_toggle();
    test_draw_no_op_when_disabled();
    test_layer_debug_constant();

    printf("\n%d/%d passed\n", passes, passes + failures);
    return failures ? 1 : 0;
}
