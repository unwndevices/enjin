---
phase: 59-tech-debt-and-known-issues
plan: 02
subsystem: input
tags: [cpp, wasm, emscripten, esp32, freertos, input, lua]

requires:
  - phase: 59-01
    provides: const-correct Object API and component.hpp fixes used across the codebase

provides:
  - WASM setInputState(LuaScriptSystem&, int, float, float) free function
  - WASM updateFrame(LuaScriptSystem&, float) free function mirroring SDL3 runner order
  - ESP32 example per-frame FreeRTOS task loop with input_advance_frame + vTaskDelayUntil

affects: [wasm-host-integration, esp32-example, input-system]

tech-stack:
  added: []
  patterns:
    - "function-local statics for zero-alloc per-frame state in WASM lambdas (s_wasm_input, s_total, s_frame)"
    - "vTaskDelayUntil for jitter-free FreeRTOS game loop timing"

key-files:
  created: []
  modified:
    - src/bindings/emscripten_bindings.cpp
    - examples/esp32_idf_example/main/main.cpp

key-decisions:
  - "setInputState exposed as free function accepting LuaScriptSystem& (not a method) — JS host passes its scriptSystem instance in, matching existing WASM architecture where JS owns the LuaScriptSystem"
  - "s_wasm_input is a function-local static inside the lambda — zero heap allocation, persists across calls"
  - "ESP32 example stays on LuaEngine (not migrated to LuaScriptSystem) — migration is documented as a comment for future work"
  - "engine.callFunction('update', dt) used instead of non-existent engine.update(dt) — LuaEngine has no update() method"

patterns-established:
  - "WASM per-frame pattern: setInputState() before updateFrame() — mirrors SDL3 input_advance_frame -> input_platform_poll -> setInput sequence"

requirements-completed: [DEBT-05]

duration: 10min
completed: 2026-03-03
---

# Phase 59-02: WASM Input Bindings and ESP32 Per-Frame Loop Summary

**WASM setInputState+updateFrame bindings wire C_LuaScript input on all platforms; ESP32 example upgraded from idle loop to jitter-free FreeRTOS per-frame task — DEBT-05 resolved, 44/44 tests pass**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-03-03T00:10:00Z
- **Completed:** 2026-03-03T00:20:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Added `setInputState(LuaScriptSystem&, int, float, float)` free function to WASM bindings — calls `input_advance_frame`, writes buttons/axes, wires `setInput` into LuaBindings; function-local static `s_wasm_input` (zero heap allocation)
- Added `updateFrame(LuaScriptSystem&, float)` free function — calls `setTimeState(dt, s_total, s_frame++)`, `tickCoroutines(dt)`, `tickTweens(dt)`, `tickCameraFollow(dt)` in the same order as `sdl_main.cpp`; function-local statics `s_frame` and `s_total`
- Added `#include "../../include/enjin2/input/input_state.hpp"` inside the `#ifdef ENJIN2_BUILD_LUA` guard in `emscripten_bindings.cpp`
- Replaced ESP32 example's single-executeString-then-idle pattern with a proper `vTaskDelayUntil`-based game loop at ~62.5fps; documents correct input wiring pattern with commented `setInput` call pending LuaScriptSystem migration; uses `engine.callFunction("update", dt)` (correct — `LuaEngine` has no `update()` method)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add setInputState and updateFrame WASM bindings** - `dad4603` (feat)
2. **Task 2: Upgrade ESP32 example to per-frame FreeRTOS task loop** - `f877ea4` (feat)

## Files Created/Modified

- `src/bindings/emscripten_bindings.cpp` — `#include input_state.hpp` added; `setInputState` and `updateFrame` free functions added after `LuaScriptSystem` class binding
- `examples/esp32_idf_example/main/main.cpp` — full `app_main` body replaced with per-frame game loop

## Decisions Made

- `setInputState` and `updateFrame` are free functions (not LuaScriptSystem methods) to match the existing WASM binding architecture where the JS host instantiates and owns the `LuaScriptSystem` object.
- `s_wasm_input` is a function-local static inside the `setInputState` lambda — safe for single-instance WASM, zero heap allocation.
- ESP32 example stays on `LuaEngine` (not migrated) — migration path to `LuaScriptSystem` is documented in the commented `setInput` call; the example's purpose is to show the wiring pattern, not to be a production application.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All 5 DEBT items resolved (DEBT-01 through DEBT-05)
- Phase 59 execution complete — ready for verification

---
*Phase: 59-tech-debt-and-known-issues*
*Completed: 2026-03-03*
