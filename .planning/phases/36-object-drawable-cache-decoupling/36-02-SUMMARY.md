---
phase: 36-object-drawable-cache-decoupling
plan: 02
subsystem: core
tags: [component-system, drawable, scene, animation, ctest, cmake]

# Dependency graph
requires:
  - phase: 36-01
    provides: getComponents<T>() generic template on Object; C_Drawable cache removed from Object; enjin2_core->enjin2_ui layering violation eliminated
provides:
  - scene.hpp renderObjects() using getComponents<C_Drawable>() — no getDrawable() calls
  - animation.hpp color track using getComponent<C_Drawable>() — no getDrawable() loop
  - drawable_decoupling_test verifying getComponents<C_Drawable>() count and pointer correctness
  - All 16 ctest tests pass with simplified named_objects_test link (no --start-group required)
affects: [37-address-prominent-codebase-concerns]

# Tech tracking
tech-stack:
  added: []
  patterns: [getComponents<T>(out, maxOut) caller-provides-buffer pattern used in scene.hpp render loop; getComponent<T>() single-instance accessor used in animation.hpp color track]

key-files:
  created:
    - tests/drawable_decoupling_test.cpp
  modified:
    - include/enjin2/core/scene.hpp
    - include/enjin2/components/animation.hpp
    - tests/CMakeLists.txt

key-decisions:
  - "named_objects_test --start-group no longer required after C_Drawable cache removal from object.cpp in Plan 01 — simplified to plain link"
  - "TestDrawable constructor uses C_Drawable(owner, 1, 1) to match the actual (Object*, uint8_t width, uint8_t height) signature"
  - "drawable_decoupling_test uses --start-group link for safety (new test needs typeinfo resolution between enjin2_core.a and enjin2_ui.a)"

patterns-established:
  - "Caller-provides-buffer pattern: getComponents<T>(T** out, size_t maxOut) used in render loop — static local array on stack, no heap allocation"

requirements-completed: []

# Metrics
duration: 2min
completed: 2026-02-27
---

# Phase 36 Plan 02: Object Drawable Cache Decoupling (Consumers) Summary

**scene.hpp and animation.hpp migrated from deleted getDrawable()/getDrawableCount() to getComponents<C_Drawable>() — project compiles cleanly; 16 ctests pass; enjin2_core->enjin2_ui layering violation fully eliminated**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-27T16:50:16Z
- **Completed:** 2026-02-27T16:52:23Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Updated `scene.hpp::renderObjects()` to use `obj->getComponents<C_Drawable>(objDrawables, OBJ_MAX_DRAW)` — replaces deleted `getDrawable()`/`getDrawableCount()` loop
- Updated `animation.hpp::start()` color track lambda to use `owner->getComponent<C_Drawable>()` — removes old per-count loop
- Created `tests/drawable_decoupling_test.cpp` with 4 test cases verifying the new `getComponents<T>()` API: empty object, one drawable, two drawables in insertion order, and maxOut cap
- Simplified `named_objects_test` CMake link — `--start-group` no longer required after Plan 01 removed the C_Drawable cache from `object.cpp`; all 16 ctests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Update scene.hpp and animation.hpp to use new API** - `b456adb` (feat)
2. **Task 2: Add drawable_decoupling_test and verify ctest suite** - `b17ccd2` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified
- `include/enjin2/core/scene.hpp` - `renderObjects()` forEach lambda: replaced `getDrawableCount()`/`getDrawable(i)` loop with `getComponents<C_Drawable>()` call
- `include/enjin2/components/animation.hpp` - `start()` color track lambda: replaced for-loop with `getComponent<C_Drawable>()` single accessor
- `tests/drawable_decoupling_test.cpp` - New test: 4 cases for `getComponents<C_Drawable>()` regression
- `tests/CMakeLists.txt` - Added `drawable_decoupling_test` target; simplified `named_objects_test` link (removed `--start-group`)

## Decisions Made
- `named_objects_test` `--start-group` is no longer required. After Plan 01 removed the C_Drawable cache from `object.cpp`, there are no longer circular typeinfo dependencies between `enjin2_core.a` and `enjin2_ui.a` for that test. Plain link order suffices.
- `TestDrawable` constructor uses `C_Drawable(owner, 1, 1)` to match the actual `(Object*, uint8_t width, uint8_t height)` signature — the plan's template used a single-argument constructor that doesn't match the actual API.
- New `drawable_decoupling_test` retains `--start-group` as a safeguard: the test includes `drawable.hpp` directly and needs typeinfo resolution between the two static archives.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected TestDrawable constructor signature in drawable_decoupling_test**
- **Found during:** Task 2 (Add drawable_decoupling_test)
- **Issue:** The plan's code template used `TestDrawable(enjin2::Object* o) : enjin2::C_Drawable(o) {}` — a single-argument constructor. The actual `C_Drawable` constructor is `C_Drawable(Object* owner, uint8_t width, uint8_t height)` and has no single-argument overload.
- **Fix:** Used `C_Drawable(o, 1, 1)` (1x1 dimensions) as the minimal valid constructor call.
- **Files modified:** `tests/drawable_decoupling_test.cpp`
- **Verification:** Build succeeded; all 4 test cases pass.
- **Committed in:** b17ccd2 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - constructor signature mismatch)
**Impact on plan:** Essential correctness fix. No scope creep.

## Issues Encountered
None beyond the TestDrawable constructor signature fix above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 36 is complete: enjin2_core no longer depends on enjin2_ui types at any level (object.hpp, object.cpp, scene.hpp, animation.hpp all clean)
- Zero `getDrawable`/`getDrawableCount`/`getDrawables` references remain anywhere in include/, src/, or tests/
- Ready for Phase 37: address prominent codebase concerns

## Self-Check: PASSED

- include/enjin2/core/scene.hpp — FOUND
- include/enjin2/components/animation.hpp — FOUND
- tests/drawable_decoupling_test.cpp — FOUND
- tests/CMakeLists.txt — FOUND
- .planning/phases/36-object-drawable-cache-decoupling/36-02-SUMMARY.md — FOUND
- Commit b456adb (Task 1) — FOUND
- Commit b17ccd2 (Task 2) — FOUND

---
*Phase: 36-object-drawable-cache-decoupling*
*Completed: 2026-02-27*
