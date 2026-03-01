# Phase 47: Debug Draw Bindings - Research

**Researched:** 2026-03-01
**Domain:** Lua binding sub-table, layer routing, zero-cost toggle pattern (C++/Lua)
**Confidence:** HIGH

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DEBUG-01 | engine.debug.rect/circle/line/cross draw bindings route to dedicated debug canvas | Layer system is fully in place; adding a 5th logical "debug" canvas pointer in LuaBindings follows the established layerCanvases[] pattern |
| DEBUG-02 | engine.debug.text overlay binding for debug text display | LuaCanvas::drawText() already exists and works identically to lua_text(); the debug canvas is just a different target |
| DEBUG-03 | engine.debug.enabled boolean toggle (zero cost when disabled) | Early-return guard at the top of every static binding function (check m_debugEnabled before touching the canvas) provides zero overhead when false |
</phase_requirements>

## Summary

Phase 47 adds a `engine.debug.*` Lua sub-table that routes debug draw calls to a dedicated top-layer `LuaCanvas`. The implementation lives entirely in a new `src/scripting/bindings_debug.cpp` file following the split-bindings pattern established in Phase 46.

The layer system is already complete: `LayerCompositor` holds 4 `Canvas4` layers (LAYER_BG=1 … LAYER_UI=4, 0-indexed as 0..3). The debug canvas must sit above all existing layers. Since `ENJIN_LAYER_COUNT = 4` and the compositor allocates exactly 4 layer buffers, the debug canvas cannot occupy a 5th compositor slot. The correct approach is to designate `layerCanvases[3]` (LAYER_UI, the current top) as the debug layer, OR to add a 5th layer by incrementing `ENJIN_LAYER_COUNT` to 5. Both options are viable; the decision analysis is in the Architecture Patterns section below. Based on the Phase 47 roadmap description ("dedicated top-layer debug canvas"), a 5th dedicated debug layer is the correct interpretation.

The `enabled` toggle must have **zero per-frame cost** when false. The pattern used in this codebase for conditional short-circuits is an early `return 0` guard in the static binding function body — identical to the `REQUIRE_CANVAS` macro pattern in `bindings_draw.cpp`. No Lua table replacement, metamethods, or coroutine overhead is needed.

**Primary recommendation:** Add `m_debugCanvas` pointer and `m_debugEnabled` bool to `LuaBindings`; in `registerEngineTable()` build the `engine.debug` sub-table using `luaBindFunctions`; increment `ENJIN_LAYER_COUNT` to 5 in `layer_compositor.hpp`, add `g_lua_layer4` in `sdl_main.cpp`, and pass it to `setDebugCanvas()`. Auto-clear the debug layer inside `clearAll()` (which SDL runner already calls at the start of each frame).

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua C API (PUC-Lua) | 5.1/5.4 (system) | Binding registration, table construction | Already the entire engine scripting stack |
| LuaCanvas | project-internal | Type-erased wrapper around Canvas4/Canvas8 | Existing abstraction for all drawing |
| LayerCompositor | project-internal | Multi-layer Canvas4 compositor | Provides the canvas buffer that debug draws into |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| bind_helpers.hpp (LuaFuncDef + luaBindFunctions) | project-internal | Registering static C functions into a Lua table | Used by all bindings_*.cpp files since Phase 45 |
| bindings_internal.hpp (after Phase 46) | project-internal | Shared forward declarations across split bindings files | Required if bindings_debug.cpp needs to access LuaBindings private members via friend or accessor pattern |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Dedicated 5th compositor layer | Reuse LAYER_UI (layer index 3) for debug | Reusing LAYER_UI means scripts cannot simultaneously use the UI layer AND debug layer — breaks the layer routing contract; dedicated 5th layer is correct |
| Member bool `m_debugEnabled` | Lua global or registry key | Registry lookup is slower than a direct member bool check; member bool is zero-overhead |
| Static C functions (current pattern) | Lua upvalues / closures | The existing codebase uses exclusively static C functions retrieving LuaBindings via `getBindings(L)`; stay consistent |

**Installation:** No new libraries. CMakeLists.txt changes only: add `src/scripting/bindings_debug.cpp` to `enjin2_lua` target_sources, and add `g_lua_layer4` in `sdl_main.cpp`.

## Architecture Patterns

### Recommended Project Structure

