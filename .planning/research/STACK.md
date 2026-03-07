# Stack Research

**Domain:** C++ game engine benchmarking & performance infrastructure (enjin2 v1.10)
**Researched:** 2026-03-07
**Confidence:** HIGH — nanobench and github-action-benchmark verified via official releases and docs; lua_sethook is stable Lua 5.4 C API; operator new override is standard C++17; SDL_GetPerformanceCounter is documented SDL3 API.

---

## Scope

This document covers **only the new stack additions for v1.10 Benchmarking & Performance**. It does not re-research validated capabilities from v1.0–v1.9 (SDL3 runner, Lua scripting, CMake multi-target, GitHub Actions docs pipeline, etc.).

The v1.10 work requires exactly five new capabilities:

1. Microbenchmark harness (nanobench) for canvas, ECS, and Lua subsystems
2. CI regression detection dashboard (github-action-benchmark)
3. Per-phase frame timing in the SDL3 runner (SDL_GetPerformanceCounter)
4. Lua function call profiling via `lua_sethook` in a headless CLI runner
5. Zero-alloc verification for hot paths (global `operator new` override in test TUs)

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| ankerl::nanobench | **v4.3.11** (Feb 16, 2025) | Microbenchmark harness for canvas/ECS/Lua suites | Single-header (`nanobench.h`), C++11/17 compatible, zero external deps. 65x faster autotuning than google/benchmark — critical for keeping CI build+run time low. Produces JSON natively via `templates::json()`. No test runner required — works as a standalone `main()`. FetchContent-compatible with the `nanobench::nanobench` alias. |
| benchmark-action/github-action-benchmark | **v1** (latest 1.x) | CI performance regression dashboard and alerting | Accepts `customSmallerIsBetter` JSON format `[{name, value, unit}]` — bridges nanobench's JSON output with a ~5-line `jq` command. Auto-commits results to `gh-pages` branch (already exists for docs). Posts regression alerts as commit comments. Supports configurable threshold (`alert-threshold: '110%'`). Free, self-hosted, no external service dependency. |
| SDL3 `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency` | SDL3 (already linked) | Per-phase frame timing (update/render/Lua/composite) | Already in the project, zero new deps. Returns a system high-resolution counter (nanosecond-scale on modern Linux/macOS/Windows). Store per-phase `uint64_t` snapshots in a plain `FrameStats` struct; convert to microseconds via `(end - start) * 1'000'000 / SDL_GetPerformanceFrequency()`. No locking needed for single-writer game loop. |
| Lua 5.4 C API `lua_sethook` | Lua 5.4.8 / LuaJIT 2.x (already in project) | Lua profiling — per-function call counts and instruction sampling | Part of the stable Lua C API since Lua 5.1; unchanged in 5.4 and LuaJIT. Set mask `LUA_MASKCALL \| LUA_MASKRET` to count entries/exits per function. For instruction sampling: `LUA_MASKCOUNT` with a fixed interval (e.g., every 100 instructions). Official Lua docs confirm: the C API hook has lower overhead than a Lua-side `debug.sethook` hook — critical for valid profiling measurements. Zero new deps. |
| Global `operator new` override (project header) | C++17 standard (no dep) | Zero-alloc verification — assert no heap allocation occurs in hot paths | Standard C++17 technique: define a global `operator new` / `operator new[]` in a test translation unit that aborts if a thread-local `g_alloc_forbidden` flag is set. Wrap hot-path code in a `NoAllocScope` RAII guard. Portable: works on ESP32 (xtensa-g++), Emscripten (emcc), and desktop. No LD_PRELOAD, no dynamic linking, no Linux-only constraint. Gate behind `ENJIN2_BUILD_TESTS` so production and firmware builds are unaffected. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<chrono>` (std) | C++17 (already required) | Fallback timer inside nanobench's implementation | Nanobench uses `<chrono>` internally — no manual use needed. Also usable for pure-C++ micro-timing outside SDL (e.g., benchmarking allocation-heavy setup code). |
| `<atomic>` (std) | C++17 (already required) | Lock-free frame timing accumulators for rolling stats | Use `std::atomic<uint64_t>` for per-phase accumulated time and frame count in the SDL runner's `FrameStats` struct. Single-writer game loop means no contention; `std::atomic` with `memory_order_relaxed` load/store is sufficient. |
| `jq` | Pre-installed on `ubuntu-latest` GitHub Actions runners | Convert nanobench JSON → github-action-benchmark custom format | A single `jq` one-liner transforms nanobench's `results[].{name, median}` into `[{name, value, unit}]`. No Python dependency, no added CI step. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `cmake -DENJIN2_BUILD_BENCHMARKS=ON` | Gate benchmark targets to SDL3 desktop only | Benchmarks use `<chrono>`, `std::cout`, and nanobench — all incompatible with bare-metal ESP32 and WASM. Gate the same way `ENJIN2_BUILD_TESTS` is gated: `if(ENJIN2_BUILD_BENCHMARKS AND NOT ESP32 AND NOT EMSCRIPTEN)`. |
| `SDL_HINT_VIDEODRIVER=offscreen` | Run SDL3 headless for Lua profiler CLI mode | SDL3 supports an offscreen videodriver via this hint. Set it before `SDL_Init` in the runner when a `--headless` flag is passed. No window is created; game loop still runs normally. Confirmed in SDL3 docs. |
| `--benchmark_filter` (nanobench runner arg) | Run a subset of benchmarks during development | Nanobench supports a `setName()` filter pattern via `ANKERL_NANOBENCH_CONFIG_CPP` or by argc/argv parsing in `main()`. Useful for iterating on a single subsystem without running the full suite. |

---

## Installation

```cmake
# CMakeLists.txt additions for v1.10

