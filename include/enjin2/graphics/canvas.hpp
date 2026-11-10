#pragma once

#include "../core/types.hpp"
#include "../core/memory.hpp"
#include "palette.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif
// Every canvas carries its own sticky text state (ADR-0029)
#include "text_state.hpp"
// Index-shader effect model (Mask x Remap x phase)
#include "effect.hpp"

namespace enjin2
{

/**
 * @file canvas.hpp
 * @brief Canvas abstraction for drawing operations
 *
 * Provides hardware-independent canvas interface for drawing pixels,
 * shapes, and text with support for 4-bit and 8-bit pixel formats.
 */

    /**
      * @brief Abstract canvas interface for drawing operations
      * @tparam TPixel Pixel type (e.g., Pixel4, uint8_t)
     *
     * Provides a hardware-independent interface for all drawing operations.
     * Concrete implementations handle the actual pixel storage and formatting.
     */
    template <typename TPixel>
    class ICanvas
    {
    public:
        /// @brief Pixel type used by this canvas
        using PixelType = TPixel;
        virtual ~ICanvas() = default;

        /**
         * @brief Get canvas width in pixels
         * @return Width in pixels
         */
        virtual uint16_t getWidth() const = 0;

        /**
         * @brief Get canvas height in pixels
         * @return Height in pixels
         */
        virtual uint16_t getHeight() const = 0;

        /**
         * @brief Set pixel color at specified coordinates
         * @param x X coordinate
         * @param y Y coordinate
         * @param color Pixel color to set
         */
        virtual void setPixel(int16_t x, int16_t y, TPixel color) = 0;

        /**
         * @brief Get pixel color at specified coordinates
         * @param x X coordinate
         * @param y Y coordinate
         * @return Pixel color at the specified location
         */
        virtual TPixel getPixel(int16_t x, int16_t y) const = 0;

        /**
         * @brief Clear entire canvas to specified color
         * @param color Color to fill canvas with (default: black)
         */
        virtual void clear(TPixel color = TPixel(0)) = 0;

        /**
         * @brief Fill rectangular region with specified color
         * @param rect Rectangle to fill
         * @param color Color to fill with
         */
        virtual void fill(const Rect &rect, TPixel color) = 0;

        /**
         * @brief Check if coordinates are within canvas bounds
         * @param x X coordinate to check
         * @param y Y coordinate to check
         * @return true if coordinates are valid, false otherwise
         */
        bool inBounds(int16_t x, int16_t y) const
        {
            return x >= 0 && x < getWidth() && y >= 0 && y < getHeight();
        }

        /**
         * @brief Get canvas bounds as rectangle
         * @return Rectangle representing entire canvas area
         */
        Rect getBounds() const
        {
            return Rect(0, 0, getWidth(), getHeight());
        }

        /// @brief This canvas's text state: face, colour, overflow, box width,
        /// cursor, scale, outline, shader and clip window (ADR-0029). Draw text
        /// through a TextRenderer, which reads and sets it.
        TextState &text() { return text_; }
        const TextState &text() const { return text_; }

    private:
        TextState text_{};
    };

    /**
     * @brief 4-bit canvas with packed pixel storage
     * @tparam WIDTH Canvas width in pixels
     * @tparam HEIGHT Canvas height in pixels
     *
     * Rows are stored at a padded byte stride of ROW_BYTES = (WIDTH + 1) / 2,
     * so odd widths are supported: each row starts on a fresh byte and an odd
     * row's final high nibble is padding (written by clear(), never addressed
     * by pixel access). For even widths the layout is identical to the old
     * flat packing.
     */
    template <uint16_t WIDTH, uint16_t HEIGHT>
    class Canvas4 : public ICanvas<Pixel4>
    {
    public:
        /// @brief Pixel type used by this canvas
        using PixelType = Pixel4;

    public:
        /// @brief Side of one dirty/redraw/art tile, in pixels. The one grid:
        /// 16×16 is the art unit, the restore unit and the composite push unit.
        static constexpr uint16_t TILE_SIZE = 16;
        /// @brief Tile columns (ceil(WIDTH / TILE_SIZE)).
        static constexpr uint16_t TILES_X = (WIDTH + TILE_SIZE - 1) / TILE_SIZE;
        /// @brief Tile rows (ceil(HEIGHT / TILE_SIZE)).
        static constexpr uint16_t TILES_Y = (HEIGHT + TILE_SIZE - 1) / TILE_SIZE;
        /// @brief Total tiles in the dirty grid.
        static constexpr uint16_t TILE_COUNT = TILES_X * TILES_Y;

    private:
        static constexpr size_t ROW_BYTES = (WIDTH + 1) / 2;
        static constexpr size_t BUFFER_SIZE = ROW_BYTES * HEIGHT;
        static constexpr size_t DIRTY_BYTES = (TILE_COUNT + 7) / 8;
        PackedPixel4 buffer[BUFFER_SIZE];

        /// @brief One bit per 16×16 tile: set when a primitive (or explicit
        /// invalidate) touched that tile this frame. Reset by clearDirty().
        uint8_t dirtyBits[DIRTY_BYTES];

        size_t getIndex(int16_t x, int16_t y) const
        {
            return y * ROW_BYTES + x / 2;
        }

        bool isLowPixel(int16_t x) const
        {
            return (x % 2) == 0;
        }

        /// @brief Mark the tile that contains pixel (x, y). Assumes in-bounds.
        void markTilePixel(int16_t x, int16_t y)
        {
            const uint16_t t = static_cast<uint16_t>(y / TILE_SIZE) * TILES_X +
                               static_cast<uint16_t>(x / TILE_SIZE);
            dirtyBits[t >> 3] |= static_cast<uint8_t>(1u << (t & 7));
        }

