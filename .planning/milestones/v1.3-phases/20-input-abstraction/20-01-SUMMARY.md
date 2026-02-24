---
phase: 20-input-abstraction
plan: 01
subsystem: input
tags: [input, abstraction, platform-agnostic, stdint, cmake, unit-testing]

# Dependency graph
requires:
  - phase: 19-palette-foundation
    provides: CMake library pattern (enjin2_graphics, enjin2_core) used as template for enjin2_input
provides:
  - Platform-agnostic InputState struct with uint16_t bitmask + float axes[8] + prev fields
  - Inline edge-detection methods: justPressed, held, justReleased
  - input_advance_frame free function (copy current->prev, zero current)
  - input_platform_poll declared-but-not-defined hook for platform implementations
  - enjin2_input STATIC library wired into enjin2 INTERFACE aggregate
  - Host-only unit test binary (input_test) with 18 assertions across 6 scenarios
affects: [21-sdl3-runner, esp32-platform, emscripten-platform]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Platform hook pattern: declare function in core header, let each platform define exactly one implementation
    - Zero-platform-header boundary: input_state.hpp includes only <stdint.h>, no platform types cross the boundary
    - Inline edge detection: bitwise prev/current comparison as inline struct methods (no heap, no virtual dispatch)
    - Frame advance via memcpy/memset: snapshot current->prev then zero current at frame start

key-files:
  created:
    - include/enjin2/input/input_state.hpp
    - src/input/input.cpp
    - tests/input_test.cpp
  modified:
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "input_platform_poll declared in header but NOT defined in core — each platform (SDL3, ESP32, WASM) provides exactly one definition"
  - "input_state.hpp includes only <stdint.h> — zero SDL3, Emscripten, Arduino, or ESP32 headers cross the abstraction boundary"
  - "InputState uses uint16_t bitmask (max 16 buttons) and float axes[8] — matches plan INP-02 spec exactly"
  - "input_advance_frame uses memcpy/memset rather than loop — simpler and consistent with zero-allocation constraint"

patterns-established:
  - "Platform hook: declare in core header, no definition in core, each platform provides one definition"
  - "Frame snapshot: copy current->prev via memcpy, zero current via memset at frame start before platform poll"

requirements-completed: [INP-01, INP-02, INP-03]

# Metrics
duration: 2min
completed: 2026-02-24
---

# Phase 20 Plan 01: Input Abstraction Summary

**Platform-agnostic InputState struct with inline edge-detection (justPressed/held/justReleased), input_advance_frame definition, input_platform_poll declaration-only hook, enjin2_input CMake static library, and 18-assertion host-only unit test — zero platform headers cross the boundary**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-24T13:51:40Z
- **Completed:** 2026-02-24T13:53:25Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- InputState struct with uint16_t button bitmask and float axes[8] with prev copies for edge detection
- Three inline edge-detection methods (justPressed, held, justReleased) that compare current/prev bitmasks with bitwise ops
- input_platform_poll declared in header but left undefined in core — each platform (SDL3 in phase 21, ESP32, WASM) provides exactly one definition
- enjin2_input STATIC library added to CMakeLists.txt and wired into enjin2 INTERFACE aggregate
- 18 assertions across 6 test scenarios all pass; palette_test unaffected (2/2 CTest suite)

## Task Commits

Each task was committed atomically:

1. **Task 1: InputState header, input.cpp implementation, and CMake enjin2_input library** - `f6d8927` (feat)
2. **Task 2: Host-only unit tests for edge detection and frame advance** - `1c35452` (test)

**Plan metadata:** (docs commit, see below)

## Files Created/Modified

- `include/enjin2/input/input_state.hpp` - InputState struct, inline justPressed/held/justReleased, declarations of input_advance_frame and input_platform_poll
- `src/input/input.cpp` - Definition of input_advance_frame only (memcpy/memset frame snapshot)
- `CMakeLists.txt` - enjin2_input STATIC library target + linked into enjin2 INTERFACE
- `tests/input_test.cpp` - 6 test functions, 18 assertions, no call to input_platform_poll
- `tests/CMakeLists.txt` - input_test executable registered with CTest

## Decisions Made

- input_platform_poll declared in header but NOT defined in core — each platform provides exactly one definition (SDL3, ESP32, WASM)
- input_state.hpp includes only <stdint.h> — zero SDL3, Emscripten, Arduino, or ESP32 headers cross the abstraction boundary
- InputState uses uint16_t bitmask (max 16 buttons) and float axes[8] — matches plan INP-02 spec exactly
- input_advance_frame uses memcpy/memset for clarity and consistency with zero-allocation constraint

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 21 (SDL3 runner) has the full input contract to implement against: InputState fields, input_platform_poll signature, and input_advance_frame semantics
- input_platform_poll will be defined in the SDL3 platform layer (phase 21) as it reads SDL events into InputState
- No blockers or concerns

---
*Phase: 20-input-abstraction*
*Completed: 2026-02-24*