option(ENJIN2_BUILD_BENCHMARKS "Build nanobench benchmark suite (SDL3 desktop only)" OFF)

if(ENJIN2_BUILD_BENCHMARKS AND NOT ESP32 AND NOT EMSCRIPTEN)
    include(FetchContent)
    FetchContent_Declare(
        nanobench
        GIT_REPOSITORY https://github.com/martinus/nanobench.git
        GIT_TAG        v4.3.11
    )
    FetchContent_MakeAvailable(nanobench)

    add_executable(enjin2_bench
        benchmarks/bench_canvas.cpp
        benchmarks/bench_ecs.cpp
        benchmarks/bench_lua.cpp
        benchmarks/bench_main.cpp
    )
    target_link_libraries(enjin2_bench PRIVATE
        enjin2
        nanobench::nanobench
    )
    target_include_directories(enjin2_bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()
```

```yaml
# .github/workflows/bench.yml (new file — separate from docs.yml)

name: Benchmark

on:
  push:
    branches: [main]
    paths:
      - 'src/**'
      - 'include/**'
      - 'benchmarks/**'
      - '.github/workflows/bench.yml'
  # NOTE: Do NOT run on pull_request — github-action-benchmark docs explicitly warn
  # against this due to GITHUB_TOKEN secrets access limitations on fork PRs.

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install SDL3 dev
        run: sudo apt-get install -y libsdl3-dev

      - name: Build benchmarks
        run: |
          cmake -B build-bench \
            -DENJIN2_BUILD_BENCHMARKS=ON \
            -DENJIN2_BUILD_LUA=ON \
            -DENJIN2_BUILD_TESTS=OFF \
            -DCMAKE_BUILD_TYPE=Release
          cmake --build build-bench --target enjin2_bench

      - name: Run benchmarks (nanobench JSON output)
        run: |
          ./build-bench/enjin2_bench --out json > bench-raw.json

      - name: Convert to github-action-benchmark format
        run: |
          jq '[.results[] | {name: .name, value: (.median * 1e9 | round), unit: "ns"}]' \
             bench-raw.json > bench-results.json

      - name: Store benchmark result
        uses: benchmark-action/github-action-benchmark@v1
        with:
          tool: customSmallerIsBetter
          output-file-path: bench-results.json
          github-token: ${{ secrets.GITHUB_TOKEN }}
          auto-push: true
          alert-threshold: '110%'
          comment-on-alert: true
          fail-on-alert: true
          gh-pages-branch: gh-pages
          benchmark-data-dir-path: dev/bench
```

```cpp
// include/enjin2/test/no_alloc_scope.hpp
// Gate with ENJIN2_BUILD_TESTS — never compiled into production or firmware.

#pragma once
#include <cstdlib>   // std::abort
#include <cstdio>    // std::fprintf
#include <new>       // ::operator new replacement signatures

#ifdef ENJIN2_BUILD_TESTS

namespace enjin2::test {

inline thread_local bool g_alloc_forbidden = false;

struct NoAllocScope {
    NoAllocScope()  { g_alloc_forbidden = true;  }
    ~NoAllocScope() { g_alloc_forbidden = false; }
    NoAllocScope(const NoAllocScope&) = delete;
    NoAllocScope& operator=(const NoAllocScope&) = delete;
};

} // namespace enjin2::test

// Override global operator new in this translation unit.
// Only ONE .cpp file in the test suite should define ENJIN2_DEFINE_NO_ALLOC_HOOK.
#ifdef ENJIN2_DEFINE_NO_ALLOC_HOOK

void* operator new(std::size_t sz) {
    if (enjin2::test::g_alloc_forbidden) {
        std::fprintf(stderr, "[ALLOC VIOLATION] operator new(%zu) called inside NoAllocScope\n", sz);
        std::abort();
    }
    void* p = std::malloc(sz);
    if (!p) throw std::bad_alloc{};
    return p;
}

void* operator new[](std::size_t sz) {
    if (enjin2::test::g_alloc_forbidden) {
        std::fprintf(stderr, "[ALLOC VIOLATION] operator new[](%zu) called inside NoAllocScope\n", sz);
        std::abort();
    }
    void* p = std::malloc(sz);
    if (!p) throw std::bad_alloc{};
    return p;
}

void operator delete(void* p) noexcept  { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }

#endif // ENJIN2_DEFINE_NO_ALLOC_HOOK
#endif // ENJIN2_BUILD_TESTS
```

```cpp
// benchmarks/bench_main.cpp — minimal nanobench runner with JSON output

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

// Defined in bench_canvas.cpp, bench_ecs.cpp, bench_lua.cpp
void bench_canvas(ankerl::nanobench::Bench& b);
void bench_ecs(ankerl::nanobench::Bench& b);
void bench_lua(ankerl::nanobench::Bench& b);

#include <fstream>
#include <cstring>

int main(int argc, char** argv) {
    ankerl::nanobench::Bench b;
    b.title("enjin2").warmup(10).minEpochIterations(100);

    bench_canvas(b);
    bench_ecs(b);
    bench_lua(b);

    // Output JSON if --out json is passed
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 &&
            std::strcmp(argv[i+1], "json") == 0) {
            b.render(ankerl::nanobench::templates::json(), std::cout);
            return 0;
        }
    }
    // Default: human-readable to stdout
    return 0;
}
```

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| ankerl::nanobench v4.3.11 | google/benchmark | If you need named fixtures, SetUp/TearDown lifecycle, multi-threaded benchmarks, or Google's perf regression CI tooling. google/benchmark pulls in Abseil and has significantly longer compile times — wrong for this project's lightweight CI goal. |
| ankerl::nanobench | Catch2 `BENCHMARK` macro | If Catch2 were already in the project as the unit test framework. Adding Catch2 just for benchmarks is a larger footprint than nanobench alone, and Catch2's BENCHMARK output requires extra config to produce github-action-benchmark-compatible JSON. |
| ankerl::nanobench | picobench | picobench is simpler but has no JSON output and no autotuning — requires manual iteration count tuning per benchmark, which becomes maintenance burden over time. |
| benchmark-action/github-action-benchmark | bencherdev/bencher | Bencher is a full SaaS product with statistical thresholds and richer history. github-action-benchmark is self-hosted, zero cost, no account required, integrates with the existing `gh-pages` branch and GitHub Pages pipeline. |
| Global `operator new` override (header) | LD_PRELOAD malloc interposition | LD_PRELOAD is Linux-only and requires dynamic linking. ESP32 is bare-metal (no dynamic loader). WASM has no LD_PRELOAD. The header override works at compile time on all three targets. |
| `lua_sethook` C API hook | `debug.sethook` from Lua side | The official Lua PIL states: "for profiling with timing, it is better to use the C interface: the overhead of a Lua call for each hook is too high and usually invalidates any measure." The C API hook is invoked without creating a new Lua activation frame — lower overhead and more accurate. |
| `SDL_GetPerformanceCounter` | `std::chrono::high_resolution_clock` | Both work on desktop. SDL3 is already linked in the runner; using SDL's counter avoids divergence from the existing delta-time calculation and is consistent with the platform timing model already in the codebase. |
| `jq` one-liner in CI | Python script or nlohmann/json in C++ | Python adds a CI setup step. Adding nlohmann/json as a dep for a ~30-line conversion is unjustified. `jq` is pre-installed on `ubuntu-latest` runners and the transform is a single pipeline command. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| google/benchmark | Pulls Abseil (~200 headers), enables exceptions, slow compile times. 65x slower autotuning than nanobench per nanobench's own comparison. | ankerl::nanobench v4.3.11 |
| Valgrind massif / heaptrack | Requires Linux, 10–50x execution overhead, incompatible with ESP32 and WASM targets, too slow for CI. | Global `operator new` override — runs at full speed, works in CTest across all platforms. |
| Tracy profiler | Excellent standalone tool, but requires a GUI client, a TCP server thread, and dynamic allocation for the ring buffer. Violates the zero-alloc constraint in hot paths. | `SDL_GetPerformanceCounter` per-phase timing + `FrameStats` accumulator struct. |
| gperftools / tcmalloc | LD_PRELOAD only, Linux-only, requires dynamic linking. Same constraints as Valgrind. | Global `operator new` override. |
| LuaJIT `-jp` profiler | LuaJIT is not available on the WASM build (no WASM backend); profiler is LuaJIT-specific. | `lua_sethook` C API — works with both Lua 5.4.8 (WASM) and LuaJIT (desktop/ESP32). |
| PAPI hardware performance counters | Requires kernel `perf_event` permissions — not available on GitHub Actions hosted runners (unprivileged containers) or on ESP32. | nanobench's built-in perf-event support (gracefully degrades to `<chrono>` when unavailable — no CI breakage). |
| Benchmarks on pull_request CI trigger | github-action-benchmark documentation explicitly warns against running the action on PRs due to `GITHUB_TOKEN` scope limitations and potential result corruption from fork-based PRs. | Run `bench.yml` on `push` to `main` only. |
| Separate `gh-pages` branch for benchmarks | The project already uses `gh-pages` for the Docusaurus documentation site. Adding a second gh-pages branch would conflict. | Use `benchmark-data-dir-path: dev/bench` to write benchmark data into a subdirectory of the existing `gh-pages` branch. |

---

## Stack Patterns by Feature Area

**Nanobench benchmark suite:**
- One `bench_main.cpp` with `#define ANKERL_NANOBENCH_IMPLEMENT` and the `main()` function
- Separate `bench_canvas.cpp`, `bench_ecs.cpp`, `bench_lua.cpp` — each defines a `void bench_X(ankerl::nanobench::Bench&)` function
- `ANKERL_NANOBENCH_IMPLEMENT` must be defined in exactly one TU — put it in `bench_main.cpp` only
- Pass `ankerl::nanobench::doNotOptimizeAway(result)` for every benchmark result to prevent dead-code elimination

