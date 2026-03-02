---
phase: 46-bindings-refactoring-null-safety
plan: 02
subsystem: scripting
tags: [lua, bindings, null-safety, testing, gtest, event-bus, proxy]

# Dependency graph
requires:
  - phase: 46-01
    provides: bindings monolith split into bindings_proxy.cpp + bindings_internal.hpp

provides:
  - LuaWrapper convenience class (header-only) combining LuaEngine + LuaBindings for test fixtures
  - Fixed sprite_load_test.cpp compilation (ICanvas abstract class issue + LuaCanvas constructor fix)
  - Null safety audit confirmation: all numeric-returning binding functions have safe defaults
  - overflow_test.cpp covering EventBus channel/subscriber overflow and proxy destruction safety
  - overflow_test registered in tests/CMakeLists.txt as ctest target

affects:
  - phases 47-52 (any phase adding new binding functions must follow null-guard pattern)
  - sprite_load_test (now compiles and passes via lua_wrapper.hpp)
  - overflow_test (new permanent test coverage for boundary conditions)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "LuaWrapper header-only class: engine declared before bindings (C++ member init order)"
    - "Null guard pattern for numeric bindings: if (!b) { lua_pushinteger(L, 0); return 1; }"
    - "Null guard pattern for void bindings: if (!b) return 0;"
    - "Proxy validity: luaL_error on stale proxy (longjmp — no push needed)"
    - "Overflow test: C_LuaScript-based fixture with flat globals (not table indexing)"

key-files:
  created:
    - include/enjin2/scripting/lua_wrapper.hpp
    - tests/overflow_test.cpp
  modified:
    - tests/sprite_load_test.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "LuaWrapper is header-only (delegates to LuaEngine + LuaBindings .cpp implementations)"
  - "sprite_load_test fixed: ICanvas<Pixel4> (abstract) -> Canvas4<64,64> (concrete); LuaCanvas(&canvas, true) -> LuaCanvas(&canvas)"
  - "All binding files already null-safe on audit — no gaps found in bindings_physics.cpp or bindings_math.cpp"
  - "overflow_test uses C_LuaScript fixture (not raw LuaEngine) because LuaEventBus::subscribe() requires m_L != nullptr"
  - "overflow_test uses flat Lua globals for C++ getScriptNumber() queries (lua_getglobal does not support table[index] syntax)"

patterns-established:
  - "LuaWrapper pattern: header-only class with engine/bindings as public members in declaration order"
  - "Test overflow pattern: use Lua-level loops + boolean flags + flat globals for C++ verification"

requirements-completed: [BIND-02, TEST-01, TEST-02]

# Metrics
duration: 7min
completed: 2026-03-01
---

# Phase 46 Plan 02: Null Safety Guards + lua_wrapper.hpp + overflow_test Summary

**Header-only LuaWrapper class for GTest fixtures, fixed sprite_load_test ICanvas/constructor bug, confirmed all binding null guards compliant, and wrote overflow_test for EventBus and proxy destruction boundaries**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-01T18:34:14Z
- **Completed:** 2026-03-01T18:41:35Z
- **Tasks:** 3
- **Files modified:** 4 (1 created header, 1 created test, 1 fixed test, 1 updated CMakeLists)

## Accomplishments
- Created `lua_wrapper.hpp` — header-only LuaWrapper class satisfying all APIs expected by sprite_load_test
- Fixed sprite_load_test.cpp: replaced invalid `ICanvas<Pixel4> canvas(64,64)` (abstract) with `Canvas4<64,64>` and removed invalid two-arg LuaCanvas constructor call — all 10 GTest cases now pass
- Audited all binding files and confirmed full null-safety compliance — no gaps found
- Created `overflow_test.cpp` with 3 test functions covering EventBus channel overflow (17th channel = 0), subscriber overflow (9th sub on same channel = 0), and component destruction proxy safety (reload + Object deletion without crash)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create lua_wrapper.hpp and fix sprite_load_test.cpp** - `2debc28` (feat)
2. **Task 2: Null safety audit of all binding files** - `03a8816` (feat, no-op — all files already compliant)
3. **Task 3: Write overflow_test.cpp and register in CMakeLists.txt** - `26b419c` (feat)

**Plan metadata:** (docs commit — follows this summary)

