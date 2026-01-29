#pragma once

#include "../core/types.hpp"
#include "canvas.hpp"

namespace enjin2 {

// Graphics effects for 4-bit canvas
template<typename TPixel>
class Effects {
public:
    // Dithering patterns for 4-bit displays
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
    
    // Simple blur effect (box filter)
    static void blur(ICanvas<TPixel>& canvas, const Rect& rect, uint8_t radius = 1) {
        // This would require a temporary buffer for proper implementation
        // Placeholder for future implementation
    }
    
    // Invert colors in a region
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