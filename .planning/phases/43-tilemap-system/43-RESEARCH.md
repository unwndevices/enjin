# Phase 43: Tilemap System - Research

**Researched:** 2026-02-28
**Domain:** C++ ECS component, zero-alloc tilemap data structure, Lua scripting bindings, SpriteSheet-based tile rendering
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Map Structure
- Maps can be larger than the visible screen (scrollable)
- Maximum map size: 64x64 tiles (4,096 tiles, ~4KB for tile indices)
- Single tile layer per tilemap (not multi-layer grid)
- Multiple tilemaps on different compositor layers can overlap visually if needed
- Collision is script-driven — tilemap only stores tile IDs, Lua decides what each ID means

#### Tileset & Tiles
- Tile size: 16x16 pixels (10x8 visible tiles on 160x128 canvas)
- Tilesets reuse existing SpriteSheet / .njn binary format — no new asset format
- Works with aseprite2enjin tool (export tileset as grid sprite sheet with 16x16 cells)
- All tiles are static (no built-in animation) — Lua can swap tile IDs manually for animated effects
- 256 tile types per tileset (uint8_t tile IDs, 0-255)
- Tile ID 0 = empty/transparent (not drawn, lets layer background show through) — 255 usable tile types

#### Lua API Surface
- Full runtime tile editing: set and get individual tiles by grid coordinate
- Map data initialized from flat Lua table of tile IDs
- Built-in world-to-tile coordinate conversion helpers (pixel coords → tile coords and reverse)
- Built-in tile-at-pixel-position query for collision support

#### Rendering & Scrolling
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

### Deferred Ideas (OUT OF SCOPE)
- Full engine-wide camera system (moves all objects/sprites, not just tilemap viewport) — future phase
- Multi-layer tile grids (ground + decoration + overhead in one tilemap) — future enhancement
- Animated tiles with automatic frame cycling — future enhancement
- Binary map format for fast loading — future enhancement if Lua table loading proves too slow
- Per-tile flags/properties baked into tileset metadata — future enhancement if script-driven collision is insufficient
</user_constraints>

---

## Summary

Phase 43 adds a tilemap system to enjin2. The tilemap is an integral part of level-based game genres (Arkanoid, platformers, roguelikes, puzzle games). This phase is entirely internal to the engine — it reuses all established patterns (SpriteSheet, C_Drawable, ComponentProxy, LuaFuncDef registration) and adds no external dependencies.

The core data structure is a fixed-size 64x64 `uint8_t` grid (4,096 bytes on the stack), paired with a `SpriteSheet` pointer as the tileset. A `C_Tilemap` component extending `C_Drawable` integrates into the existing ECS scene rendering pipeline: it draws only the visible rectangle of tiles onto the target canvas each frame, using `SpriteSheet::draw()` for per-tile blitting. A tilemap-scoped camera offset (`int16_t scrollX, scrollY`) drives viewport culling.

The Lua API follows the established `engine.tilemap.*` or `self:get("C_Tilemap")` ComponentProxy pattern. Tile initialization from a Lua flat table, per-tile get/set, coordinate conversion helpers, and tile-at-pixel queries are all required. Given the existing ECS architecture and the fact that tilemaps are usually scene-level singletons (one map per scene), the **ComponentProxy (C_Tilemap on an Object) approach integrates cleanest** — it reuses `self:get("C_Tilemap")`, the ComponentProxy invalidation lifecycle, and the existing component/scene rendering dispatch without adding a new global API namespace.

**Primary recommendation:** Implement `C_Tilemap` as a `C_Drawable`-derived component. Wire it into the existing `bindings.cpp` ComponentProxy dispatch for `self:get("C_Tilemap")`. Register a `bindings_tilemap.cpp` file for the `C_Tilemap_Proxy` metatable. This reuses all established patterns and requires the smallest net code change.

---

## Standard Stack

### Core (all already in-tree — no new dependencies)

| Library / Type | Version | Purpose | Why Standard |
|---|---|---|---|
| `SpriteSheet` | In-tree | Tileset pixel data source | Reuses .njn format, cellW/cellH already match 16x16 decision |
| `ICanvas<Pixel4>` | In-tree | Render target for tile blitting | Same target as all other C_Drawable components |
| `LayerCompositor` | In-tree | Layer 0 background compositing | Layer 0 designated background by v1.4 convention |
| `C_Drawable` | In-tree | Base class for scene rendering | Integration into Scene::renderObjects() for free |
| `ComponentProxy` | In-tree | Lua proxy userdata for self:get() | Same lifecycle as C_Timer/C_StateMachine proxies |
| `luaL_ref` | Lua 5.4 | No-op for tilemap (no callbacks) | Not needed unless map-change events added later |

