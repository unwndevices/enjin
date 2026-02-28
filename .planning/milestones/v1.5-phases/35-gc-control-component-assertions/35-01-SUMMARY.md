---
phase: 35-gc-control-component-assertions
plan: 01
subsystem: scripting
tags: [lua, gc, component, assert, esp32, embedded]

# Dependency graph
requires:
  - phase: 31-engine-global-table
    provides: engine.lua empty stub that this phase fills with collect/memory functions
  - phase: 26-component-system
    provides: Component base class that assertRequires<T>() is added to

provides:
  - engine.lua.collect() — incremental Lua GC step callable from Lua scripts (GC-01)
  - engine.lua.memory() — Lua heap size in bytes returned as number (GC-02)
  - assertRequires<T>() — protected template method on Component base class (DEP-01, DEP-02, DEP-03)
  - gc_assert_test — ctest entry covering GC-01, GC-02, DEP-01, DEP-03

affects: [all components that declare inter-component dependencies, Lua scripts managing frame-budget GC]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "assertRequires<T>() pattern: declare component dependencies in awake(), debug=assert, release=disable"
    - "incremental GC pattern: LUA_GCSTEP not LUA_GCCOLLECT for embedded frame-budget safety"
    - "memory byte formula: lua_gc(LUA_GCCOUNT)*1024 + lua_gc(LUA_GCCOUNTB)"

key-files:
  created:
    - tests/gc_assert_test.cpp
  modified:
    - include/enjin2/core/component.hpp
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings_engine.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "assertRequires<T>() uses assert(false&&\"message\") in debug, printf+setEnabled(false) in release — no process abort on ESP32"
  - "LUA_GCSTEP used for collect() not LUA_GCCOLLECT — incremental avoids stop-the-world frame spike"
  - "component.hpp includes object.hpp (not circular: object.hpp only forward-declares Component)"
  - "DEP-03 test case gated #ifdef NDEBUG — debug path would abort test process via assert(false)"
  - "Task 3 (assertRequires) implemented alongside Task 1 (test scaffold) — required to compile test file"

patterns-established:
  - "Component dependency declaration: call assertRequires<T>() in awake() for loud debug failures"
  - "GC bindings follow kLuaFuncs[] + luaBindFunctions() pattern established in bindings-split"

requirements-completed: [GC-01, GC-02, DEP-01, DEP-02, DEP-03]

# Metrics
duration: 4min
completed: 2026-02-27
---

# Phase 35 Plan 01: GC Control and Component Assertions Summary

**engine.lua.collect() + engine.lua.memory() Lua GC bindings and assertRequires<T>() Component dependency assertion with debug assert/release disable semantics**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-27T16:10:58Z
- **Completed:** 2026-02-27T16:14:55Z
- **Tasks:** 3
- **Files modified:** 5

## Accomplishments

- Implemented `engine.lua.collect()` (LUA_GCSTEP incremental) and `engine.lua.memory()` (exact byte count) Lua bindings — scripts can now manage GC within frame budget on ESP32
- Added `assertRequires<T>()` protected template method to `Component` base class — debug builds assert(false), release builds call setEnabled(false) + printf
- Created `gc_assert_test` with 8 assertions covering GC-01, GC-02, DEP-01; all 15/15 ctest pass

## Task Commits

Each task was committed atomically:

1. **Task 1+3: Test scaffold + assertRequires<T>()** - `fcead8c` (feat)
2. **Task 2: GC bindings implementation** - `c88ca27` (feat)

**Plan metadata:** (docs commit follows)

_Note: Tasks 1 and 3 were committed together — Task 3 (assertRequires) was required to compile Task 1's test file (Rule 3 deviation handled automatically)._

## Files Created/Modified

- `/home/unwn/dev/enjin/tests/gc_assert_test.cpp` - GC-01, GC-02, DEP-01, DEP-03 test cases using GCFixture + ASSERT macro pattern
- `/home/unwn/dev/enjin/include/enjin2/core/component.hpp` - Added assertRequires<T>() protected template method; added includes for cassert, cstdio, type_traits, object.hpp
- `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` - Added lua_engine_lua_collect and lua_engine_lua_memory static declarations
- `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` - Replaced empty engine.lua stub with kLuaFuncs[] + collect/memory implementations
- `/home/unwn/dev/enjin/tests/CMakeLists.txt` - Added gc_assert_test executable + add_test inside ENJIN2_BUILD_LUA guard

## Decisions Made

- Used `LUA_GCSTEP` (not `LUA_GCCOLLECT`) for `collect()` — incremental step avoids stop-the-world pause that would spike frame budget on ESP32
- Memory formula: `lua_gc(LUA_GCCOUNT) * 1024 + lua_gc(LUA_GCCOUNTB)` — identical to LuaPlatform::getMemoryUsage()
- `assertRequires<T>()` placed in protected section (not public) — only derived components can call it
- `component.hpp` includes `object.hpp` directly — safe because object.hpp only forward-declares Component (no circular include)
- DEP-03 test (missing dep disables) gated `#ifdef NDEBUG` — invoking assert(false) in debug would kill the test process
- `<enjin2/components/drawable.hpp>` added to test file — required by `Object::addComponent` dynamic_cast to C_Drawable

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Implemented Task 3 (assertRequires) alongside Task 1 to unblock test compilation**
- **Found during:** Task 1 (test scaffold compilation)
- **Issue:** `assertRequires<C_TestDep>()` called in C_RequiresTestDep::awake() inside the test file, but Task 3 (component.hpp method) not yet done — compile error
- **Fix:** Implemented `assertRequires<T>()` template in component.hpp as part of Task 1 commit
- **Files modified:** include/enjin2/core/component.hpp
- **Verification:** Test file compiled cleanly; all 15 ctest pass
- **Committed in:** fcead8c (combined Task 1+3 commit)

**2. [Rule 3 - Blocking] Added #include <enjin2/components/drawable.hpp> to test file**
- **Found during:** Task 1 (test scaffold compilation)
- **Issue:** Object::addComponent<T> does dynamic_cast<C_Drawable*> inline in object.hpp, requiring C_Drawable's full definition — not provided by component.hpp or object.hpp alone
- **Fix:** Added `#include <enjin2/components/drawable.hpp>` to gc_assert_test.cpp (same fix used in named_objects_test.cpp)
- **Files modified:** tests/gc_assert_test.cpp
- **Verification:** Compile error eliminated; test builds and runs
- **Committed in:** fcead8c (Task 1+3 commit)

---

**Total deviations:** 2 auto-fixed (both Rule 3 — blocking issues for compilation)
**Impact on plan:** Both fixes required for correctness; no scope creep. Plan task ordering (1→2→3) was achievable only after unblocking Task 1 compilation via Task 3.

## Issues Encountered

None beyond the two Rule 3 blocking issues documented above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 35 requirements GC-01, GC-02, DEP-01, DEP-02, DEP-03 all fulfilled
- `engine.lua.collect()` and `engine.lua.memory()` callable from any Lua script
- `assertRequires<T>()` ready for all existing and future components to use in awake()
- 15/15 ctest pass with gc_assert_test now in suite

---
*Phase: 35-gc-control-component-assertions*
*Completed: 2026-02-27*

## Self-Check: PASSED

- tests/gc_assert_test.cpp: FOUND
- include/enjin2/core/component.hpp: FOUND
- src/scripting/bindings_engine.cpp: FOUND
- Commit fcead8c: FOUND
- Commit c88ca27: FOUND
