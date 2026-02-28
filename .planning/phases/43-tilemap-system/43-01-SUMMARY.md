---
phase: 43-tilemap-system
plan: 01
subsystem: components
tags: [tilemap, sprite-sheet, viewport-culling, ecs, c_drawable, zero-alloc]

requires:
  - phase: 23-26-sprite-system
    provides: "SpriteSheet struct with draw() for per-tile blitting"
  - phase: 36-drawable-base
    provides: "C_Drawable base class with buffer_index, visibility, draw(ICanvas<Pixel4>&)"

provides:
  - "C_Tilemap component: fixed 64x64 uint8_t tile grid, zero dynamic allocation"
  - "Viewport-culled draw() rendering visible tiles only"
  - "Tilemap-scoped camera scroll (setScroll/getScrollX/getScrollY)"
  - "Coordinate helpers: pixelToTile, tileToPixel, tileAtPixel"
  - "Tile ID 0 transparent sentinel (skip in hot render path)"
  - "C++ unit tests: TMAP-01..TMAP-04 all passing"

affects:
  - "43-02-lua-bindings: wraps C_Tilemap in Lua self:get('C_Tilemap') ComponentProxy"
  - "44-2d-camera-system: may extend scroll/viewport concepts"

tech-stack:
  added: []
  patterns:
    - "Tile ID 0 = transparent sentinel (frame 0 in tileset wasted to avoid off-by-one in hot path)"
    - "m_mapW stride (not MAX_MAP_W) for flat-table round-trip compatibility with Lua"
    - "Floor division helper for negative scroll/pixel coordinates (C++ truncates toward zero)"
    - "Viewport culling: iterate only startTX..endTX, startTY..endTY tiles each frame"
    - "buffer_index = 0 in constructor (background layer, v1.4 convention)"

key-files:
  created:
    - include/enjin2/components/tilemap.hpp
    - src/components/tilemap.cpp
    - tests/tilemap_test.cpp
  modified:
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Stride = m_mapW (not MAX_MAP_W): tile array is logically m_mapW*m_mapH contiguous from index 0; matches setTiles copy and Lua flat-table indexing"
  - "Tile ID 0 transparent sentinel with direct frameIndex pass-through: no subtract-1 in hot path; tileset frame 0 is wasted"
  - "Floor division for negative pixel/scroll coords: C++ truncation-toward-zero gives wrong results for negative pixels; explicit floor div helper fixes this"
  - "setScroll() stores without clamping; draw() handles negative startTX/startTY by clamping to 0"
  - "C_Tilemap(Object*) constructor only (no w/h params): drawable dimensions not meaningful for tilemap; calls C_Drawable(owner,0,0)"

patterns-established:
  - "Tilemap-scoped scroll: not engine-wide; each C_Tilemap has independent m_scrollX/m_scrollY"
  - "Coordinate helper pixelToTile takes screen coords (not world): adds scroll internally to get world coords"
  - "tileToPixel returns screen coords: world pixel = tile*tileSize, screen = world - scroll"

requirements-completed: [TMAP-01, TMAP-02, TMAP-03, TMAP-04]

duration: 4min
completed: 2026-02-28
---

# Phase 43 Plan 01: Tilemap System C++ Foundation Summary

**C_Tilemap component with 64x64 uint8_t stack-allocated tile grid, SpriteSheet-based viewport-culled rendering, tilemap-scoped scroll, and coordinate conversion helpers — all TMAP-01..TMAP-04 tests passing**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-02-28T17:16:47Z
- **Completed:** 2026-02-28T17:20:16Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- C_Tilemap component compiles as part of enjin2_lua with zero dynamic allocation (4096-byte m_tiles on stack)
- draw() implements viewport culling — only 10x8 tiles iterated on 160x128 canvas, not all 4096
- Tile ID 0 is a transparent sentinel (skipped in hot render loop); IDs 1-255 map directly to SpriteSheet frameIndex
- Coordinate helpers (pixelToTile, tileToPixel, tileAtPixel) handle scroll offsets and negative-pixel floor division correctly
- 8 test functions covering TMAP-01..TMAP-04 all pass; zero regressions in 28 pre-existing tests

## Task Commits

Each task was committed atomically:

1. **Task 1: Create C_Tilemap component header and implementation** - `c1bbfe7` (feat)
2. **Task 2: Create C++ tilemap unit test** - `cb43d8f` (test)

## Files Created/Modified

- `include/enjin2/components/tilemap.hpp` - C_Tilemap class declaration (C_Drawable subclass)
- `src/components/tilemap.cpp` - All method implementations including viewport-culled draw()
- `tests/tilemap_test.cpp` - 8 test functions (TMAP-01..TMAP-04 + extras)
- `CMakeLists.txt` - Added src/components/tilemap.cpp to enjin2_lua target_sources
- `tests/CMakeLists.txt` - Registered tilemap_test in ENJIN2_BUILD_LUA block

## Decisions Made

- **Stride = m_mapW (not MAX_MAP_W):** Internal tile array layout uses m_mapW as the row stride so the tile array is logically m_mapW*m_mapH contiguous bytes from index 0. This matches how setTiles() copies data and how Lua's flat table would index tiles. Using MAX_MAP_W as stride would create gaps and break round-tripping.
- **Tile ID 0 transparent sentinel, direct frameIndex pass-through:** Tile ID is passed directly to SpriteSheet::draw() as frameIndex with no subtract-1. Frame 0 in the tileset is intentionally unused/wasted. This removes an off-by-one subtraction from the hot render path and simplifies Lua authoring (tile IDs 1..N map to frames 1..N).
- **Floor division helper for negative coords:** C++ integer division truncates toward zero; for negative world coordinates (e.g., partial tile scrolled off left edge) this produces wrong tile indices. The floorDiv() helper applies the standard fix (adjust when remainder nonzero and signs differ).
- **setScroll() stores without clamping; draw() handles negatives:** Scroll is stored as-is; the draw() method clamps startTX/startTY to 0 when scroll is negative (scroll < 0 means tilemap origin is to the right of screen left edge).

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. The `sprite_load_test` shows as "Not Run" in the full test suite — this is a pre-existing issue (GTest dependency not available at test runtime) confirmed by verifying it fails identically on the main branch before these changes.

## Next Phase Readiness

- C_Tilemap C++ foundation is complete and tested
- Plan 02 (Lua bindings) can now wrap C_Tilemap via ComponentProxy pattern, registering `self:get('C_Tilemap')` access and Lua-side tilemap methods
- All TMAP-01..TMAP-04 requirements satisfied at the C++ level

---
*Phase: 43-tilemap-system*
*Completed: 2026-02-28*
