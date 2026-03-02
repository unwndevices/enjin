# Phase 52: UI Component Bindings - Research

**Researched:** 2026-03-02
**Domain:** Lua C bindings / immediate-mode UI draw calls over LuaCanvas
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- UI draws are stateless — no pool, no slots, no capacity limits
- Each call draws immediately to the canvas and returns — no retained state between frames
- engine.ui.* bypasses C++ Label/FillUpGauge components entirely — stateless LuaCanvas draw calls only
- bindings_ui.cpp is the new split binding file (Phase 46 pattern)
- engine.ui.* sub-table registered in bindings_engine.cpp alongside other engine.* tables

### Claude's Discretion

- Which canvas layer UI calls draw to (active layer vs dedicated UI layer)
- Whether UI calls are screen-space only or support camera-relative positioning
- statBar visual design (bar only, or bar with current/max label)
- Color model (palette indices consistent with existing draw calls)
- resetUIState() implementation in registerAll()
- Internal guide document structure and content

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| UI-01 | engine.ui.progressBar(x,y,w,h,value,fg,bg) stateless draw call | LuaCanvas fillRect pattern from bindings_debug.cpp and bindings_draw.cpp; value is 0..1 float for fill proportion |
| UI-02 | engine.ui.statBar(x,y,w,h,current,max,fg,bg) stateless draw call | Same fillRect pattern; fill proportion = current/max with max-guard; boundary values: 0, max, mid |
| UI-03 | engine.ui.panel(x,y,w,h,bg,border) stateless draw call | fillRect for bg then drawRect for border — both available on LuaCanvas |
| UI-04 | engine.ui.label(x,y,text,fg) stateless draw call | LuaCanvas::drawText(str, x, y, color, size=1, font=nullptr) — same pattern as bindings_debug.cpp debug.text |
| UI-05 | Internal guide document for building new engine.ui.* components | .planning/phases/52-ui-component-bindings/ or docs/ location; covers canvas-call pattern, stateless contract, hot-reload |
</phase_requirements>

## Summary

Phase 52 adds four stateless immediate-mode UI draw functions under an `engine.ui.*` Lua sub-table. The implementation is entirely contained to a new `bindings_ui.cpp` file plus registration wiring in `bindings_engine.cpp`. No new data structures, no pool, no per-frame state — each function reads its arguments, calls one or two LuaCanvas methods, and returns.

The entire technical pattern is already established by `bindings_debug.cpp` (Phase 47). That file demonstrates the exact split-file pattern, the `REQUIRE_CANVAS` guard macro, the `registerXxxSubtable(lua_State* L)` helper called from `registerEngineTable()`, and the `LuaFuncDef` / `luaBindFunctions` / `ENJIN_ARRAY_LEN` registration idiom. Phase 52 reproduces that pattern verbatim for `engine.ui.*`.

The one design decision with real impact is: what does `progressBar(value)` expect — a 0..1 float, or integer pixels? Looking at how `statBar` is specified (`current, max`), the most consistent choice is `progressBar` takes a 0..1 float fill fraction, while `statBar` computes fill internally as `current/max`. This avoids forcing callers to pre-divide and matches the stateless contract (no retained domain model). Hot-reload handling is trivially clean: because there is no pool and no refs, `registerAll()` needs no `resetUIState()` call and `setActiveScene()` needs nothing. The only integration point is the one-line addition of `registerUISubtable(L)` in `registerEngineTable()`.

