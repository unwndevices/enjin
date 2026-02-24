---
phase: 22-lua-integration-e2e-validation
plan: "02"
subsystem: scripting/sdl
tags: [lua, sdl3, cmake, integration, e2e, input, runner]

requires:
  - phase: 22-01
    provides: LuaBindings with InputState* and four input polling functions; e2e_parity.lua
  - phase: 21-02
    provides: SDL3 runner with input_platform_poll and game loop structure

provides:
  - enjin2_sdl conditionally links enjin2_lua when ENJIN2_BUILD_LUA=ON via CMake generator expressions
  - LuaScriptSystem wired into sdl_main.cpp with init, loadScript, per-frame setInput/update/draw, and shutdown
  - draw_palette_grid() debug overlay fully removed from SDL3 runner
  - 22-VERIFICATION.md manual sign-off confirming SDL-05 and INP-05

affects:
  - CMakeLists.txt
  - src/platform/sdl/sdl_main.cpp
  - .planning/phases/22-lua-integration-e2e-validation/22-VERIFICATION.md

tech-stack:
  added: []
  patterns:
    - "#ifdef ENJIN2_BUILD_LUA guard pattern for optional Lua code paths in platform runners"
    - "CMake generator expression conditional linking: $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>"
    - "Frame loop order: advance -> poll -> setInput -> update -> draw -> expand_canvas_to_rgb"
    - "Lua error signal: canvas.clear(Pixel4(14)) + stderr log on any callFunction failure"

key-files:
  created:
    - .planning/phases/22-lua-integration-e2e-validation/22-VERIFICATION.md
  modified:
    - CMakeLists.txt
    - src/platform/sdl/sdl_main.cpp

key-decisions:
  - "enjin2_sdl uses CMake generator expressions for conditional Lua linking — zero impact on non-Lua builds"
  - "ENJIN2_BUILD_LUA=1 compile definition enables #ifdef guards in sdl_main.cpp — same pattern as platform guards"
  - "dt computed from SDL_GetTicks() frame delta before Lua calls; max_dt clamp retained; (void)max_dt removed"
  - "lua_rectangle uses lua_type(L,1)==LUA_TSTRING (not lua_isstring) — avoids Lua number-to-string coercion gotcha"

patterns-established:
  - "Platform runner Lua integration pattern: #ifdef-guarded static globals, init before game loop, per-frame setInput/update/draw, shutdown after loop"
  - "lua_type(L,N)==LUA_TSTRING for strict string detection in C bindings (lua_isstring is too permissive — numbers coerce)"

requirements-completed: [SDL-05]

duration: "~30 minutes (including manual verification)"
completed: 2026-02-24
---

# Phase 22 Plan 02: SDL3 Lua Runner Integration + Manual Sign-off Summary

**LuaScriptSystem wired into SDL3 runner via CMake conditional linking and #ifdef-guarded sdl_main.cpp integration, with manual sign-off confirming 15-color grid and live input indicator in e2e_parity.lua.**

## Performance

- **Duration:** ~30 minutes (including manual visual verification)
- **Completed:** 2026-02-24
- **Tasks:** 2 (1 auto + 1 checkpoint:human-verify)
- **Files modified:** 3

## Accomplishments

- CMakeLists.txt updated with generator-expression conditional Lua linking for `enjin2_sdl` — `ENJIN2_BUILD_LUA=ON` links `enjin2_lua` and exposes `ENJIN2_BUILD_LUA=1`; `OFF` builds with zero Lua dependency
- `sdl_main.cpp` gains `#ifdef ENJIN2_BUILD_LUA`-guarded `LuaScriptSystem g_lua`, `LuaCanvas g_lua_canvas`, init/loadScript/setInput/update/draw/shutdown calls with error-to-canvas-14 fallback
- `draw_palette_grid()` function and its render call fully removed — canvas driven exclusively by Lua
- Manual sign-off confirmed: 5×3 color grid (all 15 palette indices) visible and input indicator responds correctly to Up/W key

## Task Commits

Each task was committed atomically:

1. **Task 1: Update CMakeLists.txt and sdl_main.cpp to integrate LuaScriptSystem** — `c22fbb9` (feat)
2. **Task 2: Manual parity sign-off — VERIFICATION.md created** — `a4a34ee` (docs)

## Files Created/Modified

- `CMakeLists.txt` — Added generator-expression conditional Lua linking and ENJIN2_BUILD_LUA compile definition for enjin2_sdl target
- `src/platform/sdl/sdl_main.cpp` — Added LuaScriptSystem integration (#ifdef-guarded), removed draw_palette_grid, computed dt from frame delta
- `.planning/phases/22-lua-integration-e2e-validation/22-VERIFICATION.md` — Manual sign-off with APPROVED status, SDL-05 and INP-05 confirmation, observed behavior documented

## Decisions Made

- CMake generator expressions chosen for conditional Lua linking over if/endif blocks — cleaner, no target duplication, zero impact on non-Lua builds when `ENJIN2_BUILD_LUA=OFF`
- `ENJIN2_BUILD_LUA=1` compile definition propagated via `target_compile_definitions` so `#ifdef` guards in `sdl_main.cpp` activate consistently
- `dt` computed from `SDL_GetTicks() - frame_start` before Lua calls; `(void)max_dt` suppressor removed since `max_dt` is now used

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] lua_rectangle used lua_isstring() instead of lua_type()==LUA_TSTRING**
- **Found during:** Task 2 (manual SDL3 visual verification)
- **Issue:** In Lua, `lua_isstring()` returns true for both strings AND numbers (Lua coerces numbers to strings). The `lua_rectangle` implementation used `lua_isstring(L, 1)` to detect an optional mode string in arg 1. When called as `rectangle(x, y, w, h)` with integer arguments, it incorrectly detected a "mode" string at arg 1, shifted `startIdx` to 2, and mis-read x/y/w/h — producing zero-height rects drawn at wrong coordinates.
- **Fix:** Changed the mode-detection branch to `lua_type(L, 1) == LUA_TSTRING` in `lua_rectangle`. This is the correct pattern for distinguishing actual strings from numbers in the Lua C API.
- **Files modified:** `src/scripting/bindings.cpp` (lines 374, 378)
- **Verification:** Color grid rendered correctly during manual SDL3 verification — all 15 cells visible with correct dimensions and positions
- **Committed in:** `c22fbb9` (incorporated in Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — bug fix)
**Impact on plan:** Fix was required for correct rendering. Without it, all `rectangle()` calls from Lua with positional integer args would produce zero-height rects. No scope creep.

## Issues Encountered

None beyond the lua_rectangle Lua gotcha documented above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 22 is the final phase of v1.3 Tomodachi Readiness. Both plans (22-01 and 22-02) are complete:

- Lua input bindings (`isButtonHeld`, `isButtonJustPressed`, `isButtonJustReleased`, `getAxis`) operational
- E2E parity script (`scripts/e2e_parity.lua`) authored and running in SDL3 runner
- SDL3 runner fully wired to LuaScriptSystem with correct frame loop ordering
- Manual sign-off confirms SDL-05 and INP-05 satisfied

v1.3 milestone is complete. No blockers for tagging v1.3 release.

---

*Phase: 22-lua-integration-e2e-validation*
*Completed: 2026-02-24*
