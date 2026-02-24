# Phase 22: Lua Integration + E2E Validation - Research

**Researched:** 2026-02-24
**Domain:** C++ Lua bindings, SDL3 game loop integration, cross-platform script execution parity
**Confidence:** HIGH — all findings are from direct codebase inspection; no external library decisions required

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **Phase scope**: Wire input polling API (isButtonHeld, isButtonJustPressed, isButtonJustReleased, getAxis) and palette API into the shared Lua runtime so both are callable from scripts. Author a single E2E test script (`scripts/e2e_parity.lua`) that runs identically on SDL3 and WASM.
- **ESP32 parity sign-off is deferred** — this phase covers SDL3 + WASM only.
- **E2E test script format**: Visual demo running a continuous game loop — not assertion-based. Draws a color grid covering all 15 palette indices (0–14). Color-coded indicator cell (not text) changes state when button 0 is held.
- **Script location**: `scripts/e2e_parity.lua`
- **Visual parity acceptance bar**: SDL3 and WASM — exact RGB match required. Parity confirmed via manual eyeball. Sign-off recorded in VERIFICATION.md.
- **Input API function names (locked)**: `isButtonHeld(n)`, `isButtonJustPressed(n)`, `isButtonJustReleased(n)`, `getAxis(n)`
- **Button/axis arguments**: Raw integers, no named Lua constants in this phase.
- **E2E test exercises**: Button index 0 and axis index 0 only.
- **Lua error handling**: Engine host (C++) is responsible for catching Lua runtime errors. On error: log to stderr AND paint a visible on-canvas error signal (palette index 15 suggested as error color). This is permanent engine behavior.
- **Lua binding registration**: An existing binding file from Phase 19 (palette bindings) is already in place — input bindings extend it.

### Claude's Discretion

- Exact on-canvas error color/signal appearance
- How the color-coded input indicator cell is sized and positioned on the canvas
- Binding file structure decision (extend existing `bindings.cpp` vs. split into per-module files like `input_lua.cpp` + `palette_lua.cpp`) — planner should choose based on what exists and what scales

### Deferred Ideas (OUT OF SCOPE)

- ESP32 parity sign-off
- Named Lua constants (BTN_A = 0 etc.) exposed to Lua
- Text rendering for on-canvas debug output
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SDL-05 | Lua scripting works in SDL3 runner (same scripts as WASM/ESP32) | SDL runner (`sdl_main.cpp`) currently has NO Lua integration — `enjin2_lua` is intentionally excluded from `enjin2_sdl` link. This phase adds LuaScriptSystem to sdl_main.cpp and links enjin2_lua. |
| INP-05 | Lua input polling API (isButtonHeld, isButtonJustPressed, getAxis) | InputState struct has `held()`, `justPressed()`, `justReleased()` inline methods and `axes[]` array. New Lua bindings forward to these. The global `g_input` in sdl_main.cpp must be exposed to bindings via LuaBindings. |
</phase_requirements>

---

## Summary

Phase 22 wires two systems that already exist in the codebase but have not been connected: the SDL3 game loop (`sdl_main.cpp`) and the Lua scripting system (`enjin2_lua` library). Currently, `enjin2_sdl` deliberately excludes `enjin2_lua` from its link targets (confirmed in CMakeLists.txt line 274-279 and the Phase 21 verification). The SDL runner has no Lua integration at all.

The work has two distinct parts. First, add four input binding functions (`isButtonHeld`, `isButtonJustPressed`, `isButtonJustReleased`, `getAxis`) to `LuaBindings` in `bindings.cpp`/`bindings.hpp`, following the exact pattern already used by the palette bindings added in Phase 19. These bindings need access to the live `InputState` — meaning `LuaBindings` needs a pointer to the `InputState` that is updated each frame. Second, integrate `LuaScriptSystem` into `sdl_main.cpp`: load `scripts/e2e_parity.lua` at startup, then call its `update(dt)` and `draw()` functions inside the game loop. Link `enjin2_lua` to `enjin2_sdl` in CMakeLists.txt.

The E2E script itself (`scripts/e2e_parity.lua`) exercises both systems: it calls `setPaletteColor`/`setColor`/`rectangle` (palette and drawing, already bound) and the new `isButtonHeld(0)` (input, to be bound). The WASM path already exposes `LuaScriptSystem` via Emscripten bindings — the palette/drawing APIs work there already. Adding input bindings in `bindings.cpp` automatically makes them available to WASM as well, since WASM uses the same `LuaScriptSystem` code path.

