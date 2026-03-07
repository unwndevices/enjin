# Architecture Research

**Domain:** 2D game engine — v1.10 benchmarking & performance infrastructure
**Researched:** 2026-03-07
**Confidence:** HIGH (full codebase read; all integration points traced from source)

---

## Standard Architecture

### System Overview

The v1.10 milestone adds an instrumentation and measurement layer on top of the existing enjin2 engine. No existing subsystems are restructured. The work introduces four new components — a benchmark suite, CI pipeline, a frame timing struct, and a Lua profiler/headless runner — all of which integrate as additive layers, not as replacements.

```
┌────────────────────────────────────────────────────────────────────────┐
│                  NEW: Benchmark & Instrumentation Layer                 │
│                                                                        │
│  benchmarks/         .github/workflows/       docs/PERFORMANCE.md      │
│  ├── bench_canvas    benchmarks.yml           (performance guide)       │
│  ├── bench_ecs       bench-results/                                     │
│  └── bench_lua       (JSON history, gh-pages dashboard)                 │
│                                                                        │
│  tools/enjin_run     (NEW: headless Lua runner, CLI profiling)          │
│  src/perf/           (NEW: FrameTimingInstrumentation, alloc counter)   │
├────────────────────────────────────────────────────────────────────────┤
│                  MODIFIED: SDL3 Runner (sdl_main.cpp)                   │
│                                                                        │
│  Per-phase timing capture points (update / Lua / composite / render)   │
│  SDL_GetTicks64() calls around each phase → write to FrameTiming atomics│
├────────────────────────────────────────────────────────────────────────┤
│                  EXISTING: enjin2 Engine (unchanged in hot paths)       │
│                                                                        │
│  enjin2_core    enjin2_graphics    enjin2_ui    enjin2_input            │
│  ┌───────────┐  ┌─────────────┐   ┌──────────┐  ┌──────────────────┐  │
│  │ Object /  │  │ Canvas4/8   │   │ C_Sprite │  │  InputState      │  │
│  │ Scene /   │  │ Primitives  │   │ LuaCanvas│  │  input_platform_ │  │
│  │ SSM       │  │ LayerComp.  │   │          │  │  poll()          │  │
│  └───────────┘  └─────────────┘   └──────────┘  └──────────────────┘  │
│                                                                        │
│  enjin2_lua (LuaScriptSystem, LuaEngine, LuaBindings)                  │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ lua_sethook hook ← NEW profiler attaches here                   │   │
│  │ lua_gc tracking  ← NEW memory probe reads here each frame       │   │
│  └─────────────────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────────────────┤
│                  EXISTING: Test Layer (tests/CMakeLists.txt)            │
│  CTest targets (unchanged) + new bench_* CMake targets added below      │
└────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Status for v1.10 |
|-----------|---------------|------------------|
| `benchmarks/bench_canvas.cpp` | nanobench suite for Canvas4/8 pixel ops, rect/circle/sprite/composite | NEW |
| `benchmarks/bench_ecs.cpp` | nanobench suite for Object/Component creation, scene update scaling | NEW |
| `benchmarks/bench_lua.cpp` | nanobench suite for LuaEngine init, script load, binding call overhead, GC | NEW |
| `benchmarks/CMakeLists.txt` | CMake targets for all bench_* binaries; guards with `ENJIN2_BUILD_BENCH` option | NEW |
| `vendor/nanobench.h` | Single-header benchmark library; vendored like `stb_image.h` | NEW |
| `src/perf/frame_timing.hpp` | `FrameTimingInstrumentation` struct with `std::atomic` fields (lock-free) | NEW |
| `src/perf/alloc_counter.hpp` | `operator new`/`delete` intercept wrapper; count allocations in hot-path sections | NEW |
| `src/platform/sdl/sdl_main.cpp` | Add `SDL_GetTicks64()` capture at 4 phase boundaries; write to atomics | MODIFIED |
| `tools/enjin_run.cpp` | Headless Lua runner executable: no SDL, no display, stub graphics/input; `--profile --frames N` | NEW |
| `tools/CMakeLists.txt` | CMake target for `enjin_run`; links `enjin2_lua` but not SDL3 | NEW |
| `.github/workflows/benchmarks.yml` | CI: build bench_* on push to main + PRs; run + compare vs baseline | NEW |
| `scripts/build-bench.sh` | Build and run all benchmarks; write JSON to `bench-results/` | NEW |
| `docs/PERFORMANCE.md` | Performance guide covering all 5 subsystems; quick-start one-liner | NEW |

---

## Recommended Project Structure

### New Files and Where They Live

```
enjin/
├── benchmarks/                     (NEW directory)
│   ├── CMakeLists.txt              (NEW — bench_* targets)
│   ├── bench_canvas.cpp            (NEW — Canvas4/8 nanobench suite)
│   ├── bench_ecs.cpp               (NEW — Object/Component/Scene nanobench)
│   └── bench_lua.cpp               (NEW — LuaEngine/bindings nanobench)
│
├── src/
│   ├── perf/                       (NEW directory)
│   │   ├── frame_timing.hpp        (NEW — FrameTimingInstrumentation struct)
│   │   └── alloc_counter.hpp       (NEW — hot-path allocation intercept)
│   └── platform/
│       └── sdl/
│           └── sdl_main.cpp        (MODIFIED — phase timing capture)
│
├── tools/                          (NEW directory)
│   ├── CMakeLists.txt              (NEW — enjin_run target)
│   └── enjin_run.cpp               (NEW — headless Lua runner + profiler)
│
├── vendor/
│   ├── nanobench.h                 (NEW — single-header benchmark library)
│   ├── stb_image.h                 (existing)
│   └── stb_image_write.h           (existing)
│
├── bench-results/                  (NEW directory — gitignored locally)
│   └── *.json                      (nanobench JSON output; stored on gh-pages)
│
├── scripts/
│   ├── build-bench.sh              (NEW — build + run benchmarks end-to-end)
│   ├── convert-bench-json.sh       (NEW — merge per-binary JSON for CI dashboard)
│   └── ...                         (existing scripts unchanged)
│
├── .github/
│   └── workflows/
│       └── benchmarks.yml          (NEW — CI regression workflow)
│
├── CMakeLists.txt                  (MODIFIED — add ENJIN2_BUILD_BENCH option + subdirectory)
└── docs/
    └── PERFORMANCE.md              (NEW — performance documentation)
