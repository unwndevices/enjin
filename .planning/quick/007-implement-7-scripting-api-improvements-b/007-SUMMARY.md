---
phase: quick-007
plan: 01
subsystem: scripting
tags: [lua, api, bindings, constants, text, state-machine]
dependency_graph:
  requires: []
  provides: [BTN-constants, COLOR-constants, engine-graphics-alias, float-coords, text-scale, textCentered, textAligned, engine-config, engine-state]
  affects: [src/scripting/bindings.cpp, src/scripting/bindings_draw.cpp, src/scripting/bindings_layers_text.cpp, src/scripting/bindings_engine.cpp, include/enjin2/scripting/bindings.hpp]
tech_stack:
  added: [lround from <cmath>]
  patterns: [LuaFuncDef sub-table pattern, Lua registry ref pattern for callbacks, static constexpr fixed-array state machine]
key_files:
  created:
    - tests/scripting_api_test.cpp
  modified:
    - src/scripting/bindings.cpp
    - src/scripting/bindings_draw.cpp
    - src/scripting/bindings_layers_text.cpp
    - src/scripting/bindings_engine.cpp
    - include/enjin2/scripting/bindings.hpp
    - tests/CMakeLists.txt
decisions:
  - "engine.state uses fixed-array (MAX_GAME_STATES=16) with linear scan — zero heap allocation, consistent with C_Timer/C_StateMachine pattern"
  - "engine.state callbacks stored as Lua registry refs (luaL_ref/luaL_unref) — same pattern as C_StateMachine on_enter/on_exit"
  - "text() scale param is per-call override, never mutates currentTextSize — matches plan spec exactly"
  - "lround() used instead of static_cast<int> for float coords — rounds to nearest int, not truncates toward zero"
  - "engine.graphics sub-table built with LuaFuncDef array via luaBindFunctions — consistent with all other engine.* sub-tables"
  - "textCentered/textAligned registered as both globals and engine.graphics members — dual registration matches existing text() pattern"
metrics:
  duration_minutes: 25
  completed_date: "2026-03-01"
  tasks_completed: 2
  files_modified: 6
  tests_added: 44
---

# Quick Task 007: 7 Scripting API Improvements Summary

**One-liner:** BTN/COLOR global constant tables, engine.graphics alias sub-table, lround float coords, optional text scale, textCentered/textAligned, engine.config.resolution(), and engine.state.* lightweight state machine — 44 new tests, 0 regressions.

## What Was Built

### API-01: BTN.* global table
`BTN.UP=0, BTN.DOWN=1, BTN.LEFT=2, BTN.RIGHT=3, BTN.A=4, BTN.B=5, BTN.START=6` registered as a global Lua table in `registerAll()` after LAYER_* constants. Eliminates ~7 lines of boilerplate from every game script.

### API-02: COLOR.* global table
`COLOR.BLACK=0` through `COLOR.TRANSPARENT=15` — full 16-color PICO-8-inspired palette as a global Lua table. Eliminates ~16 lines of boilerplate constant definitions.

### API-03: engine.graphics.* alias sub-table
All 24 drawing/text/canvas functions aliased under `engine.graphics.*` using the standard `LuaFuncDef` array + `luaBindFunctions` pattern. Includes `textCentered` and `textAligned` new in this task.

### API-04: Float-to-int coordinate rounding
Added `#include <cmath>` to `bindings_draw.cpp`. Changed `lua_tointeger` to `lround(lua_tonumber(...))` in `lua_point`, `lua_line`, `lua_rectangle` (x/y only), `lua_circle` (x/y only), `lua_triangle` (all 6 coords). Width/height/radius remain integer.

### API-05: text() optional scale parameter
`text(str, x, y, scale)` — 4th arg is a per-call scale override. Does NOT mutate `currentTextSize`. Backward compatible: 3-arg form unchanged.

### API-06: textCentered() and textAligned()
- `textCentered(str, y [, scale])` — measures text width, computes centered x, draws
- `textAligned(str, x, y, align [, scale])` — "left" (default), "center" (x - tw/2), "right" (x - tw)
Both registered as globals and in engine.graphics sub-table.

### API-07: engine.config.resolution() + engine.state.*
- `engine.config.resolution()` returns canvas width, height (0, 0 if no canvas)
- `engine.state.current()` returns current state name (initially "none")
- `engine.state.switch(name)` fires on_exit callback for previous state, changes state, fires on_enter callback for new state
- `engine.state.on_enter(name, fn)` registers callback for when state becomes active
- `engine.state.on_exit(name, fn)` registers callback for when state is left
State machine uses fixed arrays (MAX_GAME_STATES=16) in LuaBindings — zero heap allocation.

## Test Results

```
scripting_api_test: 44 passed, 0 failed
Full ctest suite: 32/33 passed (sprite_load_test pre-existing missing header, unrelated)
```

## Commits

| Hash | Description |
|------|-------------|
| 69553b6 | test(quick-007): add failing tests for 7 scripting API improvements (RED) |
| f3aee25 | feat(quick-007): implement 7 scripting API improvements (GREEN) |

## Deviations from Plan

None — plan executed exactly as written. The `lua_engine_state_current` implementation was placed alongside `lua_engine_state_switch` in `bindings_engine.cpp` (rather than before) — purely organizational, no behavioral difference.

## Self-Check: PASSED

All key files found. Both implementation commits verified in git log.
- tests/scripting_api_test.cpp: FOUND
- src/scripting/bindings.cpp: FOUND
- src/scripting/bindings_draw.cpp: FOUND
- src/scripting/bindings_layers_text.cpp: FOUND
- src/scripting/bindings_engine.cpp: FOUND
- Commit 69553b6: FOUND
- Commit f3aee25: FOUND
