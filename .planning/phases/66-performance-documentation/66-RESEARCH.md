# Phase 66: Performance Documentation - Research

**Researched:** 2026-03-08
**Domain:** Technical documentation authoring — covering benchmark suite, CI pipeline, frame timing, Lua profiler, and allocation verification subsystems
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DOC-01 | docs/PERFORMANCE.md covers all 5 subsystems with how-to-first structure | All five subsystems fully implemented and verified in Phases 60-65; all source files, headers, and outputs are available to document |
| DOC-02 | Quick start with scripts/build-bench.sh one-liner and adding-new-benchmarks guide | build-bench.sh is complete and tested; bench_canvas.cpp and bench_ecs.cpp provide canonical template patterns for adding new benchmarks |
| DOC-03 | Per-platform frame budget reference (ESP32 vs WASM vs SDL3) with actual measured values from Phase 62 results | SDL3 runner defaults to 30 fps (33.3ms budget); bench-results/ JSON files contain actual measured ns/op values; ESP32 and WASM frame budgets must be derived from the architecture and platform documentation |
</phase_requirements>

---

## Summary

Phase 66 produces a single file: `docs/PERFORMANCE.md`. This is a pure documentation phase — no new code is written. The entire content is derivable from already-implemented and already-tested artifacts from Phases 60-65. The research task is to fully inventory those artifacts so the planner knows exactly what to document, what numbers to use, and which patterns to explain.

The five subsystems to cover are: (1) Benchmark Suite (nanobench, bench_canvas/ecs/lua/alloc binaries, scripts/build-bench.sh), (2) CI Pipeline (GitHub Actions benchmarks.yml, bench-data branch, regression thresholds), (3) Frame Timing (FrameTimingInstrumentation header, --show-timing SDL overlay, polling API), (4) Lua Profiler (LuaProfiler singleton, enjin_run headless runner, --profile --frames flags), and (5) Allocation Verification (AllocGuard RAII header, bench_alloc binary, CI enforcement step).

The doc structure must be how-to-first: every subsystem starts with a runnable command or copy-pasteable code, then explains the architecture. The "Adding New Benchmarks" guide is a required walkthrough. Per-platform frame budget numbers are required for ESP32, WASM, and SDL3, and must use measured values from bench-results/ where available and reasonable engineering estimates for platforms not benchmarked in CI.

**Primary recommendation:** Write `docs/PERFORMANCE.md` as a single flat Markdown file. Do not use Docusaurus front matter or sidebar config unless explicitly asked — the file is used standalone and may be linked from the README. Use a how-to-first structure: quick-start command block first, then subsystem reference sections, then the frame budget table, then the "Adding New Benchmarks" walkthrough.

---

## Inventory of Artifacts to Document

### File Map (all files exist)

| File | What It Does | How to Reference |
|------|-------------|-----------------|
| `scripts/build-bench.sh` | Builds and runs all four benchmark binaries in one command | Quick-start one-liner |
| `benchmarks/bench_canvas.cpp` | Canvas4/Canvas8/LayerCompositor pixel op benchmarks | Template for new benchmark |
| `benchmarks/bench_ecs.cpp` | Object/Scene/Component ECS benchmarks | Template for new benchmark |
| `benchmarks/bench_lua.cpp` | LuaEngine headless benchmarks | Template for new benchmark |
| `benchmarks/bench_alloc.cpp` | Zero-allocation hot-path verification binary | Explains AllocGuard usage |
| `benchmarks/CMakeLists.txt` | Adds all benchmark targets with nanobench_vendor INTERFACE | Explains how to register a new target |
| `scripts/convert-bench.py` | Converts nanobench JSON to customSmallerIsBetter format for CI | Explains CI JSON pipeline |
| `.github/workflows/benchmarks.yml` | CI workflow for benchmark history and regression detection | CI pipeline reference |
| `vendor/nanobench.h` | nanobench 4.3.11 vendored single header | Explains zero-dep setup |
| `include/enjin2/instrumentation/frame_timing.hpp` | FrameTimingInstrumentation singleton with dual compile-path | Frame timing reference |
| `include/enjin2/instrumentation/alloc_guard.hpp` | AllocGuard RAII class with dual compile-path | Allocation verification reference |
| `include/enjin2/scripting/lua_profiler.hpp` | LuaProfiler hook-based call-count profiler | Lua profiling reference |
| `src/platform/headless/headless_main.cpp` | enjin_run headless CLI runner | Lua profiling usage |
| `bench-results/bench_canvas.json` | Actual measured bench_canvas results | Source of SDL3 benchmark numbers |
| `bench-results/bench_ecs.json` | Actual measured bench_ecs results | Source of SDL3 benchmark numbers |
| `bench-results/bench_lua.json` | Actual measured bench_lua results | Source of SDL3 benchmark numbers |