```

### Structure Rationale

- **`benchmarks/` at root:** Mirrors existing `tests/` placement. CTest tests stay in `tests/`; nanobench binaries go in `benchmarks/`. Avoids contaminating `examples/` with CI-critical artifacts.
- **`src/perf/` headers-only:** `FrameTimingInstrumentation` and `alloc_counter` are header-only structs (like `layer_compositor.hpp`). No new `.cpp` — follows codebase pattern for simple structs with no external deps.
- **`tools/enjin_run`:** Separate from `benchmarks/` because it is a runtime tool (Lua profiling, headless execution), not a measurement harness. It links `enjin2_lua` but not SDL3.
- **`vendor/nanobench.h`:** Single-header pattern already established by `vendor/stb_image.h`. No new CMake `find_package` needed.
- **`bench-results/` gitignored:** JSON results accumulate locally during development; CI stores them on `gh-pages` branch only. The `.gitignore` gets one new entry.

---

## Architectural Patterns

### Pattern 1: nanobench Integration (Single-Header Vendor)

**What:** `ANKERL_NANOBENCH_IMPLEMENT` defined in exactly one `.cpp` file to provide the implementation. All other translation units include the header without the define.

**When to use:** In the first `.cpp` file linked in each benchmark binary. Since each bench binary is a single `.cpp`, each defines `ANKERL_NANOBENCH_IMPLEMENT` at the top.

**Trade-offs:** No CMake `add_library` needed; zero overhead for non-benchmark builds; same pattern as `stb_image_write.h`.

**Example:**
```cpp
// benchmarks/bench_canvas.cpp
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>
#include <enjin2/graphics/canvas.hpp>

int main() {
    ankerl::nanobench::Bench b;
    b.title("Canvas4").warmup(100).minEpochIterations(1000);

    static enjin2::Canvas4<128, 128> canvas;

    b.run("setPixel hot path", [&] {
        canvas.setPixel(64, 32, enjin2::Pixel4(3));
        ankerl::nanobench::doNotOptimizeAway(canvas);
    });

    b.run("clear 128x128", [&] {
        canvas.clear(enjin2::Pixel4(0));
        ankerl::nanobench::doNotOptimizeAway(canvas);
    });
}
```

### Pattern 2: FrameTimingInstrumentation (Lock-Free Atomics, Header-Only)

**What:** A plain struct of `std::atomic<uint64_t>` fields, cache-line aligned, written from the game loop (SDL runner) and read from a polling/display API. No locks, no heap.

**When to use:** In `sdl_main.cpp` to capture per-phase microsecond timestamps. Declared as a file-scope static; never heap-allocated.

**Trade-offs:**
- `std::atomic` on ESP32 (Xtensa) requires careful alignment; `alignas(4)` is sufficient for 32-bit atomics. Use `uint32_t` for ESP32 variants (64-bit atomics require libatomic on some toolchains).
- WASM: `std::atomic` compiles correctly under Emscripten for single-threaded builds; shared-memory atomics (`-sUSE_PTHREADS`) are not needed here.
- SDL runner: `SDL_GetTicks64()` gives millisecond resolution; for sub-ms phases use `std::chrono::steady_clock` or platform HPET. The struct should store microseconds as `uint64_t`.

**Example:**
```cpp
// src/perf/frame_timing.hpp
#pragma once
#include <atomic>
#include <cstdint>

