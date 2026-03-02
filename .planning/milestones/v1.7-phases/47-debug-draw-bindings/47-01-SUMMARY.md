---
phase: 47-debug-draw-bindings
plan: 01
subsystem: scripting
tags: [lua, bindings, debug-draw, layer-compositor, canvas]

# Dependency graph
requires:
  - phase: 45-optimized-2d-physics-engine
    provides: bindings_physics.cpp sub-table pattern used as model
  - phase: 46-bindings-refactoring
    provides: bindings_internal.hpp, null-safety audit baseline

provides:
  - engine.debug.* Lua sub-table (rect, circle, line, cross, text, setEnabled, getEnabled)
  - ENJIN_LAYER_COUNT incremented to 5 (debug layer at index 4)
  - LAYER_DEBUG=5 global constant
  - m_debugCanvas/m_debugEnabled members on LuaBindings
  - setDebugCanvas()/getDebugCanvas() public API on LuaBindings
  - Zero-cost REQUIRE_DEBUG_CANVAS guard macro
  - g_lua_layer4 debug layer wired in sdl_main.cpp

affects: [48-coroutine-scheduler, 49-tween-engine, scripting, sdl-runner]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "REQUIRE_DEBUG_CANVAS macro: zero-cost guard pattern for conditional canvas access"
    - "Debug layer is layer index 4, excluded from g_lua_layers (not script-setLayer accessible)"
    - "setDebugCanvas() called after each performReload() to re-wire after Lua state teardown"
    - "m_debugEnabled reset to true in registerAll() for clean hot-reload semantics"

key-files:
  created:
    - src/scripting/bindings_debug.cpp
    - tests/debug_draw_test.cpp
  modified:
    - include/enjin2/graphics/layer_compositor.hpp
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - src/scripting/bindings_engine.cpp
    - src/platform/sdl/sdl_main.cpp
    - CMakeLists.txt
    - tests/CMakeLists.txt
    - tests/compositor_test.cpp

key-decisions:
  - "Debug layer (index 4) excluded from g_lua_layers — accessible only via engine.debug.* not setLayer()"
  - "g_lua_layers hard-coded to 4 entries in sdl_main.cpp even though ENJIN_LAYER_COUNT is now 5"
  - "engine.debug.cross() implemented as two drawLine calls (no LuaCanvas cross primitive exists)"
  - "LuaCanvas::drawText signature is (str, x, y, color, size, font) — note size before font"
  - "setDebugCanvas() called after each performReload() — cheap pointer re-assignment for safety"

patterns-established:
  - "Sub-table registration via registerDebugSubtable() member method following camera/physics pattern"
  - "REQUIRE_DEBUG_CANVAS macro for early-return null/disabled guard in all draw functions"

requirements-completed: [DEBUG-01, DEBUG-02, DEBUG-03]

# Metrics
duration: 4min
completed: 2026-03-01
---

# Phase 47 Plan 01: Debug Draw Bindings Summary

**engine.debug.* Lua sub-table routing 7 draw functions to a dedicated 5th compositor layer with zero-cost boolean toggle**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-01T18:52:27Z
- **Completed:** 2026-03-01T18:56:27Z
- **Tasks:** 3
- **Files modified:** 10

## Accomplishments

- ENJIN_LAYER_COUNT incremented to 5, giving a dedicated debug layer (index 4) auto-cleared each frame by clearAll()
- bindings_debug.cpp created with all 7 engine.debug.* functions: rect, circle, line, cross, text, setEnabled, getEnabled
- engine.debug sub-table registered via registerDebugSubtable() called from registerEngineTable()
- LAYER_DEBUG=5 global Lua constant registered, m_debugEnabled resets to true on every hot-reload
- g_lua_layer4 wired in sdl_main.cpp with setDebugCanvas() called after each performReload()
- debug_draw_test.cpp with 5 test cases covering all requirements — all passing

## Task Commits

1. **Task 1: Add debug members to bindings.hpp, increment ENJIN_LAYER_COUNT, wire 5th layer** - `5c91b0f` (feat)
2. **Task 2: Create bindings_debug.cpp, register engine.debug sub-table, add to CMakeLists.txt** - `547bdcb` (feat)
3. **Task 3: Create debug_draw_test.cpp Lua integration test** - `500d17b` (test)

## Files Created/Modified

- `src/scripting/bindings_debug.cpp` - All 7 engine.debug.* static binding functions + registerDebugSubtable()
- `tests/debug_draw_test.cpp` - 5 test cases: table existence, null-canvas safety, toggle, disabled no-op, LAYER_DEBUG constant
- `include/enjin2/graphics/layer_compositor.hpp` - ENJIN_LAYER_COUNT changed from 4 to 5
- `include/enjin2/scripting/bindings.hpp` - m_debugCanvas, m_debugEnabled members; setDebugCanvas()/getDebugCanvas(); 7 static decls; registerDebugSubtable() decl
- `src/scripting/bindings.cpp` - m_debugEnabled=true in registerAll(); LAYER_DEBUG=5 global constant
- `src/scripting/bindings_engine.cpp` - registerDebugSubtable(L) call added before lua_setglobal
- `src/platform/sdl/sdl_main.cpp` - g_lua_layer4 added; g_lua_layers hard-coded to 4; setDebugCanvas() after each performReload()
- `CMakeLists.txt` - src/scripting/bindings_debug.cpp added to enjin2_lua target_sources
- `tests/CMakeLists.txt` - debug_draw_test added inside ENJIN2_BUILD_LUA guard
- `tests/compositor_test.cpp` - static_assert updated from 4 to 5

## Decisions Made

- Debug layer (index 4) intentionally excluded from g_lua_layers — scripts cannot access it via setLayer(); only via engine.debug.* functions, preserving the debug overlay contract
- g_lua_layers array hard-coded to 4 entries in sdl_main.cpp even though ENJIN_LAYER_COUNT is now 5, so setLayers() receives only the 4 game layers
- engine.debug.cross() implemented as two drawLine calls since LuaCanvas has no cross primitive
- LuaCanvas::drawText signature is (str, x, y, color, size, font) — size comes before font; plan snippet had them reversed; used correct order from actual header

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Updated compositor_test.cpp static_assert from 4 to 5**
- **Found during:** Task 1 verification (first build attempt)
- **Issue:** compositor_test.cpp had `static_assert(ENJIN_LAYER_COUNT == 4, ...)` which became a compile error after incrementing to 5
- **Fix:** Updated static_assert and runtime ASSERT to expect 5
- **Files modified:** tests/compositor_test.cpp
- **Verification:** Build succeeds, compositor_test passes
- **Committed in:** 5c91b0f (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug in existing test)
**Impact on plan:** Necessary update — the test was asserting the old layer count. No scope creep.

## Issues Encountered

- timer_test was already failing (pre-existing "corrupted size vs. prev_size" abort) before this plan's changes. Confirmed by stashing all changes and re-running — same failure. Out of scope per deviation rules.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- engine.debug.* fully functional for Lua game scripts to draw debug overlays on layer 4
- Layer 4 auto-cleared each frame, composited last (above all game content)
- setEnabled(false)/setEnabled(true) toggle ready for performance-sensitive code paths
- LAYER_DEBUG=5 constant available for informational use
- Ready for Phase 48 coroutine scheduler implementation

---
*Phase: 47-debug-draw-bindings*
*Completed: 2026-03-01*
