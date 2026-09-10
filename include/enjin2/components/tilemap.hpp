#pragma once
#include "drawable.hpp"
#include "../graphics/sprite.hpp"
#include "../graphics/tilemap_asset.hpp"
#include <cstring>
#include <cmath>
#include <vector>

namespace enjin2 {

/**
 * @brief The result of a swept slide (`sweepAabb` / `sweepCircle`).
 *
 * Positions are in the tilemap's *map pixel* space (tile (0,0) top-left at
 * (0,0); scroll is a rendering concern and is not applied by the query API).
 */
struct SweepResult {
    float x{0};         ///< Resolved position — AABB top-left / circle centre.
    float y{0};
    float t{1};         ///< Fraction of the requested motion completed (1 = unblocked).
    float normalX{0};   ///< Contact normal (unit); (0,0) when no contact.
    float normalY{0};
    bool  hit{false};   ///< true when the body hit a solid/one-way tile this sweep.
};

/**
 * @brief Tilemap component for grid-based level rendering.
 *
 * Stores a fixed-size 64x64 uint16_t cell grid on the stack (zero dynamic
 * allocation, 8192 bytes). Each cell is packed
 * `[band:1 | vflip:1 | hflip:1 | palbank:4 | tileid:9]` (see tilemap_asset.hpp)
 * — v1 HONORS band (via the presenter's under/over restore filter) and RESERVES
 * palbank + the flip bits (stored, not acted on). Renders only tiles visible
 * within the canvas viewport (viewport culling). Tile id 0 is transparent
 * (skipped in draw). Includes a tilemap-scoped camera offset and coordinate
 * conversion helpers.
 *
 * Designed as a C_Drawable component (layer 0 = background by default) that
 * integrates with the existing ECS and Lua binding infrastructure.
 */
class C_Tilemap : public C_Drawable {
public:
    static constexpr uint8_t MAX_MAP_W = 64;  ///< Maximum tile grid width, in tiles
    static constexpr uint8_t MAX_MAP_H = 64;  ///< Maximum tile grid height, in tiles

    /// @brief Which restore band a cell belongs to (see drawCellBand).
    enum class Band : uint8_t { Under = 0, Over = 1 };

    // ---- Cell packing (thin wrappers over the shared tilemap_asset helpers) --
    static constexpr uint16_t cellTileId(uint16_t cell)  { return tmCellTileId(cell); }
    static constexpr uint8_t  cellPalbank(uint16_t cell) { return tmCellPalbank(cell); }
    static constexpr bool     cellHFlip(uint16_t cell)   { return tmCellHFlip(cell); }
    static constexpr bool     cellVFlip(uint16_t cell)   { return tmCellVFlip(cell); }
    static constexpr uint8_t  cellBand(uint16_t cell)    { return tmCellBand(cell); }
    static constexpr uint16_t packCell(uint16_t tileId, uint8_t band = 0,
                                       uint8_t palbank = 0, bool hflip = false,
                                       bool vflip = false) {
        return tmPackCell(tileId, band, palbank, hflip, vflip);
    }

    /**
     * @brief Constructor.
     * @param owner The object that owns this component.
     *
     * Initialises the tile grid to all zeros (transparent) and sets
     * buffer_index to 0 (background layer, v1.4 convention).
     */
    explicit C_Tilemap(Object* owner);

    // ---- Tileset --------------------------------------------------------

    /**
     * @brief Set the SpriteSheet used as a tileset (value copy).
     * @param sheet SpriteSheet with cell dimensions matching the tile size.
     */
    void setSheet(const SpriteSheet& sheet);

    /**
     * @brief Get the current tileset.
     * @return Const reference to the internal SpriteSheet copy.
     */
    const SpriteSheet& getSheet() const { return m_sheet; }

    // ---- Map data -------------------------------------------------------

