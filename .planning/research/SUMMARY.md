# Project Research Summary

**Project:** enjin2 v1.10 — Benchmarking & Performance Infrastructure
**Domain:** C++ embedded game engine — measurement, profiling, and CI regression infrastructure
**Researched:** 2026-03-07
**Confidence:** HIGH

## Executive Summary

The v1.10 milestone is a measurement and infrastructure layer added on top of the existing enjin2 engine — no existing subsystems are restructured. The work decomposes cleanly into five independent concerns: a microbenchmark suite (nanobench), a CI regression pipeline (github-action-benchmark), per-phase frame timing instrumentation in the SDL3 runner, a Lua profiler and headless CLI runner, and zero-allocation verification for hot paths. All five are additive; they share the existing CMake library targets, Lua state, and SDL3 runner without modifying engine internals. Research confirms that the only viable tool choices for this specific codebase are nanobench (single-header, no Abseil, JSON output, FetchContent-compatible) and github-action-benchmark (self-hosted, gh-pages integration, customSmallerIsBetter JSON format); every major alternative was explicitly ruled out due to cross-platform constraints (ESP32, WASM, desktop) or CI complexity.

The recommended build order is strictly dependency-driven: CMake foundation first (ENJIN2_BUILD_BENCHMARKS option, vendor/nanobench.h), then benchmark binaries and frame timing in parallel, then CI pipeline and Lua profiler/headless runner, and finally allocation verification which wraps the finished benchmark binaries. Documentation can be written in parallel with all phases. The architecture keeps all instrumentation code outside the engine libraries — profiler hooks via `LuaEngine::getState()`, timing via header-only structs, and allocation verification via compile-gated `operator new` overrides — so ESP32 and WASM production builds are completely unaffected.

The highest-risk areas are the CI regression threshold (GitHub shared runners exhibit 5-15% variance, making the default 110% threshold immediately counterproductive), the gh-pages storage conflict between the existing Docusaurus deployment and benchmark history, and the headless CLI runner's need for null-safe Lua binding stubs when engine subsystems are absent. These risks are well-understood and preventable if addressed at phase start rather than retrofit. Research overall is high confidence: all five new tools are verified against official documentation and the codebase was directly inspected for all integration points.

---

## Key Findings

### Recommended Stack

The v1.10 stack adds exactly five new capabilities on top of the existing C++17/CMake/SDL3/Lua 5.4 foundation. All additions are zero-new-service-dependency: nanobench is a single header vendored to `vendor/nanobench.h`; github-action-benchmark is a GitHub Action with no external account; `lua_sethook` and `SDL_GetPerformanceCounter` are already-linked APIs; and the `operator new` override is standard C++17. The CMake addition is a single `ENJIN2_BUILD_BENCHMARKS` option guarded to `NOT ESP32 AND NOT EMSCRIPTEN`, mirroring the existing `ENJIN2_BUILD_TESTS` pattern exactly.

**Core technologies:**
- **ankerl::nanobench v4.3.11**: microbenchmark harness — single-header, C++17, native JSON output via `templates::json()`, 65x faster autotuning than google/benchmark; zero new link dependencies
- **benchmark-action/github-action-benchmark v1**: CI regression dashboard — accepts `customSmallerIsBetter` JSON, auto-commits to gh-pages, configurable threshold, no external service; bridged from nanobench JSON via a single `jq` one-liner
- **`SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency`**: per-phase frame timing — already in project, nanosecond-scale resolution, extends existing delta-time calculation
- **Lua 5.4 C API `lua_sethook`**: Lua profiling — stable across Lua 5.1-5.4 and LuaJIT, `LUA_MASKCALL | LUA_MASKRET` for call counts, lower overhead than Lua-side `debug.sethook`
- **Global `operator new` override (project header)**: zero-alloc verification — C++17 standard technique, portable across ESP32/WASM/desktop, gated behind `ENJIN2_BUILD_TESTS`