### Supporting

| Tool | Purpose | When to Use |
|---|---|---|
| `aseprite2enjin` | Export tileset PNG as .njn with 16x16 cells | For any tileset the Lua script loads via `engine.sprite.load()` |
| `engine.sprite.load()` | Load .njn tileset from disk into LuaBindings asset buffer | Called from Lua init to get SpriteSheet data before constructing tilemap |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|---|---|---|
| `C_Tilemap` (ComponentProxy) | `engine.tilemap.*` global | Global table would require storing tilemap pointer in Lua registry; doesn't participate in scene render pipeline automatically; inconsistent with C_Timer/C_StateMachine pattern |
| Fixed `uint8_t tiles[64*64]` | `uint8_t tiles[W*H]` template params | Template approach forces recompilation for each map size; 4KB fixed array on stack/in Object is acceptable and simpler |
| `SpriteSheet::draw()` per tile | Custom blit loop | `SpriteSheet::draw()` already handles transparency (index 15 passthrough), bounds, and pixel writing correctly |

**Installation:** No new packages needed. All dependencies are in-tree.

---

## Architecture Patterns

### Recommended Project Structure

```
include/enjin2/components/
└── tilemap.hpp              # C_Tilemap class declaration

src/components/
└── tilemap.cpp              # C_Tilemap method implementations (if any non-inline)

src/scripting/
└── bindings_tilemap.cpp     # C_Tilemap_Proxy metatable + Lua API impl

tests/
└── tilemap_test.cpp         # C++ + Lua integration tests
```

`C_Tilemap` follows `C_Timer` and `C_StateMachine` exactly. The proxy metatable follows the same file split as other components.

### Pattern 1: C_Tilemap as C_Drawable Component

**What:** `C_Tilemap` extends `C_Drawable`, stores a `uint8_t tiles[64*64]`, a `SpriteSheet` tileset, map dimensions (mapW, mapH up to 64), and scroll offsets (scrollX, scrollY). Registered with the ComponentProxy system.

**When to use:** Always — the component model gives free scene lifecycle, render pipeline integration, and Lua proxy access via `self:get("C_Tilemap")`.

**Example (header):**
```cpp
// Source: codebase analysis of C_Sprite, C_Timer patterns
class C_Tilemap : public C_Drawable {
public:
    static constexpr uint8_t MAX_MAP_W = 64;
    static constexpr uint8_t MAX_MAP_H = 64;

    C_Tilemap(Object* owner, uint8_t mapW, uint8_t mapH);

    // Tileset
    void setSheet(const SpriteSheet& sheet);

    // Map data init from flat array
    void setTiles(const uint8_t* data, uint8_t w, uint8_t h);

    // Per-tile get/set (0-indexed grid coords)
    void setTile(uint8_t tx, uint8_t ty, uint8_t tileId);
    uint8_t getTile(uint8_t tx, uint8_t ty) const;

    // Camera offset (tilemap-scoped)
    void setScroll(int16_t sx, int16_t sy);
    int16_t getScrollX() const { return m_scrollX; }
    int16_t getScrollY() const { return m_scrollY; }

    // Coordinate helpers
    // pixelToTile: (px, py) world pixel -> (tx, ty) grid coord
    void pixelToTile(int16_t px, int16_t py, int16_t& tx, int16_t& ty) const;
    // tileToPixel: (tx, ty) grid coord -> (px, py) top-left pixel world coord
    void tileToPixel(int16_t tx, int16_t ty, int16_t& px, int16_t& py) const;
    // tileAtPixel: returns tile ID at world pixel position (for collision)
    uint8_t tileAtPixel(int16_t px, int16_t py) const;

    // C_Drawable overrides
    void draw(ICanvas<Pixel4>& canvas) override;
    bool continueToDraw() const override;

private:
    uint8_t  m_tiles[MAX_MAP_W * MAX_MAP_H];  // 4096 bytes, zero-alloc
    SpriteSheet m_sheet;                       // non-owning pointer to pixel data
    uint8_t  m_mapW{0};
    uint8_t  m_mapH{0};
    int16_t  m_scrollX{0};
    int16_t  m_scrollY{0};
};
```

