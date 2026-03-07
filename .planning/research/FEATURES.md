# Feature Research

**Domain:** C++ game engine benchmarking & performance infrastructure — v1.10 Benchmarking & Performance
**Researched:** 2026-03-07
**Confidence:** HIGH (milestone brief is explicit; existing codebase well-understood; domain patterns well-established)

---

## Existing Baseline (Already Built — Do Not Rebuild)

These exist in enjin2 as of v1.9. They are **inputs** to v1.10 features, not deliverables.

| Already Built | Relevance to v1.10 |
|---------------|-------------------|
| Ad-hoc `std::chrono` benchmarks in `/examples/` (comprehensive_benchmark.cpp, standalone_benchmark.cpp, etc.) | These are the ad-hoc patterns being replaced; some logic may be salvaged as nanobench test cases |
| `enjin2_core`, `enjin2_graphics`, `enjin2_lua` CMake static library targets | Benchmark binaries link against these; existing CMake structure must be respected |
| `ENJIN2_BUILD_LUA` CMake flag pattern | Benchmark targets need an analogous `ENJIN2_BUILD_BENCHMARKS=ON/OFF` guard |
| `.github/workflows/docs.yml` CI workflow | The benchmark CI workflow follows this as a structural pattern |
| `engine.lua.collect()` / `engine.lua.memory()` Lua bindings | Lua memory tracking during profiling reuses these hooks |
| `LuaEngine` / `LuaScriptSystem` with `lua_sethook`-compatible Lua 5.4 state | Lua profiler hooks directly into the existing Lua state; no new scripting layer needed |
| `vendor/` directory with `stb_image.h`, `stb_image_write.h` | nanobench goes here as `vendor/nanobench.h` — same single-header pattern already established |
| `scripts/` directory with `setup-dev.sh`, `build.sh` | New scripts (`build-bench.sh`) follow the same directory and style |

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features that a "serious" benchmarking milestone must deliver. Missing these makes the milestone feel incomplete.

| Feature | Why Expected | Complexity | Dependencies on Existing |
|---------|--------------|------------|--------------------------|
| **nanobench integration** (single header in `vendor/`) | The de-facto standard for C++ microbenchmarks: accurate, fast compile, zero heap in hot path. Ad-hoc chrono code produces statistically unreliable results. | LOW — one header, one CMake target, one `#define ANKERL_NANOBENCH_IMPLEMENT` translation unit | `vendor/` directory exists; CMake library structure known |
| **bench_canvas binary** — Canvas4/Canvas8 pixel ops, fill, blit, composite | Canvas is the hottest path; 128x64 pixel operations at 60fps on ESP32 must be quantified | MEDIUM — needs proper benchmark fixture, not just a main() | `enjin2_graphics` CMake target; Canvas4/Canvas8 APIs |
| **bench_ecs binary** — Object create/destroy, component update loop, scene::update() scaling (1/8/16/32/48 objects) | ECS update is called every frame; scaling behavior must be documented before claiming "suitable for ESP32" | MEDIUM — scene must be instantiated with controlled object counts | `enjin2_core` CMake target; `Object`, `Scene`, `SceneStateMachine` |
| **bench_lua binary** — Lua init, script load, binding call overhead, ObjectProxy round-trip | Lua is the user-facing API; binding call cost is invisible to users but dominates scripting perf | HIGH — requires Lua state + bindings wired without SDL3 platform dependencies | `enjin2_lua`; LuaEngine; all binding modules; conditional compilation |
| **JSON output from all benchmarks** | github-action-benchmark requires machine-readable JSON. Ad-hoc text output cannot feed CI regression graphs. | LOW — `bench.render(ankerl::nanobench::templates::json(), ...)` is a single call | nanobench integration |
| **`scripts/build-bench.sh`** — builds and runs all benchmarks in sequence, saves JSON to `bench-results/` | Developer workflow: one command to run all benchmarks. Matches existing `build.sh` pattern. | LOW — shell script; mirrors build.sh structure | CMake bench targets |
| **CI workflow** (`.github/workflows/benchmarks.yml`) — trigger on push to main and PRs touching `src/**` or `include/**` | Regression detection is only useful if it runs automatically. Manual runs are forgotten. | MEDIUM — GitHub Actions YAML; needs correct trigger paths | Existing `docs.yml` as structural template |
| **Regression detection at 110% threshold** via `github-action-benchmark` | "10% slower than baseline = CI fail on PR" is the industry standard threshold. Without it, regressions ship silently. | LOW — Action config option (`alertThreshold: '110%'`, `failOnAlert: true`) | CI workflow; JSON results |
| **Performance dashboard on gh-pages** — auto-generated from benchmark history | Without a dashboard, benchmark history is unreadable JSON files. Contributors need a visual trend. | MEDIUM — gh-pages branch management; `github-action-benchmark` handles rendering | CI workflow; bench JSON |

