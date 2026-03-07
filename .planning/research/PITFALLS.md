# Pitfalls Research

**Domain:** Adding benchmarking infrastructure, CI regression detection, frame timing instrumentation, Lua profiling, and static allocation verification to enjin2 — a zero-alloc 2D embedded game engine
**Researched:** 2026-03-07
**Confidence:** HIGH (direct codebase analysis of enjin2 v1.9 sources, nanobench official docs, github-action-benchmark documentation, Lua 5.4 reference manual, ESP-IDF documentation, community post-mortems and issue trackers)

---

## Critical Pitfalls

### Pitfall 1: nanobench Benchmarks Measure Allocations Caused by Object and LuaEngine Construction

**What goes wrong:**
enjin2's `Object` class uses `std::array<std::unique_ptr<Component>, 16>`. Constructing Objects or attaching Components inside a nanobench lambda causes heap allocation via `std::make_unique`, which invalidates any "zero-alloc" timing claim for the ECS benchmark. The benchmark measures allocator overhead, not ECS logic. Results will look slower than reality and will vary between runs based on allocator state. The same problem applies to `LuaEngine::initialize()`, which calls `new char[MEMORY_LIMIT]` on desktop and `heap_caps_malloc` on ESP32 — calling it inside a benchmark loop measures malloc, not Lua init.

**Why it happens:**
Benchmark authors treat the `ankerl::nanobench::Bench::run()` lambda as a pure measurement zone and forget that the setup work (constructing objects, initializing Lua state) must be hoisted outside the lambda. For ECS benchmarks specifically, the natural coding pattern — "create an Object, attach components, call update" — puts all three steps inside the loop when only "call update" should be measured.

**How to avoid:**
- Hoist all construction out of the nanobench lambda. Construct Objects and initialize `LuaScriptSystem` in the benchmark fixture, before calling `bench.run()`.
- For benchmarks that specifically test Object construction (a valid case), call `ankerl::nanobench::doNotOptimizeAway()` on the constructed object to prevent dead code elimination, and accept that the result includes allocation overhead — document this explicitly.
- For bench_lua, call `LuaScriptSystem::initialize()` once per benchmark binary, not inside any `bench.run()` lambda.
- Never call `lua_close(L)` and `lua_newstate()` inside a nanobench loop unless the benchmark is explicitly titled "Lua state init overhead."

**Warning signs:**
- ECS benchmark reports 200–1000ns for simple update loops (allocation overhead is ~50–200ns per `unique_ptr` on a cold allocator)
- High variance (nanobench reports "severe outliers") in benchmarks that construct objects
- bench_lua results show 5–50ms for "script call overhead" — indicates Lua state is being recreated per iteration

**Phase to address:** Phase 1: Native Benchmark Foundation — fixture design must be reviewed before any benchmark is written.

---

### Pitfall 2: Dead Code Elimination Silences the Entire Canvas Benchmark

**What goes wrong:**
Canvas operations like `canvas.clear(0)` or `canvas.setPixel(x, y, c)` have no observable side effect if the canvas buffer is never read. GCC/Clang at `-O2` or `-O3` will eliminate the entire loop body, reporting 0ns or sub-1ns per iteration. The existing `comprehensive_benchmark.cpp` example does NOT use `doNotOptimizeAway` — it will produce entirely wrong results if compiled with optimization enabled.

**Why it happens:**
The compiler sees: "this canvas is a local variable, nothing reads from it after the loop, and setPixel has no side effects visible to the caller." It is free to eliminate the operation entirely under the as-if rule. The existing benchmark measures the empty loop overhead of `std::chrono::high_resolution_clock::now()`, not canvas operations.

**How to avoid:**
- After every canvas operation inside a nanobench lambda, call `ankerl::nanobench::doNotOptimizeAway(canvas.getBufferPtr())` or `ankerl::nanobench::doNotOptimizeAway(canvas)`.
- Alternatively, read a pixel from the canvas after each operation and pass it to `doNotOptimizeAway`.
- Build benchmarks with `-O2` (same as release) — never benchmark at `-O0`. Dead code elimination only matters when optimizations are on; using `-O0` produces false safety and unmeaningful numbers.
- Verify by comparing `-O0` vs `-O2` numbers: if `-O2` is 100x faster than `-O0` on a simple setPixel loop, dead code elimination has occurred.

