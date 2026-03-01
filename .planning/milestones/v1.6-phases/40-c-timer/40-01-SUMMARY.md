---
phase: 40-c-timer
plan: 01
subsystem: scripting
tags: [lua, luaL_ref, component, timer, c-timer, game-loop, callbacks]

# Dependency graph
requires:
  - phase: 39-componentproxy
    provides: ComponentProxy infrastructure, self:get() dispatch, C_Position_Proxy pattern, component_proxy_test
provides:
  - C_Timer component with fixed-size timer array (zero heap allocation)
  - C_Timer_Proxy Lua metatable with after/every/cancel methods
  - luaL_ref cleanup discipline: clearTimers() pattern for Lua registry ref management
  - TIMER-05 hot-reload safety: C_LuaScript calls clearTimers() on reload and destruction
  - timer_test.cpp covering TIMER-01 through TIMER-05 (7 tests)
affects:
  - 42-eventbus (same luaL_ref storage and clearRefs cleanup discipline)
  - 41-c-statemachine (same ComponentProxy metatable pattern)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "fireCallback(int cbRef) accepts ref as parameter to avoid re-entrant double-fire"
    - "clearTimers() sets m_L=nullptr after unref to prevent double-unref in destructor"
    - "One-shot timers deactivated BEFORE lua_pcall (re-entrancy safe)"
    - "C_LuaScript destructor calls sibling C_Timer::clearTimers() before shutdown()"
    - "ComponentProxy __index returns pushcfunction for each method (stateless dispatch)"

key-files:
  created:
    - include/enjin2/components/timer.hpp
    - src/components/timer.cpp
    - tests/timer_test.cpp
  modified:
    - src/scripting/bindings.cpp
    - src/components/lua_script.cpp
    - tests/CMakeLists.txt
    - CMakeLists.txt

key-decisions:
  - "fireCallback(cbRef) takes cbRef as parameter: one-shot timers set entry.callbackRef=LUA_NOREF before pcall, so saved local copy is required"
  - "clearTimers() sets m_L=nullptr sentinel: destructor calls clearTimers() which nulls m_L, preventing double-unref"
  - "C_LuaScript destructor calls C_Timer::clearTimers() before scriptSystem->shutdown() to handle component[0]-before-component[1] destruction order"
  - "lua_timer_after/every call timer->setLuaState(L) before scheduling: bindings inject Lua state into C_Timer on each call"

patterns-established:
  - "luaL_ref cleanup: every acquired ref must be released in cancel(), clearTimers(), or on slot-full rejection in scheduleInternal()"
  - "ComponentProxy binding: static functions (lua_timer_after, lua_timer_every, lua_timer_cancel) + __index dispatch function"
  - "Hot-reload safety: executeScript() and loadScriptFile() call clearTimers() after invalidating old proxy"

requirements-completed: [TIMER-01, TIMER-02, TIMER-03, TIMER-04, TIMER-05]

# Metrics
duration: 4min
completed: 2026-02-28
---

# Phase 40 Plan 01: C_Timer Summary

**C_Timer component with fixed-size 8-slot timer array, luaL_ref callback storage, and full hot-reload cleanup via clearTimers() — accessible from Lua via self:get("C_Timer"):after/every/cancel**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-28T14:43:40Z
- **Completed:** 2026-02-28T14:48:08Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- C_Timer component with MAX_TIMERS=8 fixed-size array, zero heap allocation, scheduleAfter/scheduleEvery/cancel/clearTimers/update
- C_Timer_Proxy Lua metatable with after/every/cancel methods dispatched via __index pushcfunction pattern
- TIMER-05 destruction-order safety: C_LuaScript destructor calls C_Timer::clearTimers() before scriptSystem->shutdown()
- 7 timer tests passing: TIMER-01 (one-shot), TIMER-02 (repeating), TIMER-03 (cancel), TIMER-03b (cancel specificity), TIMER-04 (self arg), TIMER-05a (hot-reload), TIMER-05b (destruction)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create C_Timer component class and register in build system** - `499ef57` (feat)
2. **Task 2: Wire C_Timer_Proxy Lua bindings, hot-reload cleanup, and test suite** - `1068a41` (feat)

**Plan metadata:** *(to be committed with this SUMMARY)*

## Files Created/Modified

- `include/enjin2/components/timer.hpp` - C_Timer class declaration with TimerEntry fixed-size array
- `src/components/timer.cpp` - Full implementation of update(), fireCallback(), clearTimers(), destructor
- `src/scripting/bindings.cpp` - C_Timer_Proxy metatable + C_Timer branch in lua_proxy_get_component_impl
- `src/components/lua_script.cpp` - TIMER-05 clearTimers() calls in destructor and executeScript()/loadScriptFile()
- `tests/timer_test.cpp` - 7 tests covering TIMER-01..TIMER-05
- `tests/CMakeLists.txt` - timer_test registered under ENJIN2_BUILD_LUA guard
- `CMakeLists.txt` - src/components/timer.cpp added to enjin2_lua target_sources

## Decisions Made

- `fireCallback(int cbRef)` accepts the callback ref as parameter because for one-shot timers, `entry.callbackRef` is set to `LUA_NOREF` before the call (preventing re-entrant double-fire), so the ref must be saved to a local before clearing
- `clearTimers()` sets `m_L = nullptr` after releasing all refs — this sentinel prevents double-unref if the destructor calls clearTimers() again when it's already been called via C_LuaScript destructor
- C_LuaScript destructor calls `C_Timer::clearTimers()` before `scriptSystem->shutdown()` — handles the case where components array destructs in index order (C_LuaScript at [0], C_Timer at [1]), meaning C_LuaScript closes the Lua state before C_Timer destructor runs
- `lua_timer_after` and `lua_timer_every` call `timer->setLuaState(L)` to inject the Lua state into C_Timer — C_Timer doesn't own the Lua state, bindings inject it on use

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

`sprite_load_test` is a pre-existing build failure (missing `lua_wrapper.hpp` header not created by this phase). All 25 other registered tests pass, including the new `timer_test`.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- C_Timer fully operational — Lua scripts can call `self:get("C_Timer"):after/every/cancel`
- luaL_ref cleanup discipline established — Phase 42 EventBus follows exact same clearRefs() pattern
- Phase 41 (C_StateMachine) can follow the same ComponentProxy metatable pattern established here
- No blockers

## Self-Check: PASSED

- `include/enjin2/components/timer.hpp`: FOUND
- `src/components/timer.cpp`: FOUND
- `tests/timer_test.cpp`: FOUND
- Commit `499ef57`: FOUND
- Commit `1068a41`: FOUND
- `ctest -R timer_test`: PASSED (1/1)

---
*Phase: 40-c-timer*
*Completed: 2026-02-28*
