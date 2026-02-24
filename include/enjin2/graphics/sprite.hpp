#pragma once

#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief Animation playback mode for sprite sheets
 */
enum class AnimMode : uint8_t {
    Once,      ///< Play once, freeze on last frame
    Loop,      ///< Loop back to frame 0 after last frame
    PingPong   ///< Reverse direction at each end
};

/**
 * @brief Zero-alloc sprite sheet value type
 *
 * Holds a non-owning pointer to raw pixel data plus the grid dimensions
 * needed to address individual frames. 1 byte per pixel; only the lower
 * 4 bits of each byte are used as the palette index (0-15).
 *
 * Pixel data lifetime is managed by the caller — SpriteSheet never
 * allocates or frees memory.
 */
struct SpriteSheet {
    const uint8_t* data;   ///< Raw pixel data: 1 byte per pixel, lower nibble = palette index (0-15)
    uint8_t        cellW;  ///< Cell width in pixels
    uint8_t        cellH;  ///< Cell height in pixels
    uint8_t        cols;   ///< Number of columns in the grid
    uint8_t        rows;   ///< Number of rows in the grid

    /// @brief Default constructor — creates an empty, unusable sheet
    SpriteSheet() : data(nullptr), cellW(0), cellH(0), cols(0), rows(0) {}

    /**
     * @brief Construct a sprite sheet with the given pixel data and grid layout
     * @param d   Pointer to raw pixel data (caller owns lifetime)
     * @param cw  Cell width in pixels
     * @param ch  Cell height in pixels
     * @param c   Number of columns in the grid
     * @param r   Number of rows in the grid
     */
    SpriteSheet(const uint8_t* d, uint8_t cw, uint8_t ch, uint8_t c, uint8_t r)
        : data(d), cellW(cw), cellH(ch), cols(c), rows(r) {}

    /// @brief Total number of frames in the sheet (rows * cols)
    uint8_t frameCount() const { return static_cast<uint8_t>(cols * rows); }

    /**
     * @brief Convert (row, col) grid position to a linear frame index
     * @param row  Zero-based row index
     * @param col  Zero-based column index
     * @return     Linear frame index suitable for draw()
     */
    uint8_t toIndex(uint8_t row, uint8_t col) const {
        return static_cast<uint8_t>(row * cols + col);
    }

    /**
     * @brief Blit a single frame to the canvas at (x, y)
     *
     * Pixels with palette index 15 are transparent and are skipped.
     * The transparency index is a compile-time constant — there is no
     * per-call matte parameter.
     *
     * @param canvas      Target canvas (ICanvas<Pixel4>)
     * @param frameIndex  Linear frame index (clamped by frameCount check)
     * @param x           Destination X coordinate on the canvas
     * @param y           Destination Y coordinate on the canvas
     */
    void draw(ICanvas<Pixel4>& canvas, uint8_t frameIndex, int16_t x, int16_t y) const;
};

/**
 * @brief Blit a single sprite frame to the canvas
 *
 * Inline definition kept in the header so that translation units that
 * only include sprite.hpp get the implementation without a separate .cpp.
 */
inline void SpriteSheet::draw(ICanvas<Pixel4>& canvas, uint8_t frameIndex, int16_t x, int16_t y) const {
    if (!data || frameIndex >= frameCount()) return;
    const uint8_t* frame = data + static_cast<uint16_t>(frameIndex) * cellW * cellH;
    for (int16_t fy = 0; fy < static_cast<int16_t>(cellH); ++fy) {
        for (int16_t fx = 0; fx < static_cast<int16_t>(cellW); ++fx) {
            uint8_t px = frame[fy * cellW + fx] & 0x0F;  // lower nibble = palette index
            if (px != 15) {  // index 15 is transparent (compile-time constant per locked decision)
                canvas.setPixel(x + fx, y + fy, Pixel4(px));
            }
        }
    }
}

} // namespace enjin2
