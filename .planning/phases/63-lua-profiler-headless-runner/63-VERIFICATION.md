---
phase: 63-lua-profiler-headless-runner
verified: 2026-03-08T08:30:00Z
status: passed
score: 9/9 must-haves verified
re_verification: false
gaps: []
human_verification:
  - test: "Run enjin_run on a script that has a Lua error mid-execution"
    expected: "exit code 1, error printed to stderr"
    why_human: "Error path branches verified by code review but not exercised interactively"
---

# Phase 63: Lua Profiler Headless Runner Verification Report

**Phase Goal:** Developers can profile any Lua script's function-level call counts and GC pressure from the command line without needing a display or SDL3 window
**Verified:** 2026-03-08T08:30:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | LuaProfiler installs a hook via lua_sethook(LUA_MASKCALL) and counts per-function calls | VERIFIED | `install()` calls `lua_sethook(L, hookCallback, LUA_MASKCALL, 0)` at line 149 of `lua_profiler.hpp`; `test_call_count_accuracy` confirms callCount>=5 for 5 calls |
| 2 | LuaProfiler tracks GC memory delta per frame via lua_gc queries | VERIFIED | `headless_main.cpp` lines 163-208: per-frame `lua_gc(LUA_GCCOUNT/GCCOUNTB)` delta ring buffer (256 slots); GC summary printed in text mode |
| 3 | When profiler is disabled via lua_sethook(L, NULL, 0, 0), zero entries are recorded | VERIFIED | `test_zero_overhead_disabled` passes (PROF-03); `headless_main.cpp` line 134: explicit `lua_sethook(L, nullptr, 0, 0)` in the no-profile branch |
| 4 | Scripts calling engine.* subtables run without crash in headless LuaBindings | VERIFIED | `test_null_safety` passes: `engine.time.delta/now`, `engine.lua.memory`, `engine.log`, `engine.input.held`, `engine.scene.find` all run without crash |
| 5 | enjin_run --frames 100 script.lua exits cleanly (exit code 0) without a window or display | VERIFIED | `./build/enjin_run --frames 100 scripts/layer_demo.lua` returned exit code 0 — no SDL3 window, no display required |
| 6 | enjin_run --profile --frames 100 script.lua prints sorted text table of per-function call counts | VERIFIED | `./build/enjin_run --profile --frames 10 scripts/layer_demo.lua` printed Function/Calls/Line/Source table with 12 entries sorted descending by call count |
| 7 | enjin_run --profile --output json --frames 100 script.lua writes valid JSON array | VERIFIED | `./build/enjin_run --profile --output json --frames 10 scripts/layer_demo.lua` produced valid JSON array `[{"name":...,"calls":...,"line":...,"source":...}]` |
| 8 | Scripts exercising every engine.* subtable run without null-dereference crash in headless mode | VERIFIED | layer_demo.lua ran 100 frames without crash; null_safety test exercises all engine.* subtables |
| 9 | Running enjin_run without --profile incurs zero hook overhead | VERIFIED | `headless_main.cpp` line 134: `lua_sethook(L, nullptr, 0, 0)` is explicit in the non-profile branch |

**Score:** 9/9 truths verified

---

## Required Artifacts

### Plan 01 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/enjin2/scripting/lua_profiler.hpp` | LuaProfiler singleton with hookCallback, install, uninstall, reset, printTable, printJSON, sortByCount | VERIFIED | 227 lines; all 7 methods present; Meyer's singleton; `struct LuaProfiler` confirmed |
| `tests/lua_profiler_test.cpp` | Unit tests for hook install/uninstall, call counting, zero-overhead disabled path, null-safety | VERIFIED | 250 lines (> 80 min); 6 test functions, 12 ASSERT checks, all 12 pass |
| `tests/CMakeLists.txt` | lua_profiler_test target inside if(ENJIN2_BUILD_LUA) block | VERIFIED | Lines 588-599: `add_executable(lua_profiler_test...)` inside `if(ENJIN2_BUILD_LUA)` block; `add_test(NAME lua_profiler_test COMMAND lua_profiler_test)` present |

