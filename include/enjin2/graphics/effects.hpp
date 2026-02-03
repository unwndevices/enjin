#pragma once

#include "../core/types.hpp"
#include "canvas.hpp"
namespace enjin2
{

/**
 * @file effects.hpp
 * @brief Graphics effects and image processing
 *
 * Provides dithering, blur, and color manipulation effects
 * for enhanced visual output.
 */

/**
 * @brief Graphics effects for pixel manipulation
 * @tparam TPixel Pixel type (e.g., Pixel4, uint8_t)
 */
template<typename TPixel>
class Effects {
public:
    /**
     * @brief Dither pattern for anti-aliasing
     * @param canvas Target canvas
     * @param rect Region to dither
     * @param color1 First color in pattern
     * @param color2 Second color in pattern
     * @param pattern Dithering pattern bitmask (default: 0xAA checkerboard)
     */
    static void ditherPattern(ICanvas<TPixel>& canvas, const Rect& rect,
                             TPixel color1, TPixel color2, uint8_t pattern = 0xAA) {
        for (int16_t y = rect.y; y < rect.y + rect.height; ++y) {
            for (int16_t x = rect.x; x < rect.x + rect.width; ++x) {
                uint8_t bit_pos = ((y - rect.y) * 8 + (x - rect.x)) % 8;
                TPixel color = (pattern & (1 << bit_pos)) ? color1 : color2;
                canvas.setPixel(x, y, color);
            }
        }
    }

    /**
     * @brief Apply simple blur effect (box filter)
     * @param canvas Target canvas
     * @param rect Region to blur
     * @param radius Blur radius in pixels (default: 1)
     * @note Currently a placeholder for future implementation
     */
    static void blur(ICanvas<TPixel>& canvas, const Rect& rect, uint8_t radius = 1) {
        // This would require a temporary buffer for proper implementation
        // Placeholder for future implementation
    }

    /**
     * @brief Invert colors in a region
     * @param canvas Target canvas
     * @param rect Region to invert
     */
    static void invert(ICanvas<TPixel>& canvas, const Rect& rect) {
        for (int16_t y = rect.y; y < rect.y + rect.height; ++y) {
            for (int16_t x = rect.x; x < rect.x + rect.width; ++x) {
                if (canvas.inBounds(x, y)) {
                    TPixel current = canvas.getPixel(x, y);
                    // For 4-bit: invert by XOR with 15 (0xF)
                    if constexpr (std::is_same_v<TPixel, Pixel4>) {
                        canvas.setPixel(x, y, Pixel4(15 - current.value));
                    } else {
                        canvas.setPixel(x, y, TPixel(255 - static_cast<uint8_t>(current)));
                    }
                }
            }
        }
    }
};

using Effects4 = Effects<Pixel4>;
using Effects8 = Effects<uint8_t>;

} // namespace enjin2