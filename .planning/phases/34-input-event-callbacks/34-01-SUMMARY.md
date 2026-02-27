---
phase: 34-input-event-callbacks
plan: "01"
subsystem: scripting
tags: [lua, input, callbacks, edge-detection, C_LuaScript, LuaBindings]

# Dependency graph
requires:
  - phase: 33-scripterrorpolicy
    provides: ScriptErrorPolicy enum + callWithProxy() dispatch pattern + error_policy_test infrastructure
  - phase: 32-scriptproxy-userdata
    provides: ScriptProxy userdata stored in Lua registry keyed by this pointer — reused by callWithProxyAndBtn
  - phase: 31-engine-global-table
    provides: LuaScriptSystem::getBindings() accessor — path to currentInput
provides:
  - on_button_pressed(self, btn) and on_button_released(self, btn) Lua callbacks firing on button edge frames
  - C_LuaScript::setInput(InputState*) for host/test input injection
  - C_LuaScript::callWithProxyAndBtn() private dispatch method
  - C_LuaScript::dispatchInputCallbacks() fired before update() each frame
  - LuaBindings::getInput() const accessor exposing currentInput
  - input_event_callback_test with 9 assertions covering INPUT-01, INPUT-02, INPUT-03 + extras
affects: [phase-35-gc-memory, any phase adding new lifecycle callbacks to C_LuaScript]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "callWithProxyAndBtn(funcName, btn) extends the callWithProxy pattern to integer second argument"
    - "dispatchInputCallbacks() iterates btns 0-15 calling justPressed/justReleased for edge detection"
    - "INPUT-03 ordering: dispatchInputCallbacks() called inside update() BEFORE lastUpdateTime increment and callWithProxy(UPDATE_FUNCTION)"

key-files:
  created:
    - tests/input_event_callback_test.cpp
  modified:
    - include/enjin2/scripting/bindings.hpp
    - include/enjin2/components/lua_script.hpp
    - src/components/lua_script.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "callWithProxyAndBtn() mirrors callWithProxy() error handling exactly — Disable/Log/Panic dispatch applies to input callbacks"
  - "dispatchInputCallbacks() guard checks hasScript/scriptError/scriptSystem — same pre-conditions as callWithProxy"
  - "Input callbacks fire BEFORE update() in same frame (INPUT-03): dispatchInputCallbacks called first in update()"
  - "After dispatchInputCallbacks(), scriptError may be set by Disable policy — update() does NOT re-check scriptError mid-frame (consistent with Phase 33)"
  - "LuaBindings::getInput() is an inline const accessor exposing currentInput — zero overhead, no ABI change"
  - "C_LuaScript::setInput() delegates to scriptSystem->getBindings().setInput() — single injection path"

patterns-established:
  - "Button edge dispatch pattern: iterate 0-15, call justPressed/justReleased, fire named callback per edge"
  - "Optional Lua callbacks: lua_getglobal + lua_isfunction check — not defined silently returns false, not an error"

requirements-completed: [INPUT-01, INPUT-02, INPUT-03]

# Metrics
duration: 3min
completed: 2026-02-27
---

# Phase 34 Plan 01: Input Event Callbacks Summary

**on_button_pressed(self, btn) and on_button_released(self, btn) Lua callbacks wired into C_LuaScript::update() via dispatchInputCallbacks(), firing on button edge frames before update(), verified with 9-assertion ctest test**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-27T16:08:33Z
- **Completed:** 2026-02-27T16:11:42Z
- **Tasks:** 5
- **Files modified:** 4 modified, 1 created

## Accomplishments

- Added `LuaBindings::getInput()` const accessor exposing `currentInput` for use by `C_LuaScript`
- Implemented `callWithProxyAndBtn(funcName, btn)` private method mirroring `callWithProxy()` with ScriptErrorPolicy dispatch
- Implemented `dispatchInputCallbacks(input)` iterating btns 0-15, firing `on_button_pressed`/`on_button_released` on edge frames
- Modified `C_LuaScript::update()` to call `dispatchInputCallbacks()` before `callWithProxy(UPDATE_FUNCTION)` — satisfies INPUT-03
- All 14 tests pass (13 pre-existing + `input_event_callback_test` with 9 assertions)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add getInput() accessor to LuaBindings** - `a276bdd` (feat)
2. **Task 2: Extend lua_script.hpp with new declarations** - `c31ea13` (feat)
3. **Task 3: Implement three new methods in lua_script.cpp** - `a02d126` (feat)
4. **Task 4: Write input_event_callback_test.cpp** - `6216759` (test)
5. **Task 5: Register test in tests/CMakeLists.txt** - `9c8e34d` (chore)

## Files Created/Modified

- `include/enjin2/scripting/bindings.hpp` - Added `getInput() const` inline accessor returning `currentInput`
- `include/enjin2/components/lua_script.hpp` - Added `setInput()` public method + `callWithProxyAndBtn()` + `dispatchInputCallbacks()` private declarations
- `src/components/lua_script.cpp` - Implemented all three new methods; modified `update()` for INPUT-03 ordering
- `tests/input_event_callback_test.cpp` - New test file: 5 test functions, 9 ASSERT checks covering INPUT-01/02/03 and extras
- `tests/CMakeLists.txt` - Registered `input_event_callback_test` inside `if(ENJIN2_BUILD_LUA)` block

## Decisions Made

- `callWithProxyAndBtn()` mirrors `callWithProxy()` error handling exactly — same Disable/Log/Panic dispatch applies to input callbacks so policy behavior is uniform
- `dispatchInputCallbacks()` guard checks `hasScript`/`scriptError`/`scriptSystem` before iterating — consistent with `callWithProxy` pre-conditions
- After `dispatchInputCallbacks()` returns, `update()` does NOT re-check `scriptError` mid-frame — Disable policy error on input callback means update still runs on same frame; caught on next frame by the top-of-update guard (consistent with Phase 33 design)
- `LuaBindings::getInput()` is inline const returning `currentInput` — zero overhead, no new `.cpp` changes needed
- `C_LuaScript::setInput()` delegates to `scriptSystem->getBindings().setInput()` to keep injection path single

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. Build and all 14 tests passed on first attempt.

## Next Phase Readiness

- INPUT-01, INPUT-02, INPUT-03 requirements fully satisfied
- Phase 34 complete — single-plan phase
- Phase 35 (gc/memory) is next and can proceed immediately
- `ctest` passes: 14/14 tests including `input_event_callback_test`

## Self-Check: PASSED

- FOUND: .planning/phases/34-input-event-callbacks/34-01-SUMMARY.md
- FOUND: tests/input_event_callback_test.cpp
- FOUND: include/enjin2/scripting/bindings.hpp
- FOUND: src/components/lua_script.cpp
- FOUND commit a276bdd (Task 1)
- FOUND commit c31ea13 (Task 2)
- FOUND commit a02d126 (Task 3)
- FOUND commit 6216759 (Task 4)
- FOUND commit 9c8e34d (Task 5)

---
*Phase: 34-input-event-callbacks*
*Completed: 2026-02-27*