**Primary recommendation:** Mirror `bindings_debug.cpp` exactly — new `bindings_ui.cpp`, `registerUISubtable` called from `registerEngineTable`, REQUIRE_CANVAS guard, static binding functions declared in `bindings.hpp` private section, one entry in `CMakeLists.txt` `target_sources`, one line in `bindings.hpp` for the registration declaration.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| LuaCanvas | project | Type-erased canvas wrapper — fillRect, drawRect, drawText | Only draw API in scope; 4-bit/8-bit dispatch handled internally |
| bind_helpers.hpp | project | LuaFuncDef, luaBindFunctions, ENJIN_ARRAY_LEN | Established in Phase 46 (BIND-01); used by every split binding file |
| bindings_internal.hpp | project | Inter-TU shared constants; included by all binding files | Established in Phase 46; avoids ODR violations for metatable name constants |
| bindings.hpp | project | LuaBindings class — getBindings(), currentCanvas, currentColor | Houses all static member declarations; new UI functions declared here |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| luaL_checkinteger / luaL_optnumber | Lua C API | Argument extraction with error | Use luaL_checkinteger for x,y,w,h; luaL_checkinteger for color palette index |
| luaL_checknumber | Lua C API | Float argument for progressBar value (0..1) | Use for value param of progressBar; cast to float |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Active canvas (currentCanvas) | Dedicated UI layer | Active canvas is the established pattern for draw functions; a dedicated layer adds complexity with no benefit for a stateless API |
| Screen-space only | Camera-relative | Screen-space is simpler and correct for HUD/UI use cases; camera-relative can be added later if needed |
| Bar only for statBar | Bar + text label overlay | Bar-only is simpler and stateless; a text label requires no additional font calls unless the discretion chooses to add one |

## Architecture Patterns

### Recommended Project Structure

```
src/scripting/
├── bindings_ui.cpp          # NEW — engine.ui.* binding functions + registerUISubtable
└── (all other existing .cpp files unchanged)

include/enjin2/scripting/
└── bindings.hpp             # ADD: 4 private static function declarations + registerUISubtable declaration
```

### Pattern 1: Split Binding File (Phase 46 Pattern)

**What:** Each sub-table gets its own .cpp file. Functions are declared as `static int` in `.cpp`, declared as `static int member` in `bindings.hpp` private section, and registered via a `registerXxxSubtable(lua_State* L)` method called from `registerEngineTable()`.

**When to use:** All Phase 47+ sub-tables follow this pattern exactly. Use it here without deviation.

**Example — bindings_debug.cpp as template:**

```cpp
// bindings_ui.cpp
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"

namespace enjin2 {

// Guard macro — same as REQUIRE_CANVAS in bindings_draw.cpp
#define REQUIRE_CANVAS(b, L) \
    LuaBindings* b = LuaBindings::getBindings(L); \
    if (!(b) || !(b)->currentCanvas) return 0

// UI-01: engine.ui.progressBar(x, y, w, h, value, fg, bg)
// value: 0.0..1.0 fill fraction. Clamped. fg=fill color, bg=background color.
int LuaBindings::lua_engine_ui_progressBar(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x     = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y     = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w     = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h     = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    float    value = static_cast<float>(luaL_checknumber(L, 5));
    uint8_t  fg    = static_cast<uint8_t>(luaL_checkinteger(L, 6));
    uint8_t  bg    = static_cast<uint8_t>(luaL_checkinteger(L, 7));

    // Clamp value to [0, 1]
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    // Draw background
    b->currentCanvas->fillRect(x, y, w, h, bg);
    // Draw fill proportional to value
    uint16_t fillW = static_cast<uint16_t>(static_cast<float>(w) * value);
    if (fillW > 0) {
        b->currentCanvas->fillRect(x, y, fillW, h, fg);
    }
    return 0;
}

// UI-02: engine.ui.statBar(x, y, w, h, current, max, fg, bg)
int LuaBindings::lua_engine_ui_statBar(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x       = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y       = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w       = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h       = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    float    current = static_cast<float>(luaL_checknumber(L, 5));
    float    max     = static_cast<float>(luaL_checknumber(L, 6));
    uint8_t  fg      = static_cast<uint8_t>(luaL_checkinteger(L, 7));
    uint8_t  bg      = static_cast<uint8_t>(luaL_checkinteger(L, 8));

    // Guard against zero/negative max
    float fraction = (max > 0.0f) ? (current / max) : 0.0f;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    b->currentCanvas->fillRect(x, y, w, h, bg);
    uint16_t fillW = static_cast<uint16_t>(static_cast<float>(w) * fraction);
    if (fillW > 0) {
        b->currentCanvas->fillRect(x, y, fillW, h, fg);
    }
    return 0;
}

// UI-03: engine.ui.panel(x, y, w, h, bg, border)
int LuaBindings::lua_engine_ui_panel(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x      = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y      = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w      = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h      = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    uint8_t  bg     = static_cast<uint8_t>(luaL_checkinteger(L, 5));
    uint8_t  border = static_cast<uint8_t>(luaL_checkinteger(L, 6));
    b->currentCanvas->fillRect(x, y, w, h, bg);
    b->currentCanvas->drawRect(x, y, w, h, border);
    return 0;
}

// UI-04: engine.ui.label(x, y, text, fg)
int LuaBindings::lua_engine_ui_label(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t     x   = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t     y   = static_cast<int16_t>(luaL_checkinteger(L, 2));
    const char* str = luaL_checkstring(L, 3);
    uint8_t     fg  = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    b->currentCanvas->drawText(str, x, y, fg, 1, nullptr);  // size=1, default font
    return 0;
}

// Sub-table registration (called from registerEngineTable)
void LuaBindings::registerUISubtable(lua_State* L) {
    static const LuaFuncDef kUIFuncs[] = {
        {"progressBar", lua_engine_ui_progressBar},
        {"statBar",     lua_engine_ui_statBar},
        {"panel",       lua_engine_ui_panel},
        {"label",       lua_engine_ui_label},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kUIFuncs, ENJIN_ARRAY_LEN(kUIFuncs));
    lua_setfield(L, -2, "ui");  // engine.ui = { ... }
}

} // namespace enjin2
```

