---
phase: quick-007
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - src/scripting/bindings.cpp
  - src/scripting/bindings_draw.cpp
  - src/scripting/bindings_layers_text.cpp
  - src/scripting/bindings_engine.cpp
  - include/enjin2/scripting/bindings.hpp
  - tests/scripting_api_test.cpp
  - tests/CMakeLists.txt
autonomous: true
requirements: [API-01, API-02, API-03, API-04, API-05, API-06, API-07]

must_haves:
  truths:
    - "Lua scripts can access BTN.UP, BTN.LEFT, COLOR.RED etc. without defining local constants"
    - "Drawing functions accept float coordinates and round them correctly instead of truncating"
    - "Drawing functions are available under engine.graphics.* namespace"
    - "text() accepts an optional 4th scale parameter"
    - "textCentered(str, y) draws text horizontally centered on the canvas"
    - "engine.config.resolution() returns the current canvas width and height"
    - "engine.state.switch(name) / engine.state.current() provide a lightweight global state machine"
  artifacts:
    - path: "src/scripting/bindings.cpp"
      provides: "BTN/COLOR constant tables, engine.graphics alias loop, engine.state registration"
      contains: "BTN"
    - path: "src/scripting/bindings_draw.cpp"
      provides: "Float-to-int rounding in drawing functions"
      contains: "lround"
    - path: "src/scripting/bindings_layers_text.cpp"
      provides: "text() optional scale param, textCentered(), textAligned()"
      contains: "lua_textCentered"
    - path: "src/scripting/bindings_engine.cpp"
      provides: "engine.config.resolution(), engine.state.* sub-tables"
      contains: "engine_config"
    - path: "tests/scripting_api_test.cpp"
      provides: "Automated tests for all 7 improvements"
      contains: "test_constants"
  key_links:
    - from: "src/scripting/bindings.cpp"
      to: "registerAll / registerEngineTable"
      via: "BTN/COLOR globals registered at init, engine.graphics aliased"
      pattern: "BTN.*UP"
    - from: "src/scripting/bindings_layers_text.cpp"
      to: "LuaCanvas::drawText"
      via: "textCentered calls measureTextWidth then drawText centered"
      pattern: "measureTextWidth.*drawText"
    - from: "src/scripting/bindings_engine.cpp"
      to: "engine.state Lua table"
      via: "Registry-stored state name, on_enter/on_exit callbacks"
      pattern: "enjin_game_state"
---

<objective>
Implement 7 scripting API improvements that reduce boilerplate and improve ergonomics for Lua game scripts.

Purpose: Every Enjin Lua script currently requires ~20 lines of constant boilerplate (BTN_LEFT=2, C_RED=8, etc.), manual math.floor() wrapping for float coords, stateful text sizing, and no built-in state machine. These 7 changes make the scripting API production-ready.

Output: Modified binding files with new APIs, comprehensive test file proving all 7 features work.
</objective>

<execution_context>
@/home/unwn/.claude/get-shit-done/workflows/execute-plan.md
@/home/unwn/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/scripting/bindings.cpp (registerAll function, lines 938-1094 — registration site for globals/tables)
@src/scripting/bindings_draw.cpp (drawing primitives — float casting targets)
@src/scripting/bindings_layers_text.cpp (text functions — text scale + centering targets)
@src/scripting/bindings_engine.cpp (registerEngineTable — new sub-tables go here)
@include/enjin2/scripting/bindings.hpp (LuaBindings class — new static method declarations)
@include/enjin2/scripting/bind_helpers.hpp (LuaFuncDef, luaBindFunctions, luaBindGlobals)
@tests/engine_table_test.cpp (test fixture pattern: LuaEngine + LuaBindings, ASSERT macro)
@tests/CMakeLists.txt (test registration pattern)

<interfaces>
<!-- Key types and contracts the executor needs. -->

From include/enjin2/scripting/bindings.hpp:
```cpp
class LuaBindings {
public:
    void registerAll();           // Registers all globals + engine.* table
    static LuaBindings* getBindings(lua_State* L);  // Registry lookup
    LuaCanvas* getCanvas() const { return currentCanvas; }
private:
    void registerEngineTable();   // Builds engine.* sub-tables
    // Text state:
    uint8_t currentTextSize{1};
    const GFXfont* currentFont{nullptr};
    // Canvas:
    LuaCanvas* currentCanvas;
    uint8_t currentColor;
};
```

