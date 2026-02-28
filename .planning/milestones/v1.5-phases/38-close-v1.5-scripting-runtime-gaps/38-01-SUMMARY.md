---
phase: 38-close-v1.5-scripting-runtime-gaps
plan: 01
subsystem: scripting
tags: [lua, scripting, bindings, ScriptProxy, SceneStateMachine, ObjectProxy]

# Dependency graph
requires:
  - phase: 32-scriptproxy-userdata
    provides: ScriptProxy userdata with metatable registration in LuaBindings
  - phase: 31-engine-global-table
    provides: registerAll() registry wiring pattern; engine.scene.switch/find closures
  - phase: 37-address-codebase-concerns
    provides: ObjectProxy userdata with metatable; ScriptErrorPolicy dispatch
provides:
  - "ENG-01: engine.scene.switch() reaches SceneStateMachine::switchTo() after post-registerAll injection"
  - "ENG-02: engine.scene.find() returns valid ObjectProxy for named Object after post-registerAll injection"
  - "PROXY-01: loadScriptFile() init(self) receives valid ScriptProxy userdata (not nil)"
  - "Live-wiring tests for ENG-01, ENG-02, PROXY-01 in engine_table_test.cpp"
affects:
  - phase 38 subsequent plans
  - v1.5 milestone completion

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "pointer-to-pointer registry: store &member not member for late-injected pointers"
    - "proxy creation block in loadScriptFile mirrors executeScript exactly"

key-files:
  created:
    - ".planning/phases/38-close-v1.5-scripting-runtime-gaps/38-01-SUMMARY.md"
  modified:
    - "src/scripting/bindings.cpp"
    - "src/scripting/bindings_engine.cpp"
    - "src/components/lua_script.cpp"
    - "tests/engine_table_test.cpp"

key-decisions:
  - "pointer-to-pointer pattern extended to m_ssm and m_activeScene — same pattern as m_timeState which already worked"
  - "loadScriptFile() proxy block is exact copy of executeScript() block — ensures identical behavior on file-load and string-load paths"
  - "ENG-01 test uses addScene<MinimalScene>(2u) before switchTo(2) — switchTo only queues for registered scene IDs"
  - "ENG-01 verified by calling mockSSM.update(0.0f) which applies deferred transition, proving switchTo was reached"
  - "PROXY-01 test writes /tmp/proxy01_test.lua and uses loadScriptFile() — exercises the actual file-load path"

patterns-established:
  - "Live-wiring test pattern: inject host object AFTER registerAll(), call via Lua, verify C++ side effect"

requirements-completed: [ENG-01, ENG-02, PROXY-01]

# Metrics
duration: 12min
completed: 2026-02-27
---

# Phase 38 Plan 01: Close v1.5 Scripting Runtime Gaps Summary

**Pointer-to-pointer registry fix closes ENG-01/ENG-02 silent no-op bugs; loadScriptFile() proxy block closes PROXY-01 nil-self bug; all 19 ctests green**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-02-27T20:52:00Z
- **Completed:** 2026-02-27T21:04:00Z
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments

- Fixed `registerAll()` to store `&m_ssm` and `&m_activeScene` (address-of-member) instead of value snapshots — enables post-registerAll injection to work live
- Fixed `lua_engine_scene_switch()` and `lua_engine_scene_find()` to dereference double pointers from registry
- Fixed `loadScriptFile()` to create ScriptProxy userdata before calling init — init(self) now receives valid proxy instead of nil
- Added three live-wiring tests: ENG-01 (switch), ENG-02 (find), PROXY-01 (loadScriptFile init proxy)
- All 19 ctests pass (61 assertions in engine_table_test)

## Task Commits

Each task was committed atomically:

1. **Task 1: Fix ENG-01 and ENG-02 — pointer-to-pointer registry wiring** - `ebd4094` (fix)
2. **Task 2: Fix PROXY-01 — loadScriptFile() proxy creation and init call** - `2c76d81` (fix)
3. **Task 3: Add live-wiring tests for ENG-01, ENG-02, and PROXY-01** - `18e2c24` (test)

## Files Created/Modified

- `src/scripting/bindings.cpp` — Changed `lua_pushlightuserdata(L, m_ssm/m_activeScene)` to `&m_ssm/&m_activeScene`
- `src/scripting/bindings_engine.cpp` — Changed single-pointer casts to double-pointer dereference in switch/find
- `src/components/lua_script.cpp` — Inserted proxy creation block before `callWithProxy(INIT_FUNCTION)` in loadScriptFile()
- `tests/engine_table_test.cpp` — Added MinimalScene struct + three live-wiring test functions + main() calls

## Decisions Made

- **pointer-to-pointer pattern**: Extended same pattern as `m_timeState` (which already worked) to `m_ssm` and `m_activeScene`. Root cause was that `registerAll()` ran before `setSceneStateMachine()`/`setActiveScene()`, so value-copied pointers were always nullptr.
- **ENG-01 test**: `switchTo()` only queues if scene ID exists in SSM. Test registers `MinimalScene(2u)` first, then verifies via `mockSSM.update()` that `currentScene` becomes scene 2 — proving switchTo was reached.
- **PROXY-01 test**: Writes actual `.lua` file to `/tmp` and uses `loadScriptFile()` — tests the real file-load path, not the string-load path.
- **loadScriptFile proxy block**: Exact copy of executeScript() proxy block — ensures identical proxy creation semantics across both load paths.

## Deviations from Plan

None — plan executed exactly as written. The ENG-01 test needed `addScene<MinimalScene>(2u)` before `switchTo(2)` (plan noted this as a potential issue to check) — used MinimalScene struct inline rather than importing TestScene from scene_transition_test.

## Issues Encountered

- `switchTo()` only sets `hasPendingTransition` when a matching scene ID is registered — needed to register a scene before calling `engine.scene.switch(2)`. Resolved by adding `MinimalScene` struct and `mockSSM.addScene<MinimalScene>(2u)`. Verified by calling `mockSSM.update(0.0f)` which applies the deferred transition.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- ENG-01, ENG-02, PROXY-01 requirements satisfied — checkboxes can be marked [x] in REQUIREMENTS.md
- All 19 ctests green; scripting runtime is behaviorally correct for v1.5 milestone
- Phase 38 subsequent plans unblocked

## Self-Check: PASSED

- SUMMARY.md: FOUND
- bindings.cpp: FOUND
- bindings_engine.cpp: FOUND
- lua_script.cpp: FOUND
- engine_table_test.cpp: FOUND
- Commit ebd4094: FOUND
- Commit 2c76d81: FOUND
- Commit 18e2c24: FOUND

---
*Phase: 38-close-v1.5-scripting-runtime-gaps*
*Completed: 2026-02-27*
