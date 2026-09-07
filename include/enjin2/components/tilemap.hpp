#pragma once
#include "drawable.hpp"
#include "../graphics/sprite.hpp"
#include "../graphics/tilemap_asset.hpp"
#include <cstring>

namespace enjin2 {

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
            m_sheet.draw(canvas, static_cast<uint8_t>(tileId), px, py);
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
};

} // namespace enjin2