---

## Actual Measured Benchmark Values (SDL3 Desktop, x86-64 Linux)

These are the real numbers from bench-results/ JSON files. All are `median(elapsed)` values.

### Canvas Benchmarks (bench_canvas)
| Operation | Median (ns) | Note |
|-----------|-------------|------|
| canvas4: setPixel | 44 ns | Canvas4<128,128> at center |
| canvas4: clear | 51 ns | Full 128x128 clear |
| canvas4: fillRect 32x32 | 25 ns | 32x32 region fill |
| canvas4: drawCircle r16 | 177 ns | Bresenham circle r=16 |
| canvas4: blit 128x128 sprite | 40,950 ns (~41 µs) | Same-size blit, all pixels |
| canvas8: setPixel | 15 ns | Canvas8<128,128> at center |
| canvas8: fillRect 32x32 | 515 ns | 32x32 region fill |
| compositor: composite 5 layers | 3,375 ns (~3.4 µs) | clearAll + fillRect layer0 + composite |

### ECS Benchmarks (bench_ecs)
| Operation | Median (ns) | Note |
|-----------|-------------|------|
| scene::addObject x1 | 181 ns | ObjectCollection::addObject (includes alloc) |
| scene::addObject x8 | 447 ns | |
| scene::addObject x16 | 733 ns | |
| scene::addObject x32 | 1,881 ns | |
| scene::addObject x48 | 2,927 ns | |
| object::addComponent<C_Position> | 56 ns | |
| object::removeComponent<C_Position> | 58 ns | |
| scene::update x1 objects | 18 ns | Zero-alloc hot path |
| scene::update x8 objects | 43 ns | Zero-alloc hot path |
| scene::update x16 objects | 73 ns | Zero-alloc hot path |
| scene::update x32 objects | 124 ns | Zero-alloc hot path |
| scene::update x48 objects | 172 ns | Zero-alloc hot path |

### Lua Benchmarks (bench_lua)
| Operation | Median (ns) | Note |
|-----------|-------------|------|
| lua engine: init+shutdown | 75,055 ns (~75 µs) | Full lua_newstate + lua_close cycle |
| lua engine: executeString (noop script) | 1,126 ns | luaL_loadstring + lua_pcall |
| lua binding: engine.time.delta call | 1,005 ns | executeString path |
| lua binding: math.clamp call | 1,503 ns | executeString path |
| lua proxy: find+field round-trip | 1,514 ns | engine.scene.find + .name field |
| lua event: emit dispatch | 947 ns | engine.event.emit |
| lua GC: full collect | 2,240 ns | lua_gc(LUA_GCCOLLECT) |

---

## Per-Platform Frame Budget Analysis (DOC-03)