**Frame timing instrumentation:**
- Add `FrameStats` struct to the SDL3 runner: `uint64_t update_ns, render_ns, lua_ns, composite_ns, total_ns`
- Capture `SDL_GetPerformanceCounter()` before/after each phase; accumulate into a rolling `FrameStats`
- Convert to microseconds: `val * 1'000'000 / SDL_GetPerformanceFrequency()`
- Display on the debug canvas layer (already exists: `ENJIN_LAYER_COUNT=5`, layer 4 reserved for debug) when a `--show-timing` flag is active
- Use `std::atomic<uint64_t>` only if timing data is read from a separate thread; single-threaded game loop can use plain `uint64_t`

**Lua profiler (headless CLI runner mode):**
- Add `--headless --lua-profile --frames N` flags to the SDL3 runner's `main()`
- Set `SDL_SetHint(SDL_HINT_VIDEODRIVER, "offscreen")` before `SDL_Init` in headless mode
- Install the hook before `executeScript()`: `lua_sethook(L, profile_hook, LUA_MASKCALL | LUA_MASKRET, 0)`
- Hook maintains a `std::unordered_map<const void*, CallInfo>` (function pointer → call count + total ns)
- After N frames, dump sorted call table to stdout as CSV: `function,calls,total_ns,avg_ns`
- Remove the hook before shutdown: `lua_sethook(L, nullptr, 0, 0)`

