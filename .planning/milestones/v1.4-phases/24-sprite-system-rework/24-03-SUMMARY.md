---
phase: 24-sprite-system-rework
plan: 03
subsystem: scripting
tags: [lua, sprite, animation, pool, zero-alloc, bindings, lua_CFunction]

# Dependency graph
requires:
  - phase: 24-01
    provides: "SpriteSheet struct and AnimMode enum in include/enjin2/graphics/sprite.hpp"
provides:
  - "Four Lua global functions: newSprite, drawSprite, updateSprite, setFrame"
  - "SpriteState struct and spritePool[16] fixed member array in LuaBindings"
  - "Zero-heap sprite pool enabling Lua-scripted sprite sheet animation"
affects:
  - 25  # Phase 25 compositor may want to know about the Lua canvas path
  - 26  # lua_CFunction pattern reinforced

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "lua_CFunction static method pattern: getBindings(L) retrieval at function start"
    - "lightuserdata for pixel data pointer: lua_topointer(L, 1) not luaL_checkstring"
    - "Millisecond accumulator animation: accumMs += dtMs; while (accumMs >= frameMs) { accumMs -= frameMs; ++frame; }"
    - "Fixed-size pool with active flag: scan for !active slot, return handle (-1 if full)"

key-files:
  created: []
  modified:
    - include/enjin2/scripting/bindings.hpp
    - include/enjin2/graphics/sprite.hpp
    - src/scripting/bindings.cpp

key-decisions:
  - "sprite.hpp uses canvas.hpp instead of icanvas.hpp to avoid ICanvas<TPixel> redefinition when compiled with Lua target"
  - "lua_drawSprite blits via LuaCanvas::setPixel (type-erased path) rather than SpriteSheet::draw() directly — avoids requiring ICanvas<Pixel4> cast in the binding"
  - "SpriteState struct defined inside LuaBindings private section with C++ in-class initializers — no constructor changes needed"
  - "Pool size is a static constexpr int LUA_SPRITE_POOL_SIZE = 16 — not a macro"

patterns-established:
  - "Handle validation pattern: handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active"
  - "PingPong animation: forward flag inverts at endpoints; total > 1 guard before decrement prevents single-frame oscillation"

requirements-completed: [SPR-06]

# Metrics
duration: 4min
completed: 2026-02-25
---

# Phase 24 Plan 03: Lua Sprite Pool Bindings Summary

**Four Lua globals (newSprite/drawSprite/updateSprite/setFrame) backed by a 16-slot fixed SpriteState pool with millisecond accumulator animation in three modes (Once/Loop/PingPong)**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-24T23:16:07Z
- **Completed:** 2026-02-24T23:23:37Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Added `SpriteState` struct with 8 fields (sheet, fps, accumMs, frame, mode, forward, done, active) inside `LuaBindings` private section
- Added `spritePool[16]` fixed member array — zero heap allocation
- Implemented `lua_newSprite`: scans pool for first inactive slot, initializes SpriteSheet from lightuserdata pointer, returns integer handle 0-15 or -1 if full
- Implemented `lua_drawSprite`: blits current frame via `LuaCanvas::setPixel`, skips index 15 (transparent)
- Implemented `lua_updateSprite`: millisecond accumulator with Once/Loop/PingPong mode handling
- Implemented `lua_setFrame`: direct frame set clamped to [0, frameCount-1], clears accumulator
- Registered all four as Lua globals in `registerAll()` after input polling section
- Fixed pre-existing `ICanvas<TPixel>` redefinition conflict in sprite.hpp (see Deviations)
- `enjin2_lua` target builds cleanly — zero errors

## Lua API Reference

### newSprite(data_lightuserdata, cell_w, cell_h, cols, rows) -> integer

| Argument | Type | Description |
|----------|------|-------------|
| data | lightuserdata | Non-owning pointer to const uint8_t pixel array (caller manages lifetime) |
| cell_w | integer | Cell width in pixels |
| cell_h | integer | Cell height in pixels |
| cols | integer | Number of columns in the sprite sheet grid |
| rows | integer | Number of rows in the sprite sheet grid |

Returns: handle (0-15) on success, -1 if pool is full.

### drawSprite(handle, x, y)

Blits the current frame of the sprite to the active canvas. Pixels with palette index 15 are transparent and are skipped.

### updateSprite(handle, dt_ms)

Advances animation state by `dt_ms` milliseconds using accumulator logic. Respects AnimMode (Once/Loop/PingPong). No-op if `done == true` (Once mode) or `fps <= 0`.

### setFrame(handle, frame_index)

Directly sets the current frame. Clamped to [0, frameCount-1]. Clears `accumMs`. Does not affect `done` or `forward` state.

### Handle Allocation

Pool is scanned linearly for the first inactive slot. Handles are integers 0-15. Maximum 16 concurrent sprites. Returns -1 if all slots are occupied.

