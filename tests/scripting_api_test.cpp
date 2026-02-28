/**
 * @file scripting_api_test.cpp
 * @brief Tests for 7 scripting API improvements (quick task 007)
 *
 * API-01: BTN.* global table (UP, DOWN, LEFT, RIGHT, A, B, START)
 * API-02: COLOR.* global table (BLACK..TRANSPARENT)
 * API-03: engine.graphics.* alias sub-table
 * API-04: Float-to-int rounding in drawing primitives (lround)
 * API-05: text(str, x, y, scale) optional 4th scale param
 * API-06: textCentered(str, y) and textAligned(str, x, y, align)
 * API-07: engine.config.resolution(), engine.state.* manager
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
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
// Fixture: LuaEngine + LuaBindings + 16x16 canvas
// ============================================================
struct Fixture {
    LuaEngine engine;
    LuaBindings bindings;
    Canvas4<16, 16> rawCanvas;
    LuaCanvas luaCanvas;

    Fixture() : bindings(&engine), luaCanvas(&rawCanvas) {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }

    bool getBool(const char* name) {
        return engine.getGlobalNumber(name) != 0.0;
    }
};

// ============================================================
// test_constants: BTN.* and COLOR.* global tables (API-01, API-02)
// ============================================================
static void test_constants() {
    printf("--- test_constants: BTN.* and COLOR.* ---\n");
    Fixture f;

    // BTN constants
    LuaResult r = f.exec(
        "btn_up    = BTN.UP\n"
        "btn_down  = BTN.DOWN\n"
        "btn_left  = BTN.LEFT\n"
        "btn_right = BTN.RIGHT\n"
        "btn_a     = BTN.A\n"
        "btn_b     = BTN.B\n"
        "btn_start = BTN.START\n"
    );
    ASSERT(r.success, "BTN table access did not error");
    ASSERT((int)f.getNum("btn_up")    == 0, "BTN.UP == 0");
    ASSERT((int)f.getNum("btn_down")  == 1, "BTN.DOWN == 1");
    ASSERT((int)f.getNum("btn_left")  == 2, "BTN.LEFT == 2");
    ASSERT((int)f.getNum("btn_right") == 3, "BTN.RIGHT == 3");
    ASSERT((int)f.getNum("btn_a")     == 4, "BTN.A == 4");
    ASSERT((int)f.getNum("btn_b")     == 5, "BTN.B == 5");
    ASSERT((int)f.getNum("btn_start") == 6, "BTN.START == 6");

    // COLOR constants
    r = f.exec(
        "c_black = COLOR.BLACK\n"
        "c_red   = COLOR.RED\n"
        "c_white = COLOR.WHITE\n"
        "c_green = COLOR.GREEN\n"
        "c_blue  = COLOR.BLUE\n"
        "c_trans = COLOR.TRANSPARENT\n"
    );
    ASSERT(r.success, "COLOR table access did not error");
    ASSERT((int)f.getNum("c_black") ==  0, "COLOR.BLACK == 0");
    ASSERT((int)f.getNum("c_red")   ==  8, "COLOR.RED == 8");
    ASSERT((int)f.getNum("c_white") ==  7, "COLOR.WHITE == 7");
    ASSERT((int)f.getNum("c_green") == 11, "COLOR.GREEN == 11");
    ASSERT((int)f.getNum("c_blue")  == 12, "COLOR.BLUE == 12");
    ASSERT((int)f.getNum("c_trans") == 15, "COLOR.TRANSPARENT == 15");
}

// ============================================================
// test_engine_graphics: engine.graphics.* alias sub-table (API-03)
// ============================================================
static void test_engine_graphics() {
    printf("--- test_engine_graphics: engine.graphics alias sub-table ---\n");
    Fixture f;

    LuaResult r = f.exec(
        "ok_circle    = (type(engine.graphics.circle)    == 'function') and 1 or 0\n"
        "ok_rect      = (type(engine.graphics.rectangle) == 'function') and 1 or 0\n"
        "ok_setColor  = (type(engine.graphics.setColor)  == 'function') and 1 or 0\n"
        "ok_line      = (type(engine.graphics.line)      == 'function') and 1 or 0\n"
        "ok_text      = (type(engine.graphics.text)      == 'function') and 1 or 0\n"
    );
    ASSERT(r.success, "engine.graphics access did not error");
    ASSERT((int)f.getNum("ok_circle")   == 1, "engine.graphics.circle is a function");
    ASSERT((int)f.getNum("ok_rect")     == 1, "engine.graphics.rectangle is a function");
    ASSERT((int)f.getNum("ok_setColor") == 1, "engine.graphics.setColor is a function");
    ASSERT((int)f.getNum("ok_line")     == 1, "engine.graphics.line is a function");
    ASSERT((int)f.getNum("ok_text")     == 1, "engine.graphics.text is a function");
}

// ============================================================
// test_float_coords: Float coords accepted without error (API-04)
// ============================================================
static void test_float_coords() {
    printf("--- test_float_coords: float coordinates accepted ---\n");
    Fixture f;

    LuaResult r = f.exec("circle('fill', 10.7, 20.3, 5)");
    ASSERT(r.success, "circle() accepts float coords");

    r = f.exec("line(1.5, 2.5, 10.9, 20.1)");
    ASSERT(r.success, "line() accepts float coords");

    r = f.exec("rectangle(10.6, 20.4, 4, 4)");
    ASSERT(r.success, "rectangle() accepts float coords");

    r = f.exec("point(5.7, 3.2)");
    ASSERT(r.success, "point() accepts float coords");
}

// ============================================================
// test_text_scale: text(str, x, y, scale) optional 4th param (API-05)
// ============================================================
static void test_text_scale() {
    printf("--- test_text_scale: optional scale param ---\n");
    Fixture f;

    // text with scale 2 should not error and should NOT change global textSize
    LuaResult r = f.exec(
        "text('hello', 0, 0, 2)\n"
        "size_after = getTextSize()\n"
    );
    ASSERT(r.success, "text('hello', 0, 0, 2) did not error");
    ASSERT((int)f.getNum("size_after") == 1, "text() with scale 2 does not change global textSize");

    // backward compat: text with 3 args still works
    r = f.exec("text('hi', 0, 0)");
    ASSERT(r.success, "text('hi', 0, 0) backward compat works");
}

// ============================================================
// test_text_centered_aligned: textCentered + textAligned (API-06)
// ============================================================
static void test_text_centered_aligned() {
    printf("--- test_text_centered_aligned: new text functions ---\n");
    Fixture f;

    LuaResult r = f.exec("textCentered('hi', 8)");
    ASSERT(r.success, "textCentered('hi', 8) did not error");

    r = f.exec("textAligned('hi', 0, 8, 'center')");
    ASSERT(r.success, "textAligned center did not error");

    r = f.exec("textAligned('hi', 15, 8, 'right')");
    ASSERT(r.success, "textAligned right did not error");

    r = f.exec("textAligned('hi', 0, 8, 'left')");
    ASSERT(r.success, "textAligned left (default) did not error");
}

// ============================================================
// test_engine_config_resolution: engine.config.resolution() (API-07a)
// ============================================================
static void test_engine_config_resolution() {
    printf("--- test_engine_config_resolution ---\n");
    Fixture f;  // canvas is 16x16

    LuaResult r = f.exec(
        "local w, h = engine.config.resolution()\n"
        "res_w = w\n"
        "res_h = h\n"
    );
    ASSERT(r.success, "engine.config.resolution() did not error");
    ASSERT((int)f.getNum("res_w") == 16, "resolution() width == 16");
    ASSERT((int)f.getNum("res_h") == 16, "resolution() height == 16");
}

// ============================================================
// test_engine_state: engine.state.* state machine (API-07b)
// ============================================================
static void test_engine_state() {
    printf("--- test_engine_state: state machine ---\n");
    Fixture f;

    // Initial state is "none"
    LuaResult r = f.exec(
        "initial_state = engine.state.current()\n"
    );
    ASSERT(r.success, "engine.state.current() did not error");
    // Check it's a string "none"
    r = f.exec("is_none = (engine.state.current() == 'none') and 1 or 0");
    ASSERT(r.success, "state current check did not error");
    ASSERT((int)f.getNum("is_none") == 1, "initial state is 'none'");

    // Switch to "play" changes current
    r = f.exec(
        "engine.state.switch('play')\n"
        "is_play = (engine.state.current() == 'play') and 1 or 0\n"
    );
    ASSERT(r.success, "engine.state.switch('play') did not error");
    ASSERT((int)f.getNum("is_play") == 1, "engine.state.current() == 'play' after switch");

    // on_enter callback fires on transition into "menu"
    r = f.exec(
        "enter_called = 0\n"
        "engine.state.on_enter('menu', function() enter_called = 1 end)\n"
        "engine.state.switch('menu')\n"
    );
    ASSERT(r.success, "on_enter + switch('menu') did not error");
    ASSERT((int)f.getNum("enter_called") == 1, "on_enter callback fires on switch into 'menu'");

    // on_exit callback fires when leaving state
    r = f.exec(
        "exit_called = 0\n"
        "engine.state.on_exit('menu', function() exit_called = 1 end)\n"
        "engine.state.switch('play')\n"  // leave 'menu' -> enter 'play'
    );
    ASSERT(r.success, "on_exit + switch did not error");
    ASSERT((int)f.getNum("exit_called") == 1, "on_exit callback fires when leaving 'menu'");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== scripting_api_test ===\n");

    test_constants();
    test_engine_graphics();
    test_float_coords();
    test_text_scale();
    test_text_centered_aligned();
    test_engine_config_resolution();
    test_engine_state();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
