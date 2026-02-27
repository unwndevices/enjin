---
phase: 33-scripterrorpolicy
plan: 02
subsystem: scripting/components
tags: [lua, error-policy, C_LuaScript, ctest, unit-test]

requires:
  - phase: 33-01
    provides: lua_script.cpp compiled into enjin2_lua; lua_script.hpp reconciled to LuaScriptSystem/LuaCanvas
provides:
  - ScriptErrorPolicy enum (Disable/Log/Panic) in enjin2 namespace in lua_script.hpp
  - callWithProxy() policy dispatch — Disable sets scriptError, Log does not, Panic calls std::abort
  - error_policy_test with 20 assertions covering ERR-01 through ERR-05
  - error_policy_test registered as CTest target (13th test)
affects: [Phase 34 onwards, any C_LuaScript consumers]

tech-stack:
  added: []
  patterns:
    - "ScriptErrorPolicy enum class : uint8_t with Disable/Log/Panic values; default Disable"
    - "Policy dispatch in callWithProxy() via switch(errorPolicy) after lua_pcall non-LUA_OK"
    - "Panic policy verified by field value only — live abort not exercised in tests to avoid killing test process"
    - "error_policy_test uses Object::addComponent<C_LuaScript> pattern to test C_LuaScript directly"

key-files:
  created:
    - tests/error_policy_test.cpp
  modified:
    - include/enjin2/components/lua_script.hpp
    - src/components/lua_script.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Policy dispatch moved INTO callWithProxy() — the 'callers decide' comment was the explicit target; single error capture point dispatches based on errorPolicy field"
  - "ERR-04 (Panic) tested by field value only — std::abort() kills the test process; live invocation excluded from automated tests"
  - "Test uses Object::addComponent<C_LuaScript>(16u, 16u) for direct C_LuaScript policy testing — no LuaEngine fallback needed; Object construction is straightforward"
  - "Log policy logs to stdout (visible in test output) but does not set scriptError — expected and intentional per ERR-03 spec"

patterns-established:
  - "ScriptErrorPolicy: enum class with Disable(0)/Log(1)/Panic(2); Disable is always the default"
  - "callWithProxy() is the single error capture point — policy dispatch belongs here, not in update()/draw() callers"

requirements-completed: [ERR-01, ERR-02, ERR-03, ERR-04, ERR-05]

duration: 3min
completed: 2026-02-27
---

# Phase 33 Plan 02: ScriptErrorPolicy Dispatch and Tests Summary

**ScriptErrorPolicy enum (Disable/Log/Panic) wired into callWithProxy() with 20-assertion test covering ERR-01..ERR-05; 13/13 ctest tests pass.**

## Performance

- **Duration:** ~3 min
- **Started:** 2026-02-27T15:16:56Z
- **Completed:** 2026-02-27T15:19:01Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- ScriptErrorPolicy enum declared in enjin2 namespace in lua_script.hpp with Disable/Log/Panic values and errorPolicy member defaulting to Disable
- callWithProxy() now dispatches on errorPolicy after lua_pcall failure: Disable logs once and sets scriptError=true; Log logs every frame without setting scriptError; Panic calls printf then std::abort() (ESP32: esp_restart())
- error_policy_test.cpp covers all five requirements with 20 assertions using Object::addComponent<C_LuaScript> directly; all pass
- ctest passes 13/13 tests (8 pre-existing + 5 Lua tests + new error_policy_test)

## Task Commits

1. **Task 1: Add ScriptErrorPolicy enum and field to lua_script.hpp** - `dc03653` (feat)
2. **Task 2: Implement policy dispatch in callWithProxy() and write error_policy_test** - `8affc10` (feat)

## Files Created/Modified

- `include/enjin2/components/lua_script.hpp` - ScriptErrorPolicy enum class, errorPolicy member, setErrorPolicy/getErrorPolicy accessors
- `src/components/lua_script.cpp` - switch(errorPolicy) dispatch in callWithProxy(); #include <cstdlib> added
- `tests/error_policy_test.cpp` - 20 assertions covering ERR-01..ERR-05 using C_LuaScript directly
- `tests/CMakeLists.txt` - error_policy_test registered as CTest target within ENJIN2_BUILD_LUA block

## Decisions Made

1. **Policy dispatch in callWithProxy()** — The plan explicitly targeted the "callers decide" comment in callWithProxy() as the dispatch insertion point. This is the single error capture point; dispatching here keeps update()/draw() callers unchanged.

2. **ERR-04 (Panic) not live-invoked in tests** — std::abort() terminates the process. The test verifies the policy field stores and retrieves Panic correctly and performs a round-trip back to Disable. Source inspection confirms the abort branch is present. This approach is documented in the research as the correct strategy.

3. **Object::addComponent<C_LuaScript> used directly** — The plan noted a fallback to LuaEngine-only testing if Object construction was complex. Object() has a default constructor and addComponent<C_LuaScript>(16u, 16u) works without Scene or ObjectCollection infrastructure, so the preferred C_LuaScript approach was used.

4. **Log policy stdout output visible in test run** — ERR-03 intentionally exercises the log path; the "[lua] script error" lines in test output are correct behavior, not noise.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None — callWithProxy() compiled cleanly on first build after adding the policy switch. The test compiled and all 20 assertions passed on first execution.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- ScriptErrorPolicy is fully implemented and tested — Phase 33 is complete
- Phase 34 (or any C_LuaScript consumer) can rely on setErrorPolicy()/getErrorPolicy() and the three policy behaviors
- ERR-01 through ERR-05 requirements satisfied and covered by automated tests

---
*Phase: 33-scripterrorpolicy*
*Completed: 2026-02-27*

## Self-Check: PASSED

- [x] include/enjin2/components/lua_script.hpp — ScriptErrorPolicy enum + errorPolicy field + accessors present
- [x] src/components/lua_script.cpp — switch(errorPolicy) dispatch in callWithProxy()
- [x] tests/error_policy_test.cpp — 20 assertions, ERR-01..ERR-05
- [x] tests/CMakeLists.txt — error_policy_test registered as CTest target
- [x] .planning/phases/33-scripterrorpolicy/33-02-SUMMARY.md — exists
- [x] Commit dc03653 — feat(33-02): ScriptErrorPolicy enum in header
- [x] Commit 8affc10 — feat(33-02): policy dispatch + error_policy_test
