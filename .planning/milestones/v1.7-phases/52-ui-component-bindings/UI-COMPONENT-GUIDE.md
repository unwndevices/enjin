# How to Add a New engine.ui.* Component

**Internal developer guide for Phase 52 and beyond.**

A developer who reads this guide should be able to add a new `engine.ui.*` draw
function (e.g., `engine.ui.healthBar`, `engine.ui.button`, `engine.ui.checkbox`)
without reading any other file first.

---

## 1. Overview

`engine.ui.*` functions are **stateless immediate-mode draw calls**. Each call:

1. Reads arguments from the Lua stack
2. Draws to the currently active canvas (`currentCanvas`)
3. Returns immediately with no retained state

Key design facts:

- **No per-call allocation** — no `new`, no `std::vector`, no `std::string`
- **No retained state between frames** — no pool, no slots, no capacity limits
- **No Lua registry refs** — no `luaL_ref` / `luaL_unref`
- **Bypasses C++ Label and FillUpGauge components entirely** — those types use
  `std::string`, which is incompatible with the zero-alloc pipeline
- **Screen-space only** — UI draws go to `currentCanvas`, the active layer
  canvas, not the camera-relative scene

This makes hot-reload handling trivial (see Section 5) and keeps the binding
functions easy to reason about.

---

## 2. Anatomy of a UI Function

Every `engine.ui.*` function follows the same five-step pattern.

### Step-by-Step Template

```
1. REQUIRE_CANVAS guard  — early return 0 if bindings or canvas are null
2. Extract arguments     — luaL_checkinteger for pixel coords/colors, luaL_checknumber for floats
3. Validate / clamp      — clamp ratios to [0, 1], guard division by zero
4. Call LuaCanvas        — fillRect, drawRect, drawText, etc.
5. return 0              — no values pushed to Lua
```

### Concrete Example: engine.ui.progressBar

```cpp
// UI-01: engine.ui.progressBar(x, y, w, h, value, fg, bg)
// value: 0.0..1.0 fill fraction; fg = fill color, bg = background color.
int LuaBindings::lua_engine_ui_progressBar(lua_State* L) {
    REQUIRE_CANVAS(b, L);                                                 // Step 1

    int16_t  x     = static_cast<int16_t>(luaL_checkinteger(L, 1));      // Step 2
    int16_t  y     = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w     = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h     = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    float    value = static_cast<float>(luaL_checknumber(L, 5));
    uint8_t  fg    = static_cast<uint8_t>(luaL_checkinteger(L, 6));
    uint8_t  bg    = static_cast<uint8_t>(luaL_checkinteger(L, 7));

    if (value < 0.0f) value = 0.0f;                                       // Step 3
    if (value > 1.0f) value = 1.0f;

    b->currentCanvas->fillRect(x, y, w, h, bg);                          // Step 4
    uint16_t fillW = static_cast<uint16_t>(static_cast<float>(w) * value);
    if (fillW > 0) b->currentCanvas->fillRect(x, y, fillW, h, fg);

    return 0;                                                             // Step 5
}
```

### Concrete Example: engine.ui.panel (two canvas calls)

```cpp
// UI-03: engine.ui.panel(x, y, w, h, bg, border)
int LuaBindings::lua_engine_ui_panel(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x      = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y      = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w      = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h      = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    uint8_t  bg     = static_cast<uint8_t>(luaL_checkinteger(L, 5));
    uint8_t  border = static_cast<uint8_t>(luaL_checkinteger(L, 6));
    b->currentCanvas->fillRect(x, y, w, h, bg);    // filled background
    b->currentCanvas->drawRect(x, y, w, h, border); // outline on top
    return 0;
}
```

### The REQUIRE_CANVAS Macro

Defined at the top of `src/scripting/bindings_draw.cpp` and redefined (copied)
at the top of `src/scripting/bindings_ui.cpp`:

```cpp
#define REQUIRE_CANVAS(b, L) \
    LuaBindings* b = LuaBindings::getBindings(L); \
    if (!(b) || !(b)->currentCanvas) return 0
```

`b` becomes your local `LuaBindings*`. Access the canvas via `b->currentCanvas`.
This also handles the SDL test fixture case where no canvas is injected.

---

## 3. Available Canvas Primitives

All methods are on `LuaCanvas` (declared in
`include/enjin2/scripting/bindings.hpp`). Access via `b->currentCanvas->...`.

