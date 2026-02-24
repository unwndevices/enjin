---
phase: 19-palette-foundation
plan: 02
subsystem: graphics
tags: [lua, wasm, emscripten, palette, bindings, typescript]

# Dependency graph
requires:
  - phase: 19-01
    provides: Palette struct, g_palette global, parseHexColor, loadPreset, getColor, setColor, getSize
provides:
  - lua_setPaletteColor registered in registerAll() (RGB integer and hex string overloads)
  - lua_getPaletteColor returning r, g, b multi-return (3 values)
  - lua_loadPalette returning bool success
  - lua_getPaletteSize returning active entry count
  - WASM getPaletteRGB() returning flat Uint8Array(45) live view via typed_memory_view
  - WASM setPaletteColor(index, r, g, b) modifying g_palette
  - WASM loadPalette(name) returning bool success
  - WASM getPaletteSize() returning int
  - TypeScript definitions for all 4 WASM palette functions in Enjin2Module interface
affects:
  - 19-03
  - JavaScript renderer reading palette via getPaletteRGB()
  - Lua game scripts swapping colors via setPaletteColor()

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Lua palette bindings as static LuaBindings member functions in bindings.cpp"
    - "WASM palette bindings outside ENJIN2_BUILD_LUA guard (core graphics, not Lua-only)"
    - "typed_memory_view with static buffer for zero-copy live palette view to JavaScript"
    - "Lua dual-overload via lua_isstring dispatch (string for hex, integers for RGB)"

key-files:
  created: []
  modified:
    - src/scripting/bindings.cpp
    - include/enjin2/scripting/bindings.hpp
    - src/bindings/emscripten_bindings.cpp
    - src/bindings/enjin2.d.ts

key-decisions:
  - "Lua palette functions are static LuaBindings member functions — declared in bindings.hpp, implemented in bindings.cpp, following existing pattern"
  - "WASM palette bindings placed outside ENJIN2_BUILD_LUA guard — palette is core graphics, available regardless of Lua build"
  - "getPaletteRGB uses static uint8_t buf[45] with typed_memory_view — live view not copy, consistent with research Pitfall 3"
  - "lua_setPaletteColor uses lua_isstring(L, 2) dispatch for hex vs RGB integer overloads"

patterns-established:
  - "Lua-WASM parity: both binding layers use the same g_palette global instance"
  - "WASM palette guard: palette bindings never inside #ifdef ENJIN2_BUILD_LUA"

requirements-completed: [PAL-04, PAL-05]

# Metrics
duration: 2min
completed: 2026-02-24
---

# Phase 19 Plan 02: Palette Bindings Summary

**Lua and WASM bindings wired to g_palette: four Lua functions registered in registerAll(), four WASM functions exposed via emscripten_bindings.cpp with matching TypeScript types in Enjin2Module**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-24T08:28:34Z
- **Completed:** 2026-02-24T08:30:01Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Lua setPaletteColor handles both hex string ('#rrggbb') and RGB integer (r, g, b) overloads via lua_isstring dispatch
- Lua getPaletteColor returns r, g, b as 3 separate Lua multi-return values
- Lua loadPalette/getPaletteSize wired to g_palette.loadPreset/getSize
- WASM getPaletteRGB returns Uint8Array(45) live zero-copy view via typed_memory_view with static buffer
- WASM setPaletteColor/loadPalette/getPaletteSize exposed as module-level functions (not class methods)
- TypeScript definitions added to Enjin2Module interface, matching C++ bindings exactly
- Both binding layers share the same g_palette global — modifications from one are visible to the other

## Task Commits

Each task was committed atomically:

1. **Task 1: Add Lua palette bindings** - `49cfebd` (feat)
2. **Task 2: Add WASM palette bindings and TypeScript types** - `c5d4fb4` (feat)

**Plan metadata:** (docs commit — pending)

## Files Created/Modified

- `src/scripting/bindings.cpp` - Added palette.hpp include, 4 static Lua palette functions, registered in registerAll()
- `include/enjin2/scripting/bindings.hpp` - Added 4 static palette function declarations to LuaBindings class
- `src/bindings/emscripten_bindings.cpp` - Added palette.hpp and string includes, 4 WASM palette functions outside ENJIN2_BUILD_LUA guard
- `src/bindings/enjin2.d.ts` - Added Palette section with 4 TypeScript function definitions to Enjin2Module interface

## Decisions Made

- Lua palette functions follow the existing static LuaBindings member function pattern (declared in .hpp, implemented in .cpp) — no new pattern introduced
- WASM palette bindings are NOT behind `#ifdef ENJIN2_BUILD_LUA` — palette is a core graphics feature independent of Lua
- `static uint8_t buf[45]` in getPaletteRGB ensures the typed_memory_view pointer remains valid after function returns (live view semantics)
- `lua_isstring(L, 2)` dispatch chosen over arg count check for the setPaletteColor overload — more robust since hex strings won't be confused for numbers

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Palette feature surface is complete: Lua scripts can swap colors dynamically, JS renderer can read palette RGB data
- Both binding layers share g_palette — consistent runtime state
- Ready for Phase 19-03 (blit-time palette application or display integration)

---
*Phase: 19-palette-foundation*
*Completed: 2026-02-24*

## Self-Check: PASSED

- src/scripting/bindings.cpp: FOUND
- include/enjin2/scripting/bindings.hpp: FOUND
- src/bindings/emscripten_bindings.cpp: FOUND
- src/bindings/enjin2.d.ts: FOUND
- 19-02-SUMMARY.md: FOUND
- Commit 49cfebd (Task 1): FOUND
- Commit c5d4fb4 (Task 2): FOUND