```
src/scripting/
├── bindings.cpp                  # registerAll(), layer constants, global state
├── bindings_draw.cpp             # line/rect/circle/pixel/palette — currentCanvas
├── bindings_engine.cpp           # engine.* table registration
├── bindings_debug.cpp            # NEW: engine.debug.* sub-table (this phase)
├── bindings_layers_text.cpp      # setLayer/getLayer/text bindings
└── ...
```

### Pattern 1: engine.debug Sub-Table Registration (in registerEngineTable)

**What:** Build the `engine.debug` table the same way `engine.camera` and `engine.physics` are built in `bindings_engine.cpp`.
**When to use:** At `registerEngineTable()` time, exactly once per `registerAll()` call.

```cpp
// Source: bindings_engine.cpp pattern (engine.camera sub-table, line 131)
// In registerEngineTable() inside bindings_engine.cpp OR bindings_debug.cpp
// called from registerEngineTable():

static const LuaFuncDef kDebugFuncs[] = {
    {"rect",   lua_engine_debug_rect},
    {"circle", lua_engine_debug_circle},
    {"line",   lua_engine_debug_line},
    {"cross",  lua_engine_debug_cross},
    {"text",   lua_engine_debug_text},
};
lua_newtable(L);
luaBindFunctions(L, -1, kDebugFuncs, ENJIN_ARRAY_LEN(kDebugFuncs));
// engine.debug.enabled is a regular boolean field, not a function.
// Set it to true as the default.
lua_pushboolean(L, 1);
lua_setfield(L, -2, "enabled");
lua_setfield(L, -2, "debug");    // engine.debug = { ... }
```

**IMPORTANT NOTE:** The `enabled` field is a plain Lua boolean on the table. The C binding functions read it by fetching `engine.debug.enabled` from the Lua global at call-time, OR store it as `m_debugEnabled` in `LuaBindings` and expose it via `__newindex`. The simpler path that requires no metamethods is: each binding function calls `LuaBindings::getBindings(L)`, checks `b->m_debugEnabled`, and returns 0 immediately if false. The Lua script sets `engine.debug.enabled = false` which writes the Lua table field but does NOT automatically propagate to C. To propagate, either:

1. **Read from Lua table each call** — `lua_getglobal(L, "engine"); lua_getfield(L, -1, "debug"); lua_getfield(L, -1, "enabled"); bool en = lua_toboolean(L, -1); lua_pop(L, 3);` — costs 3 Lua stack ops per call, non-trivial overhead.

2. **Use `m_debugEnabled` member + __newindex metamethod on engine.debug** — metamethod intercepts `engine.debug.enabled = false` and updates `m_debugEnabled`. More machinery but zero call-site cost.

3. **Simplest: expose `setDebugEnabled(bool)` function instead** — `engine.debug.setEnabled(false)`. Avoids metamethods entirely. Easier to implement, but departs slightly from the `engine.debug.enabled = false` API spec.

**Recommendation:** Use approach (1) but cache via registry. Store a `bool* m_debugEnabled` pointer-to-member in the Lua registry key `"enjin_debug_enabled"` as a lightuserdata (same pattern as `enjin_time`), and read it from C. The Lua API is `engine.debug.setEnabled(false)` / `engine.debug.getEnabled()` functions — the roadmap says "boolean toggle" which a function pair satisfies. Alternatively the roadmap plan description says `engine.debug.enabled` as a field, so implement it as a `__newindex` intercept on a metatable for the debug sub-table. See Pattern 2 below.

### Pattern 2: engine.debug Table with __newindex for enabled Toggle

**What:** Give the `engine.debug` table a metatable whose `__newindex` catches writes to `"enabled"` and updates `b->m_debugEnabled`.
**When to use:** When the API spec requires `engine.debug.enabled = false` syntax (field assignment, not a function call).

```cpp
// Create engine.debug table
lua_newtable(L);                                 // [debug_table]
luaBindFunctions(L, -1, kDebugFuncs, ENJIN_ARRAY_LEN(kDebugFuncs));
lua_pushboolean(L, 1);
lua_setfield(L, -2, "enabled");                  // default true stored in table

// Create and attach metatable with __newindex
lua_newtable(L);                                 // [debug_table, meta]
lua_pushcfunction(L, lua_engine_debug_newindex);
lua_setfield(L, -2, "__newindex");
lua_setmetatable(L, -2);                         // attach meta to debug_table
lua_setfield(L, -2, "debug");                    // engine.debug = debug_table
```

