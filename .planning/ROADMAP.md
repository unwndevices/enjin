# Roadmap: enjin2

## Milestones

- [x] **v1.0 Migration + Documentation** - Phases 1-6 (shipped 2026-02-01)
- [x] **v1.1 Project Infrastructure & Documentation Enhancement** - Phases 7-15 (shipped 2026-02-23)
- [x] **v1.2 Tech Debt Cleanup** - Phases 16-18 (shipped 2026-02-23)
- [x] **v1.3 Tomodachi Readiness** - Phases 19-22 (shipped 2026-02-24)
- [x] **v1.4 Engine Capabilities** - Phases 23-26 (shipped 2026-02-26)
- [x] **v1.5 Lua Scripting Foundation** - Phases 27-38 (shipped 2026-02-28)
- [x] **v1.6 Game Ready** - Phases 39-42 (shipped 2026-02-28)
- [x] **v1.7 Developer Experience & New Capability** - Phases 43-52 (shipped 2026-03-02)
- [x] **v1.8 Ship Ready** - Phases 53-58 (shipped 2026-03-03)
- [x] **v1.9 Tech Debt Resolved** - Phase 59 (shipped 2026-03-03)
- [ ] **v1.10 Benchmarking & Performance** - Phases 60-66 (in progress)

## Phases

<details>
<summary>v1.0 Migration + Documentation (Phases 1-6) - SHIPPED 2026-02-01</summary>

Phases 1-6 complete. See milestones/v1.0-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.1 Project Infrastructure & Documentation Enhancement (Phases 7-15) - SHIPPED 2026-02-23</summary>

Phases 7-15 complete. See milestones/v1.1-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.2 Tech Debt Cleanup (Phases 16-18) - SHIPPED 2026-02-23</summary>

Phases 16-18 complete. See milestones/v1.2-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.3 Tomodachi Readiness (Phases 19-22) - SHIPPED 2026-02-24</summary>

Phases 19-22 complete. See milestones/v1.3-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.4 Engine Capabilities (Phases 23-26) - SHIPPED 2026-02-26</summary>

Phases 23-26 complete. See milestones/v1.4-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.5 Lua Scripting Foundation (Phases 27-38) - SHIPPED 2026-02-28</summary>

Phases 27-38 complete. See milestones/v1.5-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.6 Game Ready (Phases 39-42) - SHIPPED 2026-02-28</summary>

Phases 39-42 complete. See milestones/v1.6-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.7 Developer Experience & New Capability (Phases 43-52) - SHIPPED 2026-03-02</summary>

Phases 43-52 complete. See milestones/v1.7-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.8 Ship Ready (Phases 53-58) --- SHIPPED 2026-03-03</summary>

- [x] Phase 53: Environment and Build Verification (3/3 plans) --- completed 2026-03-02
- [x] Phase 54: JSON Serializer Refactor (1/1 plan) --- completed 2026-03-02
- [x] Phase 55: Platform Storage Backends (2/2 plans) --- completed 2026-03-02
- [x] Phase 56: Tech Debt Cleanup (1/1 plan) --- completed 2026-03-02
- [x] Phase 57: QoL Features (3/3 plans) --- completed 2026-03-02
- [x] Phase 58: Documentation and Build Tooling (3/3 plans) --- completed 2026-03-02

</details>

<details>
<summary>v1.9 Tech Debt Resolved (Phase 59) --- SHIPPED 2026-03-03</summary>

- [x] Phase 59: Tech Debt and Known Issues (2/2 plans) --- completed 2026-03-03

</details>

### v1.10 Benchmarking & Performance (Phases 60-66)

- [x] **Phase 60: CMake Foundation & Vendor** - ENJIN2_BUILD_BENCHMARKS option, nanobench single-header, directory scaffolding (completed 2026-03-07)
- [ ] **Phase 61: Native Benchmark Suite** - bench_canvas, bench_ecs, bench_lua executables with JSON output and build-bench.sh
- [ ] **Phase 62: Frame Timing Instrumentation** - FrameTimingInstrumentation struct, SDL3 runner integration, debug overlay
- [ ] **Phase 63: Lua Profiler & Headless Runner** - enjin_run CLI, lua_sethook profiler, JSON/text output, null-safe stub host
- [ ] **Phase 64: CI Regression Pipeline** - GitHub Actions workflow, github-action-benchmark dashboard, gh-pages storage
- [ ] **Phase 65: Allocation Verification** - AllocGuard RAII, operator new override, CI hot-path zero-alloc enforcement
- [ ] **Phase 66: Performance Documentation** - docs/PERFORMANCE.md covering all subsystems with measured numbers