### Pattern 2: Viewport Culling in draw()

**What:** Calculate which tile columns and rows are visible given canvas dimensions and scroll offset. Only iterate that rectangle (~99 tiles on 160x128 canvas with 16x16 tiles).

**When to use:** Every draw() call. This is the only rendering performance concern — 4,096 tiles is too slow to blit unconditionally each frame.

**Example:**
```cpp
// Source: codebase analysis of SpriteSheet::draw() + LayerCompositor dimensions
void C_Tilemap::draw(ICanvas<Pixel4>& canvas) {
    if (!is_visible || !m_sheet.data || m_mapW == 0 || m_mapH == 0) return;

    const int16_t canvasW = static_cast<int16_t>(canvas.getWidth());
    const int16_t canvasH = static_cast<int16_t>(canvas.getHeight());
    const int16_t tileW   = static_cast<int16_t>(m_sheet.cellW);  // 16
    const int16_t tileH   = static_cast<int16_t>(m_sheet.cellH);  // 16

    // First visible tile column/row
    int16_t startTX = static_cast<int16_t>(m_scrollX / tileW);
    int16_t startTY = static_cast<int16_t>(m_scrollY / tileH);
    if (startTX < 0) startTX = 0;
    if (startTY < 0) startTY = 0;

    // Last visible tile column/row (inclusive), clamped to map bounds
    int16_t endTX = static_cast<int16_t>(startTX + (canvasW / tileW) + 1);
    int16_t endTY = static_cast<int16_t>(startTY + (canvasH / tileH) + 1);
    if (endTX > static_cast<int16_t>(m_mapW)) endTX = static_cast<int16_t>(m_mapW);
    if (endTY > static_cast<int16_t>(m_mapH)) endTY = static_cast<int16_t>(m_mapH);

    for (int16_t ty = startTY; ty < endTY; ++ty) {
        for (int16_t tx = startTX; tx < endTX; ++tx) {
            uint8_t tileId = m_tiles[ty * m_mapW + tx];
            if (tileId == 0) continue;  // transparent: tile 0 = skip

            // Pixel position on canvas = tile world position - scroll offset
            int16_t px = static_cast<int16_t>(tx * tileW - m_scrollX);
            int16_t py = static_cast<int16_t>(ty * tileH - m_scrollY);

            m_sheet.draw(canvas, static_cast<uint8_t>(tileId - 1), px, py);
            // Note: tile ID 1..255 maps to SpriteSheet frame 0..254
            // OR tile ID is used directly as frame index if tileset is 0-indexed — see Open Questions
        }
    }
}
```

### Pattern 3: ComponentProxy Registration for C_Tilemap

**What:** Add `C_Tilemap` dispatch to `lua_proxy_get_component_impl` in `bindings.cpp`, and add `bindings_tilemap.cpp` with the `C_Tilemap_Proxy` metatable.

**When to use:** Required for `self:get("C_Tilemap")` access from Lua.

**Example (dispatch entry in bindings.cpp):**
```cpp
// Source: existing pattern from bindings.cpp Phase 40/41 additions
} else if (strcmp(typeName, "C_Tilemap") == 0) {
    comp = owner->getComponent<enjin2::C_Tilemap>();
    metaName = "C_Tilemap_Proxy";
}
```

**Example (bindings_tilemap.cpp proxy methods):**
```cpp
// tilemap:setTile(tx, ty, id)
static int lua_tilemap_setTile(lua_State* L) { ... }
// tilemap:getTile(tx, ty) -> id
static int lua_tilemap_getTile(lua_State* L) { ... }
// tilemap:setScroll(sx, sy)
static int lua_tilemap_setScroll(lua_State* L) { ... }
// tilemap:getScroll() -> sx, sy
static int lua_tilemap_getScroll(lua_State* L) { ... }
// tilemap:setTiles(flat_table, w, h)
static int lua_tilemap_setTiles(lua_State* L) { ... }
// tilemap:pixelToTile(px, py) -> tx, ty
static int lua_tilemap_pixelToTile(lua_State* L) { ... }
// tilemap:tileToPixel(tx, ty) -> px, py
static int lua_tilemap_tileToPixel(lua_State* L) { ... }
// tilemap:tileAtPixel(px, py) -> tileId
static int lua_tilemap_tileAtPixel(lua_State* L) { ... }
// tilemap:setSheet(spriteHandle)  — uses sprite pool handle to set SpriteSheet
static int lua_tilemap_setSheet(lua_State* L) { ... }
```

