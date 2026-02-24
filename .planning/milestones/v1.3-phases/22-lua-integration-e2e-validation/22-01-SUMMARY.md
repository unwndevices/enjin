---
phase: 22-lua-integration-e2e-validation
plan: "01"
subsystem: scripting/input
tags: [lua, input, bindings, e2e, parity]
dependency_graph:
  requires:
    - 20-01 (InputState struct, input_state.hpp)
    - 21-02 (SDL3 runner with input_platform_poll)
  provides:
    - Lua-callable input polling API (isButtonHeld, isButtonJustPressed, isButtonJustReleased, getAxis)
    - E2E parity test script (scripts/e2e_parity.lua)
  affects:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - scripts/e2e_parity.lua
tech_stack:
  added: []
  patterns:
    - null-guard pattern for InputState* matches existing currentCanvas null-guard in LuaBindings
    - static binding functions retrieve LuaBindings* via getBindings(L) from LUA_REGISTRYINDEX
key_files:
  created:
    - scripts/e2e_parity.lua
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
decisions:
  - "InputState* currentInput initialized to nullptr in constructor — mirrors currentCanvas pattern"
  - "Null guard on currentInput returns 0/false before btn/axis access — no crash when host omits setInput()"
  - "lua_getAxis bounds-checks axis index (0-7) before dereferencing axes[] array"
metrics:
  duration: "~2 minutes"
  completed: 2026-02-24
  tasks_completed: 2
  files_changed: 3
---

# Phase 22 Plan 01: Lua Input Bindings + E2E Parity Script Summary

**One-liner:** Lua input polling API (isButtonHeld/isButtonJustPressed/isButtonJustReleased/getAxis) added to LuaBindings with null-guard safety, plus e2e_parity.lua exercising all 15 palette indices and live button/axis indicators.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Extend LuaBindings with InputState* and four input binding functions | 4043f2c | bindings.hpp, bindings.cpp |
| 2 | Author scripts/e2e_parity.lua color grid and input indicator | 9b1440d | scripts/e2e_parity.lua |

## What Was Built

### Task 1 — LuaBindings input polling extension

`include/enjin2/scripting/bindings.hpp` gained:
- `#include "../input/input_state.hpp"` — zero-platform header, safe in all build targets
- `InputState* currentInput` private member (initialized to `nullptr`)
- `void setInput(InputState* input)` public method declaration

`src/scripting/bindings.cpp` gained:
- `currentInput(nullptr)` in constructor initializer list
- `setInput()` implementation
- Four registrations in `registerAll()`: `isButtonHeld`, `isButtonJustPressed`, `isButtonJustReleased`, `getAxis`
- Four static implementations, each with `!b || !b->currentInput` null guard returning 0/false safely

### Task 2 — E2E parity test script

`scripts/e2e_parity.lua` provides:
- 5x3 color grid of all 15 palette indices (0–14), each cell 24x24 px, occupying top-left 120x72 area
- Button-0 indicator (bottom-right cell): color 7 (bright) when `isButtonHeld(0)`, color 2 (dim) otherwise
- Axis-0 indicator (one cell left of button): color 10 (active) when `math.abs(getAxis(0)) > 0.1`, color 1 (idle) otherwise
- `update(dt)` and `draw()` globals per host engine contract
- Uses only bound APIs: `setColor`, `rectangle`, `clear`, `getWidth`, `getHeight`, `isButtonHeld`, `getAxis`

## Verification Results

- `cmake --build build_22_check --target enjin2_lua` — **PASSED** (100% Built target enjin2_lua)
- `grep "setInput" bindings.hpp` — **PASSED**
- All four lua_isButton*/lua_getAxis implementations present in bindings.cpp — **PASSED**
- `scripts/e2e_parity.lua` exists with `isButtonHeld` (1 match) and `getAxis` (2 matches) — **PASSED**

## Deviations from Plan

None - plan executed exactly as written.

## Self-Check: PASSED

- `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — FOUND, contains `InputState* currentInput` and `setInput()` declaration
- `/home/unwn/dev/enjin/src/scripting/bindings.cpp` — FOUND, contains all four input binding implementations
- `/home/unwn/dev/enjin/scripts/e2e_parity.lua` — FOUND, contains `update()`, `draw()`, `isButtonHeld(0)`, `getAxis(0)`
- Commit `4043f2c` — FOUND (feat(22-01): extend LuaBindings)
- Commit `9b1440d` — FOUND (feat(22-01): author scripts/e2e_parity.lua)