```cpp
// __newindex intercept for engine.debug.enabled
static int lua_engine_debug_newindex(lua_State* L) {
    // Stack: [1]=table, [2]=key, [3]=value
    const char* key = lua_tostring(L, 2);
    if (key && strcmp(key, "enabled") == 0) {
        LuaBindings* b = LuaBindings::getBindings(L);
        if (b) b->m_debugEnabled = (lua_toboolean(L, 3) != 0);
        // Also write into the raw table so reads still work
        lua_rawset(L, 1);
        return 0;
    }
    lua_rawset(L, 1);  // passthrough for other keys
    return 0;
}
```

### Pattern 3: Cross Primitive (Two Lines)

**What:** `engine.debug.cross(x, y, size, color)` — draws a "+" crosshair. No `drawCross` exists in `LuaCanvas`; implement as two `drawLine` calls.
**When to use:** Always — this is the only way to draw a cross with the existing canvas API.

```cpp
static int lua_engine_debug_cross(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b || !b->m_debugEnabled || !b->m_debugCanvas) return 0;
    int16_t x    = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t y    = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t sz  = static_cast<uint16_t>(luaL_optinteger(L, 3, 4));
    uint8_t col  = static_cast<uint8_t>(luaL_optinteger(L, 4, 8));  // default red
    b->m_debugCanvas->drawLine(x - sz, y,      x + sz, y,      col);
    b->m_debugCanvas->drawLine(x,      y - sz, x,      y + sz, col);
    return 0;
}
```

### Pattern 4: Debug Canvas Wiring in sdl_main.cpp

**What:** Add a 5th `LuaCanvas` wrapper for the debug layer and pass it to `LuaBindings::setDebugCanvas()`.
**When to use:** At SDL runner init, identical to how layer 0-3 are wired.

```cpp
// In sdl_main.cpp, after incrementing ENJIN_LAYER_COUNT to 5:
static enjin2::LuaCanvas g_lua_layer4(&g_compositor.layers[4]);  // debug layer

// In performReload():
lua.getBindings().setDebugCanvas(&g_lua_layer4);
```

The debug layer must be cleared before each frame's draw pass. Since `g_compositor.clearAll()` already clears layers 1..N-1 to transparent (Pixel4(15)), and the debug layer is layer index 4 (= 0-indexed), it gets cleared automatically — no extra code needed in sdl_main.cpp.

### Pattern 5: setDebugCanvas() API on LuaBindings

**What:** New public method to inject the debug canvas pointer, mirroring `setCanvas()` and `setLayers()`.
**When to use:** Called from `performReload()` alongside `setLayers()`.

```cpp
// In bindings.hpp (public section):
void setDebugCanvas(LuaCanvas* canvas) { m_debugCanvas = canvas; }

// New private members in LuaBindings:
LuaCanvas* m_debugCanvas{nullptr};
bool       m_debugEnabled{true};
```

### Pattern 6: registerAll() Reset for Debug State

**What:** Reset `m_debugEnabled = true` and leave `m_debugCanvas` pointing to the injected canvas (it is stable across reloads) in `registerAll()`.
**When to use:** In `registerAll()` reset block, alongside the existing game-state reset.

```cpp
// In registerAll():
m_debugEnabled = true;  // re-enable debug draw on every hot-reload
// m_debugCanvas is NOT reset — it is injected by host and survives reloads
```

### Anti-Patterns to Avoid