    public:
        /**
         * @brief Constructor initializes clear canvas
         */
        Canvas4()
        {
            clear();
        }

        uint16_t getWidth() const override { return WIDTH; }
        uint16_t getHeight() const override { return HEIGHT; }

        void setPixel(int16_t x, int16_t y, Pixel4 color) override
        {
            if (!inBounds(x, y))
                return;

            size_t index = getIndex(x, y);
            if (isLowPixel(x))
            {
                buffer[index].setLow(color);
            }
            else
            {
                buffer[index].setHigh(color);
            }
            markTilePixel(x, y);
        }

        Pixel4 getPixel(int16_t x, int16_t y) const override
        {
            if (!inBounds(x, y))
                return Pixel4(0);

            size_t index = getIndex(x, y);
            return isLowPixel(x) ? buffer[index].getLow() : buffer[index].getHigh();
        }

        void clear(Pixel4 color = Pixel4(0)) override
        {
            uint8_t packed = (color.value << 4) | color.value;
            memset(buffer, packed, BUFFER_SIZE);
            invalidateAll();
        }

        // ========================================
        // DIRTY-TILE TRACKING
        // ========================================

        /// @brief Mark a single tile (by tile coordinates) dirty. Out-of-range
        /// tile coordinates are ignored.
        void invalidateTile(uint16_t tx, uint16_t ty)
        {
            if (tx >= TILES_X || ty >= TILES_Y)
                return;
            const uint16_t t = ty * TILES_X + tx;
            dirtyBits[t >> 3] |= static_cast<uint8_t>(1u << (t & 7));
        }

        /// @brief Mark every tile touched by a pixel rectangle dirty. The rect
        /// is clipped to the canvas; a zero/negative extent marks nothing.
        void invalidate(int16_t x, int16_t y, int16_t w, int16_t h)
        {
            if (w <= 0 || h <= 0)
                return;
            int16_t x1 = x < 0 ? 0 : x;
            int16_t y1 = y < 0 ? 0 : y;
            int16_t x2 = x + w; // exclusive
            int16_t y2 = y + h;
            if (x2 > WIDTH) x2 = WIDTH;
            if (y2 > HEIGHT) y2 = HEIGHT;
            if (x1 >= x2 || y1 >= y2)
                return;
            const uint16_t tx0 = static_cast<uint16_t>(x1 / TILE_SIZE);
            const uint16_t ty0 = static_cast<uint16_t>(y1 / TILE_SIZE);
            const uint16_t tx1 = static_cast<uint16_t>((x2 - 1) / TILE_SIZE);
            const uint16_t ty1 = static_cast<uint16_t>((y2 - 1) / TILE_SIZE);
            for (uint16_t ty = ty0; ty <= ty1; ++ty)
            {
                for (uint16_t tx = tx0; tx <= tx1; ++tx)
                {
                    const uint16_t t = ty * TILES_X + tx;
                    dirtyBits[t >> 3] |= static_cast<uint8_t>(1u << (t & 7));
                }
            }
        }

        /// @brief Mark all tiles dirty.
        void invalidateAll()
        {
            memset(dirtyBits, 0xFF, DIRTY_BYTES);
        }

        /// @brief Reset the dirty grid — no tiles dirty. Called at the frame
        /// boundary once the compositor has consumed the dirty set.
        void clearDirty()
        {
            memset(dirtyBits, 0, DIRTY_BYTES);
        }

        /// @brief Is the tile at (tx, ty) marked dirty? Out-of-range is false.
        bool isTileDirty(uint16_t tx, uint16_t ty) const
        {
            if (tx >= TILES_X || ty >= TILES_Y)
                return false;
            const uint16_t t = ty * TILES_X + tx;
            return (dirtyBits[t >> 3] & static_cast<uint8_t>(1u << (t & 7))) != 0;
        }

        /// @brief Does any tile carry a dirty mark?
        bool hasDirty() const
        {
            for (size_t i = 0; i < DIRTY_BYTES; ++i)
            {
                if (dirtyBits[i] != 0)
                    return true;
            }
            return false;
        }

        /// @brief Count of tiles currently marked dirty.
        uint16_t dirtyTileCount() const
        {
            uint16_t count = 0;
            for (uint16_t ty = 0; ty < TILES_Y; ++ty)
            {
                for (uint16_t tx = 0; tx < TILES_X; ++tx)
                {
                    if (isTileDirty(tx, ty))
                        ++count;
                }
            }
            return count;
        }

        /// @brief Read-only pointer to the raw dirty-tile bitmap (row-major,
        /// bit `ty*TILES_X + tx`). Used by the compositor to snapshot and merge.
        const uint8_t *dirtyBitmap() const { return dirtyBits; }
        /// @brief Size of the dirty-tile bitmap in bytes.
        static constexpr size_t dirtyBitmapBytes() { return DIRTY_BYTES; }
        /// @brief Overwrite the dirty-tile bitmap from a snapshot of the same
        /// size. Lets the compositor save/restore marks around a restore paint.
        void setDirtyBitmap(const uint8_t *bits)
        {
            memcpy(dirtyBits, bits, DIRTY_BYTES);
        }