### SDL3 Desktop
- Default FPS: 30 (DEFAULT_FPS = 30 in sdl_main.cpp line 35)
- Frame budget at 30 fps: **33.3 ms (33,333 µs)**
- Frame budget at 60 fps (--fps 60): **16.6 ms (16,667 µs)**
- Benchmark context: x86-64 Linux desktop. All bench-results/ numbers are from this platform.
- Typical hot path: scene::update x48 = 172 ns, compositor = 3.4 µs, Lua event emit = 947 ns
- All measured values are WELL within 33.3ms budget.

### WASM (Emscripten/Browser)
- Not benchmarked in CI (per REQUIREMENTS.md Out of Scope: "Benchmark on WASM/ESP32 targets in CI")
- Frame budget: **16.6 ms (16,667 µs)** at 60 fps (browser requestAnimationFrame target)
- Frame timing instrumentation: ENJIN2_FRAME_TIMING is NOT defined for WASM builds — zero-overhead stub path in frame_timing.hpp. WASM uses the plain uint32_t stub.
- WASM performance relative to SDL3: generally slower due to JS/WASM boundary and wasm32 constraints. No measured numbers available for WASM.
- Documentation should state: "WASM benchmarks are not run in CI. Use the SDL3 desktop numbers as a conservative upper bound — WASM hot paths may be 2-5x slower depending on Emscripten optimization level."

### ESP32-S3 (FreeRTOS)
- Not benchmarked in CI (per REQUIREMENTS.md Out of Scope)
- Frame budget: depends on target FPS. The esp32_lcd_demo uses `vTaskDelay(1)` (yield) and tracks actual FPS via esp_timer_get_time(). No fixed FPS cap — the loop runs as fast as the hardware allows.
- The esp32_lcd_demo targets ~60 fps based on the dt clamp `if (dt <= 0.0f || dt > 0.1f) { dt = 1.0f / 60.0f; }` guard.
- Frame budget at 60 fps: **16.6 ms (16,667 µs)**
- ESP32-S3 LX7 dual-core at 240 MHz. Canvas4<320,240> composite+expand+DMA with strip pipelining.
- FrameTimingInstrumentation stub: ENJIN2_FRAME_TIMING NOT defined for ESP32 — plain uint32_t zero-overhead stub.
- Documentation should state: "ESP32-S3 benchmarks are not run in CI. Measured FPS from the esp32_lcd_demo with strip-pipelined DMA (40-line strips, 80MHz SPI) typically achieves 30-60 fps depending on Lua script complexity. Use enjin_run with --profile to measure Lua hot paths on desktop as a proxy."

---

## How-to-First Documentation Structure

### Quick Start (copy-paste one-liner)

```bash
bash scripts/build-bench.sh
```

Prerequisites: cmake, make/ninja, liblua5.4-dev (or equivalent). This builds and runs all four benchmark binaries and writes results to `bench-results/`.

### enjin_run Lua Profiler Quick Start

```bash
# Build headless runner (requires ENJIN2_BUILD_HEADLESS=ON)
cmake -DENJIN2_BUILD_LUA=ON -DENJIN2_BUILD_HEADLESS=ON -DCMAKE_BUILD_TYPE=Release -B build-headless .
cmake --build build-headless --target enjin_run

# Profile a script for 100 frames
./build-headless/src/platform/headless/enjin_run --profile --frames 100 scripts/my_game.lua

# JSON output
./build-headless/src/platform/headless/enjin_run --profile --output json --frames 100 scripts/my_game.lua
```

### Frame Timing Quick Start (SDL3 runner)

```bash
# Build SDL3 runner (ENJIN2_FRAME_TIMING=1 is injected automatically for enjin2_sdl target)
cmake -DENJIN2_BUILD_SDL=ON -DENJIN2_BUILD_LUA=ON -DCMAKE_BUILD_TYPE=Release -B build .
cmake --build build --target enjin2_sdl

# Run with frame timing overlay visible on screen (top-left corner)
./build/src/platform/sdl/enjin2_sdl --show-timing --script scripts/layer_demo.lua
```

---

## Adding New Benchmarks — Step-by-Step