### Pattern 2: Wiring in registerEngineTable (bindings_engine.cpp)

**What:** One line added to `registerEngineTable()` at the end of the sub-table registrations, before `lua_setglobal(L, "engine")`.

**Example:**
```cpp
// --- engine.ui sub-table (Phase 52: UI-01..UI-04) ---
registerUISubtable(L);
```

### Pattern 3: Declaration in bindings.hpp

**What:** Four private static `int` declarations + one `void registerUISubtable(lua_State*)` declaration. Follow the exact layout of Phase 47 debug declarations.

**Example (added to private section of LuaBindings class):**
```cpp
// -- UI component bindings (Phase 52: UI-01..UI-04) ----------------------------
static int lua_engine_ui_progressBar(lua_State* L);
static int lua_engine_ui_statBar(lua_State* L);
static int lua_engine_ui_panel(lua_State* L);
static int lua_engine_ui_label(lua_State* L);
```

**And one public-facing registration declaration (alongside debug/async/tween):**
```cpp
void registerUISubtable(lua_State* L);  ///< engine.ui.* sub-table (called from registerEngineTable)
```

### Pattern 4: CMakeLists.txt Addition

**What:** One line in `target_sources(enjin2_lua PRIVATE ...)` in the root `CMakeLists.txt`.

**Example:**
```cmake
src/scripting/bindings_ui.cpp
```

### Anti-Patterns to Avoid

- **Pulling in std::string or Label/FillUpGauge headers:** These components use std::string and are incompatible with the zero-alloc pipeline. The REQUIREMENTS.md Out of Scope table explicitly states "std::string Label adaptation" is excluded.
- **Adding per-call allocation:** No `new`, no `std::vector`, no Lua registry refs. Every function must be a pure read-arguments / call-canvas / return-0 body.
- **Not declaring functions in bindings.hpp:** All binding functions are `static int` member functions declared in the private section of `LuaBindings`. Functions that are not members cannot access `currentCanvas` without an extra registry lookup. Follow the debug pattern.
- **Using `registerAll()` for UI teardown:** Because there is no state (no pool, no refs), `registerAll()` and `setActiveScene()` need no UI-specific cleanup. Do not add a `resetUIState()` call — it would be a no-op and add confusion.
- **Forgetting to add bindings_ui.cpp to CMakeLists.txt:** The file won't link. Check that the new file appears in `target_sources(enjin2_lua PRIVATE ...)`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Fill fraction math | Custom proportional fill | `static_cast<uint16_t>(w * value)` inline | One multiply, one cast — simpler than any abstraction |
| Text rendering | Custom font blit | `LuaCanvas::drawText(str, x, y, color, 1, nullptr)` | Already handles 4-bit/8-bit dispatch internally |
| Background + border panel | Custom pixel loop | `fillRect` then `drawRect` — two canvas calls | LuaCanvas already handles these primitives |