### Data Pointer Protocol

Pixel data is passed as a `lightuserdata` (via `lua_topointer`) — the Lua binding never allocates or frees memory. The caller owns the pixel buffer lifetime and must keep it alive as long as the sprite handle is active.

### Animation Accumulator

```
accumMs += dt_ms
frameMs = 1000.0f / fps  (default fps = 8.0)
while (accumMs >= frameMs):
    accumMs -= frameMs   -- carry-over preserved
    advance frame per mode
```

## Task Commits

1. **Task 1: Add SpriteState pool to LuaBindings header** - `31655da` (feat)
2. **Task 2: Implement four sprite lua_CFunction bindings and register them** - `d4032e2` (feat)

**Plan metadata:** (docs commit to follow)

## Files Created/Modified

- `include/enjin2/scripting/bindings.hpp` - Added SpriteState struct, spritePool[16] member, LUA_SPRITE_POOL_SIZE constexpr, four static lua_CFunction declarations, sprite.hpp include
- `include/enjin2/graphics/sprite.hpp` - Fixed include: canvas.hpp instead of icanvas.hpp
- `src/scripting/bindings.cpp` - Added four lua_CFunction implementations + four registerFunction calls in registerAll()

## Decisions Made

- **canvas.hpp instead of icanvas.hpp in sprite.hpp:** When the Lua target is compiled, both bindings.hpp (via canvas.hpp) and sprite.hpp (via icanvas.hpp) pulled in separate `ICanvas<TPixel>` definitions, causing a redefinition error. Fix: sprite.hpp now includes canvas.hpp which is the canonical `ICanvas` used by `Canvas4`/`Canvas8` throughout the codebase.
- **lua_drawSprite via LuaCanvas::setPixel:** The plan specified blitting via `b->currentCanvas->setPixel(x + fx, y + fy, px)` rather than `SpriteSheet::draw()`. This avoids requiring a direct `ICanvas<Pixel4>` reference — the type-erased `LuaCanvas` path handles both 4-bit and 8-bit canvases safely.
- **In-class initializers for SpriteState fields:** C++ in-class initializers (`{8.0f}`, `{0.0f}`, etc.) ensure the pool array is properly initialized without modifying the `LuaBindings` constructor.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed ICanvas<TPixel> redefinition in sprite.hpp**
- **Found during:** Task 1 verification (cmake --build enjin2_lua)
- **Issue:** `sprite.hpp` included `abstract/icanvas.hpp` which defines `ICanvas<TPixel>`. When compiled together with `bindings.hpp` (which includes `canvas.hpp`, also defining `ICanvas<TPixel>`), the compiler reported: `error: redefinition of 'class enjin2::ICanvas<TPixel>'`. Both files use `#pragma once` so the problem was two separate files each defining the same template class.
- **Fix:** Changed `sprite.hpp` to include `../graphics/canvas.hpp` instead of `../abstract/icanvas.hpp`. The `canvas.hpp` ICanvas is the canonical base class used by Canvas4/Canvas8 everywhere else.
- **Files modified:** `include/enjin2/graphics/sprite.hpp`
- **Verification:** `g++ -DVCV_RACK -fsyntax-only bindings.cpp` — no errors; `cmake --build build_24_check --target enjin2_lua` — zero error lines
- **Committed in:** `31655da` (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - Bug)
**Impact on plan:** Pre-existing conflict exposed when Lua target compiled sprite.hpp with bindings.hpp. Fix is correct and necessary — no scope creep.

## Build Status

```
cmake -B build_24_check -DENJIN2_BUILD_SDL=OFF -DENJIN2_BUILD_LUA=ON -DENJIN2_BUILD_TESTS=OFF
cmake --build build_24_check --target enjin2_lua

Result: [100%] Built target enjin2_lua — zero errors, zero warnings
```

## Issues Encountered

- `ICanvas<TPixel>` redefinition conflict between `canvas.hpp` and `icanvas.hpp` — resolved via Rule 1 fix above (see Deviations).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Lua sprite pool complete. Lua scripts can create sprite sheets from raw pixel data buffers, animate them, and blit to the active canvas.
- Phase 25 (compositor) can proceed — no sprite-pool blockers.
- Phase 26 (LuaCallback fix) — no impact from this plan.

## Self-Check: PASSED

- FOUND: `include/enjin2/scripting/bindings.hpp` contains `spritePool`
- FOUND: `src/scripting/bindings.cpp` contains `lua_newSprite` implementation
- FOUND: commit `31655da` in git log (Task 1)
- FOUND: commit `d4032e2` in git log (Task 2)
- FOUND: `.planning/phases/24-sprite-system-rework/24-03-SUMMARY.md` (this file)

---
*Phase: 24-sprite-system-rework*
*Completed: 2026-02-25*
