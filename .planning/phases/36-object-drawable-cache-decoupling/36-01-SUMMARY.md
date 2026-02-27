---
phase: 36-object-drawable-cache-decoupling
plan: 01
subsystem: core
tags: [object, component, drawable, decoupling, template, dynamic_cast]

# Dependency graph
requires:
  - phase: 35-gc-control-component-assertions
    provides: Component base with assertRequires<T>() and full component system
provides:
  - Object as pure generic component container — no C_Drawable coupling
  - getComponents<T>() template for multi-component lookup via dynamic_cast
  - enjin2_core compile unit (object.cpp) free of drawable.hpp include
affects:
  - 36-02 — Plan 02 migrates scene.hpp and animation.hpp callers from getDrawable()/getDrawableCount() to getComponents<C_Drawable>()

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "getComponents<T>(T** out, size_t maxOut) — caller-provides-buffer pattern; no heap allocation; O(n) dynamic_cast scan"
    - "SFINAE cachePositionIfType<T>() retained as narrow performance cache for C_Position only"

key-files:
  created: []
  modified:
    - include/enjin2/core/object.hpp
    - src/core/object.cpp

key-decisions:
  - "getComponents<T>() uses caller-provides-buffer pattern (T** out, size_t maxOut) matching project zero-heap-allocation convention"
  - "Only C_Position cache retained in Object — generic getComponents<T>() replaces type-specific C_Drawable cache"
  - "scene.hpp/animation.hpp callers of getDrawable()/getDrawableCount() are intentionally broken at Plan 01 boundary — fixed by Plan 02"
  - "static_assert(is_base_of<Component,T>) added to getComponents<T>() for type safety at compile time"

patterns-established:
  - "Multi-component access: getComponents<T>(buf, max) — no cache, O(n) scan, zero allocation"

requirements-completed: []

# Metrics
duration: 3min
completed: 2026-02-27
---

# Phase 36 Plan 01: Object Drawable Cache Decoupling Summary

**Object stripped of C_Drawable cache and getDrawable() API; getComponents<T>() template added as zero-allocation generic component scanner**

## Performance

- **Duration:** ~3 min
- **Started:** 2026-02-27T16:45:01Z
- **Completed:** 2026-02-27T16:48:00Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Deleted `class C_Drawable;` forward declaration, `drawables[]` array, and `drawableCount` field from `object.hpp`
- Removed `getDrawables()`, `getDrawableCount()`, and `getDrawable()` public methods from `Object`
- Removed drawable cache maintenance code from `addComponent<T>()` and `removeComponent<T>()`
- Added `getComponents<T>(T** out, size_t maxOut) const` generic template with `static_assert` type safety
- Removed `drawable.hpp` include, `drawableCount(0)` initializer, and `drawables.fill()` from `object.cpp`
- Simplified `initializeComponentCache()` to only rebuild the position cache
- `object.cpp` compiles cleanly with zero drawable references

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove drawable cache from object.hpp** - `7d15f2a` (refactor)
2. **Task 2: Remove drawable coupling from object.cpp** - `eb116d4` (refactor)

## Files Created/Modified

- `include/enjin2/core/object.hpp` - C_Drawable forward decl, fields, methods removed; getComponents<T>() added
- `src/core/object.cpp` - drawable.hpp include removed; drawableCount initializer and drawables.fill() removed; initializeComponentCache() simplified

## Decisions Made

- `getComponents<T>()` uses caller-provides-buffer pattern (`T** out, size_t maxOut`) matching the project's zero-heap-allocation convention — consistent with how `renderObjects()` in scene.hpp already works with a local stack array.
- Only the `C_Position` fast cache is retained in Object. The C_Drawable cache was the only other type-specific cache; removing it eliminates the layering violation without losing meaningful performance (drawable lookup is not in the hot render path the same way position is).
- scene.hpp and animation.hpp callers of the removed API are intentionally left broken at this plan boundary — Plan 02 will migrate them to `getComponents<C_Drawable>()`. This approach keeps Plan 01 narrowly scoped and reviewable.

## Deviations from Plan

None - plan executed exactly as written. The build error in `scene.cpp` at plan boundary is explicitly acknowledged and expected by the plan's verification note ("That is expected and fixed in Plan 02").

## Issues Encountered

None. The plan's verification note correctly predicted that `scene.hpp` would fail to compile after removing `getDrawable()`/`getDrawableCount()` — this is the expected Plan 01 end state. `object.cpp` itself compiled cleanly.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 02 is unblocked: `getComponents<C_Drawable>()` is available for scene.hpp and animation.hpp to migrate to
- Object is now a pure generic component container — enjin2_core no longer depends on enjin2_ui types at the header level
- Any consumer needing drawable access must use `getComponents<C_Drawable>(buf, maxBuf)` — the caller-provides-buffer API

## Self-Check: PASSED

- FOUND: include/enjin2/core/object.hpp
- FOUND: src/core/object.cpp
- FOUND: .planning/phases/36-object-drawable-cache-decoupling/36-01-SUMMARY.md
- FOUND commit: 7d15f2a (Task 1)
- FOUND commit: eb116d4 (Task 2)

---
*Phase: 36-object-drawable-cache-decoupling*
*Completed: 2026-02-27*