- **Storing debug canvas index instead of pointer:** The debug layer's `LuaCanvas*` pointer is stable (static allocation in sdl_main.cpp); store the pointer, not an index into `layerCanvases[]`, to avoid off-by-one errors after the layer array grows.
- **Checking `engine.debug.enabled` from Lua side in every binding call:** This requires 3 Lua stack ops per call. Use `m_debugEnabled` C++ member with `__newindex` intercept or a setter function.
- **Forgetting to reset `m_debugEnabled` to `true` in registerAll():** Scripts that set `enabled = false` during a session would carry the disabled state into the next hot-reload if not reset.
- **Not registering `engine.debug` sub-table in `registerEngineTable()`:** The sub-table must be added inside `registerEngineTable()` (called from `registerAll()`). Adding it directly in `registerAll()` after `registerEngineTable()` would require re-fetching the `engine` global, which is error-prone.
- **Incrementing ENJIN_LAYER_COUNT without updating sdl_main.cpp static layer array:** Both `ENJIN_LAYER_COUNT` in `layer_compositor.hpp` and the `g_lua_layers[]` array in `sdl_main.cpp` must be kept in sync.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Debug sub-table registration | Custom table-building loop | `luaBindFunctions` + `LuaFuncDef` array | Already the project-standard pattern (used for every sub-table since Phase 44) |
| Drawing to debug canvas | Duplicated canvas drawing logic | `LuaCanvas::drawLine/drawRect/drawCircle/fillCircle/drawText` | LuaCanvas already wraps all primitives including 4-bit/8-bit transparency |
| Zero-cost disabled check | Lua metamethods or proxy tables | C++ `m_debugEnabled` bool member + early `return 0` | Single bool check in C, no Lua GC pressure |
| Cross/plus-sign shape | New Canvas primitive | Two `drawLine` calls | LuaCanvas has no cross primitive; two lines is both correct and sufficient |
| Auto-clear of debug layer | Manual `engine.debug.clear()` in scripts | `LayerCompositor::clearAll()` (called by SDL runner each frame) | clearAll() already clears upper layers to transparent; debug layer is cleared for free |

**Key insight:** The debug system reuses the entire existing primitive drawing stack — no new canvas operations need to be implemented. The entire feature is a thin routing layer from a new Lua API to existing C++ canvas methods, with a single boolean guard.

## Common Pitfalls

### Pitfall 1: ENJIN_LAYER_COUNT Sync
**What goes wrong:** Incrementing `ENJIN_LAYER_COUNT` in `layer_compositor.hpp` to 5 without adding the matching `g_lua_layer4` and updating `g_lua_layers[]` in `sdl_main.cpp` causes an out-of-bounds layer canvas pointer (the 5th slot remains null) and silent drawing failure.
**Why it happens:** The layer array in sdl_main.cpp is manually sized to `ENJIN_LAYER_COUNT` at the call site but the static `LuaCanvas` wrappers are explicitly named.
**How to avoid:** Search for all usages of `ENJIN_LAYER_COUNT` and `g_lua_layer` before incrementing. The count is used in three places: `layer_compositor.hpp` (the definition), `sdl_main.cpp` (the static layer array), and the `performReload` call. The `MAX_LUA_LAYERS = 8` ceiling in `bindings.hpp` comfortably supports 5 layers.
**Warning signs:** `m_debugCanvas` is non-null but draws nothing; `layerCanvases[4]` is null.

### Pitfall 2: engine.debug.enabled = false Not Propagating to C
**What goes wrong:** Script writes `engine.debug.enabled = false` but debug shapes still appear because the C binding functions only check `m_debugEnabled`, which was never updated.
**Why it happens:** Without a `__newindex` metamethod or a setter function, Lua table writes do not automatically update C++ struct fields.
**How to avoid:** Choose one of: (a) `__newindex` metamethod on the debug sub-table, or (b) expose `engine.debug.setEnabled(bool)` as a function instead of a field. Document whichever is chosen clearly.
**Warning signs:** `engine.debug.enabled = false` in Lua does not suppress drawing.

### Pitfall 3: LAYER_DEBUG Constant Missing
**What goes wrong:** Scripts that want to switch to the debug layer with `setLayer(LAYER_DEBUG)` fail because the constant is not registered.
**Why it happens:** Layer globals `LAYER_BG=1…LAYER_UI=4` are registered in `bindings.cpp:registerAll()` for the 4-layer configuration. If a 5th layer is added, `LAYER_DEBUG=5` must also be registered.
**How to avoid:** Add `lua_pushinteger(L, 5); lua_setglobal(L, "LAYER_DEBUG");` in `registerAll()` alongside the existing LAYER_* constants. However, note that scripts should NOT normally draw to the debug layer via `setLayer()` — the `engine.debug.*` API is the intended access path. The constant is informational only.
**Warning signs:** `setLayer(5)` silently clamps to layer 4 (LAYER_UI) because `clampLayerIdx` clamps to `layerCount - 1`.