### Differentiators (Enjin-Specific Value)

Features beyond a generic benchmark suite. These address enjin2's unique constraints.

| Feature | Value Proposition | Complexity | Dependencies on Existing |
|---------|-------------------|------------|--------------------------|
| **Frame timing instrumentation** — lock-free atomics struct tracking `updateTime_us`, `renderTime_us`, `luaTime_us`, `compositeTime_us` per frame | Frame budget is enjin2's primary runtime concern (16.6ms at 60fps). The ad-hoc debug overlay shows FPS but no per-phase breakdown. With per-phase timing, authors know whether Lua or rendering is the budget hog. | HIGH — lock-free atomics, cache-line alignment, platform-specific measurement (POSIX `clock_gettime` on Linux, `esp_timer_get_time()` on ESP32) | SDL3 runner game loop; existing `engine.debug.*` overlay; ESP32 FreeRTOS game loop |
| **Lua profiler via `lua_sethook`** — per-function call counts and self-time, `LUA_MASKCALL | LUA_MASKRET` | Lua scripting perf is opaque to users. "My script is slow" has no diagnosis tool today. The profiler tells users exactly which `engine.*` bindings they're calling too frequently. | HIGH — C-level hook function; per-function hash map (fixed-size static array for zero-alloc); timing via `clock_gettime`; zero overhead when disabled via `lua_sethook(L, NULL, 0, 0)` | `LuaEngine` Lua state; `lua_Debug` struct (`what`, `short_src`, `linedefined` fields) |
| **Headless CLI runner** (`enjin_run --profile --frames N script.lua`) | CI can validate that a Lua script runs without crashing in N frames. Authors can profile scripts without the SDL3 window. All platform APIs (gfx, input) are stubbed as no-ops. | HIGH — new executable; stub implementations of `ICanvas`, input, SDL3; `lua_sethook` profiler attached optionally | `enjin2_lua`; LuaScriptSystem; existing binding structure |
| **Lua GC pressure tracking** — ring buffer of per-frame `lua_gc(L, LUA_GCCOUNT, 0)` readings | GC spikes cause frame drops on embedded targets. Today there is no way to see when GC fires relative to frame boundaries. | MEDIUM — ring buffer of fixed size (e.g., 64 frames); `engine.lua.memory()` already exists as a Lua binding but is not surfaced over time | `engine.lua.memory()` binding; SDL3 runner frame loop |
| **Static allocation verification** — custom allocator wrapper counting `malloc`/`free` during hot-path sections | enjin2's core value proposition is "zero dynamic allocation." This is today an assertion without a proof. The verifier provides CI evidence. | HIGH — Linux-specific `malloc_count` or overriding `operator new`; run under allocator counter in CI benchmark job; fail if hot-path allocation detected | bench_canvas/bench_ecs binaries; nanobench `doNotOptimizeAway()` pattern |
| **Per-platform frame budget documentation** — ESP32 vs WASM vs SDL3 targets with concrete numbers | "How fast is enjin2?" is unanswerable today. After benchmarking, the docs cite measured nanoseconds-per-op and translate to frame budget percentages. | LOW-MEDIUM — documentation work once benchmarks produce numbers | bench_canvas, bench_ecs, bench_lua results; frame timing instrumentation results |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic for Enjin2 | Better Approach |
|---------|---------------|---------------------------|-----------------|
| **Google Benchmark instead of nanobench** | More widely known; used at Google scale | Heavy dependency; slower compilation; JSON output format differs from github-action-benchmark `customSmallerIsBetter`; nanobench is already used in the tomodachi codebase this milestone models after | Use nanobench. Single header. Zero config. Proven JSON output path. |
| **Valgrind/ASan for allocation verification** | Standard memory safety tools | ASan instruments every allocation but cannot draw the "hot path vs. allowed cold path" boundary that enjin2 needs. Valgrind is too slow for benchmarking. | Custom `operator new` counter or `malloc_count` approach: count allocations only during the specific benchmark window, fail if count > 0 |
| **Lua-level profiler via `debug.sethook`** | Seems easier to implement in pure Lua | "The overhead of a Lua call for each hook is too high and usually invalidates any measure" (Lua PIL 23.3). Self-time measurements are corrupted by hook overhead. | C-level `lua_sethook` with `lua_Debug` struct and `clock_gettime()`. Zero Lua overhead on the measured path. |
| **LuaJIT for benchmarking** | Much faster than Lua 5.4 | LuaJIT does not support WASM or ESP32 (no WASM backend, no ARM cross-compile for standard ESP32-S3). enjin2 already vendor-ships Lua 5.4.8 for these targets. Benchmarking under LuaJIT measures a different runtime. | Benchmark against system Lua 5.4 on desktop. This is what ships on the target platforms. |
| **Tracy Profiler integration** | Industry-standard C++ game profiler with GUI | Tracy requires a persistent background thread and network connection for live profiling. This violates enjin2's zero-dynamic-allocation, zero-threading model. Overkill for a 128x64 pixel engine. | Lightweight `FrameTimingInstrumentation` struct with atomics. No extra threads, no network, no heap. |
| **Benchmark on WASM or ESP32 targets** | "Real" platform numbers | Cross-platform benchmarking requires Emscripten or ESP-IDF toolchain in CI, dramatically increasing CI complexity. Native desktop benchmarks with known hardware are reproducible and comparable over time. | All benchmarks run on Linux x86-64 desktop. Performance is characterized on embedded targets via frame timing instrumentation (runtime, not CI). |
| **Per-entity-per-component microbenchmark** | Seems thorough | enjin2's ECS is statically allocated with 16 components max per object. The interesting metric is `scene::update()` scaling, not individual component access. Over-granular benchmarks add maintenance burden without insight. | Benchmark at the scene::update() level with controlled object counts (1/8/16/32/48). Match real game loop topology. |
| **Runtime toggling of timing via Lua scripts** | Authors want `engine.perf.start()` / `engine.perf.stop()` | Lua binding overhead would pollute the measurement. Toggle should happen at C++ level via a compile-time `#define ENJIN2_FRAME_TIMING=1` or a CMake option. | Frame timing is always-on in `ENJIN2_FRAME_TIMING` builds and always-off in normal builds. No Lua exposure needed. |

