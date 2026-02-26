---
phase: 24-sprite-system-rework
plan: 01
subsystem: graphics
tags: [sprite, animation, pixel4, icanvas, zero-alloc, header-only]

# Dependency graph
requires: []
provides:
  - "SpriteSheet struct — zero-alloc value type with draw(ICanvas<Pixel4>&, frameIndex, x, y)"
  - "AnimMode enum class (Once, Loop, PingPong) for C_Sprite and Lua pool"
  - "Foundational sprite types in include/enjin2/graphics/sprite.hpp"
affects:
  - 24-02  # C_Sprite component depends on SpriteSheet and AnimMode
  - 24-03  # Lua pool depends on SpriteSheet and AnimMode

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Zero-alloc value type pattern: SpriteSheet holds non-owning pointer to caller-managed pixel data"
    - "Compile-time transparency constant: index 15 is transparent, baked into draw() with no parameter"
    - "Header-only inline draw implementation: no separate .cpp needed for SpriteSheet"
    - "Lower-nibble pixel format: raw bytes masked with & 0x0F to extract palette index 0-15"

key-files:
  created: []
  modified:
    - include/enjin2/graphics/sprite.hpp

key-decisions:
  - "Inline draw() implementation in header (no separate .cpp) — matches codebase pattern, keeps SpriteSheet header-only"
  - "Transparency index 15 is a compile-time constant baked into draw(), no matte parameter — per locked decision from RESEARCH.md"
  - "AnimMode defined before SpriteSheet so C_Sprite (Plan 02) can forward-include just the enum without pulling SpriteSheet"
  - "Switched from #ifndef include guard to #pragma once — consistent with icanvas.hpp and canvas.hpp codebase style"
  - "Removed BlendMode dependency (drawable.hpp) — SpriteSheet blit-with-skip is the only drawing behavior; no blend modes"

patterns-established:
  - "SpriteSheet: zero-alloc value type, non-owning const uint8_t* pointer, grid dimensions as uint8_t members"
  - "draw() loop: int16_t fy/fx loop variables, static_cast<uint16_t>(frameIndex)*cellW*cellH for frame offset, & 0x0F mask"

requirements-completed: [SPR-01, SPR-02, SPR-03]

# Metrics
duration: 2min
completed: 2026-02-24
---

# Phase 24 Plan 01: Sprite System Foundation Summary

**SpriteSheet zero-alloc value type with AnimMode enum replacing legacy Sprite class, targeting ICanvas<Pixel4> with compile-time transparency index 15**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-24T22:52:29Z
- **Completed:** 2026-02-24T22:54:40Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments

- Removed legacy `Sprite` class with six public `_`-prefixed fields (`_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode`) and `ICanvas<uint8_t>` draw target
- Introduced `AnimMode` enum class with exactly three values: `Once`, `Loop`, `PingPong`
- Introduced `SpriteSheet` struct: zero-alloc, no virtual methods, no heap, non-owning pointer to caller-managed pixel data
- Implemented `SpriteSheet::draw(ICanvas<Pixel4>&, uint8_t frameIndex, int16_t x, int16_t y)` inline in the header with `& 0x0F` mask and transparent index 15 baked in
- Full project build (SDL=OFF, LUA=OFF, TESTS=OFF) completes with zero errors and zero warnings

## Task Commits

1. **Task 1: Replace legacy Sprite class with SpriteSheet struct and AnimMode enum** - `f1b2745` (feat)

**Plan metadata:** (docs commit to follow)

## SpriteSheet API Reference

### Fields

| Field   | Type           | Description |
|---------|----------------|-------------|
| `data`  | `const uint8_t*` | Raw pixel data; 1 byte per pixel, lower nibble = palette index 0-15 |
| `cellW` | `uint8_t`      | Cell width in pixels |
| `cellH` | `uint8_t`      | Cell height in pixels |
| `cols`  | `uint8_t`      | Number of columns in the grid |
| `rows`  | `uint8_t`      | Number of rows in the grid |

### Methods

| Signature | Returns | Description |
|-----------|---------|-------------|
| `SpriteSheet()` | — | Default constructor; creates empty unusable sheet |
| `SpriteSheet(data, cw, ch, c, r)` | — | Construct with pixel data and grid layout |
| `frameCount() const` | `uint8_t` | Total frames = rows * cols |
| `toIndex(row, col) const` | `uint8_t` | Linear index = row * cols + col |
| `draw(canvas, frameIndex, x, y) const` | `void` | Blit frame to ICanvas<Pixel4>; skips pixels with index 15 |

### AnimMode Enum

```cpp
enum class AnimMode : uint8_t {
    Once,      // Play once, freeze on last frame
    Loop,      // Loop back to frame 0 after last frame
    PingPong   // Reverse direction at each end
};
```

## Files Created/Modified

- `include/enjin2/graphics/sprite.hpp` - Replaced entire file: removed Sprite class, added AnimMode enum and SpriteSheet struct with inline draw() implementation

## Decisions Made

- **Inline draw() in header:** No separate .cpp file created. The draw() implementation is small and self-contained. This matches the codebase's header-only pattern for simple structs and avoids a new CMake source entry.
- **Compile-time transparency constant:** Index 15 is hardcoded in draw(); no matte parameter. Per the locked decision in RESEARCH.md, this is consistent with Phase 25's layer system.
- **Removed BlendMode dependency:** Old Sprite included `drawable.hpp` for `BlendMode`. New SpriteSheet has no blend modes — blit-with-skip is the only behavior.
- **#pragma once:** Switched from old `#ifndef` guard style to `#pragma once`, matching icanvas.hpp and canvas.hpp conventions.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## Build Status

```
cmake -B build_24_check -DENJIN2_BUILD_SDL=OFF -DENJIN2_BUILD_LUA=OFF -DENJIN2_BUILD_TESTS=OFF
cmake --build build_24_check

Result: [100%] Built target ecs_phase4_demo — zero errors, zero warnings
```

Note: `C_Sprite` in `include/enjin2/components/sprite.hpp` still references the old `Sprite` class. This is expected and intentionally resolved in Plan 02.

## Next Phase Readiness

- Plan 02 (C_Sprite component) can now include `sprite.hpp` to get `SpriteSheet` and `AnimMode` without pulling in any component or legacy dependencies
- Plan 03 (Lua pool) similarly depends only on `sprite.hpp` for `SpriteSheet` and `AnimMode`
- No blockers for Plan 02

## User Setup Required

None - no external service configuration required.

## Self-Check: PASSED

- FOUND: `include/enjin2/graphics/sprite.hpp`
- FOUND: `.planning/phases/24-sprite-system-rework/24-01-SUMMARY.md`
- FOUND: commit `f1b2745` in git log

---
*Phase: 24-sprite-system-rework*
*Completed: 2026-02-24*
