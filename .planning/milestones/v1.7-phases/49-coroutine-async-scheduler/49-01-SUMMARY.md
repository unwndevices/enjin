---
phase: 49-coroutine-async-scheduler
plan: 01
subsystem: scripting
tags: [lua, coroutine, async, scheduler, bindings]

# Dependency graph
requires:
  - phase: 48-camera-follow-save-load
    provides: bindings pattern for member function bindings and registerAll/setActiveScene cleanup hooks
provides:
  - 8-slot fixed coroutine pool (CoroutineSlot[8]) with zero dynamic allocation
  - engine.async.start/wait/cancel/cancelAll Lua API
  - tickCoroutines(dt) frame tick integrated into SDL game loop
  - clearCoroutines() cleanup on hot-reload and scene-transition
affects:
  - 49-02: depends on coroutine pool being in place for any coroutine library extension
  - Any future Lua scripting phases using cooperative scheduling

# Tech tracking
tech-stack:
  added: []
  patterns:
    - CoroutineSlot fixed-pool pattern: 8-slot array with threadRef/waitRemaining/id/active fields
    - lua_resume compat guard: LUA_VERSION_NUM >= 504 for 4-arg vs 2-arg signatures
    - Post-yield dt subtraction: first tick's dt counts toward wait duration
    - 0.001f epsilon for float accumulation in wait timer countdown

key-files:
  created:
    - src/scripting/bindings_async.cpp
    - tests/coroutine_async_test.cpp
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - src/scripting/bindings_engine.cpp
    - src/platform/sdl/sdl_main.cpp
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Float epsilon 0.001f used in tickCoroutines wait timer to handle 5*0.1f != 0.5f float precision issue"
  - "Post-yield dt subtraction: after lua_resume returns LUA_YIELD, subtract current frame dt so the start tick counts toward wait duration"
  - "clearSlot helper uses template<typename Slot> to avoid private CoroutineSlot type access from file-scope static function"
  - "tickCoroutines placed AFTER tickCameraFollow but BEFORE draw() pcall in SDL runner — outside any pcall scope"

patterns-established:
  - "Post-yield dt accounting: when LUA_YIELD returned, subtract current frame's dt from new waitRemaining"
  - "Epsilon check for float timer: use 0.001f threshold instead of 0.0f to handle cumulative float error"

requirements-completed: [ASYNC-01, ASYNC-02, ASYNC-03]

# Metrics
duration: 7min
completed: 2026-03-01
---

# Phase 49 Plan 01: Coroutine Async Scheduler Summary

**8-slot fixed coroutine pool with lua_resume compat guard and engine.async.start/wait/cancel/cancelAll Lua API**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-01T21:20:58Z
- **Completed:** 2026-03-01T21:27:48Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments
- CoroutineSlot[8] fixed pool added to LuaBindings with zero dynamic allocation
- bindings_async.cpp implementing all 4 binding functions (start/wait/cancel/cancelAll) plus tickCoroutines/clearCoroutines
- lua_resume API compat guard for Lua 5.4 (4-arg) vs LuaJIT/5.1 (2-arg)
- tickCoroutines integrated into SDL game loop outside any pcall scope
- clearCoroutines called from registerAll() and setActiveScene() for hot-reload/scene-change safety
- coroutine_async_test with 8 test cases — all passing; no regressions (39/39 tests pass)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add CoroutineSlot struct and pool, create bindings_async.cpp** - `0a551f2` (feat)
2. **Task 2: Wire tickCoroutines into SDL runner, registerAll/setActiveScene, add test** - `d769bec` (feat)

**Plan metadata:** (docs commit to follow)