### Plan 02 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/platform/headless/headless_main.cpp` | enjin_run entry point: arg parse, LuaScriptSystem init, null-safe wiring, headless frame loop, profiler output | VERIFIED | 245 lines (> 100 min); all sections present: arg parsing, LuaScriptSystem init, setLayers/setDebugCanvas wiring, profiler install/uninstall, frame loop, GC ring buffer, text/JSON output |
| `CMakeLists.txt` | ENJIN2_BUILD_HEADLESS option and enjin_run executable target | VERIFIED | Lines 377-405: `option(ENJIN2_BUILD_HEADLESS...)` and `add_executable(enjin_run src/platform/headless/headless_main.cpp)` with correct link libs and FATAL_ERROR guards |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `lua_profiler.hpp` | `lua_sethook` | `install()` calls `lua_sethook(L, hookCallback, LUA_MASKCALL, 0)` | WIRED | Line 149 confirmed with exact pattern `lua_sethook.*hookCallback.*LUA_MASKCALL` |
| `tests/lua_profiler_test.cpp` | `lua_profiler.hpp` | `#include <enjin2/scripting/lua_profiler.hpp>` | WIRED | Line 13 confirmed |
| `headless_main.cpp` | `lua_profiler.hpp` | `#include <enjin2/scripting/lua_profiler.hpp>` and `LuaProfiler::get().install(L)` | WIRED | Line 27 (include) and lines 130-131 (`LuaProfiler::get().reset(); LuaProfiler::get().install(L)`) confirmed |
| `headless_main.cpp` | `bindings.hpp` | `setLayers`, `setTimeState`, `tickCoroutines`, `tickTweens` calls | WIRED | Lines 120, 160, 184, 185 confirmed; `setDebugCanvas` at line 121 also wired |
| `CMakeLists.txt` | `headless_main.cpp` | `add_executable(enjin_run src/platform/headless/headless_main.cpp)` | WIRED | Lines 386-388 confirmed |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| PROF-01 | 63-01 | C-level profiler via lua_sethook with per-function call counts | SATISFIED | `hookCallback` with `LUA_MASKCALL` counts every function call; `test_call_count_accuracy` verifies callCount>=5 for 5 invocations. Note: REQUIREMENTS.md mentions `LUA_MASKCALL \| LUA_MASKRET` but implementation uses `LUA_MASKCALL` only — call counting is correct and working; MASKRET is not needed for count-only profiling |
| PROF-02 | 63-01 | Memory tracking via lua_gc with per-frame GC pressure ring buffer | SATISFIED | `test_gc_memory_query` verifies `lua_gc` returns non-zero; `headless_main.cpp` implements 256-slot GC ring buffer with min/max/avg summary |
| PROF-03 | 63-01 | Zero overhead when profiler disabled (lua_sethook(L, NULL, 0, 0)) | SATISFIED | `uninstall()` explicitly calls `lua_sethook(L, NULL, 0, 0)`; `headless_main.cpp` no-profile branch calls `lua_sethook(L, nullptr, 0, 0)`; `test_zero_overhead_disabled` verifies entryCount==0 |
| PROF-04 | 63-02 | Headless CLI runner (enjin_run) with --profile --frames N script.lua | SATISFIED | `enjin_run --frames 100 scripts/layer_demo.lua` exits 0; no SDL3 window, no display required |
| PROF-05 | 63-02 | enjin_run produces JSON and text table output formats | SATISFIED | Text table output confirmed with Function/Calls/Line/Source columns; JSON array confirmed with `{"name","calls","line","source"}` fields |
| PROF-06 | 63-01, 63-02 | enjin_run stubs all platform APIs (gfx, input) as no-ops | SATISFIED | `setInput()` not called (currentInput=nullptr, null-guarded); `test_null_safety` passes: engine.input.held(0), engine.scene.find("x") run without crash; layer canvases wired to prevent draw null-deref |