| Method | Signature | Description |
|--------|-----------|-------------|
| `fillRect` | `(x, y, w, h, color)` | Solid filled rectangle |
| `drawRect` | `(x, y, w, h, color)` | Rectangle outline only |
| `drawLine` | `(x1, y1, x2, y2, color)` | Single-pixel line |
| `drawCircle` | `(x, y, radius, color)` | Circle outline |
| `fillCircle` | `(x, y, radius, color)` | Solid filled circle |
| `drawTriangle` | `(x1,y1, x2,y2, x3,y3, color)` | Triangle outline |
| `fillTriangle` | `(x1,y1, x2,y2, x3,y3, color)` | Solid filled triangle |
| `setPixel` | `(x, y, color)` | Single pixel write |
| `getPixel` | `(x, y)` | Single pixel read (uint8_t) |
| `drawText` | `(str, x, y, color, size, font)` | Text; size=1 and font=nullptr for default |
| `drawTextWrapped` | `(str, x, y, maxWidth, color, size, font)` | Word-wrapped text |
| `measureTextWidth` | `(str, size, font)` | Pixel width of string |
| `measureTextHeight` | `(size, font)` | Pixel height of a glyph row |
| `getWidth` | `()` | Canvas width in pixels |
| `getHeight` | `()` | Canvas height in pixels |
| `clear` | `(color)` | Fill entire canvas |

All `color` parameters are **palette indices** (`uint8_t`). See Section 6.

---

## 4. Wiring Checklist

Follow these steps in order when adding a new `engine.ui.*` function.

### Step 1 — Declare in bindings.hpp (private section)

Open `include/enjin2/scripting/bindings.hpp`. Find the block:

```cpp
// -- UI component bindings (Phase 52: UI-01..UI-04) ----------------------------
static int lua_engine_ui_progressBar(lua_State* L);
static int lua_engine_ui_statBar(lua_State* L);
static int lua_engine_ui_panel(lua_State* L);
static int lua_engine_ui_label(lua_State* L);
```

Add a new line in the same block:

```cpp
static int lua_engine_ui_myNewFunc(lua_State* L);
```

### Step 2 — Implement in bindings_ui.cpp

Open `src/scripting/bindings_ui.cpp`. Add a new function following the template
in Section 2. Name it `int LuaBindings::lua_engine_ui_myNewFunc(lua_State* L)`.

### Step 3 — Register in kUIFuncs array

Still in `bindings_ui.cpp`, find `registerUISubtable`:

```cpp
void LuaBindings::registerUISubtable(lua_State* L) {
    static const LuaFuncDef kUIFuncs[] = {
        {"progressBar", lua_engine_ui_progressBar},
        {"statBar",     lua_engine_ui_statBar},
        {"panel",       lua_engine_ui_panel},
        {"label",       lua_engine_ui_label},
        // add your new entry here:
        {"myNewFunc",   lua_engine_ui_myNewFunc},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kUIFuncs, ENJIN_ARRAY_LEN(kUIFuncs));
    lua_setfield(L, -2, "ui");
}
```

### Step 4 — No CMakeLists.txt change needed

`bindings_ui.cpp` is already in `target_sources(enjin2_lua PRIVATE ...)`.
A CMakeLists.txt change is only required if you create a brand-new `.cpp` file.

### Step 5 — Write tests

Add tests to `tests/ui_binding_test.cpp` (see Section 8).
No changes to `tests/CMakeLists.txt` — `ui_binding_test` is already registered
under the `ENJIN2_BUILD_LUA` guard.

---

## 5. Hot-Reload Contract

Because `engine.ui.*` functions have **no state** (no pool, no registry refs,
no member variables), they survive hot-reload (F5) for free:

- `registerAll()` does **not** need a `resetUIState()` call
- `setActiveScene()` does **not** need any UI cleanup
- `clearCoroutines()` / `clearTweens()` are **not** called for UI

Contrast with sub-systems that do need cleanup:

| Sub-system | Needs cleanup on reload? | Why |
|------------|--------------------------|-----|
| engine.async.* | Yes — `clearCoroutines()` | Coroutine pool holds Lua registry refs |
| engine.tween.* | Yes — `clearTweens()` | Tween pool holds target refs and done-cb refs |
| engine.ui.* | **No** | Stateless — no refs, no pool slots |
| engine.debug.* | **No** | Stateless — same pattern as engine.ui.* |

If you add a UI function that needs cleanup (e.g., a persistent overlay that
stores a Lua ref), it is no longer a stateless UI function — reconsider the
design or move it to a different sub-table.

---

## 6. Color Model

All `engine.ui.*` colors are **palette indices**:

- 4-bit canvas: indices `0..15`
- 8-bit canvas: indices `0..255`