**Key insight:** Every primitive needed (fillRect, drawRect, drawText) is already in LuaCanvas. There is nothing to hand-roll.

## Common Pitfalls

### Pitfall 1: value parameter type mismatch
**What goes wrong:** `progressBar` takes a float `value` (0..1) but it's declared as `luaL_checkinteger`. Passing 0.5 from Lua silently truncates to 0.
**Why it happens:** Copy-paste from integer-only debug bindings.
**How to avoid:** Use `luaL_checknumber` and cast to float for the `value` and `current`/`max` params. Use `luaL_checkinteger` only for x, y, w, h, and color index params.
**Warning signs:** progressBar with value=0.5 always draws empty; statBar always shows full or empty.

### Pitfall 2: Division by zero in statBar
**What goes wrong:** `current / max` crashes or returns NaN/Inf when `max == 0`.
**Why it happens:** Unchecked division.
**How to avoid:** Guard: `float fraction = (max > 0.0f) ? (current / max) : 0.0f;`
**Warning signs:** statBar(x,y,w,h,0,0,fg,bg) crashes or draws corrupt output.

### Pitfall 3: Missing file in CMakeLists.txt
**What goes wrong:** Linker error — `undefined reference to LuaBindings::lua_engine_ui_progressBar` etc.
**Why it happens:** New .cpp file added to src/scripting/ but not listed in `target_sources(enjin2_lua PRIVATE ...)` in the root CMakeLists.txt.
**How to avoid:** Add `src/scripting/bindings_ui.cpp` to the `target_sources` block.
**Warning signs:** Link fails with undefined symbols for all four UI functions.

### Pitfall 4: Declaration omitted from bindings.hpp
**What goes wrong:** Compile error in bindings_ui.cpp — `LuaBindings::lua_engine_ui_*` is not a member.
**Why it happens:** Split-file pattern requires all static members to be declared in the class header.
**How to avoid:** Add all four `static int lua_engine_ui_*` declarations to the private section of `LuaBindings` in `bindings.hpp`. Add `void registerUISubtable(lua_State* L)` to the private section alongside `registerDebugSubtable` / `registerAsyncSubtable` / `registerTweenSubtable`.
**Warning signs:** Compiler error: "'lua_engine_ui_progressBar' is not a member of 'enjin2::LuaBindings'".

### Pitfall 5: Forgetting to call registerUISubtable in registerEngineTable
**What goes wrong:** `engine.ui` is nil at runtime — tests fail with "attempt to index a nil value (global 'engine')".
**Why it happens:** The sub-table is implemented but never wired into the engine table.
**How to avoid:** Add `registerUISubtable(L);` inside `registerEngineTable()` in `bindings_engine.cpp`, before `lua_setglobal(L, "engine")`.
**Warning signs:** `type(engine.ui) == 'nil'` instead of `'table'`.

### Pitfall 6: fillW overflow when w is large and value is tiny float rounding
**What goes wrong:** If `static_cast<uint16_t>(w * value)` produces a value >= w due to float rounding, the fill overflows the background rect.
**Why it happens:** Value clamped to 1.0f but `w * 1.0f` may round up due to float precision.
**How to avoid:** After computing fillW, clamp: `if (fillW > w) fillW = w;`
**Warning signs:** Fill draws 1 pixel beyond the background bar.

## Code Examples

Verified patterns from official project sources:

