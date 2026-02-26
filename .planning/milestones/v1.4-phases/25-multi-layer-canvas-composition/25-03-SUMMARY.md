---
phase: 25-multi-layer-canvas-composition
plan: 03
subsystem: scripting
tags: [lua, bindings, layers, compositor, canvas, lua_CFunction, cpp, sdl3]

# Dependency graph
requires:
  - phase: 25-01
    provides: "LayerCompositor<W,H> with ENJIN_LAYER_COUNT=4, visible[] array"
  - phase: 25-02
    provides: "Per-layer LuaCanvas wrappers (g_lua_layers[0..3]) as static locals in sdl_main.cpp"
provides:
  - "Lua layer API: setLayer(n), getLayer(), clearLayer(n, color), getLayerCount(), setLayerVisible(n, bool), isLayerVisible(n)"
  - "Lua global constants LAYER_BG=1, LAYER_MID=2, LAYER_FG=3, LAYER_UI=4"
  - "LuaBindings::setLayers() public method wiring compositor layer pointers into bindings"
  - "scripts/layer_demo.lua exercising the full layer API across all 4 layers"
affects:
  - phase-26 (any Lua scripting that uses multi-layer drawing)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "All layer indices are 1-indexed in Lua; converted to 0-based C++ by subtracting 1 before use"
    - "Out-of-range layer index is silently clamped to [0, layerCount-1] — matches setFrame pattern"
    - "setLayers() sets activeLayer=0 and currentCanvas=layerCanvases[0] as the initial default"
    - "layerVisible points directly into LayerCompositor::visible[] — no copy, no indirection"
    - "lua_CFunction implementations guard on b->layerCount == 0 before dereferencing layerCanvases"

key-files:
  created:
    - scripts/layer_demo.lua
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - src/platform/sdl/sdl_main.cpp

key-decisions:
  - "setLayers() replaces the previous g_lua.setCanvas(g_lua_layers[0]) call in sdl_main.cpp — setLayers() already sets currentCanvas to layerCanvases[0] internally, so the explicit setCanvas call is redundant and removed"
  - "layerVisible stored as bool* pointing to LayerCompositor::visible[] — direct pointer avoids a copy and keeps compositor and bindings visibility state in sync automatically"
  - "lua_clearLayer() accepts optional second argument (color); defaults to 0 (black) via luaL_optinteger if not supplied"

patterns-established:
  - "Lua layer API pattern: lua_idx passed by user, cpp_idx = lua_idx - 1, clamp cpp_idx to [0, layerCount-1]"
  - "setLayer(n) updates both activeLayer and currentCanvas atomically — all subsequent draw calls go to the selected layer"

requirements-completed: [LAYER-06]

# Metrics
duration: 3min
completed: 2026-02-26
---

# Phase 25 Plan 03: Lua Layer API Bindings Summary

**6 lua_CFunction layer bindings (setLayer/getLayer/clearLayer/getLayerCount/setLayerVisible/isLayerVisible) plus LAYER_BG/MID/FG/UI globals registered in LuaBindings, wired to compositor via setLayers() in sdl_main.cpp**

## Performance

- **Duration:** ~3 min
- **Started:** 2026-02-26T11:44:35Z
- **Completed:** 2026-02-26T11:47:12Z
- **Tasks:** 2
- **Files modified:** 4 (3 modified, 1 created new)

## Accomplishments
- Added layer state members (`layerCanvases[8]`, `layerVisible*`, `activeLayer`, `layerCount`) and `setLayers()` public method to `LuaBindings` header
- Implemented 6 lua_CFunction static methods in bindings.cpp: setLayer, getLayer, clearLayer, getLayerCount, setLayerVisible, isLayerVisible
- Registered all 6 functions and 4 global constants (LAYER_BG/MID/FG/UI = 1/2/3/4) in `registerAll()`
- Wired `g_lua.getBindings().setLayers(g_lua_layers, ENJIN_LAYER_COUNT, g_compositor.visible)` in sdl_main.cpp, removing the now-redundant `setCanvas` call
- Created `scripts/layer_demo.lua` drawing distinct content on 4 layers — BG fill, MID rectangle, FG circle, UI header strip

## Task Commits

Each task was committed atomically:

1. **Task 1: Add layer state and 6 lua_CFunction declarations to LuaBindings header** - `6e24b8b` (feat)
2. **Task 2: Implement 6 layer bindings, register globals, wire from sdl_main, create demo** - `411606a` (feat)

## Files Created/Modified
- `include/enjin2/scripting/bindings.hpp` - Layer state members (layerCanvases, layerVisible, activeLayer, layerCount), setLayers() declaration, 6 lua_CFunction private declarations
- `src/scripting/bindings.cpp` - setLayers() implementation, 6 lua_CFunction implementations, registerAll() registrations + 4 LAYER_* global constant pushes
- `src/platform/sdl/sdl_main.cpp` - Replaced g_lua.setCanvas() with g_lua.getBindings().setLayers() wiring all 4 compositor layers
- `scripts/layer_demo.lua` - Demo script drawing BG/MID/FG/UI layers with setLayer, clear, rectangle, circle, getLayerCount, getLayer

## Decisions Made
- `setLayers()` replaces the previous `g_lua.setCanvas(g_lua_layers[0])` call — setLayers already sets `currentCanvas` to `layerCanvases[0]` internally, making the explicit setCanvas call redundant
- `layerVisible` stored as `bool*` pointing into `LayerCompositor::visible[]` directly — no copy, compositor and bindings visibility stay in sync automatically
- `clearLayer()` accepts an optional second color argument (defaults to 0/black via `luaL_optinteger`) for flexibility

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 25 complete: LayerCompositor core (25-01), SDL3 integration (25-02), and Lua layer API (25-03) all done
- Phase 26 (LuaCallback dangling-pointer fix): no dependency on layer API; the layer bindings use lua_CFunction exclusively per the established pattern
- All LAYER-01 through LAYER-06 requirements satisfied

## Self-Check: PASSED

- `include/enjin2/scripting/bindings.hpp` - FOUND
- `src/scripting/bindings.cpp` - FOUND
- `src/platform/sdl/sdl_main.cpp` - FOUND
- `scripts/layer_demo.lua` - FOUND
- Commit 6e24b8b - FOUND
- Commit 411606a - FOUND

---
*Phase: 25-multi-layer-canvas-composition*
*Completed: 2026-02-26*