**Warning signs:**
- Canvas benchmark reports single-digit nanoseconds per `clear()` — a 128x64 canvas clear should take ~1–5 µs at minimum
- nanobench reports 0 or near-0 iterations per second
- Removing the entire loop body from the lambda does not change the reported time

**Phase to address:** Phase 1: Native Benchmark Foundation — `-O2` build and `doNotOptimizeAway` usage must be in the benchmark CMake target from day one.

---

### Pitfall 3: GitHub Actions Shared Runners Produce 5–15% Variance — 110% Threshold Will Cause False Positives

**What goes wrong:**
GitHub Actions `ubuntu-latest` shared runners exhibit 5–15% coefficient of variation on microbenchmarks due to noisy neighbors, CPU frequency scaling, and background OS activity. A 110% regression threshold (fail if result is more than 10% slower than baseline) will fire on nearly every PR that touches a file — not because code regressed, but because the runner was slightly busier. This produces alert fatigue: developers learn to ignore benchmark failures and the CI check becomes worthless.

**Why it happens:**
Shared runners are VMs on shared physical hardware. The 2.66% average CoV measured in controlled studies represents ideal conditions; real-world conditions with background processes, garbage collection, and network activity push this to 5–15% regularly. Microbenchmarks measuring sub-microsecond operations amplify this noise multiplicatively.

**How to avoid:**
- Use a 130–150% alert threshold with the `github-action-benchmark` action for initial deployment, only tightening after establishing a stable baseline over 50+ runs.
- Run benchmark jobs with `runs-on: ubuntu-latest` and restrict to `push` on `main` only — do not run on PRs initially. PRs trigger too frequently and accumulate noise without historical context.
- Treat the first 30–50 CI benchmark runs as calibration. Do not set `fail-on-alert: true` until the baseline is established.
- For a sub-microsecond embedded engine, the most meaningful benchmark signal is tracking trends over time (week-over-week), not pass/fail on each PR.
- Consider running benchmarks in a dedicated self-hosted runner pinned to a single CPU core (`taskset -c 0`) if accuracy becomes critical. This reduces variance to <2%.

**Warning signs:**
- CI benchmark fails on PRs that only change documentation
- Benchmark history shows results oscillating ±20% between identical commits
- Developers begin adding `[skip ci]` to commit messages to avoid flaky benchmark failures

**Phase to address:** Phase 2: CI Regression Detection — threshold calibration must happen after the first 30+ baseline runs, not at workflow creation time.

---

### Pitfall 4: lua_sethook with LUA_MASKCALL|LUA_MASKRET Measures Its Own Overhead, Not Script Performance

**What goes wrong:**
Every function call in Lua triggers the `LUA_MASKCALL` hook, and every return triggers `LUA_MASKRET`. If the hook function calls `esp_cpu_get_cycle_count()` or `std::chrono::high_resolution_clock::now()` to record timestamps, the overhead of the clock call is included in every measured function's time. For short Lua functions (1–5 instructions), the hook overhead can exceed the actual function execution time by 10–50x. The profiler then reports that every Lua function takes the same amount of time — specifically, the time to execute the hook body itself.

**Why it happens:**
Lua's debug hook is invoked at the C level for every call/return boundary. Each invocation requires a C→Lua context switch that is already ~48ns overhead on desktop. Adding a clock call (~10–50ns on x86, ~100–300ns on ESP32 via `esp_timer_get_time()`) means the minimum measurable function time is floor-limited by instrumentation cost. The community has documented this: "the overhead of a Lua call for each hook is too high and usually invalidates any timing measure."

**How to avoid:**
- Use `LUA_MASKCALL | LUA_MASKRET` only for call-count profiling (which call sites are hot), not for precise timing. Call counts are accurate regardless of hook overhead.
- For timing-aware profiling, use `LUA_MASKCOUNT` with a count value of 100–1000 instructions to sample at lower frequency, reducing overhead by 100–1000x.
- On the headless CLI profiler (`enjin_run --profile`), restrict timing to the top level: record `lua_resume` entry/exit time in C++, not per-function times inside Lua.
- Use `lua_gc(L, LUA_GCCOUNT, 0)` before and after running a script to measure GC allocation delta — this is accurate and zero-overhead because it reads a counter, not a hook.
- Explicitly document the profiler as "call-count accurate, timing approximate" to set correct expectations.

**Warning signs:**
- All Lua functions report nearly identical execution times regardless of their actual complexity
- A 1-instruction Lua function reports the same time as a 100-instruction function
- Profiler shows "update()" taking 10x longer than observed frame time suggests it should

