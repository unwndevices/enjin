---
phase: 19-palette-foundation
plan: 01
subsystem: graphics
tags: [palette, rgb, pico8, canvas4, cpp, cmake]

requires: []
provides:
  - "RGB struct with constexpr constructors and equality operators"
  - "Palette struct with 15-entry color table, wrapping, and isTransparent/resolve API"
  - "PICO-8 default palette (indices 0-14) and gameboy 4-color preset"
  - "PALETTE_TRANSPARENT constant (index 15) and PALETTE_MAX_ENTRIES constant"
  - "parseHexColor free function supporting #rrggbb and rrggbb formats"
  - "g_palette global instance in enjin2_graphics"
  - "palette_test: 29 assertions covering all palette behavior"
affects:
  - 19-palette-foundation (plans 02+: Lua bindings, WASM bindings, SDL blit)
  - lua-bindings (setPaletteColor/getPaletteColor/loadPalette Lua API)
  - wasm-bindings (getPaletteRGB for JS renderer)
  - sdl-blit (palette lookup at display time)

tech-stack:
  added: []
  patterns:
    - "Transparency-before-modulo: isTransparent(index) checked BEFORE index % size to prevent index 15 folding into smaller palettes"
    - "Global palette instance: g_palette defined once in palette.cpp, extern declared in palette.hpp"
    - "Named preset table: PalettePreset struct with name/colors/size, linear scan for loadPreset()"
    - "Custom test executable: standalone main() with ASSERT macro, no external test framework"

key-files:
  created:
    - include/enjin2/graphics/palette.hpp
    - src/graphics/palette.cpp
    - tests/palette_test.cpp
  modified:
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Transparency check (index == 15) occurs BEFORE modulo wrapping in setColor/getColor/resolve — prevents 4-color gameboy preset from treating index 15 as color 3"
  - "debugTransparent flag stored on Palette struct for future runtime magenta debug mode (default false)"
  - "loadPreset() silently preserves existing colors beyond new preset size (no clear of slots > new size) since size field gates wrapping"
  - "parseHexColor uses sscanf %02x (lowercase) which accepts both upper and lower hex digits per C standard"

patterns-established:
  - "Palette: Always call isTransparent(index) before resolve(index) at every draw site"
  - "Preset colors: defined as static constexpr RGB arrays at file scope in palette.cpp"

requirements-completed: [PAL-01, PAL-02, PAL-03]

duration: 2min
completed: 2026-02-24
---

# Phase 19 Plan 01: Palette Foundation Summary

**PICO-8 15-color palette with transparent index 15, gameboy preset, hex parser, and 29-assertion unit test — core display-time indirection layer for Canvas4**

## Performance

- **Duration:** ~2 min
- **Started:** 2026-02-24T00:24:24Z
- **Completed:** 2026-02-24T00:26:52Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Created `palette.hpp` with RGB struct, Palette struct (setColor/getColor/resolve/isTransparent/loadPreset), PALETTE_TRANSPARENT/MAX_ENTRIES constants, parseHexColor free function, and g_palette extern
- Created `palette.cpp` with DEFAULT_COLORS (exact PICO-8 minus #94b0c2 set), GAMEBOY_COLORS, PalettePreset table, and all method implementations with transparency-before-modulo ordering
- Created `palette_test.cpp` covering all 7 test categories with 29 assertions — all pass

## Task Commits

1. **Task 1: Create Palette struct, presets, and hex parser** - `61a737b` (feat)
2. **Task 2: Create palette unit test** - `0e03672` (feat)

**Plan metadata:** (pending — docs commit)

## Files Created/Modified

- `include/enjin2/graphics/palette.hpp` - Public API: RGB, Palette, constants, parseHexColor, g_palette extern
- `src/graphics/palette.cpp` - Implementation: DEFAULT_COLORS, GAMEBOY_COLORS, preset table, all methods, g_palette definition
- `tests/palette_test.cpp` - 29-assertion unit test covering init, transparency, setColor, wrapping, pitfall, presets, hex parse
- `CMakeLists.txt` - Added palette.cpp to enjin2_graphics target_sources
- `tests/CMakeLists.txt` - Added palette_test executable and add_test registration

## Decisions Made

- Transparency-before-modulo ordering is the critical invariant: `if (index == PALETTE_TRANSPARENT) return early` before any `% size` computation
- `debugTransparent` field kept on struct for future magenta debug mode without API change
- `parseHexColor` uses `%02x` (unsigned) which the C standard specifies accepts both upper and lower hex digits, verified with mixed-case test

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `g_palette`, `Palette`, `RGB`, `parseHexColor` are ready for Lua binding (phase 19 plan 02)
- `g_palette` is ready for WASM `getPaletteRGB()` binding
- `isTransparent()` + `resolve()` pattern is the canonical blit-path API for SDL3 display driver
- No blockers

---
*Phase: 19-palette-foundation*
*Completed: 2026-02-24*
