#pragma once

/**
 * @file bayer.hpp
 * @brief The 4×4 Bayer matrix every ordered dither in enjin reads (the role
 *        dither, Effect's dissolve mask) and the music player's Generated
 *        covers.
 */

#include <cstdint>

namespace enjin2 {

/// The standard 4×4 Bayer threshold matrix, values 0..15, row-major: entry
/// `(y & 3) * 4 + (x & 3)`.
constexpr uint8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

/// The matrix's entry for pixel (x, y): it repeats every 4 pixels each way.
constexpr uint8_t bayer4(int x, int y) { return BAYER4[((y & 3) << 2) | (x & 3)]; }

} // namespace enjin2
