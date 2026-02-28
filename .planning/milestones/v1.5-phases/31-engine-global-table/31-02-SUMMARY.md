---
phase: 31-engine-global-table
plan: 02
subsystem: scripting
tags: [lua, bindings, engine-table, input, scene, time, logging]

# Dependency graph
requires:
  - phase: 31-01
    provides: "engine.* global table with sub-tables and 10 stub C functions; EngineTimeState struct; registry keys enjin_ssm/enjin_active_scene/enjin_time"
  - phase: 30-01
    provides: "SceneStateMachine::switchTo(uint32_t) deferred transition API"
  - phase: 29-02
    provides: "Scene::findByName(const char*) proxy method returning Object*"
provides:
  - "lua_engine_scene_switch: fetches enjin_ssm from Lua registry, calls SceneStateMachine::switchTo(uint32_t)"
  - "lua_engine_scene_find: fetches enjin_active_scene from Lua registry, calls Scene::findByName(), returns lightuserdata or nil"
  - "lua_engine_input_held/just_pressed/just_released: delegate to currentInput pointer via getBindings(), null-guarded"
  - "lua_engine_input_axis: delegates to currentInput->axes[n] with bounds check [0,8)"
  - "lua_engine_time_delta/now/frame: read from EngineTimeState* fetched from enjin_time registry key"
  - "lua_engine_log: printf-only variadic logger handling all Lua types without crashing"
affects: [32-scriptproxy-userdata, 35-lua-gc-memory]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Registry pointer retrieval: lua_getfield(L, LUA_REGISTRYINDEX, key) + static_cast + lua_pop(L,1) for SSM/Scene/TimeState"
    - "Null guard pattern: all engine.* functions return safe defaults (false/0/nil) when pointers are nullptr"
    - "printf-only logging: engine.log uses printf not std::cout for cross-platform compatibility (ESP32/Emscripten/desktop)"

key-files:
  created: []
  modified:
    - "src/scripting/bindings.cpp"

key-decisions:
  - "lua_engine_scene_find returns lightuserdata (Object*) — Phase 32 upgrades to full ScriptProxy with metatable"
  - "engine.log uses printf exclusively — std::cout excluded for embedded target compatibility"
  - "lua_tostring fallback to lua_typename for non-string Lua types prevents printf nullptr deref"

patterns-established:
  - "All engine.* null guards match existing lua_isButtonHeld pattern — consistent defensive coding"

requirements-completed: [ENG-01, ENG-02, ENG-03, ENG-04, ENG-05]

# Metrics
duration: 1min
completed: 2026-02-27
---

# Phase 31 Plan 02: engine.* Global Table Implementation Summary

**10 engine.* C functions fully implemented: scene switch/find via registry pointers, input polling via currentInput, time state from enjin_time registry, printf-based variadic logger**

## Performance

- **Duration:** 1 min
- **Started:** 2026-02-27T02:28:15Z
- **Completed:** 2026-02-27T02:29:19Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- `lua_engine_scene_switch` and `lua_engine_scene_find` implemented: fetch SSM/Scene pointers from Lua registry, delegate to `switchTo(uint32_t)` and `findByName(const char*)`, null-safe throughout
- All 4 input functions (`held`, `just_pressed`, `just_released`, `axis`) implemented using `getBindings(L)+currentInput` pattern matching existing `lua_isButtonHeld`
- All 3 time functions (`delta`, `now`, `frame`) fetch `EngineTimeState*` from `enjin_time` registry key and return the appropriate field
- `engine.log` implemented with `printf` only (no `std::cout`), handles nil/boolean/table types via `lua_typename` fallback — never crashes on any argument
- All 42 existing tests pass (18 layer_binding + 24 hot_reload), zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement engine.scene.switch and engine.scene.find** - `b2c7a9f` (feat)
2. **Task 2: Implement engine.input.*, engine.time.*, and engine.log** - `4e75c03` (feat)

**Plan metadata:** (docs commit — see below)

## Files Created/Modified
- `src/scripting/bindings.cpp` - Replaced 10 stub implementations with full logic

## Decisions Made
- `lua_engine_scene_find` returns `lightuserdata` (raw `Object*`) at this stage — Phase 32 upgrades to full `ScriptProxy` userdata with metatable, as planned
- `engine.log` uses `printf` exclusively — `std::cout` is excluded because it adds overhead and is incompatible with ESP32/Emscripten targets; the existing `lua_print` function already uses `std::cout` and remains unchanged for backward compatibility
- `lua_tostring` returns `nullptr` for booleans, tables, functions, and nil — the `lua_typename` fallback prevents passing `nullptr` to `printf("%s")` which would be undefined behavior

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. Both method signatures (`switchTo(uint32_t)` and `findByName(const char*)`) matched the plan documentation exactly.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All 10 `engine.*` C functions are fully operational; Lua scripts can now use `engine.scene.switch()`, `engine.scene.find()`, all `engine.input.*` polling, `engine.time.*`, and `engine.log()`
- Phase 31-03 (if any) can proceed — or Phase 32 (ScriptProxy Userdata) can begin upgrading `lua_engine_scene_find` to return a full proxy instead of lightuserdata

---
*Phase: 31-engine-global-table*
*Completed: 2026-02-27*

## Self-Check: PASSED

- FOUND: src/scripting/bindings.cpp
- FOUND: .planning/phases/31-engine-global-table/31-02-SUMMARY.md
- FOUND commit: b2c7a9f (feat: engine.scene.switch and engine.scene.find)
- FOUND commit: 4e75c03 (feat: engine.input.*, engine.time.*, engine.log)