From include/enjin2/scripting/bind_helpers.hpp:
```cpp
struct LuaFuncDef { const char* name; lua_CFunction func; };
void luaBindFunctions(lua_State* L, int tableIdx, const LuaFuncDef* defs, int n);
void luaBindGlobals(lua_State* L, const LuaFuncDef* defs, int n);
#define ENJIN_ARRAY_LEN(arr) (enjin2::luaArrayLen(arr))
```

From include/enjin2/input/input_state.hpp:
```cpp
// Button indices used by sprite_sdl_test.cpp input_platform_poll:
// 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT, 4=A(Z), 5=B(X), 6=START(Enter)
```

From scripts/arkanoid.lua (the boilerplate this eliminates):
```lua
local BTN_UP=0, BTN_DOWN=1, BTN_LEFT=2, BTN_RIGHT=3, BTN_Z=4, BTN_X=5, BTN_START=6
local C_BLACK=0, C_DARK_BLUE=1, ..., C_RED=8, ..., C_PINK=14
```

Test fixture pattern (from engine_table_test.cpp):
```cpp
struct Fixture {
    LuaEngine engine;
    LuaBindings bindings;
    Fixture() : bindings(&engine) { engine.initialize(); bindings.registerAll(); }
    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* name) { return engine.getGlobalNumber(name); }
};
```
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Built-in constants, engine.graphics namespace, and float-to-int rounding</name>
  <files>src/scripting/bindings.cpp, src/scripting/bindings_draw.cpp, src/scripting/bindings_engine.cpp, include/enjin2/scripting/bindings.hpp, tests/scripting_api_test.cpp, tests/CMakeLists.txt</files>
  <behavior>
    - Test: BTN.UP == 0, BTN.DOWN == 1, BTN.LEFT == 2, BTN.RIGHT == 3, BTN.A == 4, BTN.B == 5, BTN.START == 6
    - Test: COLOR.BLACK == 0, COLOR.RED == 8, COLOR.WHITE == 7, COLOR.GREEN == 11, COLOR.BLUE == 12
    - Test: engine.graphics.circle is a function (not nil)
    - Test: engine.graphics.rectangle is a function (not nil)
    - Test: engine.graphics.setColor is a function (not nil)
    - Test: engine.graphics.line is a function (not nil)
    - Test: engine.graphics.text is a function (not nil)
    - Test: calling circle("fill", 10.7, 20.3, 5) does not error (float coords accepted)
    - Test: calling line(1.5, 2.5, 10.9, 20.1) does not error (float coords accepted)
    - Test: calling rectangle(10.6, 20.4, 30, 40) does not error
    - Test: calling point(5.7, 3.2) does not error
  </behavior>
  <action>
**1. Register BTN and COLOR global tables in registerAll() (bindings.cpp).**

After the existing `LAYER_BG`/`LAYER_MID`/`LAYER_FG`/`LAYER_UI` constants block (around line 1042), add:

```cpp
// Built-in button constants — BTN.UP, BTN.LEFT, etc.
lua_newtable(L);
lua_pushinteger(L, 0); lua_setfield(L, -2, "UP");
lua_pushinteger(L, 1); lua_setfield(L, -2, "DOWN");
lua_pushinteger(L, 2); lua_setfield(L, -2, "LEFT");
lua_pushinteger(L, 3); lua_setfield(L, -2, "RIGHT");
lua_pushinteger(L, 4); lua_setfield(L, -2, "A");
lua_pushinteger(L, 5); lua_setfield(L, -2, "B");
lua_pushinteger(L, 6); lua_setfield(L, -2, "START");
lua_setglobal(L, "BTN");

// Built-in palette color constants — COLOR.BLACK, COLOR.RED, etc.
// Matches the default 16-color PICO-8-inspired palette
lua_newtable(L);
lua_pushinteger(L, 0);  lua_setfield(L, -2, "BLACK");
lua_pushinteger(L, 1);  lua_setfield(L, -2, "DARK_BLUE");
lua_pushinteger(L, 2);  lua_setfield(L, -2, "DARK_RED");
lua_pushinteger(L, 3);  lua_setfield(L, -2, "DARK_GREEN");
lua_pushinteger(L, 4);  lua_setfield(L, -2, "BROWN");
lua_pushinteger(L, 5);  lua_setfield(L, -2, "DARK_GRAY");
lua_pushinteger(L, 6);  lua_setfield(L, -2, "GRAY");
lua_pushinteger(L, 7);  lua_setfield(L, -2, "WHITE");
lua_pushinteger(L, 8);  lua_setfield(L, -2, "RED");
lua_pushinteger(L, 9);  lua_setfield(L, -2, "ORANGE");
lua_pushinteger(L, 10); lua_setfield(L, -2, "YELLOW");
lua_pushinteger(L, 11); lua_setfield(L, -2, "GREEN");
lua_pushinteger(L, 12); lua_setfield(L, -2, "BLUE");
lua_pushinteger(L, 13); lua_setfield(L, -2, "INDIGO");
lua_pushinteger(L, 14); lua_setfield(L, -2, "PINK");
lua_pushinteger(L, 15); lua_setfield(L, -2, "TRANSPARENT");
lua_setglobal(L, "COLOR");
```