The planner must produce a clear walkthrough. These are the exact steps:

### Step 1: Create the benchmark source file

Copy pattern from `benchmarks/bench_canvas.cpp`:

```cpp
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <enjin2/your/header.hpp>
#include <fstream>
#include <sys/stat.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("my_feature").warmup(10).epochs(100).epochIterations(1);

    bench.run("my_feature: operation name", [&] {
        // ... exercise the hot path ...
        ankerl::nanobench::doNotOptimizeAway(result);
    });

    // Write JSON results
    mkdir("bench-results", 0755);
    std::ofstream out("bench-results/bench_my_feature.json");
    ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out);
    return 0;
}
```

Key rules:
- `#define ANKERL_NANOBENCH_IMPLEMENT` only once per binary — each benchmark binary is a separate executable.
- Always call `doNotOptimizeAway()` on the result to prevent dead-code elimination.
- Write JSON to `bench-results/bench_NAME.json`.
- Use `mkdir("bench-results", 0755)` before the `std::ofstream` to ensure the directory exists.

### Step 2: Register the CMake target

Add to `benchmarks/CMakeLists.txt`:

```cmake
add_executable(bench_my_feature bench_my_feature.cpp)
target_include_directories(bench_my_feature PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bench_my_feature PRIVATE
    nanobench_vendor
    enjin2_core
    # add other enjin2_* targets as needed
)
```

If the benchmark requires Lua, guard with `if(TARGET enjin2_lua)` and add `${LUA_INCLUDE_DIRS}` to `target_include_directories`.

### Step 3: Add to build-bench.sh

In `scripts/build-bench.sh`, add the new target to the `cmake --build` line and add a run step:

```bash
cmake --build "${BUILD_DIR}" --target bench_canvas bench_ecs bench_lua bench_alloc bench_my_feature -- -j"$(nproc)"
# ...
echo ""
echo "--- running bench_my_feature ---"
"${BUILD_DIR}/benchmarks/bench_my_feature"
```

### Step 4: Add to convert-bench.py

In `scripts/convert-bench.py`, add `'bench_my_feature.json'` to the `INPUT_FILES` list. The conversion logic is generic — no other changes needed.

### Step 5: Verify in CI

The CI workflow (`.github/workflows/benchmarks.yml`) uses `bash scripts/build-bench.sh` and `python3 scripts/convert-bench.py` — both will pick up the new benchmark automatically after Steps 3-4.

---

## Architecture Patterns

### Dual-Path Compile Guard Pattern

Both `frame_timing.hpp` and `alloc_guard.hpp` use the same pattern:

```cpp
#ifdef ENJIN2_FEATURE_NAME
// Full implementation with heavy includes
#else
// Zero-overhead no-op stub
#endif
```

CMake injects the define only for targets that need it. WASM and ESP32 targets never get the define.

### nanobench INTERFACE Target Pattern

`nanobench_vendor` is an INTERFACE library (no compilation) exposing the single-header path:

```cmake
add_library(nanobench_vendor INTERFACE)
target_include_directories(nanobench_vendor INTERFACE ${CMAKE_SOURCE_DIR}/vendor)
```

This means every benchmark binary that links `nanobench_vendor` gets the header include path automatically. No library compilation step.

### AllocGuard Pattern (bench_alloc.cpp)

The operator new override MUST come before any includes that could call new. Structure:

1. `#define ENJIN2_ALLOC_VERIFICATION 1`
2. Define `thread_local int g_alloc_guard_depth` and `thread_local long g_alloc_count` (NOT static — alloc_guard.hpp declares them extern)
3. Override all six operator new/delete forms
4. `#include <enjin2/instrumentation/alloc_guard.hpp>`
5. Engine headers
6. Setup section (allocations allowed, reset `g_alloc_count = 0` at end)
7. Hot-path section: each `{ AllocGuard g("label"); ... }` scope

### CI Two-Conditional-Step Pattern

