---
phase: 03-feature-support
plan: 03
subsystem: architecture
tags: compile-time-selection, abstraction-interface, seam-pattern, backend-routing

# Dependency graph
requires:
  - phase: 03-feature-support (plan 01)
    provides: CMake option USE_ENJIN1 with USE_ENJIN1_BACKEND compile definition
  - phase: 03-feature-support (plan 02)
    provides: IComponent and IScene abstract interfaces for compile-time polymorphism
provides:
  - ComponentSeam with compile-time USE_ENJIN1_BACKEND routing and IComponent interface implementation
  - SceneSeam with compile-time USE_ENJIN1_BACKEND routing and IScene<PixelType> interface implementation
  - Deprecated runtime switching methods for backward compatibility during migration
  - Compile-time conditional compilation preventing enjin1 code paths in enjin2-only builds
affects: future feature migration phases that will use these seams for isolated testing

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Compile-time backend selection via preprocessor directives (#if USE_ENJIN1_BACKEND)
    - Template-based seam pattern (SceneSeam templated on PixelType)
    - Interface inheritance with pure virtual method implementation
    - Deprecation attributes for backward compatibility ([[deprecated]])

key-files:
  created: []
  modified:
    - enjin2/include/enjin2/seams/component_seam.hpp
    - enjin2/include/enjin2/seams/scene_seam.hpp

key-decisions:
  - Used #if USE_ENJIN1_BACKEND (not #ifdef) to allow CMake to set 0 or 1 and still work correctly
  - Kept void* legacy implementation pointers (legacyImpl, enjin1SM) for future enjin1 integration
  - Deprecated runtime switching methods (switchToNew, switchToEnjin2) with [[deprecated]] attribute instead of removing
  - Implemented all IComponent and IScene pure virtual methods in both seams
  - Added #error "enjin1 backend not yet integrated" to prevent accidental compilation with enjin1 before integration

patterns-established:
  - Compile-time routing pattern: #if USE_ENJIN1_BACKEND guards around implementation routing
  - Interface inheritance pattern: Seams inherit from IComponent/IScene and implement all pure virtual methods
  - Deprecation pattern: Mark deprecated methods with [[deprecated]] attribute documenting the replacement
  - Template seam pattern: SceneSeam templated on PixelType to match IScene<PixelType> interface

# Metrics
duration: 3 min
completed: 2026-01-30
---

# Phase 3 Plan 3: Compile-Time Seam Routing Summary

**ComponentSeam and SceneSeam updated with compile-time USE_ENJIN1_BACKEND routing, IComponent and IScene interface inheritance, and deprecated runtime switching methods for backward compatibility**

## Performance

- **Duration:** 3 min
- **Started:** 2026-01-30T21:00:52Z
- **Completed:** 2026-01-30T21:04:05Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Updated ComponentSeam to inherit from IComponent and implement all pure virtual methods (awake, start, update, lateUpdate, onEnable, onDisable, getOwner, isEnabled, setEnabled)
- Updated SceneSeam to inherit from IScene<PixelType> (templated) and implement all pure virtual methods (onCreate, onActivate, onDeactivate, onDestroy, onUpdate, onRender, getId, isActive, isInitialized)
- Added compile-time #if USE_ENJIN1_BACKEND guards to both seams for conditional compilation
- Added #error "enjin1 backend not yet integrated" directive to prevent enjin1 code paths before integration
- Marked runtime switching methods (switchToNew, switchToEnjin2) with [[deprecated]] attribute for backward compatibility
- Kept void* legacy implementation pointers (legacyImpl, enjin1SM) for future enjin1 integration
- Added enabled state management to ComponentSeam with proper onEnable/onDisable lifecycle
- Added scene state management (active, initialized) to SceneSeam

## Task Commits

Each task was committed atomically:

1. **Task 1: Update ComponentSeam with compile-time routing and IComponent interface** - `4bf53f6` (feat)
2. **Task 2: Update SceneSeam with compile-time routing and IScene interface** - `76cfd4c` (feat)

**Plan metadata:** TBD (docs: complete plan)

## Files Created/Modified

- `enjin2/include/enjin2/seams/component_seam.hpp` - Added IComponent inheritance, compile-time routing, all pure virtual methods
- `enjin2/include/enjin2/seams/scene_seam.hpp` - Added IScene<PixelType> inheritance, compile-time routing, all pure virtual methods, template parameter

## Decisions Made

- Used #if USE_ENJIN1_BACKEND (instead of #ifdef) to allow CMake to set 0 or 1 and still work correctly - more robust for conditional compilation
- Kept all deprecated runtime switching methods with [[deprecated]] attributes instead of removing - maintains backward compatibility during migration period
- Added #error "enjin1 backend not yet integrated" directive in all enjin1 code paths - prevents accidental compilation before enjin1 integration is ready
- Used opaque void* pointers for legacy implementations (legacyImpl, enjin1SM) - allows seam headers to compile without enjin1 dependencies
- Deprecated SceneSeam's initialize/update/render methods with proper lifecycle alternatives (onCreate/onActivate/onUpdate/onRender) - guides migration to IScene interface

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - all verifications passed.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Both seams now use compile-time backend selection via USE_ENJIN1_BACKEND macro
- enjin2-only builds (USE_ENJIN1=OFF) compile cleanly with only enjin2 code paths
- enjin1 builds (USE_ENJIN1=ON) will trigger clear error until enjin1 is integrated
- Deprecated runtime switching methods retained for backward compatibility during migration
- Ready for future feature migration plans that will use these seams for isolated testing
- No blockers identified

---
*Phase: 03-feature-support*
*Completed: 2026-01-30*
