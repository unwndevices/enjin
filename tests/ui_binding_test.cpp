/**
 * @file ui_binding_test.cpp
 * @brief Lua integration test for engine.ui.* sub-table (Phase 52: UI-01..UI-04)
 *
 * Verifies:
 * - engine.ui sub-table exists and has all four functions
 * - All four functions callable without crash when canvas is null (null-canvas safety)
 * - progressBar draws correct fill pixels (pixel-level verification)
 * - statBar boundary values (0/0, 0/max, max/max, half)
 * - progressBar value clamping (< 0, > 1)
 * - panel draws background then border (pixel-level: interior bg, edge border)
 * - label callable without crash
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
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
    } while (0)

// ============================================================
// Fixture without canvas — tests null-canvas safety
// ============================================================
struct UIFixture {
    LuaEngine  engine;
    LuaBindings bindings;

    UIFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
        // Intentionally do NOT call setCanvas() — tests null-canvas safety
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* name)  { return engine.getGlobalNumber(name); }
};

// ============================================================
// Fixture with canvas — pixel-level draw verification
// ============================================================
struct UICanvasFixture {
    LuaEngine   engine;
    LuaBindings bindings;
    Canvas4<128, 128> canvas;
    LuaCanvas luaCanvas;

    UICanvasFixture() : bindings(&engine), luaCanvas(&canvas) {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);
        canvas.clear(Pixel4(0));  // clear to palette index 0
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* name)  { return engine.getGlobalNumber(name); }
};

// ============================================================
// Test 1: engine.ui table exists and has all four functions
// ============================================================
static void test_ui_table_exists() {
    printf("--- Test 1: engine.ui table exists and has all functions ---\n");

    UIFixture f;

    LuaResult r = f.exec(
        "ok_table        = (type(engine.ui) == 'table') and 1 or 0\n"
        "ok_progressBar  = (type(engine.ui.progressBar) == 'function') and 1 or 0\n"
        "ok_statBar      = (type(engine.ui.statBar) == 'function') and 1 or 0\n"
        "ok_panel        = (type(engine.ui.panel) == 'function') and 1 or 0\n"
        "ok_label        = (type(engine.ui.label) == 'function') and 1 or 0\n"
    );

    ASSERT(r.success,                          "engine.ui table checks must not error");
    ASSERT(f.getNum("ok_table")       == 1.0, "engine.ui must be a table");
    ASSERT(f.getNum("ok_progressBar") == 1.0, "engine.ui.progressBar must be a function");
    ASSERT(f.getNum("ok_statBar")     == 1.0, "engine.ui.statBar must be a function");
    ASSERT(f.getNum("ok_panel")       == 1.0, "engine.ui.panel must be a function");
    ASSERT(f.getNum("ok_label")       == 1.0, "engine.ui.label must be a function");
}

// ============================================================
// Test 2: All four functions callable without crash when canvas is null
// ============================================================
static void test_null_canvas_safety() {
    printf("--- Test 2: null canvas safety — all four functions early-return ---\n");

    UIFixture f;

    LuaResult r1 = f.exec("engine.ui.progressBar(0, 0, 10, 4, 0.5, 7, 0)");
    ASSERT(r1.success, "progressBar must not crash with null canvas");

    LuaResult r2 = f.exec("engine.ui.statBar(0, 0, 10, 4, 5, 10, 7, 0)");
    ASSERT(r2.success, "statBar must not crash with null canvas");

    LuaResult r3 = f.exec("engine.ui.panel(0, 0, 10, 10, 3, 7)");
    ASSERT(r3.success, "panel must not crash with null canvas");

    LuaResult r4 = f.exec("engine.ui.label(0, 0, 'hello', 5)");
    ASSERT(r4.success, "label must not crash with null canvas");
}

// ============================================================
// Test 3: progressBar pixel fill — value=0.5 on 10-wide bar
// fillW = floor(10 * 0.5) = 5 → pixels 0..4 are fg=7, 5..9 are bg=0
// ============================================================
static void test_progressBar_pixel_fill() {
    printf("--- Test 3: progressBar pixel-level fill verification ---\n");

    UICanvasFixture f;

    // Draw progressBar: w=10, h=4, value=0.5, fg=7, bg=0
    LuaResult r = f.exec("engine.ui.progressBar(0, 0, 10, 4, 0.5, 7, 0)");
    ASSERT(r.success, "progressBar draw call must succeed");

    // fillW = int(10 * 0.5) = 5 → pixels 0..4 are fg=7
    uint8_t p0 = f.canvas.getPixel(0, 0);
    uint8_t p4 = f.canvas.getPixel(4, 0);
    // pixels 5..9 are bg=0
    uint8_t p5 = f.canvas.getPixel(5, 0);
    uint8_t p9 = f.canvas.getPixel(9, 0);

    ASSERT(p0 == 7, "progressBar pixel(0,0) must be fg=7 (in filled half)");
    ASSERT(p4 == 7, "progressBar pixel(4,0) must be fg=7 (last filled pixel)");
    ASSERT(p5 == 0, "progressBar pixel(5,0) must be bg=0 (first unfilled pixel)");
    ASSERT(p9 == 0, "progressBar pixel(9,0) must be bg=0 (rightmost pixel)");
}

// ============================================================
// Test 4: statBar boundary values
// ============================================================
static void test_statBar_boundary_values() {
    printf("--- Test 4: statBar boundary values ---\n");

    UICanvasFixture f;

    // statBar: 0/0 — division by zero guard, no crash, fill=0 (all bg=2)
    f.canvas.clear(Pixel4(0));
    LuaResult r1 = f.exec("engine.ui.statBar(0, 0, 10, 1, 0, 0, 7, 2)");
    ASSERT(r1.success, "statBar(0,0) must not crash");
    ASSERT(f.canvas.getPixel(0, 0) == 2, "statBar 0/0: all bg=2");

    // statBar: current=0, max=10 — empty bar (all bg=2)
    f.canvas.clear(Pixel4(0));
    LuaResult r2 = f.exec("engine.ui.statBar(0, 0, 10, 1, 0, 10, 7, 2)");
    ASSERT(r2.success, "statBar(0,10) must succeed");
    ASSERT(f.canvas.getPixel(0, 0) == 2, "statBar 0/10: pixel(0,0) must be bg=2");
    ASSERT(f.canvas.getPixel(9, 0) == 2, "statBar 0/10: pixel(9,0) must be bg=2");

    // statBar: current=max=10 — full bar (all fg=7)
    f.canvas.clear(Pixel4(0));
    LuaResult r3 = f.exec("engine.ui.statBar(0, 0, 10, 1, 10, 10, 7, 2)");
    ASSERT(r3.success, "statBar(10,10) must succeed");
    ASSERT(f.canvas.getPixel(0, 0) == 7, "statBar 10/10: pixel(0,0) must be fg=7");
    ASSERT(f.canvas.getPixel(9, 0) == 7, "statBar 10/10: pixel(9,0) must be fg=7");

    // statBar: current=5, max=10 — half fill
    // fillW = int(10 * 0.5) = 5 → pixel(0,0)=7, pixel(5,0)=2
    f.canvas.clear(Pixel4(0));
    LuaResult r4 = f.exec("engine.ui.statBar(0, 0, 10, 1, 5, 10, 7, 2)");
    ASSERT(r4.success, "statBar(5,10) must succeed");
    ASSERT(f.canvas.getPixel(0, 0) == 7, "statBar 5/10: pixel(0,0) must be fg=7");
    ASSERT(f.canvas.getPixel(5, 0) == 2, "statBar 5/10: pixel(5,0) must be bg=2");
}

// ============================================================
// Test 5: progressBar value clamping (< 0 and > 1)
// ============================================================
static void test_progressBar_value_clamping() {
    printf("--- Test 5: progressBar value clamping ---\n");

    UICanvasFixture f;

    // value=-1.0 → clamped to 0 → fillW=0 → all bg=0
    f.canvas.clear(Pixel4(1));  // clear to 1 so we can distinguish
    LuaResult r1 = f.exec("engine.ui.progressBar(0, 0, 10, 1, -1.0, 7, 0)");
    ASSERT(r1.success, "progressBar with negative value must succeed");
    // bg=0 should cover full bar
    ASSERT(f.canvas.getPixel(0, 0) == 0, "progressBar value<0: pixel(0,0) must be bg=0");
    ASSERT(f.canvas.getPixel(9, 0) == 0, "progressBar value<0: pixel(9,0) must be bg=0");

    // value=2.0 → clamped to 1 → fillW=10 → all fg=7
    f.canvas.clear(Pixel4(0));
    LuaResult r2 = f.exec("engine.ui.progressBar(0, 0, 10, 1, 2.0, 7, 0)");
    ASSERT(r2.success, "progressBar with value > 1 must succeed");
    ASSERT(f.canvas.getPixel(0, 0) == 7, "progressBar value>1: pixel(0,0) must be fg=7");
    ASSERT(f.canvas.getPixel(9, 0) == 7, "progressBar value>1: pixel(9,0) must be fg=7");
}

// ============================================================
// Test 6: panel pixel verification — bg=3, border=7
// ============================================================
static void test_panel_pixel_verification() {
    printf("--- Test 6: panel pixel-level verification ---\n");

    UICanvasFixture f;

    // panel: x=0, y=0, w=10, h=10, bg=3, border=7
    LuaResult r = f.exec("engine.ui.panel(0, 0, 10, 10, 3, 7)");
    ASSERT(r.success, "panel draw call must succeed");

    // Interior pixel (5,5) — fillRect covers all, drawRect only edges
    // Interior should be bg=3
    uint8_t interior = f.canvas.getPixel(5, 5);
    ASSERT(interior == 3, "panel interior pixel(5,5) must be bg=3");

    // Edge pixels — drawRect overwrites the edge pixels with border=7
    uint8_t edge_tl = f.canvas.getPixel(0, 0);
    uint8_t edge_tr = f.canvas.getPixel(9, 0);
    ASSERT(edge_tl == 7, "panel edge pixel(0,0) must be border=7");
    ASSERT(edge_tr == 7, "panel edge pixel(9,0) must be border=7");
}

// ============================================================
// Test 7: label callable without crash
// ============================================================
static void test_label_callable() {
    printf("--- Test 7: label callable with string argument ---\n");

    UIFixture f;

    LuaResult r = f.exec("engine.ui.label(10, 10, 'hello', 5)");
    ASSERT(r.success, "engine.ui.label must not crash (null canvas safety)");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== ui_binding_test (Phase 52: UI-01..UI-04) ===\n");

    test_ui_table_exists();
    test_null_canvas_safety();
    test_progressBar_pixel_fill();
    test_statBar_boundary_values();
    test_progressBar_value_clamping();
    test_panel_pixel_verification();
    test_label_callable();

    printf("\n%d/%d passed\n", passes, passes + failures);
    return failures ? 1 : 0;
}
