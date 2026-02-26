---
phase: 25-multi-layer-canvas-composition
plan: 01
subsystem: graphics
tags: [compositor, canvas, layers, pixel4, cpp, template, header-only]

# Dependency graph
requires:
  - phase: 24-sprite-system-rework
    provides: "ICanvas<Pixel4> interface, Canvas4<W,H> template with raw buffer access"
provides:
  - "LayerCompositor<W,H> template struct with ENJIN_LAYER_COUNT=4 Canvas4 layer buffers plus output"
  - "clearAll() + composite() compositor API using painter's order with index-15 transparency"
  - "C_Drawable.buffer_index (uint8_t) replacing DrawLayer enum and sort_order"
  - "compositor_test executable with 7 passing test functions"
affects:
  - 25-02 (SDL3 integration — compositor output buffer replaces single canvas blit)
  - 25-03 (Lua bindings — setLayer()/clearLayer() use LayerCompositor.layers[])

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "LayerCompositor is a plain template struct (no virtual, no inheritance) — aggregates Canvas4 arrays by value"
    - "Transparency uses palette index 15 as hard-coded passthrough — never a drawable color"
    - "Raw PackedPixel4 buffer walk in composite() hot loop — avoids virtual getPixel/setPixel overhead"
    - "Per-layer visibility flag (visible[]) — compositor skips invisible layers entirely"
    - "ENJIN_LAYER_COUNT constexpr with static_assert range check — compile-time configurable"

key-files:
  created:
    - include/enjin2/graphics/layer_compositor.hpp
    - tests/compositor_test.cpp
  modified:
    - include/enjin2/components/drawable.hpp
    - src/components/drawable.cpp
    - include/enjin2/components/probe.hpp
    - include/enjin2/components/satellite.hpp
    - include/enjin2/components/planet.hpp
    - tests/shadow_mode_test.cpp
    - examples/canvas_demo.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "BUFFER_SIZE is private on Canvas4; use getBufferSize() in LayerCompositor composite() instead of Canvas4<W,H>::BUFFER_SIZE"
  - "DrawLayer enum deleted entirely — uint8_t buffer_index is a direct buffer slot index, not a sort key"
  - "sort_order removed from C_Drawable — layer ordering is entirely by buffer index; within-layer draw order is arrival order in scene"

patterns-established:
  - "C_Drawable callers assign SetBufferIndex(0/1/2/3) to select the canvas layer they draw to"
  - "shouldDrawBefore() now a single comparison: buffer_index < other.buffer_index"

requirements-completed: [LAYER-01, LAYER-02, LAYER-03, LAYER-04]

# Metrics
duration: 22min
completed: 2026-02-26
---

# Phase 25 Plan 01: LayerCompositor Core Infrastructure Summary

**LayerCompositor<W,H> template with 4 Canvas4 layers, painter's-order compositor, index-15 transparency, and uint8_t buffer_index replacing DrawLayer enum across all callsites**

## Performance

- **Duration:** 22 min
- **Started:** 2026-02-26T11:33:27Z
- **Completed:** 2026-02-26T11:55:00Z
- **Tasks:** 3
- **Files modified:** 9 (1 created new header, 1 new test, 7 modified)

## Accomplishments
- Created `layer_compositor.hpp` — header-only `LayerCompositor<W,H>` with `ENJIN_LAYER_COUNT=4`, `clearAll()`, `composite()`, and `visible[]` array
- Deleted `DrawLayer` enum and `sort_order` from `C_Drawable`; replaced with `uint8_t buffer_index` and `SetBufferIndex()`/`GetBufferIndex()`
- Updated all 7+ callsites (probe.hpp, satellite.hpp, planet.hpp, shadow_mode_test.cpp, canvas_demo.cpp, drawable.cpp) — zero DrawLayer references remain in source
- Added `compositor_test` with 7 test functions; all 4 test suite targets pass (input, palette, sprite, compositor)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create LayerCompositor template struct** - `4c58773` (feat)
2. **Task 2: Overhaul C_Drawable and update all callers** - `d2fe030` (feat)
3. **Task 3: Add compositor unit test** - `241a5bd` (feat)

## Files Created/Modified
- `include/enjin2/graphics/layer_compositor.hpp` - LayerCompositor<W,H> struct: ENJIN_LAYER_COUNT, clearAll(), composite(), visible[]
- `tests/compositor_test.cpp` - 7 test functions covering clear, composition, transparency, multi-layer, visibility, constexpr
- `include/enjin2/components/drawable.hpp` - Removed DrawLayer enum + sort_order; added uint8_t buffer_index + SetBufferIndex()/GetBufferIndex(); simplified shouldDrawBefore()
- `src/components/drawable.cpp` - Constructor init: buffer_index(0) replaces layer(DrawLayer::Default) + sort_order(0)
- `include/enjin2/components/probe.hpp` - setDrawLayer(DrawLayer::FOREGROUND) -> SetBufferIndex(2)
- `include/enjin2/components/satellite.hpp` - setDrawLayer(DrawLayer::ENTITIES) -> SetBufferIndex(1)
- `include/enjin2/components/planet.hpp` - setDrawLayer(DrawLayer::BACKGROUND) -> SetBufferIndex(0)
- `tests/shadow_mode_test.cpp` - 3 SetDrawLayer calls updated to SetBufferIndex
- `examples/canvas_demo.cpp` - 3 SetDrawLayer calls updated to SetBufferIndex
- `tests/CMakeLists.txt` - Added compositor_test executable and CTest registration

## Decisions Made
- `Canvas4::BUFFER_SIZE` is private — used `getBufferSize()` method in the compositor hot loop instead of direct template access
- `DrawLayer` enum removed entirely (not adapted) — buffer_index is a raw slot integer, removing the abstraction layer that was causing the legacy bug (FOREGROUND vs Foreground)
- `sort_order` dropped — draw ordering within a buffer is not needed; use different buffer indices for z-ordering

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed private BUFFER_SIZE access in LayerCompositor**
- **Found during:** Task 3 (compositor_test build)
- **Issue:** `Canvas4<W,H>::BUFFER_SIZE` is declared private; the plan specified using it directly, which caused a compile error
- **Fix:** Used `layers[0].getBufferSize()` (public method returning the same value) instead of `Canvas4<W,H>::BUFFER_SIZE`
- **Files modified:** `include/enjin2/graphics/layer_compositor.hpp`
- **Verification:** `compositor_test` builds and all 7 assertions pass
- **Committed in:** `241a5bd` (Task 3 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — bug/compile error in plan's prescribed API usage)
**Impact on plan:** Single-line fix required for correctness. No scope change.

## Issues Encountered
- `getBufferSize()` returns `size_t BUFFER_SIZE` which equals `(W*H)/2` — same value the plan expected from the private constant. Semantically equivalent.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Plan 02 (SDL3 integration): `LayerCompositor` is ready to receive a blit path — SDL3 runner will composite once per frame and upload `output.getBuffer()` to the SDL3 texture
- Plan 03 (Lua bindings): `layers[]` array is accessible on `LayerCompositor`; Lua `setLayer(n)` can swap `LuaCanvas`'s internal pointer to `&layers[n-1]`
- No blockers — all 4 phase-25 requirements marked complete

---
*Phase: 25-multi-layer-canvas-composition*
*Completed: 2026-02-26*