**Phase to address:** Phase 4: Lua Profiling — profiler design must choose call-count vs. timing mode before any implementation begins.

---

### Pitfall 5: FrameTimingInstrumentation Atomics Create False Cache Pressure on Single-Core ESP32

**What goes wrong:**
The proposed `FrameTimingInstrumentation` struct uses `std::atomic` fields with cache-line alignment. On multi-core desktop this is correct and prevents false sharing. On ESP32-S3 (dual-core Xtensa, with the game loop running on a single core), `std::atomic` operations with `memory_order_seq_cst` are significantly more expensive than non-atomic reads/writes: they emit memory fence instructions on Xtensa even with no contention. For per-phase timing (update/render/Lua/composite measured every frame at 60fps), this adds 4 × (fence overhead) per frame.

More critically: `alignas(64)` padding on ESP32 wastes 64 bytes per atomic field in static RAM. For a struct with 6 timing fields, that is 384 bytes of padding in a system where every byte of internal RAM counts.

**Why it happens:**
The instrumentation struct is designed for multi-threaded operation (reader on one core, writer on another). But enjin2's game loop runs on a single FreeRTOS task — there is no reader/writer concurrency in the core path on ESP32. The desktop implementation's alignment requirements don't translate meaningfully to an Xtensa embedded target.

**How to avoid:**
- Use compile-time platform guards: `#ifdef ESP32` use plain `uint32_t` fields with volatile (sufficient for single-task single-core reads via serial/UART inspection), `#else` use the full `std::atomic<uint64_t>` with `alignas(64)` for desktop/WASM.
- On ESP32, use `esp_cpu_get_cycle_count()` (single-cycle read, no fence) to record timestamps. Divide by CPU clock frequency for microseconds.
- On desktop, `std::chrono::high_resolution_clock::now()` inside a `memory_order_relaxed` atomic store is sufficient.
- Provide a simple API: `FrameTimer::begin(Phase::UPDATE)` / `FrameTimer::end(Phase::UPDATE)` — the implementation selects the platform path internally.

**Warning signs:**
- Frame time reported by the instrumentation is consistently 10–20% higher than actual frame time observed by the host application
- ESP32 static RAM usage increases unexpectedly after adding the timing struct
- `esp_get_free_heap_size()` decreases proportionally to the number of atomic fields defined

**Phase to address:** Phase 3: Frame Timing Instrumentation — platform-conditional implementation required from the start, not as a follow-up optimization.

---

### Pitfall 6: Custom lua_Alloc Hook Over-Counts Allocations Due to Lua VM Realloc Pattern

**What goes wrong:**
Lua 5.4's VM uses a single `lua_Alloc` callback for all three operations: `malloc(nsize)` when `ptr == NULL`, `free(ptr)` when `nsize == 0`, and `realloc(ptr, nsize)` when both are non-null. A naive counting allocator that increments a counter whenever `nsize > 0` and `ptr == NULL` will miss the realloc path. More importantly, Lua's string interning, table resizing, and upvalue management all generate reallocs that look like `(ptr != NULL, nsize > 0)` — these are not new allocations but are counted as such by many hook implementations.

The result: a hot path that makes zero net allocations (reallocates the same buffer repeatedly) is flagged as "allocating" and fails the CI zero-alloc check.

**Why it happens:**
The Lua VM aggressively reallocs its internal data structures. A table that grows from 4 to 8 slots calls the allocator with `(ptr, old_size, new_size)` — the C standard would call this `realloc`, but Lua routes it through the same hook. The allocation counter should track "net new allocation events" (new `ptr` returned for a unique memory region), not "any call with nsize > 0."

**How to avoid:**
- Count allocations correctly: increment only when `old_size == 0 && new_size > 0` (true malloc). Decrement when `new_size == 0` (true free). Ignore reallocs (`old_size > 0 && new_size > 0`).
- Install the custom allocator at state creation time via `lua_newstate(countingAlloc, &counter)` — NOT via `lua_setallocf()` after state creation. Post-creation `lua_setallocf` risks mismatched allocator/deallocator for memory allocated during `luaL_newstate()`.
- Run the allocation counter only over specific hot-path spans: call `counter.reset()` immediately before the measured operation and `counter.check()` immediately after. Do not measure the counter over an entire benchmark run, which includes GC activity.
- Document expected Lua internal allocations (string interning, stack growth) that are acceptable vs. unexpected allocations in "hot paths" (script execution after load, update(), draw() callbacks).

