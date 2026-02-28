---
phase: 43-tilemap-system
plan: 02
subsystem: scripting
tags: [lua, tilemap, component-proxy, bindings, ecs]

# Dependency graph
requires:
  - phase: 43-01
    provides: C_Tilemap C++ component with setSheet, setTiles, setTile, getTile, setScroll, getScrollX/Y, pixelToTile, tileToPixel, tileAtPixel
  - phase: 39-component-proxy
    provides: ComponentProxy userdata pattern, self:get() dispatch infrastructure, stale-proxy invalidation
provides:
  - C_Tilemap_Proxy Lua metatable with 10 proxy methods accessible via self:get("C_Tilemap")
  - ComponentProxy dispatch entry for "C_Tilemap" type name
  - getSpriteSheet() public accessor on LuaBindings for sprite pool reads
  - tilemap_lua_test.cpp — passing Lua integration tests for TMAP-05..TMAP-08
affects:
  - 44-2d-camera-system (camera may need tilemap scroll integration)
  - future-tilemap-features (extends this proxy pattern)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "C_Tilemap_Proxy follows established C_Timer_Proxy/C_StateMachine_Proxy static-function pattern"
    - "CTILEMAP_PROXY_CHECK macro reduces stale-check boilerplate across 10 proxy methods"
    - "getSpriteSheet() accessor pattern allows static Lua functions to read sprite pool without friendship"
    - "getBindings() made public to allow access from non-member static Lua callback functions"

key-files:
  created:
    - tests/tilemap_lua_test.cpp
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "getBindings(lua_State*) moved to public section — static proxy functions in bindings.cpp are not class members and cannot access private methods; public access is safe (reads Lua registry only)"
  - "getSpriteSheet() returns const pointer — setSheet() via Lua handle binds tileset without exposing mutable sprite pool state"
  - "CTILEMAP_PROXY_CHECK macro used for DRY stale-check preamble across all 10 proxy methods"
  - "setSheet(invalid_handle) raises luaL_error (not silent failure) — early detection of Lua-side handle mistakes"

patterns-established:
  - "C_Tilemap_Proxy: static int lua_tilemap_<method>(lua_State*) functions with CTILEMAP_PROXY_CHECK preamble"
  - "Multi-return Lua functions: pixelToTile, tileToPixel, getScroll, getMapSize all return 2 values via lua_pushinteger x2"

requirements-completed: [TMAP-05, TMAP-06, TMAP-07, TMAP-08]

# Metrics
duration: 4min
completed: 2026-02-28
---

# Phase 43 Plan 02: Tilemap Lua Bindings Summary

**C_Tilemap_Proxy metatable with 10 methods (setTile, getTile, setTiles, setSheet, setScroll, getScroll, pixelToTile, tileToPixel, tileAtPixel, getMapSize) exposed to Lua via self:get("C_Tilemap"), with stale-proxy safety and 9 integration tests passing**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-28T17:23:57Z
- **Completed:** 2026-02-28T17:28:00Z
- **Tasks:** 2 (both TDD)
- **Files modified:** 4

## Accomplishments

- Added C_Tilemap_Proxy metatable to bindings.cpp with all 10 Lua methods from the plan spec
- Added ComponentProxy dispatch for self:get("C_Tilemap") returning typed C_Tilemap_Proxy userdata
- Added getSpriteSheet() public accessor to LuaBindings for safe sprite pool reads from static functions
- Made getBindings() public (was private) to allow non-member static Lua functions to retrieve bindings instance
- Created tilemap_lua_test.cpp with 9 test functions covering all TMAP-05..TMAP-08 requirements, all passing

## Task Commits

1. **Task 1: Add C_Tilemap_Proxy metatable and ComponentProxy dispatch** - `11e18fd` (feat)
2. **Task 2: Create Lua integration test for tilemap bindings** - `22475dc` (test)

## Files Created/Modified

- `/home/unwn/dev/enjin/src/scripting/bindings.cpp` - Added include for tilemap.hpp, C_Tilemap_Proxy proxy methods, __index metamethod, getSpriteSheet() implementation, ComponentProxy dispatch entry, registerComponentProxyMetatable() call
- `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` - Added getSpriteSheet() public accessor, moved getBindings() to public section
- `/home/unwn/dev/enjin/tests/tilemap_lua_test.cpp` - Created with 9 Lua integration tests (TMAP-05..TMAP-08)
- `/home/unwn/dev/enjin/tests/CMakeLists.txt` - Added tilemap_lua_test target inside ENJIN2_BUILD_LUA block

## Decisions Made

- `getBindings(lua_State*)` moved to public section: the static proxy functions in `bindings.cpp` are plain non-member C functions (not class members), so they cannot call private methods. Making it public is safe since it only reads from the Lua registry. This follows the same need as static functions in `bindings_draw.cpp` which use it via the `REQUIRE_CANVAS` macro.
- `getSpriteSheet()` returns `const SpriteSheet*`: gives read access to sprite pool data without allowing mutation. The `lua_tilemap_setSheet` function dereferences it as `tm->setSheet(*sheet)` to pass by value.
- `CTILEMAP_PROXY_CHECK` macro: reduces boilerplate across 10 proxy methods. The macro validates and casts the proxy, naming the C_Tilemap pointer via the `varname` parameter.
- `setSheet(invalid_handle)` raises `luaL_error`: consistent with rest of bindings' error approach; pcall-catchable from Lua side.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] getBindings() was private — inaccessible to non-member static proxy functions**
- **Found during:** Task 1 (build of enjin2_lua)
- **Issue:** `lua_tilemap_setSheet` needed `LuaBindings::getBindings(L)` but `getBindings` was declared private. Static proxy functions are not class members.
- **Fix:** Moved `getBindings(lua_State*)` to the public section of `LuaBindings`. Added documentation comment explaining why it's public.
- **Files modified:** `include/enjin2/scripting/bindings.hpp`
- **Verification:** Library compiles cleanly; full test suite (29/30 runnable tests) passes
- **Committed in:** `11e18fd` (Task 1 commit)

**2. [Rule 1 - Bug] getScriptString() returns std::string, not const char***
- **Found during:** Task 2 (tilemap_lua_test.cpp compile)
- **Issue:** Test code had `const char* errMsg = script->getScriptString(...)` but the method returns `std::string`
- **Fix:** Changed to `std::string errMsg = script->getScriptString(...)` and added `#include <string>`; changed null check to `.empty()` check
- **Files modified:** `tests/tilemap_lua_test.cpp`
- **Verification:** Test compiles and passes
- **Committed in:** `22475dc` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (2x Rule 1 - Bug)
**Impact on plan:** Both fixes required for correctness; no scope creep.

## Issues Encountered

- `sprite_load_test` reports "Not Run" in full ctest run — pre-existing issue (GTest not installed), unrelated to this plan.

## Next Phase Readiness

- Phase 43 complete: C_Tilemap C++ foundation (Plan 01) + Lua bindings (Plan 02) fully tested
- Phase 44 (2D Camera System) can proceed
- Tilemap is available from Lua via self:get("C_Tilemap") with the full coordinate API

---
*Phase: 43-tilemap-system*
*Completed: 2026-02-28*
