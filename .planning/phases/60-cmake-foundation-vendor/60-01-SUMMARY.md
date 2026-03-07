---
phase: 60-cmake-foundation-vendor
plan: 01
subsystem: infra
tags: [cmake, nanobench, benchmarking, vendor, build-system]

# Dependency graph
requires: []
provides:
  - nanobench v4.3.11 single header vendored at vendor/nanobench.h
  - benchmarks/CMakeLists.txt with nanobench_vendor INTERFACE target
  - benchmarks/bench_smoke.cpp trivial smoke benchmark proving header compiles and runs
  - ENJIN2_BUILD_BENCHMARKS CMake option (OFF by default) with EMSCRIPTEN/ESP32 guard
  - bench-results/ gitignore entry preventing timing data commits
affects:
  - 61-lua-benchmark
  - 62-renderer-benchmark
  - 63-lua-profiler
  - 64-ci-pipeline
  - 65-regression-threshold

# Tech tracking
tech-stack:
  added:
    - nanobench v4.3.11 (single-header C++ microbenchmark library, ANSI/POSIX perf counters)
  patterns:
    - Vendored single-header pattern (vendor/ directory, INTERFACE CMake target)
    - Option-gated subdirectory pattern mirroring ENJIN2_BUILD_TESTS
    - Desktop-only guard via EMSCRIPTEN/ESP32 FATAL_ERROR

key-files:
  created:
    - vendor/nanobench.h
    - benchmarks/CMakeLists.txt
    - benchmarks/bench_smoke.cpp
  modified:
    - CMakeLists.txt
    - .gitignore

key-decisions:
  - "nanobench_vendor is an INTERFACE target (not STATIC/SHARED) because it is a single header — no compilation needed at library level"
  - "bench_smoke links only enjin2_core and enjin2_graphics (not enjin2_lua) — Lua dependency deferred to Phase 61 bench_lua"
  - "EMSCRIPTEN OR ESP32 FATAL_ERROR guard prevents accidental cross-platform benchmark builds with missing POSIX perf headers"
  - "bench-results/ gitignore entry at end of file after Tool caches section as specified in plan"

patterns-established:
  - "Benchmark INTERFACE target pattern: add_library(nanobench_vendor INTERFACE) + target_include_directories(... INTERFACE ${CMAKE_SOURCE_DIR}/vendor)"
  - "Option gate pattern for benchmarks mirrors existing ENJIN2_BUILD_TESTS gate exactly"

requirements-completed:
  - BENCH-01

# Metrics
duration: 2min
completed: 2026-03-07
---

# Phase 60 Plan 01: CMake Foundation & Vendor Summary

**nanobench v4.3.11 single header vendored, benchmarks/ scaffold created, and ENJIN2_BUILD_BENCHMARKS CMake option added with EMSCRIPTEN/ESP32 guard — bench_smoke compiles and runs timing output**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-07T21:22:20Z
- **Completed:** 2026-03-07T21:24:20Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Vendored nanobench v4.3.11 single header (124KB) at vendor/nanobench.h — 143 ANKERL_NANOBENCH symbols verified present
- Created benchmarks/ directory with nanobench_vendor INTERFACE CMake target and bench_smoke executable linking enjin2_core + enjin2_graphics
- Created bench_smoke.cpp with trivial noop benchmark (warmup=3, epochs=5) that produces timing table output
- Added ENJIN2_BUILD_BENCHMARKS option to root CMakeLists.txt (OFF default) with EMSCRIPTEN/ESP32 FATAL_ERROR guard
- Appended bench-results/ to .gitignore to prevent committing machine-specific timing JSON

## Task Commits

Each task was committed atomically:

1. **Task 1: Vendor nanobench header and create benchmarks directory scaffold** - `88b1906` (feat)
2. **Task 2: Patch root CMakeLists.txt with ENJIN2_BUILD_BENCHMARKS option and verify builds** - `7c14168` (feat)

## Files Created/Modified
- `vendor/nanobench.h` - nanobench v4.3.11 single header (124KB), ANKERL_NANOBENCH_IMPLEMENT pattern
- `benchmarks/CMakeLists.txt` - nanobench_vendor INTERFACE target + bench_smoke executable linked to enjin2_core, enjin2_graphics
- `benchmarks/bench_smoke.cpp` - Trivial noop benchmark, ANKERL_NANOBENCH_IMPLEMENT, warmup=3, epochs=5
- `CMakeLists.txt` - Added ENJIN2_BUILD_BENCHMARKS option and add_subdirectory(benchmarks) guard
- `.gitignore` - Added bench-results/ entry after Tool caches section

## Decisions Made
- Used INTERFACE library for nanobench_vendor (not STATIC) because single headers require no compilation at library level — only the translation unit that defines ANKERL_NANOBENCH_IMPLEMENT compiles the implementation
- bench_smoke links only enjin2_core and enjin2_graphics — enjin2_lua is intentionally excluded as Phase 61 owns Lua benchmark infrastructure
- FATAL_ERROR on EMSCRIPTEN OR ESP32 chosen over silent skip to prevent confusion from missed option

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. All CMake configuration, build, and execution steps succeeded on the first attempt. The bench_smoke binary produced timing output as expected.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All downstream phases (61-65) can immediately add benchmark targets by linking nanobench_vendor and enjin2_* libraries
- bench_smoke binary verifies the full pipeline: header include, IMPLEMENT define, Bench API, doNotOptimizeAway — foundation is solid
- No blockers

---
*Phase: 60-cmake-foundation-vendor*
*Completed: 2026-03-07*

## Self-Check: PASSED

All files verified present. All commits verified in git log.