namespace enjin2 {

/// Per-frame timing data updated by the host game loop.
/// All fields in microseconds (us). Written atomically; read by overlay/CLI.
/// alignas(64): one cache line per struct on x86/ARM.
struct alignas(64) FrameTimingInstrumentation {
    std::atomic<uint32_t> frameCount{0};
    std::atomic<uint32_t> updateTime_us{0};   // C++ update + camera/coroutine/tween
    std::atomic<uint32_t> luaTime_us{0};      // Lua update() + draw() + callbacks
    std::atomic<uint32_t> compositeTime_us{0};// LayerCompositor::composite()
    std::atomic<uint32_t> renderTime_us{0};   // expand_canvas_to_rgb + SDL blit
    std::atomic<uint32_t> totalFrame_us{0};   // full frame wall time
};

/// Global singleton; zero-initialized at program start.
/// sdl_main.cpp writes it; tools/enjin_run.cpp also writes it if instrumented.
extern FrameTimingInstrumentation g_frameTiming;

} // namespace enjin2
```

**SDL runner integration points** — six SDL_GetTicks64 calls inserted in `sdl_main.cpp`'s game loop:
```
frame_start         ← already exists (used for pacing)
after input poll    ← t0
after Lua callbacks + update() + tickCamera/Coroutines/Tweens ← t1
after draw()        ← t2  (lua phase = t1..t2 skips C++ scheduler ticks... see note)
after composite()   ← t3
after SDL_RenderPresent ← t4

updateTime_us  = t1 - t0   (includes C++ scheduler ticks)
luaTime_us     = t2 - t1
compositeTime_us = t3 - t2
renderTime_us  = t4 - t3
totalFrame_us  = t4 - frame_start
```

Note: the existing SDL runner interleaves Lua calls and C++ scheduler ticks (tickCameraFollow, tickCoroutines, tickTweens) in one block. The timing split treats the entire block as "update phase" and the Lua draw() call as "lua render phase." This is sufficient precision for the per-phase breakdown target.

### Pattern 3: Lua Profiler via lua_sethook (Additive, Zero Cost When Disabled)

**What:** Call `lua_sethook(L, profiler_hook, LUA_MASKCALL | LUA_MASKRET, 0)` to receive a callback on every function entry/exit. The hook accumulates call counts and estimates self-time using `std::chrono::steady_clock`. Disable with `lua_sethook(L, nullptr, 0, 0)` — zero overhead.

**When to use:** In `tools/enjin_run.cpp` when `--profile` is passed. Also as an optional toggle in the SDL runner (disabled by default; activated via CLI flag or engine.debug toggle).

**Integration point:** `LuaEngine::getState()` exposes `lua_State*` publicly. The profiler calls `lua_sethook` directly on this pointer — no modification to `LuaEngine` is required.

**Trade-offs:**
- `lua_sethook` adds measurable overhead when enabled (~5–15% per-frame on Lua-heavy scripts). Document this: profiling results show relative costs, not absolute production costs.
- Function name extraction uses `lua_getinfo(L, "nS", &ar)` — this is safe to call inside the hook.
- Per-function timing is approximate (shared `std::chrono` call in hook). Good enough for identifying hot Lua functions.

**Example:**
```cpp
// tools/enjin_run.cpp (relevant section)
struct LuaFuncStat {
    char name[64];
    uint32_t calls{0};
    uint64_t totalUs{0};
};
static LuaFuncStat g_stats[128];
static uint64_t    g_hookEnterUs{0};

void profiler_hook(lua_State* L, lua_Debug* ar) {
    using clk = std::chrono::steady_clock;
    if (ar->event == LUA_HOOKCALL) {
        g_hookEnterUs = (uint64_t)std::chrono::duration_cast<
            std::chrono::microseconds>(clk::now().time_since_epoch()).count();
    } else if (ar->event == LUA_HOOKRET) {
        uint64_t elapsed = (uint64_t)std::chrono::duration_cast<
            std::chrono::microseconds>(clk::now().time_since_epoch()).count()
            - g_hookEnterUs;
        lua_getinfo(L, "n", ar);
        // find or create entry for ar->name, accumulate
    }
}