    /**
     * @brief Copy a byte tile-id map into the internal grid.
     *
     * Convenience overload: each input byte is a plain tile id (band 0, no
     * flip, palbank 0), widened into a packed uint16 cell. Excess rows/columns
     * are left as zero. Both w and h are clamped to MAX_MAP_W/MAX_MAP_H.
     *
     * @param data  Pointer to tile IDs in row-major order (row 0 first).
     * @param w     Map width in tiles (columns).
     * @param h     Map height in tiles (rows).
     */
    void setTiles(const uint8_t* data, uint8_t w, uint8_t h);

    /**
     * @brief Copy packed uint16 cells into the internal grid (the .njm path).
     *
     * Copies w*h cells verbatim (band/flip/palbank/tileid preserved). Excess
     * rows/columns are left as zero. Both w and h clamp to MAX_MAP_W/MAX_MAP_H.
     *
     * @param data Pointer to packed cells in row-major order (row 0 first).
     * @param w    Map width in tiles (columns).
     * @param h    Map height in tiles (rows).
     */
    void setTiles(const uint16_t* data, uint8_t w, uint8_t h);

    /**
     * @brief Set a single packed cell by grid coordinate.
     * @param tx   Column index (0-indexed). Silently ignored if >= m_mapW.
     * @param ty   Row index (0-indexed). Silently ignored if >= m_mapH.
     * @param cell Packed cell (tile id 0 = transparent).
     */
    void setTile(uint8_t tx, uint8_t ty, uint16_t cell);

    /**
     * @brief Get the packed cell at a grid coordinate.
     * @param tx Column index.
     * @param ty Row index.
     * @return Packed cell, or 0 if out of bounds.
     */
    uint16_t getTile(uint8_t tx, uint8_t ty) const;

    /**
     * @brief Get just the tile id (0-511) at a grid coordinate.
     * @param tx Column index.
     * @param ty Row index.
     * @return Tile id, or 0 if out of bounds / transparent.
     */
    uint16_t getTileId(uint8_t tx, uint8_t ty) const { return cellTileId(getTile(tx, ty)); }

    /**
     * @brief Get the band (0 = under, 1 = over) at a grid coordinate.
     * @param tx Column index.
     * @param ty Row index.
     * @return Band bit, or 0 if out of bounds.
     */
    uint8_t getTileBand(uint8_t tx, uint8_t ty) const { return cellBand(getTile(tx, ty)); }

    // ---- Tile attributes (ADR-0003 §3) -----------------------------------

    /**
     * @brief Set the per-tile attribute table (flat array keyed by tile id).
     *
     * The records are copied into the component's fixed 512-entry table (up to
     * @p count entries). A zero count clears the table so every query reports a
     * passable tile.
     *
     * @param attrs Pointer to @p count TileAttr records (tile 0, 1, …).
     * @param count Number of records (clamped to 512).
     */
    void setAttrs(const TileAttr* attrs, uint16_t count);

    /// @brief The current attribute table (identically TileAttr{} entries when unset).
    const TileAttr* getAttrs() const { return m_attrs; }
    /// @brief Number of attribute records (0 when unset).
    uint16_t getAttrCount() const { return m_attrCount; }

    /**
     * @brief The transform-resolved attribute at a grid cell.
     *
     * DIR is resolved by the cell's flip flags (tmResolveDir) so a flipped
     * directional tile answers for its *flipped* direction. An out-of-bounds
     * cell, a tile without an attribute, or an unset attribute table returns a
     * default (passable, kind 0) TileAttr.
     *
     * @param tx Column index.
     * @param ty Row index.
     * @return Resolved TileAttr.
     */
    TileAttr attrAt(uint8_t tx, uint8_t ty) const;

    /**
     * @brief The resolved attribute at a map-pixel position.
     * @param px Map-pixel X (tile space; scroll not applied).
     * @param py Map-pixel Y.
     * @return Resolved TileAttr (default if out of bounds / unset).
     */
    TileAttr attrAtPixel(int16_t px, int16_t py) const;

