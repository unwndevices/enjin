# Phase 22: Lua Integration + E2E Validation — Manual Sign-off

**Status: APPROVED**
**Date:** 2026-02-24
**Verifier:** User (manual run of `./build_22_sdl_lua/enjin2_sdl`)

---

## Acceptance Criteria

- [x] **SDL-05** — SDL3 runner loads and executes `scripts/e2e_parity.lua`, calling `update(dt)` and `draw()` each frame
- [x] **INP-05** — Lua `isButtonHeld(0)` responds to live SDL3 input; bottom-right indicator cell changes brightness on Up/W held

---

## What Was Observed

The SDL3 runner (`./build_22_sdl_lua/enjin2_sdl`) was run from the project root and the following was confirmed:

1. **Color grid visible** — A 5-column × 3-row grid of colored rectangles filled the top portion of the 512×512 window. All 15 palette colors (indices 0–14) were distinctly visible with no visual overlap or incorrect rendering.

2. **No draw_palette_grid overlay** — The Phase 21 debug overlay is fully removed. The canvas content is driven entirely by the Lua script. No residual overlay was present.

3. **Input indicator working** — Holding the Up arrow key (or W key) caused the bottom-right indicator cell to change to a brighter color. Releasing the key returned the cell to dim. This confirms `isButtonHeld(0)` is wired to live SDL3 keyboard state via `input_platform_poll` → `setInput` → Lua binding.

4. **No crashes or errors** — The runner launched cleanly and ran stably for the duration of the test.

---

## Bug Found and Fixed During Verification

During testing, it was confirmed that `lua_rectangle()` in `src/scripting/bindings.cpp` uses `lua_type(L, 1) == LUA_TSTRING` (not `lua_isstring()`) to detect an optional mode string in the first argument.

**Why this matters:** In Lua, `lua_isstring()` returns true for numbers as well as strings (Lua coerces numbers to strings), which caused `rectangle(x, y, w, h)` calls with integer arguments to incorrectly detect a "mode" string at position 1, shift `startIdx` to 2, and mis-read x/y/w/h — resulting in zero-height rects drawn at wrong coordinates.

**Fix applied:** `lua_type(L, 1) == LUA_TSTRING` is used instead of `lua_isstring(L, 1)` for the mode-detection branch in `lua_rectangle`. This is the correct Lua C API pattern for distinguishing actual strings from numbers that happen to be coercible.

This fix was already present in `bindings.cpp` at the time of SDL3 runner testing; the color grid rendered correctly because `lua_rectangle` arguments were correctly dispatched.

---

## Build Verification (Automated)

Both build configurations were verified clean:

- `cmake -DENJIN2_BUILD_SDL=ON -DENJIN2_BUILD_LUA=ON` → `enjin2_sdl` builds and links `enjin2_lua` with `ENJIN2_BUILD_LUA=1` defined
- `cmake -DENJIN2_BUILD_SDL=ON -DENJIN2_BUILD_LUA=OFF` → `enjin2_sdl` builds without any Lua dependency (no link errors, no compile errors)

---

## Requirements Satisfied

| Requirement | Description | Confirmed |
|-------------|-------------|-----------|
| SDL-05 | SDL3 runner executes Lua scripts via LuaScriptSystem | Yes |
| INP-05 | Lua input bindings return live SDL3 input state | Yes |

---

*Signed off: 2026-02-24 — User confirmed "yes it all works! pass"*