**Primary recommendation:** Extend `bindings.cpp` with input bindings (do not split files yet — one new decision-making burden per phase), add an `InputState*` member to `LuaBindings`, integrate `LuaScriptSystem` into `sdl_main.cpp`, and author `scripts/e2e_parity.lua`.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua (system) | 5.x (system Lua, found via `find_package(Lua)`) | Script execution engine | Already in use; CMakeLists.txt desktop path uses system Lua |
| enjin2_lua | project-internal | LuaEngine + LuaBindings + LuaScriptSystem | Already built; Phase 22 just links it into enjin2_sdl |
| enjin2_input | project-internal | InputState, input_advance_frame, input_platform_poll | Already linked to enjin2_sdl; SDL3 poll already working |
| SDL3 | release-3.4.2 (FetchContent) | Window, renderer, event loop | Already in use in sdl_main.cpp |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| enjin2_graphics | project-internal | Canvas4, palette, primitives | Already linked; used by LuaBindings canvas drawing |
| enjin2_core | project-internal | Types, memory | Already linked |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Extend `bindings.cpp` | Split to `input_lua.cpp` | Splitting adds file management overhead now with no immediate benefit; deferring to when a third module actually exists is the right call |
| `g_input` global pointer in bindings | Passing InputState per-call | Global pointer is simpler and matches existing `g_palette` global pattern already used in bindings.cpp |

---

## Architecture Patterns

### Recommended Project Structure

```
src/
├── scripting/
│   └── bindings.cpp       # Extended: add lua_isButtonHeld, lua_isButtonJustPressed,
│                          # lua_isButtonJustReleased, lua_getAxis + registration
├── platform/sdl/
│   └── sdl_main.cpp       # Extended: add LuaScriptSystem, load script, call update/draw
include/enjin2/
└── scripting/
    └── bindings.hpp       # Extended: add InputState* member, 4 new static method declarations
scripts/
└── e2e_parity.lua         # New: E2E test script (color grid + input indicator)
```

### Pattern 1: Input Binding Registration (follows existing palette pattern)

**What:** Add `InputState*` to `LuaBindings`, register four new Lua globals in `registerAll()`, implement as static functions that pull `LuaBindings*` from registry and forward to `InputState` methods.

**When to use:** Any time a new platform-provided resource (input, audio, etc.) needs Lua access.

**Example (following bindings.cpp pattern exactly):**

```cpp
// bindings.hpp — new member
class LuaBindings {
private:
    // existing members...
    InputState* currentInput;  // NEW: set by host before each frame's script call

public:
    void setInput(InputState* input);  // NEW
    // ... existing methods ...

private:
    static int lua_isButtonHeld(lua_State* L);          // NEW
    static int lua_isButtonJustPressed(lua_State* L);   // NEW
    static int lua_isButtonJustReleased(lua_State* L);  // NEW
    static int lua_getAxis(lua_State* L);               // NEW
};

// bindings.cpp — registerAll() addition (follows palette block)
engine->registerFunction("isButtonHeld",       lua_isButtonHeld);
engine->registerFunction("isButtonJustPressed", lua_isButtonJustPressed);
engine->registerFunction("isButtonJustReleased", lua_isButtonJustReleased);
engine->registerFunction("getAxis",            lua_getAxis);

// bindings.cpp — implementation
int LuaBindings::lua_isButtonHeld(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->held(btn) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_isButtonJustPressed(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justPressed(btn) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_isButtonJustReleased(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justReleased(btn) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_getAxis(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushnumber(L, 0.0); return 1; }
    int axis = static_cast<int>(luaL_checkinteger(L, 1));
    float val = (axis >= 0 && axis < 8) ? b->currentInput->axes[axis] : 0.0f;
    lua_pushnumber(L, static_cast<lua_Number>(val));
    return 1;
}
```

### Pattern 2: LuaScriptSystem Integration in sdl_main.cpp

**What:** Instantiate `LuaScriptSystem` as a static global, initialize it, load `scripts/e2e_parity.lua` at startup, then call Lua functions `update(dt)` and `draw()` inside the game loop. Wire `g_input` to `bindings.setInput()` after each poll.

**When to use:** Any SDL platform runner that needs to execute Lua scripts.

**Example (sdl_main.cpp additions):**