### Pattern 4: Tileset Binding via Sprite Pool Handle

**What:** Lua scripts load a tileset via `engine.sprite.load("tileset")` which returns a sprite pool handle (0..15). The tilemap proxy's `setSheet(handle)` looks up the `SpriteState` in `LuaBindings::spritePool[handle]` and copies the `SpriteSheet` value into `C_Tilemap`.

**Why:** Keeps the tileset data lifetime under the existing asset buffer system. No new loading path needed.

**Example:**
```lua
-- In Lua init():
local handle = engine.sprite.load("world_tiles")  -- loads .njn
local map = self:get("C_Tilemap")
map:setSheet(handle)
map:setTiles({1,2,3,2, 0,0,0,0, ...}, 16, 12)
```

The `setSheet(handle)` binding retrieves `LuaBindings::spritePool[handle].sheet` and passes it to `C_Tilemap::setSheet()`.

### Pattern 5: Lua Tile Initialization from Flat Table

**What:** `tilemap:setTiles(luaTable, w, h)` iterates `luaTable[1..w*h]` using `lua_rawgeti`, clamping each value to `uint8_t`. Table is 1-indexed in Lua (consistent with existing layer constants).

**Example (binding):**
```cpp
static int lua_tilemap_setTiles(lua_State* L) {
    // proxy on stack [1], table [2], w [3], h [4]
    luaL_checktype(L, 2, LUA_TTABLE);
    uint8_t w = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    uint8_t h = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    // clamp to 64
    if (w > 64) w = 64;
    if (h > 64) h = 64;

    uint8_t buf[64*64] = {};
    int count = w * h;
    for (int i = 0; i < count; ++i) {
        lua_rawgeti(L, 2, i + 1);  // Lua 1-indexed
        buf[i] = static_cast<uint8_t>(lua_tointeger(L, -1) & 0xFF);
        lua_pop(L, 1);
    }
    auto* tm = /* cast proxy->component */;
    tm->setTiles(buf, w, h);
    return 0;
}
```

### Anti-Patterns to Avoid

- **Drawing all 4,096 tiles unconditionally:** Viewport culling is mandatory. On a 160x128 canvas with 16x16 tiles, the visible area is only 10x8 = 80 tiles. Full iteration would be ~50x more work per frame with no visual benefit.
- **Using a global `engine.tilemap.*` table:** This does not integrate with the scene rendering pipeline (which collects `C_Drawable` components), creating a separate render path that diverges from the established pattern.
- **Storing the tileset pixel data inside C_Tilemap:** `SpriteSheet` is already a non-owning value type. The asset buffer in `LuaBindings` owns the pixel data. `C_Tilemap` should store a `SpriteSheet` value (copied from the sprite pool slot), not pointer-to-allocated memory.
- **Tile ID 1..255 as 0-indexed frame:** Tile ID 0 is reserved for "transparent/empty". This means either: (a) tile ID 1 = frame 0, tile ID 2 = frame 1, etc. (subtract 1 before calling `SpriteSheet::draw`), OR (b) tile ID is used as-is as the frame index (frame 0 = blank tile in the tileset). Both are valid; pick one and document it. Option (b) is simpler: Lua uses 0 as the "no tile" sentinel, and the tileset frame at index 0 can be anything (it will never be drawn). This avoids the off-by-one in every draw call.
- **Negative scroll without clamping:** Scroll values can go negative if the camera is at the edge of the map. The viewport culling math must handle negative startTX/startTY correctly (clamp to 0 before loop).

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---|---|---|---|
| Per-tile pixel blitting | Custom blit loop | `SpriteSheet::draw()` | Already handles transparency (index 15), bounds safety, and pixel writing. Duplicating it creates inconsistency. |
| Sprite/tileset loading | New .njn loader | `engine.sprite.load()` + `LuaBindings::spritePool` | The asset buffer, NjnHeader parsing, and sprite pool are all in place. Reuse completely. |
| Asset lifetime management | Custom pixel buffer | `LuaBindings::assetBuffer_` arena | Zero-alloc, fixed 64KB arena already owns all sprite pixel data lifetime. |
| Component lifecycle | Custom init/teardown | `C_Drawable` base + `Component::awake()` / `~Component()` | Scene adds/removes drawables automatically. ComponentProxy invalidation is handled by the base class destructor. |