// Enable:
lua_sethook(lua.getEngine().getState(), profiler_hook,
            LUA_MASKCALL | LUA_MASKRET, 0);

// Disable:
lua_sethook(lua.getEngine().getState(), nullptr, 0, 0);
```

### Pattern 4: Hot-Path Allocation Counter (Intercept new/delete)

**What:** A compile-unit-local `operator new` and `operator delete` override that increments/decrements a thread-local (or global, since enjin2 is single-threaded) counter. A RAII scope guard resets the counter before a hot-path section and asserts zero at the end.

**When to use:** In CI (`alloc_check` target or inside the bench_* binaries). Not in production builds — the intercept adds per-allocation cost.

**Trade-offs:**
- Only valid in the same translation unit where the override is defined (or linked as a weak symbol). For checking `Canvas4::setPixel`, include the counter header in `bench_canvas.cpp` only.
- Does not catch allocations inside nanobench itself (nanobench does allocate internally for its stats). Gate the counter AROUND the benchmarked lambda body only, not the entire `b.run()` call.
- On ESP32, dynamic allocation already goes through a custom allocator; the intercept pattern works but must route through the same hook.

**Example:**
```cpp
// src/perf/alloc_counter.hpp
#pragma once
#include <cstdlib>
#include <cstdint>
#include <cassert>

namespace enjin2 {

struct AllocGuard {
    static uint32_t s_count;
    AllocGuard()  { s_count = 0; }
    ~AllocGuard() { assert(s_count == 0 && "hot-path allocation detected"); }
    static void onAlloc()   { ++s_count; }
    static void onDealloc() { if (s_count) --s_count; }
};

} // namespace enjin2

// In ONE .cpp file per benchmark binary:
// #define ENJIN2_ALLOC_INTERCEPT
// This replaces global new/delete with the counter versions.
#ifdef ENJIN2_ALLOC_INTERCEPT
void* operator new(std::size_t n) {
    enjin2::AllocGuard::onAlloc();
    return std::malloc(n);
}
void operator delete(void* p) noexcept {
    enjin2::AllocGuard::onDealloc();
    std::free(p);
}
#endif
```

### Pattern 5: Headless Lua Runner (tools/enjin_run)

**What:** A standalone executable that initializes `LuaScriptSystem`, stubs all graphics/input operations (or links to real `enjin2_lua` with no display output), runs a script for N frames, then exits. Supports `--profile`, `--frames N`, `--output json|text`.

**When to use:** In CI for script validation without a display. Also as the Lua profiling CLI tool. Separate from `enjin2_sdl` to avoid requiring SDL3 in CI.

**Integration point:** `enjin_run` links `enjin2_lua` (which includes `enjin2_graphics`, `enjin2_ui`, `enjin2_core`). It does NOT link SDL3. Graphics drawing calls on `LuaCanvas` write into a real `Canvas4<128,128>` in memory — they just never blit to a screen. The `input_platform_poll` stub returns all-zeros.

**CMake target:**
```cmake
# tools/CMakeLists.txt
add_executable(enjin_run tools/enjin_run.cpp src/input/input.cpp)
target_include_directories(enjin_run PRIVATE include ${LUA_INCLUDE_DIRS})
target_link_libraries(enjin_run PRIVATE enjin2_lua)
target_compile_definitions(enjin_run PRIVATE
    ENJIN2_CANVAS_WIDTH=128
    ENJIN2_CANVAS_HEIGHT=128
    $<$<BOOL:${ENJIN2_BUILD_LUA}>:ENJIN2_BUILD_LUA=1>
)
```

**`input_platform_poll` stub:** `enjin2_input` declares but does not define `input_platform_poll`. `sdl_main.cpp` provides the SDL3 definition. `enjin_run.cpp` must provide its own stub:
```cpp
namespace enjin2 {
void input_platform_poll(InputState*) { /* all zeros — no input in headless mode */ }
}
```
This is the same pattern as the ESP32 and WASM hosts, which each provide their own definition.

---

## Data Flow

### Benchmark Data Flow (Phase 1 + Phase 2)

```
Developer/CI runs: scripts/build-bench.sh
    └── cmake -B build -DENJIN2_BUILD_BENCH=ON -DENJIN2_BUILD_LUA=ON
    └── cmake --build build --target bench_canvas bench_ecs bench_lua
    └── ./build/bench_canvas --out json > bench-results/canvas.json
    └── ./build/bench_ecs   --out json > bench-results/ecs.json
    └── ./build/bench_lua   --out json > bench-results/lua.json
    └── scripts/convert-bench-json.sh bench-results/ > bench-results/combined.json

