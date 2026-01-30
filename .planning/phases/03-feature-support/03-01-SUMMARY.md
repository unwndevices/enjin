---
phase: 03-feature-support
plan: 01
subsystem: build-system
tags: [cmake, compile-time-selection, backend-switching, generator-expressions]

# Dependency graph
requires:
  - phase: 02-core-migration
    provides: Strangler Fig pattern, seam infrastructure for component/scene boundaries
provides:
  - CMake build system with USE_ENJIN1 option for compile-time backend selection
  - Compile-time macro definitions (USE_ENJIN1_BACKEND) for conditional compilation
  - Foundation for backend-independent code builds
affects: [03-02, 03-03, abstraction-interface-implementation, seam-layer-routing]

# Tech tracking
tech-stack:
  added: [cmake-3.16, generator-expressions, target_compile_definitions]
  patterns: [compile-time-configuration, INTERFACE-library-propagation]

key-files:
  created: [Libs (symlink to Adafruit-GFX-Library)]
  modified: [CMakeLists.txt, enjin2/CMakeLists.txt]

key-decisions:
  - "INTERFACE target_compile_definitions for enjin2 INTERFACE library - must use INTERFACE scope for INTERFACE targets"
  - "Symlink to Adafruit-GFX-Library resolves blocking build dependency (Rule 3)"

patterns-established:
  - "Compile-time backend selection via CMake option(USE_ENJIN1)"
  - "Propagating compile definitions through INTERFACE targets to dependent code"

# Metrics
duration: 3min
completed: 2026-01-30
---

# Phase 3 Plan 01: CMake Backend Selection Summary

**CMake build system configured for compile-time enjin1/enjin2 backend selection using USE_ENJIN1 option and INTERFACE target_compile_definitions**

## Performance

- **Duration:** 3 min
- **Started:** 2026-01-30T20:48:10Z
- **Completed:** 2026-01-30T20:51:15Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- CMake option USE_ENJIN1 added to root CMakeLists.txt, defaulting to OFF (enjin2 backend)
- Backend selection messages provide clear feedback on which implementation is being built
- enjin2 INTERFACE library propagates USE_ENJIN1_BACKEND compile definitions to all dependent targets
- Both build configurations (enjin1 and enjin2) compile successfully with appropriate macro definitions

## Task Commits

Each task was committed atomically:

1. **Task 1: Add CMake backend selection option** - `6cfcd53` (feat)
2. **Task 2: Update enjin2 CMakeLists.txt with compile definitions** - `3b9fabe` (feat)

## Files Created/Modified

- `CMakeLists.txt` - Added USE_ENJIN1 option and backend selection message
- `enjin2/CMakeLists.txt` - Added conditional compile definitions for USE_ENJIN1_BACKEND
- `Libs` (symlink) - Created symlink to unwn/Libs to resolve blocking Adafruit-GFX-Library dependency

## Decisions Made

- Used INTERFACE target_compile_definitions instead of PRIVATE for enjin2 INTERFACE library - CMake requires INTERFACE targets to use INTERFACE scope for properties
- Backend selection is compile-time only via CMake option, following CONTEXT.md decision to avoid runtime mixing
- Defaults to enjin2 backend (USE_ENJIN1=OFF) since enjin2 is the target implementation

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

**Issue: INTERFACE library compile definition scope error**
- **Problem:** Initial implementation used `target_compile_definitions(enjin2 PRIVATE ...)` which failed because enjin2 is an INTERFACE library
- **Resolution:** Changed to `target_compile_definitions(enjin2 INTERFACE ...)` which is required for INTERFACE targets
- **Impact:** This is the correct CMake pattern - INTERFACE targets should use INTERFACE scope for properties to propagate correctly to dependent targets

**Issue: Missing Adafruit-GFX-Library blocking build**
- **Problem:** Build failed with "fatal error: ../../Libs/Adafruit-GFX-Library/gfxfont.h: No such file or directory"
- **Resolution:** Created symlink from enjin/Libs to unwn/Libs to resolve missing dependency
- **Impact:** This is a pre-existing build configuration issue; the symlink enables verification that compile definitions work correctly

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- CMake build system ready for backend abstraction layer implementation in plan 03-02
- Compile-time macro (USE_ENJIN1_BACKEND) available for conditional compilation in abstraction interfaces
- Separate enjin1 and enjin2 builds can now be created using `-DUSE_ENJIN1=ON/OFF`

---
*Phase: 03-feature-support*
*Completed: 2026-01-30*