### Exact debug.text binding (the label pattern)
```cpp
// Source: src/scripting/bindings_debug.cpp:72-80
int LuaBindings::lua_engine_debug_text(lua_State* L) {
    REQUIRE_DEBUG_CANVAS(b);
    const char* str = luaL_checkstring(L, 1);
    int16_t x   = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y   = static_cast<int16_t>(luaL_checkinteger(L, 3));
    uint8_t col = static_cast<uint8_t>(luaL_optinteger(L, 4, 8));
    b->m_debugCanvas->drawText(str, x, y, col, 1, nullptr);  // size=1, default font
    return 0;
}
// For label: same shape, but uses currentCanvas and mandatory fg param
```

### Exact debug sub-table registration pattern
```cpp
// Source: src/scripting/bindings_debug.cpp:99-113
void LuaBindings::registerDebugSubtable(lua_State* L) {
    static const LuaFuncDef kDebugFuncs[] = {
        {"rect",       lua_engine_debug_rect},
        // ...
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kDebugFuncs, ENJIN_ARRAY_LEN(kDebugFuncs));
    lua_setfield(L, -2, "debug");
}
// For UI: replace "debug" with "ui", replace function list
```

### Wiring call in registerEngineTable (existing pattern)
```cpp
// Source: src/scripting/bindings_engine.cpp:220-226
// --- engine.debug sub-table (Phase 47: DEBUG-01..DEBUG-03) ---
registerDebugSubtable(L);

// --- engine.async sub-table (Phase 49: ASYNC-01..ASYNC-03) ---
registerAsyncSubtable(L);

// --- engine.tween sub-table (Phase 50: TWEEN-01..TWEEN-03) ---
registerTweenSubtable(L);

// ADD HERE (Phase 52: UI-01..UI-04):
// --- engine.ui sub-table (Phase 52: UI-01..UI-04) ---
registerUISubtable(L);
```

### Test fixture pattern (from debug_draw_test.cpp / persistent_lua_test.cpp)
```cpp
// Source: tests/debug_draw_test.cpp:35-52
struct UIFixture {
    LuaEngine engine;
    LuaBindings bindings;

    UIFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
        // Inject a canvas if pixel-level verification needed:
        // Canvas4<128,128> canvas;
        // LuaCanvas lc(&canvas);
        // bindings.setCanvas(&lc);
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* name)  { return engine.getGlobalNumber(name); }
    bool getBool(const char* name)   { return engine.getGlobalBool(name); }
};
```

### Table existence + function shape test (from debug_draw_test.cpp)
```lua
ok_table       = (type(engine.ui) == 'table') and 1 or 0
ok_progressBar = (type(engine.ui.progressBar) == 'function') and 1 or 0
ok_statBar     = (type(engine.ui.statBar) == 'function') and 1 or 0
ok_panel       = (type(engine.ui.panel) == 'function') and 1 or 0
ok_label       = (type(engine.ui.label) == 'function') and 1 or 0
```