**Warning signs:**
- Allocation counter reports 50–200 allocations per frame even in a script that creates no tables or strings
- "Zero alloc" CI check fails on commit that adds no user-level Lua code changes
- Counter reports negative allocation count (decrement without prior increment) when GC collects previously allocated objects

**Phase to address:** Phase 5: Static Allocation Verification — allocator hook design must be validated against a known-zero-alloc path before CI check is enabled.

---

### Pitfall 7: Benchmark Binary Links Against Debug Build of enjin2 Core — Results Are Meaningless

**What goes wrong:**
If the benchmark CMake target links against a debug build of `enjin2_core` (compiled without optimizations, with assertions enabled, with `-O0`), the measured performance is not representative of production behavior. Canvas pixel operations measured at `-O0` can be 5–20x slower than at `-O2` due to inlined operations not being inlined. The benchmark results stored in CI history will never be comparable to actual production performance, making regression detection moot.

**Why it happens:**
CMake's default `CMAKE_BUILD_TYPE` is empty (not Release). If the benchmark CMake target is added without explicitly setting `CMAKE_BUILD_TYPE=Release`, the build system produces an unoptimized binary. The existing `examples/CMakeLists.txt` likely inherits the parent's build type — which may be Debug in local developer builds.

**How to avoid:**
- The benchmark CMake target must force `-O2` for all enjin2 targets it links against. Use a CMake preset or CI workflow flag: `cmake -B build -DCMAKE_BUILD_TYPE=Release`.
- In `scripts/build-bench.sh`, always pass `-DCMAKE_BUILD_TYPE=Release` explicitly — never rely on the default.
- Add a CMake check in the benchmark target: `if(NOT CMAKE_BUILD_TYPE STREQUAL "Release") message(WARNING "Benchmarks built without -O2 — results are not production-representative") endif()`.
- For CI, the `.github/workflows/benchmarks.yml` must explicitly pass `--config Release` to the cmake build command.

**Warning signs:**
- Benchmark results are 5–20x slower than expected for simple operations (e.g., `canvas.clear()` takes >100µs instead of 1–5µs)
- Results vary dramatically between developer machines with different CMake defaults
- CI benchmark history shows a sudden 10x performance improvement after a commit that only "fixed build type" — indicating all prior results were at `-O0`

**Phase to address:** Phase 1: Native Benchmark Foundation — the build script must enforce Release mode before any baseline is recorded.

---

### Pitfall 8: gh-pages Benchmark History Is Destroyed by Force-Push or Branch Recreation

**What goes wrong:**
`github-action-benchmark` accumulates results by committing JSON data to the `gh-pages` branch. If `gh-pages` is ever force-pushed (e.g., by the documentation deployment workflow, which deploys Docusaurus to the same branch), all benchmark history is lost. The existing `docs.yml` deploys Docusaurus to GitHub Pages — it uses `actions/upload-pages-artifact` and `actions/deploy-pages@v4`, which manages the deployment artifact separately from the branch. However, if anyone manually recreates `gh-pages` or a future workflow change adds `git push --force`, history is silently deleted.

**Why it happens:**
The `github-action-benchmark` action and Docusaurus deployment share `gh-pages` as the GitHub Pages branch. `actions/deploy-pages@v4` uses a deployment artifact that replaces the entire Pages content on each deploy. This is not the same as force-pushing the branch, but it does overwrite the branch content. If benchmark data lives in `gh-pages/dev/bench/`, it will be wiped on the next Docusaurus deployment unless the benchmark workflow explicitly preserves the data directory or uses a separate storage mechanism.

**How to avoid:**
- Use the `external-data-json-path` option in `github-action-benchmark` instead of the gh-pages approach. This stores benchmark history in a JSON file that can live in a separate branch (e.g., `bench-data`) independent of the Pages deployment. The JSON file is committed to `bench-data`, and the dashboard is generated separately.
- Alternatively: keep benchmark data in a `bench-results/` directory committed to `main` (small JSON files), and generate the dashboard as part of the Docusaurus build.
- At minimum: document that `gh-pages` must NOT be recreated or force-pushed, and add branch protection to `gh-pages` disabling force-push.
- Verify the separation works by running both the docs workflow and benchmark workflow on the same commit and confirming neither erases the other's data.

