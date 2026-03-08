---
phase: 66-performance-documentation
plan: 01
subsystem: documentation
tags: [nanobench, benchmarks, performance, profiling, alloc-guard, frame-timing, lua-profiler, ci]

# Dependency graph
requires:
  - phase: 65-allocation-verification
    provides: AllocGuard RAII header and bench_alloc binary used as documentation subject
  - phase: 64-ci-regression-pipeline
    provides: benchmarks.yml workflow and bench-data branch architecture documented
  - phase: 63-lua-profiler-headless-runner
    provides: LuaProfiler and enjin_run headless runner documented
  - phase: 62-frame-timing-instrumentation
    provides: FrameTimingInstrumentation dual-path header documented
  - phase: 61-native-benchmark-suite
    provides: bench_canvas/ecs/lua binaries and measured results documented
provides:
  - docs/PERFORMANCE.md covering all 5 performance subsystems with how-to-first structure
  - README.md link to performance documentation
  - 5-step Adding New Benchmarks guide
  - Per-platform frame budget table (SDL3 measured, WASM/ESP32 estimated)
affects: [contributors, new-developers, ci-pipeline]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "How-to-first documentation: every subsystem starts with a runnable command before explaining architecture"
    - "Standalone Markdown doc in docs/ root (no Docusaurus front matter) for accessibility without hosting"

key-files:
  created:
    - docs/PERFORMANCE.md
  modified:
    - README.md

key-decisions:
  - "docs/PERFORMANCE.md placed at docs/ root (not docs/src/) as standalone file — no Docusaurus front matter needed, accessible without Docusaurus build"
  - "README link uses relative path docs/PERFORMANCE.md (not hosted URL) since the file lives in the repo"

patterns-established:
  - "Performance doc structure: Quick Start -> subsystem sections -> Frame Budgets table -> Adding New Benchmarks guide -> Architecture Notes"

requirements-completed: [DOC-01, DOC-02, DOC-03]

# Metrics
duration: 2min
completed: 2026-03-08
---

# Phase 66 Plan 01: Performance Documentation Summary

**Complete performance guide covering benchmark suite, CI pipeline, frame timing, Lua profiler, and allocation verification — with measured SDL3 results tables and a 5-step Adding New Benchmarks guide**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-08T13:33:13Z
- **Completed:** 2026-03-08T13:35:26Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Created `docs/PERFORMANCE.md` with 34 section headers covering all 5 subsystems (benchmark suite, CI pipeline, frame timing, Lua profiler, allocation verification) in how-to-first structure
- Included actual measured SDL3 desktop benchmark values in three tables (Canvas, ECS, Lua) sourced from bench-results/ JSON files
- Added 5-step Adding New Benchmarks guide and per-platform Frame Budgets table (SDL3 measured, WASM/ESP32 estimated with explicit caveats)
- Added `[Performance](docs/PERFORMANCE.md)` link to README.md Documentation section

## Task Commits

Each task was committed atomically:

1. **Task 1: Write docs/PERFORMANCE.md with all 5 subsystems** - `e456e5f` (feat)
2. **Task 2: Add performance docs link to README.md** - `863c806` (feat)

**Plan metadata:** (this commit)

## Files Created/Modified

- `docs/PERFORMANCE.md` - Complete performance guide covering all 5 subsystems with measured values, frame budgets, and contributor walkthrough
- `README.md` - Added Performance link to Documentation section

## Decisions Made

- `docs/PERFORMANCE.md` placed at `docs/` root (not `docs/src/`) as a standalone Markdown file with no Docusaurus front matter, per RESEARCH.md recommendation — accessible as a flat file without Docusaurus build infrastructure
- README link uses relative path `docs/PERFORMANCE.md` (consistent with a file in the repo, not a hosted URL)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 66 complete — all v1.10 Benchmarking & Performance milestone requirements fulfilled (DOC-01, DOC-02, DOC-03)
- `docs/PERFORMANCE.md` is the single entry point for contributors to understand, run, and extend the entire performance infrastructure
- No blockers for any future work

## Self-Check

- [x] `docs/PERFORMANCE.md` exists: FOUND
- [x] `README.md` updated: FOUND
- [x] Commit `e456e5f` exists: FOUND
- [x] Commit `863c806` exists: FOUND

## Self-Check: PASSED

---
*Phase: 66-performance-documentation*
*Completed: 2026-03-08*