### Pixel-level verification (for canvas-injected tests)
```cpp
// After calling bindings.exec("engine.ui.progressBar(0,0,10,4,0.5,7,0)")
// canvas[0,0] should be palette index 7 (fg, in filled half)
// canvas[5,0] should be palette index 0 (bg, in unfilled half)
uint8_t pixel_fg = canvas.getPixel(0, 0);  // expect 7
uint8_t pixel_bg = canvas.getPixel(5, 0);  // expect 0
ASSERT(pixel_fg == 7, "progressBar fills with fg color");
ASSERT(pixel_bg == 0, "progressBar leaves bg color in unfilled area");
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Monolithic bindings.cpp | Split binding files (bindings_debug.cpp, bindings_tween.cpp, etc.) | Phase 46 (BIND-01) | bindings_ui.cpp follows the split pattern directly |
| C++ Label / FillUpGauge components | Stateless LuaCanvas draw calls | Phase 52 decision | No std::string contamination; zero-alloc |

**Deprecated/outdated:**
- Using C++ Label or FillUpGauge for Lua UI: out of scope per REQUIREMENTS.md and CONTEXT.md locked decisions.

## Open Questions

1. **Which canvas layer do UI calls draw to?**
   - What we know: `currentCanvas` is the active layer canvas; `setLayer(n)` changes which layer is active. Debug uses a dedicated `m_debugCanvas` to be always on top.
   - What's unclear: Should engine.ui.* always draw to `currentCanvas` (caller-controlled), or should there be a dedicated UI canvas?
   - Recommendation: Draw to `currentCanvas` — this is the established pattern for all non-debug draw calls. Callers control the layer by calling `setLayer(n)` before UI calls. This is consistent with the stateless design and requires no new member.

2. **statBar visual design: bar only, or bar + text label?**
   - What we know: CONTEXT.md leaves this to Claude's discretion. The bar-only approach is simpler and stateless. Adding a text overlay would require sprintf or snprintf (stack-allocated, no heap) which IS compatible with the zero-alloc constraint.
   - What's unclear: Whether the user expects HP/MP style bars or labeled bars.
   - Recommendation: Bar-only for MVP. The label() function already provides text overlay if needed. A statBar with text can be composited from `statBar + label` in Lua.

3. **progressBar value parameter: float 0..1 or integer pixel count?**
   - What we know: The requirements say `engine.ui.progressBar(x,y,w,h,value,fg,bg)` — "value" is ambiguous.
   - What's unclear: None — "value" in the context of a progress bar is universally 0..1.
   - Recommendation: Use float 0..1 via `luaL_checknumber`. This is the canonical progress bar API.

4. **Location of UI-05 internal guide document**
   - What we know: CONTEXT.md says an "internal guide document exists explaining how to add a new engine.ui.* component."
   - What's unclear: Directory — `.planning/phases/52-ui-component-bindings/`, `docs/`, or the .planning root?
   - Recommendation: Create as `docs/ui-component-guide.md` (or `.planning/phases/52-ui-component-bindings/UI-GUIDE.md` if docs/ does not exist). Check with `ls /home/unwn/dev/enjin/docs` before planning.

## Sources

### Primary (HIGH confidence)

- `/home/unwn/dev/enjin/src/scripting/bindings_debug.cpp` — canonical split-file pattern; REQUIRE_DEBUG_CANVAS macro; registerDebugSubtable; drawText signature
- `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` — registerEngineTable wiring location; pattern for sub-table registration calls
- `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — LuaBindings class: currentCanvas member, LuaCanvas API (fillRect, drawRect, drawText), private static declaration pattern, registerXxxSubtable declaration pattern
- `/home/unwn/dev/enjin/include/enjin2/scripting/bind_helpers.hpp` — LuaFuncDef, luaBindFunctions, ENJIN_ARRAY_LEN
- `/home/unwn/dev/enjin/CMakeLists.txt` — target_sources(enjin2_lua) block; where to add bindings_ui.cpp
- `/home/unwn/dev/enjin/tests/CMakeLists.txt` — test registration pattern for ENJIN2_BUILD_LUA guard
- `/home/unwn/dev/enjin/tests/debug_draw_test.cpp` — test fixture and assertion pattern for sub-table tests
- `/home/unwn/dev/enjin/tests/persistent_lua_test.cpp` — pixel-canvas test fixture pattern for draw verification

### Secondary (MEDIUM confidence)

- `.planning/STATE.md` — confirms "engine.ui.* bypasses C++ Label/FillUpGauge entirely — stateless LuaCanvas draw calls only" decision; Phase 46+ split pattern history
- `.planning/REQUIREMENTS.md` — UI-01..UI-05 requirement text; Out of Scope confirms Label/FillUpGauge bypass

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — All tools are internal project APIs verified by reading the actual source files
- Architecture: HIGH — The exact pattern is implemented in three prior phases (47, 49, 50) and verified by source inspection
- Pitfalls: HIGH — Pitfalls derived from reading the source code and understanding the split-file + CMake wiring requirements; one pitfall (float truncation) is a known C API gotcha
- UI-05 guide location: MEDIUM — docs/ directory existence not verified; planner should check before choosing location

**Research date:** 2026-03-02
**Valid until:** Stable — internal project patterns; valid until bindings.hpp or LuaCanvas API changes