**Warning signs:**
- Benchmark dashboard shows only one data point (the most recent run) despite multiple runs
- `gh-pages` branch commit history shows a single "Deploy" commit from the docs workflow that wiped all previous commits
- `github-action-benchmark` action fails with "no previous data found" on every run

**Phase to address:** Phase 2: CI Regression Detection — storage strategy must be chosen before the first benchmark run is recorded.

---

### Pitfall 9: Headless enjin_run Profiler Stubs Platform APIs but Pulls In Lua Binding Globals That Depend on Real State

**What goes wrong:**
The headless CLI tool (`enjin_run --profile script.lua`) stubs all graphics/input/platform APIs as no-ops. However, enjin2's Lua bindings register engine global tables (`engine.scene`, `engine.input`, `engine.lua`, etc.) that hold pointers to real subsystem instances (`LuaScriptSystem*`, `InputState*`, `SceneStateMachine*`). If the headless runner creates a `LuaScriptSystem` but does not initialize the SDL runner or any scene, the bindings will register these globals with null pointers. Any Lua script that calls `engine.scene.find()`, `engine.input.isButtonHeld()`, or any subsystem API will dereference null inside the binding.

**Why it happens:**
The bindings layer was designed with an implicit contract: by the time any Lua script runs, all pointers have been set via `registerAll()`. The headless runner breaks this contract by initializing Lua without a complete engine host. The existing null guards in input bindings (`if (!m_inputState) return 0`) catch some cases, but not all bindings have defensive null checks — particularly `engine.scene.*` and `engine.camera.*`.

**How to avoid:**
- The headless runner must not call `registerAll()` from `LuaScriptSystem`. Instead, create a minimal stub host that satisfies the pointer contracts: a no-op `SceneStateMachine`, a zeroed `InputState`, and a no-op render canvas.
- Alternatively: create a `HeadlessLuaRunner` class that calls `lua_engine.loadScriptFile()` directly without going through `LuaScriptSystem::registerAll()`, bypassing all engine-table registration. Scripts under profiling should only use engine.lua and engine.time; all other subsystems are out of scope for profiling.
- Add null guard assertions to all engine table bindings that would be missing in headless mode, so failures are explicit errors rather than silent null dereferences.
- The headless runner's test suite must include a script that exercises every `engine.*` subtable to verify graceful null handling.

**Warning signs:**
- Headless runner segfaults on any script that calls `engine.scene.*` or `engine.camera.*`
- Profiler output shows correct call counts for pure Lua functions but crashes on binding-crossing calls
- Scripts that work in SDL runner fail in `enjin_run` for no obvious reason

**Phase to address:** Phase 4: Lua Profiling — headless runner architecture must define the binding stub contract before implementation.

---

### Pitfall 10: esp_cpu_get_cycle_count() Wraps Every 17.9 Seconds at 240 MHz

**What goes wrong:**
The ESP32's `esp_cpu_get_cycle_count()` returns a `uint32_t` that wraps around every ~4.3 billion cycles. At 240 MHz, this is approximately 17.9 seconds. For frame timing (measuring a 16ms budget), this is safe — 16ms is far shorter than 17.9 seconds. However, if the frame timing code subtracts start from end without handling wrapping (`end - start` as unsigned arithmetic), it is technically correct for the normal case. But if an interrupt fires between start and end and consumes more than 17.9 seconds of execution time (impossible for a frame), the subtraction overflows.

The real problem: if the timing values are stored as `uint64_t` atomics (to handle future long sessions), but `esp_cpu_get_cycle_count()` returns `uint32_t`, the counter silently resets on every wrap and the "cumulative frame count" metric becomes incorrect after 17.9 seconds.

**Why it happens:**
Developers copy the desktop timing pattern (`uint64_t` nanosecond timestamps) to ESP32 without adapting it to the 32-bit cycle counter. The mismatch between a 32-bit hardware counter and a 64-bit storage type works until wrap-around produces a large "negative" cycle count that inflates the stored value.

**How to avoid:**
- On ESP32, use only per-frame delta measurements with `uint32_t`: `uint32_t delta = end_cycles - start_cycles`. Unsigned 32-bit subtraction handles wrapping correctly for intervals shorter than 17.9 seconds.
- Accumulate frame counts as a frame counter (increment per frame), not as a total elapsed cycle count.
- Convert cycles to microseconds at storage time: `uint32_t us = delta / (CPU_FREQ_MHZ)` where `CPU_FREQ_MHZ` is 240.
- Never store absolute cycle counts — only deltas. Reset per-frame timing fields to 0 at the start of each frame.

