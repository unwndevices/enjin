---
gsd_state_version: 1.0
milestone: v1.10
milestone_name: Benchmarking & Performance
status: completed
stopped_at: Completed 66-01-PLAN.md — docs/PERFORMANCE.md written with all 5 subsystems, README.md linked
last_updated: "2026-03-08T13:36:19.473Z"
last_activity: "2026-03-07 — Plan 60-01 complete: nanobench vendored, bench_smoke builds and runs"
progress:
  total_phases: 7
  completed_phases: 7
  total_plans: 10
  completed_plans: 10
  percent: 14
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-07)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.10 Benchmarking & Performance — Phase 60 complete, Phase 61 (Lua benchmark) next

## Current Position

Phase: 60 of 66 (CMake Foundation & Vendor)
Plan: 1 of 1 (60-01 complete)
Status: Phase 60 complete — ready for Phase 61
Last activity: 2026-03-07 — Plan 60-01 complete: nanobench vendored, bench_smoke builds and runs

Progress: [█░░░░░░░░░] 14% (v1.10)

## Performance Metrics

**Velocity:**
- Total plans completed: 121 (v1.0-v1.9)
- v1.0: 21 | v1.1: 17 | v1.2: 5 | v1.3: 7 | v1.4: 8 | v1.5: 21 | v1.6: 4 | v1.7: 19 | v1.8: 13 | v1.9: 2

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.