        /// @brief Fill a tile's pixel region with a colour WITHOUT marking it
        /// dirty. The compositor's restore step uses this so restored tiles are
        /// tracked via its own restore set rather than re-dirtying the layer
        /// (which would stop a backdrop layer from ever settling).
        void fillTileNoMark(uint16_t tx, uint16_t ty, Pixel4 color)
        {
            const int16_t x0 = static_cast<int16_t>(tx * TILE_SIZE);
            const int16_t y0 = static_cast<int16_t>(ty * TILE_SIZE);
            const int16_t x1 = static_cast<int16_t>(x0 + TILE_SIZE > WIDTH ? WIDTH : x0 + TILE_SIZE);
            const int16_t y1 = static_cast<int16_t>(y0 + TILE_SIZE > HEIGHT ? HEIGHT : y0 + TILE_SIZE);
            for (int16_t py = y0; py < y1; ++py)
            {
                for (int16_t px = x0; px < x1; ++px)
                {
                    size_t index = getIndex(px, py);
                    if (isLowPixel(px))
                        buffer[index].setLow(color);
                    else
                        buffer[index].setHigh(color);
                }
            }
        }

        // ========================================
        // PERFORMANCE OPTIMIZATIONS
        // ========================================

        /**
         * @brief Optimized horizontal line drawing with batch operations
         * @param x Starting x coordinate
         * @param y Y coordinate
         * @param width Line width in pixels
         * @param color Line color
         */
        void drawHLine(int16_t x, int16_t y, int16_t width, Pixel4 color)
        {
            if (y < 0 || y >= HEIGHT || x >= WIDTH || width <= 0)
                return;

            // Clip to canvas bounds
            if (x < 0)
            {
                width += x;
                x = 0;
            }
            if (x + width > WIDTH)
            {
                width = WIDTH - x;
            }

            // Clipping a span that starts far off the left edge can drive width
            // negative; bail before the memset fast path reads a negative count.
            if (width <= 0)
                return;

            // Mark the row's tile span dirty (the packed writes below bypass
            // setPixel).
            invalidate(x, y, width, 1);

            // Nibble-aware span writer, any start/width parity: an odd start
            // owns the high nibble of its byte, the byte-aligned middle is one
            // memset, and an odd remainder owns the low nibble of the last byte.
            PackedPixel4 *p = &buffer[getIndex(x, y)];
            if (x & 1)
            {
                (p++)->setHigh(color);
                --width;
            }
            const size_t pairs = static_cast<size_t>(width) >> 1;
            if (pairs != 0)
            {
                const uint8_t packed = static_cast<uint8_t>((color.value << 4) | color.value);
                memset(p, packed, pairs);
                p += pairs;
            }
            if (width & 1)
                p->setLow(color);
        }

        /**
         * @brief Bayer-dithered horizontal span: fill `[x, x+width)` on row `y`
         *        with `color` wherever `Mask::bayer(level)` is set.
         *
         * Paints exactly the pixels the per-pixel path
         * `Mask::bayer(level).sample(ax + phaseX, y + phaseY)` would, so the
         * pattern is screen-anchored (R4 "one grid") and a span split across
         * dirty tiles stays seamless. Unmasked pixels are left untouched. The
         * 4×4 row is resolved once per call into two nibble-select bytes, and
         * the span is written byte-wise, so there is no per-pixel matrix lookup.
         * Dirty tiles are marked as @ref drawHLine does; a span that paints
         * nothing (level 0, or a row with no set bits) marks nothing.
         *
         * @param x      Starting x coordinate.
         * @param y      Y coordinate.
         * @param width  Span width in pixels.
         * @param color  Fill colour.
         * @param level  Bayer coverage 0..16 (clamped): 0 = none, 16 = solid.
         * @param phaseX Absolute x offset added before sampling the pattern.
         * @param phaseY Absolute y offset added before sampling the pattern.
         */
        void drawHLineDither(int16_t x, int16_t y, int16_t width, Pixel4 color,
                             uint8_t level, int16_t phaseX = 0, int16_t phaseY = 0)
        {
            if (y < 0 || y >= HEIGHT || x >= WIDTH || width <= 0)
                return;

            // Clip in int so a far-off-left start can't wrap int16.
            int x0 = x;
            int x1 = static_cast<int>(x) + width; // exclusive
            if (x0 < 0)
                x0 = 0;
            if (x1 > WIDTH)
                x1 = WIDTH;
            if (x0 >= x1)
                return;

            // The pattern row for this y. `& 3` is the positive modulo for
            // negatives too; int16 wrap in Mask::sample is a multiple of 4,
            // so it agrees with this.
            const uint32_t rowBits =
                Mask::bayer(level).rows[(static_cast<int>(y) + phaseY) & 3];
            if (rowBits == 0)
                return;

            // pat bit k = mask value for pixels with (ax & 3) == k.
            uint8_t pat = 0;
            for (int k = 0; k < 4; ++k)
                pat |= static_cast<uint8_t>(((rowBits >> ((k + phaseX) & 3)) & 1u) << k);

            // Nibble-select per byte: a byte holds pixels 2b (low) and 2b+1
            // (high), so even bytes take pat bits 0/1 and odd bytes bits 2/3.
            const uint8_t sel[2] = {
                static_cast<uint8_t>(((pat & 1u) ? 0x0Fu : 0u) | ((pat & 2u) ? 0xF0u : 0u)),
                static_cast<uint8_t>(((pat & 4u) ? 0x0Fu : 0u) | ((pat & 8u) ? 0xF0u : 0u)),
            };

            invalidate(static_cast<int16_t>(x0), y, static_cast<int16_t>(x1 - x0), 1);

            const uint8_t packed = static_cast<uint8_t>((color.value << 4) | color.value);
            PackedPixel4 *row = &buffer[getIndex(0, y)];
            const int bFirst = x0 >> 1;
            const int bLast = (x1 - 1) >> 1;
            for (int b = bFirst; b <= bLast; ++b)
            {
                uint8_t m = sel[b & 1];
                if (b == bFirst && (x0 & 1))
                    m &= 0xF0u; // span starts on the high nibble
                if (b == bLast && !((x1 - 1) & 1))
                    m &= 0x0Fu; // span ends on the low nibble
                const uint8_t v = row[b].getByte();
                row[b] = PackedPixel4(static_cast<uint8_t>((v & ~m) | (packed & m)));
            }
        }

