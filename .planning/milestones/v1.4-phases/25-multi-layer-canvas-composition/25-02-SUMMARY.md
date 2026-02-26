---
phase: 25-multi-layer-canvas-composition
plan: 02
subsystem: graphics
tags: [compositor, sdl3, canvas, layers, pixel4, cpp, frame-loop]

# Dependency graph
requires:
  - phase: 25-01
    provides: "LayerCompositor<W,H> template with ENJIN_LAYER_COUNT=4, clearAll(), composite(), visible[]"
provides:
  - "SDL3 runner with LayerCompositor replacing g_canvas — multi-layer rendering live in SDL3 window"
  - "Per-layer LuaCanvas wrappers (g_lua_layer0..3) pointing to compositor.layers[0..3]"
  - "Frame loop: clearAll -> input -> Lua update/draw -> composite -> expand -> blit"
  - "expand_canvas_to_rgb reads from g_compositor.output (not per-layer buffer)"
affects:
  - 25-03 (Lua bindings — setLayer()/clearLayer() can swap g_lua.setCanvas to any g_lua_layers[n])

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "g_compositor is a file-scope static LayerCompositor — constructed before main(), used for entire process lifetime"
    - "Per-layer LuaCanvas wrappers declared as static locals inside main() — safe because g_compositor outlives them"
    - "clearAll() precedes Lua draw calls; composite() follows; expand_canvas_to_rgb() reads compositor output"
    - "Error-display clears target layers[0] so layer 0 always shows error colors even with compositor in path"
    - "Active canvas layer does NOT reset between frames — only pixel content is cleared by clearAll()"

key-files:
  created: []
  modified:
    - src/platform/sdl/sdl_main.cpp

key-decisions:
  - "LuaCanvas wrappers declared as static locals inside main() (not global statics) because LuaCanvas has no default constructor — static locals are safe since g_compositor is file-scope and constructed first"
  - "Default active canvas at startup = layers[0] (background, Lua layer 1 per CONTEXT.md 1-indexing convention)"
  - "Frame loop ordering: clearAll() BEFORE Lua draw (not after), composite() AFTER Lua draw — ensures Lua draws to freshly-cleared layers each frame, then compositor merges before blit"

patterns-established:
  - "SDL3 runner clearAll/composite sandwich: g_compositor.clearAll() before Lua calls, g_compositor.composite() immediately after draw(), before expand_canvas_to_rgb()"
  - "Error display routes to layers[0] to ensure visibility through the compositor pipeline"

requirements-completed: [LAYER-05]

# Metrics
duration: 2min
completed: 2026-02-26
---

# Phase 25 Plan 02: SDL3 LayerCompositor Integration Summary

**SDL3 runner rewired to LayerCompositor: g_canvas replaced, per-layer LuaCanvas wrappers created, frame loop uses clearAll/composite sandwich, expand_canvas_to_rgb reads compositor output**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-26T11:40:07Z
- **Completed:** 2026-02-26T11:42:17Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- Replaced `g_canvas` (single Canvas4) with `g_compositor` (LayerCompositor<128,128>) as the sole rendering state
- Created 4 static per-layer LuaCanvas wrappers inside `main()` pointing to `g_compositor.layers[0..3]`
- Rewired frame loop: `clearAll()` before Lua update/draw, `composite()` after draw, `expand_canvas_to_rgb()` reads `g_compositor.output`
- All error-display clears now target `g_compositor.layers[0]` — visible through compositor pipeline
- `enjin2_sdl` builds cleanly with zero errors (SDL+Lua build config)

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace g_canvas with LayerCompositor and rewire LuaCanvas** - `5717716` (feat)

## Files Created/Modified
- `src/platform/sdl/sdl_main.cpp` - LayerCompositor<128,128> replacing single Canvas4; per-layer LuaCanvas wrappers; clearAll/composite frame loop ordering; expand_canvas_to_rgb reads compositor output

## Decisions Made
- LuaCanvas wrappers are `static` locals inside `main()` because `LuaCanvas` has no default constructor, making a plain array impossible. Static locals are safe here since `g_compositor` is a file-scope static constructed before `main()` begins.
- Default active canvas set to `g_lua_layers[0]` (background layer), matching the CONTEXT.md decision that "Default active layer at startup/init = layer 1 (Lua index, i.e., C++ buffer 0, background)".
- Frame loop ordering: `clearAll()` is called immediately after input poll and before Lua calls. `composite()` is called immediately after all Lua calls and before `expand_canvas_to_rgb()`. This ensures each frame starts with clean layers and the blit always sees a fully-composited output.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Plan 03 (Lua bindings): `g_lua_layers[0..3]` array is wired to `g_compositor.layers[0..3]`. Plan 03 can add `setLayer(n)` by calling `g_lua.setCanvas(g_lua_layers[n-1])` — the infrastructure is in place.
- No blockers — SDL3 runner will render all 4 compositor layers to screen on next frame.

## Self-Check: PASSED

- src/platform/sdl/sdl_main.cpp — FOUND
- 25-02-SUMMARY.md — FOUND
- Commit 5717716 — FOUND

---
*Phase: 25-multi-layer-canvas-composition*
*Completed: 2026-02-26*