```cpp
// New includes (after existing includes):
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>

// New static globals (alongside g_canvas, g_input):
static enjin2::LuaScriptSystem g_lua;
static enjin2::LuaCanvas       g_lua_canvas(&g_canvas);

// In main(), after SDL init and before game loop:
if (!g_lua.initialize()) {
    SDL_Log("Lua init failed");
    // ... cleanup and return 1
}
g_lua.setCanvas(&g_lua_canvas);
g_lua.getBindings().setInput(&g_input);   // wire input

LuaResult load_result = g_lua.loadScript("scripts/e2e_parity.lua");
if (!load_result.success) {
    std::cerr << "Script load error: " << load_result.error << std::endl;
    // Fill canvas with error color (index 14 or similar visible non-transparent index)
    g_canvas.clear(enjin2::Pixel4(14));
}

// In game loop, after input poll, before expand_canvas_to_rgb():
LuaResult update_result = g_lua.callFunction("update", dt);
if (!update_result.success) {
    std::cerr << load_result.error << std::endl;
    g_canvas.clear(enjin2::Pixel4(14)); // error signal on canvas
}
LuaResult draw_result = g_lua.callFunction("draw");
if (!draw_result.success) {
    std::cerr << draw_result.error << std::endl;
    g_canvas.clear(enjin2::Pixel4(14));
}
```

### Pattern 3: CMakeLists.txt — Link enjin2_lua to enjin2_sdl

**What:** Phase 21 deliberately excluded `enjin2_lua` from `enjin2_sdl`. Phase 22 requires adding it.

```cmake
target_link_libraries(enjin2_sdl PRIVATE
    enjin2_core
    enjin2_graphics
    enjin2_input
    enjin2_lua      # NEW in Phase 22
    SDL3::SDL3
)
```

Also add `ENJIN2_BUILD_LUA=1` compile definition to `enjin2_sdl`:
```cmake
target_compile_definitions(enjin2_sdl PRIVATE
    $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>
)
```

And include `enjin2_lua` include dirs so Lua headers are visible:
```cmake
target_include_directories(enjin2_sdl PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<$<BOOL:${ENJIN2_BUILD_LUA}>:${LUA_INCLUDE_DIRS}>
)
```

### Pattern 4: E2E Test Script Structure

**What:** `scripts/e2e_parity.lua` — color grid + input indicator, continuous game loop driven by host calling `update(dt)` and `draw()` each frame.

**Example:**

```lua
-- scripts/e2e_parity.lua
-- E2E parity test: color grid + button-0 input indicator
-- Runs identically on SDL3, WASM, ESP32.

local function draw_color_grid()
    -- 5x3 grid of cells (15 swatches = indices 0-14)
    -- Cell size: 24x24 canvas pixels on a 128x128 canvas
    local COLS = 5
    local CELL_W = 24
    local CELL_H = 24
    for i = 0, 14 do
        local col = i % COLS
        local row = math.floor(i / COLS)
        setColor(i)
        rectangle(col * CELL_W, row * CELL_H, CELL_W, CELL_H)
    end
end

local function draw_input_indicator()
    -- Bottom-right corner cell: color 1 (held) or color 0 (not held)
    local x = getWidth() - 24
    local y = getHeight() - 24
    if isButtonHeld(0) then
        setColor(7)  -- bright color when held
    else
        setColor(2)  -- dim color when not held
    end
    rectangle(x, y, 24, 24)
end

function update(dt)
    -- No state to update in this demo; input is polled per-frame by host
end

function draw()
    clear(0)
    draw_color_grid()
    draw_input_indicator()
end
```

### Anti-Patterns to Avoid

- **Registering `isButtonHeld` before `setInput()` is called in host**: Always guard `currentInput` for null — bindings already use this pattern (see `lua_clear` checking `currentCanvas`).
- **Calling `g_lua.getBindings().setInput()` AFTER `callFunction("update")`**: Must set input pointer AFTER `input_platform_poll` and BEFORE Lua script call, so the Lua script sees the current frame's input.
- **Using `lua_pcall` directly in sdl_main.cpp**: Use `g_lua.callFunction(...)` — it handles pcall internally and returns `LuaResult`.
- **Linking enjin2_lua unconditionally**: Wrap in `$<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>` so SDL builds with `ENJIN2_BUILD_LUA=OFF` still work (no Lua scripting in runner).
- **Using `VCV_RACK` macro in sdl_main.cpp**: The existing Lua platform layer uses `#ifdef VCV_RACK` for desktop Lua. The SDL runner compiles with `VCV_RACK` already defined (via `enjin2_core` and `enjin2_lua` public defines) — so this works without any new `#ifdef`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Lua state creation | Custom allocator or `lua_newstate` | `LuaEngine::initialize()` → `LuaPlatform::createState()` | Already platform-aware; handles VCV_RACK vs ESP32 |
| Script file loading | Direct `fopen`/`ifstream` | `LuaEngine::executeFile()` → `LuaFileSystem::readScriptFile()` | Platform-aware; works on desktop via VCV_RACK |
| Error recovery from Lua | Custom `lua_pcall` wrapper | `LuaEngine::executeString()` / `LuaResult` | Already wraps pcall; returns structured error |
| Calling Lua functions | Manual `lua_getglobal` + `lua_pcall` | `LuaScriptSystem::callFunction("name", args...)` | Variadic template handles arg pushing; returns LuaResult |
| Palette bindings | Re-registering palette functions | Already registered in `LuaBindings::registerAll()` | `setPaletteColor`, `getPaletteColor`, `loadPalette`, `getPaletteSize` are present since Phase 19 |