**Warning signs:**
- Frame timing reports occasionally show impossibly large values (200ms+) that disappear on the next frame
- Cumulative timing stats roll over to near-zero after approximately 17 seconds of runtime
- Timing values look correct in short tests but drift after 30–60 seconds of operation

**Phase to address:** Phase 3: Frame Timing Instrumentation — ESP32 platform path must use delta-only `uint32_t` timing from the start.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Benchmark at `-O0` (default CMake debug) | Easier to debug test failures | Results are 5–20x off from production; CI history is useless | Never — always benchmark at `-O2` |
| Hoist nothing — construct Objects inside nanobench lambda | Simpler test code | Measures allocator, not ECS logic; inflated variance | Never for benchmarks that claim to measure ECS update performance |
| Set 110% fail threshold immediately | Looks rigorous | 5–15% shared runner variance causes false positives, alert fatigue | Only after 50+ baseline runs on the same runner type |
| Use `LUA_MASKCALL|LUA_MASKRET` for timing in hook | Simple one-hook solution | Overhead per hook call is larger than short Lua functions; timing is meaningless | Only for call-count profiling, never for timing |
| Store benchmark history in `gh-pages` alongside Docusaurus | One branch for all GitHub Pages content | Docusaurus deployment overwrites benchmark data | Never if docs.yml uses deploy-pages artifact |
| `std::atomic<uint64_t>` with `alignas(64)` everywhere including ESP32 | Same struct on all platforms | 384+ bytes wasted static RAM on ESP32; unnecessary fences on Xtensa | Never on ESP32 — use platform guard |
| Call `lua_setallocf` after `luaL_newstate` to install counting allocator | Easy to add post-init | Mismatched allocator for memory allocated during state creation; risk of memory corruption | Never — use `lua_newstate` with custom allocator from the start |
| Benchmark the existing `comprehensive_benchmark.cpp` examples with nanobench | Preserves existing test coverage | These benchmarks use `std::vector<BenchmarkResult>` (heap alloc) and don't use `doNotOptimizeAway` — they are incorrect | Never without rewriting them from scratch |

---

## Integration Gotchas

Common mistakes when connecting these new features to the existing system.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| nanobench + enjin2_core | Linking bench binary against enjin2_core compiled as part of the default (possibly Debug) build | Create a dedicated `bench` CMake preset that builds all dependencies at `-O2`; the benchmark CMakeLists must be isolated from the test CMakeLists |
| Lua allocator hook + existing LuaEngine | Installing counting allocator via `lua_setallocf` after `LuaEngine::initialize()` | The existing `LuaEngine` uses `luaL_newstate()` — replace with `lua_newstate(countingAlloc, &ctx)` when building in benchmark/verification mode; use a compile-time flag `ENJIN2_BENCH_ALLOC=ON` |
| Frame timing + SDL runner | Adding timing calls inside the existing `sdl_main.cpp` frame loop at arbitrary points | The SDL runner already has a defined phase order (tickCamera → tickCoroutines → tickTweens); wrap each phase with `FrameTimer::begin/end` at that existing phase boundary, not inside the subsystem implementations |
| github-action-benchmark + existing docs.yml | Both workflows deploy to GitHub Pages via `gh-pages` branch | Use `external-data-json-path` option to decouple benchmark history from Pages deployment; store JSON in a separate `bench-data` branch |
| headless enjin_run + LuaScriptSystem | Calling `LuaScriptSystem::initialize()` and `registerAll()` from the CLI tool | `registerAll()` requires a live `SceneStateMachine*`, `InputState*`, and canvas — none of which exist in headless mode; create a `MinimalLuaHost` that provides only `engine.lua.*` and `engine.time.*` |
| ESP32 frame timing + `esp_cpu_get_cycle_count()` | Storing cycle counts in `uint64_t` fields of the timing struct | Declare timing fields as `uint32_t` on ESP32; convert deltas to microseconds immediately; never accumulate absolute cycle counts |
| nanobench JSON output + github-action-benchmark | nanobench's default JSON output format is not directly parseable by `github-action-benchmark` | Write a `scripts/convert-bench-json.sh` that transforms nanobench's output to the `github-action-benchmark` customSmallerIsBetter format before the action reads it |

---

## Performance Traps