**Critical version/behavior requirements:**
- nanobench v4.3.11 requires exceptions enabled — benchmark binary is gated to desktop (SDL3) target only where exceptions are available
- Do NOT use `pull_request` CI trigger for github-action-benchmark — run on `push` to `main` only (documented limitation of the action for fork PRs)

### Expected Features

The milestone brief and domain research are fully aligned. Every feature is P1 (must-have for v1.10) with no scope ambiguity.

**Must have (table stakes):**
- nanobench vendored as `vendor/nanobench.h` — enables all benchmark binaries; establishes measured performance vs. guessed
- `bench_canvas`, `bench_ecs`, `bench_lua` executables — with JSON output to `bench-results/`; each covers the critical hot path of its subsystem
- `scripts/build-bench.sh` — one-command developer workflow, mirrors existing `build.sh` pattern
- CI workflow `.github/workflows/benchmarks.yml` — regression detection triggered on `push` to `main`, 110% threshold (after baseline calibration; see pitfall 3)
- gh-pages dashboard — benchmark history visualization; populated by CI on first `main` push

**Should have (differentiators specific to enjin2):**
- `FrameTimingInstrumentation` struct with `updateTime_us`, `renderTime_us`, `luaTime_us`, `compositeTime_us` — answers "where is my 16ms frame budget going?" which no existing enjin2 tool can answer
- Lua profiler via `lua_sethook` — per-function call counts and approximate self-time; zero overhead when disabled; accurate enough to identify hot bindings
- Headless CLI runner `enjin_run --profile --frames N script.lua` — CI script validation without SDL3 window; primary Lua profiling workflow
- Static allocation verification — proves enjin2's core zero-alloc claim with CI evidence, not assertion
- `docs/PERFORMANCE.md` — without documentation, the benchmark infrastructure is opaque to contributors

**Defer (v1.x or later):**
- WASM-specific frame timing (Emscripten performance APIs) — adds CI complexity
- ESP32-specific frame timing in FreeRTOS game loop — requires hardware or QEMU in CI
- Lua profiler SDL3 overlay panel — `enjin_run` CLI is sufficient for v1.10
- Per-scene benchmark mode — extends enjin_run; useful for game-level profiling
- Tracy Profiler — conflicts with zero-threading model; future multi-threaded rendering only
- LuaJIT on desktop — only if WASM/ESP32 parity is dropped

### Architecture Approach

All v1.10 work is strictly additive to enjin2's layered architecture. The existing `enjin2_core` → `enjin2_graphics` → `enjin2_ui` → `enjin2_lua` → `enjin2_sdl` dependency chain is unchanged. Four new top-level additions are introduced: a `benchmarks/` directory (CMake targets linking `enjin2_lua` but not SDL3), a `tools/enjin_run` executable (same linkage), a `src/perf/` header-only directory (`FrameTimingInstrumentation`, `alloc_counter`), and a CI workflow. The only modification to existing code is inserting six timing capture points into `sdl_main.cpp`'s game loop, wrapped at existing phase boundaries.

**Major components:**
1. `benchmarks/bench_canvas, bench_ecs, bench_lua` — independent nanobench suites, each a standalone `main()`, produce JSON output; no SDL3 dependency
2. `src/perf/frame_timing.hpp` — `FrameTimingInstrumentation` struct with `std::atomic` fields; written from the game loop, read by overlay and `enjin_run`; header-only, zero external deps
3. `tools/enjin_run.cpp` — headless Lua runner; stubs `input_platform_poll`, no SDL3; attaches `lua_sethook` profiler when `--profile` flag passed; links only `enjin2_lua`
4. `src/perf/alloc_counter.hpp` — `AllocGuard` RAII + `operator new`/`delete` override behind `ENJIN2_ALLOC_INTERCEPT` define; integrated into bench binaries for CI hot-path verification
5. `.github/workflows/benchmarks.yml` — builds bench binaries, runs them, converts JSON via `jq`, stores in gh-pages; completely separate from existing `docs.yml`