**Key insight:** The entire Lua infrastructure is already built and tested. Phase 22 is additive only — no new infrastructure, no new patterns.

---

## Common Pitfalls

### Pitfall 1: `setInput()` Called Too Late in Frame
**What goes wrong:** Script reads stale (previous frame) input or zeroed input.
**Why it happens:** `input_advance_frame` zeroes `buttons` at frame start, then `input_platform_poll` writes new state. If `setInput()` is called before `input_platform_poll`, the script gets zeroed buttons.
**How to avoid:** Order in game loop: `input_advance_frame` → `input_platform_poll` → `g_lua.getBindings().setInput(&g_input)` → `callFunction("update")` → `callFunction("draw")`.
**Warning signs:** `isButtonHeld(0)` always returns false even when key is held.

### Pitfall 2: `enjin2_lua` Link Without Lua Headers Visible to `enjin2_sdl`
**What goes wrong:** `sdl_main.cpp` includes `bindings.hpp` which includes `lua_engine.hpp` which includes `lua_platform.hpp` which does `#include "lua.h"` — compile fails if Lua headers are not in the include path.
**Why it happens:** `enjin2_sdl` currently has no `${LUA_INCLUDE_DIRS}` in its `target_include_directories`.
**How to avoid:** Add `${LUA_INCLUDE_DIRS}` (or the equivalent) to `enjin2_sdl`'s include path in CMakeLists.txt. Guard with `$<$<BOOL:${ENJIN2_BUILD_LUA}>:...>` so non-Lua builds are unaffected.
**Warning signs:** `fatal error: lua.h: No such file or directory` during SDL build.

### Pitfall 3: `LuaScriptSystem` Initialized Before SDL
**What goes wrong:** No functional impact — Lua doesn't need SDL. But if order is wrong (SDL init fails and early return skips Lua shutdown), the destructor handles cleanup. Low risk.
**How to avoid:** Initialize Lua after SDL init succeeds and before game loop. Destroy/shutdown at end of main before SDL teardown.

### Pitfall 4: Lua `rectangle()` Uses `"fill"` Mode String
**What goes wrong:** The existing `lua_rectangle` binding interprets first arg as mode string if it's a string. Script must call `rectangle(x, y, w, h)` without a mode string (defaults to "fill"), or call `rectangle("fill", x, y, w, h)`. Inconsistent usage causes silent rendering failures (empty canvas).
**Why it happens:** The binding at lines 363-380 of `bindings.cpp` checks `lua_isstring(L, 1)` to detect mode — script must be consistent.
**How to avoid:** In `e2e_parity.lua`, use `rectangle(x, y, w, h)` (no mode string) — the default is "fill", which is what the E2E test needs. Or explicitly use `rectangle("fill", x, y, w, h)`.

### Pitfall 5: `draw_palette_grid()` in sdl_main.cpp Overlaps Lua Canvas
**What goes wrong:** The existing `draw_palette_grid()` in `sdl_main.cpp` renders a 4x4 swatch grid (32x32 screen pixels = 8x8 canvas pixels each = 32x32 total) directly on the SDL renderer AFTER `SDL_RenderTexture`. If the E2E Lua script draws in the top-left (0,0) area of the canvas, the SDL renderer overlay will cover it.
**Why it happens:** `draw_palette_grid()` is drawn with render scale active at canvas-space coordinates 0,0 to 32,32 (before scale = 0,0 to 128,128 after scale). It covers the exact region where a naive color grid in Lua would draw.
**How to avoid:** Either (a) remove `draw_palette_grid()` call when Lua is active, (b) position the Lua color grid to avoid the top-left 32x32 pixels, or (c) remove the SDL-level debug overlay entirely now that the Lua script provides the color grid. Option (c) is cleanest — the `draw_palette_grid()` was a Phase 21 debug aid, now superseded by the Lua script's own color grid.

