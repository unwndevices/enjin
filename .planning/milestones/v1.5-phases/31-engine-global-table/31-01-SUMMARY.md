---
phase: 31-engine-global-table
plan: 01
subsystem: scripting
tags: [lua, bindings, engine-table, scene, input, time, cpp]

# Dependency graph
requires:
  - phase: 30-scene-self-transitions
    provides: SceneStateMachine and Scene with switchTo() and back-pointer injection
  - phase: 29-named-objects-tags
    provides: Scene::findByName, Scene::findAllWithTag proxy methods

provides:
  - EngineTimeState struct in LuaBindings (dt, totalTime, frameCount)
  - Scene and SceneStateMachine forward declarations in bindings.hpp
  - Private fields m_ssm, m_activeScene, m_timeState in LuaBindings
  - Public setters setSceneStateMachine(), setActiveScene(), setTimeState()
  - registerEngineTable() registered in registerAll()
  - engine global Lua table with scene, input, time, lua sub-tables and log function
  - 10 stub C functions for engine.* API (ENG-01..ENG-06)

affects:
  - 31-02-PLAN (implements real logic for all 10 stubs)
  - 32-scriptproxy-userdata (uses engine.scene.find() result)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "engine.* global Lua table built with nested lua_newtable construction"
    - "Lua registry stores C++ pointers (m_ssm, m_activeScene, &m_timeState) at registerAll() time"
    - "Forward declarations in header; full includes in .cpp only (avoids circular includes)"
    - "Stub C functions return 0/nil at Plan 01 stage; Plan 02 fills real implementations"

key-files:
  created: []
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp

key-decisions:
  - "setters are public (not private) so host code (LuaScriptSystem, SDL runner) can inject SSM/activeScene without friend declarations"
  - "m_ssm and m_activeScene stored in Lua registry via lua_pushlightuserdata at registerAll() time — not at call time — so stubs can be replaced without re-calling registerAll()"
  - "engine.lua sub-table registered as empty table at Plan 01 stage; Phase 35 adds gc/memory functions"

patterns-established:
  - "registerEngineTable() follows same nested lua_newtable pattern as love.graphics construction in registerAll()"
  - "All 10 stub C functions use (void)L to suppress unused-parameter warnings"

requirements-completed: [ENG-06]

# Metrics
duration: 3min
completed: 2026-02-27
---

# Phase 31 Plan 01: engine.* Global Table Interface Summary

**engine global Lua table wired into registerAll() with scene/input/time/lua sub-tables and 10 stub C functions — ENG-06 satisfied, Plan 02 fills implementations**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-27T02:22:03Z
- **Completed:** 2026-02-27T02:25:11Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Extended bindings.hpp with EngineTimeState struct, Scene/SceneStateMachine forward declarations, m_ssm/m_activeScene/m_timeState private fields, public injection setters, registerEngineTable() declaration, and 10 new static lua_engine_* function declarations
- Implemented registerEngineTable() in bindings.cpp building engine.scene, engine.input, engine.time, engine.lua sub-tables and engine.log as a global Lua table with verified stack balance
- Added registry storage for m_ssm/m_activeScene/&m_timeState in registerAll() so Plan 02 closures can retrieve them without re-registration
- All 10 stub C functions defined (return 0/nil); layer_binding_test (18 tests) and hot_reload_test (24 tests) pass without regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Extend bindings.hpp** - `b9b438b` (feat)
2. **Task 2: Implement registerEngineTable() and stub C functions** - `6597497` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

- `include/enjin2/scripting/bindings.hpp` - Added EngineTimeState struct, forward declarations, new private fields, public setters, registerEngineTable() declaration, 10 static function declarations
- `src/scripting/bindings.cpp` - Added scene.hpp/scene_state_machine.hpp includes, registry storage for engine pointers, registerEngineTable() call in registerAll(), full registerEngineTable() implementation, 10 stub C functions

## Decisions Made

- setters are public so host code can inject SSM/activeScene without friend declarations
- Lua registry stores C++ pointers at registerAll() time (not at call time), so Plan 02 replacements need no re-registration
- engine.lua registered as empty table now; Phase 35 adds gc/memory functions
- Forward declarations only in header; full includes kept in .cpp to avoid circular include issues

## Deviations from Plan

None - plan executed exactly as written.

One minor discovery: `ENJIN2_BUILD_LUA=OFF` was the cmake cache default, so reconfigured with `-DENJIN2_BUILD_LUA=ON` to build the enjin2_lua target. This is expected behaviour in a host without Lua cached — not a deviation.

## Issues Encountered

None - build succeeded cleanly after enabling ENJIN2_BUILD_LUA=ON.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- engine global table is registered and accessible in Lua after registerAll() returns
- engine.scene, engine.input, engine.time, engine.lua are non-nil tables; engine.log is a function
- All stubs are ready for Plan 02 real implementations
- ENG-06 (module-level access) satisfied: scripts that access engine.* at module load time will not crash

## Self-Check: PASSED

All files found. All commits found. Key content verified.

---
*Phase: 31-engine-global-table*
*Completed: 2026-02-27*
