---
phase: 61-native-benchmark-suite
plan: 01
subsystem: benchmarking
tags: [nanobench, canvas, ecs, lua, benchmarks, performance, cmake, shell]

# Dependency graph
requires:
  - phase: 60-cmake-foundation-vendor
    provides: nanobench_vendor INTERFACE target, bench_smoke CMake scaffold, ENJIN2_BUILD_BENCHMARKS flag

provides:
  - bench_canvas binary: Canvas4/Canvas8 setPixel/clear/fillRect/drawCircle/blit/LayerCompositor composite benchmarks
  - bench_ecs binary: ObjectCollection::addObject, Object::addComponent/removeComponent, Scene::update at 1/8/16/32/48 object counts
  - bench_lua binary: LuaEngine init+shutdown, executeString, binding call overhead (engine.time.delta, math.clamp), GC pressure
  - scripts/build-bench.sh: one-command configure+build+run for all three benchmarks
  - bench-results/ directory populated at runtime with bench_canvas.json, bench_ecs.json, bench_lua.json

affects:
  - 62-wasm-benchmark-parity
  - 63-lua-profiler-headless-runner
  - 64-ci-benchmark-pipeline

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "doNotOptimizeAway(canvas.getPixel(x, y)) after every pixel op to prevent dead-code elimination at -O2"
    - "if(TARGET enjin2_lua) CMake guard for Lua-dependent benchmarks (more robust than if(ENJIN2_BUILD_LUA))"
    - "Separate build-bench/ directory prevents cache contamination with dev build/"
    - "registerAll() called once outside timed lambda; only null-pointer-safe bindings exercised in headless mode"

key-files:
  created:
    - benchmarks/bench_canvas.cpp
    - benchmarks/bench_ecs.cpp
    - benchmarks/bench_lua.cpp
    - scripts/build-bench.sh
  modified:
    - benchmarks/CMakeLists.txt
    - .gitignore

key-decisions:
  - "blit() requires same-size canvas dimensions — Canvas4<W,H>.blit(const Canvas4<W,H>&, ...) — sprite changed to Canvas4<128,128>"
  - "if(TARGET enjin2_lua) guard chosen over if(ENJIN2_BUILD_LUA) in benchmarks/CMakeLists.txt — target existence is the definitive check"
  - "build-bench.sh uses separate build-bench/ directory; /build-bench/ added to .gitignore"
  - "ENJIN2_BUILD_LUA=ON added to build-bench.sh so bench_lua target is always included in one-command build"

patterns-established:
  - "Pattern: Each benchmark binary is a single .cpp file with #define ANKERL_NANOBENCH_IMPLEMENT exactly once"
  - "Pattern: JSON written via ankerl::nanobench::render(templates::json(), bench, ofstream) after all bench.run() calls complete"
  - "Pattern: mkdir('bench-results', 0755) before ofstream open — no-op if directory already exists"
  - "Pattern: scene.activate() mandatory before Scene::update() — update() has if(!active) return guard"

requirements-completed: [BENCH-02, BENCH-03, BENCH-04, BENCH-05, BENCH-06]

# Metrics
duration: 3min
completed: 2026-03-07
---

# Phase 61 Plan 01: Native Benchmark Suite Summary

