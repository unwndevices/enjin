/**
 * @file style_slot_lua_test.cpp
 * @brief Lua-surface tests for the style-slot layer (#37 / #19).
 *
 * Verifies the behaviour at the binding boundary:
 *  - setStyle overrides only the keys present; absent keys read the ROM default
 *  - the override sets the slot's mask bit; registerAll() clears it
 *  - setTheme swaps the base and clears every override
 *  - panel dispatches slot-vs-legacy by lua_type (string = slot, number = bg)
 *  - unknown slot / theme names raise a Lua error, not UB
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/palette.hpp>
#include <enjin2/ui/style.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); failures++; } \
        else { passes++; } \
    } while (0)

struct StyleFixture {
    LuaEngine   engine;
    LuaBindings bindings;
    Canvas4<64, 64> canvas;
    LuaCanvas   luaCanvas;

    StyleFixture() : bindings(&engine), luaCanvas(&canvas) {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);
        canvas.clear(Pixel4(0));
    }
    LuaResult exec(const char* code) { return engine.executeString(code); }
};

// setStyle overrides only present keys; absent keys inherit the ROM default.
static void test_setstyle_masks_present_keys() {
    printf("--- setStyle overlays present keys, inherits the rest ---\n");
    StyleFixture f;
    const Style& def = kDefaultStyles[(int)StyleSlot::Panel];

    LuaResult r = f.exec("engine.ui.setStyle('panel', { fill = 7, radius = 4 })");
    ASSERT(r.success, "setStyle must not error");

    const Style& got = f.bindings.resolveStyle(StyleSlot::Panel);
    ASSERT(got.fill.value == 7, "fill overridden to 7");
    ASSERT(got.radius == 4, "radius overridden to 4");
    ASSERT(got.border.value == def.border.value, "border inherits the ROM default");
    ASSERT(got.padding == def.padding, "padding inherits the ROM default");
}

// The override marks the slot's mask bit; registerAll() (a Lua reload) clears it.
static void test_reset_clears_mask() {
    printf("--- registerAll clears the override mask ---\n");
    StyleFixture f;
    f.exec("engine.ui.setStyle('panel', { fill = 9 })");
    ASSERT(f.bindings.resolveStyle(StyleSlot::Panel).fill.value == 9, "override active");

    f.bindings.registerAll();  // applet boundary — fresh Lua state
    const Style& def = kDefaultStyles[(int)StyleSlot::Panel];
    ASSERT(f.bindings.resolveStyle(StyleSlot::Panel).fill.value == def.fill.value,
           "after reload the ROM default resolves again");
}

// setTheme clears every override (a new base is a fresh start).
static void test_settheme_clears_overrides() {
    printf("--- setTheme clears overrides ---\n");
    StyleFixture f;
    f.exec("engine.ui.setStyle('sceneObject', { fill = 5 })");
    ASSERT(f.bindings.resolveStyle(StyleSlot::SceneObject).fill.value == 5, "override active");

    LuaResult r = f.exec("engine.ui.setTheme('default')");
    ASSERT(r.success, "setTheme('default') must not error");
    const Style& def = kDefaultStyles[(int)StyleSlot::SceneObject];
    ASSERT(f.bindings.resolveStyle(StyleSlot::SceneObject).fill.value == def.fill.value,
           "setTheme drops the override");
}

// panel dispatches by lua_type: a string is a slot, a number is legacy bg.
static void test_panel_dispatch() {
    printf("--- panel slot-vs-legacy dispatch ---\n");
    StyleFixture f;
    const Style& def = kDefaultStyles[(int)StyleSlot::Panel];

    // String → style slot: interior takes the resolved fill, edge the border.
    LuaResult r1 = f.exec("engine.ui.panel(2, 2, 10, 10, 'panel')");
    ASSERT(r1.success, "panel with slot name must not error");
    ASSERT(f.canvas.getPixel(6, 6).value == def.fill.value, "interior is the slot fill");
    ASSERT(f.canvas.getPixel(2, 2).value == def.border.value, "edge is the slot border");

    // Number → legacy explicit (bg, border).
    LuaResult r2 = f.exec("engine.ui.panel(20, 20, 10, 10, 8, 3)");
    ASSERT(r2.success, "panel with numeric bg must not error");
    ASSERT(f.canvas.getPixel(24, 24).value == 8, "interior is the legacy bg");
    ASSERT(f.canvas.getPixel(20, 20).value == 3, "edge is the legacy border");
}

// Unknown slot / theme names raise a Lua error rather than silently mis-drawing.
static void test_unknown_names_error() {
    printf("--- unknown slot / theme names error ---\n");
    StyleFixture f;
    ASSERT(!f.exec("engine.ui.setStyle('nope', {})").success, "unknown slot errors");
    ASSERT(!f.exec("engine.ui.setTheme('neon')").success, "unknown theme errors");
    ASSERT(!f.exec("engine.ui.panel(0,0,4,4,'nope')").success, "unknown panel slot errors");
}

int main() {
    test_setstyle_masks_present_keys();
    test_reset_clears_mask();
    test_settheme_clears_overrides();
    test_panel_dispatch();
    test_unknown_names_error();
    printf("\nstyle_slot_lua_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