Use `luaL_checkinteger` and cast to `uint8_t`:

```cpp
uint8_t fg = static_cast<uint8_t>(luaL_checkinteger(L, 6));
```

Do **not** accept `#RRGGBB` hex strings, RGB triples, or any other color format.
This is consistent with all existing draw bindings (`lua_setColor`, `lua_rectangle`,
`lua_engine_debug_rect`, etc.).

---

## 7. Anti-Patterns

Avoid these mistakes:

| Anti-pattern | Why it breaks |
|-------------|---------------|
| `#include <string>` or using `std::string` | Heap allocation; forbidden in zero-alloc pipeline |
| `#include` Label.hpp or FillUpGauge.hpp | Those types use `std::string`; excluded from UI path |
| `std::vector`, `new`, `malloc` | Zero-alloc contract violation |
| `luaL_ref` / `luaL_unref` | Retained state — no longer stateless; hot-reload breaks |
| Member variables storing per-call state | Makes hot-reload cleanup mandatory; breaks stateless contract |
| `luaL_checkinteger` for a float parameter | Truncates 0.9 to 0 — use `luaL_checknumber` for ratios |
| Division by zero in ratio calculation | Guard with `if (max > 0.0f)` before dividing; return 0 fill otherwise |
| Forgetting `REQUIRE_CANVAS` | Null deref crash when canvas is not set (e.g., in SDL test fixture) |
| Forgetting to add to `kUIFuncs[]` | Function is implemented but invisible from Lua |
| Adding a new `.cpp` file without updating CMakeLists.txt | Link failure — file is silently excluded |

---

## 8. Testing Pattern

Tests live in `tests/ui_binding_test.cpp` and use two fixtures.

### UIFixture — no canvas injected

Tests null-canvas safety and table structure:

```cpp
struct UIFixture {
    LuaEngine  engine;
    LuaBindings bindings;

    UIFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
        // Intentionally no canvas — tests that calls don't crash
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* name)  { return engine.getGlobalNumber(name); }
};
```

Use this fixture to verify:
- `engine.ui` is a table
- Each function exists and is callable without crashing

### UICanvasFixture — with Canvas4<128,128>

Tests actual pixel output:

```cpp
struct UICanvasFixture {
    Canvas4<128, 128> canvas;
    LuaCanvas         luaCanvas;
    LuaEngine         engine;
    LuaBindings       bindings;

    UICanvasFixture() : luaCanvas(&canvas), bindings(&engine) {
        engine.initialize();
        bindings.setCanvas(&luaCanvas);
        bindings.registerAll();
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    uint8_t pixel(int x, int y) { return canvas.getPixel(x, y); }
};
```

Use `canvas.getPixel(x, y)` to assert specific pixels were drawn.

### Example test for a new function

```cpp
static void test_myNewFunc_draws_correctly() {
    printf("--- engine.ui.myNewFunc pixel output ---\n");
    UICanvasFixture f;
    LuaResult r = f.exec(
        "engine.ui.myNewFunc(10, 10, 20, 4, 7)"
    );
    ASSERT(r.success, "myNewFunc must not error");
    ASSERT(f.pixel(10, 10) == 7, "expected color 7 at (10, 10)");
}
```

### Adding to tests/CMakeLists.txt

No change needed — `ui_binding_test` is already registered under the
`ENJIN2_BUILD_LUA` guard. If you create a separate test file for your function,
add it under the same guard:

```cmake
if(ENJIN2_BUILD_LUA)
    add_executable(ui_myfeature_test tests/ui_myfeature_test.cpp ...)
    target_link_libraries(ui_myfeature_test PRIVATE enjin2_lua GTest::gtest_main)
    add_test(NAME ui_myfeature_test COMMAND ui_myfeature_test)
endif()
```

---

## Quick Reference: File Map

| File | What to change |
|------|---------------|
| `include/enjin2/scripting/bindings.hpp` | Add `static int lua_engine_ui_myNewFunc(lua_State* L);` in private section |
| `src/scripting/bindings_ui.cpp` | Implement the function; add entry to `kUIFuncs[]` |
| `src/scripting/bindings_engine.cpp` | No change — `registerUISubtable(L)` is already wired |
| `CMakeLists.txt` | No change if adding to existing `bindings_ui.cpp`; change only if creating a new file |
| `tests/ui_binding_test.cpp` | Add tests using UIFixture (null safety) and UICanvasFixture (pixel output) |
| `tests/CMakeLists.txt` | No change unless creating a new test executable |

---

*Phase 52 — UI Component Bindings — Internal Developer Guide*
*Last updated: 2026-03-02*