**Orphaned requirements:** None — all 6 PROF-01..PROF-06 requirements are claimed by plans 63-01 and 63-02.

---

## Commit Verification

All commits documented in SUMMARY files exist in git history:

| Commit | Plan | Description | Verified |
|--------|------|-------------|----------|
| `62986a3` | 63-01 Task 1 | feat(63-01): add LuaProfiler header-only singleton | YES |
| `596aa91` | 63-01 Task 2 | feat(63-01): add lua_profiler_test with 6 unit tests and CMake target | YES |
| `dbffe71` | 63-02 Task 1 | feat(63-02): create headless_main.cpp with frame loop and profiler integration | YES |
| `309780d` | 63-02 Task 2 | feat(63-02): add ENJIN2_BUILD_HEADLESS option and enjin_run CMake target | YES |

---

## Anti-Patterns Found

No anti-patterns found in phase artifacts:

- No TODO/FIXME/PLACEHOLDER comments in `lua_profiler.hpp`, `lua_profiler_test.cpp`, or `headless_main.cpp`
- No stub return patterns (`return null`, `return {}`, `return []`) in any phase file
- No empty handler implementations

---

## Test Execution Results

**lua_profiler_test (46/46 ctest):**
```
=== lua_profiler_test ===
--- hook install/uninstall ---
--- zero overhead disabled (PROF-03) ---
--- call count accuracy (PROF-01) ---
--- GC memory query (PROF-02) ---
--- null safety engine.* headless (PROF-06) ---
null safety test
--- sort by count ---

=== Results: 12 passed, 0 failed ===
```

**enjin_run --frames 100 layer_demo.lua:** exit code 0 (no window, no display)

**enjin_run --profile --frames 10 layer_demo.lua:**
```
Function                                    Calls   Line Source
---------------------------------------- -------- ------ ------
point                                          50     -1 [C]
setLayer                                       40     -1 [C]
setColor                                       40     -1 [C]
rectangle                                      20     -1 [C]
[?]                                            10      4 [string ...]
...

GC pressure (10 frames): min=+0  max=+136  avg=+13 bytes/frame
```

**enjin_run --profile --output json --frames 10 layer_demo.lua:** Valid JSON array produced

**Full ctest suite:** 46/46 passed, 0 failures, 0 regressions

---

## Human Verification Required

### 1. Error Path Execution

**Test:** Create a Lua script with a deliberate runtime error inside `update()`. Run `enjin_run --frames 10 error_script.lua`.
**Expected:** exit code 1, `[lua error] update: ...` printed to stderr
**Why human:** Error branch in the frame loop (lines 172-176 of headless_main.cpp) is code-verified but not exercised in the automated test run above.

---

## Summary

Phase 63 fully achieves its goal. All 9 observable truths are verified against the actual codebase:

- `lua_profiler.hpp` is a complete, substantive header-only singleton with all required methods implemented and wired to Lua's `lua_sethook` API.
- `lua_profiler_test.cpp` is substantive (250 lines, 6 tests, 12 assertions) and all 12 assertions pass.
- `headless_main.cpp` is substantive (245 lines) with complete arg parsing, null-safe LuaScriptSystem wiring, profiler integration, headless frame loop, GC ring buffer, and both output formats.
- `CMakeLists.txt` contains the `ENJIN2_BUILD_HEADLESS` option and `enjin_run` target with correct guards.
- All 6 PROF requirements are satisfied with direct implementation evidence.
- 46/46 ctest tests pass with zero regressions.
- All phase commits exist in git history.

One minor observation: REQUIREMENTS.md describes PROF-01 as using `LUA_MASKCALL | LUA_MASKRET`, but the plan's `must_haves` and implementation use only `LUA_MASKCALL`. This is functionally correct for call-count profiling (MASKRET would only be needed for timing, not counting) and the plan's specification takes precedence. No gap.

---

_Verified: 2026-03-08T08:30:00Z_
_Verifier: Claude (gsd-verifier)_