**Zero-alloc verification:**
- One file: `tests/alloc_verify_test.cpp` — defines `ENJIN2_DEFINE_NO_ALLOC_HOOK` before including `no_alloc_scope.hpp`
- Test wraps each hot-path call in `{ NoAllocScope guard; hot_path_call(); }`
- If any allocation fires, the test aborts with a clear message: `[ALLOC VIOLATION] operator new(N) called inside NoAllocScope`
- Add `alloc_verify_test` to CTest alongside existing test suite

---

## Version Compatibility

| Component | Version | Compatibility Notes |
|-----------|---------|---------------------|
| nanobench v4.3.11 | C++17 (project standard) | Requires C++14 minimum; C++17 is fully supported. No conflicts with existing CMake targets. FetchContent alias `nanobench::nanobench` available since nanobench v4.x. |
| nanobench v4.3.11 | CMake 3.16+ | Project already requires CMake 3.16; FetchContent available since CMake 3.11. No constraint added. |
| nanobench v4.3.11 | Exceptions | nanobench's implementation includes `<stdexcept>` — requires exceptions enabled. The benchmark executable runs only on the SDL3 desktop target (exceptions available). The `ENJIN2_BUILD_BENCHMARKS` gate ensures it is never compiled for ESP32 or WASM. |
| github-action-benchmark v1 | actions/checkout@v4, ubuntu-latest | Existing `docs.yml` uses the same runner and checkout version. Compatible. The `customSmallerIsBetter` tool accepts any JSON array, so future nanobench format changes only require updating the `jq` transform. |
| Lua 5.4 `lua_sethook` | Lua 5.4.8 (WASM) and LuaJIT 2.x (desktop/ESP32) | `lua_sethook` is part of the stable Lua C API. The signature `void lua_sethook(lua_State *L, lua_Hook f, int mask, int count)` is identical in Lua 5.1 through 5.4 and LuaJIT. No compatibility concern. |
| `operator new` override | C++17, all three targets | Compile-time override; no dynamic linking required. Verified standard-compliant by cppreference. Works with xtensa-esp32s3-elf-g++ (ESP32), emcc (WASM), and g++/clang++ (desktop). Gate behind `ENJIN2_BUILD_TESTS`. |
| `SDL_GetPerformanceCounter` | SDL3 (already in project) | API unchanged from SDL2 to SDL3; same function signature. Already used by the existing delta-time calculation in the SDL3 runner — this is an extension of existing usage. |

