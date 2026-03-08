---
phase: 63-lua-profiler-headless-runner
plan: 02
subsystem: scripting
tags: [lua, headless, enjin_run, profiler, cmake, lua_sethook, gc, frame-loop]

# Dependency graph
requires:
  - phase: 63-lua-profiler-headless-runner
    plan: 01
    provides: LuaProfiler header-only singleton (install/uninstall/printTable/printJSON)
  - phase: 61-native-benchmark-suite
    provides: LuaEngine + LuaBindings headless pattern (bench_lua.cpp)
provides:
  - enjin_run binary: headless Lua runner with --profile, --frames, --output json CLI flags
  - src/platform/headless/headless_main.cpp: headless entry point with null-safe wiring
  - CMakeLists.txt ENJIN2_BUILD_HEADLESS option and enjin_run executable target
affects: [64-ci-pipeline, benchmarking, profiling]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Headless runner: static LayerCompositor<W,H> + 5 static LuaCanvas wrappers — zero heap alloc"
    - "Null-safe wiring: setLayers(4 canvases) + setDebugCanvas(); setInput() NOT called — currentInput stays null"
    - "GC ring buffer (256 slots): per-frame memory delta tracked via lua_gc(LUA_GCCOUNT/GCCOUNTB)"
    - "PROF-03 explicit path: lua_sethook(L, nullptr, 0, 0) in no-profiler branch — confirmed zero overhead"
    - "Uninstall hook BEFORE g_lua.shutdown() to prevent hook firing during lua_close GC sweep"

key-files:
  created:
    - src/platform/headless/headless_main.cpp
  modified:
    - CMakeLists.txt

key-decisions:
  - "setInput() NOT called in headless runner — currentInput remains nullptr; all engine.input.* bindings null-guard safely"
  - "Profiler installed BEFORE loadScript so module-level init() calls are counted (mirrors 63-01 pattern)"
  - "ENJIN2_BUILD_HEADLESS FATAL_ERROR on EMSCRIPTEN or ESP32 — same guard pattern as ENJIN2_BUILD_BENCHMARKS"
  - "enjin_ui linked to enjin_run (same as enjin2_sdl) — needed for UI binding registration in LuaBindings::registerAll()"
  - "No ENJIN2_FRAME_TIMING define for enjin_run — headless runner does not use frame timing atomics"

patterns-established:
  - "Pattern: Headless frame loop passes self=nil to update(self, dt) and draw(self) — matches SDL runner nil pattern"
  - "Pattern: GC delta computed as (GCCOUNT*1024 + GCCOUNTB) diff before/after each frame's update+draw cycle"
  - "Pattern: exit code 1 on frame error (lua_pcall failure) — allows CI scripts to detect broken scripts"

requirements-completed: [PROF-04, PROF-05, PROF-06]

# Metrics
duration: 2min
completed: 2026-03-08
---

# Phase 63 Plan 02: Headless Lua Profiling Runner (enjin_run) Summary

**enjin_run CLI binary with --profile/--frames/--output json flags, null-safe headless frame loop, GC pressure tracking, and zero-overhead disabled path (PROF-03) — 46/46 ctest pass, no regressions**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-08T07:45:25Z
- **Completed:** 2026-03-08T07:47:32Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- `enjin_run` binary: `--frames 100 script.lua` runs 100 headless frames and exits 0 (PROF-04), no window or display required
- `--profile` flag: installs LuaProfiler before loadScript, prints sorted Function/Calls/Line/Source table with GC pressure summary (PROF-05)
- `--profile --output json` flag: writes valid JSON array of profiling results to stdout (PROF-05)
- `--profile` disabled path: explicit `lua_sethook(L, nullptr, 0, 0)` — confirmed zero hook overhead (PROF-03)
- Null-safe engine.* subtable wiring: setLayers(4) + setDebugCanvas() called; setInput() omitted — no null-dereference crash (PROF-06)
- 46/46 ctest passes — zero regressions across all existing tests

## Task Commits

Each task was committed atomically:

1. **Task 1: Create headless_main.cpp with headless frame loop and profiler integration** - `dbffe71` (feat)
2. **Task 2: Add ENJIN2_BUILD_HEADLESS CMake option and enjin_run target** - `309780d` (feat)

**Plan metadata:** TBD (docs: complete plan)

## Files Created/Modified

- `src/platform/headless/headless_main.cpp` — 245-line headless entry point: arg parsing, null-safe LuaScriptSystem wiring, profiler install/uninstall, headless frame loop (update/tickCoroutines/tickTweens/draw), GC ring buffer, text/JSON profiler output
- `CMakeLists.txt` — ENJIN2_BUILD_HEADLESS option + enjin_run executable target with FATAL_ERROR guards for EMSCRIPTEN/ESP32 and missing enjin2_lua

## Decisions Made

- `setInput()` NOT called in headless runner — `currentInput` remains nullptr; all `engine.input.*` bindings null-guard `currentInput` safely (verified: layer_demo.lua runs 100 frames without crash)
- Profiler installed BEFORE `loadScript()` so module-level `init()` calls are counted — matches Phase 63-01 pattern established in Plan 01 patterns
- FATAL_ERROR on EMSCRIPTEN or ESP32 — matches ENJIN2_BUILD_BENCHMARKS guard pattern for consistency
- `enjin2_ui` linked to `enjin_run` (same as `enjin2_sdl`) — required because `LuaBindings::registerAll()` registers UI bindings that depend on enjin2_ui
- No `ENJIN2_FRAME_TIMING` define — headless runner has no frame timing atomics; plain uint32_t counters used

## Deviations from Plan

None — plan executed exactly as written. headless_main.cpp structure, CMake target, and verification commands all match the plan specification precisely.

## Issues Encountered

None — binary compiled on first attempt, all 46 tests passed immediately.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `enjin_run` binary is ready for use in Phase 64 (CI pipeline: `enjin_run --profile --frames 100` as benchmark baseline)
- All PROF-01 through PROF-06 requirements satisfied across 63-01 and 63-02
- No SDL3 dependency in `enjin_run` — CI runners do not need a display or SDL3 installed
- Full ctest suite green, no regressions

## Self-Check: PASSED

- `src/platform/headless/headless_main.cpp` — FOUND
- `CMakeLists.txt` contains `add_executable(enjin_run...)` — FOUND
- Commit `dbffe71` (Task 1) — FOUND
- Commit `309780d` (Task 2) — FOUND
- `./build/enjin_run --frames 100 scripts/layer_demo.lua` exits 0 — VERIFIED
- `./build/enjin_run --profile --frames 10 scripts/layer_demo.lua` prints Function/Calls/Line/Source table — VERIFIED
- `./build/enjin_run --profile --output json --frames 10 scripts/layer_demo.lua` prints JSON array — VERIFIED
- 46/46 ctest passes — VERIFIED

---
*Phase: 63-lua-profiler-headless-runner*
*Completed: 2026-03-08*