The benchmarks.yml uses two conditional steps for the same action:

- Push to main / workflow_dispatch: store history on bench-data branch (`auto-push: true`, `save-data-file: true` implied)
- Pull request: regression check only (`auto-push: false`, `save-data-file: false`, `fail-on-alert: true`)

---

## Common Pitfalls (for Documentation Authors)

### Pitfall 1: ANKERL_NANOBENCH_IMPLEMENT in Multiple TUs
**What goes wrong:** Linker multiple-definition errors.
**Why:** Each binary defines the nanobench implementation. If two `.cpp` files in the same binary both define it, the linker sees duplicate symbols.
**How to avoid:** Exactly one `.cpp` file per benchmark binary defines `ANKERL_NANOBENCH_IMPLEMENT`. Each benchmark binary is a separate CMake executable.

### Pitfall 2: executeString Inside AllocGuard
**What goes wrong:** AllocGuard trips immediately because `luaL_loadstring` allocates for chunk parsing.
**Why:** `eng.executeString("...")` calls `luaL_loadstring` internally, which allocates a string object.
**How to avoid:** Pre-register functions with `luaL_ref` + `lua_rawgeti` + `lua_call` for zero-alloc Lua calling inside AllocGuard. See bench_alloc.cpp `deltaRef` pattern.

### Pitfall 3: Confusing bench-data Branch With gh-pages
**What goes wrong:** Docusaurus deploy wipes benchmark history.
**Why:** `docs.yml` uses `actions/deploy-pages` which replaces all content on gh-pages. If benchmark history were on gh-pages, every docs deploy would destroy it.
**How to avoid:** Benchmark history lives on the `bench-data` branch (orphan, separate from gh-pages). This is a deliberate architectural decision.

### Pitfall 4: Documenting Platform Numbers That Were Never Measured
**What goes wrong:** Documentation states specific ns numbers for ESP32 or WASM — numbers that were never actually measured.
**How to avoid:** Be explicit about what is measured (SDL3 desktop CI) vs what is a frame budget target (ESP32, WASM). Use "estimated" or "target" language for non-CI platforms. The bench-results/ JSON files are the ground truth for SDL3.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON output from benchmarks | Custom JSON serializer | `ankerl::nanobench::render(ankerl::nanobench::templates::json(), bench, out)` | nanobench 4.3.11 includes a built-in JSON template; already used by all three benchmark binaries |
| Benchmark result format conversion | Custom converter | `scripts/convert-bench.py` | Already exists, handles all three inputs, exits non-zero on missing files |
| Building benchmark suite | Manual cmake commands | `bash scripts/build-bench.sh` | Already exists, handles build dir, Lua flag, Release mode, all four targets |

---

## Document Content Checklist (DOC-01, DOC-02, DOC-03)

DOC-01 requires all five subsystems covered:
- [x] Benchmark suite (nanobench, bench_canvas/ecs/lua, bench_alloc, build-bench.sh)
- [x] CI pipeline (benchmarks.yml, bench-data branch, 150% threshold, convert-bench.py)
- [x] Frame timing (frame_timing.hpp, --show-timing, polling API, compilation flags)
- [x] Lua profiler (lua_profiler.hpp, enjin_run CLI, --profile, GC pressure, output formats)
- [x] Allocation verification (alloc_guard.hpp, bench_alloc, CI step, operator new pattern)

DOC-02 requires:
- [x] Quick-start with `bash scripts/build-bench.sh` one-liner
- [x] Adding new benchmarks walkthrough (5 steps as documented above)