## Phase Details

### Phase 60: CMake Foundation & Vendor
**Goal**: The build system is ready to compile benchmark and tool targets; no benchmark code exists yet but every subsequent phase can start immediately
**Depends on**: Nothing (first phase of v1.10)
**Requirements**: BENCH-01
**Success Criteria** (what must be TRUE):
  1. `cmake -DENJIN2_BUILD_BENCHMARKS=ON ..` configures without error and the benchmarks CMake subdirectory is included
  2. `vendor/nanobench.h` exists at the expected path and a trivial include compiles cleanly
  3. `cmake -DENJIN2_BUILD_BENCHMARKS=OFF ..` (or default) produces no benchmark targets and does not touch ESP32 or WASM targets
  4. `bench-results/` is listed in `.gitignore` so generated JSON is never committed
**Plans:** 1/1 plans complete
Plans:
- [x] 60-01-PLAN.md -- Vendor nanobench, create benchmarks/ scaffold, patch root CMakeLists.txt

### Phase 61: Native Benchmark Suite
**Goal**: Developers can run a single command to benchmark canvas ops, ECS throughput, and Lua binding overhead and get JSON results locally
**Depends on**: Phase 60
**Requirements**: BENCH-02, BENCH-03, BENCH-04, BENCH-05, BENCH-06
**Success Criteria** (what must be TRUE):
  1. `scripts/build-bench.sh` executes without error and three binaries (`bench_canvas`, `bench_ecs`, `bench_lua`) appear in the build directory
  2. Each binary writes a valid JSON file into `bench-results/` covering its named subsystem operations (pixel ops/fill/rect/circle/composite for canvas; object create/attach/update at multiple counts for ECS; engine init/script load/binding call/GC pressure for Lua)
  3. Results are not invalidated by dead code elimination --- canvas benchmark reports non-trivially short timings consistent with actual pixel writes at -O2
  4. All three binaries link and run without SDL3 being present or initialized
**Plans**: TBD

### Phase 62: Frame Timing Instrumentation
**Goal**: Developers can see where frame time is spent (update/render/Lua/composite) with sub-millisecond resolution in the SDL3 runner
**Depends on**: Phase 60
**Requirements**: FRAME-01, FRAME-02, FRAME-03
**Success Criteria** (what must be TRUE):
  1. Running the SDL3 runner with `--show-timing` displays a per-phase breakdown (updateTime_us, renderTime_us, luaTime_us, compositeTime_us) on screen each frame
  2. A polling API exists so host code can read the four timing fields without the overlay (e.g., `FrameTimingInstrumentation::get()`)
  3. Disabling the overlay at compile time or runtime incurs zero overhead --- no atomics written, no counters incremented when instrumentation is off
  4. WASM and ESP32 builds compile cleanly with the new header present (even if full instrumentation is not wired on those platforms)
**Plans**: TBD

### Phase 63: Lua Profiler & Headless Runner
**Goal**: Developers can profile any Lua script's function-level call counts and GC pressure from the command line without needing a display or SDL3 window
**Depends on**: Phase 61
**Requirements**: PROF-01, PROF-02, PROF-03, PROF-04, PROF-05, PROF-06
**Success Criteria** (what must be TRUE):
  1. `enjin_run --frames 100 script.lua` executes a Lua script for 100 simulated frames and exits cleanly without a window, display, or input device
  2. `enjin_run --profile --frames 100 script.lua` prints a sorted table of per-function call counts to stdout
  3. `enjin_run --profile --output json --frames 100 script.lua` writes a JSON file of profiling results
  4. A script that exercises every `engine.*` subtable (scene, input, time, lua, log) runs without null-dereference crash in headless mode
  5. Running `enjin_run` without `--profile` incurs zero hook overhead --- `lua_sethook(L, NULL, 0, 0)` is confirmed in that path
**Plans**: TBD