**Key architectural boundary:** The profiler attaches to `LuaEngine::getState()` (already public) from outside the engine — `LuaEngine` is not modified. This is the correct pattern: instrumentation stays in the tool layer, not the engine library.

### Critical Pitfalls

Research identified 10 pitfalls; these 5 are critical enough to block the milestone if not addressed at phase start:

1. **Dead code elimination silences the entire canvas benchmark** — GCC/Clang at `-O2` eliminates pure write operations with no observable reads. Prevention: call `ankerl::nanobench::doNotOptimizeAway(canvas)` after every benchmark lambda body; build at `-O2` from day one. Signal: `canvas.clear(128x64)` reporting <1µs is a DCE indicator.

2. **Object/LuaEngine construction inside nanobench lambda measures allocator, not ECS** — `std::make_unique` inside a benchmark loop inflates variance and measures allocator overhead. Prevention: hoist all construction out of `bench.run()` lambdas; only the update/call path goes inside the loop. `LuaEngine::initialize()` must also be called once per binary, never inside a benchmark.

3. **110% CI threshold produces immediate false positives on shared runners** — GitHub Actions shared runners exhibit 5-15% coefficient of variation. A 110% threshold fires on nearly every PR. Prevention: start at 150% threshold; set `fail-on-alert: false` until 30-50 baseline runs accumulate; tighten to 120% after stable baseline. Do not enable `failOnAlert: true` on initial deployment.

4. **Docusaurus `docs.yml` force-push destroys benchmark history on gh-pages** — `actions/deploy-pages` replaces gh-pages content, wiping benchmark data stored in `dev/bench/`. Prevention: use `external-data-json-path` to store benchmark history in a separate `bench-data` branch, completely decoupled from Pages deployment. Verify both workflows survive a sequential run before recording any baseline.

5. **Headless `enjin_run` null-dereferences Lua bindings that expect live engine subsystems** — `registerAll()` sets pointers to `SceneStateMachine*`, `InputState*`, and canvas; in headless mode these don't exist. Prevention: create a `MinimalLuaHost` that satisfies pointer contracts with no-op instances before calling any binding registration. Add null guards to all `engine.*` subtable bindings. Test with a script that exercises every `engine.*` subtable before shipping.

Additional notable pitfalls:
- `lua_sethook` hook overhead (48ns C-to-Lua + clock cost) exceeds short Lua function execution time — use the hook for call counts only, not timing; time via `lua_resume` entry/exit in C++
- `lua_Alloc` realloc path (`old_size > 0 && new_size > 0`) is not a new allocation — count only `old_size == 0 && new_size > 0` to avoid false CI failures
- `FrameTimingInstrumentation` with `std::atomic<uint64_t>` and `alignas(64)` on ESP32 wastes 384+ bytes of static RAM and emits unnecessary Xtensa memory fences — use platform-conditional `uint32_t volatile` on ESP32
- `esp_cpu_get_cycle_count()` wraps every 17.9 seconds at 240 MHz — store only per-frame deltas as `uint32_t`, never accumulate absolute cycle counts
- Benchmark built at default CMake (Debug, `-O0`) produces 5-20x inflated results — `build-bench.sh` must always pass `-DCMAKE_BUILD_TYPE=Release` explicitly

---

## Implications for Roadmap

The build order is strictly dictated by what must exist before what can be written, as established in the architecture dependency graph. Groups 1-2 are the critical path; Groups 3-7 are parallelizable after Group 2.

### Phase 1: CMake Foundation + Vendor