**2. Add engine.graphics sub-table in registerEngineTable() (bindings_engine.cpp).**

Add a new sub-table block before the `lua_setglobal(L, "engine")` line at the end of registerEngineTable(). This aliases the same static methods already registered as globals:

```cpp
// --- engine.graphics sub-table (aliases for global drawing functions) ---
static const LuaFuncDef kGraphicsFuncs[] = {
    {"clear",        lua_clear},
    {"setColor",     lua_setColor},
    {"getColor",     lua_getColor},
    {"setLineWidth", lua_setLineWidth},
    {"getLineWidth", lua_getLineWidth},
    {"point",        lua_point},
    {"line",         lua_line},
    {"rectangle",    lua_rectangle},
    {"circle",       lua_circle},
    {"triangle",     lua_triangle},
    {"setPixel",     lua_setPixel},
    {"getPixel",     lua_getPixel},
    {"text",         lua_text},
    {"textWrapped",  lua_textWrapped},
    {"setTextSize",  lua_setTextSize},
    {"getTextSize",  lua_getTextSize},
    {"setFont",      lua_setFont},
    {"getFont",      lua_getFont},
    {"getTextWidth", lua_getTextWidth},
    {"getTextHeight",lua_getTextHeight},
    {"getWidth",     lua_getWidth},
    {"getHeight",    lua_getHeight},
};
lua_newtable(L);
luaBindFunctions(L, -1, kGraphicsFuncs, ENJIN_ARRAY_LEN(kGraphicsFuncs));
lua_setfield(L, -2, "graphics");
```

NOTE: The static methods referenced here are private members of LuaBindings. Since registerEngineTable() is a member function, it has access. This works because register*() methods are non-static members.

**3. Fix float-to-int coordinate casting in drawing functions (bindings_draw.cpp).**

Add `#include <cmath>` at top of bindings_draw.cpp.

In every drawing function that reads coordinate arguments via `lua_tointeger`, change the pattern to use `lround(lua_tonumber(...))` for coordinates. Specifically update these functions:

- `lua_point`: change `lua_tointeger(L, 1)` and `(L, 2)` to `static_cast<int16_t>(lround(lua_tonumber(L, 1)))` etc.
- `lua_line`: same for all 4 coordinate args
- `lua_rectangle`: x and y args (width/height stay as integers)
- `lua_circle`: x and y args (radius stays as integer)
- `lua_triangle`: all 6 coordinate args