DOC-03 requires:
- [x] SDL3 frame budget with measured values from Phase 62 results
- [x] WASM frame budget (stated as not measured in CI, target 16.6ms at 60fps)
- [x] ESP32 frame budget (stated as not measured in CI, target 16.6ms at 60fps, strip DMA architecture note)

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Ad-hoc chrono benchmarks in examples/ | nanobench 4.3.11 formal benchmark suite | Phase 61 | Reproducible, CI-integrated, JSON output |
| No CI performance tracking | github-action-benchmark on bench-data branch | Phase 64 | Regression detection on every PR |
| No allocation guarantees | AllocGuard + bench_alloc CI step | Phase 65 | Machine-verifiable zero-alloc hot paths |
| No frame timing visibility | FrameTimingInstrumentation + --show-timing overlay | Phase 62 | Per-phase microsecond breakdown in SDL3 runner |
| No headless Lua profiling | enjin_run --profile --frames N | Phase 63 | Per-function call count without SDL dependency |

**Deprecated/outdated:**
- Raw `std::chrono` benchmarks in `examples/`: superseded by bench_canvas/ecs/lua binaries. The examples/ benchmarks can be removed or left as-is; they are not referenced by CI.

---

## Open Questions

1. **Where does docs/PERFORMANCE.md live in the Docusaurus sidebar?**
   - What we know: The docs/ directory contains a Docusaurus site with `sidebars.js`. The docs do not yet have a performance section.
   - What's unclear: Whether the planner should add the file to Docusaurus sidebars or leave it as a standalone root-level doc.
   - Recommendation: Write the file to `docs/PERFORMANCE.md` as required by DOC-01. Adding a Docusaurus sidebar entry is a separate concern — not part of DOC-01/DOC-02/DOC-03. The planner should add the file to `docs/PERFORMANCE.md` and note that Docusaurus sidebar integration is optional/out-of-scope for this phase.

2. **Should docs/PERFORMANCE.md use Docusaurus front matter?**
   - What we know: Other docs in `docs/src/` use Markdown with no front matter (e.g., `intro.md`). The requirement says `docs/PERFORMANCE.md` — placing it in the root `docs/` directory (not `docs/src/`) means it won't auto-render in Docusaurus.
   - Recommendation: Place the file at the repo root `docs/PERFORMANCE.md` (not `docs/src/`) so it is accessible as a standalone document. This matches the requirement text exactly. No Docusaurus front matter needed.

3. **Should the planner add a README link to docs/PERFORMANCE.md?**
   - What we know: README.md currently links to Docusaurus hosted docs for Getting Started, API Reference, and Architecture.
   - Recommendation: Yes — adding a one-liner link to `docs/PERFORMANCE.md` in the README is a natural inclusion and takes one task line. The planner should include this as a task.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | No automated test for a documentation file — correctness verified by manual check-out of all referenced commands |
| Config file | N/A |
| Quick run command | `bash scripts/build-bench.sh` (verifies the documented quick-start works) |
| Full suite command | `bash scripts/build-bench.sh && cmake --build build --target enjin_run && ./build/src/platform/headless/enjin_run --profile --frames 10 scripts/features_demo.lua` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DOC-01 | docs/PERFORMANCE.md exists with all 5 subsections | manual-only | `test -f docs/PERFORMANCE.md` | Wave 0 creates it |
| DOC-01 | Quick-start one-liner in the doc actually runs | smoke | `bash scripts/build-bench.sh` | Yes — script exists |
| DOC-02 | Adding-new-benchmarks section present and has 5 steps | manual-only | Read docs/PERFORMANCE.md | Wave 0 creates it |
| DOC-03 | Frame budget table with ESP32/WASM/SDL3 rows present | manual-only | Read docs/PERFORMANCE.md | Wave 0 creates it |

### Sampling Rate
- **Per task commit:** `test -f docs/PERFORMANCE.md && bash scripts/build-bench.sh`
- **Per wave merge:** Full build-bench.sh run to confirm all referenced commands remain valid
- **Phase gate:** Manual review of all five subsystems documented, all commands copy-pasteable and functional

### Wave 0 Gaps
- [ ] `docs/PERFORMANCE.md` — the entire output of this phase; no prior version exists

*(No test framework gaps — documentation phase has no automated test requirements beyond command validity)*