---

## Feature Dependencies

```
[nanobench vendor header]
    └──required by──> [bench_canvas binary]
    └──required by──> [bench_ecs binary]
    └──required by──> [bench_lua binary]
                          └──required by──> [JSON output to bench-results/]
                                                └──required by──> [CI workflow benchmarks.yml]
                                                                      └──required by──> [Regression detection 110%]
                                                                      └──required by──> [gh-pages dashboard]

[enjin2_lua existing target]
    └──required by──> [bench_lua binary]
    └──required by──> [headless CLI runner enjin_run]
    └──required by──> [Lua profiler lua_sethook]
                          └──enhances──> [headless CLI runner enjin_run]

[SDL3 runner game loop]
    └──required by──> [Frame timing instrumentation SDL3 path]
    └──enhances──> [Lua GC pressure ring buffer]

[enjin2_core existing target]
    └──required by──> [bench_ecs binary]

[enjin2_graphics existing target]
    └──required by──> [bench_canvas binary]

[bench_canvas / bench_ecs binaries]
    └──required by──> [Static allocation verification]

[Frame timing instrumentation]
    └──feeds data to──> [Per-platform frame budget documentation]
```

### Dependency Notes

- **bench_lua requires headless stub implementations:** The Lua bindings call into graphics, input, and debug APIs. Without stubs, the binary cannot link without SDL3. Stubs must be created before bench_lua or enjin_run can build.
- **Static allocation verification requires bench binaries first:** The verifier wraps existing benchmark runs; it cannot be built until the benchmarks themselves work correctly.
- **CI workflow requires JSON output to exist:** If bench output format changes, the CI workflow breaks. Lock the JSON schema early (nanobench `templates::json()` is stable).
- **gh-pages dashboard requires CI workflow to run successfully at least once:** The dashboard data is populated by the first successful CI run. Bootstrap order matters.
- **Lua profiler conflicts with accurate Lua benchmark timing:** Do not enable `lua_sethook` during `bench_lua` runs. The profiler adds hook overhead that distorts call cost measurements. They are separate tools for separate use cases.

---

## MVP Definition

### Launch With (v1.10 — this milestone)

These are the features the milestone brief explicitly commits to.

