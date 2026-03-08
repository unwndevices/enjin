---
phase: 61-native-benchmark-suite
plan: 02
subsystem: benchmarking
tags: [nanobench, lua, objectproxy, event-dispatch, gap-closure, performance]

# Dependency graph
requires:
  - phase: 61-01
    provides: bench_lua binary scaffold with LuaEngine init/shutdown, executeString, binding call, GC benchmarks

provides:
  - benchmarks/bench_lua.cpp: ObjectProxy round-trip benchmark (engine.scene.find -> userdata -> metatable -> field read)
  - benchmarks/bench_lua.cpp: LuaEventBus emit dispatch benchmark (subscribe + emit with Lua callback)
  - bench-results/bench_lua.json: 7 benchmark entries (5 existing + 2 new gap closures)

affects:
  - 62-wasm-benchmark-parity
  - 63-lua-profiler-headless-runner
  - 64-ci-benchmark-pipeline

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "BenchScene (Scene id 99) wired into LuaBindings via setActiveScene() before ObjectProxy benchmark"
    - "setActiveScene() clears event bus — subscribe after it, not before"
    - "setActiveScene(nullptr) before BenchScene destructor — prevents dangling Lua registry pointer"
    - "engine.event.on() called via executeString outside timed lambda to pre-register handler"

key-files:
  created: []
  modified:
    - benchmarks/bench_lua.cpp

key-decisions:
  - "BenchScene uses id 99 (not 1 as in bench_ecs) to avoid any ID collision with future multi-scene tests"
  - "Event subscription placed via executeString outside timed region — only emit() is the hot path measured"
  - "setActiveScene(nullptr) cleanup placed before GC benchmark — ensures no dangling proxy during GC collect"

# Metrics
duration: 2min
completed: 2026-03-08
---

# Phase 61 Plan 02: ObjectProxy and Event Dispatch Gap Closure Summary

**ObjectProxy round-trip (BENCH-04) and LuaEventBus event dispatch (BENCH-03) benchmarks added to bench_lua.cpp using a real BenchScene and named Object; both produce non-zero timings in bench-results/bench_lua.json, closing both verification gaps from 61-VERIFICATION.md**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-08T01:03:32Z
- **Completed:** 2026-03-08T01:05:41Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments

- Added `BenchScene` class at file scope in bench_lua.cpp (same pattern as bench_ecs.cpp, uses id 99)
- Wired real `BenchScene` + `enjin2::Object` named "bench_target" into LuaBindings via `setActiveScene()` before the proxy benchmark
- Added `lua proxy: find+field round-trip` benchmark: measures full path from Lua `engine.scene.find('bench_target')` through C++ `findByName` -> `lua_newuserdata(ObjectProxy)` -> metatable attach -> `setLuaProxy` -> Lua `p.name` -> `__index` -> `Object::getName()` -> string return. Timing: ~1,679 ns
- Added `lua event: emit dispatch` benchmark: Lua no-op handler registered via `executeString` outside timed loop; benchmark measures `engine.event.emit('bench_evt')` -> `LuaEventBus::emit` -> channel lookup -> snapshot refs -> `lua_rawgeti` + `lua_pcall`. Timing: ~920 ns
- Added `bindings.setActiveScene(nullptr)` before BenchScene destructor to prevent dangling pointer in Lua registry
- All 7 benchmarks (5 existing + 2 new) verified in `bench-results/bench_lua.json`
- `bash scripts/build-bench.sh` end-to-end integration passes with all 3 binaries

## Task Commits

Each task was committed atomically:

1. **Task 1: Add ObjectProxy round-trip and event dispatch benchmarks to bench_lua.cpp** - `ab9f9ce` (feat)

## Files Created/Modified

- `benchmarks/bench_lua.cpp` - Added BenchScene class, setActiveScene wiring, two new bench.run() cases, and cleanup

## Decisions Made

- `BenchScene` assigned id 99 (not 1) to avoid any ID-collision risk with potential future multi-scene tests.
- Event handler subscription (`engine.event.on`) placed outside the timed lambda via `executeString` — only `engine.event.emit` is the measured hot path. This correctly isolates dispatch overhead from subscribe overhead.
- `setActiveScene(nullptr)` placed immediately after both new benchmarks and before the GC benchmark — cleanly detaches the scene from the Lua registry before `BenchScene` goes out of scope at end of the `{}` block.

## Deviations from Plan

None — plan executed exactly as written. The ordering, BenchScene class placement, setActiveScene call ordering, event subscription timing, and cleanup all matched the plan specification precisely.

## Self-Check: PASSED

- `benchmarks/bench_lua.cpp` FOUND
- commit `ab9f9ce` FOUND
- `bench-results/bench_lua.json` contains 7 entries including both `lua proxy: find+field round-trip` and `lua event: emit dispatch`
- `bash scripts/build-bench.sh` completed successfully end-to-end

## Next Phase Readiness

- BENCH-03 and BENCH-04 requirements are now fully satisfied
- bench_lua.json provides 7 benchmark entries covering init, script load, binding calls, ObjectProxy, event dispatch, and GC — comprehensive Lua overhead baseline
- Phase 62 (WASM benchmark parity) can target these same 7 benchmark names as the native baseline
- Phase 64 (CI benchmark pipeline) has a complete 7-entry baseline for regression detection

---
*Phase: 61-native-benchmark-suite*
*Completed: 2026-03-08*