    /**
     * @brief Whether the cell at (tx,ty) is SOLID (transform-aware).
     */
    bool isSolid(uint8_t tx, uint8_t ty) const { return attrAt(tx, ty).solid(); }

    // ---- Palette banks ----------------------------------------------------

    /**
     * @brief Set one palette-bank remap (16-entry index→index LUT).
     *
     * The cell's 4-bit palbank field selects which bank's remap recolours the
     * tile when drawn. Bank 0 defaults to identity (no remap).
     *
     * @param index Bank index (0-15).
     * @param remap The index→index LUT for that bank.
     */
    void setPalbank(uint8_t index, const Remap& remap);

    /**
     * @brief Set up to 16 palette-bank remaps from a contiguous array.
     * @param banks Pointer to @p count Remap records (bank 0, 1, …).
     * @param count Number of banks (≤16).
     */
    void setPalbanks(const Remap* banks, uint8_t count);

    /// @brief The remap for one palette bank (identity if unset).
    const Remap& getPalbank(uint8_t index) const { return m_palbanks[index & 0x0F]; }

    // ---- Collision query API (ADR-0003 §3) --------------------------------

    /**
     * @brief Sweep an AABB through the tilemap, sliding along SOLID tiles.
     *
     * Axis-separated: the X axis resolves first, then Y, each sliding along
     * solid (and one-way) tiles. One-way tiles follow the Playdate rule — they
     * block only movement into their facing face and let movement pass from the
     * other side. Positions are map-pixel space (scroll not applied).
     *
     * @param x  Top-left X (map px).
     * @param y  Top-left Y (map px).
     * @param w  Width.
     * @param h  Height.
     * @param vx X velocity (px/s).
     * @param vy Y velocity (px/s).
     * @param dt Timestep (s).
     * @return SweepResult — resolved position, contact normal and t.
     */
    SweepResult sweepAabb(float x, float y, float w, float h,
                          float vx, float vy, float dt) const;

    /**
     * @brief Sweep a circle through the tilemap (see sweepAabb for semantics).
     *
     * Axis-separated slide only ever makes contact at the circle's horizontal /
     * vertical extremes, which coincide with its bounding box, so the circle is
     * resolved through its AABB and the result reports the resolved centre.
     * Corner-rounding is not modelled (slopes + rounded corners are out of
     * scope, ADR-0003 §3).
     *
     * @param cx Circle centre X (map px).
     * @param cy Circle centre Y (map px).
     * @param r  Radius.
     * @param vx X velocity.
     * @param vy Y velocity.
     * @param dt Timestep.
     * @return SweepResult — resolved circle centre, contact normal and t.
     */
    SweepResult sweepCircle(float cx, float cy, float r,
                            float vx, float vy, float dt) const;

    /**
     * @brief Invoke a callback for every cell overlapping a map-pixel AABB.
     *
     * Iterates grid cells whose tiles intersect [x, x+w) × [y, y+h) in map
     * pixel space, row-major, clipped to the map bounds.
     *
     * @param x  AABB X (map px).
     * @param y  AABB Y (map px).
     * @param w  AABB width.
     * @param h  AABB height.
     * @param fn  Callback invoked as fn(tx, ty, cell).
     */
    template <typename Fn>
    void forEachCellIn(float x, float y, float w, float h, Fn&& fn) const {
        const int16_t tileW = static_cast<int16_t>(m_sheet.cellW);
        const int16_t tileH = static_cast<int16_t>(m_sheet.cellH);
        if (tileW <= 0 || tileH <= 0 || w < 0 || h < 0) return;

        int sx = static_cast<int>(std::floor(x / tileW));
        int sy = static_cast<int>(std::floor(y / tileH));
        int ex = static_cast<int>(std::floor((x + w - 1e-6f) / tileW));
        int ey = static_cast<int>(std::floor((y + h - 1e-6f) / tileH));
        if (ex < sx) ex = sx;
        if (ey < sy) ey = sy;

        const int mw = m_mapW;
        const int mh = m_mapH;
        for (int ty = sy; ty <= ey; ++ty) {
            if (ty < 0 || ty >= mh) continue;
            for (int tx = sx; tx <= ex; ++tx) {
                if (tx < 0 || tx >= mw) continue;
                fn(static_cast<uint8_t>(tx), static_cast<uint8_t>(ty),
                   m_tiles[ty * m_mapW + tx]);
            }
        }
    }

