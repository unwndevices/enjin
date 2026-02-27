/**
 * @file input_event_callback_test.cpp
 * @brief Unit tests for on_button_pressed/on_button_released Lua callbacks (INPUT-01, INPUT-02, INPUT-03)
 *
 * Tests are ordered to match requirement IDs:
 *   INPUT-01: on_button_pressed(self, btn) fires once on the press-edge frame
 *   INPUT-02: on_button_released(self, btn) fires once on the release-edge frame
 *   INPUT-03: input callbacks fire before update() in the same frame
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/input/input_state.hpp>
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
// INPUT-01: on_button_pressed fires once on the press-edge frame
// ============================================================
static void test_input01_on_button_pressed_fires_on_edge()
{
    printf("--- INPUT-01: on_button_pressed fires on press edge ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "INPUT-01: addComponent<C_LuaScript> should succeed");

    bool loaded = script->loadScript(
        "pressed_count = 0\n"
        "function on_button_pressed(self, btn)\n"
        "    if btn == 0 then pressed_count = pressed_count + 1 end\n"
        "end\n"
    );
    ASSERT(loaded, "INPUT-01: loadScript should succeed");

    // Frame 1: button 0 transitions released -> pressed (justPressed == true)
    InputState input{};
    input.prev_buttons = 0;
    input.buttons = static_cast<uint16_t>(1u << 0);
    script->setInput(&input);
    script->update(0.016f);

    double count = script->getScriptNumber("pressed_count");
    ASSERT(count == 1.0, "INPUT-01: pressed_count should be 1 after one press edge");

    // Frame 2: button 0 still held (justPressed == false — no edge)
    input.prev_buttons = static_cast<uint16_t>(1u << 0);
    input.buttons = static_cast<uint16_t>(1u << 0);
    script->setInput(&input);
    script->update(0.016f);

    count = script->getScriptNumber("pressed_count");
    ASSERT(count == 1.0, "INPUT-01: pressed_count should still be 1 while button held (no new edge)");
}

// ============================================================
// INPUT-02: on_button_released fires once on the release-edge frame
// ============================================================
static void test_input02_on_button_released_fires_on_edge()
{
    printf("--- INPUT-02: on_button_released fires on release edge ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "INPUT-02: addComponent<C_LuaScript> should succeed");

    bool loaded = script->loadScript(
        "released_count = 0\n"
        "function on_button_released(self, btn)\n"
        "    if btn == 0 then released_count = released_count + 1 end\n"
        "end\n"
    );
    ASSERT(loaded, "INPUT-02: loadScript should succeed");

    // Frame 1: button 0 transitions pressed -> released (justReleased == true)
    InputState input{};
    input.prev_buttons = static_cast<uint16_t>(1u << 0);
    input.buttons = 0;
    script->setInput(&input);
    script->update(0.016f);

    double count = script->getScriptNumber("released_count");
    ASSERT(count == 1.0, "INPUT-02: released_count should be 1 after one release edge");

    // Frame 2: button 0 still released (justReleased == false — no edge)
    input.prev_buttons = 0;
    input.buttons = 0;
    script->setInput(&input);
    script->update(0.016f);

    count = script->getScriptNumber("released_count");
    ASSERT(count == 1.0, "INPUT-02: released_count should still be 1 on subsequent no-edge frame");
}

// ============================================================
// INPUT-03: input callbacks fire before update() in the same frame
// ============================================================
static void test_input03_callbacks_fire_before_update()
{
    printf("--- INPUT-03: on_button_pressed fires before update() in the same frame ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "INPUT-03: addComponent<C_LuaScript> should succeed");

    // Script sets 'callback_order' in on_button_pressed and update().
    // If callbacks fire first, callback_order should be "callback_then_update".
    bool loaded = script->loadScript(
        "callback_order = 'none'\n"
        "function on_button_pressed(self, btn)\n"
        "    if btn == 0 then callback_order = 'callback' end\n"
        "end\n"
        "function update(self, dt)\n"
        "    if callback_order == 'callback' then\n"
        "        callback_order = 'callback_then_update'\n"
        "    end\n"
        "end\n"
    );
    ASSERT(loaded, "INPUT-03: loadScript should succeed");

    // Frame: button 0 press edge
    InputState input{};
    input.prev_buttons = 0;
    input.buttons = static_cast<uint16_t>(1u << 0);
    script->setInput(&input);
    script->update(0.016f);

    std::string order = script->getScriptString("callback_order");
    ASSERT(order == "callback_then_update",
           "INPUT-03: callback_order should be 'callback_then_update' — callback fires before update()");
}

// ============================================================
// Extra: multiple buttons pressed in the same frame each fire the callback
// ============================================================
static void test_input_multi_button_press()
{
    printf("--- EXTRA: multiple button edges in one frame each fire callback ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "EXTRA: addComponent should succeed");

    bool loaded = script->loadScript(
        "press_total = 0\n"
        "function on_button_pressed(self, btn)\n"
        "    press_total = press_total + 1\n"
        "end\n"
    );
    ASSERT(loaded, "EXTRA: loadScript should succeed");

    // Press buttons 0, 1, and 2 simultaneously in one frame
    InputState input{};
    input.prev_buttons = 0;
    input.buttons = static_cast<uint16_t>((1u << 0) | (1u << 1) | (1u << 2));
    script->setInput(&input);
    script->update(0.016f);

    double total = script->getScriptNumber("press_total");
    ASSERT(total == 3.0, "EXTRA: press_total should be 3 when 3 buttons pressed in one frame");
}

// ============================================================
// Extra: optional callbacks — script without them does not crash
// ============================================================
static void test_input_optional_callbacks_no_crash()
{
    printf("--- EXTRA: script without on_button_pressed/released does not crash ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "EXTRA: addComponent should succeed");

    bool loaded = script->loadScript(
        "-- No on_button_pressed or on_button_released defined\n"
        "function update(self, dt)\n"
        "    -- normal update\n"
        "end\n"
    );
    ASSERT(loaded, "EXTRA: loadScript should succeed");

    InputState input{};
    input.prev_buttons = 0;
    input.buttons = static_cast<uint16_t>(1u << 0);  // press edge
    script->setInput(&input);
    script->update(0.016f);  // must not crash

    ASSERT(!script->hasErrors(), "EXTRA: hasErrors() should be false — missing callbacks are not errors");
}

// ============================================================
// main
// ============================================================
int main()
{
    printf("input_event_callback_test\n");
    printf("=========================\n");

    test_input01_on_button_pressed_fires_on_edge();
    test_input02_on_button_released_fires_on_edge();
    test_input03_callbacks_fire_before_update();
    test_input_multi_button_press();
    test_input_optional_callbacks_no_crash();

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
