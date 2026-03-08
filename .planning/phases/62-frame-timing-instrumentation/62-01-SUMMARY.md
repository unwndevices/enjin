---
phase: 62-frame-timing-instrumentation
plan: 01
subsystem: instrumentation
tags: [frame-timing, atomics, sdl3, instrumentation, performance, cpp]

# Dependency graph
requires:
  - phase: 60-cmake-foundation-vendor
    provides: build system foundation for SDL3 target
  - phase: 61-native-benchmark-suite
    provides: understanding of measurement patterns and perf_freq usage
provides:
  - FrameTimingInstrumentation header-only singleton with atomic/stub dual path
  - Four measurement sites in SDL3 game loop (lua, update, composite, render)
  - --show-timing debug overlay drawn to debug layer (layer 4)
  - Polling API: FrameTimingInstrumentation::get() readable without overlay
affects: [63-lua-profiler, 64-ci-pipeline, future-debug-tooling]

# Tech tracking
tech-stack:
  added: [std::atomic<uint32_t>, SDL_GetPerformanceCounter, SDL_GetPerformanceFrequency]
  patterns: [header-only dual-path compile guards, Meyer's singleton, relaxed atomics for per-frame stats]

key-files:
  created:
    - include/enjin2/instrumentation/frame_timing.hpp
    - tests/frame_timing_test.cpp
  modified:
    - tests/CMakeLists.txt
    - CMakeLists.txt
    - src/platform/sdl/sdl_main.cpp

key-decisions:
  - "#include <atomic> placed OUTSIDE namespace enjin2 {} to avoid nesting std namespace inside enjin2 (compiler error on GCC 15)"
  - "ENJIN2_FRAME_TIMING=1 injected only for enjin2_sdl target — WASM and ESP32 targets omit the define and get the zero-overhead plain uint32_t stub"
  - "Overlay draws previous-frame timing for lua/update (stored earlier in the same iteration) and current-frame for composite/render — one-frame lag is acceptable for real-time display"
  - "frame_timing_test uses target_compile_definitions (CMake) not a #define in .cpp for ENJIN2_FRAME_TIMING, consistent with project pattern"

patterns-established:
  - "Dual-path header pattern: #ifdef guard before namespace enjin2, includes outside namespace to prevent std nesting"
  - "Per-frame timing stores use std::memory_order_relaxed — no cross-field ordering required for telemetry"

requirements-completed: [FRAME-01, FRAME-02, FRAME-03]

# Metrics
duration: 5min
completed: 2026-03-08
---

# Phase 62 Plan 01: Frame Timing Instrumentation Summary

**Header-only FrameTimingInstrumentation singleton with four lock-free atomic uint32_t fields, instrumented SDL3 game loop with --show-timing overlay drawn to debug layer**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-03-08T01:35:44Z
- **Completed:** 2026-03-08T01:40:30Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Created `frame_timing.hpp` with dual compilation paths: enabled path uses `std::atomic<uint32_t>` with lock-free static_assert; disabled stub uses plain `uint32_t` with zero overhead
- Created `frame_timing_test.cpp` with 10 assertions covering singleton identity, store/load round-trip, default zeros, and all four field round-trips
- Instrumented SDL3 game loop with four measurement sites (luaTime_us, updateTime_us, compositeTime_us, renderTime_us) using SDL_GetPerformanceCounter
- Added `--show-timing` overlay that draws 4 timing lines to debug layer (layer 4) before composite(), using previous frame values
- Added `ENJIN2_FRAME_TIMING=1` to `enjin2_sdl` target compile definitions; all 45 existing tests pass with no regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Create FrameTimingInstrumentation header and unit test** - `bea5fb6` (feat)
2. **Task 2: Instrument SDL3 runner with measurement sites, --show-timing overlay, and CMake definition** - `a0c2070` (feat)

**Plan metadata:** (docs commit follows)

_Note: Task 1 was TDD — header and test created together, tested with GREEN pass on first real build after fixing the namespace/include bug._

## Files Created/Modified

- `include/enjin2/instrumentation/frame_timing.hpp` - Header-only singleton with enabled (atomic) and disabled (stub) dual-path via ENJIN2_FRAME_TIMING define
- `tests/frame_timing_test.cpp` - Unit test for singleton identity, store/load, defaults, and all four fields (10 assertions, all pass)
- `tests/CMakeLists.txt` - Added frame_timing_test before if(ENJIN2_BUILD_LUA) block with ENJIN2_FRAME_TIMING=1 via target_compile_definitions
- `CMakeLists.txt` - Added ENJIN2_FRAME_TIMING=1 to enjin2_sdl target_compile_definitions
- `src/platform/sdl/sdl_main.cpp` - Added include, --show-timing arg parse, perf_freq cache, four measurement sites, and timing overlay

## Decisions Made

- `#include <atomic>` placed outside `namespace enjin2 {}` to avoid nesting `std` inside `enjin2` — GCC 15 catches this as an error when `<new>` is pulled transitively
- `ENJIN2_FRAME_TIMING=1` injected only to `enjin2_sdl` target; WASM and ESP32 omit it for zero-overhead stub path
- Overlay timing values for lua/update come from earlier in the same frame iteration; composite/render show current-frame values — one-frame lag for the first two is acceptable and expected
- `target_compile_definitions` chosen over `#define` in test source file for consistency with project convention

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed `#include <atomic>` inside namespace causing std-nesting compile errors**

- **Found during:** Task 1 (header creation + first build)
- **Issue:** Plan action specified `#include <atomic> inside the ifdef guard` but also `namespace enjin2 { }` wrapping the whole file. Placing `#include` inside the namespace causes GCC 15 to error: `void* enjin2::operator new(...)` may not be declared within a namespace
- **Fix:** Moved `#include <atomic>` and `#include <cstdint>` above the `namespace enjin2 {` opening brace (outside the namespace). static_assert and struct remain inside namespace as intended
- **Files modified:** `include/enjin2/instrumentation/frame_timing.hpp`
- **Verification:** `cmake --build build --target frame_timing_test` succeeds, all 10 assertions pass
- **Committed in:** bea5fb6 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - bug in namespace/include placement)
**Impact on plan:** Fix necessary for correct compilation. No scope creep. The intent of the plan was preserved exactly.

## Issues Encountered

None beyond the auto-fixed namespace/include ordering bug documented above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `FrameTimingInstrumentation::get()` singleton is available to any future phase that includes `frame_timing.hpp`
- SDL3 runner can be launched with `--show-timing` to see per-phase microsecond timing in the debug overlay
- Polling API works without overlay: `enjin2::FrameTimingInstrumentation::get().luaTime_us.load(std::memory_order_relaxed)`
- WASM and ESP32 builds unaffected — they use the zero-overhead stub path
- Phase 63 (Lua profiler + headless runner) can use this infrastructure as reference pattern for instrumented timing

---
*Phase: 62-frame-timing-instrumentation*
*Completed: 2026-03-08*

## Self-Check: PASSED

- FOUND: include/enjin2/instrumentation/frame_timing.hpp
- FOUND: tests/frame_timing_test.cpp
- FOUND: .planning/phases/62-frame-timing-instrumentation/62-01-SUMMARY.md
- FOUND commit: bea5fb6 (Task 1)
- FOUND commit: a0c2070 (Task 2)