### Pitfall 6: `ENJIN2_BUILD_SDL=ON` Without `ENJIN2_BUILD_LUA=ON`
**What goes wrong:** SDL runner builds without Lua — `enjin2_lua` target doesn't exist, CMake errors if unconditionally linked.
**Why it happens:** Both options exist independently.
**How to avoid:** Guard Lua integration in `enjin2_sdl` behind `$<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>` — same pattern as the main `enjin2` interface library. SDL runner should gracefully not link Lua when `ENJIN2_BUILD_LUA=OFF`.

---

## Code Examples

### Existing Pattern: Palette Binding (in bindings.cpp — follow this for input)

```cpp
// Source: src/scripting/bindings.cpp lines 580-621
int LuaBindings::lua_setPaletteColor(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    if (lua_isstring(L, 2)) {
        const char* hex = luaL_checkstring(L, 2);
        uint8_t r = 0, g = 0, b = 0;
        enjin2::parseHexColor(hex, r, g, b);
        enjin2::g_palette.setColor(static_cast<uint8_t>(index), r, g, b);
    } else {
        int r = luaL_checkinteger(L, 2);
        int g = luaL_checkinteger(L, 3);
        int b = luaL_checkinteger(L, 4);
        enjin2::g_palette.setColor(static_cast<uint8_t>(index),
            static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
    }
    return 0;
}
```

### Existing Pattern: InputState Methods (in input_state.hpp — forward these to Lua)

```cpp
// Source: include/enjin2/input/input_state.hpp lines 25-39
inline bool justPressed(int btn) const {
    uint16_t mask = static_cast<uint16_t>(1u << btn);
    return !(prev_buttons & mask) && (buttons & mask);
}
inline bool held(int btn) const {
    return (buttons & static_cast<uint16_t>(1u << btn)) != 0;
}
inline bool justReleased(int btn) const {
    uint16_t mask = static_cast<uint16_t>(1u << btn);
    return (prev_buttons & mask) && !(buttons & mask);
}
// axes[8]: float array, normalized -1.0 to 1.0
```

### Existing Pattern: getBindings() retrieval in static functions

```cpp
// Source: src/scripting/bindings.cpp lines 214-218
LuaBindings* LuaBindings::getBindings(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
    LuaBindings* bindings = static_cast<LuaBindings*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return bindings;
}
```

### Existing Pattern: Frame loop order in sdl_main.cpp

```cpp
// Source: src/platform/sdl/sdl_main.cpp lines 202-214
// Order MUST be: advance first (clears current), then poll (writes current).
enjin2::input_advance_frame(&g_input);
enjin2::input_platform_poll(&g_input);
// --- Render ---
expand_canvas_to_rgb();
SDL_UpdateTexture(texture, nullptr, g_rgb_staging, CANVAS_W * 3);
SDL_RenderClear(renderer);
SDL_RenderTexture(renderer, texture, nullptr, nullptr);
draw_palette_grid(renderer);  // NOTE: Phase 22 should evaluate removing this
SDL_RenderPresent(renderer);
```

### Existing Pattern: LuaEngine error on canvas

The `luaPanic` handler in `lua_engine.cpp` calls `std::abort()` for unrecoverable errors. For recoverable script errors, `executeString`/`executeFile` uses `lua_pcall` and returns `LuaResult`. The host (sdl_main.cpp) must catch `LuaResult::success == false` and paint the error signal.

