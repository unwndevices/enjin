---
phase: 21-sdl3-cmake-runner
plan: 01
subsystem: infra
tags: [cmake, sdl3, fetchcontent, build-system]

# Dependency graph
requires:
  - phase: 20-input-abstraction
    provides: enjin2_input library target that enjin2_sdl links against
  - phase: 19-wasm-palette
    provides: enjin2_graphics library target that enjin2_sdl links against
provides:
  - ENJIN2_BUILD_SDL CMake option (default OFF) gates all SDL3 involvement
  - FetchContent SDL3 block pinned to release-3.4.2 with EXCLUDE_FROM_ALL
  - enjin2_sdl executable target linking enjin2_core, enjin2_graphics, enjin2_input, SDL3::SDL3
affects: [21-sdl3-cmake-runner plan 02 (runner source), any future SDL platform plans]

# Tech tracking
tech-stack:
  added: [SDL3 via FetchContent (release-3.4.2)]
  patterns: [opt-in platform target via CMake option, FetchContent with EXCLUDE_FROM_ALL to prevent install bleed]

key-files:
  created: []
  modified: [CMakeLists.txt]

key-decisions:
  - "ENJIN2_BUILD_SDL defaults OFF — build is completely unaffected unless explicitly enabled"
  - "FetchContent_Declare uses EXCLUDE_FROM_ALL so SDL3 install rules cannot bleed into project install target"
  - "enjin2_lua aggregate NOT linked to enjin2_sdl — Lua cannot become a transitive SDL dependency"
  - "GIT_SHALLOW TRUE with exact tag release-3.4.2 for reproducible builds with minimal clone depth"

patterns-established:
  - "Platform runner isolation: new platform executable targets are opt-in CMake options, not always-on"
  - "Dependency bleed prevention: EXCLUDE_FROM_ALL on FetchContent_Declare for third-party libs"

requirements-completed: [SDL-01]

# Metrics
duration: 1min
completed: 2026-02-24
---

# Phase 21 Plan 01: SDL3 CMake Runner Summary

**ENJIN2_BUILD_SDL=OFF gate added to CMakeLists.txt with FetchContent SDL3 release-3.4.2 and enjin2_sdl executable target linking core, graphics, input, and SDL3::SDL3**

## Performance

- **Duration:** 1 min
- **Started:** 2026-02-24T14:08:55Z
- **Completed:** 2026-02-24T14:09:57Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- ENJIN2_BUILD_SDL option (default OFF) appended to CMakeLists.txt after the examples block
- SDL3 FetchContent block pinned to release-3.4.2 with GIT_SHALLOW TRUE and EXCLUDE_FROM_ALL inside the guard
- enjin2_sdl executable target defined linking enjin2_core, enjin2_graphics, enjin2_input, and SDL3::SDL3 — enjin2_lua excluded
- cmake -DENJIN2_BUILD_SDL=OFF configures and builds cleanly with no SDL3 download or symbols

## Task Commits

Each task was committed atomically:

1. **Task 1: Add ENJIN2_BUILD_SDL option and FetchContent SDL3 block** - `48d5a03` (feat)

**Plan metadata:** (docs commit — pending)

## Files Created/Modified
- `CMakeLists.txt` - Appended SDL3 desktop runner option, FetchContent block, and enjin2_sdl executable target

## Decisions Made
- ENJIN2_BUILD_SDL defaults OFF so the build is completely unaffected unless the user explicitly enables it
- EXCLUDE_FROM_ALL on FetchContent_Declare prevents SDL3's own install rules from appearing in the project's install target
- enjin2_lua is deliberately excluded from enjin2_sdl link libraries — Lua must not be a transitive SDL dependency
- GIT_SHALLOW TRUE with release-3.4.2 tag ensures reproducible builds with minimal network overhead

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- CMake wiring is complete; Plan 02 (runner source file) can now compile against the enjin2_sdl target
- The src/platform/sdl/sdl_main.cpp source path is declared in CMakeLists.txt and must be created in Plan 02

---
*Phase: 21-sdl3-cmake-runner*
*Completed: 2026-02-24*
