---
phase: 32-scriptproxy-userdata
plan: 02
subsystem: scripting
tags: [lua, script-proxy, callback-signatures, sdl-runner, hot-reload]

# Dependency graph
requires:
  - phase: 32-01
    provides: ScriptProxy userdata + metatable registration; callWithProxy() for (self, ...) dispatch
  - phase: 28-01
    provides: float seconds dt throughout update chain; accumSec in updateSprite binding
provides:
  - All four Lua scripts migrated to update(self, dt) and draw(self) callback signatures
  - SDL runner direct lua_pcall with nil self before dt for update; nil self for draw
  - pikachu_demo.lua uses updateSprite(sprite, dt) without * 1000 (Phase 28 seconds API)
  - PROXY-04 requirement satisfied
affects: [phase-33, phase-34, phase-35, any future Lua script authoring]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Lua callback signature (self, dt) — self receives proxy (or nil for SDL runner), dt is seconds"
    - "SDL runner pushes nil as self via lua_pushnil before lua_pushnumber(dt) — no callFunction wrapper"

key-files:
  created: []
  modified:
    - scripts/reload_test.lua
    - scripts/layer_demo.lua
    - scripts/pikachu_demo.lua
    - scripts/e2e_parity.lua
    - src/platform/sdl/sdl_main.cpp

key-decisions:
  - "Option A chosen: SDL runner updated to push nil as self before dt (not Option B which would leave scripts on old signature)"
  - "lua_L local variable name used in SDL runner to avoid shadowing any outer scope variable named L"
  - "pikachu_demo dt * 1000 removed — Phase 28 confirmed complete (accumSec present in bindings.cpp)"

patterns-established:
  - "All Lua scripts use (self, dt) and (self) signatures; self=nil is safe for SDL runner because none of the scripts use self in their bodies"
  - "SDL runner direct lua_pcall replaces callFunction wrapper for update/draw — allows explicit nil-self push before dt"

requirements-completed: [PROXY-04]

# Metrics
duration: 2min
completed: 2026-02-27
---

# Phase 32 Plan 02: Script Signature Migration Summary

**Atomic migration of all four Lua scripts to (self, ...) callback signatures plus SDL runner nil-self proxy, with pikachu_demo dt * 1000 removal (Phase 28 seconds API)**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-27T02:46:30Z
- **Completed:** 2026-02-27T02:48:23Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- All four scripts (reload_test.lua, layer_demo.lua, pikachu_demo.lua, e2e_parity.lua) migrated from update(dt)/draw() to update(self, dt)/draw(self)
- SDL runner updated to push nil as self before dt in both update and draw call sites using direct lua_pcall
- pikachu_demo.lua updateSprite call corrected from `dt * 1000` to `dt` — Phase 28 confirmed complete via accumSec in bindings.cpp
- All 9 unit tests pass (100%); SDL runner smoke test starts without Lua errors

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate all four Lua scripts to (self, ...) callback signatures** - `91168ff` (feat)
2. **Task 2: Update SDL runner to push nil as self before dt** - `d00ddff` (feat)

## Files Created/Modified

- `scripts/reload_test.lua` - update(dt)->update(self, dt), draw()->draw(self)
- `scripts/layer_demo.lua` - update(dt)->update(self, dt), draw()->draw(self)
- `scripts/pikachu_demo.lua` - update(dt)->update(self, dt), draw()->draw(self); dt*1000 removed
- `scripts/e2e_parity.lua` - update(dt)->update(self, dt), draw()->draw(self)
- `src/platform/sdl/sdl_main.cpp` - callFunction("update", dt) and callFunction("draw") replaced with direct lua_pcall pushing nil self

## Decisions Made

- **Option A for SDL runner**: Updated SDL runner to push nil as first arg (self), then dt — ensures pikachu_demo.lua receives correct dt value. Option B (keep callFunction unchanged) would have broken pikachu_demo.lua's use of dt inside update().
- **lua_L variable name**: Used `lua_L` instead of `L` to avoid shadowing any outer scope variables in SDL main.
- **Phase 28 confirmation**: Verified `accumSec` present in bindings.cpp before removing `* 1000` — confirms updateSprite expects seconds, not milliseconds.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. Build clean on first attempt. All tests passed immediately.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- PROXY-04 satisfied: all scripts run with new (self, ...) callback signature
- Phase 32 complete: ScriptProxy userdata (32-01) + script signature migration (32-02) both done
- Phase 33 can proceed: all scripts already accept self parameter; callWithProxy() can now wire real proxy objects

## Self-Check: PASSED

All files verified present:
- scripts/reload_test.lua: FOUND
- scripts/layer_demo.lua: FOUND
- scripts/pikachu_demo.lua: FOUND
- scripts/e2e_parity.lua: FOUND
- src/platform/sdl/sdl_main.cpp: FOUND
- .planning/phases/32-scriptproxy-userdata/32-02-SUMMARY.md: FOUND

All commits verified:
- 91168ff (Task 1 - Lua script migration): FOUND
- d00ddff (Task 2 - SDL runner nil-self proxy): FOUND

---
*Phase: 32-scriptproxy-userdata*
*Completed: 2026-02-27*
