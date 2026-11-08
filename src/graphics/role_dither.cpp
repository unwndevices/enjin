// Ordered dither of RGB565 into the Palette roles (ADR-0026, Tomodachi #459).
// See role_dither.hpp.

#include "../../include/enjin2/graphics/role_dither.hpp"

namespace enjin2 {

namespace {

// The 4×4 Bayer matrix, row-major: entry (y & 3) * 4 + (x & 3).
constexpr uint8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

constexpr int CELL_SHIFT = 8 - RoleDither::GRID_BITS;

inline int clampByte(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

} // namespace

void RoleDither::build(const Palette& pal, uint8_t spread) {
    for (uint8_t i = 0; i < ROLE_COUNT; ++i) {
        const RGB c = pal.getColor(i);
        roles_[i] = srgbToOklabQ(c.r, c.g, c.b);
    }
    // Centred on 0: (2k + 1 - 16) / 32 of the spread, k the Bayer entry.
    for (int i = 0; i < 16; ++i)
        offset_[i] = int16_t((2 * BAYER4[i] + 1 - 16) * int(spread) / 32);
    for (int r = 0; r < GRID; ++r)
        for (int g = 0; g < GRID; ++g)
            for (int b = 0; b < GRID; ++b) {
                const int half = 1 << (CELL_SHIFT - 1);
                table_[(r * GRID + g) * GRID + b] =
                    nearestRole(uint8_t((r << CELL_SHIFT) + half), uint8_t((g << CELL_SHIFT) + half),
                                uint8_t((b << CELL_SHIFT) + half));
            }
}

uint8_t RoleDither::nearestRole(uint8_t r, uint8_t g, uint8_t b) const {
    const OklabQ p = srgbToOklabQ(r, g, b);
    uint8_t best = 0;
    int64_t bestD = INT64_MAX;
    for (uint8_t i = 0; i < ROLE_COUNT; ++i) {
        const int64_t dL = p.L - roles_[i].L, da = p.a - roles_[i].a, db = p.b - roles_[i].b;
        const int64_t d = dL * dL + da * da + db * db;
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

uint8_t RoleDither::pick(uint16_t rgb565, int x, int y) const {
    const int r5 = rgb565 >> 11, g6 = (rgb565 >> 5) & 63, b5 = rgb565 & 31;
    const int d = offset_[((y & 3) << 2) | (x & 3)];
    const int r = clampByte(((r5 << 3) | (r5 >> 2)) + d) >> CELL_SHIFT;
    const int g = clampByte(((g6 << 2) | (g6 >> 4)) + d) >> CELL_SHIFT;
    const int b = clampByte(((b5 << 3) | (b5 >> 2)) + d) >> CELL_SHIFT;
    return table_[(r * GRID + g) * GRID + b];
}

void RoleDither::dither(const uint16_t* src, uint16_t w, uint16_t h, size_t srcStride,
                        uint8_t* dst, size_t dstRowBytes) const {
    if (!src || !dst || w == 0 || h == 0) return;
    for (int y = 0; y < h; ++y) {
        const uint16_t* in = src + size_t(y) * srcStride;
        uint8_t* out = dst + size_t(y) * dstRowBytes;
        int x = 0;
        for (; x + 1 < w; x += 2)
            *out++ = uint8_t(pick(in[x], x, y) | (pick(in[x + 1], x + 1, y) << 4));
        if (x < w) *out = pick(in[x], x, y);
    }
}

} // namespace enjin2
