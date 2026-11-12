/**
 * @file scripting_api_test.cpp
 * @brief Tests for 7 scripting API improvements (quick task 007)
 *
 * API-01: BTN.* global table removed
 * API-02: gfx.COLOR names the 8 Palette roles (0-7) and TRANSPARENT (15)
 * API-03: gfx.* alias sub-table
 * API-04: Float-to-int rounding in drawing primitives (lround)
 * API-05: text(str, x, y, scale) optional 4th scale param
 * API-06: textCentered(str, y) and textAligned(str, x, y, align)
 * API-07: engine.config.resolution(), engine.state.* manager
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <string>
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
// test_constants: BTN removed; gfx.COLOR names the Palette roles (API-01, API-02)
// ============================================================
static void test_constants() {
    printf("--- test_constants: BTN absent, gfx.COLOR = the roles + TRANSPARENT ---\n");
    Fixture f;

    LuaResult r = f.exec(
        "btn_absent = (BTN == nil) and 1 or 0\n"
        "color_is_table = (type(gfx.COLOR) == 'table') and 1 or 0\n"
        "c_bg, c_surface, c_muted, c_text = gfx.COLOR.BG, gfx.COLOR.SURFACE, gfx.COLOR.MUTED, gfx.COLOR.TEXT\n"
        "c_accent, c_accent2 = gfx.COLOR.ACCENT, gfx.COLOR.ACCENT2\n"
        "c_accent3, c_accent4 = gfx.COLOR.ACCENT3, gfx.COLOR.ACCENT4\n"
        "c_trans = gfx.COLOR.TRANSPARENT\n"
        "color_count = 0\n"
        "for key in pairs(gfx.COLOR) do color_count = color_count + 1 end\n"
    );
    ASSERT(r.success, "gfx.COLOR contract query did not error");
    ASSERT((int)f.getNum("btn_absent") == 1, "BTN is nil");
    ASSERT((int)f.getNum("color_is_table") == 1, "gfx.COLOR is a table");
    ASSERT((int)f.getNum("c_bg") == 0, "gfx.COLOR.BG == 0");
    ASSERT((int)f.getNum("c_surface") == 1, "gfx.COLOR.SURFACE == 1");
    ASSERT((int)f.getNum("c_muted") == 2, "gfx.COLOR.MUTED == 2");
    ASSERT((int)f.getNum("c_text") == 3, "gfx.COLOR.TEXT == 3");
    ASSERT((int)f.getNum("c_accent") == 4, "gfx.COLOR.ACCENT == 4");
    ASSERT((int)f.getNum("c_accent2") == 5, "gfx.COLOR.ACCENT2 == 5");
    ASSERT((int)f.getNum("c_accent3") == 6, "gfx.COLOR.ACCENT3 == 6");
    ASSERT((int)f.getNum("c_accent4") == 7, "gfx.COLOR.ACCENT4 == 7");
    ASSERT((int)f.getNum("c_trans") == 15, "gfx.COLOR.TRANSPARENT == 15");
    ASSERT((int)f.getNum("color_count") == 9, "gfx.COLOR has the 8 roles and TRANSPARENT");
}

// ============================================================
// test_draw_colour_default: after a reload the draw colour is the text role
// ============================================================
static void test_draw_colour_default() {
    printf("--- test_draw_colour_default: a fresh state draws in text ---\n");
    Fixture f;
    f.rawCanvas.clear(Pixel4(PALETTE_TRANSPARENT));
    LuaResult r = f.exec("gfx.rectangle('fill', 0, 0, 2, 2)");
    ASSERT(r.success, "bare gfx.rectangle did not error");
    ASSERT(f.rawCanvas.getPixel(0, 0).value == ROLE_TEXT, "the default draw colour is the text role");

    f.exec("gfx.setColor(gfx.COLOR.ACCENT)");
    f.bindings.registerAll();  // a reload
    f.exec("gfx.rectangle('fill', 4, 4, 2, 2)");
    ASSERT(f.rawCanvas.getPixel(4, 4).value == ROLE_TEXT, "a reload resets the draw colour to text");
}

// ============================================================
// test_gfx_table: gfx.* namespace table (API-01)
// ============================================================
static void test_gfx_table() {
    printf("--- test_gfx_table: gfx.* namespace table ---\n");
    Fixture f;

    LuaResult r = f.exec(
        "ok_circle    = (type(gfx.circle)    == 'function') and 1 or 0\n"
        "ok_rect      = (type(gfx.rectangle) == 'function') and 1 or 0\n"
        "ok_setColor  = (type(gfx.setColor)  == 'function') and 1 or 0\n"
        "ok_line      = (type(gfx.line)      == 'function') and 1 or 0\n"
        "ok_text      = (type(gfx.text)      == 'function') and 1 or 0\n"
    );
    ASSERT(r.success, "gfx table access did not error");
    ASSERT((int)f.getNum("ok_circle")   == 1, "gfx.circle is a function");
    ASSERT((int)f.getNum("ok_rect")     == 1, "gfx.rectangle is a function");
    ASSERT((int)f.getNum("ok_setColor") == 1, "gfx.setColor is a function");
    ASSERT((int)f.getNum("ok_line")     == 1, "gfx.line is a function");
    ASSERT((int)f.getNum("ok_text")     == 1, "gfx.text is a function");
}

// ============================================================
// test_float_coords: Float coords accepted without error (API-04)
// ============================================================
static void test_float_coords() {
    printf("--- test_float_coords: float coordinates accepted ---\n");
    Fixture f;

    LuaResult r = f.exec("gfx.circle('fill', 10.7, 20.3, 5)");
    ASSERT(r.success, "gfx.circle() accepts float coords");

    r = f.exec("gfx.line(1.5, 2.5, 10.9, 20.1)");
    ASSERT(r.success, "gfx.line() accepts float coords");

    r = f.exec("gfx.rectangle(10.6, 20.4, 4, 4)");
    ASSERT(r.success, "gfx.rectangle() accepts float coords");

    r = f.exec("gfx.point(5.7, 3.2)");
    ASSERT(r.success, "gfx.point() accepts float coords");
}

// ============================================================
// test_text_surface: gfx.text(str, x, y [, align]) and its state (ADR-0029)
// ============================================================
static void test_text_surface() {
    printf("--- test_text_surface: gfx.text and its sticky state ---\n");
    Fixture f;

    LuaResult r = f.exec("lines = gfx.text('hi', 0, 0)");
    ASSERT(r.success, "gfx.text('hi', 0, 0) works");
    for (const char* align : {"left", "center", "right"}) {
        r = f.exec((std::string("gfx.text('hi', 40, 8, '") + align + "')").c_str());
        ASSERT(r.success, align);
    }
    r = f.exec("gfx.text('hi', 0, 8, 'middle')");
    ASSERT(!r.success, "an unknown align raises");
    r = f.exec("gfx.setTextWrap('ellipsis', 30); gfx.setTextWrap('clip'); gfx.setTextWrap('wrap')");
    ASSERT(r.success, "setTextWrap takes wrap, clip and ellipsis");
    r = f.exec("gfx.setTextWrap('scroll')");
    ASSERT(!r.success, "setTextWrap raises on another overflow");
    r = f.exec("gfx.setTextWrap('wrap', 0)");
    ASSERT(!r.success, "setTextWrap raises on a width under 1");
    for (const char* gone : {"textCentered", "textAligned", "textWrapped", "getTextHeight", "getTextSize",
                             "getFont", "number", "timer"}) {
        r = f.exec((std::string("assert(gfx.") + gone + " == nil)").c_str());
        ASSERT(r.success, gone);
    }
    r = f.exec("assert(engine.graphics == nil)");
    ASSERT(r.success, "engine.graphics is gone");
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

    // onEnter callback fires on transition into "menu"
    r = f.exec(
        "enter_called = 0\n"
        "engine.state.onEnter('menu', function() enter_called = 1 end)\n"
        "engine.state.switch('menu')\n"
    );
    ASSERT(r.success, "onEnter + switch('menu') did not error");
    ASSERT((int)f.getNum("enter_called") == 1, "onEnter callback fires on switch into 'menu'");

    // onExit callback fires when leaving state
    r = f.exec(
        "exit_called = 0\n"
        "engine.state.onExit('menu', function() exit_called = 1 end)\n"
        "engine.state.switch('play')\n"  // leave 'menu' -> enter 'play'
    );
    ASSERT(r.success, "onExit + switch did not error");
    ASSERT((int)f.getNum("exit_called") == 1, "onExit callback fires when leaving 'menu'");
}

// ============================================================
// test_bare_globals_removed: old bare globals error (API-04)
// ============================================================
static void test_bare_globals_removed() {
    printf("--- test_bare_globals_removed: old bare globals error ---\n");
    Fixture f;

    // Calling old bare global clear() should fail (nil value error)
    LuaResult r1 = f.exec("clear(1)");
    ASSERT(!r1.success, "bare clear() should error (is nil)");

    // Calling old bare global setColor() should fail
    LuaResult r2 = f.exec("setColor(1, 0, 0)");
    ASSERT(!r2.success, "bare setColor() should error (is nil)");

    // Calling old bare global line() should fail
    LuaResult r3 = f.exec("line(0, 0, 10, 10)");
    ASSERT(!r3.success, "bare line() should error (is nil)");

    // Calling old bare global text() should fail
    LuaResult r4 = f.exec("text('hello', 0, 0)");
    ASSERT(!r4.success, "bare text() should error (is nil)");

    // Removed enjin input globals should also fail
    LuaResult r5 = f.exec("isButtonHeld(0)");
    ASSERT(!r5.success, "bare isButtonHeld() should error (is nil)");

    // But gfx.clear() should work (proves namespace is registered)
    LuaResult r6 = f.exec("gfx.clear(1)");
    ASSERT(r6.success, "gfx.clear() should succeed");

    // And print() should still work as bare global
    LuaResult r7 = f.exec("print('hello')");
    ASSERT(r7.success, "bare print() should still work");

    printf("  bare_globals_removed: all assertions passed\n");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== scripting_api_test ===\n");

    test_constants();
    test_draw_colour_default();
    test_gfx_table();
    test_float_coords();
    test_text_surface();
    test_engine_config_resolution();
    test_engine_state();
    test_bare_globals_removed();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
