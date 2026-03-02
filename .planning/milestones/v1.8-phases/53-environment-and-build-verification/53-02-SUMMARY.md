---
phase: 53-environment-and-build-verification
plan: "02"
subsystem: infra
tags: [cmake, emscripten, esp-idf, bash, build-system]

requires:
  - phase: 53-01
    provides: scripts/setup-dev.sh (referenced in actionable error messages)
provides:
  - build.sh unified build entry point for SDL3, WASM, and ESP32 targets
  - build/<target>/ output directory convention (build/sdl3/, build/wasm/, build/esp32/)
affects:
  - 53-03 (build verification uses build.sh for all three platforms)
  - all future development (single command to build any target)

tech-stack:
  added: []
  patterns:
    - Unified build script: single entry point dispatches to target-specific build functions
    - Env var detection for toolchain activation ($EMSDK, $IDF_PATH) with actionable error messages
    - --clean flag pattern: rm -rf build/<target>/ before configure

key-files:
  created:
    - build.sh
  modified: []

key-decisions:
  - "Single build.sh at project root (not in scripts/) — matches conventional project structure"
  - "Default target: sdl3 (most common development build)"
  - "Env var check: [ -z \"${EMSDK:-}\" ] not directory check — $EMSDK is set by emsdk_env.sh"
  - "emcmake cmake must run from INSIDE build/wasm/ (cd into it), not from project root"
  - "build_wasm.sh deleted — fully replaced by build.sh --target wasm"
  - "WASM output: build/wasm/ (was build_wasm/) — cleaner convention"

requirements-completed:
  - BLDINFRA-02
  - BLDINFRA-03

duration: 4min
completed: 2026-03-02
---

# Phase 53 Plan 02: Unified Build Entry Point Summary

**build.sh replaces build_wasm.sh with single entry point for SDL3/WASM/ESP32 builds, env var toolchain checks, --clean flag, and DROP copy logic**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-02T18:17:00Z
- **Completed:** 2026-03-02T18:21:00Z
- **Tasks:** 2
- **Files modified:** 2 (build.sh created, build_wasm.sh deleted)

## Accomplishments
- Created `build.sh` with all three build targets (SDL3, WASM, ESP32) and correct CMake flags
- `$EMSDK` and `$IDF_PATH` checks with multi-line actionable error messages referencing `setup-dev.sh`
- `--clean` flag implemented (removes build/<target>/ before reconfiguring)
- WASM DROP copy behavior preserved exactly from old `build_wasm.sh`
- `build_wasm.sh` deleted — replaced entirely by `build.sh --target wasm`

## Task Commits

1. **Task 1: Create build.sh** - `5f7ecd9` (feat)
2. **Task 2: Remove build_wasm.sh** - `4717ad4` (chore)

## Files Created/Modified
- `build.sh` - Unified build entry point: SDL3/WASM/ESP32 targets, env var checks, --clean, DROP copy
- `build_wasm.sh` - Deleted (superseded by `build.sh --target wasm`)

## Decisions Made
- `emcmake cmake` must run from inside `build/wasm/` directory (not from project root) — this matches the original build_wasm.sh behavior and is required by Emscripten's cmake integration
- Used `${EMSDK:-}` syntax (not `${EMSDK}`) to prevent unbound variable error with set -euo pipefail

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Plan 53-03 can proceed: build.sh is ready for SDL3 build verification
- Wave 2 checkpoint plan requires toolchains to be installed and activated in the user's shell

---
*Phase: 53-environment-and-build-verification*
*Completed: 2026-03-02*

## Self-Check: PASSED
- `build.sh` exists on disk: confirmed
- `git log --oneline --grep="53-02"` returns ≥1 commit: confirmed (5f7ecd9, 4717ad4)
