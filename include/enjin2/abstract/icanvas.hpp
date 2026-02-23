/**
 * @file icanvas.hpp
 * @brief Abstract canvas interface for drawing operations
 *
 * Provides hardware-independent interface for all drawing operations.
 * Both enjin1 and enjin2 can implement this interface.
 */
#pragma once

#include "../core/types.hpp"

namespace enjin2 {

/**
 * @brief Abstract canvas interface for drawing operations
 * @tparam TPixel Pixel type (e.g., Pixel4, uint8_t)
 *
 * Provides a hardware-independent interface for all drawing operations.
 * Both enjin1 and enjin2 can implement this interface for compile-time polymorphism.
 */
template <typename TPixel>
class ICanvas {
public:
    /// @brief Pixel type used by this canvas
    using PixelType = TPixel;

    /**
     * @brief Virtual destructor for proper cleanup through base pointer
     */
    virtual ~ICanvas() = default;

    // ===== Core canvas methods =====

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
     * @param color Color to fill canvas with (default: black/zero)
     */
    virtual void clear(TPixel color = TPixel(0)) = 0;

    /**
     * @brief Fill rectangular region with specified color
     * @param rect Rectangle to fill
     * @param color Color to fill with
     */
    virtual void fill(const Rect& rect, TPixel color) = 0;

    // ===== Text rendering =====

    /**
     * @brief Draw text at specified position
     * @param text Text to draw
     * @param x X coordinate
     * @param y Y coordinate
     * @param color Text color
     */
    virtual void drawText(const char* text, int16_t x, int16_t y, TPixel color) = 0;

    /**
     * @brief Set text color for subsequent text operations
     * @param color Text color
     */
    virtual void setTextColor(TPixel color) = 0;

    /**
     * @brief Set text size for subsequent text operations
     * @param size Text scaling factor
     */
    virtual void setTextSize(uint8_t size) = 0;

    // ===== Shape drawing =====

    /**
     * @brief Draw a line from (x0, y0) to (x1, y1)
     * @param x0 Start X coordinate
     * @param y0 Start Y coordinate
     * @param x1 End X coordinate
     * @param y1 End Y coordinate
     * @param color Line color
     */
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, TPixel color) = 0;

    /**
     * @brief Draw rectangle outline
     * @param x Top-left X coordinate
     * @param y Top-left Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param color Line color
     */
    virtual void drawRect(int16_t x, int16_t y, int16_t width, int16_t height, TPixel color) = 0;

    /**
     * @brief Draw filled rectangle
     * @param x Top-left X coordinate
     * @param y Top-left Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param color Fill color
     */
    virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, TPixel color) = 0;

    /**
     * @brief Draw circle outline
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Line color
     */
    virtual void drawCircle(int16_t x, int16_t y, int16_t radius, TPixel color) = 0;

    /**
     * @brief Draw filled circle
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     */
    virtual void fillCircle(int16_t x, int16_t y, int16_t radius, TPixel color) = 0;

    // ===== Image drawing =====

    /**
     * @brief Draw bitmap image at specified position
     * @param bitmap Pointer to bitmap data
     * @param x Destination X coordinate
     * @param y Destination Y coordinate
     * @param width Image width
     * @param height Image height
     * @param color Image color
     */
    virtual void drawBitmap(const uint8_t* bitmap, int16_t x, int16_t y,
                        int16_t width, int16_t height, TPixel color) = 0;

    /**
     * @brief Draw bitmap with transparency (skip pixels matching matte color)
     * @param bitmap Pointer to bitmap data
     * @param matte Matte color (pixels matching this are skipped)
     * @param x Destination X coordinate
     * @param y Destination Y coordinate
     * @param width Image width
     * @param height Image height
     */
    virtual void drawBitmap(const uint8_t* bitmap, uint8_t matte,
                        int16_t x, int16_t y, int16_t width, int16_t height) = 0;
};

} // namespace enjin2