**Key insight:** The tilemap rendering problem is 99% already solved — it is `SpriteSheet::draw()` called in a loop with viewport culling. The unique work is the data structure and Lua bindings.

---

## Common Pitfalls

### Pitfall 1: Tile ID 0 = Empty Sentinel vs SpriteSheet Frame 0

**What goes wrong:** If tile ID 0 is treated as frame 0 of the SpriteSheet, the "transparent" tile might accidentally blit a real tile from the tileset. If tile ID 1 is treated as frame 0, every draw call subtracts 1 — easy to forget, leading to off-by-one visual errors.

**Why it happens:** The user decision says "tile 0 = transparent, not drawn." The SpriteSheet frame index is 0-based. These two namespaces conflict.

**How to avoid:** Explicitly document which convention is used. Recommended: tile ID 0 = skip (do not call `SpriteSheet::draw`), tile IDs 1..255 = call `SpriteSheet::draw(canvas, tileId, px, py)` directly (the tileset's frame 0 slot is implicitly "wasted" or used as an extra tile type). This is simpler — no subtraction needed in the hot path. Alternatively use `tileId - 1` as the frame index (tileset frame 0 = tile type 1 visually) — just pick one and document it.

**Warning signs:** Tileset frame 0 appearing on screen in places where tile 0 was specified; or valid tiles rendering one frame off.

### Pitfall 2: Negative Scroll Culling Math

**What goes wrong:** If `scrollX` is negative (camera before the start of the map), computing `startTX = scrollX / tileW` yields a negative tile index. The loop using this as an array index reads `m_tiles[-N * m_mapW + ...]` — undefined behavior.

**Why it happens:** The culling math is derived from scroll offsets, which can validly be 0 or positive (showing later portions of the map). Negative values would show beyond the map start — unlikely in practice but possible if scroll is not clamped to [0, maxScroll].

**How to avoid:** Either clamp scroll in `setScroll()` to `[0, (mapW * tileW) - canvasW]` and `[0, (mapH * tileH) - canvasH]`, OR clamp `startTX/startTY` to 0 in `draw()` before the loop. Both are valid. Clamping in `setScroll()` is simpler and prevents invalid state at the source. Note: clamping requires knowing the canvas dimensions at the time of setScroll — which the tilemap does not necessarily have. Simpler to clamp `startTX/startTY` in `draw()`.

**Warning signs:** Out-of-bounds reads, wrong tiles at the top-left of the screen when scroll is near 0.

### Pitfall 3: Sprite Pool Handle Invalidation After Hot-Reload

**What goes wrong:** Lua script stores a sprite pool handle before hot-reload (F5). After reload, `resetSpritePool()` is called, zeroing all sprite pool slots. The tilemap's `SpriteSheet` value holds a now-invalid pixel data pointer (pointing into the old `assetBuffer_` contents, which are overwritten).

**Why it happens:** `LuaBindings::resetSpritePool()` is called on every `registerAll()` (hot-reload). The `assetBuffer_` is reset. Any `SpriteSheet` values already copied into `C_Tilemap` now point to stale data.

**How to avoid:** The `C_Tilemap` component on an `Object` persists across hot-reloads (the Object and its components are not destroyed on F5 — only the Lua state is reset). The Lua `init()` function must re-call `engine.sprite.load()` and `map:setSheet(handle)` and `map:setTiles(...)` after every hot-reload. This is the correct behavior — Lua state is cleared, so the game script's `init()` is responsible for re-initializing all tilemap state. Document this in the API: "Always call `setSheet()` and `setTiles()` from `init()`."

**Warning signs:** Tilemap renders garbage pixels or crashes after F5 reload.

### Pitfall 4: C_Tilemap Constructed Without Map Dimensions

**What goes wrong:** `C_Tilemap` is created via `obj->addComponent<C_Tilemap>(...)` but map width/height are not known at construction time. If `mapW == 0`, the draw loop divides by 0 or skips completely without feedback.

**Why it happens:** `Object::addComponent<T>()` forwards constructor args. If the user constructs without setting dimensions, the default (0x0) map draws nothing silently.

**How to avoid:** In `draw()`, guard: `if (m_mapW == 0 || m_mapH == 0) return;`. In Lua, `setTiles(table, w, h)` sets both the tile data and dimensions atomically — this is the authoritative initialization path.

### Pitfall 5: luaL_ref Handling in setTiles

**What goes wrong:** The `setTiles` Lua binding iterates a potentially large table (up to 4,096 entries). Each `lua_rawgeti` / `lua_pop` pair modifies the stack. An off-by-one in the pop count unbalances the Lua stack.

**Why it happens:** The Lua stack is easy to unbalance in a loop when each iteration pushes one value and should pop exactly one value.

**How to avoid:** The loop body must be exactly `lua_rawgeti(L, 2, i+1); value = lua_tointeger(L, -1); lua_pop(L, 1);`. No other stack operations inside the loop. Verify with `lua_gettop(L)` assertions in debug builds.

---

## Code Examples

Verified patterns from existing codebase:

### Existing SpriteSheet::draw() — the tile blit primitive
```cpp
// Source: include/enjin2/graphics/sprite.hpp (inline, verified)
inline void SpriteSheet::draw(ICanvas<Pixel4>& canvas, uint8_t frameIndex,
                               int16_t x, int16_t y) const {
    if (!data || frameIndex >= frameCount()) return;
    const uint8_t* frame = data + static_cast<uint16_t>(frameIndex) * cellW * cellH;
    for (int16_t fy = 0; fy < static_cast<int16_t>(cellH); ++fy) {
        for (int16_t fx = 0; fx < static_cast<int16_t>(cellW); ++fx) {
            uint8_t px = frame[fy * cellW + fx] & 0x0F;
            if (px != 15) {  // index 15 is transparent
                canvas.setPixel(x + fx, y + fy, Pixel4(px));
            }
        }
    }
}
```
The tilemap `draw()` calls this in a loop. No modification needed.

### Existing ComponentProxy Dispatch Pattern (bindings.cpp)
```cpp
// Source: src/scripting/bindings.cpp — add C_Tilemap entry here
} else if (strcmp(typeName, "C_Tilemap") == 0) {
    comp = owner->getComponent<enjin2::C_Tilemap>();
    metaName = "C_Tilemap_Proxy";
}
```

### Existing registerComponentProxyMetatable Pattern
```cpp
// Source: src/scripting/bindings.cpp — add in registerComponentProxyMetatable()
if (luaL_newmetatable(L, "C_Tilemap_Proxy")) {
    lua_pushcfunction(L, lua_ctilemap_proxy_index_impl);
    lua_setfield(L, -2, "__index");
}
lua_pop(L, 1);
```

### Existing C_Drawable Constructor and draw() Pattern (C_Sprite reference)
```cpp
// Source: include/enjin2/components/sprite.hpp (verified)
C_Sprite(Object* owner, uint8_t width, uint8_t height)
    : C_Drawable(owner, width, height) { ... }

void draw(ICanvas<Pixel4>& canvas) override {
    if (!is_visible || !_sheet.data) return;
    Point pos = GetOffsetPosition();
    _sheet.draw(canvas, _frame, pos.x, pos.y);
}
```
`C_Tilemap` follows this exactly: takes `owner, mapW*tileW, mapH*tileH` as width/height for `C_Drawable` (the drawable extent for anchor/sort math), ignores `GetOffsetPosition()` in favor of scroll-based positioning.

### Existing LuaFuncDef Table Registration Pattern
```cpp
// Source: src/scripting/bindings_engine.cpp (verified)
static const LuaFuncDef kTilemapFuncs[] = {
    {"setTile",       lua_tilemap_setTile},
    {"getTile",       lua_tilemap_getTile},
    {"setTiles",      lua_tilemap_setTiles},
    {"setSheet",      lua_tilemap_setSheet},
    {"setScroll",     lua_tilemap_setScroll},
    {"getScroll",     lua_tilemap_getScroll},
    {"pixelToTile",   lua_tilemap_pixelToTile},
    {"tileToPixel",   lua_tilemap_tileToPixel},
    {"tileAtPixel",   lua_tilemap_tileAtPixel},
};
// NOTE: these are registered via __index dispatch, not luaBindFunctions
// (same as C_Timer_Proxy — __index function checks key and dispatches)
```

### Test Framework Pattern (custom assert, no external framework)
```cpp
// Source: tests/eventbus_test.cpp, timer_test.cpp (verified)
static int passes = 0;
static int failures = 0;
#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else { passes++; } } while(0)

int main() {
    // ... test functions ...
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
```
Tests link against `enjin2` and `enjin2_lua`. Added to `tests/CMakeLists.txt` under `if(ENJIN2_BUILD_LUA)`.

### Sprite Pool Handle -> SpriteSheet Access Pattern
```cpp
// Source: src/scripting/bindings_input_sprites.cpp + bindings_sprite_load.cpp (verified)
// The SpriteState struct in LuaBindings has: SpriteSheet sheet, bool active
// Access pattern in a Lua binding:
static int lua_tilemap_setSheet(lua_State* L) {
    auto* proxy = /* checkudata C_Tilemap_Proxy */;
    auto* tm = static_cast<enjin2::C_Tilemap*>(proxy->component);
    LuaBindings* b = LuaBindings::getBindings(L);
    int handle = static_cast<int>(luaL_checkinteger(L, 2));
    if (handle < 0 || handle >= 16 || !b->spritePool[handle].active) {
        luaL_error(L, "C_Tilemap.setSheet: invalid sprite handle %d", handle);
        return 0;
    }
    tm->setSheet(b->spritePool[handle].sheet);
    return 0;
}
```
Note: `LuaBindings::spritePool` is `private` — `bindings_tilemap.cpp` needs access. Pattern: add `friend class`-style access or expose a getter, OR put the binding implementation in a method of `LuaBindings` (same as how `bindings_input_sprites.cpp` uses `b->spritePool` directly since it's a `LuaBindings` member function).

---

## Architecture Decision: Component vs Global API

**Recommendation: C_Tilemap component (self:get("C_Tilemap") ComponentProxy)**

Evidence:
1. Scene rendering pipeline (`Scene::renderObjects`) collects `C_Drawable` components and draws them sorted by `buffer_index`. A `C_Tilemap` at `buffer_index = 0` (layer 0 = background) is automatically drawn at the correct layer position without any additional wiring.
2. The ComponentProxy invalidation lifecycle (destructor sets `valid = false`) is already implemented in `Component::~Component()`. Tilemap gets this for free.
3. `engine.tilemap.*` global would require storing a raw `C_Tilemap*` pointer in the Lua registry (same pointer-to-pointer injection pattern as `engine.scene.*`) and a separate render hook — significantly more boilerplate for no benefit.
4. Consistency: C_Timer, C_StateMachine, and C_Position all use the ComponentProxy pattern. Game scripts already know `self:get("X")`.

**Lua usage pattern:**
```lua
-- In game Object that holds the tilemap:
function init(self)
    local handle = engine.sprite.load("world_tiles")
    local map = self:get("C_Tilemap")
    map:setSheet(handle)
    map:setTiles({
        1, 1, 1, 1, 2, 1, 1, 1, 1, 1,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        -- ... 64x64 max
    }, 10, 3)
end

function update(self, dt)
    local map = self:get("C_Tilemap")
    -- Script-driven collision: check tile at player position
    local tileId = map:tileAtPixel(playerX, playerY)
    if tileId == 2 then
        -- solid tile: respond
    end
    -- Scroll camera based on player position
    map:setScroll(math.max(0, playerX - 80), 0)
end
```

---

## Open Questions

1. **Tile ID to SpriteSheet frame mapping convention**
   - What we know: Tile ID 0 = transparent (skip), IDs 1..255 are drawable.
   - What's unclear: Does tile ID 1 map to frame 0 (subtract 1 on blit), OR does tile ID map directly to frame index (frame 0 is wasted as the "empty" tile, IDs 0..255 = frames 0..255)?
   - Recommendation: Use tile ID directly as frame index (`tileId` is the frame). Tile 0 = skip (empty). This avoids off-by-one. The tileset's frame 0 slot just needs to be something (even blank) — game authors won't set a tile to ID 0 deliberately. **Decision should be locked in planning.**

2. **spritePool access from bindings_tilemap.cpp**
   - What we know: `LuaBindings::spritePool` is `private`. Existing bindings files that access it are member functions of `LuaBindings`.
   - What's unclear: Should `bindings_tilemap.cpp` contain static `lua_tilemap_*` functions (as friend-accessible via `LuaBindings::getBindings(L)`) or should they be member functions of `LuaBindings`?
   - Recommendation: Follow existing `bindings_input_sprites.cpp` pattern — make the Lua C-function statics call `LuaBindings::getBindings(L)` to get `b`, then access `b->spritePool` via a public accessor method `getSpritePool(handle)` or a new public method `getSpriteSheet(handle)`. Alternatively, declare the tilemap bindings as static member functions of `LuaBindings` (same class as all other bindings). **Either approach works; pick the one that minimizes public API surface.**

3. **Object construction: who calls addComponent\<C_Tilemap\>?**
   - What we know: Existing components are added in C++ (`obj->addComponent<C_Timer>()` in test, or in scene `onCreate()`). There is no Lua-side `engine.scene.addComponent()`.
   - What's unclear: Is `C_Tilemap` always set up in C++ scene code, or should there be a `engine.scene.spawn()` + Lua-side component add path?
   - Recommendation: `C_Tilemap` is added in C++ (scene `onCreate()` or the LuaScene host). Lua only reads/writes via `self:get("C_Tilemap")`. This is consistent with how C_Timer and C_StateMachine are added in tests. For game-level use, the game's scene object adds the component at startup. **No Lua-side component creation needed.**

---

## State of the Art (internal — project decisions)

| Old Approach | Current Approach | When Changed | Impact on Phase 43 |
|---|---|---|---|
| Raw Object/Component access from Lua | ComponentProxy (valid flag, typed metatable) | Phase 39 | C_Tilemap gets ComponentProxy for free with same pattern |
| lightuserdata proxies | Full userdata with metatable | Phase 37 | All proxies use full userdata; tilemap proxy does the same |
| Global function registration | engine.* sub-tables | Phase 31 | Tilemap uses component proxy, not engine.* table |
| Manual luaL_ref cleanup | clearTimers() / clearStates() sentinel pattern | Phase 40/41 | Tilemap has no luaL_ref handles (no callbacks) — no cleanup needed |

**Deprecated/outdated:**
- None relevant to this phase. All patterns used are current.

---

## Sources

### Primary (HIGH confidence)
- In-tree source analysis: `include/enjin2/graphics/sprite.hpp` — SpriteSheet::draw() confirmed inline, transparency at index 15
- In-tree source analysis: `include/enjin2/components/drawable.hpp` — C_Drawable base class, buffer_index, draw() virtual
- In-tree source analysis: `include/enjin2/graphics/layer_compositor.hpp` — Layer 0 = background (clear to Pixel4(0)), Layer 1..N-1 = transparent (Pixel4(15))
- In-tree source analysis: `src/scripting/bindings.cpp` — ComponentProxy dispatch pattern for C_Timer, C_StateMachine; registerComponentProxyMetatable() structure
- In-tree source analysis: `src/scripting/bindings_engine.cpp` — registerEngineTable(), LuaFuncDef array pattern
- In-tree source analysis: `src/scripting/bindings_input_sprites.cpp` — spritePool access in Lua bindings
- In-tree source analysis: `tests/CMakeLists.txt` — ENJIN2_BUILD_LUA guard, add_executable + add_test pattern
- In-tree source analysis: `tests/eventbus_test.cpp`, `tests/timer_test.cpp` — custom ASSERT macro test pattern

### Secondary (MEDIUM confidence)
- Codebase pattern inference: viewport culling formula derived from canvas dimensions (160x128) + tile size (16x16) = 10x8 = 80 visible tiles
- Codebase pattern inference: scroll clamping approach based on existing patterns in similar embedded game engines

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components are in-tree, verified by direct code reading
- Architecture: HIGH — component pattern is directly derived from Phase 39/40/41 code, no inference
- Pitfalls: HIGH for tile ID convention (code analysis) / MEDIUM for hot-reload fragility (inferred from assetBuffer_ reset behavior)
- Lua API: HIGH — follows established ComponentProxy/LuaFuncDef patterns exactly

**Research date:** 2026-02-28
**Valid until:** 2026-03-30 (stable codebase; valid until major architectural refactor)