- [ ] **nanobench vendored** as `vendor/nanobench.h` — enables all benchmark binaries
- [ ] **bench_canvas** — Canvas4/Canvas8 pixel ops, fill, rect, circle, sprite blit, multi-layer composite
- [ ] **bench_ecs** — Object create, component attach, `scene::update()` at 1/8/16/32/48 objects, event dispatch
- [ ] **bench_lua** — Lua init, `loadScript()`, per-module binding call, ObjectProxy round-trip, GC cycle
- [ ] **JSON output** from all three benchmarks to `bench-results/`
- [ ] **`scripts/build-bench.sh`** — one-command build+run for all benchmarks
- [ ] **CI workflow** `.github/workflows/benchmarks.yml` — triggered on push to main and PRs touching `src/**`/`include/**`
- [ ] **Regression detection** at 110% threshold with `failOnAlert: true` for PRs
- [ ] **gh-pages dashboard** — benchmark history visualization
- [ ] **Frame timing instrumentation** — `FrameTimingInstrumentation` struct, `updateTime_us`, `renderTime_us`, `luaTime_us`, `compositeTime_us` — SDL3 runner integration
- [ ] **Lua profiler** — `lua_sethook(LUA_MASKCALL | LUA_MASKRET)`, per-function call counts and self-time, zero overhead when disabled
- [ ] **Headless CLI runner** `enjin_run` — `--profile`, `--frames N`, JSON+text output, stubbed platform APIs
- [ ] **Static allocation verification** — malloc/free counter wrapping hot-path benchmark sections, CI fails on any hot-path allocation
- [ ] **`docs/PERFORMANCE.md`** — subsystem performance guide, quick-start commands, adding-benchmarks guide, per-platform frame budget reference

### Add After Validation (v1.x)

Defer these until post-v1.10 if time is constrained.

- [ ] **WASM-specific perf numbers** — frame timing measurements in the WASM target using Emscripten performance APIs (adds CI complexity)
- [ ] **ESP32-specific perf numbers** — `esp_timer_get_time()` frame timing instrumented into FreeRTOS game loop (requires hardware or QEMU)
- [ ] **Lua profiler SDL3 overlay panel** — live profiler data overlaid on debug canvas (nice UX; not needed for CI validation)
- [ ] **Per-scene benchmark mode** — run a Lua script for N frames and profile it end-to-end (extends enjin_run; useful for game-level profiling)

### Future Consideration (v2+)