---

## Sources

### Primary (HIGH confidence)
- `/home/unwn/git/enjin/scripts/build-bench.sh` — exact quick-start command, all benchmark binary targets
- `/home/unwn/git/enjin/.github/workflows/benchmarks.yml` — CI pipeline structure, trigger conditions, bench-data branch, 150% threshold
- `/home/unwn/git/enjin/scripts/convert-bench.py` — JSON conversion pipeline, input file list
- `/home/unwn/git/enjin/benchmarks/bench_canvas.cpp` — canonical benchmark source pattern
- `/home/unwn/git/enjin/benchmarks/bench_alloc.cpp` — AllocGuard usage, operator new override pattern
- `/home/unwn/git/enjin/benchmarks/CMakeLists.txt` — nanobench_vendor INTERFACE target, benchmark target registration
- `/home/unwn/git/enjin/include/enjin2/instrumentation/frame_timing.hpp` — FrameTimingInstrumentation dual-path header
- `/home/unwn/git/enjin/include/enjin2/instrumentation/alloc_guard.hpp` — AllocGuard dual-path header
- `/home/unwn/git/enjin/include/enjin2/scripting/lua_profiler.hpp` — LuaProfiler singleton, install/uninstall API
- `/home/unwn/git/enjin/src/platform/headless/headless_main.cpp` — enjin_run --profile, --frames, --output json flags
- `/home/unwn/git/enjin/src/platform/sdl/sdl_main.cpp` — DEFAULT_FPS=30, --show-timing arg, FrameTimingInstrumentation measurement sites
- `/home/unwn/git/enjin/bench-results/bench_canvas.json` — actual measured SDL3 desktop ns/op values
- `/home/unwn/git/enjin/bench-results/bench_ecs.json` — actual measured SDL3 desktop ns/op values
- `/home/unwn/git/enjin/bench-results/bench_lua.json` — actual measured SDL3 desktop ns/op values
- `/home/unwn/git/enjin/examples/esp32_lcd_demo/main/main.cpp` — ESP32 frame loop architecture, FPS tracking, 60fps dt guard
- `/home/unwn/git/enjin/vendor/nanobench.h` — nanobench version 4.3.11 confirmed
- `.planning/STATE.md` — key decisions (bench-data branch, 150% threshold, cancel-in-progress:false)
- `.planning/REQUIREMENTS.md` — out-of-scope confirmations (WASM/ESP32 not benchmarked in CI)

### Secondary (MEDIUM confidence)
- `.planning/phases/62-frame-timing-instrumentation/62-01-SUMMARY.md` — confirmed ENJIN2_FRAME_TIMING=1 injected only for enjin2_sdl target; WASM/ESP32 use stub path
- `.planning/phases/65-allocation-verification/65-01-SUMMARY.md` — confirmed AllocGuard patterns, six operator new/delete override forms
- `.planning/phases/64-ci-regression-pipeline/64-01-SUMMARY.md` — confirmed bench-data orphan branch, dual-step CI pattern

### Tertiary (LOW confidence)
- WASM and ESP32 performance estimates: no measured values in bench-results/. Engineering estimates based on platform characteristics (240MHz LX7, wasm32 constraints).

---

## Metadata

**Confidence breakdown:**
- Artifact inventory: HIGH — all files read directly from the repo
- Measured benchmark numbers: HIGH — read directly from bench-results/ JSON files
- Architecture patterns for doc: HIGH — confirmed from implementation summaries
- Platform frame budgets (SDL3): HIGH — DEFAULT_FPS=30 confirmed in sdl_main.cpp
- Platform frame budgets (ESP32/WASM): MEDIUM — derived from architecture and out-of-scope statements; no direct measurement

**Research date:** 2026-03-08
**Valid until:** 2026-06-08 — all documented artifacts are stable; bench-results/ numbers may shift with new CI runs but the structure will not change