**Three nanobench binaries (bench_canvas, bench_ecs, bench_lua) + build-bench.sh shell driver: Canvas4/Canvas8 pixel ops, ECS object/scene/component throughput, and LuaEngine headless binding overhead measured with JSON output to bench-results/**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-07T21:45:42Z
- **Completed:** 2026-03-07T21:48:51Z
- **Tasks:** 3
- **Files modified:** 6

## Accomplishments

- bench_canvas benchmarks Canvas4 (setPixel, clear, fillRect 32x32, drawCircle r16, blit 128x128) and Canvas8 (setPixel, fillRect 32x32) and LayerCompositor composite; doNotOptimizeAway prevents -O2 dead-code elimination (timings range 14-45794 ns)
- bench_ecs benchmarks ObjectCollection::addObject at 5 object counts (1/8/16/32/48), addComponent/removeComponent<C_Position>, and Scene::update at 5 counts with activate() guard applied; timing scales correctly (194-3261 ns for addObject, 21-196 ns for update)
- bench_lua benchmarks LuaEngine init+shutdown (44 us), executeString noop (710 ns), engine.time.delta binding call (1.2 us), math.clamp (1.8 us), and GC full collect (2.5 us) — fully headless, no SDL3
- scripts/build-bench.sh provides one-command configure+build+run; uses separate build-bench/ directory; all three JSON files produced in bench-results/

## Task Commits

Each task was committed atomically:

1. **Task 1: Create bench_canvas and bench_ecs binaries with CMake targets** - `91dd80e` (feat)
2. **Task 2: Create bench_lua binary with Lua-guarded CMake target** - `f4bb421` (feat)
3. **Task 3: Create build-bench.sh orchestration script and verify end-to-end** - `57b1f7f` (feat)

**Plan metadata:** (docs commit — see below)

## Files Created/Modified

- `benchmarks/bench_canvas.cpp` - Canvas4/Canvas8/LayerCompositor pixel op benchmarks with doNotOptimizeAway
- `benchmarks/bench_ecs.cpp` - ObjectCollection/Object/Scene ECS benchmarks at 1/8/16/32/48 object counts
- `benchmarks/bench_lua.cpp` - LuaEngine headless init/shutdown, executeString, binding call, GC benchmarks
- `benchmarks/CMakeLists.txt` - Added bench_canvas, bench_ecs, and bench_lua (with if(TARGET enjin2_lua) guard) targets
- `scripts/build-bench.sh` - One-command build+run script with separate build-bench/ directory
- `.gitignore` - Added /build-bench/ to prevent benchmark build directory from being committed

## Decisions Made

- `blit()` requires same-size canvas: `Canvas4<W,H>.blit(const Canvas4<W,H>&, ...)` — the plan's 32x32 sprite onto a 128x128 canvas would not compile. Changed sprite to `Canvas4<128,128>` to match destination. This makes the blit benchmark more representative (full-canvas blit) and correctly uses the public API.
- Used `if(TARGET enjin2_lua)` guard in benchmarks/CMakeLists.txt (not `if(ENJIN2_BUILD_LUA)`) — target existence is the definitive check regardless of variable scoping.
- Added `ENJIN2_BUILD_LUA=ON` to build-bench.sh configure step so bench_lua is always available in the one-command build.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed blit() sprite size mismatch**
- **Found during:** Task 1 (bench_canvas compilation)
- **Issue:** Plan specified `Canvas4<32,32>` sprite blitted onto `Canvas4<128,128>` canvas. The `blit()` API signature is `void blit(const Canvas4<WIDTH,HEIGHT>&, ...)` — both source and destination must be the same template dimensions.
- **Fix:** Changed sprite to `Canvas4<128,128>` initialized with `clear(Pixel4(5))`. Benchmark name updated to "canvas4: blit 128x128 sprite". The benchmark still correctly measures the blit path at -O2.
- **Files modified:** benchmarks/bench_canvas.cpp
- **Verification:** bench_canvas compiled and ran, blit timing ~42 us (non-trivial — memory copy for 128x128 canvas).
- **Committed in:** 91dd80e (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — API type error)
**Impact on plan:** Fix required for compilation. Blit benchmark remains meaningful — full-canvas blit is a realistic workload.

## Issues Encountered

None beyond the blit size mismatch above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Three benchmark binaries in place; `bash scripts/build-bench.sh` is the developer entry point
- bench-results/*.json files are valid JSON and ready for future CI integration (Phase 64)
- Phase 62 (WASM benchmark parity) can now target the same nanobench API patterns established here
- Phase 63 (Lua profiler + headless runner) should note: registerAll() is safe with null engine pointers; only null-pointer-safe bindings (engine.time.delta, math.clamp) were validated here

---
*Phase: 61-native-benchmark-suite*
*Completed: 2026-03-07*
