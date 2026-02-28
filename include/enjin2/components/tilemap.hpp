#pragma once
#include "drawable.hpp"
#include "../graphics/sprite.hpp"
#include <cstring>

namespace enjin2 {

/**
 * @brief Tilemap component for grid-based level rendering.
 *
 * Stores a fixed-size 64x64 uint8_t tile grid on the stack (zero dynamic
 * allocation). Renders only tiles visible within the canvas viewport
 * (viewport culling). Tile ID 0 is transparent (skipped in draw). IDs 1-255
 * are drawn via SpriteSheet::draw(). Includes a tilemap-scoped camera offset
 * and coordinate conversion helpers.
 *
 * Designed as a C_Drawable component (layer 0 = background by default) that
 * integrates with the existing ECS and Lua binding infrastructure.
 */
class C_Tilemap : public C_Drawable {
public:
    static constexpr uint8_t MAX_MAP_W = 64;
    static constexpr uint8_t MAX_MAP_H = 64;

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
     * @brief Copy tile data into the internal grid.
     *
     * Copies w*h bytes from data into m_tiles. Excess rows/columns are left
     * as zero. Both w and h are clamped to MAX_MAP_W/MAX_MAP_H.
     *
     * @param data  Pointer to tile IDs in row-major order (row 0 first).
     * @param w     Map width in tiles (columns).
     * @param h     Map height in tiles (rows).
     */
    void setTiles(const uint8_t* data, uint8_t w, uint8_t h);

    /**
     * @brief Set a single tile by grid coordinate.
     * @param tx    Column index (0-indexed). Silently ignored if >= m_mapW.
     * @param ty    Row index (0-indexed). Silently ignored if >= m_mapH.
     * @param tileId Tile ID to place (0 = transparent).
     */
    void setTile(uint8_t tx, uint8_t ty, uint8_t tileId);

    /**
     * @brief Get the tile ID at a grid coordinate.
     * @param tx Column index.
     * @param ty Row index.
     * @return Tile ID, or 0 if out of bounds.
     */
    uint8_t getTile(uint8_t tx, uint8_t ty) const;

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
     * @return Tile ID at that position, or 0 if out of bounds / transparent.
     */
    uint8_t tileAtPixel(int16_t px, int16_t py) const;

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
    uint8_t     m_tiles[MAX_MAP_W * MAX_MAP_H];  ///< 4096 bytes, zero-alloc on stack
    SpriteSheet m_sheet;                          ///< Tileset (value copy)
    uint8_t     m_mapW{0};                        ///< Active map width in tiles
    uint8_t     m_mapH{0};                        ///< Active map height in tiles
    int16_t     m_scrollX{0};                     ///< Horizontal camera offset (pixels)
    int16_t     m_scrollY{0};                     ///< Vertical camera offset (pixels)
};

} // namespace enjin2