CI (benchmarks.yml):
    └── push to main / PR touching src/** or include/**
    └── build bench_* binaries
    └── run benchmarks → combined.json
    └── github-action-benchmark compares vs stored baseline on gh-pages
    └── 110% threshold: FAIL PR if any benchmark regresses > 10%
    └── On main: update baseline on gh-pages
```

### Frame Timing Data Flow (Phase 3)

```
sdl_main.cpp game loop:
    frame_start = SDL_GetTicks64()
    [input advance + poll]
    t0 = SDL_GetTicks64()
    [Lua callbacks + update() + C++ scheduler ticks]
    t1 = SDL_GetTicks64()
    [Lua draw()]
    t2 = SDL_GetTicks64()
    [g_compositor.composite()]
    t3 = SDL_GetTicks64()
    [expand_canvas_to_rgb + SDL_UpdateTexture + SDL_RenderPresent]
    t4 = SDL_GetTicks64()

    g_frameTiming.updateTime_us.store(t1 - t0)
    g_frameTiming.luaTime_us.store(t2 - t1)
    g_frameTiming.compositeTime_us.store(t3 - t2)
    g_frameTiming.renderTime_us.store(t4 - t3)
    g_frameTiming.totalFrame_us.store(t4 - frame_start)
    g_frameTiming.frameCount.fetch_add(1)

Consumers of g_frameTiming:
    - SDL runner debug overlay (engine.debug.text calls)
    - enjin_run headless: print timing table at end of N frames
    - Future WASM: read via JS SharedArrayBuffer (atomics)
```

### Lua Profiling Data Flow (Phase 4)

```
enjin_run --profile --frames 300 my_script.lua
    └── LuaScriptSystem::initialize()
    └── lua_sethook(L, profiler_hook, LUA_MASKCALL | LUA_MASKRET, 0)
    └── lua.loadScript("my_script.lua")
    └── for frame in [0..300]:
    │       tick input stub (all zeros)
    │       call Lua update(dt)
    │       call Lua draw()
    │       [profiler_hook fires on every function entry/exit]
    └── lua_sethook(L, nullptr, 0, 0)   // disable
    └── sort g_stats by totalUs descending
    └── output JSON or text table to stdout

Output (text):
    Function          Calls   Self-time(us)  % of total
    update              300       45000         62%
    draw                300       18000         25%
    engine.draw.rect   1500        4500          6%
    ...
```

### Allocation Verification Data Flow (Phase 5)

```
CI: cmake --build build --target bench_canvas
    └── bench_canvas built with ENJIN2_ALLOC_INTERCEPT=1

bench_canvas.cpp hot-path section:
    {
        AllocGuard guard;    // resets counter to 0
        canvas.setPixel(64, 32, Pixel4(3));  // must not allocate
        canvas.clear(Pixel4(0));             // must not allocate
        sprite.draw(canvas, 0, 0);           // must not allocate
    }
    // ~AllocGuard() asserts counter == 0, fails test if any allocation occurred

CI: if bench_canvas exits non-zero → FAIL build
```

---

## New vs Modified: Explicit Table

| Item | Status | File(s) |
|------|--------|---------|
| `vendor/nanobench.h` | NEW | `vendor/nanobench.h` |
| `benchmarks/` directory | NEW | `benchmarks/` |
| `benchmarks/CMakeLists.txt` | NEW | `benchmarks/CMakeLists.txt` |
| `benchmarks/bench_canvas.cpp` | NEW | `benchmarks/bench_canvas.cpp` |
| `benchmarks/bench_ecs.cpp` | NEW | `benchmarks/bench_ecs.cpp` |
| `benchmarks/bench_lua.cpp` | NEW | `benchmarks/bench_lua.cpp` |
| `src/perf/frame_timing.hpp` | NEW | `src/perf/frame_timing.hpp` |
| `src/perf/alloc_counter.hpp` | NEW | `src/perf/alloc_counter.hpp` |
| `tools/enjin_run.cpp` | NEW | `tools/enjin_run.cpp` |
| `tools/CMakeLists.txt` | NEW | `tools/CMakeLists.txt` |
| `scripts/build-bench.sh` | NEW | `scripts/build-bench.sh` |
| `scripts/convert-bench-json.sh` | NEW | `scripts/convert-bench-json.sh` |
| `.github/workflows/benchmarks.yml` | NEW | `.github/workflows/benchmarks.yml` |
| `docs/PERFORMANCE.md` | NEW | `docs/PERFORMANCE.md` |
| `bench-results/` directory | NEW | `bench-results/` (gitignored) |
| `CMakeLists.txt` | MODIFIED | `CMakeLists.txt` — add `ENJIN2_BUILD_BENCH` option + `add_subdirectory(benchmarks)` + `add_subdirectory(tools)` |
| `src/platform/sdl/sdl_main.cpp` | MODIFIED | `src/platform/sdl/sdl_main.cpp` — add 6 timing capture points; include `src/perf/frame_timing.hpp`; define `g_frameTiming` |
| `src/scripting/bindings_engine.cpp` or new `bindings_perf.cpp` | MODIFIED or NEW | Add `engine.perf.*` Lua sub-table for reading `g_frameTiming` from Lua scripts |
| `.gitignore` | MODIFIED | Add `bench-results/` entry |
| `examples/CMakeLists.txt` | MODIFIED | Remove old ad-hoc benchmark targets (or mark DISABLED) that `nanobench` replaces |

---

## Integration Boundaries

### CMake Target Dependency Graph (new)

```
enjin2_core
    └── enjin2_graphics
            └── enjin2_ui
                    └── enjin2_lua
                            ├── bench_canvas    (links enjin2_lua; ENJIN2_BUILD_BENCH guard)
                            ├── bench_ecs       (links enjin2_lua; ENJIN2_BUILD_BENCH guard)
                            ├── bench_lua       (links enjin2_lua; ENJIN2_BUILD_BENCH guard)
                            └── enjin_run       (links enjin2_lua; ENJIN2_BUILD_TOOLS guard)
                    └── enjin2_sdl              (links enjin2_lua + SDL3; ENJIN2_BUILD_SDL guard)
                            └── (frame timing instrumentation lives here)
```

Benchmarks link `enjin2_lua` (not the INTERFACE `enjin2` target) to avoid dragging in SDL3. The INTERFACE target `enjin2` bundles all four static libs — safe to use but benchmarks only need the Lua stack.

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `sdl_main.cpp` ↔ `frame_timing.hpp` | Direct struct write via `std::atomic::store()` | `g_frameTiming` declared `extern` in `frame_timing.hpp`; defined in `sdl_main.cpp` |
| `bench_lua.cpp` ↔ `LuaEngine` | `LuaEngine::getState()` exposed; profiler calls `lua_sethook` directly | No LuaEngine modification needed; `getState()` already public |
| `bench_ecs.cpp` ↔ `Scene`/`Object` | Direct construction of `Scene` subclasses with `addObject<T>()` | Same pattern as existing tests in `tests/`; no test framework needed |
| `enjin_run.cpp` ↔ `enjin2_input` | Provides `input_platform_poll` stub (same pattern as SDL runner and ESP32/WASM hosts) | One-liner stub; exactly one definition per TU |
| `alloc_counter.hpp` ↔ bench_*.cpp | `#define ENJIN2_ALLOC_INTERCEPT` in bench TU; global `operator new`/`delete` override | Only active in benchmark builds; zero impact on engine or test builds |
| `nanobench.h` ↔ bench_*.cpp | `#define ANKERL_NANOBENCH_IMPLEMENT` in exactly one TU per binary | Single-header pattern identical to `stb_image_write.h` usage in existing examples |
| CI workflow ↔ `bench-results/` | `actions/upload-artifact` + `github-action-benchmark` action reads JSON | JSON schema must match nanobench's `--out json` format (or converted by `convert-bench-json.sh`) |
| `examples/` benchmark targets | The old `standalone_benchmark`, `packing_overhead_benchmark`, etc. in `examples/CMakeLists.txt` use raw `std::chrono` and are ad-hoc | These are NOT removed from `examples/CMakeLists.txt` — they can be disabled/commented to keep git history clean. New `benchmarks/` are the CI-canonical suite. |

---

## Suggested Build Order

Dependencies are strict between phases. Phases with the same group number can be developed in parallel.

```
Group 1 — Foundation (no deps, unblock everything else)
  Phase 1a: Vendor nanobench.h into vendor/
  Phase 1b: Add ENJIN2_BUILD_BENCH CMake option + benchmarks/ subdirectory stub
  Phase 1c: Add ENJIN2_BUILD_TOOLS CMake option + tools/ subdirectory stub
  Rationale: CMake changes must land before any bench_* or enjin_run targets exist.

Group 2 — Benchmark Suite (depends on Group 1)
  Phase 2a: bench_canvas.cpp — canvas pixel ops, fill, composite
  Phase 2b: bench_ecs.cpp — Object/Scene/Component scaling
  Phase 2c: bench_lua.cpp — LuaEngine init, script load, binding call, GC
  Rationale: Each bench_* file is independent; order doesn't matter within group.

Group 3 — CI Pipeline (depends on Group 2)
  Phase 3a: scripts/build-bench.sh — build + run + JSON output
  Phase 3b: scripts/convert-bench-json.sh — combine JSON files
  Phase 3c: .github/workflows/benchmarks.yml — push/PR trigger, 110% threshold
  Rationale: CI needs working binaries and scripts before it can compare baselines.

Group 4 — Frame Timing (depends on Group 1; parallel with Group 2)
  Phase 4a: src/perf/frame_timing.hpp — FrameTimingInstrumentation struct
  Phase 4b: src/platform/sdl/sdl_main.cpp — insert 6 timing capture points
  Rationale: Timing struct is self-contained; sdl_main modification needs struct first.

Group 5 — Lua Profiling (depends on Group 1; parallel with Groups 2/4)
  Phase 5a: tools/enjin_run.cpp — headless runner with stub input_platform_poll
  Phase 5b: Add --profile flag, lua_sethook hook, per-function stat accumulation
  Phase 5c: Add --frames N, --output json|text, print sorted stat table
  Rationale: enjin_run can be built first as bare runner, profiling added incrementally.

Group 6 — Allocation Verification (depends on Group 2 bench binaries)
  Phase 6a: src/perf/alloc_counter.hpp — AllocGuard + operator new/delete intercept
  Phase 6b: Integrate AllocGuard into bench_canvas.cpp hot-path sections
  Phase 6c: Add CI step: run bench_canvas with intercept; fail on non-zero exit
  Rationale: Needs bench binaries to exist; the alloc intercept is additive.

Group 7 — Documentation (no deps; parallel with everything)
  Phase 7a: docs/PERFORMANCE.md — quick-start + subsystem reference
  Rationale: Documentation can be written against the plan before code ships.
```

**Summary table:**

| Group | Phases | Blocks |
|-------|--------|--------|
| 1 — Foundation | CMake options + stub subdirectories | Groups 2, 3, 4, 5, 6 |
| 2 — Benchmark suite | bench_canvas, bench_ecs, bench_lua | Group 3, 6 |
| 3 — CI pipeline | build-bench.sh, convert script, yml | Nothing (terminal) |
| 4 — Frame timing | frame_timing.hpp + sdl_main.cpp | Nothing (terminal) |
| 5 — Lua profiling | enjin_run + --profile | Nothing (terminal) |
| 6 — Alloc verification | alloc_counter.hpp + bench integration | Group 3 (CI step) |
| 7 — Documentation | PERFORMANCE.md | Nothing (terminal) |

---

## Anti-Patterns

### Anti-Pattern 1: Putting Benchmarks in `tests/CMakeLists.txt`

**What people do:** Add `bench_canvas` as a new executable in the existing `tests/CMakeLists.txt` alongside CTest targets.

**Why it is wrong:** CTest runs all registered tests on every CI push. nanobench binaries take 10–30 seconds each and produce variable timing output. Mixing them into CTest breaks fast test feedback and produces spurious CI "failures" when wall-clock variance triggers 110% thresholds.

**Do this instead:** Separate `benchmarks/CMakeLists.txt` with its own `ENJIN2_BUILD_BENCH` CMake option. Benchmark runs are a distinct CI job triggered only on relevant file changes, not on every commit.

### Anti-Pattern 2: Modifying LuaEngine to Add Profiling Hooks

**What people do:** Add a `startProfiling()` / `stopProfiling()` method to `LuaEngine` with internal state for the hook.

**Why it is wrong:** Adds instrumentation code to the production `enjin2_lua` library; bakes profiling into every build target including ESP32 and WASM; violates the "zero overhead when disabled" requirement.

**Do this instead:** Call `lua_sethook` directly on `LuaEngine::getState()` from outside the engine — in `enjin_run.cpp`. The `getState()` accessor is already public. The profiler is entirely in the tool layer, not the engine layer.

### Anti-Pattern 3: Using `SDL_GetTicks` (32-bit, 49-day overflow) for Frame Timing

**What people do:** Reuse the existing `SDL_GetTicks()` call (used for frame pacing) to capture phase timestamps.

**Why it is wrong:** `SDL_GetTicks()` returns `Uint32` milliseconds — millisecond resolution is too coarse for per-phase timing (Lua draw() on a 30fps frame is ~16ms total; sub-phases are ~1–5ms). `SDL_GetTicks()` also overflows after 49 days.

**Do this instead:** Use `SDL_GetTicks64()` for millisecond counts, or `std::chrono::steady_clock::now()` for microsecond precision. The frame timing struct stores microseconds (`uint32_t`; wraps after ~71 minutes which is fine for per-frame deltas).

### Anti-Pattern 4: Defining `operator new` Override in a Header

**What people do:** Put the `operator new` / `operator delete` intercept in `alloc_counter.hpp` unconditionally.

**Why it is wrong:** If the header is included in multiple translation units that are linked together, the linker sees multiple definitions of `operator new` and either errors or silently picks one. This breaks any binary that includes the header without intending to intercept allocations.

**Do this instead:** The override lives behind `#ifdef ENJIN2_ALLOC_INTERCEPT` and is documented as "define this in exactly one .cpp per binary." The `AllocGuard` struct and counter are always available; only the intercept requires the define.

### Anti-Pattern 5: Separate `enjin2_bench` Static Library

**What people do:** Create an `enjin2_bench` static library containing `FrameTimingInstrumentation` and `alloc_counter`, then link it into all targets.

**Why it is wrong:** `FrameTimingInstrumentation` and `alloc_counter` are header-only with no external dependencies. A static library adds unnecessary CMake complexity and link overhead. The codebase pattern (`layer_compositor.hpp`, `input_state.hpp`) is header-only for infrastructure structs.

**Do this instead:** `src/perf/frame_timing.hpp` and `src/perf/alloc_counter.hpp` are plain headers. Targets that need them add the `src/` include path (already available in `enjin2_lua` and `enjin2_sdl`). The `g_frameTiming` global is defined in `sdl_main.cpp` and `enjin_run.cpp` respectively — not in a shared library.

---

## Confidence Assessment

| Area | Confidence | Basis |
|------|------------|-------|
| nanobench integration pattern | HIGH | Pattern matches existing `stb_image_write.h` vendor in this codebase; nanobench single-header is well-documented |
| `lua_sethook` profiler correctness | HIGH | Lua 5.4 manual; same pattern used in multiple embedded Lua profilers; `getState()` is already public |
| `input_platform_poll` stub for headless | HIGH | Pattern verified from three existing definitions: SDL runner, WASM, ESP32 — all provide their own definition |
| `std::atomic` on ESP32/Emscripten | MEDIUM | Works on both; 64-bit atomics may need `libatomic` on Xtensa; use `uint32_t` to be safe |
| CI github-action-benchmark integration | MEDIUM | Action is well-documented; nanobench JSON output format matches expected schema; convert script may need one iteration |
| `SDL_GetTicks64()` microsecond precision | MEDIUM | SDL_GetTicks64 is millisecond resolution; may need `std::chrono` for sub-ms phases; frame-level timing is fine at ms resolution |
| Allocation intercept via global `operator new` | MEDIUM | Works reliably in single-TU benchmarks; behavior in presence of system libraries that also override `new` needs verification |
| Removal of old ad-hoc benchmarks from `examples/` | HIGH | These are confirmed present in `examples/CMakeLists.txt`; disabling them is mechanical |

---

## Sources

- `/home/unwn/git/enjin/CMakeLists.txt` — existing CMake targets, options, guard patterns (HIGH)
- `/home/unwn/git/enjin/src/platform/sdl/sdl_main.cpp` — game loop structure, existing tick points, SDL_GetTicks usage (HIGH)
- `/home/unwn/git/enjin/include/enjin2/scripting/bindings.hpp` — LuaBindings, LuaScriptSystem, getState() access (HIGH)
- `/home/unwn/git/enjin/include/enjin2/scripting/lua_engine.hpp` — LuaEngine::getState() public accessor (HIGH)
- `/home/unwn/git/enjin/include/enjin2/graphics/layer_compositor.hpp` — header-only struct pattern (HIGH)
- `/home/unwn/git/enjin/include/enjin2/core/memory.hpp` — StaticPool header-only pattern (HIGH)
- `/home/unwn/git/enjin/tests/CMakeLists.txt` — existing test target pattern; bench targets must not contaminate this (HIGH)
- `/home/unwn/git/enjin/examples/CMakeLists.txt` — ad-hoc benchmark targets to be superseded (HIGH)
- `/home/unwn/git/enjin/examples/packing_overhead_benchmark.cpp` — ad-hoc chrono-based benchmark; shows what nanobench replaces (HIGH)
- `/home/unwn/git/enjin/vendor/` — existing single-header vendor pattern (HIGH)
- `/home/unwn/git/enjin/.planning/PROJECT.md` — v1.10 target features and constraints (HIGH)
- `/home/unwn/git/enjin/project/benchmarking-milestone.md` — phase-by-phase requirements (HIGH)
- [nanobench README](https://github.com/martinus/nanobench) — single-header integration, JSON output format, `doNotOptimizeAway` (MEDIUM — training data; verify current API)
- [Lua 5.4 manual — lua_sethook](https://www.lua.org/manual/5.4/manual.html#lua_sethook) — hook mask constants, `lua_Debug` fields (HIGH)

---

*Architecture research for: enjin2 v1.10 benchmarking & performance infrastructure*
*Researched: 2026-03-07*
