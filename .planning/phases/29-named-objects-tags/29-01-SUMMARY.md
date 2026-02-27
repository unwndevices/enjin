---
phase: 29-named-objects-tags
plan: 01
subsystem: core
tags: [cpp, object, tags, identity, zero-allocation, tdd, ctest]

# Dependency graph
requires:
  - phase: 28-float-dt-migration
    provides: Object class with float dt update API already in place
provides:
  - Object::setName / getName (const char* pointer, no allocation)
  - Object::addTag / hasTag / clearTags / getTagCount (up to 8 slots)
  - ObjectCollection::findByName (strcmp guard, null-safe)
  - ObjectCollection::findAllWithTag (caller-provides-buffer pattern)
  - named_objects_test CTest target covering OBJ-01 through OBJ-04
affects: [30-scene-self-transitions, 31-engine-global-table, 32-scriptproxy-userdata]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Zero-allocation name/tag identity: store raw const char* pointers, caller owns lifetime"
    - "8-slot tag array with early-return false on overflow"
    - "Caller-provides-buffer for collection lookups (mirrors existing findObjects template)"
    - "--start-group/--end-group linker flags for tests that use Object without pulling in C_Drawable"

key-files:
  created:
    - tests/named_objects_test.cpp
  modified:
    - include/enjin2/core/object.hpp
    - src/core/object.cpp
    - include/enjin2/core/object_collection.hpp
    - tests/CMakeLists.txt

key-decisions:
  - "Used --start-group/--end-group in named_objects_test CMake target to resolve C_Drawable typeinfo circular reference between enjin2_core.a and enjin2_ui.a"
  - "Test includes drawable.hpp (not just component.hpp) to ensure complete type definitions available"
  - "tagCount and name initialized in both member initializer list and constructor body (tags.fill) per Pitfall 4 from research"

patterns-established:
  - "Tests using Object directly must include drawable.hpp or use --start-group to resolve C_Drawable vtable from enjin2_ui.a"
  - "findByName null-guards both the search string and each object's getName() before strcmp"

requirements-completed: [OBJ-01, OBJ-02, OBJ-03, OBJ-04]

# Metrics
duration: 4min
completed: 2026-02-27
---

# Phase 29 Plan 01: Object Name + Tag Identity Summary

**Zero-heap-allocation name/tag identity on Object using const char* pointers, with ObjectCollection::findByName and findAllWithTag using caller-provides-buffer pattern — all 4 OBJ requirements TDD-verified**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-27T02:02:09Z
- **Completed:** 2026-02-27T02:06:42Z
- **Tasks:** 2 (RED + GREEN)
- **Files modified:** 5

## Accomplishments
- Object carries name (const char*) and up to 8 tag pointers with zero heap allocation
- ObjectCollection::findByName searches by strcmp with full null guards on both sides
- ObjectCollection::findAllWithTag uses caller-provides-buffer pattern matching existing findObjects template
- named_objects_test covers all 4 OBJ requirements with 29 assertions, 0 failures
- All 5 existing non-visual ctest targets continue to pass after changes

## Task Commits

Each task was committed atomically:

1. **Task RED: Write failing named_objects_test** - `29b0d47` (test)
2. **Task GREEN: Implement name/tag fields and collection lookup** - `1393ef9` (feat)

**Plan metadata:** pending (docs commit below)

_Note: TDD plan — RED commit fails to compile (methods don't exist); GREEN commit makes all tests pass._

## Files Created/Modified
- `tests/named_objects_test.cpp` - Test file covering OBJ-01 through OBJ-04, ASSERT macro pattern, 29 assertions
- `include/enjin2/core/object.hpp` - Added name field, MAX_TAGS=8, tags array, tagCount (private); setName/getName/addTag/hasTag/clearTags/getTagCount (public inline)
- `src/core/object.cpp` - Updated constructor initializer list (name, tagCount) and body (tags.fill)
- `include/enjin2/core/object_collection.hpp` - Added findByName and findAllWithTag non-template inline methods
- `tests/CMakeLists.txt` - Registered named_objects_test as CTest target; used --start-group/--end-group for linker

## Decisions Made
- Used `--start-group`/`--end-group` linker flags for named_objects_test to handle a pre-existing circular reference: `enjin2_core.a` (object.cpp.o) references `typeinfo for C_Drawable`, which lives in `enjin2_ui.a`. Tests that include `drawable.hpp` transitively (sprite_test) resolve this automatically; tests that only use Object must use the group flag.
- Included `<enjin2/components/drawable.hpp>` in named_objects_test.cpp (instead of just component.hpp) to provide complete type definitions for the dynamic_cast in Object's addComponent template.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added --start-group/--end-group linker flags and updated includes for named_objects_test**
- **Found during:** Task GREEN (link step after GREEN implementation)
- **Issue:** named_objects_test.cpp.o did not pull in `drawable.cpp.o` from libenjin2_ui.a (no C_Drawable symbols referenced), so `object.cpp.o`'s unresolved reference to `typeinfo for C_Drawable` could not be satisfied. Spray_test works because it includes sprite.hpp->drawable.hpp, causing drawable.cpp.o to be included first.
- **Fix:** Changed named_objects_test CMakeLists.txt to link with `--start-group enjin2_core enjin2_graphics enjin2_ui enjin2_input --end-group` (allows repeated archive scanning). Added `#include <enjin2/components/drawable.hpp>` to named_objects_test.cpp.
- **Files modified:** tests/CMakeLists.txt, tests/named_objects_test.cpp
- **Verification:** cmake --build succeeds, all 29 test assertions pass
- **Committed in:** 1393ef9 (Task GREEN commit)

---

**Total deviations:** 1 auto-fixed (1 blocking linker issue)
**Impact on plan:** Required fix to resolve a pre-existing circular dependency between enjin2_core.a and enjin2_ui.a that only surfaces in tests that use Object without also using drawable components. No scope creep.

## Issues Encountered
- Pre-existing linker issue: enjin2_core.a's object.cpp.o references `typeinfo for C_Drawable` (from dynamic_cast in addComponent template), but C_Drawable typeinfo lives in enjin2_ui.a. Tests that don't pull in drawable symbols need `--start-group`/`--end-group` to allow repeated archive scanning.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- OBJ-01 through OBJ-04 complete; name and tag identity fully tested
- ObjectCollection lookup API (findByName, findAllWithTag) available for use by scene and scripting layers
- Phase 29 Plan 02 can proceed (tags-based filtering or second part of named objects work)
- No blockers introduced

---
*Phase: 29-named-objects-tags*
*Completed: 2026-02-27*