        /**
         * @brief Optimized vertical line drawing
         * @param x X coordinate
         * @param y Starting y coordinate
         * @param height Line height in pixels
         * @param color Line color
         */
        void drawVLine(int16_t x, int16_t y, int16_t height, Pixel4 color)
        {
            if (x < 0 || x >= WIDTH || y >= HEIGHT || height <= 0)
                return;

            // Clip to canvas bounds
            if (y < 0)
            {
                height += y;
                y = 0;
            }
            if (y + height > HEIGHT)
            {
                height = HEIGHT - y;
            }

            // Vertical lines can't use memset optimization due to stride
            for (int16_t i = 0; i < height; ++i)
            {
                setPixel(x, y + i, color);
            }
        }

        /**
         * @brief Optimized rectangle filling with batch operations
         * @param x Starting x coordinate
         * @param y Starting y coordinate
         * @param width Rectangle width
         * @param height Rectangle height
         * @param color Fill color
         */
        void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, Pixel4 color)
        {
            // Use optimized horizontal lines for each row
            for (int16_t row = 0; row < height; ++row)
            {
                drawHLine(x, y + row, width, color);
            }
        }

        /**
         * @brief Set a row's run of pixels from an array: pixels[0..count-1]
         *        to x..x+count-1 on row y, clipped to the canvas.
         *
         * Paints exactly what setPixel would for each, at any start and width
         * parity: an odd start owns the high nibble of its byte, the pairs
         * between are one byte each, an odd remainder owns the low nibble of
         * the last byte. The span's tiles are marked dirty once.
         * @param x Starting x coordinate
         * @param y Y coordinate
         * @param pixels Array of pixel values
         * @param count Number of pixels to set
         */
        void setPixelBatch(int16_t x, int16_t y, const Pixel4 *pixels, int16_t count)
        {
            if (y < 0 || y >= HEIGHT || x >= WIDTH || count <= 0)
                return;

            // Clip to canvas bounds, in int so a far-off-left start can't wrap.
            int x0 = x, n = count;
            if (x0 < 0)
            {
                pixels += -x0;
                n += x0;
                x0 = 0;
            }
            if (x0 + n > WIDTH)
                n = WIDTH - x0;
            if (n <= 0)
                return;

            // Mark the span dirty (the packed writes below bypass setPixel).
            invalidate(static_cast<int16_t>(x0), y, static_cast<int16_t>(n), 1);

            PackedPixel4 *p = &buffer[getIndex(static_cast<int16_t>(x0), y)];
            int i = 0;
            if (x0 & 1)
                (p++)->setHigh(pixels[i++]);
            for (; i + 1 < n; i += 2)
                *p++ = PackedPixel4(static_cast<uint8_t>((pixels[i + 1].value << 4) | pixels[i].value));
            if (i < n)
                p->setLow(pixels[i]);
        }

        void fill(const Rect &rect, Pixel4 color) override
        {
            int16_t x1 = std::max<int16_t>(0, rect.x);
            int16_t y1 = std::max<int16_t>(0, rect.y);
            int16_t x2 = std::min<int16_t>(WIDTH, rect.x + rect.width);
            int16_t y2 = std::min<int16_t>(HEIGHT, rect.y + rect.height);

            for (int16_t y = y1; y < y2; ++y)
            {
                for (int16_t x = x1; x < x2; ++x)
                {
                    setPixel(x, y, color);
                }
            }
        }

        /**
         * @brief Apply an index shader over a rectangle.
         *
         * Each pixel in `rect` (clipped to the canvas) is read, run through the
         * @ref Effect (mask x remap x phase) sampled at its **absolute** canvas
         * coordinates, and written back. Because the mask is keyed on absolute
         * coords, a shaded rect that spans several dirty tiles carries one
         * continuous pattern phase — no seam appears at a tile boundary. Writes
         * go through @ref setPixel, so the touched tiles mark themselves dirty.
         *
         * This is one of the two shader apply sites; @ref SpriteSheet::draw is
         * the other, and both funnel through @ref Effect::shadePixel.
         *
         * @param rect Region to shade, in canvas coordinates.
         * @param fx   The effect to apply.
         */
        void shade(const Rect &rect, const Effect &fx)
        {
            int16_t x1 = std::max<int16_t>(0, rect.x);
            int16_t y1 = std::max<int16_t>(0, rect.y);
            int16_t x2 = std::min<int16_t>(WIDTH, rect.x + rect.width);
            int16_t y2 = std::min<int16_t>(HEIGHT, rect.y + rect.height);

            for (int16_t y = y1; y < y2; ++y)
            {
                for (int16_t x = x1; x < x2; ++x)
                {
                    const uint8_t src = getPixel(x, y).value;
                    setPixel(x, y, Pixel4(fx.shadePixel(src, x, y)));
                }
            }
        }

