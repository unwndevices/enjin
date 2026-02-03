---
phase: 08-build-system-fixes
plan: 01
subsystem: build-system
tags: [cmake, lua, optional-dependencies, build-configuration]

# Dependency graph
requires: []
provides:
  - CMakeLists.txt with optional Lua support via ENJIN2_BUILD_LUA option
  - Clear error messages when Lua is requested but not found
affects: [08-build-system-fixes-02, ci-cd-pipelines, docs-deployment]

# Tech tracking
tech-stack:
  added: []
  patterns: [optional-dependencies, conditional-compilation, cmake-error-handling]

key-files:
  created: []
  modified: [CMakeLists.txt]

key-decisions:
  - "Use QUIET flag for find_package(Lua) to make it optional"
  - "Provide actionable error messages with installation instructions for Lua"

patterns-established:
  - "Pattern: Optional dependencies - Use option() + find_package(PACKAGE QUIET) + check ${PACKAGE}_FOUND"
  - "Pattern: User guidance - Include package manager commands in FATAL_ERROR messages"

# Metrics
duration: 3min
completed: 2026-02-03
---

# Phase 8: Plan 1 - Optional Lua Dependency Summary

**Made Lua dependency optional in CMake build system via ENJIN2_BUILD_LUA option with clear error messaging for missing dependencies**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-03T13:18:22Z
- **Completed:** 2026-02-03T13:21:07Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- Changed Lua from required to optional dependency using `find_package(Lua QUIET)` instead of `find_package(Lua REQUIRED)`
- Added clear, actionable error message when ENJIN2_BUILD_LUA=ON but Lua is not found, including installation instructions for Debian/Ubuntu and macOS
- Verified both configurations (ON and OFF) build successfully without dependency errors

## Task Commits

Each task was committed atomically:

1. **Task 1: Make Lua dependency optional in CMake** - `4e71dcd` (feat)
2. **Task 2: Test both Lua configurations** - (no code changes, verification only)

**Plan metadata:** (to be committed)

## Files Created/Modified

- `CMakeLists.txt` - Modified Lua handling block (lines 122-136) to use QUIET flag and add error messaging

## Decisions Made

None - followed plan as specified. All changes were planned based on the research findings in 08-RESEARCH.md.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - both builds completed successfully without errors.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Lua dependency is now optional, fixing the CI/CD docs deployment failure
- Users can build with `ENJIN2_BUILD_LUA=OFF` for documentation builds or when Lua is not needed
- Ready for next phase: Plan 08-02 (Update README with dependency documentation)

---
*Phase: 08-build-system-fixes*
*Completed: 2026-02-03*
