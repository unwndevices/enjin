---
phase: 51-persistent-objects
plan: 02
subsystem: scripting
tags: [lua, bindings, scene-state-machine, persistent-objects, integration-test]

# Dependency graph
requires:
  - phase: 51-persistent-objects
    plan: 01
    provides: persistObject/unpersistObject/findPersistentByName C++ API on SSM
provides:
  - engine.scene.persist(proxy) Lua binding — returns true or nil (pool full)
  - engine.scene.unpersist(proxy) Lua binding — deferred removal on next transition
  - engine.scene.find(name) extended — fallback to persistent registry (PERSIST-03)
  - persistent_lua_test.cpp with 8-case Lua integration test suite
affects:
  - Phase 52 (if any) — completes Phase 51 PERSIST-01/02/03 requirements

# Tech tracking
tech-stack:
  added: []
  patterns:
    - lua_engine_scene_persist/unpersist as static LuaBindings member functions (same access pattern as camera follow)
    - getBindings(L) + b->m_ssm pattern to reach SSM from static binding functions
    - Active scene priority in find(): scene->findByName() first, then m_ssm->findPersistentByName() fallback

key-files:
  created:
    - tests/persistent_lua_test.cpp
  modified:
    - src/scripting/bindings_engine.cpp
    - include/enjin2/scripting/bindings.hpp
    - tests/CMakeLists.txt

key-decisions:
  - "persist() binding returns nil (not false) on overflow — consistent with coroutine/tween pool pattern; Lua scripts check ~= nil rather than not"
  - "find() fallback uses getBindings(L) + b->m_ssm; does NOT add a second Lua registry field for SSM — m_ssm is already accessible via getBindings"
  - "find() active scene priority: scene->findByName() runs first so local objects shadow persistent ones with same name (Test 8 verified)"

patterns-established:
  - "persist/unpersist follow the engine.scene.destroy() proxy-null-guard pattern: luaL_testudata + !proxy || !proxy->valid || !proxy->object -> nil"

requirements-completed: [PERSIST-01, PERSIST-02, PERSIST-03]

# Metrics
duration: 3min
completed: 2026-03-02
---

# Phase 51 Plan 02: Persistent Objects Lua Bindings Summary

**engine.scene.persist/unpersist/find Lua bindings exposing SSM PersistentObjectRegistry — 8 integration tests, 42/42 tests pass**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-01T23:58:21Z
- **Completed:** 2026-03-02T00:02:14Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- bindings.hpp: added static declarations for `lua_engine_scene_persist` and `lua_engine_scene_unpersist` in the private section alongside existing scene bindings
- bindings_engine.cpp: extended `kSceneFuncs[]` from 4 to 6 entries (persist + unpersist); implemented both binding functions; extended `lua_engine_scene_find` with persistent registry fallback (PERSIST-03)
- `lua_engine_scene_persist`: extracts ObjectProxy null-guard, calls `b->m_ssm->persistObject()`, returns `true` or `nil` (pool full / invalid proxy)
- `lua_engine_scene_unpersist`: null-guards proxy, calls `b->m_ssm->unpersistObject()`, returns `true`
- `lua_engine_scene_find`: fallback `b->m_ssm->findPersistentByName(name)` BEFORE the nil-return check, after active scene search — active scene priority maintained
- persistent_lua_test.cpp: 8-case Lua integration test suite; 47 assertions; covers all PERSIST-01/02/03 requirements including overflow, double-persist no-op, self-transition, and find priority

## Task Commits

1. **Task 1: Add persist/unpersist bindings and extend find()** — `8246943` (feat)
2. **Task 2: Lua integration tests** — `2ae6c0a` (feat)

## Files Created/Modified

- `include/enjin2/scripting/bindings.hpp` — Added static declarations for lua_engine_scene_persist and lua_engine_scene_unpersist
- `src/scripting/bindings_engine.cpp` — Extended kSceneFuncs (6 entries), implemented persist/unpersist bindings, extended find() with persistent fallback
- `tests/persistent_lua_test.cpp` — 8-test Lua integration suite (created)
- `tests/CMakeLists.txt` — Registered persistent_lua_test executable

## Decisions Made

- `persist()` returns nil (not false) on overflow — consistent with coroutine/tween pool pattern used in engine.async and engine.tween; Lua callers check `~= nil`.
- `find()` extended by inserting fallback between the scene search and the existing nil-return — requires only adding one `if (!obj)` block before the existing nil check.
- Active scene priority confirmed: `scene->findByName()` runs before persistent registry search; Test 8 verified scene-local "hero8" (x=200) shadows persistent "hero8" (x=100).

## Deviations from Plan

None — plan executed exactly as written. The implementation was straightforward since Plan 01 already provided the complete C++ API (persistObject/unpersistObject/findPersistentByName). The TDD cycle went directly to GREEN because Task 1 was implemented first; all 8 test cases passed on first run.

## Issues Encountered

None.

## User Setup Required

None.

## Self-Check: PASSED

---
*Phase: 51-persistent-objects*
*Completed: 2026-03-02*
