---
phase: 02-core-migration
plan: 02
subsystem: architecture
tags: strangler-fig, seam-pattern, migration, component-seam, scene-seam

# Dependency graph
requires:
  - phase: 01-dependency-analysis
    provides: Verification that enjin2 has zero enjin1 dependencies, established include directory scoping, build isolation confirmation
  - phase: 02-core-migration (plan 01)
    provides: Compatibility namespace (enjin) wrapping enjin2 types, lifecycle method mappings
provides:
  - ComponentSeam class for routing component lifecycle calls between enjin1 and enjin2 implementations
  - SceneSeam class for routing scene management between enjin1 and enjin2 backends
  - Runtime switching capability via switchToNew() and switchToEnjin2() methods
  - Stubbed legacy paths ready for enjin1 integration
affects: future migration phases implementing seam routing to enjin1 legacy code

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Strangler Fig pattern - incremental migration seams
    - Runtime implementation switching via opaque void* pointers for legacy implementations
    - Seaming pattern for component and scene boundaries

key-files:
  created:
    - enjin2/include/enjin2/seams/component_seam.hpp
    - enjin2/include/enjin2/seams/scene_seam.hpp
  modified: []

key-decisions:
  - "Removed initialize() method from SceneSeam - SceneStateMachine doesn't have this method (initialization happens via changeScene() activation)"

patterns-established:
  - "Seam pattern: Enum-based routing (Implementation::LEGACY/NEW, Backend::ENJIN1/ENJIN2)"
  - "Opaque void* for legacy implementation pointers (avoid enjin1 includes during enjin2 development)"
  - "TODO comments marking legacy integration points"

# Metrics
duration: 5 min
completed: 2026-01-30
---

# Phase 2 Plan 2: Strangler Fig Seams Summary

**ComponentSeam and SceneSeam classes implementing Strangler Fig pattern for incremental enjin1→enjin2 migration with runtime implementation switching**

## Performance

- **Duration:** 5 min
- **Started:** 2026-01-30T17:26:32Z
- **Completed:** 2026-01-30T17:31:32Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Created ComponentSeam class with Implementation enum (LEGACY/NEW) and runtime switching via switchToNew()
- Created SceneSeam class with Backend enum (ENJIN1/ENJIN2) and runtime switching via switchToEnjin2()
- Implemented routing logic for enjin2 implementations in both seams (awake, start, update for components; update, render for scenes)
- Stubbed legacy routing paths with TODO comments, ready for enjin1 integration without breaking enjin2-only development
- Used opaque void* pointers for legacy implementations to avoid enjin1 header dependencies

## Task Commits

Each task was committed atomically:

1. **Task 1: Create component seam** - `cdb1697` (feat)
2. **Task 2: Create scene seam** - `6b1f938` (feat)

**Plan metadata:** TBD (docs: complete plan)

## Files Created/Modified

- `enjin2/include/enjin2/seams/component_seam.hpp` - Component lifecycle seam with LEGACY/NEW routing
- `enjin2/include/enjin2/seams/scene_seam.hpp` - Scene management seam with ENJIN1/ENJIN2 routing

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Removed initialize() method from SceneSeam**

- **Found during:** Task 2 (scene seam implementation)
- **Issue:** Plan specified initialize() method, but SceneStateMachine interface doesn't have this method (initialization happens via changeScene() which activates scenes and triggers their initialize())
- **Fix:** Removed initialize() from SceneSeam - seam should only expose methods that exist in underlying implementations
- **Files modified:** enjin2/include/enjin2/seams/scene_seam.hpp
- **Verification:** SceneStateMachine verified to not have initialize() method, scene_seam.hpp compiles successfully
- **Committed in:** 6b1f938 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Deviation necessary for correctness - seam must match underlying implementation interface. No scope creep.

## Issues Encountered

None - plan executed as expected with one necessary deviation.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Both seams ready for enjin2 development and testing
- Legacy routing paths stubbed but documented with TODO comments
- Ready for subsequent plans to integrate these seams into game engine architecture
- No blockers identified

---
*Phase: 02-core-migration*
*Completed: 2026-01-30*