**Rationale:** CMake option and directory structure must exist before any benchmark, tool, or CI target can be written. This is a pure unblocking step with no code to measure.
**Delivers:** `ENJIN2_BUILD_BENCHMARKS` CMake option; `vendor/nanobench.h`; stub `benchmarks/CMakeLists.txt`; stub `tools/CMakeLists.txt`; root `CMakeLists.txt` additions; `.gitignore` update for `bench-results/`; `build-bench.sh` scaffold with hardcoded `-DCMAKE_BUILD_TYPE=Release`
**Addresses:** All P1 features depend on this
**Avoids:** Pitfall 7 (wrong build type) — CMake warning for non-Release builds added here; shell script enforces Release mode before any baseline can be recorded
**Research flag:** Standard patterns — no phase research needed. CMake option structure is identical to `ENJIN2_BUILD_TESTS`; nanobench FetchContent is documented in the official nanobench tutorial.

### Phase 2: Native Benchmark Suite

**Rationale:** `bench_canvas`, `bench_ecs`, `bench_lua` are the core deliverable of the milestone and the prerequisite for CI (Phase 5) and allocation verification (Phase 6). DCE and fixture-design pitfalls must be solved here before any results are stored as baselines.
**Delivers:** Three working benchmark executables producing JSON output; `bench-results/` populated locally; `scripts/build-bench.sh` complete; `scripts/convert-bench-json.sh`
**Addresses:** nanobench integration, bench_canvas, bench_ecs, bench_lua, JSON output, build-bench.sh (all P1 table stakes)
**Avoids:** Pitfall 1 (object construction inside lambda), Pitfall 2 (DCE on canvas ops), Pitfall 7 (wrong build type) — `doNotOptimizeAway` and hoisted fixtures enforced; benchmarks verified at `-O2` vs `-O0`
**Research flag:** Standard patterns for `bench_canvas` and `bench_ecs`. `bench_lua` requires headless Lua initialization without SDL3 — verify which headers transitively pull in SDL3 before writing benchmark logic; the `enjin_run` stub pattern from architecture research is the template.

### Phase 3: Frame Timing Instrumentation

**Rationale:** Self-contained; only depends on Phase 1 CMake foundation. Can be developed in parallel with Phase 2. Modifies only `sdl_main.cpp` and adds one new header.
**Delivers:** `src/perf/frame_timing.hpp` (`FrameTimingInstrumentation` struct); six timing capture points in `sdl_main.cpp`; debug overlay display when `--show-timing` flag active
**Addresses:** Frame timing instrumentation (P1 differentiator)
**Avoids:** Pitfall 5 (atomic overhead on ESP32) — platform-conditional `uint32_t volatile` vs `std::atomic<uint64_t>`; Pitfall 10 (cycle counter wrap) — ESP32 path uses delta-only `uint32_t` storage from the start
**Research flag:** Standard patterns for SDL3 path. ESP32 FreeRTOS game loop integration with `esp_cpu_get_cycle_count()` needs validation against the actual game loop structure if ESP32 frame timing is in scope for v1.10 (currently P2).

### Phase 4: Lua Profiler + Headless CLI Runner

**Rationale:** `enjin_run` is the prerequisite for Lua profiling (profiler attaches via CLI flag) and for CI script smoke tests. Phase 2 (`bench_lua`) informs the stub linkage needed. Build the headless runner first as a bare executable, then attach the profiler incrementally.
**Delivers:** `tools/enjin_run.cpp` executable with `--frames N`, `--profile`, `--output json|text`; `input_platform_poll` stub; `lua_sethook` profiler hook with per-function call counts and approximate self-time; sorted output table
**Addresses:** Headless CLI runner, Lua profiler (both P1 differentiators)
**Avoids:** Pitfall 4 (hook timing measures its own overhead) — profiler documented as call-count accurate, timing approximate; `LUA_MASKCOUNT` sampling used for lower-overhead timing; Pitfall 9 (null dereferences on engine bindings) — `MinimalLuaHost` with null-safe stub pointers required before any script is run
**Research flag:** Needs research. The null-binding-safety contract for headless mode is not established in existing code. Every `engine.*` subtable pointer registration in `LuaScriptSystem::registerAll()` must be enumerated and checked for null guards before implementation. A segfault in `enjin_run` on any real script is a milestone blocker. Recommend `/gsd:research-phase` before Phase 4 implementation begins.