Recent decisions affecting current work:
- Research: Start CI regression threshold at 150% (not 110%) due to shared runner variance; tighten after 30-50 baseline runs
- Research: Store benchmark history on separate `bench-data` branch — NOT gh-pages — to prevent Docusaurus deploy-pages from wiping history
- Research: Phase 63 (Lua profiler + headless runner) needs null-safe MinimalLuaHost before any script runs; enumerate all `engine.*` pointer registrations first
- Research: Phase 64 (CI pipeline) needs proof-of-concept `external-data-json-path` + deploy-pages coexistence before recording any baseline
- 60-01: nanobench_vendor is INTERFACE target (not STATIC) — single headers need no compilation at library level
- 60-01: bench_smoke links only enjin2_core + enjin2_graphics; enjin2_lua excluded — Lua benchmark infrastructure owned by Phase 61
- 60-01: FATAL_ERROR guard on EMSCRIPTEN OR ESP32 chosen over silent skip to prevent silent misconfiguration
- [Phase 61-01]: blit() requires same-size canvas dimensions — Canvas4<W,H>.blit(const Canvas4<W,H>&,...) — sprite changed to Canvas4<128,128>
- [Phase 61-01]: if(TARGET enjin2_lua) guard chosen over if(ENJIN2_BUILD_LUA) in benchmarks/CMakeLists.txt — target existence is the definitive check
- [Phase 61-01]: build-bench.sh uses separate build-bench/ directory; ENJIN2_BUILD_LUA=ON added to ensure bench_lua always built
- [Phase 61-02]: setActiveScene(nullptr) cleanup placed before GC benchmark to prevent dangling Lua registry pointer after BenchScene destructor
- [Phase 61-02]: Event subscription placed outside timed lambda via executeString — only emit() is measured as the hot path
- [Phase 62-01]: #include <atomic> placed outside namespace enjin2 to avoid nesting std namespace inside enjin2 (GCC 15 error)
- [Phase 62-01]: ENJIN2_FRAME_TIMING=1 injected only to enjin2_sdl target — WASM and ESP32 targets use the zero-overhead plain uint32_t stub
- [Phase 63-01]: lua_profiler.hpp includes lua_platform.hpp (not raw lua.h) for cross-platform Lua include guard
- [Phase 63-01]: hookCallback checks active flag AFTER lua_getinfo+lua_topointer to avoid Lua stack imbalance on early return
- [Phase 63-01]: null_safety test uses static LayerCompositor<128,128> + LuaCanvas wrappers — zero heap allocation matching headless runner pattern
- [Phase 63-02]: setInput() NOT called in headless runner — currentInput remains nullptr; all engine.input.* bindings null-guard safely
- [Phase 63-02]: ENJIN2_BUILD_HEADLESS FATAL_ERROR on EMSCRIPTEN or ESP32 — same guard pattern as ENJIN2_BUILD_BENCHMARKS
- [Phase 63-02]: No ENJIN2_FRAME_TIMING define for enjin_run — headless runner does not use frame timing atomics
- [Phase 64]: bench-data orphan branch (not gh-pages) — docs.yml uses actions/deploy-pages which would wipe benchmark history
- [Phase 64]: alert-threshold: 150% on PR regression check — shared runner variance; tighten after 30-50 baseline runs
- [Phase 64]: cancel-in-progress: false — interrupted auto-push leaves bench-data in partial state
- [Phase 64]: workflow_dispatch trigger added — initial commits may not touch src/** or include/**, manual seed run needed
- [Phase 64]: bench-data orphan branch pushed with single empty root commit; main branch simultaneously pushed with 45 pending commits including benchmarks.yml; first CI run requires manual workflow_dispatch
- [Phase 64]: FindLua sets LUA_INCLUDE_DIR singular — always normalise to LUA_INCLUDE_DIRS after find_package(Lua) in desktop else branch
- [Phase 64]: ObjectProxy __gc metamethod required — without it, Lua GC frees proxy userdata while Object::m_luaProxy still holds raw pointer causing heap-use-after-free in Object::~Object()
- [Phase 65]: Operator new override in bench_alloc.cpp TU (not header) to avoid ODR violations; g_alloc_count reset after setup; Lua binding tested via lua_rawgeti not executeString; all six delete forms overridden for C++14 sized deallocation
- [Phase 66]: docs/PERFORMANCE.md placed at docs/ root (not docs/src/) as standalone Markdown file — no Docusaurus front matter, accessible without Docusaurus build
- [Phase 66]: README link to docs/PERFORMANCE.md uses relative path (not hosted URL) since the file lives in the repo

### Pending Todos

None.

### Blockers/Concerns

- Phase 63: null-binding safety contract for headless enjin_run not yet mapped — enumerate all engine.* subtable pointer registrations before implementation (segfault risk)
- Phase 64: gh-pages storage strategy (external-data-json-path coexistence with docs.yml) needs verified playbook before first baseline is recorded

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 8 | move to lua 5.4 and check all impacted areas | 2026-03-04 | 876beb8 | [8-move-to-lua-5-4-and-check-all-impacted-a](./quick/8-move-to-lua-5-4-and-check-all-impacted-a/) |
| 9 | update examples arduino to lua54 lib + idf CMakeLists | 2026-03-04 | 0ea4d52 | [9-update-examples-arduino-to-lua54-lib-idf](./quick/9-update-examples-arduino-to-lua54-lib-idf/) |
| Phase 61-native-benchmark-suite P01 | 3 | 3 tasks | 6 files |
| Phase 61 P02 | 2min | 1 tasks | 1 files |
| Phase 62-frame-timing-instrumentation P01 | 5min | 2 tasks | 5 files |
| Phase 63-lua-profiler-headless-runner P01 | 2 | 2 tasks | 3 files |
| Phase 63-lua-profiler-headless-runner P02 | 2min | 2 tasks | 2 files |
| Phase 64 P01 | 2 | 2 tasks | 2 files |
| Phase 64 P02 | 98 | 1 tasks | 0 files |
| Phase 64-ci-regression-pipeline P02 | 2h | 2 tasks | 2 files |
| Phase 65-allocation-verification P01 | 2min | 2 tasks | 5 files |
| Phase 66 P01 | 2 | 2 tasks | 2 files |

### Technical Debt (carried forward)

None.

## Session Continuity

Last session: 2026-03-08T13:36:19.471Z
Stopped at: Completed 66-01-PLAN.md — docs/PERFORMANCE.md written with all 5 subsystems, README.md linked
Resume file: None