Patterns that work at small scale but fail under real conditions.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Measuring allocations over full benchmark run, not per hot-path span | Counter reports hundreds of "allocations" per frame even in zero-alloc code (Lua GC background activity) | Reset counter immediately before and after each specific hot-path assertion | Immediately — first CI run will fail |
| Hook-based Lua timing for short functions | All functions report same time (hook overhead floor) | Use hook for counts, time via `lua_resume` entry/exit in C++ | Always — hook overhead > short function execution time |
| Not calling `lua_gc(L, LUA_GCSTOP, 0)` before Lua benchmark section | GC fires mid-benchmark, inflating measured time by 5–50ms | Stop GC before benchmark, run it once manually after | Random — GC fires non-deterministically |
| 110% threshold without baseline stabilization | CI fails on every PR (false alarms) | Use 150% threshold initially; tighten to 120% after 50+ stable runs | Immediately on first real PR |
| `alignas(64)` on ESP32 timing struct | 384+ bytes wasted internal RAM | Platform-conditional: `alignas(64)` on desktop, plain struct on ESP32 | At first ESP32 build with timing struct |
| nanobench with default epoch/iteration settings measuring GPU-influenced operations | Results vary 3–5x between runs because compositor step varies with canvas state | Pre-fill canvas with deterministic content before each benchmark iteration | On benchmarks that test `LayerCompositor::composite()` |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Benchmark suite:** Compiles and runs locally. Verify that removing the `doNotOptimizeAway` call from a canvas benchmark changes the reported time by more than 2x — if it doesn't, dead code elimination is active.
- [ ] **nanobench JSON output:** Benchmark binary produces `bench-results/*.json`. Verify the conversion script can parse it and produces valid github-action-benchmark format by running it locally before CI.
- [ ] **CI regression detection:** Workflow file exists and runs. Verify it writes to a persistent store (not ephemeral artifact) by checking that the second run has access to the first run's results.
- [ ] **Frame timing instrumentation:** SDL runner reports per-phase timing. Verify by adding an artificial 1ms sleep inside the update phase and confirming `updateTime_us` increases by ~1000µs.
- [ ] **Lua profiler call counts:** Hook fires for every function. Verify against a known script: a script that calls one function 10 times must show exactly 10 in the call count output.
- [ ] **Lua GC pressure tracking:** `lua_gc(L, LUA_GCCOUNT, 0)` returns bytes. Verify the before/after delta is zero for a script that creates no tables or strings in its update() function.
- [ ] **Allocation verification:** Counter reports zero allocations for `canvas.clear()`, `canvas.setPixel()`, and the ECS update loop. Verify by checking these specific hot paths, not a full frame which includes GC.
- [ ] **headless enjin_run:** CLI tool runs `script.lua` without segfault when the script calls `engine.lua.memory()` and `engine.time.delta()`. Verify with a script that calls every `engine.*` subtable — confirm graceful null handling, not segfault.
- [ ] **ESP32 frame timing:** Cycle counter delta is in reasonable range. Verify that a known-duration operation (a tight loop of N iterations with measured latency) produces the expected microsecond reading when divided by CPU clock frequency.

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Benchmark history wiped from gh-pages | HIGH | Restore from git reflog if within 30 days; otherwise accept loss and start fresh; implement external JSON storage going forward |
| All baseline results recorded at `-O0` | HIGH | Delete all stored benchmark results; force new baseline with correct Release build; document the reset in the PR |
| Counting allocator installed after state creation (mismatched) | MEDIUM | Switch to `lua_newstate` with custom allocator; this requires modifying `LuaPlatform::createState()` to accept an optional allocator hook; existing API is already compatible (it has `allocator` parameter) |
| 110% threshold causing alert fatigue | LOW | Raise threshold to 150% temporarily; allow history to accumulate for 30+ runs; lower gradually; add comment in workflow explaining the calibration |
| Hook-based timing producing useless results | LOW | Replace per-function timing with `LUA_MASKCOUNT` sampling; keep call-count profiling; add documentation explaining the limitation |
| Dead code elimination invalidating benchmark | LOW | Add `doNotOptimizeAway` after every canvas operation; rebuild; verify results are now in expected range |
| ESP32 cycle counter wrap corruption | MEDIUM | Replace `uint64_t` accumulation with per-frame `uint32_t` delta; existing wrap-safe unsigned subtraction handles this; test over 30-second runtime |