## Files Created/Modified
- `include/enjin2/scripting/bindings.hpp` - Added CoroutineSlot struct, m_coroutinePool[8], m_nextCoroutineId, tickCoroutines/clearCoroutines public methods, static async binding function declarations, registerAsyncSubtable() private declaration
- `src/scripting/bindings_async.cpp` - All async binding implementations: lua_engine_async_start/wait/cancel/cancelAll, tickCoroutines(dt), clearCoroutines(), registerAsyncSubtable()
- `src/scripting/bindings.cpp` - Added clearCoroutines() call in registerAll() and setActiveScene()
- `src/scripting/bindings_engine.cpp` - Added registerAsyncSubtable(L) call in registerEngineTable()
- `src/platform/sdl/sdl_main.cpp` - Added tickCoroutines(dt) call after tickCameraFollow(), before draw() pcall
- `CMakeLists.txt` - Added bindings_async.cpp to enjin2_lua sources
- `tests/coroutine_async_test.cpp` - 8 tests covering table existence, start/ID, coroutine runs, pool overflow, wait delays, cancel by ID, cancelAll, clear on reload
- `tests/CMakeLists.txt` - Added coroutine_async_test target and ctest entry

## Decisions Made
- Float epsilon 0.001f used in tickCoroutines wait timer: `5 * 0.1f` does not equal exactly `0.5f` in IEEE 754, causing the timer to fire on tick 6 instead of tick 5. 0.001f epsilon is below any meaningful game dt but above float precision error.
- Post-yield dt subtraction: when lua_resume returns LUA_YIELD, subtract the current frame's dt from the newly-set waitRemaining. This ensures the frame where a coroutine starts and immediately calls wait() counts as the first tick of the wait duration.
- clearSlot helper uses `template<typename Slot>` to access the private CoroutineSlot struct from a file-scope static function in bindings_async.cpp.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Float precision fix in tickCoroutines wait timer**
- **Found during:** Task 2 (coroutine_async_test verification)
- **Issue:** `5 * 0.1f` accumulates to `~0.50000002f` instead of `0.5f`, causing the timer not to fire on tick 5
- **Fix:** Changed comparison from `> 0.0f` to `> 0.001f` epsilon for the wait timer countdown
- **Files modified:** src/scripting/bindings_async.cpp
- **Verification:** test_async_wait_delays_resume passes correctly
- **Committed in:** d769bec (Task 2 commit)

**2. [Rule 1 - Bug] Post-yield dt subtraction for correct frame timing**
- **Found during:** Task 2 (test_async_wait_delays_resume analysis)
- **Issue:** Without accounting for the current frame's dt after yield, the start tick wasted a full frame, making wait(0.5) fire on tick 6 instead of tick 5 (one extra frame of delay)
- **Fix:** After lua_resume returns LUA_YIELD, subtract dt from slot.waitRemaining
- **Files modified:** src/scripting/bindings_async.cpp
- **Verification:** test_async_wait_delays_resume passes with exactly 5 ticks at dt=0.1
- **Committed in:** d769bec (Task 2 commit)

**3. [Rule 1 - Bug] clearSlot template to access private CoroutineSlot**
- **Found during:** Task 1 (first compile attempt)
- **Issue:** File-scope static `clearSlot()` couldn't access private `LuaBindings::CoroutineSlot{}`
- **Fix:** Changed to `template<typename Slot>` helper and manual field reset instead of aggregate re-init
- **Files modified:** src/scripting/bindings_async.cpp
- **Verification:** Compiles cleanly
- **Committed in:** 0a551f2 (Task 1 commit)

---

**Total deviations:** 3 auto-fixed (all Rule 1 bugs found during compilation and test verification)
**Impact on plan:** All auto-fixes were essential for correctness. No scope creep.

## Issues Encountered
- CoroutineSlot private access from file-scope static helper — solved with template
- Float accumulation in wait timer — solved with epsilon threshold
- Frame timing for wait duration — solved with post-yield dt subtraction

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- engine.async.* API fully functional from Lua scripts
- Coroutine pool ready for use in any Lua script (tamagotchi, arkanoid demos can use cooperative scheduling)
- Phase 49-02 (ESP32 coroutine library registration) can proceed independently

---
*Phase: 49-coroutine-async-scheduler*
*Completed: 2026-03-01*
