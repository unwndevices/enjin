/**
 * @file blit_rgb565.hpp
 * @brief Expand a packed 4-bit indexed frame to an RGB565 buffer for a panel.
 *
 * The Canvas4 family stores two palette indices per byte (low nibble = the
 * even/first pixel, high nibble = the odd/second pixel — matching Canvas4's
 * `index = (y*W + x)`, `odd = index & 1`, even→low, odd→high). A QSPI/SPI panel
 * wants a linear RGB565 scanline buffer, so `blitIndexed4ToRgb565` walks the
 * packed data byte by byte and emits two pre-resolved RGB565 words per byte.
 *
 * The mapping index→RGB565 is passed in as a 16-entry lookup table so the hot
 * loop stays a pure, allocation-free, palette-agnostic memory transform (this is
 * the unit-tested seam). Build the table once per palette/orientation with
 * @ref makeRgb565Lut, which folds in the panel's byte order.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "palette.hpp"

namespace enjin2 {

/**
 * @brief Pack 8-bit R/G/B into a 16-bit RGB565 word (native byte order).
 * @param r Red   (0-255) — top 5 bits used.
 * @param g Green (0-255) — top 6 bits used.
 * @param b Blue  (0-255) — top 5 bits used.
 * @return RGB565 value with red in the high bits.
 */
constexpr uint16_t packRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/**
 * @brief Swap the two bytes of a 16-bit word.
 *
 * ESP32 is little-endian; the CO5300 (and most esp_lcd AMOLED/TFT panels) latch
 * RGB565 most-significant byte first, so the LUT is byte-swapped up front and the
 * blit loop never has to.
 */
constexpr uint16_t byteSwap16(uint16_t v) {
    return static_cast<uint16_t>((v >> 8) | (v << 8));
}

/**
 * @brief Resolve all 16 palette indices to RGB565 words for the blit LUT.
 *
 * Index 15 (transparent) has no color of its own; it resolves to `getColor(15)`
 * (black) so a full-frame blit that ignores transparency still produces a
 * defined value. Callers that honor transparency should skip index 15 before the
 * blit rather than relying on this entry.
 *
 * @param pal   Source palette.
 * @param out   16-entry destination table.
 * @param swapBytes When true (default), entries are stored MSB-first for the panel.
 */
inline void makeRgb565Lut(const Palette& pal, uint16_t out[16], bool swapBytes = true) {
    for (uint8_t i = 0; i < 16; ++i) {
        const RGB c = pal.getColor(i);
        uint16_t v = packRgb565(c.r, c.g, c.b);
        out[i] = swapBytes ? byteSwap16(v) : v;
    }
}

/**
 * @brief Expand a packed 4-bit indexed buffer to RGB565 via a 16-entry LUT.
 *
 * Pure transform: reads `(pixelCount + 1) / 2` packed bytes and writes
 * `pixelCount` RGB565 words. No bounds checks beyond the pixel count, no
 * allocation — `out` must hold at least `pixelCount` words.
 *
 * @param data4      Packed 4-bit indices, two pixels per byte (low nibble first).
 * @param pixelCount Number of pixels to emit (usually width * height).
 * @param lut        index→RGB565 table, already in panel byte order.
 * @param out        Destination RGB565 scanline buffer (>= pixelCount words).
 */
inline void blitIndexed4ToRgb565(const uint8_t* data4, size_t pixelCount,
                                 const uint16_t lut[16], uint16_t* out) {
    const size_t pairs = pixelCount / 2;
    size_t o = 0;
    for (size_t i = 0; i < pairs; ++i) {
        const uint8_t byte = data4[i];
        out[o++] = lut[byte & 0x0F]; // even/first pixel = low nibble
        out[o++] = lut[byte >> 4];   // odd/second pixel = high nibble
    }
    if ((pixelCount & 1U) != 0) {
        out[o] = lut[data4[pairs] & 0x0F];
    }
}

} // namespace enjin2