---

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Object construction inside nanobench lambda | Phase 1: Benchmark Foundation | ECS update benchmark reports <50ns per iteration for a 16-object scene |
| Dead code elimination on canvas benchmark | Phase 1: Benchmark Foundation | canvas.clear() benchmark reports >1µs; removal of doNotOptimizeAway changes result by >2x |
| Benchmark built at wrong optimization level | Phase 1: Benchmark Foundation | `build-bench.sh` passes `-DCMAKE_BUILD_TYPE=Release`; CMake warns if not Release |
| nanobench JSON → github-action-benchmark format | Phase 1 → Phase 2 handoff | Conversion script runs locally and produces parseable output before CI workflow is created |
| gh-pages history destruction | Phase 2: CI Regression Detection | Docs workflow and benchmark workflow run sequentially on same commit; benchmark data survives |
| 110% threshold false positives | Phase 2: CI Regression Detection | Threshold set to 150% initially; calibration documented in workflow comment |
| Frame timing atomic overhead on ESP32 | Phase 3: Frame Timing | ESP32 build uses platform-conditional `uint32_t volatile` fields, not `std::atomic<uint64_t>` |
| esp_cpu_get_cycle_count() wrap | Phase 3: Frame Timing | 30-second runtime test shows stable timing values; no outlier spikes |
| lua_sethook timing measures its own overhead | Phase 4: Lua Profiling | Call-count and timing are separate modes; profiler documentation states timing is approximate |
| Headless runner null-dereferences on engine.* | Phase 4: Lua Profiling | Test script exercises all engine.* subtables without segfault; CI runs headless profiler as smoke test |
| lua_Alloc realloc over-counting | Phase 5: Allocation Verification | Known-zero-alloc path (canvas.clear) reports 0 allocations; Lua string interning does not increment counter |
| Counting allocator installed post-init | Phase 5: Allocation Verification | Allocation hook is provided at lua_newstate() via build flag, not installed after the fact |

---

## Sources

- [nanobench documentation — Installation and Tutorial](https://nanobench.ankerl.com/tutorial.html)
- [nanobench GitHub — martinus/nanobench](https://github.com/martinus/nanobench)
- [github-action-benchmark — benchmark-action/github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark)
- [Is GitHub Actions suitable for running benchmarks? — Quansight Labs](https://labs.quansight.org/blog/github-actions-benchmarks)
- [Benchmarks in CI: Escaping the Cloud Chaos — CodSpeed](https://codspeed.io/blog/benchmarks-in-ci-without-noise)
- [Detection of Performance Changes Using Nyrkiö on GitHub Actions — arXiv 2510.11310](https://arxiv.org/html/2510.11310)
- [Lua 5.4 Reference Manual — debug.sethook / lua_sethook](https://pgl.yoyo.org/luai/i/lua_sethook)
- [Programming in Lua 5.4 — Chapter 23: Debug Library](https://www.lua.org/pil/23.3.html)
- [lua-users wiki: Pepperfish Profiler — hook overhead documented](http://lua-users.org/wiki/PepperfishProfiler)
- [lua-users wiki: Memory Allocation — lua_Alloc protocol](http://lua-users.org/wiki/MemoryAllocation)
- [lua-allocspy — siffiejoe/lua-allocspy (reference for correct allocation counting)](https://github.com/siffiejoe/lua-allocspy)
- [bitsquid dev blog: Fixing memory issues in Lua](http://bitsquid.blogspot.com/2011/08/fixing-memory-issues-in-lua.html)
- [ESP-IDF FreeRTOS Overview — esp_cpu_get_cycle_count()](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos.html)
- [ESP32 Forum: Very accurate hardware timer & FreeRTOS — cycle counter wrap discussion](https://esp32.com/viewtopic.php?t=10331)
- [Cache Line Alignment in C++ — false sharing and atomic overhead](https://ryonaldteofilo.medium.com/cache-line-alignment-in-c-1aac85e4482f)
- enjin2 v1.9 codebase: `src/scripting/lua_platform.cpp`, `src/scripting/lua_engine.cpp`, `include/enjin2/core/object.hpp`, `examples/comprehensive_benchmark.cpp`, `examples/memory_profiler.cpp`, `.github/workflows/docs.yml`
- enjin2 PROJECT.md — Active requirements for v1.10, constraints, key decisions

---
*Pitfalls research for: enjin2 v1.10 Benchmarking & Performance — adding nanobench suite, CI regression detection, frame timing, Lua profiling, and allocation verification*
*Researched: 2026-03-07*
