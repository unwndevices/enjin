---
phase: 65-allocation-verification
plan: 01
subsystem: testing
tags: [allocation-verification, operator-new, raii, benchmarks, ci, lua, canvas, ecs]

requires:
  - phase: 64-ci-regression-pipeline
    provides: benchmarks.yml CI workflow and build-bench.sh script to extend
  - phase: 61-native-benchmark-suite
    provides: bench_canvas.cpp, bench_ecs.cpp, bench_lua.cpp patterns; enjin2_lua target

provides:
  - AllocGuard RAII header (include/enjin2/instrumentation/alloc_guard.hpp) with enabled/no-op stub paths
  - bench_alloc binary that proves 6 hot-path operations are allocation-free
  - Operator new override pattern for catching ALL C++ heap allocations in a scope
  - CI enforcement: bench_alloc runs in GitHub Actions benchmarks.yml as explicit step

affects:
  - any phase modifying Canvas4, Scene::update, Lua bindings hot paths
  - any phase adding new hot-path operations that should be zero-alloc

tech-stack:
  added: []
  patterns:
    - "AllocGuard RAII: thread_local counter arms/disarms in ctor/dtor; exit(1) on detected alloc"
    - "Operator new override in bench TU: global override applies to all linked libraries"
    - "Registry-ref Lua calling: lua_rawgeti + lua_call avoids luaL_loadstring parse allocations"
    - "g_alloc_count reset after setup: prevents setup allocations from polluting hot-path checks"

key-files:
  created:
    - include/enjin2/instrumentation/alloc_guard.hpp
    - benchmarks/bench_alloc.cpp
  modified:
    - benchmarks/CMakeLists.txt
    - scripts/build-bench.sh
    - .github/workflows/benchmarks.yml

key-decisions:
  - "Operator new override lives in bench_alloc.cpp TU (not in a header) to avoid ODR violations across TUs"
  - "g_alloc_guard_depth is NOT static — declared extern in alloc_guard.hpp so header-only class can access it"
  - "Lua binding tested via lua_rawgeti + lua_call on pre-registered deltaRef, NOT executeString — executeString allocates for chunk parsing"
  - "g_alloc_count reset to 0 after setup completes — Lua state init and addObject allocations must not bleed into guarded sections"
  - "bench_alloc uses ANKERL_NANOBENCH_IMPLEMENT for doNotOptimizeAway (prevents dead-code elimination of hot paths)"
  - "All six operator new/delete forms overridden — C++14 sized deallocation requires delete(void*, size_t) forms to prevent bypass"
  - "bench_alloc added to benchmarks.yml as explicit step (redundant with build-bench.sh but provides separate CI log visibility)"

patterns-established:
  - "AllocGuard: always reset g_alloc_count after setup, before first AllocGuard scope"
  - "Lua zero-alloc testing: use luaL_ref + lua_rawgeti not executeString"
  - "bench_alloc structure: operator new override BEFORE any includes that could call new"

requirements-completed: [ALLOC-01, ALLOC-02, ALLOC-03]

duration: 2min
completed: 2026-03-08
---

# Phase 65 Plan 01: Allocation Verification Summary

**AllocGuard RAII header + bench_alloc binary proving Canvas4 ops, scene::update, and Lua engine.time.delta binding are all zero-heap in hot paths**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-08T12:08:23Z
- **Completed:** 2026-03-08T12:10:34Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- AllocGuard header-only RAII class with compile-time enabled/no-op stub paths matches frame_timing.hpp conventions
- bench_alloc binary overrides all six operator new/delete forms, wraps 6 hot-path operations in guards, exits 0 with `[ALLOC-PASS]`
- build-bench.sh extended to build and run bench_alloc as part of the full benchmark suite
- benchmarks.yml gains explicit "Allocation verification" CI step after "Build and run benchmarks"

## Task Commits

Each task was committed atomically:

1. **Task 1: Create AllocGuard header and bench_alloc verification binary** - `0af5325` (feat)
2. **Task 2: Extend build-bench.sh and CI workflow for allocation verification** - `abfdac3` (feat)

**Plan metadata:** (pending docs commit)

## Files Created/Modified

- `include/enjin2/instrumentation/alloc_guard.hpp` - AllocGuard RAII class; enabled path uses extern thread_local counters; no-op stub when ENJIN2_ALLOC_VERIFICATION not defined
- `benchmarks/bench_alloc.cpp` - operator new override + 6 hot-path AllocGuard sections (canvas setPixel/clear/fillRect/blit, scene::update x8, lua engine.time.delta via registry ref)
- `benchmarks/CMakeLists.txt` - bench_alloc target with ENJIN2_ALLOC_VERIFICATION=1, guarded by if(TARGET enjin2_lua)
- `scripts/build-bench.sh` - builds bench_alloc alongside existing targets; runs it after bench_lua
- `.github/workflows/benchmarks.yml` - explicit "Allocation verification" step after "Build and run benchmarks"

## Decisions Made

- Operator new override lives in bench_alloc.cpp TU (not in alloc_guard.hpp) — headers included in multiple TUs would cause ODR violations
- g_alloc_guard_depth/g_alloc_count are non-static (NOT `static thread_local`) so alloc_guard.hpp can declare them `extern` and access them
- Lua binding tested via pre-registered registry reference (luaL_ref + lua_rawgeti + lua_call) — executeString path calls luaL_loadstring which compiles Lua source and allocates
- g_alloc_count reset to 0 after all setup (Lua init, registerAll, addObject x8) to prevent setup allocations from triggering false positives
- All six operator new/delete forms overridden — C++14 -fsized-deallocation (GCC default) requires sized delete forms to prevent bypass

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 65 complete: enjin2's "zero dynamic allocation" core value is now machine-verifiable in CI
- bench_alloc exits 0 on CI, failing the job if any operator new fires in canvas/ECS/Lua hot paths
- Note: Lua's internal allocator (l_alloc via realloc) is NOT caught by operator new override — this is intentional; the guard verifies C++ binding code does not call new, not Lua VM internals

---
*Phase: 65-allocation-verification*
*Completed: 2026-03-08*