    /**
     * @brief Build the merged AABBs covering every SOLID tile of the map.
     *
     * Contiguous solid runs are merged horizontally then vertically into a
     * small set of axis-aligned rectangles (the tile half of a ColliderSet).
     * Returned rects are in map-pixel space.
     *
     * @return A vector of solid rectangles (possibly empty).
     */
    std::vector<Rect> buildSolidRects() const;

    // ---- Dimensions -----------------------------------------------------

    /// @brief Get map width in tiles. @return Column count.
    uint8_t getMapWidth() const { return m_mapW; }
    /// @brief Get map height in tiles. @return Row count.
    uint8_t getMapHeight() const { return m_mapH; }

    // ---- Camera offset --------------------------------------------------

    /**
     * @brief Set the tilemap-scoped camera scroll offset (world pixels).
     * @param sx Horizontal scroll in pixels.
     * @param sy Vertical scroll in pixels.
     */
    void setScroll(int16_t sx, int16_t sy);

    /// @brief Get current horizontal scroll offset (pixels). @return Scroll X.
    int16_t getScrollX() const { return m_scrollX; }
    /// @brief Get current vertical scroll offset (pixels). @return Scroll Y.
    int16_t getScrollY() const { return m_scrollY; }

    // ---- Coordinate helpers ---------------------------------------------

    /**
     * @brief Convert screen-pixel coordinates to tile grid coordinates.
     *
     * Screen pixel (px, py) → world pixel (px + scrollX, py + scrollY) →
     * tile grid (tx, ty). Handles negative world coords with floor division.
     *
     * @param px  Screen X in pixels.
     * @param py  Screen Y in pixels.
     * @param tx  Output: grid column.
     * @param ty  Output: grid row.
     */
    void pixelToTile(int16_t px, int16_t py, int16_t& tx, int16_t& ty) const;

    /**
     * @brief Convert tile grid coordinates to screen-pixel top-left.
     *
     * Grid (tx, ty) → world pixel (tx*tileW, ty*tileH) → screen pixel
     * (world - scroll).
     *
     * @param tx  Grid column.
     * @param ty  Grid row.
     * @param px  Output: screen X.
     * @param py  Output: screen Y.
     */
    void tileToPixel(int16_t tx, int16_t ty, int16_t& px, int16_t& py) const;

    /**
     * @brief Return the tile ID at a world-pixel position.
     *
     * Converts via pixelToTile then bounds-checks. Returns 0 if out of bounds.
     *
     * @param px  Screen X in pixels.
     * @param py  Screen Y in pixels.
     * @return Packed cell at that position, or 0 if out of bounds / transparent.
     */
    uint16_t tileAtPixel(int16_t px, int16_t py) const;