Do NOT change `lua_setPixel`/`lua_getPixel` (pixel coords should be exact integers) or the high-performance functions (they already use `luaL_checknumber` with `static_cast<int>` which truncates toward zero, but that's intentional for their optimized path).

**4. Create the test file tests/scripting_api_test.cpp** using the same fixture pattern as engine_table_test.cpp. Add it to tests/CMakeLists.txt inside the `if(ENJIN2_BUILD_LUA)` block, linked to `enjin2` and `enjin2_lua`.

Also need to add `#include <cstdio>` to the test and use the ASSERT macro pattern.

To test the float coord rounding, provide a 16x16 LuaCanvas so drawing calls have a real canvas to target and do not early-return. Create a `Canvas4<16,16>` and a `LuaCanvas` wrapping it, then call `bindings.setCanvas(&luaCanvas)`.
  </action>
  <verify>
    <automated>cd /home/unwn/dev/enjin && cmake --build build --target scripting_api_test 2>&1 && ./build/tests/scripting_api_test</automated>
  </verify>
  <done>BTN.UP/DOWN/LEFT/RIGHT/A/B/START and COLOR.BLACK through COLOR.TRANSPARENT are accessible as global Lua tables. engine.graphics.* aliases all drawing/text functions. Drawing primitives accept float coordinates via lround without error. All tests pass.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: Text scale parameter, textCentered, textAligned, engine.config.resolution, and engine.state manager</name>
  <files>src/scripting/bindings_layers_text.cpp, src/scripting/bindings_engine.cpp, src/scripting/bindings.cpp, include/enjin2/scripting/bindings.hpp, tests/scripting_api_test.cpp</files>
  <behavior>
    - Test: text("hello", 0, 0, 2) draws at scale 2 without changing global text size (getTextSize() still returns 1 after call)
    - Test: text("hello", 0, 0) still works (backward compat, 3 args)
    - Test: textCentered("hi", 64) does not error and is callable (needs canvas)
    - Test: textAligned("hi", 0, 64, "center") does not error
    - Test: textAligned("hi", 0, 64, "right") does not error
    - Test: engine.config.resolution() returns width, height (both > 0 when canvas is set)
    - Test: engine.state.current() returns "none" initially
    - Test: engine.state.switch("play") changes engine.state.current() to "play"
    - Test: engine.state.switch("play") triggers on_enter callback
    - Test: engine.state.switch from "play" to "menu" triggers on_exit("play") then on_enter("menu")
  </behavior>
  <action>
**1. Add optional scale parameter to lua_text() (bindings_layers_text.cpp).**

Modify `lua_text` to check for a 4th numeric argument. If present, use it as a temporary scale override:

```cpp
int LuaBindings::lua_text(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    const char* str = luaL_checkstring(L, 1);
    int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 3));
    // Optional 4th arg: temporary scale override
    uint8_t scale = b->currentTextSize;
    if (lua_gettop(L) >= 4 && lua_isnumber(L, 4)) {
        int s = static_cast<int>(lua_tointeger(L, 4));
        if (s > 0 && s <= 255) scale = static_cast<uint8_t>(s);
    }
    b->currentCanvas->drawText(str, x, y, b->currentColor, scale, b->currentFont);
    return 0;
}
```

Note: the global `currentTextSize` is NOT modified — this is a per-call override.

**2. Add textCentered(str, y [, scale]) binding (bindings_layers_text.cpp).**

New static method `lua_textCentered`:
```cpp
int LuaBindings::lua_textCentered(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    const char* str = luaL_checkstring(L, 1);
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint8_t scale = b->currentTextSize;
    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) {
        int s = static_cast<int>(lua_tointeger(L, 3));
        if (s > 0 && s <= 255) scale = static_cast<uint8_t>(s);
    }
    uint16_t tw = b->currentCanvas->measureTextWidth(str, scale, b->currentFont);
    int16_t x = static_cast<int16_t>((b->currentCanvas->getWidth() - tw) / 2);
    b->currentCanvas->drawText(str, x, y, b->currentColor, scale, b->currentFont);
    return 0;
}
```

**3. Add textAligned(str, x, y, align [, scale]) binding (bindings_layers_text.cpp).**

New static method `lua_textAligned`:
```cpp
int LuaBindings::lua_textAligned(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    const char* str = luaL_checkstring(L, 1);
    int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 3));
    const char* align = luaL_optstring(L, 4, "left");
    uint8_t scale = b->currentTextSize;
    if (lua_gettop(L) >= 5 && lua_isnumber(L, 5)) {
        int s = static_cast<int>(lua_tointeger(L, 5));
        if (s > 0 && s <= 255) scale = static_cast<uint8_t>(s);
    }
    if (strcmp(align, "center") == 0) {
        uint16_t tw = b->currentCanvas->measureTextWidth(str, scale, b->currentFont);
        x = static_cast<int16_t>(x - static_cast<int16_t>(tw / 2));
    } else if (strcmp(align, "right") == 0) {
        uint16_t tw = b->currentCanvas->measureTextWidth(str, scale, b->currentFont);
        x = static_cast<int16_t>(x - static_cast<int16_t>(tw));
    }
    // "left" = no adjustment (default)
    b->currentCanvas->drawText(str, x, y, b->currentColor, scale, b->currentFont);
    return 0;
}
```

**4. Declare new methods in bindings.hpp.**

In the private section of LuaBindings, alongside the existing text bindings, add:
```cpp
static int lua_textCentered(lua_State* L);
static int lua_textAligned(lua_State* L);
```

Also add for engine.config/state:
```cpp
static int lua_engine_config_resolution(lua_State* L);
static int lua_engine_state_switch(lua_State* L);
static int lua_engine_state_current(lua_State* L);
static int lua_engine_state_on_enter(lua_State* L);
static int lua_engine_state_on_exit(lua_State* L);
```

Add private member for state management:
```cpp
char m_currentGameState[64]{"none"};        ///< Current game state name
int  m_stateOnEnterRefs[16]{};              ///< Lua registry refs for on_enter callbacks (per named state, linear scan)
int  m_stateOnExitRefs[16]{};               ///< Lua registry refs for on_exit callbacks
char m_stateNames[16][64]{};                ///< State names corresponding to callback refs
int  m_stateCount{0};                       ///< Number of registered states
```

Initialize m_stateOnEnterRefs and m_stateOnExitRefs entries to LUA_NOREF in constructor or registerAll().

**5. Register textCentered and textAligned in registerAll() (bindings.cpp).**

After the existing text registration block (around line 1033), add:
```cpp
engine->registerFunction("textCentered", lua_textCentered);
engine->registerFunction("textAligned",  lua_textAligned);
```

Also add them to the engine.graphics sub-table in registerEngineTable (from Task 1).

**6. Add engine.config sub-table in registerEngineTable() (bindings_engine.cpp).**

```cpp
// --- engine.config sub-table ---
static const LuaFuncDef kConfigFuncs[] = {
    {"resolution", lua_engine_config_resolution},
};
lua_newtable(L);
luaBindFunctions(L, -1, kConfigFuncs, ENJIN_ARRAY_LEN(kConfigFuncs));
lua_setfield(L, -2, "config");
```

Implement `lua_engine_config_resolution`:
```cpp
int LuaBindings::lua_engine_config_resolution(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        return 2;
    }
    lua_pushinteger(L, b->currentCanvas->getWidth());
    lua_pushinteger(L, b->currentCanvas->getHeight());
    return 2;
}
```

**7. Add engine.state sub-table in registerEngineTable() (bindings_engine.cpp).**

Implement a lightweight global state machine stored in LuaBindings. States are name strings. on_enter/on_exit are Lua callback references stored per state name.

```cpp
// --- engine.state sub-table (lightweight scene/state manager) ---
static const LuaFuncDef kStateFuncs[] = {
    {"switch",   lua_engine_state_switch},
    {"current",  lua_engine_state_current},
    {"on_enter", lua_engine_state_on_enter},
    {"on_exit",  lua_engine_state_on_exit},
};
lua_newtable(L);
luaBindFunctions(L, -1, kStateFuncs, ENJIN_ARRAY_LEN(kStateFuncs));
lua_setfield(L, -2, "state");
```

`lua_engine_state_current(L)` pushes `b->m_currentGameState` as a string.

`lua_engine_state_switch(L)` takes a string name. If current state has an on_exit callback registered, call it via `lua_rawgeti + lua_pcall`. Set `m_currentGameState` to the new name. If new state has an on_enter callback, call it via `lua_rawgeti + lua_pcall`.

`lua_engine_state_on_enter(L)` takes (state_name, callback). Finds or creates the state entry, stores luaL_ref for the callback in m_stateOnEnterRefs.

`lua_engine_state_on_exit(L)` same pattern for exit callbacks.

Reset state in registerAll() alongside other per-reload resets: set m_currentGameState to "none", unref all callback refs, zero m_stateCount.

**8. Update the test file** (scripting_api_test.cpp) from Task 1 to add tests for all Task 2 features. The test fixture already has a canvas from Task 1 so text functions can execute.
  </action>
  <verify>
    <automated>cd /home/unwn/dev/enjin && cmake --build build --target scripting_api_test 2>&1 && ./build/tests/scripting_api_test</automated>
  </verify>
  <done>text("str", x, y, scale) works with optional 4th param without mutating global textSize. textCentered and textAligned registered as both globals and engine.graphics members. engine.config.resolution() returns canvas dimensions. engine.state.switch/current/on_enter/on_exit work with callback invocation. All tests pass.</done>
</task>

</tasks>

<verification>
All 7 scripting API improvements verified by running the unified test binary:

```bash
cd /home/unwn/dev/enjin && cmake --build build --target scripting_api_test && ./build/tests/scripting_api_test
```

Additionally, verify existing tests still pass (no regressions from float-to-int changes):
```bash
cd /home/unwn/dev/enjin && cmake --build build && ctest --test-dir build --output-on-failure -j$(nproc)
```
</verification>

<success_criteria>
- All 7 improvements implemented and tested
- BTN.UP/DOWN/LEFT/RIGHT/A/B/START global table accessible from Lua
- COLOR.BLACK through COLOR.TRANSPARENT global table accessible
- engine.graphics.* aliases all drawing/text functions
- Drawing primitives accept float coords without error (lround)
- text(str, x, y, scale) optional 4th arg works, global state unchanged
- textCentered(str, y) and textAligned(str, x, y, align) registered
- engine.config.resolution() returns canvas w, h
- engine.state.switch/current/on_enter/on_exit provide game-state management
- scripting_api_test passes with 0 failures
- Existing test suite (ctest) passes with 0 regressions
</success_criteria>

<output>
After completion, create `.planning/quick/007-implement-7-scripting-api-improvements-b/007-SUMMARY.md`
</output>