### Pitfall 4: Debug Layer Not Registered in setLayers() Call
**What goes wrong:** The debug canvas pointer is set via `setDebugCanvas()` but is not included in the `setLayers()` call, so `layerCount` remains 4 and `layerCanvases[4]` remains null.
**Why it happens:** `setLayers()` and `setDebugCanvas()` are separate injection paths. The debug canvas is intentionally not part of the `layerCanvases[]` array (it is a dedicated pointer).
**How to avoid:** `setDebugCanvas()` stores its pointer in `m_debugCanvas`, separate from `layerCanvases[]`. No change to `setLayers()` is needed. Verify that `m_debugCanvas != nullptr` before drawing.
**Warning signs:** `m_debugCanvas` is always null even after wiring; compiler may not warn because null check in binding function silently returns 0.

### Pitfall 5: registerAll() Does Not Reset m_debugEnabled
**What goes wrong:** A script session that ends with `engine.debug.enabled = false` leaves `m_debugEnabled = false` in the `LuaBindings` object. On F5 hot-reload, `registerAll()` runs but if it does not reset `m_debugEnabled = true`, the new script session starts with debug draws silently suppressed.
**Why it happens:** Hot-reload calls `lua.shutdown()` followed by `lua.initialize()` then `performReload()`. The `LuaBindings` instance is persistent across reloads (it is a member of `LuaScriptSystem`). Only `registerAll()` resets per-reload state.
**How to avoid:** Add `m_debugEnabled = true;` to the reset block at the start of `registerAll()`.
**Warning signs:** Debug shapes stop appearing after a reload even when no script explicitly disables debug.

## Code Examples

Verified patterns from official sources (project codebase):

### engine.debug.rect binding function
```cpp
// Source: bindings_draw.cpp REQUIRE_CANVAS macro pattern + bindings_engine.cpp sub-table pattern
static int lua_engine_debug_rect(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b || !b->m_debugEnabled || !b->m_debugCanvas) return 0;
    int16_t x  = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t y  = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    uint8_t col= static_cast<uint8_t>(luaL_optinteger(L, 5, 8));  // default red
    b->m_debugCanvas->drawRect(x, y, w, h, col);
    return 0;
}
```

### engine.debug sub-table construction inside registerEngineTable()
```cpp
// Source: bindings_engine.cpp lines 131-143 (engine.camera pattern)
static const LuaFuncDef kDebugFuncs[] = {
    {"rect",      lua_engine_debug_rect},
    {"circle",    lua_engine_debug_circle},
    {"line",      lua_engine_debug_line},
    {"cross",     lua_engine_debug_cross},
    {"text",      lua_engine_debug_text},
    {"setEnabled",lua_engine_debug_setEnabled},
    {"getEnabled",lua_engine_debug_getEnabled},
};
lua_newtable(L);
luaBindFunctions(L, -1, kDebugFuncs, ENJIN_ARRAY_LEN(kDebugFuncs));
lua_setfield(L, -2, "debug");
```

### LuaBindings header additions
```cpp
// In bindings.hpp (private section, after m_activeCamera):
LuaCanvas* m_debugCanvas{nullptr};   ///< Non-owning; top debug layer, set by host
bool       m_debugEnabled{true};     ///< engine.debug.enabled toggle — true by default

// In bindings.hpp (public section):
void setDebugCanvas(LuaCanvas* canvas) { m_debugCanvas = canvas; }
```

### sdl_main.cpp additions (5th layer)
```cpp
// After existing 4 layer wrappers:
static enjin2::LuaCanvas g_lua_layer4(&g_compositor.layers[4]);

// In performReload():
lua.getBindings().setDebugCanvas(&g_lua_layer4);
```

### layer_compositor.hpp change
```cpp
// Change ENJIN_LAYER_COUNT from 4 to 5:
constexpr uint8_t ENJIN_LAYER_COUNT = 5;
```

### LAYER_DEBUG constant registration in registerAll()
```cpp
// After existing LAYER_UI registration in bindings.cpp:
lua_pushinteger(L, 5); lua_setglobal(L, "LAYER_DEBUG");
```

