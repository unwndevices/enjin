---
phase: 63-lua-profiler-headless-runner
plan: 01
subsystem: scripting
tags: [lua, lua_sethook, lua_Debug, profiler, headless, hooks, unit-test]

# Dependency graph
requires:
  - phase: 61-native-benchmark-suite
    provides: LuaEngine + LuaBindings headless pattern (bench_lua.cpp)
  - phase: 62-frame-timing-instrumentation
    provides: instrumentation singleton pattern (FrameTimingInstrumentation)
provides:
  - LuaProfiler header-only singleton with hookCallback, install, uninstall, reset, printTable, printJSON, sortByCount
  - lua_profiler_test with 6 passing unit tests (PROF-01, PROF-02, PROF-03, PROF-06)
  - lua_profiler_test registered in ctest (test #46, 46/46 pass)
affects: [63-02, headless-runner, enjin_run]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "LuaProfiler: Meyer's singleton, fixed 256-slot FuncEntry table, lua_topointer for stable function identity"
    - "hookCallback: lua_getinfo(nSf) inside hook to populate ar->name/short_src/linedefined; lua_pop after f option"
    - "PROF-03 zero-overhead: lua_sethook(L, NULL, 0, 0) — explicit disabled path confirmed"
    - "null-safe headless wiring: setLayers + setDebugCanvas required; currentInput/m_ssm/m_activeScene safely nil"

key-files:
  created:
    - include/enjin2/scripting/lua_profiler.hpp
    - tests/lua_profiler_test.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "lua_profiler.hpp includes lua_platform.hpp (not raw lua.h) for cross-platform Lua include guard"
  - "hookCallback checks active flag AFTER lua_getinfo+lua_topointer to avoid stack imbalance on early return"
  - "null_safety test uses static LayerCompositor<128,128> + LuaCanvas wrappers — zero heap allocation"
  - "LuaScriptSystem used for null_safety test (not bare LuaEngine+LuaBindings) to mirror headless runner pattern"

patterns-established:
  - "Pattern: Install profiler hook BEFORE loadScript so module-level function calls are counted"
  - "Pattern: Uninstall hook BEFORE g_lua.shutdown() to avoid hook firing during GC cleanup (lua_close)"
  - "Pattern: Guard ar->name with if (ar->name) check; use '[?]' fallback for C functions / anonymous closures"

requirements-completed: [PROF-01, PROF-02, PROF-03, PROF-06]

# Metrics
duration: 2min
completed: 2026-03-08
---

# Phase 63 Plan 01: LuaProfiler Header-Only Singleton Summary

**C-level Lua call-count profiler via lua_sethook(LUA_MASKCALL) with fixed 256-slot function table, zero-overhead disabled path (PROF-03), and 6 passing unit tests covering PROF-01/02/03/06**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-08T07:40:01Z
- **Completed:** 2026-03-08T07:42:34Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- LuaProfiler header-only singleton implementing PROF-01 (call count tracking via lua_sethook), PROF-02 (GC memory query), PROF-03 (zero-overhead disabled path via lua_sethook NULL), and PROF-06 (null-safe engine.* subtable calls in headless mode)
- 6 unit tests all passing: hook install/uninstall lifecycle, zero-overhead disabled, call count accuracy, GC memory query, null safety (headless wiring with static 128x128 compositor), sort by count
- lua_profiler_test registered as ctest #46 — full suite 46/46 passes with zero regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Create LuaProfiler header-only singleton** - `62986a3` (feat)
2. **Task 2: Create lua_profiler_test with unit tests and CMake target** - `596aa91` (feat)

**Plan metadata:** TBD (docs: complete plan)

## Files Created/Modified

- `include/enjin2/scripting/lua_profiler.hpp` — LuaProfiler struct with Meyer's singleton, hookCallback, install/uninstall, sortByCount, printTable, printJSON
- `tests/lua_profiler_test.cpp` — 6 unit tests for PROF-01/02/03/06 (12 ASSERT checks, all passing)
- `tests/CMakeLists.txt` — lua_profiler_test executable target inside if(ENJIN2_BUILD_LUA) block

## Decisions Made

- `lua_profiler.hpp` includes `lua_platform.hpp` (not raw lua.h) to respect the project's cross-platform Lua include guard pattern (VCV_RACK vs ESP32)
- `hookCallback` checks `active` flag AFTER `lua_getinfo + lua_topointer` to avoid Lua stack imbalance on early return (if active check were before lua_getinfo, returning early without lua_pop would corrupt the stack for the "f" option case)
- `test_null_safety` uses `LuaScriptSystem` (not bare LuaEngine+LuaBindings) to mirror the headless runner pattern
- Static `LayerCompositor<128,128>` + static `LuaCanvas` wrappers for null_safety test — zero heap allocation matching the research pattern

## Deviations from Plan

None — plan executed exactly as written. Header structure, test coverage, and CMake target placement all match the plan specification precisely.

## Issues Encountered

None — all 6 tests passed on first build. No compilation errors, no test failures.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- LuaProfiler singleton is ready for use in Phase 63 Plan 02 (headless enjin_run runner)
- `install()` / `uninstall()` / `reset()` / `printTable()` / `printJSON()` API is stable
- null-safe headless canvas wiring pattern (static LayerCompositor + LuaCanvas) established and verified
- All 46 ctest tests green, no regressions

## Self-Check: PASSED

- `include/enjin2/scripting/lua_profiler.hpp` — FOUND
- `tests/lua_profiler_test.cpp` — FOUND
- `.planning/phases/63-lua-profiler-headless-runner/63-01-SUMMARY.md` — FOUND
- Commit `62986a3` (Task 1) — FOUND
- Commit `596aa91` (Task 2) — FOUND

---
*Phase: 63-lua-profiler-headless-runner*
*Completed: 2026-03-08*