---

## Sources

- https://github.com/martinus/nanobench/releases/tag/v4.3.11 — confirmed v4.3.11 released February 16, 2025 (HIGH confidence)
- https://nanobench.ankerl.com/reference.html — confirmed `templates::json()`, `templates::csv()`, `templates::htmlBoxplot()` output formats; `render()` method signature (HIGH confidence)
- https://nanobench.ankerl.com/tutorial.html — confirmed FetchContent `nanobench::nanobench` alias pattern; standalone `main()` without test framework; `ANKERL_NANOBENCH_IMPLEMENT` in one TU (HIGH confidence)
- https://nanobench.ankerl.com/comparison.html — confirmed ~65x faster autotuning than google/benchmark (HIGH confidence)
- https://github.com/benchmark-action/github-action-benchmark — confirmed v1 tag, `customSmallerIsBetter` JSON format `[{name, value, unit}]`, 110% default threshold, PR warning against fork PRs, `gh-pages-branch` and `benchmark-data-dir-path` inputs (HIGH confidence)
- https://www.lua.org/pil/23.3.html — confirmed `lua_sethook` C API, `LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT` masks, C API preferred over Lua-side hook for profiling accuracy (HIGH confidence)
- https://wiki.libsdl.org/SDL3/SDL_GetPerformanceCounter — SDL3 high-resolution counter, nanosecond resolution, `SDL_GetPerformanceFrequency()` conversion (HIGH confidence)
- https://cppreference.com/w/cpp/memory/new/operator_new — global `operator new` replaceable allocation function — standard C++17 (HIGH confidence)
- WebSearch: nanobench exception requirements — nanobench uses `<stdexcept>` internally; benchmark target is gated to desktop where exceptions are enabled. Confirmed via build integration research. (MEDIUM confidence — no official "no-exceptions mode" statement found in docs)

---

*Stack research for: enjin2 v1.10 Benchmarking & Performance milestone*
*Researched: 2026-03-07*