    /**
     * @brief Paint a single 16×16 cell into a canvas, filtered by band.
     *
     * The presenter's under/over restore filter (issue #34/#41): the under-band
     * layer (L0) restores with band Under, the over-band layer (L2) with band
     * Over; actors on L1 render between the two. For compositor tile (tx,ty)
     * this fills the cell's screen region with @p clearColor, then — only if the
     * cell's band matches @p band and its tile id is non-zero — draws the tile
     * over it (index 15 within the tile stays transparent, letting clearColor
     * show through). Cells of the other band, or empty cells, leave just the
     * clear fill. Honors the tilemap scroll offset like draw().
     *
     * @param canvas     Target 4-bit canvas (a compositor layer).
     * @param tx         Grid column of the cell.
     * @param ty         Grid row of the cell.
     * @param band       Which band this call paints (Under → L0, Over → L2).
     * @param clearColor Fill for the cell region before drawing (background for
     *                   the under band, transparent index 15 for the over band).
     */
    void drawCellBand(ICanvas<Pixel4>& canvas, uint16_t tx, uint16_t ty,
                      Band band, Pixel4 clearColor) const {
        const int16_t tileW = static_cast<int16_t>(m_sheet.cellW);
        const int16_t tileH = static_cast<int16_t>(m_sheet.cellH);
        if (tileW == 0 || tileH == 0) {
            return;
        }
        const int16_t px = static_cast<int16_t>((tx * tileW) - m_scrollX);
        const int16_t py = static_cast<int16_t>((ty * tileH) - m_scrollY);

        // Reset the cell region to the clear colour (setPixel bounds-checks).
        for (int16_t yy = 0; yy < tileH; ++yy) {
            for (int16_t xx = 0; xx < tileW; ++xx) {
                canvas.setPixel(static_cast<int16_t>(px + xx),
                                static_cast<int16_t>(py + yy), clearColor);
            }
        }

        if (tx >= m_mapW || ty >= m_mapH) {
            return;
        }
        const uint16_t cell = m_tiles[(ty * m_mapW) + tx];
        const uint16_t tileId = cellTileId(cell);
        if (tileId == 0) {                                    // transparent cell
            return;
        }
        if (cellBand(cell) != static_cast<uint8_t>(band)) {   // other band
            return;
        }
        if (m_sheet.data != nullptr) {
            m_sheet.draw(canvas, tileId, px, py,
                         cellHFlip(cell), cellVFlip(cell),
                         m_palbanks[cellPalbank(cell) & 0x0F]);
        }
    }

    // ---- C_Drawable overrides -------------------------------------------

    /**
     * @brief Render visible tiles to the canvas with viewport culling.
     *
     * Only tiles within the current scroll viewport are iterated. Tile ID 0
     * is skipped (transparent). Tile IDs are passed directly to
     * SpriteSheet::draw() as frameIndex.
     *
     * @param canvas Target 4-bit pixel canvas.
     */
    void draw(ICanvas<Pixel4>& canvas) override;

    /**
     * @brief Draw with camera offset applied (Phase 44: CAM-09).
     *
     * Screen-space tilemaps (m_screenSpace==true) ignore offset and call draw().
     * World-space tilemaps integrate the camera offset with the tilemap's own
     * scroll offset: effective_scroll = m_scrollX - offset.x (additive because
     * getScreenOffset() is the negative of camera position).
     *
     * @param canvas Target 4-bit canvas.
     * @param offset Camera screen offset (= -camera_pos).
     */
    void drawWithOffset(ICanvas<Pixel4>& canvas, Point offset) override;

    /**
     * @brief Continue drawing while the owner object is not queued for removal.
     * @return true if should continue, false otherwise.
     */
    bool continueToDraw() const override;

private:
    uint16_t    m_tiles[MAX_MAP_W * MAX_MAP_H];  ///< 8192 bytes, zero-alloc on stack
    SpriteSheet m_sheet;                          ///< Tileset (value copy)
    uint8_t     m_mapW{0};                        ///< Active map width in tiles
    uint8_t     m_mapH{0};                        ///< Active map height in tiles
    int16_t     m_scrollX{0};                     ///< Horizontal camera offset (pixels)
    int16_t     m_scrollY{0};                     ///< Vertical camera offset (pixels)
    TileAttr    m_attrs[TM_MAX_TILES]{};           ///< Per-tile attributes (owned, one per tile id)
    uint16_t    m_attrCount{0};                    ///< Attribute record count
    Remap       m_palbanks[TM_PALBANK_COUNT]{};    ///< Palette-bank remaps (index = palbank)
};

} // namespace enjin2
