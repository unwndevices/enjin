---
phase: 49-coroutine-async-scheduler
plan: 02
subsystem: scripting
tags: [lua, coroutine, esp32, embedded, async]

# Dependency graph
requires:
  - phase: 49-01
    provides: engine.async.* Lua bindings (scheduler relies on coroutine library being available)
provides:
  - luaopen_coroutine registered on ESP32 via luaL_requiref in openEmbeddedLibraries()
affects: [49-03, any plan relying on engine.async.* on ESP32]

# Tech tracking
tech-stack:
  added: []
  patterns: [ESP32 selective library loading — luaL_requiref pattern for each library, lightweight coroutine library safe to open unconditionally]

key-files:
  created: []
  modified:
    - src/scripting/lua_platform.cpp

key-decisions:
  - "Coroutine library placed unconditionally (not behind free_heap > 200KB guard) — it is lightweight (no heap allocation, no I/O) and required for correct engine.async.* operation"

patterns-established:
  - "ESP32 library registration pattern: luaL_requiref then lua_pop; coroutine follows same pattern as base/math/string/table"

requirements-completed: [ASYNC-04]

# Metrics
duration: 1min
completed: 2026-03-01
---

# Phase 49 Plan 02: Coroutine Library ESP32 Registration Summary

**luaopen_coroutine registered in ESP32 openEmbeddedLibraries() via luaL_requiref, enabling coroutine.create/resume/yield and engine.async.* scheduler on embedded targets**

## Performance

- **Duration:** ~1 min
- **Started:** 2026-03-01T21:20:38Z
- **Completed:** 2026-03-01T21:21:14Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- Added `luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1)` to the ESP32 `openEmbeddedLibraries()` function
- Positioned after the table library block and before the conditional UTF8 block, matching the existing registration pattern
- Desktop and WASM builds are unaffected — `luaL_openlibs()` already includes all standard libraries including coroutine

## Task Commits

Each task was committed atomically:

1. **Task 1: Add luaopen_coroutine to ESP32 openEmbeddedLibraries** - `77151a9` (feat)

**Plan metadata:** (docs commit — see below)

## Files Created/Modified
- `src/scripting/lua_platform.cpp` - Added coroutine library registration inside `#ifdef ESP32` `openEmbeddedLibraries()` block

## Decisions Made
- Coroutine library placed unconditionally (not behind the `free_heap > 200KB` guard used for UTF8) because it is lightweight (no heap allocation, no I/O) and is required for correct `engine.async.*` scheduler operation

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- ESP32 now supports coroutine.create/resume/yield and engine.async.* at the library level
- Phase 49-03 can proceed with scheduler implementation knowing the coroutine library is available on all target platforms (desktop via luaL_openlibs, ESP32 via this registration)

---
*Phase: 49-coroutine-async-scheduler*
*Completed: 2026-03-01*