```cpp
// Recommended error signal pattern for sdl_main.cpp:
// Use palette index 14 (last user color, visually distinct from black/transparent)
// and fill the entire canvas so it's impossible to miss.
if (!result.success) {
    std::cerr << "Lua error: " << result.error << std::endl;
    g_canvas.clear(enjin2::Pixel4(14));  // fill canvas with error color
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `enjin2_sdl` excludes `enjin2_lua` | `enjin2_sdl` links `enjin2_lua` (Phase 22) | Phase 22 | SDL runner now executes Lua scripts |
| `LuaBindings` has no input access | `LuaBindings` holds `InputState*` | Phase 22 | Scripts can poll input state |
| `draw_palette_grid()` is SDL-renderer debug overlay | Color grid is drawn by Lua script into Canvas4 | Phase 22 | Visual parity with WASM (both see identical canvas pixels) |

**Deprecated/outdated:**
- `draw_palette_grid()` in sdl_main.cpp: Was added as a Phase 21 debug aid. Phase 22 supersedes it — the Lua E2E script provides a richer color grid written directly to the canvas. The SDL-level overlay should be removed or gated to avoid covering the Lua-drawn content.

---

## Critical Architecture Decision: Binding File Structure

The CONTEXT.md flags this as a discretion area. Evidence from the codebase:

- **Current state**: All bindings in one file — `src/scripting/bindings.cpp` (~620 lines) and `include/enjin2/scripting/bindings.hpp`. Contains canvas drawing, palette, and utility bindings.
- **Scale concern**: Adding 4 input functions = ~50 lines. File grows to ~670 lines. Not a maintenance problem yet.
- **Recommendation**: Extend the existing file. A per-module split (e.g., `input_lua.cpp`) is the right eventual pattern, but introducing it for 4 functions adds friction without payoff. The correct trigger for splitting is when `bindings.cpp` becomes hard to navigate (300+ lines per domain) or when different domains have different compile requirements. Neither applies here.
- **Future-proofing**: When the split does happen, the pattern is clean: each module file implements its registration function (e.g., `register_input_bindings(LuaEngine*)`) called from `LuaBindings::registerAll()`. No architectural change required — just file organization.

---

## Open Questions

1. **`LuaScriptSystem` as global vs local in `main()`**
   - What we know: `g_canvas`, `g_input` are already static globals in sdl_main.cpp.
   - What's unclear: `LuaScriptSystem` contains `LuaEngine` which uses a static memory pool (`static char memoryPool[]`). Making `LuaScriptSystem` itself a local variable in `main()` vs a static global is equivalent from a lifetime perspective — both live for the program duration.
   - Recommendation: Follow existing `g_canvas` pattern — declare as a static global (`static enjin2::LuaScriptSystem g_lua`). Consistent with file conventions.

2. **Script argument: hardcoded path vs command-line arg**
   - What we know: Phase 21 already has `--fps N` argument parsing in main.
   - What's unclear: Should the script path be hardcoded as `"scripts/e2e_parity.lua"` or passed as a CLI argument?
   - Recommendation: For this phase, hardcode. The phase success criteria is a specific E2E script. CLI argument support is a future enhancement (like `--fps` was a natural extension of the runner). Keep scope tight.

3. **Error signal: fill entire canvas vs fill a region**
   - What we know: CONTEXT.md says "filling the canvas or a region with a 'something broke' color". Index 15 is transparent — effectively invisible as an error signal. A solid non-transparent fill is needed.
   - What's unclear: Which palette index to use as the error color before setPaletteColor has been called (palette is uninitialized state).
   - Recommendation: Use `g_canvas.clear(Pixel4(14))` — index 14 (last user color) renders as the 14th entry of whatever palette is loaded. Alternatively, fill with index 1 (typically a non-black color in any reasonable palette). Claude's discretion; pick index 14 and document it.

---

## Sources

### Primary (HIGH confidence)

- Direct codebase inspection — `src/scripting/bindings.cpp` (620 lines, read in full)
- Direct codebase inspection — `include/enjin2/scripting/bindings.hpp` (read in full)
- Direct codebase inspection — `include/enjin2/input/input_state.hpp` (read in full)
- Direct codebase inspection — `src/platform/sdl/sdl_main.cpp` (230 lines, read in full)
- Direct codebase inspection — `CMakeLists.txt` (280 lines, read in full)
- Direct codebase inspection — `src/scripting/lua_engine.cpp`, `lua_platform.cpp` (read in full)
- `.planning/phases/21-sdl3-cmake-runner/21-VERIFICATION.md` — Phase 21 completion proof

### Secondary (MEDIUM confidence)

- `.planning/STATE.md` — accumulated decisions from Phases 19-21
- `.planning/REQUIREMENTS.md` — requirement definitions for SDL-05 and INP-05

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries are existing project components, no external dependencies to evaluate
- Architecture: HIGH — binding pattern is well-established in bindings.cpp; input wiring follows exact same approach as canvas and palette
- Pitfalls: HIGH — derived from direct reading of sdl_main.cpp and bindings.cpp; specific line references provided
- E2E script design: HIGH — constraints fully specified in CONTEXT.md; Lua APIs available confirmed from bindings.cpp

**Research date:** 2026-02-24
**Valid until:** 2026-04-24 (stable — all findings are codebase-internal, no external library versions to expire)
