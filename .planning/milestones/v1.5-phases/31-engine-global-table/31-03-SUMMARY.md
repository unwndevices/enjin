---
phase: 31-engine-global-table
plan: 03
subsystem: testing
tags: [lua, engine-table, ctest, sdl, time-state, tdd]

# Dependency graph
requires:
  - phase: 31-01
    provides: engine.* global table registration and LuaBindings::registerAll()
  - phase: 31-02
    provides: engine.scene/input/time/log C implementations with null guards

provides:
  - Automated test suite (engine_table_test) covering ENG-01..ENG-06 behavioral requirements
  - Module-level access verification resolving the Phase 31 STATE.md blocker
  - setTimeState() wired into SDL main loop for live engine.time.* values during gameplay

affects: [32-scriptproxy-userdata, phase-32]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "EngineTableFixture: minimal LuaEngine + LuaBindings with no canvas/layers/input — sufficient for engine.* testing"
    - "luaBool helper: exec('if (expr) then ok = 1 else ok = 0 end') + getNum('ok') == 1.0 for boolean Lua assertions"
    - "Float-tolerance assertions: d > 0.015 && d < 0.017 for float→double conversion from setTimeState"

key-files:
  created:
    - tests/engine_table_test.cpp
  modified:
    - tests/CMakeLists.txt
    - src/platform/sdl/sdl_main.cpp

key-decisions:
  - "Removed broken NaN check in test_engine_scene_null_guards — used cleaner Lua nil comparison via ok = (find('x') == nil) and 1 or 0"
  - "s_totalTime and s_frameCount declared inside ENJIN2_BUILD_LUA guard to avoid unused-variable warnings in non-Lua builds"
  - "Time accumulators reset on F5 hot-reload alongside prev_ticks to prevent time-jump artifacts after script reload"

patterns-established:
  - "Minimal fixture pattern: LuaEngine + LuaBindings without canvas/layers is sufficient for engine.* sub-table testing"
  - "setTimeState called after setInput() and before callFunction('update', dt) in SDL main loop"

requirements-completed: [ENG-01, ENG-02, ENG-03, ENG-04, ENG-05, ENG-06]

# Metrics
duration: 2min
completed: 2026-02-27
---

# Phase 31 Plan 03: engine.* Test Suite and SDL Time Wiring Summary

**34-assertion ctest suite verifying all engine.* sub-table behaviors (null guards, type checks, time state, module-level access) plus live setTimeState() wired into SDL game loop**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-27T02:31:28Z
- **Completed:** 2026-02-27T02:33:14Z
- **Tasks:** 2 of 2
- **Files modified:** 3

## Accomplishments
- engine_table_test.cpp with 7 test functions and 34 assertions, all passing, registered as ctest
- Resolves the Phase 31 STATE.md blocker: "Must verify with module-level access test script before Phase 32 begins"
- SDL main loop now calls setTimeState(dt, totalTime, frameCount) before each update() call; time accumulators reset on hot-reload
- All 9 ctest tests pass with 0 regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Write engine_table_test.cpp and register in CMakeLists.txt** - `0212b67` (test)
2. **Task 2: Wire setTimeState into SDL main loop with accumulated time and frame counter** - `3e31ece` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `tests/engine_table_test.cpp` - 7-function test suite covering ENG-01..ENG-06: module-level access, type checks, input null guards, time defaults, time after setTimeState, engine.log no-crash, scene null guards
- `tests/CMakeLists.txt` - Added engine_table_test target under ENJIN2_BUILD_LUA guard after hot_reload_test
- `src/platform/sdl/sdl_main.cpp` - Added s_totalTime/s_frameCount accumulators (ENJIN2_BUILD_LUA guarded), setTimeState() call per-frame, accumulator reset on F5 reload

## Decisions Made
- Removed the broken NaN double-check in test_engine_scene_null_guards (f.getNum("result") != f.getNum("result") pattern) and used only the cleaner Lua nil comparison check. The NaN pattern was unreliable and the plan itself called it "awkward".
- Declared s_totalTime and s_frameCount inside the #ifdef ENJIN2_BUILD_LUA block to avoid unused-variable warnings in non-Lua builds.
- Hot-reload resets both s_totalTime and s_frameCount to prevent time-jump and frame-count continuity confusion after script reload.

## Deviations from Plan

None - plan executed exactly as written (with one minor clean-up: the NaN check the plan itself marked as "awkward" was removed; only the cleaner nil check was kept, which the plan also specified).

## Issues Encountered

None - Plans 01 and 02 implementations were complete and correct. Tests went GREEN immediately on first compile+run as predicted by the plan.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 31 is now fully complete: ENG-01..ENG-06 all met, automated test suite passing, SDL loop wired
- The STATE.md blocker for Phase 32 (ScriptProxy Userdata) is resolved — module-level access verified
- Phase 32 can begin: validity mechanism decision (generation token vs valid flag) is the remaining architectural decision to resolve before writing proxy code

---
*Phase: 31-engine-global-table*
*Completed: 2026-02-27*
