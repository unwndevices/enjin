# Performance Guide

enjin2 is designed with a zero-dynamic-allocation constraint on hot paths: canvas pixel ops, component updates, and Lua binding calls must not allocate. This document covers the complete benchmarking and profiling infrastructure — how to run benchmarks, how to profile Lua scripts, how to verify allocation guarantees, and how to add new benchmarks to the suite.

## Quick Start

Run all benchmarks with one command:

```bash
bash scripts/build-bench.sh
```

Prerequisites: `cmake`, `make` or `ninja`, `liblua5.4-dev` (or equivalent).

This command builds and runs four benchmark binaries — `bench_canvas`, `bench_ecs`, `bench_lua`, and `bench_alloc` — and writes JSON results to `bench-results/`.

## Benchmark Suite

The benchmark suite uses [nanobench 4.3.11](https://github.com/martinus/nanobench) (vendored at `vendor/nanobench.h`). Each benchmark binary is a standalone executable that writes results to `bench-results/bench_NAME.json`.

### Benchmark Binaries

| Binary | What It Benchmarks |
|--------|--------------------|
| `bench_canvas` | Canvas4/Canvas8 pixel ops: setPixel, clear, fillRect, drawCircle, blit, multi-layer composite |
| `bench_ecs` | Object creation, component attach/detach, scene::update at 1/8/16/32/48 objects, event dispatch |
| `bench_lua` | Lua engine init, script load, binding call overhead, ObjectProxy round-trip, GC pressure |
| `bench_alloc` | Zero-allocation verification for hot-path operations (exits non-zero if any allocation detected) |

Build and run individually:

```bash
cmake -DENJIN2_BUILD_LUA=ON -DENJIN2_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release -B build-bench .
cmake --build build-bench --target bench_canvas bench_ecs bench_lua bench_alloc -- -j$(nproc)
./build-bench/benchmarks/bench_canvas
./build-bench/benchmarks/bench_ecs
./build-bench/benchmarks/bench_lua
./build-bench/benchmarks/bench_alloc
```

Or use the script, which handles all of the above:

```bash
bash scripts/build-bench.sh
```

### Canvas Benchmark Results (SDL3 Desktop, x86-64 Linux)

All values are `median(elapsed)`. Measured on x86-64 Linux desktop (SDL3 runner).

| Operation | Median |
|-----------|--------|
| canvas4: setPixel | 44 ns |
| canvas4: clear | 51 ns |
| canvas4: fillRect 32x32 | 25 ns |
| canvas4: drawCircle r16 | 177 ns |
| canvas4: blit 128x128 sprite | 40,950 ns (~41 us) |
| canvas8: setPixel | 15 ns |
| canvas8: fillRect 32x32 | 515 ns |
| compositor: composite 5 layers | 3,375 ns (~3.4 us) |

### ECS Benchmark Results (SDL3 Desktop, x86-64 Linux)

| Operation | Median |
|-----------|--------|
| scene::addObject x1 | 181 ns |
| scene::addObject x8 | 447 ns |
| scene::addObject x16 | 733 ns |
| scene::addObject x32 | 1,881 ns |
| scene::addObject x48 | 2,927 ns |
| object::addComponent<C_Position> | 56 ns |
| object::removeComponent<C_Position> | 58 ns |
| scene::update x1 objects | 18 ns |
| scene::update x8 objects | 43 ns |
| scene::update x16 objects | 73 ns |
| scene::update x32 objects | 124 ns |
| scene::update x48 objects | 172 ns |

### Lua Benchmark Results (SDL3 Desktop, x86-64 Linux)

| Operation | Median |
|-----------|--------|
| lua engine: init+shutdown | 75,055 ns (~75 us) |
| lua engine: executeString (noop script) | 1,126 ns |
| lua binding: engine.time.delta call | 1,005 ns |
| lua binding: math.clamp call | 1,503 ns |
| lua proxy: find+field round-trip | 1,514 ns |
| lua event: emit dispatch | 947 ns |
| lua GC: full collect | 2,240 ns |

These numbers are from SDL3 desktop (x86-64 Linux). See [Frame Budgets](#frame-budgets) for platform-specific context.

## CI Pipeline

The CI pipeline runs benchmarks on every push to `main` and on pull requests touching `src/**` or `include/**`.

Workflow file: `.github/workflows/benchmarks.yml`

### How It Works

1. `bash scripts/build-bench.sh` builds and runs all four binaries, producing `bench-results/*.json`.
2. `python3 scripts/convert-bench.py` converts the nanobench JSON format to `customSmallerIsBetter` format consumed by `github-action-benchmark`.
3. Results are stored on the `bench-data` orphan branch (not `gh-pages` — see [Architecture Notes](#architecture-notes) for why).
4. A historical dashboard is served from the gh-pages performance page using data from `bench-data`.

### Regression Detection

| Event | Behavior |
|-------|----------|
| Push to main | Results stored on `bench-data` branch; history accumulates |
| Pull request | Regression check only; `fail-on-alert: true` flags PRs that regress beyond threshold |
| Alert threshold | 150% (shared runner variance; will be tightened after 30-50 baseline runs) |
| `cancel-in-progress` | `false` — interrupted auto-push leaves `bench-data` in partial state |

PRs that regress a benchmark beyond 150% of the historical median fail the CI check.

### Manual Trigger

The workflow includes `workflow_dispatch` for seeding the initial baseline. If the first CI runs do not touch `src/**` or `include/**`, trigger manually from the Actions tab.

## Frame Timing

The `FrameTimingInstrumentation` singleton tracks per-phase microsecond timings: update, render, Lua, and composite. It is enabled only for the SDL3 desktop runner.

### Running with Frame Timing Overlay

```bash
cmake -DENJIN2_BUILD_SDL=ON -DENJIN2_BUILD_LUA=ON -DCMAKE_BUILD_TYPE=Release -B build .
cmake --build build --target enjin2_sdl
./build/src/platform/sdl/enjin2_sdl --show-timing --script scripts/layer_demo.lua
```

The `--show-timing` flag renders a live overlay in the top-left corner of the window showing `update`, `render`, `lua`, and `composite` times in microseconds.

### Compile-Path Details

`ENJIN2_FRAME_TIMING=1` is injected via CMake only for the `enjin2_sdl` target. WASM and ESP32 targets use the zero-overhead stub path in `include/enjin2/instrumentation/frame_timing.hpp`. The stub replaces all instrumentation calls with no-ops at compile time — no runtime cost on embedded or WASM builds.

### Polling API

Read timing values programmatically without the on-screen overlay:

```cpp
#include <enjin2/instrumentation/frame_timing.hpp>

auto& ft = enjin2::FrameTimingInstrumentation::get();
uint32_t update_us  = ft.updateTime_us;
uint32_t render_us  = ft.renderTime_us;
uint32_t lua_us     = ft.luaTime_us;
uint32_t comp_us    = ft.compositeTime_us;
```

## Lua Profiler

The `LuaProfiler` singleton is a C-level hook profiler (`LUA_MASKCALL | LUA_MASKRET`) that counts per-function calls without modifying Lua scripts. It is used via the `enjin_run` headless runner.

### Profiling a Lua Script

```bash
cmake -DENJIN2_BUILD_LUA=ON -DENJIN2_BUILD_HEADLESS=ON -DCMAKE_BUILD_TYPE=Release -B build-headless .
cmake --build build-headless --target enjin_run
./build-headless/src/platform/headless/enjin_run --profile --frames 100 scripts/my_game.lua
```

For JSON output:

```bash
./build-headless/src/platform/headless/enjin_run --profile --output json --frames 100 scripts/my_game.lua
```

### How enjin_run Works

`enjin_run` stubs all platform APIs (graphics, input) as no-ops. It runs the Lua script headlessly without SDL3 or any display dependency. This makes it suitable for CI profiling and scripting hot-path analysis on desktop as a proxy for ESP32 performance.

### Zero Overhead When Disabled

When `--profile` is not passed, `enjin_run` calls `lua_sethook(L, NULL, 0, 0)`, which removes all hooks. There is no runtime cost on non-profiled runs.

## Allocation Verification

Hot paths in enjin2 — canvas pixel ops, component updates, Lua binding calls — must not allocate heap memory. The allocation verification subsystem enforces this at CI time.

### The Zero-Alloc Guarantee

The `AllocGuard` RAII class in `include/enjin2/instrumentation/alloc_guard.hpp` wraps a scope and counts `operator new` calls. If any allocation occurs inside the scope, it records the count. The `bench_alloc` binary checks this at exit and returns a non-zero exit code if any allocation was detected.

### AllocGuard Usage Pattern

```cpp
#define ENJIN2_ALLOC_VERIFICATION 1
// (define g_alloc_guard_depth and g_alloc_count as thread_local, then)
// (override all six operator new/delete forms)
#include <enjin2/instrumentation/alloc_guard.hpp>

// Setup (allocations allowed here)
// ...
g_alloc_count = 0; // reset before hot-path section

// Hot-path section
{
    AllocGuard guard("canvas setPixel");
    canvas.setPixel(64, 64, 15);
}
// guard destructor checks g_alloc_count
```

See `benchmarks/bench_alloc.cpp` for the full pattern including all six `operator new`/`delete` form overrides.

### CI Enforcement

`scripts/build-bench.sh` runs `bench_alloc` as the final step. A non-zero exit from `bench_alloc` fails the CI job. This makes the zero-alloc guarantee machine-verifiable on every push.

### Pitfall: executeString Inside AllocGuard

`eng.executeString("...")` calls `luaL_loadstring` internally, which allocates a string object. Do not call `executeString` inside an `AllocGuard` scope. Instead, pre-register Lua functions with `luaL_ref` and invoke them with `lua_rawgeti` + `lua_call` for zero-alloc Lua calling. See the `deltaRef` pattern in `benchmarks/bench_alloc.cpp`.

## Frame Budgets

| Platform | Target FPS | Frame Budget | Instrumentation | Notes |
|----------|-----------|--------------|-----------------|-------|
| SDL3 Desktop | 30 fps (default, configurable via --fps) | 33.3 ms | Full FrameTimingInstrumentation | Measured values in tables above. Typical hot path: scene::update x48 = 172 ns, compositor = 3.4 us, Lua event emit = 947 ns — all well within budget. |
| WASM | 60 fps (requestAnimationFrame) | 16.6 ms | Zero-overhead stub (not instrumented) | WASM benchmarks are not run in CI. Use SDL3 desktop numbers as a conservative upper bound — WASM hot paths may be 2-5x slower depending on Emscripten optimization level. |
| ESP32-S3 | 60 fps target (dt guard) | 16.6 ms | Zero-overhead stub (not instrumented) | ESP32-S3 benchmarks are not run in CI. Measured FPS from the esp32_lcd_demo with strip-pipelined DMA (40-line strips, 80MHz SPI) typically achieves 30-60 fps depending on Lua script complexity. Use enjin_run with --profile to measure Lua hot paths on desktop as a proxy. |

## Adding New Benchmarks

Follow these five steps to register a new nanobench case.

### Step 1: Create the Benchmark Source File

Copy the pattern from `benchmarks/bench_canvas.cpp`:

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
        // exercise the hot path
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

- Define `ANKERL_NANOBENCH_IMPLEMENT` exactly once per binary. Each benchmark binary is a separate executable.
- Always call `doNotOptimizeAway()` on the result to prevent dead-code elimination.
- Write JSON to `bench-results/bench_NAME.json`.
- Call `mkdir("bench-results", 0755)` before the `std::ofstream` to ensure the directory exists.

### Step 2: Register the CMake Target

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

The CI workflow (`.github/workflows/benchmarks.yml`) runs `bash scripts/build-bench.sh` and `python3 scripts/convert-bench.py`. Both pick up the new benchmark automatically after Steps 3 and 4.

## Architecture Notes

### Dual-Path Compile Guard Pattern

Both `frame_timing.hpp` and `alloc_guard.hpp` use the same pattern to eliminate overhead on embedded and WASM builds:

```cpp
#ifdef ENJIN2_FEATURE_NAME
// Full implementation with heavy includes
class Feature { /* ... */ };
#else
// Zero-overhead no-op stub
class Feature { /* all methods are empty inline stubs */ };
#endif
```

CMake injects the feature define only for targets that need it. The `enjin2_sdl` target gets `ENJIN2_FRAME_TIMING=1`. WASM and ESP32 targets never receive the define and compile the stub path.

### nanobench INTERFACE Target Pattern

`nanobench_vendor` is an INTERFACE library (no compilation step) that exposes the single-header include path:

```cmake
add_library(nanobench_vendor INTERFACE)
target_include_directories(nanobench_vendor INTERFACE ${CMAKE_SOURCE_DIR}/vendor)
```

Every benchmark binary that links `nanobench_vendor` gets the header include path automatically. There is no library compilation step — the implementation is compiled once per binary via `#define ANKERL_NANOBENCH_IMPLEMENT`.

### bench-data Branch Isolation from gh-pages

Benchmark history is stored on the `bench-data` orphan branch, not on `gh-pages`. This is a deliberate architectural decision: the `docs.yml` workflow uses `actions/deploy-pages`, which replaces all content on `gh-pages` on every docs deployment. Storing benchmark history on `gh-pages` would cause every docs deploy to destroy the accumulated benchmark data. The `bench-data` branch is written only by the benchmarks workflow and is never touched by the docs deployment.