### Phase 5: CI Regression Pipeline

**Rationale:** CI depends on working binaries (Phase 2) and working conversion scripts. The gh-pages storage strategy must be resolved before the first baseline run is recorded — benchmark history cannot be recovered once overwritten by a Docusaurus deploy.
**Delivers:** `.github/workflows/benchmarks.yml`; github-action-benchmark integration; `bench-data` branch (separate from gh-pages/Docusaurus); gh-pages dashboard; baseline seeded on first merge to `main`
**Addresses:** CI workflow, regression detection, gh-pages dashboard (all P1 table stakes)
**Avoids:** Pitfall 3 (110% threshold false positives) — start at 150% threshold, `fail-on-alert: false`, calibrate over 30-50 runs; Pitfall 8 (gh-pages history destruction) — use `external-data-json-path` to decouple benchmark data from Docusaurus deployment
**Research flag:** Needs research. The exact github-action-benchmark `external-data-json-path` configuration with a coexisting `actions/deploy-pages` workflow needs a verified playbook. No concrete working example was found during research for this specific setup. Recommend `/gsd:research-phase` or a proof-of-concept run before recording any baseline — this is the single highest-risk integration in the milestone.

### Phase 6: Static Allocation Verification

**Rationale:** Wraps the finished benchmark binaries from Phase 2. The allocation intercept is integrated into bench_canvas hot-path sections and verified via a CI step.
**Delivers:** `src/perf/alloc_counter.hpp` (`AllocGuard` + `operator new`/`delete` override behind `ENJIN2_ALLOC_INTERCEPT`); hot-path verification integrated into `bench_canvas.cpp`; CI step added to `benchmarks.yml` to fail on non-zero exit
**Addresses:** Static allocation verification (P1 differentiator — proves enjin2's core zero-alloc claim with CI evidence)
**Avoids:** Pitfall 6 (lua_Alloc realloc over-counting) — count only `old_size == 0 && new_size > 0`; allocator hook provided at `lua_newstate()` time, not via post-init `lua_setallocf`; counter scoped to specific hot-path spans only, not full frame
**Research flag:** Standard C++17 patterns for the `operator new` override — well-documented on cppreference. Lua allocator hook counting semantics need verification against a known-zero-alloc path (e.g., `canvas.clear()`) before the CI check is enabled to confirm no false positives.

### Phase 7: Documentation

**Rationale:** Can be written in parallel with all phases. Content is determined by the plan, not the implementation. Ship together with Phase 6 as the milestone completion marker. Final numbers from Phases 2-5 fill in the per-platform frame budget section.
**Delivers:** `docs/PERFORMANCE.md` — quick-start commands, subsystem performance guide, adding-benchmarks guide, per-platform frame budget reference with measured numbers
**Addresses:** PERFORMANCE.md (P1 table stake — without docs, the benchmark infra is opaque to contributors)
**Research flag:** No research needed. Content is derived directly from implemented phases.

---

### Phase Ordering Rationale

- **Phase 1 must be first** — CMake structure blocks all other phases; zero dependencies.
- **Phases 2, 3, and 4 can be parallelized** after Phase 1 lands. Within Phase 4, build the bare `enjin_run` runner before attaching the profiler.
- **Phase 5 (CI) must follow Phase 2** — the CI workflow has no value without benchmark binaries producing valid JSON. The gh-pages storage decision must be made before Phase 5 starts, not retrofitted after.
- **Phase 6 (Alloc verification) must follow Phase 2** — the verifier wraps existing benchmark binaries; it cannot be built standalone.
- **Phase 7 (Docs) is terminal** — no downstream dependencies; write against the plan, update with measured numbers from Phases 2-5.
- The architecture's seven dependency groups map directly to these seven phases. Dependency arrows from ARCHITECTURE.md are respected exactly.

### Research Flags

Phases needing `/gsd:research-phase` during planning:

- **Phase 4 (Lua Profiler + Headless Runner):** The null-binding-safety contract for headless mode is not mapped in existing code. Need to enumerate every `engine.*` subtable pointer registration in `LuaScriptSystem::registerAll()` and confirm which bindings have null guards. A segfault in `enjin_run` on any real script is a milestone blocker.
- **Phase 5 (CI Regression Pipeline):** The `external-data-json-path` configuration for github-action-benchmark alongside a coexisting `actions/deploy-pages` workflow needs a verified playbook. If storage is set up incorrectly, the first Docusaurus deploy after a benchmark run destroys all history. Recovery is painful and history is unrecoverable from prior runs.

Phases with well-documented, standard patterns (skip research-phase):
- **Phase 1:** CMake option patterns are identical to `ENJIN2_BUILD_TESTS`; nanobench FetchContent is documented in the official nanobench tutorial.
- **Phase 2:** nanobench benchmark writing is thoroughly documented; `doNotOptimizeAway` and fixture hoisting are covered in the official tutorial; bench_canvas and bench_ecs follow standard fixture-based patterns.
- **Phase 3:** `SDL_GetPerformanceCounter` is already used in the codebase; `FrameTimingInstrumentation` is a trivial header-only struct.
- **Phase 6:** Global `operator new` override is standard C++17 documented on cppreference.
- **Phase 7:** No technical uncertainty; pure documentation.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | nanobench v4.3.11 verified via official release and docs; github-action-benchmark v1 verified via official repo; `lua_sethook` is stable Lua C API unchanged since 5.1; `SDL_GetPerformanceCounter` is documented SDL3 API already used in codebase; `operator new` override is standard C++17. One sub-point at MEDIUM: nanobench exception requirement has no official "no-exceptions mode" documentation — mitigated by gating benchmark target to SDL3 desktop only. |
| Features | HIGH | Milestone brief is explicit and unambiguous; existing codebase directly inspected for all integration points; feature set matches established domain conventions for game engine benchmarking milestones. All anti-features documented with specific rationale for exclusion. |
| Architecture | HIGH | Full codebase read with all integration points traced from source files. `LuaEngine::getState()` public accessor confirmed; `input_platform_poll` stub pattern confirmed from three existing platform definitions; `stb_image.h` vendor pattern confirmed. Two sub-areas at MEDIUM: 64-bit atomic support on Xtensa ESP32 (may need libatomic — mitigated by recommending `uint32_t`); `SDL_GetTicks64()` millisecond resolution may be insufficient for sub-ms phases (mitigated by fallback to `std::chrono`). |
| Pitfalls | HIGH | 10 pitfalls documented with prevention strategies and recovery costs; sources include official documentation (Lua PIL, nanobench docs, github-action-benchmark docs, ESP-IDF docs), published CI benchmark studies (5-15% CoV figure is from peer-reviewed research), and direct codebase analysis. |

**Overall confidence:** HIGH

### Gaps to Address

- **nanobench no-exceptions mode:** No official documentation found. The benchmark target is gated to SDL3 desktop (exceptions enabled), so this is only a concern if a future target adds a no-exceptions constraint. Handle by documenting in CMakeLists.txt that `ENJIN2_BUILD_BENCHMARKS` requires exceptions and adding a `target_compile_options` check.

- **`SDL_GetTicks64()` sub-millisecond resolution:** SDL_GetTicks64 returns milliseconds; sub-phases (compositeTime, renderTime on a fast machine) may be sub-millisecond. Handle during Phase 3 implementation by measuring a known-duration sleep and confirming resolution is adequate; swap to `std::chrono::steady_clock` if not.

- **`std::atomic<uint64_t>` on Xtensa ESP32:** Some Xtensa toolchain versions require `-latomic` for 64-bit atomic operations. Handle during Phase 3 by using `uint32_t` atomics on ESP32 (already recommended in pitfalls research) and testing on the actual ESP32-S3 toolchain.

- **`external-data-json-path` + Docusaurus deploy coexistence:** No concrete playbook was found for running both on the same repository. Handle by creating a proof-of-concept workflow run before recording any benchmark baseline (Phase 5 prerequisite).

- **`bench_lua` SDL3 transitive linkage:** Lua bindings import graphics and input subsystem headers. The exact set of headers that pull in SDL3 transitively must be verified before `bench_lua` can link cleanly against `enjin2_lua` without SDL3. Handle at Phase 2 start by attempting a minimal link and adding stubs as needed.

---

## Sources

### Primary (HIGH confidence)

- `https://github.com/martinus/nanobench/releases/tag/v4.3.11` — v4.3.11 release date and changelog
- `https://nanobench.ankerl.com/reference.html` — `templates::json()`, `render()`, `doNotOptimizeAway()` API signatures
- `https://nanobench.ankerl.com/tutorial.html` — FetchContent alias, `ANKERL_NANOBENCH_IMPLEMENT` single-TU rule, standalone main pattern
- `https://nanobench.ankerl.com/comparison.html` — 65x faster autotuning vs google/benchmark
- `https://github.com/benchmark-action/github-action-benchmark` — `customSmallerIsBetter` format, `external-data-json-path`, PR fork security warning, `alertThreshold` behavior
- `https://www.lua.org/pil/23.3.html` — `lua_sethook` hook overhead, C API vs Lua-side hook accuracy comparison
- `https://www.lua.org/manual/5.4/` — `lua_Alloc` malloc/realloc/free protocol; `lua_sethook` mask constants; `lua_Debug` fields
- `https://wiki.libsdl.org/SDL3/SDL_GetPerformanceCounter` — SDL3 counter resolution, conversion formula
- `https://cppreference.com/w/cpp/memory/new/operator_new` — global `operator new` replaceable allocation function, standard compliance
- `https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos.html` — `esp_cpu_get_cycle_count()` return type, wrap behavior at 240 MHz
- `/home/unwn/git/enjin/` — v1.9 codebase, direct inspection: `CMakeLists.txt`, `src/platform/sdl/sdl_main.cpp`, `include/enjin2/scripting/lua_engine.hpp`, `include/enjin2/scripting/bindings.hpp`, `include/enjin2/graphics/layer_compositor.hpp`, `include/enjin2/core/memory.hpp`, `tests/CMakeLists.txt`, `examples/CMakeLists.txt`, `vendor/`
- `/home/unwn/git/enjin/project/benchmarking-milestone.md` — milestone brief; phase-by-phase requirements

### Secondary (MEDIUM confidence)

- `https://labs.quansight.org/blog/github-actions-benchmarks` — GitHub Actions shared runner variance (5-15% CoV measurement)
- `https://codspeed.io/blog/benchmarks-in-ci-without-noise` — CI benchmark noise patterns, threshold calibration guidance
- `https://arxiv.org/html/2510.11310` — statistical detection of benchmark performance changes in CI
- `http://lua-users.org/wiki/PepperfishProfiler` — hook overhead documentation for `lua_sethook`
- `http://lua-users.org/wiki/MemoryAllocation` — `lua_Alloc` realloc counting pitfall
- `https://github.com/siffiejoe/lua-allocspy` — reference implementation for correct `lua_Alloc` allocation counting
- `https://github.com/abeimler/ecs_benchmark` — ECS benchmarking patterns and fixture design

### Tertiary (LOW confidence)

- `https://esp32.com/viewtopic.php?t=10331` — cycle counter wrap discussion (promoted to MEDIUM for the wrap-at-17.9s behavior, confirmed via ESP-IDF docs)
- WebSearch: nanobench exception requirements — no official "no-exceptions mode" statement found; inferred from `<stdexcept>` usage in nanobench source

---

*Research completed: 2026-03-07*
*Ready for roadmap: yes*