- [ ] **Tracy Profiler integration** — only worth it if enjin2 scales to multi-threaded rendering; conflicts with current zero-threading model
- [ ] **LuaJIT on desktop** — only if enjin2 adds a desktop-only mode that doesn't need WASM/ESP32 parity
- [ ] **Automated benchmark hardware matrix** — cross-platform CI runners (ARM, WASM) for multi-target benchmark comparison

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| nanobench vendor + 3 benchmark binaries | HIGH — proves engine is measured, not guessed | LOW-MEDIUM | P1 |
| JSON output + `build-bench.sh` | HIGH — enables CI workflow | LOW | P1 |
| CI workflow + 110% regression detection | HIGH — prevents perf regressions from shipping | MEDIUM | P1 |
| gh-pages dashboard | MEDIUM — visual trend; not blocking | MEDIUM | P1 |
| Frame timing instrumentation (SDL3) | HIGH — answers "where is my frame budget going?" | HIGH | P1 |
| Lua profiler via `lua_sethook` | HIGH — answers "which bindings are slow?" | HIGH | P1 |
| Headless CLI runner `enjin_run` | HIGH — enables CI script validation; key for Lua profiling workflow | HIGH | P1 |
| Static allocation verification | HIGH — proves zero-alloc guarantee (enjin2's core claim) | HIGH | P1 |
| `docs/PERFORMANCE.md` | MEDIUM — without docs, benchmark infra is unusable by contributors | LOW-MEDIUM | P1 |
| Lua GC pressure ring buffer | MEDIUM — useful but secondary to call-count profiling | MEDIUM | P2 |
| ESP32 frame timing integration | MEDIUM — real target; blocked by hardware in CI | HIGH | P2 |
| Profiler SDL3 overlay panel | LOW — nice UX; enjin_run CLI is sufficient | MEDIUM | P3 |

**Priority key:**
- P1: Must have for v1.10 milestone
- P2: Should have, add when P1 is stable
- P3: Nice to have, future milestone

---

## Implementation Notes by Feature Area

### nanobench Setup Pattern

```cpp
// bench_impl.cpp — ONE translation unit only
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

// bench_canvas.cpp, bench_ecs.cpp, bench_lua.cpp — include header only
#include <nanobench.h>
```

CMake: add `bench_canvas`, `bench_ecs`, `bench_lua` executables under `if(ENJIN2_BUILD_BENCHMARKS)` guard. Link against respective engine targets. Do NOT link against SDL3 (use stubs for bench_lua).

JSON output pattern:
```cpp
ankerl::nanobench::Bench bench;
bench.run("canvas fill 128x64", [&] { canvas.fill(0); });
bench.render(ankerl::nanobench::templates::json(), outStream);
```

### Lua Profiler Hook Pattern

```c
// C-level hook — called at LUA_MASKCALL and LUA_MASKRET events
static void lua_profiler_hook(lua_State* L, lua_Debug* ar) {
    lua_getinfo(L, "nS", ar);   // populate ar->what, ar->short_src, ar->linedefined
    // ar->event == LUA_HOOKCALL  -> record entry timestamp
    // ar->event == LUA_HOOKRET   -> compute self_time = now - entry
}

// Enable:  lua_sethook(L, lua_profiler_hook, LUA_MASKCALL | LUA_MASKRET, 0);
// Disable: lua_sethook(L, NULL, 0, 0);
```

Key `lua_Debug` fields for profiling: `event` (CALL/RET), `what` ("Lua"/"C"/"main"), `short_src` (file path), `linedefined`, `name`.

Per-function tracking must use fixed-size static array (no `std::unordered_map` — violates zero-alloc). Hash function maps `(short_src, linedefined)` to slot index. Max 64 tracked functions is sufficient for typical Lua scripts.

### Static Allocation Verification Pattern

```cpp
// In benchmark translation unit — Linux only
extern "C" {
    static size_t g_alloc_count = 0;
    void* malloc(size_t size) { ++g_alloc_count; return __libc_malloc(size); }
}

void verify_zero_alloc(const char* section_name, auto fn) {
    g_alloc_count = 0;
    fn();
    assert(g_alloc_count == 0 && "Hot path allocated!");
}
```

Alternative (portable): link against `malloc_count` and use its counter API. The key is: reset counter before the section, assert count == 0 after.

### Frame Timing Instrumentation Pattern

```cpp
struct alignas(64) FrameTimingInstrumentation {
    std::atomic<uint32_t> frameCount{0};
    std::atomic<uint32_t> updateTime_us{0};
    std::atomic<uint32_t> renderTime_us{0};
    std::atomic<uint32_t> luaTime_us{0};
    std::atomic<uint32_t> compositeTime_us{0};
};
```

Store timing measurements with `store(relaxed)` — no cross-thread synchronization needed for display-only reads. `alignas(64)` prevents false sharing if ever read from a display thread.

Platform measurement: `clock_gettime(CLOCK_MONOTONIC, &ts)` on Linux/WASM; `esp_timer_get_time()` on ESP32.

---

## Sources

- [nanobench documentation — tutorial and API reference](https://nanobench.ankerl.com/tutorial.html) (HIGH confidence — official docs)
- [nanobench GitHub repository](https://github.com/martinus/nanobench) (HIGH confidence — official source)
- [github-action-benchmark — Continuous Benchmark GitHub Action](https://github.com/benchmark-action/github-action-benchmark) (HIGH confidence — official source)
- [Lua 5.4 reference manual — debug interface](https://www.lua.org/manual/5.4/) (HIGH confidence — official Lua docs)
- [Lua PIL 23.3 — Profiling via hooks](https://www.lua.org/pil/23.3.html) (HIGH confidence — authoritative Lua book)
- [Debugging and profiling Lua — martin-fieber.de](https://martin-fieber.de/blog/debugging-and-profiling-lua/) (MEDIUM confidence — verified blog post)
- [ECS benchmark frameworks — abeimler/ecs_benchmark](https://github.com/abeimler/ecs_benchmark) (MEDIUM confidence — community reference)
- [malloc_count allocation tracking — panthema.net](https://panthema.net/2013/malloc_count/) (MEDIUM confidence — established tool, single source)
- enjin2 codebase: `/home/unwn/git/enjin/` — v1.9 source (HIGH confidence — direct inspection)
- enjin2 milestone brief: `/home/unwn/git/enjin/project/benchmarking-milestone.md` (HIGH confidence — direct read)

---

*Feature research for: C++ game engine benchmarking & performance infrastructure (enjin2 v1.10)*
*Researched: 2026-03-07*
