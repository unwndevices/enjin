# Phase 25: Multi-Layer Canvas Composition - Context

**Gathered:** 2026-02-26
**Status:** Ready for planning

<domain>
## Phase Boundary

The engine renders up to 4 independent Canvas4 layers composited in draw order at blit time, with index 15 as the transparency passthrough and a Lua API for layer selection and clearing. The existing `DrawLayer` enum and `sort_order` system in `C_Drawable` are replaced by a direct buffer index (uint8_t, 0-3). Existing `BlendMode` remains on `C_Drawable` but is only active for grayscale canvas builds — ignored for Pixel4/color projects.

</domain>

<decisions>
## Implementation Decisions

### Composition & Stacking (C++ Engine)
- Layer 0 = backmost (background), layer 3 = frontmost (top) — standard painter's order
- Layer 0 initializes to index 0 (black); layers 1-3 initialize to index 15 (transparent passthrough)
- Compositor walks layers 0→3, takes the topmost non-index-15 value per pixel
- Compositing writes to a **separate output buffer** — individual layer buffers remain untouched after compositing
- Index 15 is **always** passthrough/transparent — it is never a drawable color and drawing index 15 is equivalent to erasing
- All layers are auto-cleared at the start of each frame (layer 0 → index 0, layers 1-3 → index 15)

### DrawLayer Enum Overhaul (C++ Engine)
- The existing `DrawLayer` enum (Default, Background, Entities, Foreground, Overlay, UI) in `drawable.hpp` is **replaced entirely** by a `uint8_t` buffer index (0-3) on `C_Drawable`
- `sort_order` is **dropped entirely** — draw ordering within a buffer is not needed; use different layers for z-ordering
- `shouldDrawBefore()` comparator is either removed or simplified to compare buffer indices only
- `BlendMode` enum stays on `C_Drawable` but is only active for grayscale (Canvas8) builds; for Pixel4/color the compositor ignores it

### Lua API Surface
- Required functions: `setLayer(n)`, `clearLayer(n, color)`, `getLayerCount()`, `getLayer()`
- Additional: `setLayerVisible(n, bool)`, `isLayerVisible(n)` — compositor skips hidden layers
- **One-indexed** in Lua (1-4), internally mapped to C++ buffer indices (0-3) — idiomatic Lua convention
- Named global constants exposed to Lua: `LAYER_BG = 1`, `LAYER_MID = 2`, `LAYER_FG = 3`, `LAYER_UI = 4`

### Default Behavior & Persistence
- Default active layer at startup/init = layer 1 (Lua index, i.e., C++ buffer 0, background)
- `setLayer()` persists across frames — active layer does NOT reset between draw() calls
- All layers are auto-cleared at start of each frame before draw() is called

### Compile-Time Configuration (C++ Engine)
- Layer count is a simple `constexpr` value (e.g., `constexpr uint8_t ENJIN_LAYER_COUNT = 4`)
- Changing this value and rebuilding adjusts the system — no runtime configuration needed

### Intended Usage Convention
- Layer 1 (BG): static background art, tilemap
- Layer 2 (Midground): main gameplay entities, sprites
- Layer 3 (FG): foreground obstacles, near objects, parallax
- Layer 4 (UI): score, health, HUD, menus

### Claude's Discretion
- Out-of-range layer index handling in Lua (clamp vs error — decide based on codebase conventions)
- How layer canvas array is passed to C_LuaScript::draw() (signature change vs stored on LuaBindings)
- Where layer buffers live (new LayerCompositor class vs runner-owned array — decide based on runner structure)
- Compositor performance approach (inner loop optimization for the passthrough check)

</decisions>

<specifics>
## Specific Ideas

- The existing `C_Drawable` `DrawLayer` enum and `sort_order` are a legacy sort-order system — they should be **overhauled or completely redone** to fit the new buffer-based architecture, not just adapted
- `LuaCanvas` currently wraps a single canvas pointer — needs to support switching between layer buffers when `setLayer()` is called
- `LuaBindings::currentCanvas` pointer swap is the likely mechanism for `setLayer()` in the Lua binding layer

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 25-multi-layer-canvas-composition*
*Context gathered: 2026-02-26*