### Phase 64: CI Regression Pipeline
**Goal**: Every push to main automatically benchmarks the codebase, records results, and a public dashboard shows performance history; PRs are alerted when a regression threshold is crossed
**Depends on**: Phase 61
**Requirements**: CI-01, CI-02, CI-03, CI-04, CI-05
**Success Criteria** (what must be TRUE):
  1. A push to `main` touching `src/**` or `include/**` triggers `.github/workflows/benchmarks.yml` and the workflow completes without error
  2. Benchmark history accumulates on a dedicated branch (`bench-data`) that is not overwritten by the existing Docusaurus `docs.yml` deploy-pages workflow
  3. The gh-pages performance dashboard URL is accessible and shows at least one data point after the first successful workflow run
  4. A PR that introduces a benchmark result exceeding the regression threshold triggers an alert visible in the GitHub Actions log
  5. The JSON conversion script produces valid `customSmallerIsBetter` format accepted by github-action-benchmark without manual intervention
**Plans**: TBD

### Phase 65: Allocation Verification
**Goal**: CI proves with instrumented evidence that canvas operations, component updates, and Lua binding calls in hot paths perform zero heap allocations
**Depends on**: Phase 61
**Requirements**: ALLOC-01, ALLOC-02, ALLOC-03
**Success Criteria** (what must be TRUE):
  1. `AllocGuard` RAII wraps a hot-path benchmark section and the benchmark binary exits non-zero if any `operator new` call fires inside that scope
  2. The CI workflow step running allocation verification fails the job when a hot-path allocation is detected, and passes when none occur
  3. Canvas pixel ops, `Canvas4::clear()`, component `update()` dispatch, and Lua binding calls to `engine.*` all pass the zero-alloc check in CI
**Plans**: TBD

### Phase 66: Performance Documentation
**Goal**: Any contributor can understand how to run benchmarks, interpret results, and add new benchmark cases without reading source code
**Depends on**: Phase 65, Phase 64, Phase 63, Phase 62
**Requirements**: DOC-01, DOC-02, DOC-03
**Success Criteria** (what must be TRUE):
  1. `docs/PERFORMANCE.md` exists and covers all five subsystems (benchmark suite, CI pipeline, frame timing, Lua profiler, allocation verification) with a how-to-first structure
  2. The quick-start section has a `scripts/build-bench.sh` one-liner that a developer can copy-paste and run successfully on a fresh checkout
  3. A "Adding New Benchmarks" section walks through the steps to add a new nanobench case and have it appear in CI results
  4. Per-platform frame budget numbers for ESP32, WASM, and SDL3 are documented with actual measured values from Phase 62 results

**Plans**: TBD

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-6. Migration + Docs | v1.0 | 21/21 | Complete | 2026-02-01 |
| 7-15. Infrastructure | v1.1 | 17/17 | Complete | 2026-02-23 |
| 16-18. Tech Debt | v1.2 | 5/5 | Complete | 2026-02-23 |
| 19-22. Tomodachi Readiness | v1.3 | 7/7 | Complete | 2026-02-24 |
| 23-26. Engine Capabilities | v1.4 | 8/8 | Complete | 2026-02-26 |
| 27-38. Lua Scripting Foundation | v1.5 | 21/21 | Complete | 2026-02-28 |
| 39-42. Game Ready | v1.6 | 4/4 | Complete | 2026-02-28 |
| 43-52. Developer Experience | v1.7 | 19/19 | Complete | 2026-03-02 |
| 53-58. Ship Ready | v1.8 | 13/13 | Complete | 2026-03-03 |
| 59. Tech Debt | v1.9 | 2/2 | Complete | 2026-03-03 |
| 60. CMake Foundation & Vendor | v1.10 | 1/1 | Complete | 2026-03-07 |
| 61. Native Benchmark Suite | v1.10 | 0/? | Not started | - |
| 62. Frame Timing Instrumentation | v1.10 | 0/? | Not started | - |
| 63. Lua Profiler & Headless Runner | v1.10 | 0/? | Not started | - |
| 64. CI Regression Pipeline | v1.10 | 0/? | Not started | - |
| 65. Allocation Verification | v1.10 | 0/? | Not started | - |
| 66. Performance Documentation | v1.10 | 0/? | Not started | - |
