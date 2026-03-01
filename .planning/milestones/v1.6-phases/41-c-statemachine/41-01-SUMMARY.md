---
phase: 41-c-statemachine
plan: 01
subsystem: components
tags: [lua, fsm, state-machine, luaL_ref, component-proxy, deferred-transition]

# Dependency graph
requires:
  - phase: 39-componentproxy
    provides: ComponentProxy infrastructure (full userdata, setLuaProxy, valid flag pattern)
  - phase: 40-c-timer
    provides: C_Timer pattern (luaL_ref management, clearTimers in destructor/reload, setLuaState)

provides:
  - C_StateMachine component class with addState/setState/getState/clearStates API
  - Deferred FSM transition model preventing re-entrant corruption
  - C_StateMachine_Proxy Lua metatable (fsm:addState/setState/getState)
  - Hot-reload and destruction-order safety via clearStates() in C_LuaScript lifecycle
  - FSM-01..FSM-05 test coverage (state_machine_test)

affects:
  - 42-eventbus
  - future component phases using luaL_ref lifecycle pattern

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Deferred FSM transitions: setState() queues m_pendingState, applied at END of update() after state's update callback"
    - "m_hasPending cleared BEFORE applyPendingTransition() — matches SceneStateMachine ordering so exit/enter callbacks can re-queue without re-entrant corruption"
    - "fireCallback(ref, dt, passDt) pattern: push function, push ScriptProxy via lua_pushlightuserdata+lua_gettable, optional dt, pcall"
    - "clearStates() sets m_L=nullptr after unref — prevents double-unref in destructor (mirrors clearTimers sentinel pattern)"
    - "Fixed-size StateEntry array (MAX_STATES=8, MAX_STATE_NAME=32) — zero dynamic allocation"
    - "C_LuaScript destructor calls clearStates() BEFORE scriptSystem->shutdown() — destruction-order safety"

key-files:
  created:
    - include/enjin2/components/state_machine.hpp
    - src/components/state_machine.cpp
    - tests/state_machine_test.cpp
  modified:
    - CMakeLists.txt (added state_machine.cpp to enjin2_lua target_sources)
    - src/scripting/bindings.cpp (C_StateMachine_Proxy metatable + getComponent dispatch)
    - src/components/lua_script.cpp (clearStates in destructor, executeScript, loadScriptFile)
    - tests/CMakeLists.txt (state_machine_test registration)

key-decisions:
  - "Self-transition (setState to same state while in that state) triggers full exit+enter cycle — consistent with SceneStateMachine semantics"
  - "addState() rejects names where strlen >= MAX_STATE_NAME to prevent silent truncation (Pitfall 7)"
  - "addState() overwrites duplicate state names (releases old refs first) — prevents leaks on re-definition"
  - "C_StateMachine::update() fires active state update FIRST, then applies pending transition — FSM-04 deferred ordering"
  - "setLuaState(L) called in lua_fsm_addState to bind the Lua state before first use"

patterns-established:
  - "Component proxy type registration: add include in bindings.cpp + add branch in lua_proxy_get_component_impl + add metatable in registerComponentProxyMetatable()"
  - "Component cleanup registration: add include in lua_script.cpp + call clearX() in destructor/executeScript/loadScriptFile alongside clearTimers()"

requirements-completed: [FSM-01, FSM-02, FSM-03, FSM-04, FSM-05]

# Metrics
duration: 5min
completed: 2026-02-28
---

# Phase 41 Plan 01: C_StateMachine Summary

**Deferred FSM component with per-state enter/update/exit Lua callbacks, fixed-size array storage, and hot-reload safety via clearStates() integrated into C_LuaScript lifecycle**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-02-28T14:51:37Z
- **Completed:** 2026-02-28T14:56:31Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- C_StateMachine component class with zero dynamic allocation (StateEntry fixed-size array, char buffers), deferred transition model matching SceneStateMachine pattern exactly
- C_StateMachine_Proxy Lua metatable with addState/setState/getState; registered in bindings.cpp alongside C_Timer_Proxy and C_Position_Proxy
- Hot-reload and destruction-order safety: clearStates() called in C_LuaScript destructor, executeScript(), and loadScriptFile() before Lua state shutdown
- Full FSM-01..FSM-05c test coverage passing with zero regressions in 26-test suite

## Task Commits

Each task was committed atomically:

1. **Task 1: Create C_StateMachine component class and register in build system** - `36ff5d8` (feat)
2. **Task 2: Wire C_StateMachine_Proxy Lua bindings, hot-reload cleanup, and test suite** - `c758d1e` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `include/enjin2/components/state_machine.hpp` - C_StateMachine class with StateEntry struct, deferred transition fields, public API
- `src/components/state_machine.cpp` - Full implementation: update(), applyPendingTransition(), fireCallback(), clearStates(), addState(), getState(), setState()
- `src/scripting/bindings.cpp` - state_machine.hpp include, C_StateMachine branch in lua_proxy_get_component_impl, C_StateMachine_Proxy metatable (addState/setState/getState), metatable registration
- `src/components/lua_script.cpp` - state_machine.hpp include, clearStates() in destructor/executeScript/loadScriptFile
- `tests/state_machine_test.cpp` - FSM-01 through FSM-05c test coverage
- `tests/CMakeLists.txt` - state_machine_test registration under ENJIN2_BUILD_LUA
- `CMakeLists.txt` - state_machine.cpp added to enjin2_lua target_sources

## Decisions Made
- Self-transition triggers full exit+enter cycle (consistent with SceneStateMachine semantics)
- `addState()` overwrites duplicate names (releases old refs first) — prevents leaks on re-definition
- `addState()` rejects names where `strlen(name) >= MAX_STATE_NAME` to prevent silent truncation
- `m_hasPending` cleared BEFORE `applyPendingTransition()` — matches SceneStateMachine ordering so `setState()` from within exit/enter callbacks queues for NEXT frame
- `clearStates()` sets `m_L = nullptr` after unref — prevents double-unref in destructor (mirrors clearTimers sentinel pattern)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- `sprite_load_test` is pre-existing failing test (missing GTest dependency and lua_wrapper.hpp) — not introduced by this phase. All 26 runnable tests pass.

## Next Phase Readiness
- C_StateMachine fully implemented and tested; ready for Phase 42 (EventBus)
- ComponentProxy infrastructure now supports three component types: C_Position, C_Timer, C_StateMachine
- Pattern for adding future component proxy types is established and documented

---
*Phase: 41-c-statemachine*
*Completed: 2026-02-28*