## Files Created/Modified
- `include/enjin2/scripting/lua_wrapper.hpp` - Header-only LuaWrapper class for test fixtures
- `tests/sprite_load_test.cpp` - Fixed abstract ICanvas usage and two-arg LuaCanvas constructor
- `tests/overflow_test.cpp` - Overflow boundary tests: EventBus channels, subscribers, proxy safety
- `tests/CMakeLists.txt` - Added overflow_test build target and ctest registration

## Decisions Made
- LuaWrapper is header-only (delegates to LuaEngine + LuaBindings .cpp); no companion .cpp needed
- Fixed sprite_load_test: `ICanvas<Pixel4>` is abstract — used `Canvas4<64, 64>` instead; `LuaCanvas(&canvas, true)` has no matching constructor — used `LuaCanvas(&canvas)` (the ICanvas<Pixel4>* overload)
- overflow_test uses C_LuaScript (not raw LuaEngine) because LuaEventBus::subscribe() guards on `m_L != nullptr` and needs a running Lua state for callback refs
- Flat globals in Lua scripts (not tables) because C_LuaScript::getScriptNumber() calls lua_getglobal() which does not support `ids[16]` table indexing syntax

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] sprite_load_test uses abstract ICanvas<Pixel4> as concrete type**
- **Found during:** Task 1 (Create lua_wrapper.hpp and fix sprite_load_test.cpp)
- **Issue:** Line 146 had `ICanvas<Pixel4> canvas(64, 64)` — ICanvas is an abstract class, cannot be instantiated. Plan only mentioned fixing line 147 (the two-arg LuaCanvas constructor), but line 146 had a separate compilation error.
- **Fix:** Changed to `Canvas4<64, 64> canvas;` — the concrete 4-bit canvas type used throughout the test suite. Canvas4<W,H> is available via the include chain (lua_wrapper.hpp -> bindings.hpp -> canvas.hpp).
- **Files modified:** tests/sprite_load_test.cpp
- **Verification:** sprite_load_test builds and all 10 GTest cases pass
- **Committed in:** 2debc28 (Task 1 commit)

**2. [Rule 1 - Bug] overflow_test OVERFLOW-04 pure-C++ LuaEventBus test fails due to m_L guard**
- **Found during:** Task 3 (Write overflow_test.cpp)
- **Issue:** Initial design included OVERFLOW-04 testing LuaEventBus directly via C++ with no Lua state. LuaEventBus::subscribe() guards `if (!m_L || !name) { return 0; }` — all subscriptions returned 0 without a Lua state, making the C++ direct test fail trivially (all assertions about non-zero IDs fail).
- **Fix:** Removed OVERFLOW-04 from overflow_test.cpp. The Lua-script-based OVERFLOW-01 and OVERFLOW-02 tests already cover the same overflow boundary with a valid Lua state.
- **Files modified:** tests/overflow_test.cpp
- **Verification:** overflow_test passes 21/21 assertions
- **Committed in:** 26b419c (Task 3 commit)

---

**Total deviations:** 2 auto-fixed (Rule 1 - Bug)
**Impact on plan:** Both fixes were necessary for correctness. No scope creep.

## Issues Encountered
- `getScriptNumber()` uses `lua_getglobal()` directly — does not support table[index] syntax. Initial overflow test design used `ids[i]` table access which returned default values. Fixed by restructuring Lua scripts to use flat boolean flags (`all_valid`, `all_subs_valid`) for the overflow detection, which maps cleanly to `getScriptBool()`.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- lua_wrapper.hpp is ready for use by any future GTest-based test needing a Lua+bindings fixture
- All binding null-safety patterns documented and consistent across all files
- overflow_test provides permanent regression coverage for EventBus capacity limits
- Phase 46 is now complete (both plans done)
- Phase 47 can proceed without any blockers from Phase 46

---
*Phase: 46-bindings-refactoring-null-safety*
*Completed: 2026-03-01*

## Self-Check: PASSED

All artifacts verified:
- FOUND: include/enjin2/scripting/lua_wrapper.hpp
- FOUND: tests/overflow_test.cpp
- FOUND: .planning/phases/46-bindings-refactoring-null-safety/46-02-SUMMARY.md
- FOUND commit: 2debc28 (Task 1)
- FOUND commit: 03a8816 (Task 2)
- FOUND commit: 26b419c (Task 3)
