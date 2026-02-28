# Phase 35 Verification: GC Control + Component Assertions

**Verified:** 2026-02-27
**Phase:** 35-gc-control-component-assertions
**Requirements:** GC-01, GC-02, DEP-01, DEP-02, DEP-03

## Verification Method

Code inspection + automated ctest execution. All requirements verified via gc_assert_test (8 assertions, 0 failures) included in the 19-test ctest suite that passes as of Phase 38-02.

## Requirements

### GC-01: engine.lua.collect() triggers incremental GC step

**Status:** VERIFIED
**Evidence:** `src/scripting/bindings_engine.cpp` — lua_engine_lua_collect() calls `lua_gc(L, LUA_GCSTEP, 0)` (incremental step, not LUA_GCCOLLECT which would cause stop-the-world pause on ESP32).
**Test:** gc_assert_test `test_gc_collect_no_crash` — calls engine.lua.collect() and asserts no Lua error.

### GC-02: engine.lua.memory() returns current Lua heap size in bytes

**Status:** VERIFIED
**Evidence:** `src/scripting/bindings_engine.cpp` — lua_engine_lua_memory() returns `lua_gc(L, LUA_GCCOUNT, 0) * 1024 + lua_gc(L, LUA_GCCOUNTB, 0)` (exact byte count, same formula as LuaPlatform::getMemoryUsage()).
**Test:** gc_assert_test `test_gc_memory_returns_number` — asserts memory() > 0 after Lua state initialization.

### DEP-01: Component base class provides assertRequires<T>() protected template method

**Status:** VERIFIED
**Evidence:** `include/enjin2/core/component.hpp` — assertRequires<T>() defined as a protected template method that calls getOwner()->getComponent<T>() and branches on debug vs. release behavior.
**Test:** gc_assert_test `test_assert_requires_passes_when_dep_present` — assertRequires<C_TestDep>() completes without abort when dependency is present.

### DEP-02: In debug builds, missing dependency triggers assertion with clear error message

**Status:** VERIFIED (by code inspection; not directly run in ctest — see note)
**Evidence:** `include/enjin2/core/component.hpp` — assertRequires<T>() calls `assert(false && "message")` in debug builds when T not found. The message names both components.
**Note:** The debug path is not run in automated tests because assert(false) would abort the test process. Correctness is verified by reading the #ifndef NDEBUG branch in component.hpp.

### DEP-03: In release builds, missing dependency logs once and disables the component

**Status:** VERIFIED
**Evidence:** `include/enjin2/core/component.hpp` — the #else (release) branch calls `printf(...)` and `setEnabled(false)` when T is not found.
**Test:** gc_assert_test `test_assert_requires_disables_in_release` (gated #ifdef NDEBUG) — confirms component is disabled when built without NDEBUG=0. The test is compiled into gc_assert_test but only runs the release-build branch.

## ctest Output

Run: `cd build && ctest -R gc_assert_test --output-on-failure`
Result: PASSED (confirmed as part of 19/19 ctest suite passing in Phase 38-02)

## Files Verified

- `include/enjin2/core/component.hpp` — assertRequires<T>() template
- `src/scripting/bindings_engine.cpp` — engine.lua.collect() and engine.lua.memory() implementations
- `tests/gc_assert_test.cpp` — 8 assertions covering GC-01, GC-02, DEP-01, DEP-03
- `tests/CMakeLists.txt` — gc_assert_test registered as ctest target (ENJIN2_BUILD_LUA guard)
