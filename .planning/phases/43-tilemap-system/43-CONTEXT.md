# Phase 43: Tilemap System - Context

**Gathered:** 2026-02-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Grid-based tilemap rendering and management for level-based games (Arkanoid, platformers, roguelikes, puzzles). Provides a C++ tilemap data structure, a drawable component for rendering tile grids to the canvas, and Lua bindings for creating, editing, and querying tilemaps at runtime. Camera/viewport scrolling is tilemap-scoped (built-in offset). A full engine-wide camera system is a separate future phase.

</domain>

<decisions>
## Implementation Decisions

### Map Structure
- Maps can be larger than the visible screen (scrollable)
- Maximum map size: 64x64 tiles (4,096 tiles, ~4KB for tile indices)
- Single tile layer per tilemap (not multi-layer grid)
- Multiple tilemaps on different compositor layers can overlap visually if needed
- Collision is script-driven — tilemap only stores tile IDs, Lua decides what each ID means

### Tileset & Tiles
- Tile size: 16x16 pixels (10x8 visible tiles on 160x128 canvas)
- Tilesets reuse existing SpriteSheet / .njn binary format — no new asset format
- Works with aseprite2enjin tool (export tileset as grid sprite sheet with 16x16 cells)
- All tiles are static (no built-in animation) — Lua can swap tile IDs manually for animated effects
- 256 tile types per tileset (uint8_t tile IDs, 0-255)
- Tile ID 0 = empty/transparent (not drawn, lets layer background show through) — 255 usable tile types

### Lua API Surface
- Full runtime tile editing: set and get individual tiles by grid coordinate
- Map data initialized from flat Lua table of tile IDs
- Built-in world-to-tile coordinate conversion helpers (pixel coords → tile coords and reverse)
- Built-in tile-at-pixel-position query for collision support

### Rendering & Scrolling
- Built-in camera offset on the tilemap (tilemap-scoped, not engine-wide)
- Renders to LayerCompositor layer 0 (background) by default — consistent with v1.4 convention
- Viewport culling: only tiles visible on screen are drawn each frame (~99 tiles max vs 4,096)
- Tile 0 is transparent — gaps in the map let the layer background show through

### Claude's Discretion
- Whether tilemap is a component (C_Tilemap on an Object, accessed via self:get()) or a global engine API (engine.tilemap.*) — pick what integrates best with existing ECS architecture
- Camera offset implementation details (clamping, smooth scrolling)
- Exact Lua API method names and parameter conventions
- Internal rendering optimization (loop structure, pixel blitting approach)
- How tileset SpriteSheet is associated with the tilemap (constructor parameter, setter method)
- Zero-alloc data structure design (template parameters vs fixed sizes)

</decisions>

<specifics>
## Specific Ideas

- User wants maps larger than the screen for platformers and exploration games
- Reusing SpriteSheet as tileset source is important — leverages existing asset pipeline and aseprite2enjin tool
- Script-driven collision keeps the engine thin — Lua checks tile IDs and decides behavior
- Coordinate conversion helpers (tileAt, toPixel) should be built-in to avoid every game reimplementing the same math
- Animated tile effects can be achieved by Lua swapping tile IDs at runtime (no engine support needed)

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `SpriteSheet` (include/enjin2/graphics/sprite.hpp): Cell-based pixel grid with draw() — natural tileset data source. 16x16 cell size matches tile size decision.
- `LayerCompositor` (include/enjin2/graphics/layer_compositor.hpp): 4-layer compositor. Layer 0 designated for "tilemap/background" per v1.4 convention.
- `C_Drawable` (include/enjin2/components/drawable.hpp): Base for renderable components. Has buffer_index (layer), visibility, draw(ICanvas<Pixel4>&) virtual.
- `ComponentProxy` pattern: Enables Lua `self:get("C_Tilemap")` if tilemap is implemented as a component.
- `aseprite2enjin` tool: Already exports grid sprite sheets — can produce 16x16 tilesets.

### Established Patterns
- Zero dynamic allocation: Fixed-size arrays, compile-time template parameters (Canvas4<W,H>, ObjectCollection max 128)
- Component pattern: C_Timer, C_StateMachine, C_Sprite all follow C_Drawable or Component base with Lua proxy
- Lua bindings: LuaFuncDef arrays + luaBindFunctions, registered in bindings.cpp
- Pixel4 with index 15 = transparent passthrough in compositor
- SpriteSheet::draw() blits individual frames to ICanvas<Pixel4> — tilemap can reuse this for per-tile rendering

### Integration Points
- Scene rendering pipeline: C_Drawable components collected and sorted by buffer_index, drawn in order
- Lua scripting: bindings_*.cpp files register engine.* tables; new bindings_tilemap.cpp would follow same pattern
- Collision system: engine.collision.* exists; tilemap coordinate helpers complement it
- Hot-reload: luaL_ref handles need cleanup on F5 (pattern established in C_Timer, EventBus)

</code_context>

<deferred>
## Deferred Ideas

- Full engine-wide camera system (moves all objects/sprites, not just tilemap viewport) — future phase
- Multi-layer tile grids (ground + decoration + overhead in one tilemap) — future enhancement
- Animated tiles with automatic frame cycling — future enhancement
- Binary map format for fast loading — future enhancement if Lua table loading proves too slow
- Per-tile flags/properties baked into tileset metadata — future enhancement if script-driven collision is insufficient

</deferred>

---

*Phase: 43-tilemap-system*
*Context gathered: 2026-02-28*