        /// @brief Get read-only pointer to pixel buffer
        /// @return Pointer to packed pixel data
        const PackedPixel4 *getBuffer() const { return buffer; }
        /// @brief Get mutable pointer to pixel buffer
        /// @return Pointer to packed pixel data
        PackedPixel4 *getBuffer() { return buffer; }
        /// @brief Get buffer size in bytes
        /// @return Buffer size
        size_t getBufferSize() const { return BUFFER_SIZE; }

        /**
         * @brief Copy pixels from another canvas
         * @param other Source canvas to copy from
         * @param dst_x Destination X offset
         * @param dst_y Destination Y offset
         */
        void copyFrom(const Canvas4 &other, int16_t dst_x = 0, int16_t dst_y = 0)
        {
            for (int16_t y = 0; y < HEIGHT && y < other.getHeight(); ++y)
            {
                for (int16_t x = 0; x < WIDTH && x < other.getWidth(); ++x)
                {
                    setPixel(dst_x + x, dst_y + y, other.getPixel(x, y));
                }
            }
        }

        /**
         * @brief Blit sprite with transparency
         * @param sprite Source canvas to blit
         * @param x Destination X coordinate
         * @param y Destination Y coordinate
         * @param transparent Color to treat as transparent
         */
        void blit(const Canvas4 &sprite, int16_t x, int16_t y, Pixel4 transparent = Pixel4(0))
        {
            for (int16_t sy = 0; sy < sprite.getHeight(); ++sy)
            {
                for (int16_t sx = 0; sx < sprite.getWidth(); ++sx)
                {
                    Pixel4 pixel = sprite.getPixel(sx, sy);
                    if (pixel.value != transparent.value)
                    {
                        setPixel(x + sx, y + sy, pixel);
                    }
                }
            }
        }
    };

    /**
     * @brief 8-bit canvas with per-pixel byte storage
     * @tparam WIDTH Canvas width in pixels
     * @tparam HEIGHT Canvas height in pixels
     *
     * @note Bench-only. The visual-parity bench (tests/visual_parity_bench.cpp)
     * is this class's sole consumer: it serves as the bench's BASE reference,
     * carrying the pre-migration drawing algorithms (midpoint-octant circle
     * fill/draw, Adafruit ink-box getTextBounds/charBounds) that HEAD callers
     * migrated away from. Do not clean up, modernise, or delete these members —
     * changing them silently rebaselines the bench. See the locked
     * Visual-Parity-Bench-Design document (unwn repo), section 1.
     */
    template <uint16_t WIDTH, uint16_t HEIGHT>
    class Canvas8 : public ICanvas<uint8_t>
    {
    public:
        /// @brief Pixel type used by this canvas
        using PixelType = uint8_t;

    private:
        uint8_t buffer[WIDTH * HEIGHT];


    public:
        Canvas8()
        {
            clear();
        }

        uint16_t getWidth() const override { return WIDTH; }
        uint16_t getHeight() const override { return HEIGHT; }

        void setPixel(int16_t x, int16_t y, uint8_t color) override
        {
            if (!inBounds(x, y))
                return;
            buffer[y * WIDTH + x] = color;
        }

        uint8_t getPixel(int16_t x, int16_t y) const override
        {
            if (!inBounds(x, y))
                return 0;
            return buffer[y * WIDTH + x];
        }

        void clear(uint8_t color = 0) override
        {
            std::fill(buffer, buffer + WIDTH * HEIGHT, color);
        }

        void fill(const Rect &rect, uint8_t color) override
        {
            int16_t x1 = std::max<int16_t>(0, rect.x);
            int16_t y1 = std::max<int16_t>(0, rect.y);
            int16_t x2 = std::min<int16_t>(WIDTH, rect.x + rect.width);
            int16_t y2 = std::min<int16_t>(HEIGHT, rect.y + rect.height);

            for (int16_t y = y1; y < y2; ++y)
            {
                for (int16_t x = x1; x < x2; ++x)
                {
                    setPixel(x, y, color);
                }
            }
        }

        /**
         * @brief Convert canvas contents to 4-bit format
         * @param dst Destination 4-bit canvas
         */
        void convertTo4bit(Canvas4<WIDTH, HEIGHT> &dst) const
        {
            for (int16_t y = 0; y < HEIGHT; ++y)
            {
                for (int16_t x = 0; x < WIDTH; ++x)
                {
                    dst.setPixel(x, y, Pixel4::from8bit(getPixel(x, y)));
                }
            }
        }

        /// @brief Get read-only pointer to pixel buffer
        /// @return Pointer to pixel data
        const uint8_t *getBuffer() const { return buffer; }
        /// @brief Get mutable pointer to pixel buffer
        /// @return Pointer to pixel data
        uint8_t *getBuffer() { return buffer; }

        // === Adafruit_GFX Compatibility Methods ===

        /**
         * @brief Fill entire canvas (Adafruit_GFX compatibility)
         * @param color Fill color
         */
        void fillScreen(uint8_t color)
        {
            clear(color);
        }

        /**
         * @brief Draw single pixel (Adafruit_GFX compatibility)
         * @param x X coordinate
         * @param y Y coordinate
         * @param color Pixel color
         */
        void drawPixel(int16_t x, int16_t y, uint8_t color)
        {
            setPixel(x, y, color);
        }

        /**
         * @brief Get canvas width (Adafruit_GFX compatibility)
         * @return Width in pixels
         */
        uint16_t width() const { return WIDTH; }

        /**
         * @brief Get canvas height (Adafruit_GFX compatibility)
         * @return Height in pixels
         */
        uint16_t height() const { return HEIGHT; }


        /**
         * @brief Fill rectangle
         * @param x Top-left X coordinate
         * @param y Top-left Y coordinate
         * @param w Width in pixels
         * @param h Height in pixels
         * @param color Fill color
         */
        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
        {
            for (int16_t py = y; py < y + h && py < HEIGHT; py++)
            {
                for (int16_t px = x; px < x + w && px < WIDTH; px++)
                {
                    if (px >= 0 && py >= 0)
                    {
                        setPixel(px, py, color);
                    }
                }
            }
        }

        /**
         * @brief Draw rectangle outline
         * @param x Top-left X coordinate
         * @param y Top-left Y coordinate
         * @param w Width in pixels
         * @param h Height in pixels
         * @param color Outline color
         */
        void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
        {
            drawLine(x, y, x + w - 1, y, color);                 // Top
            drawLine(x, y + h - 1, x + w - 1, y + h - 1, color); // Bottom
            drawLine(x, y, x, y + h - 1, color);                 // Left
            drawLine(x + w - 1, y, x + w - 1, y + h - 1, color); // Right
        }

        /**
         * @brief Draw a line using Bresenham's algorithm
         * @param x0 Start X coordinate
         * @param y0 Start Y coordinate
         * @param x1 End X coordinate
         * @param y1 End Y coordinate
         * @param color Line color
         */
        void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
        {
            int16_t dx = std::abs(x1 - x0);
            int16_t dy = std::abs(y1 - y0);
            int16_t sx = (x0 < x1) ? 1 : -1;
            int16_t sy = (y0 < y1) ? 1 : -1;
            int16_t err = dx - dy;

            while (true)
            {
                setPixel(x0, y0, color);

                if (x0 == x1 && y0 == y1)
                    break;

                int16_t e2 = 2 * err;
                if (e2 > -dy)
                {
                    err -= dy;
                    x0 += sx;
                }
                if (e2 < dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

        /**
         * @brief Draw a filled circle using midpoint circle algorithm
         * @param x0 Center X coordinate
         * @param y0 Center Y coordinate
         * @param radius Circle radius
         * @param color Fill color
         */
        void fillCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)
        {
            int16_t x = radius;
            int16_t y = 0;
            int16_t radiusError = 1 - x;

            while (x >= y)
            {
                // Draw horizontal lines for each octant
                for (int16_t i = x0 - x; i <= x0 + x; i++)
                {
                    setPixel(i, y0 + y, color);
                    setPixel(i, y0 - y, color);
                }
                for (int16_t i = x0 - y; i <= x0 + y; i++)
                {
                    setPixel(i, y0 + x, color);
                    setPixel(i, y0 - x, color);
                }

                y++;
                if (radiusError < 0)
                {
                    radiusError += 2 * y + 1;
                }
                else
                {
                    x--;
                    radiusError += 2 * (y - x + 1);
                }
            }
        }

        /**
         * @brief Draw circle outline using midpoint circle algorithm
         * @param x0 Center X coordinate
         * @param y0 Center Y coordinate
         * @param radius Circle radius
         * @param color Outline color
         */
        void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)
        {
            int16_t x = radius;
            int16_t y = 0;
            int16_t radiusError = 1 - x;

            while (x >= y)
            {
                setPixel(x0 + x, y0 + y, color);
                setPixel(x0 + y, y0 + x, color);
                setPixel(x0 - y, y0 + x, color);
                setPixel(x0 - x, y0 + y, color);
                setPixel(x0 - x, y0 - y, color);
                setPixel(x0 - y, y0 - x, color);
                setPixel(x0 + y, y0 - x, color);
                setPixel(x0 + x, y0 - y, color);

                y++;
                if (radiusError < 0)
                {
                    radiusError += 2 * y + 1;
                }
                else
                {
                    x--;
                    radiusError += 2 * (y - x + 1);
                }
            }
        }

        /**
         * @brief Draw rounded rectangle outline
         * @param x Top-left X coordinate
         * @param y Top-left Y coordinate
         * @param w Width in pixels
         * @param h Height in pixels
         * @param radius Corner radius
         * @param color Outline color
         */
        void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)
        {
            // Draw straight edges
            drawLine(x + radius, y, x + w - radius - 1, y, color);                 // Top
            drawLine(x + radius, y + h - 1, x + w - radius - 1, y + h - 1, color); // Bottom
            drawLine(x, y + radius, x, y + h - radius - 1, color);                 // Left
            drawLine(x + w - 1, y + radius, x + w - 1, y + h - radius - 1, color); // Right

            // Draw corner arcs (simplified to quarter circles)
            for (int16_t i = 0; i <= radius; i++)
            {
                for (int16_t j = 0; j <= radius; j++)
                {
                    if (i * i + j * j <= radius * radius && i * i + j * j >= (radius - 1) * (radius - 1))
                    {
                        setPixel(x + radius - i, y + radius - j, color);                 // Top-left
                        setPixel(x + w - radius - 1 + i, y + radius - j, color);         // Top-right
                        setPixel(x + radius - i, y + h - radius - 1 + j, color);         // Bottom-left
                        setPixel(x + w - radius - 1 + i, y + h - radius - 1 + j, color); // Bottom-right
                    }
                }
            }
        }

        /**
         * @brief Fill rounded rectangle
         * @param x Top-left X coordinate
         * @param y Top-left Y coordinate
         * @param w Width in pixels
         * @param h Height in pixels
         * @param radius Corner radius
         * @param color Fill color
         */
        void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)
        {
            // Fill main rectangle areas
            fillRect(x + radius, y, w - 2 * radius, h, color);                   // Center
            fillRect(x, y + radius, radius, h - 2 * radius, color);              // Left edge
            fillRect(x + w - radius, y + radius, radius, h - 2 * radius, color); // Right edge

            // Fill corner arcs (simplified to quarter circles)
            for (int16_t i = 0; i <= radius; i++)
            {
                for (int16_t j = 0; j <= radius; j++)
                {
                    if (i * i + j * j <= radius * radius)
                    {
                        setPixel(x + radius - i, y + radius - j, color);                 // Top-left
                        setPixel(x + w - radius - 1 + i, y + radius - j, color);         // Top-right
                        setPixel(x + radius - i, y + h - radius - 1 + j, color);         // Bottom-left
                        setPixel(x + w - radius - 1 + i, y + h - radius - 1 + j, color); // Bottom-right
                    }
                }
            }
        }

        /**
         * @brief Fill rectangle with repeating pattern
         * @param x Top-left X coordinate
         * @param y Top-left Y coordinate
         * @param w Width in pixels
         * @param h Height in pixels
         * @param pattern Pointer to pattern data
         * @param patternWidth Pattern width in pixels
         * @param patternHeight Pattern height in pixels
         */
        void fillRectWithPattern(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *pattern, int16_t patternWidth, int16_t patternHeight)
        {
            if (!pattern)
                return;

            for (int16_t i = 0; i < w; i++)
            {
                for (int16_t j = 0; j < h; j++)
                {
                    // Calculate the corresponding pixel in the pattern
                    int16_t patternX = i % patternWidth;
                    int16_t patternY = j % patternHeight;

                    // Get the color from the pattern
                    uint8_t color = pattern[patternY * patternWidth + patternX];

                    // Draw the pixel
                    setPixel(x + i, y + j, color);
                }
            }
        }

        // === GFXcanvas8 Compatibility Methods ===

        /**
         * @brief Draw grayscale bitmap with basic parameters
         * @param x Destination X coordinate
         * @param y Destination Y coordinate
         * @param bitmap Pointer to bitmap data
         * @param w Bitmap width
         * @param h Bitmap height
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)
        {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++)
            {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++)
                {
                    uint8_t pixel = bitmap[py * w + px];
                    // Draw ALL pixels when no matte specified (no transparency)
                    setPixel(x + px, y + py, pixel);
                }
            }
        }

        /**
         * @brief Draw grayscale bitmap with matte threshold
         * @param x Destination X coordinate
         * @param y Destination Y coordinate
         * @param bitmap Pointer to bitmap data
         * @param matte Transparent color value to skip
         * @param w Bitmap width
         * @param h Bitmap height
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h)
        {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++)
            {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++)
                {
                    uint8_t pixel = bitmap[py * w + px];
                    if (pixel != matte)
                    { // Skip pixels that match the matte color
                        setPixel(x + px, y + py, pixel);
                    }
                }
            }
        }

        /**
         * @brief Draw grayscale bitmap with mask
         * @param x Destination X coordinate
         * @param y Destination Y coordinate
         * @param bitmap Pointer to bitmap data
         * @param mask Pointer to mask data (non-zero values allow drawing)
         * @param w Bitmap width
         * @param h Bitmap height
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t w, uint8_t h)
        {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++)
            {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++)
                {
                    uint8_t pixel = bitmap[py * w + px];
                    uint8_t maskValue = mask[py * w + px];
                    if (maskValue > 0)
                    { // Only draw where mask allows
                        setPixel(x + px, y + py, pixel);
                    }
                }
            }
        }

        /**
         * @brief Draw grayscale bitmap with opacity
         * @param x Destination X coordinate
         * @param y Destination Y coordinate
         * @param bitmap Pointer to bitmap data
         * @param matte Transparent color value to skip
         * @param w Bitmap width
         * @param h Bitmap height
         * @param opacity_divisor Opacity divisor (higher = more transparent)
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h, uint8_t opacity_divisor)
        {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++)
            {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++)
                {
                    uint8_t pixel = bitmap[py * w + px];
                    if (pixel != matte)
                    {                                                    // Skip pixels that match the matte color
                        uint8_t opacity_pixel = pixel / opacity_divisor; // Apply opacity
                        setPixel(x + px, y + py, opacity_pixel);
                    }
                }
            }
        }

        /**
         * @brief Add blending operation with another canvas
         * @param over Canvas to blend over this one
         */
        void add(Canvas8 *over)
        {
            if (!over)
                return;
            for (uint16_t y = 0; y < HEIGHT && y < over->getHeight(); y++)
            {
                for (uint16_t x = 0; x < WIDTH && x < over->getWidth(); x++)
                {
                    uint8_t base = getPixel(x, y);
                    uint8_t overlay = over->getPixel(x, y);
                    uint8_t result = std::min(255, static_cast<int>(base) + static_cast<int>(overlay));
                    setPixel(x, y, result);
                }
            }
        }

        /**
         * @brief Add blending operation with texture data
         * @param texture Pointer to texture data
         */
        void add(const uint8_t *texture)
        {
            if (!texture)
                return;
            for (uint16_t y = 0; y < HEIGHT; y++)
            {
                for (uint16_t x = 0; x < WIDTH; x++)
                {
                    uint8_t base = getPixel(static_cast<int16_t>(x), static_cast<int16_t>(y));
                    uint8_t overlay = texture[y * WIDTH + x];
                    uint8_t result = std::min(255, static_cast<int>(base) + static_cast<int>(overlay));
                    setPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), result);
                }
            }
        }

        /**
         * @brief Subtract blending operation with another canvas
         * @param over Canvas to subtract from this one
         */
        void subtract(Canvas8 *over)
        {
            if (!over)
                return;
            for (uint16_t y = 0; y < HEIGHT && y < over->getHeight(); y++)
            {
                for (uint16_t x = 0; x < WIDTH && x < over->getWidth(); x++)
                {
                    uint8_t base = getPixel(x, y);
                    uint8_t overlay = over->getPixel(x, y);
                    uint8_t result = std::max(0, static_cast<int>(base) - static_cast<int>(overlay));
                    setPixel(x, y, result);
                }
            }
        }

        /**
         * @brief Subtract blending operation with texture data
         * @param texture Pointer to texture data
         */
        void subtract(const uint8_t *texture)
        {
            if (!texture)
                return;
            for (uint16_t y = 0; y < HEIGHT; y++)
            {
                for (uint16_t x = 0; x < WIDTH; x++)
                {
                    uint8_t base = getPixel(static_cast<int16_t>(x), static_cast<int16_t>(y));
                    uint8_t overlay = texture[y * WIDTH + x];
                    uint8_t result = std::max(0, static_cast<int>(base) - static_cast<int>(overlay));
                    setPixel(static_cast<int16_t>(x), static_cast<int16_t>(y), result);
                }
            }
        }

        /**
         * @brief Difference blending operation with texture data
         * @param x Destination X coordinate
         * @param y Destination Y coordinate
         * @param texture Pointer to texture data
         * @param w Texture width
         * @param h Texture height
         */
        void difference(int16_t x, int16_t y, const uint8_t *texture, uint8_t w, uint8_t h)
        {
            if (!texture)
                return;
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++)
            {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++)
                {
                    uint8_t base = getPixel(x + px, y + py);
                    uint8_t overlay = texture[py * w + px];
                    uint8_t result = static_cast<uint8_t>(std::abs(static_cast<int>(base) - static_cast<int>(overlay)));
                    setPixel(x + px, y + py, result);
                }
            }
        }

        /**
         * @brief Export canvas to PGM format with proper color scaling
         * @param filename Output filename
         */
        void exportToPGM(const char *filename) const
        {
            FILE *file = fopen(filename, "w");
            if (!file)
                return;

            // PGM header
            fprintf(file, "P2\n");
            fprintf(file, "%d %d\n", WIDTH, HEIGHT);
            fprintf(file, "255\n"); // Max gray value

            // Export pixels with proper scaling
            for (int16_t y = 0; y < HEIGHT; y++)
            {
                for (int16_t x = 0; x < WIDTH; x++)
                {
                    uint8_t pixel = getPixel(x, y);
                    // Scale 4-bit (0-15) to 8-bit (0-255)
                    uint8_t scaled = (pixel * 255) / 15;
                    fprintf(file, "%d ", scaled);
                }
                fprintf(file, "\n");
            }

            fclose(file);
        }

        /**
         * @brief Export canvas to BMP format (24-bit RGB, grayscale as gray=R=G=B)
         * @param filename Output filename
         */
        void exportToBMP(const char *filename) const;


        /**
         * @brief Fill triangle (basic implementation)
         * @param x0 First vertex X
         * @param y0 First vertex Y
         * @param x1 Second vertex X
         * @param y1 Second vertex Y
         * @param x2 Third vertex X
         * @param y2 Third vertex Y
         * @param color Fill color
         */
        void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color)
        {
            // Sort vertices by Y coordinate (y0 <= y1 <= y2)
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
            }
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            if (y0 > y1)
            {
                std::swap(x0, x1);
                std::swap(y0, y1);
            }

            // Fill triangle using horizontal lines
            for (int16_t y = y0; y <= y2; y++)
            {
                int16_t xa, xb;

                if (y <= y1)
                {
                    // Upper part of triangle
                    if (y1 - y0 != 0)
                    {
                        xa = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
                    }
                    else
                    {
                        xa = x0;
                    }
                }
                else
                {
                    // Lower part of triangle
                    if (y2 - y1 != 0)
                    {
                        xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
                    }
                    else
                    {
                        xa = x1;
                    }
                }

                if (y2 - y0 != 0)
                {
                    xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
                }
                else
                {
                    xb = x0;
                }

                if (xa > xb)
                    std::swap(xa, xb);

                for (int16_t x = xa; x <= xb; x++)
                {
                    setPixel(x, y, color);
                }
            }
        }
    };

    /// @brief 4-bit 128x64 canvas type alias
    using Canvas4_128x64 = Canvas4<128, 64>;
    /// @brief 4-bit 128x128 canvas type alias
    using Canvas4_128x128 = Canvas4<128, 128>;
    /// @brief 4-bit 320x240 canvas type alias
    using Canvas4_320x240 = Canvas4<320, 240>;
    /// @brief 4-bit 480x480 canvas type alias (Tomodachi square AMOLED)
    using Canvas4_480x480 = Canvas4<480, 480>;
    /// @brief 8-bit 128x64 canvas type alias
    using Canvas8_128x64 = Canvas8<128, 64>;
    /// @brief 8-bit 128x128 canvas type alias
    using Canvas8_128x128 = Canvas8<128, 128>;
    /// @brief 8-bit 320x240 canvas type alias
    using Canvas8_320x240 = Canvas8<320, 240>;
    /// @brief 8-bit 480x480 canvas type alias (Tomodachi square AMOLED)
    using Canvas8_480x480 = Canvas8<480, 480>;

} // namespace enjin2