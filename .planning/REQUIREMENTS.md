# Requirements: enjin2 v1.10 Benchmarking & Performance

**Defined:** 2026-03-07
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1.10 Requirements

### Benchmark Suite

- [x] **BENCH-01**: nanobench vendored as single header in vendor/ with CMake ENJIN2_BUILD_BENCHMARKS option
- [x] **BENCH-02**: bench_canvas binary benchmarks Canvas4/Canvas8 pixel ops, fill, rect, circle, sprite blit, multi-layer composite
- [x] **BENCH-03**: bench_ecs binary benchmarks Object creation, component attach/detach, scene::update() at 1/8/16/32/48 objects, event dispatch
- [x] **BENCH-04**: bench_lua binary benchmarks Lua engine init, script load, per-module binding call overhead, ObjectProxy round-trip, GC pressure
- [x] **BENCH-05**: All benchmark binaries produce JSON output to bench-results/ directory
- [x] **BENCH-06**: scripts/build-bench.sh builds and runs all benchmarks in one command

### CI Pipeline

- [x] **CI-01**: GitHub Actions workflow (.github/workflows/benchmarks.yml) triggers on push to main and PRs touching src/** or include/**
- [x] **CI-02**: JSON conversion script combines benchmark results into github-action-benchmark customSmallerIsBetter format
- [x] **CI-03**: Benchmark history stored on gh-pages branch (isolated from Docusaurus deployment)
- [x] **CI-04**: Performance dashboard auto-generated on gh-pages from benchmark history
- [x] **CI-05**: Regression threshold with fail-on-alert for PRs

### Frame Timing

- [x] **FRAME-01**: FrameTimingInstrumentation struct with lock-free uint32_t atomics tracking updateTime_us, renderTime_us, luaTime_us, compositeTime_us
- [x] **FRAME-02**: Per-phase timing instrumented into SDL3 runner game loop
- [x] **FRAME-03**: Frame budget usage exposed via debug overlay or polling API

### Lua Profiling

- [x] **PROF-01**: C-level profiler via lua_sethook (LUA_MASKCALL | LUA_MASKRET) with per-function call counts
- [x] **PROF-02**: Memory tracking via lua_gc with per-frame GC pressure ring buffer
- [x] **PROF-03**: Zero overhead when profiler disabled (lua_sethook(L, NULL, 0, 0))
- [x] **PROF-04**: Headless CLI runner (enjin_run) with --profile --frames N script.lua
- [x] **PROF-05**: enjin_run produces JSON and text table output formats
- [x] **PROF-06**: enjin_run stubs all platform APIs (gfx, input) as no-ops

### Allocation Verification

- [x] **ALLOC-01**: Custom allocator wrapper counts malloc/free calls during benchmarked hot-path sections
- [x] **ALLOC-02**: CI check runs benchmarks under allocation counter and fails if any hot-path allocation detected
- [x] **ALLOC-03**: Canvas operations, Component updates, and Lua binding calls verified allocation-free

### Documentation

- [ ] **DOC-01**: docs/PERFORMANCE.md covers all 5 subsystems with how-to-first structure
- [ ] **DOC-02**: Quick start with scripts/build-bench.sh one-liner and adding-new-benchmarks guide
- [ ] **DOC-03**: Per-platform frame budget reference (ESP32 vs WASM vs SDL3)

## Future Requirements

### Platform-Specific Instrumentation

- **PLAT-01**: WASM frame timing via Emscripten performance APIs
- **PLAT-02**: ESP32 frame timing via esp_timer_get_time() in FreeRTOS game loop
- **PLAT-03**: Lua profiler SDL3 overlay panel with live data visualization

## Out of Scope

| Feature | Reason |
|---------|--------|
| Google Benchmark | nanobench is lighter, faster compile, proven JSON path, matches tomodachi pattern |
| Tracy Profiler | Requires background thread + network; violates zero-alloc/zero-threading model |
| LuaJIT benchmarking | Not available on WASM or ESP32; measures different runtime than what ships |
| Valgrind/ASan for alloc verification | Cannot distinguish hot-path vs cold-path allocations |
| Lua-level debug.sethook profiler | Hook overhead invalidates measurements (PIL 23.3) |
| Benchmark on WASM/ESP32 targets in CI | Requires Emscripten/ESP-IDF toolchain in CI; native desktop is reproducible |
| Runtime Lua toggle for timing | Lua binding overhead pollutes measurement; compile-time gate is correct |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| BENCH-01 | Phase 60 | Complete |
| BENCH-02 | Phase 61 | Complete |
| BENCH-03 | Phase 61 | Complete |
| BENCH-04 | Phase 61 | Complete |
| BENCH-05 | Phase 61 | Complete |
| BENCH-06 | Phase 61 | Complete |
| CI-01 | Phase 64 | Complete |
| CI-02 | Phase 64 | Complete |
| CI-03 | Phase 64 | Complete |
| CI-04 | Phase 64 | Complete |
| CI-05 | Phase 64 | Complete |
| FRAME-01 | Phase 62 | Complete |
| FRAME-02 | Phase 62 | Complete |
| FRAME-03 | Phase 62 | Complete |
| PROF-01 | Phase 63 | Complete |
| PROF-02 | Phase 63 | Complete |
| PROF-03 | Phase 63 | Complete |
| PROF-04 | Phase 63 | Complete |
| PROF-05 | Phase 63 | Complete |
| PROF-06 | Phase 63 | Complete |
| ALLOC-01 | Phase 65 | Complete |
| ALLOC-02 | Phase 65 | Complete |
| ALLOC-03 | Phase 65 | Complete |
| DOC-01 | Phase 66 | Pending |
| DOC-02 | Phase 66 | Pending |
| DOC-03 | Phase 66 | Pending |

**Coverage:**
- v1.10 requirements: 26 total
- Mapped to phases: 26
- Unmapped: 0

---
*Requirements defined: 2026-03-07*
*Last updated: 2026-03-07 after roadmap creation (Phases 60-66)*