### Lua usage from a script
```lua
function draw(self)
    -- Normal game rendering on layers 1-4
    setLayer(LAYER_BG)
    -- ... game content ...

    -- Debug overlay (suppressed with zero cost when disabled)
    engine.debug.rect(player.x, player.y, 8, 8, COLOR.RED)      -- hitbox
    engine.debug.circle(player.x, player.y, 4, COLOR.YELLOW)    -- center
    engine.debug.cross(player.x, player.y, 3, COLOR.WHITE)      -- position marker
    engine.debug.line(player.x, player.y, target.x, target.y, COLOR.GREEN)
    engine.debug.text("hp:" .. player.hp, 2, 2, COLOR.WHITE)
end

-- Disable debug draw (zero per-frame cost):
engine.debug.setEnabled(false)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Monolithic `bindings.cpp` | Split `bindings_*.cpp` files | Phase 46 | New bindings_debug.cpp follows the split pattern; no code goes back into bindings.cpp |
| 4 layers (BG/MID/FG/UI) | 5 layers (+DEBUG) | Phase 47 | `ENJIN_LAYER_COUNT` grows to 5; sdl_main.cpp adds `g_lua_layer4`; clearAll() automatically handles the 5th layer |

**No deprecated approaches:** The LuaFuncDef/luaBindFunctions pattern is the current standard and must be used.

## Open Questions

1. **API design for `enabled`: field vs function**
   - What we know: The roadmap specifies `engine.debug.enabled = false` (field assignment syntax). Implementing this requires `__newindex` metamethod.
   - What's unclear: Whether the added complexity of a debug-table metamethod is acceptable given Phase 46 is the first split-bindings phase (not yet shipped) and the metatable machinery would need to be carefully tested.
   - Recommendation: Use `engine.debug.setEnabled(false)` / `engine.debug.getEnabled()` as functions for simplicity, and document why. Alternatively, implement the `__newindex` intercept — it is a small function (~10 lines) with a clear pattern.

2. **Should `ENJIN_LAYER_COUNT` go to 5, or should the debug canvas be a separate non-compositor buffer?**
   - What we know: The compositor only composites `layers[0..ENJIN_LAYER_COUNT-1]`. A separate buffer outside the compositor would not be automatically composited — it would need a manual blit step in sdl_main.cpp.
   - What's unclear: Whether the Phase 25 "ESP32 PSRAM availability for 4-layer stack" concern (noted in STATE.md) would be worsened by adding a 5th layer. A 128x128 Canvas4 = 8192 bytes = 8 KB per layer. A 5th layer = +8 KB.
   - Recommendation: Increment `ENJIN_LAYER_COUNT` to 5. The 8 KB increment is well within desktop and PSRAM margins. If ESP32 heap is tight, the debug canvas can be disabled via a compile flag, but this is a future concern.

3. **Should `engine.debug.text` respect the current font/textSize, or use a fixed debug font?**
   - What we know: `LuaCanvas::drawText` accepts font and size parameters. `LuaBindings` tracks `currentFont` and `currentTextSize`.
   - What's unclear: Should debug text always use the default font and size 1 (safe, predictable), or should it inherit the script's current text state?
   - Recommendation: Use a fixed default (font=nullptr, size=1) for debug text to avoid state dependency. Accept an optional color parameter only.

## Sources

### Primary (HIGH confidence)
- Project source: `src/scripting/bindings_engine.cpp` — sub-table construction pattern (engine.camera, lines 131-143)
- Project source: `src/scripting/bindings_draw.cpp` — `REQUIRE_CANVAS` early-return pattern
- Project source: `src/platform/sdl/sdl_main.cpp` — layer wiring in performReload(), clearAll() auto-clear
- Project source: `include/enjin2/graphics/layer_compositor.hpp` — `ENJIN_LAYER_COUNT=4`, clearAll() behavior
- Project source: `include/enjin2/scripting/bindings.hpp` — LuaBindings struct, layer members, MAX_LUA_LAYERS=8
- Project source: `src/scripting/bindings.cpp` — registerAll() reset pattern, LAYER_* constants registration, getBindings()
- Project source: `include/enjin2/scripting/bind_helpers.hpp` — `LuaFuncDef`, `luaBindFunctions`, `ENJIN_ARRAY_LEN`

### Secondary (MEDIUM confidence)
- Project source: `src/scripting/bindings_layers_text.cpp` — `lua_text()` as model for `lua_engine_debug_text()`
- Project planning: `.planning/ROADMAP.md` Phase 47 description — "dedicated top-layer debug canvas", "zero cost when disabled"

### Tertiary (LOW confidence)
- None — all findings are from direct project source inspection.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries are already in the project; no new dependencies
- Architecture: HIGH — patterns verified from existing bindings_engine.cpp, bindings_draw.cpp, sdl_main.cpp
- Pitfalls: HIGH — derived from direct inspection of layer wiring code and the registerAll() reset pattern

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable internal codebase; only invalidated by Phase 46 completion changes to bindings_internal.hpp)
