#pragma once

/**
 * @file role_dither.hpp
 * @brief Ordered dither of RGB565 into the 8 Palette roles (ADR-0026, Tomodachi #459).
 *
 * A picture (the music player's Covers) is drawn in the palette by dithering
 * it into the roles: each pixel gets a 4×4 Bayer offset added to its three
 * channels, then the role whose colour is nearest in Oklab. The output holds
 * roles 0-7 only, never 8-15, so a palette change recolours it like any art.
 *
 * The nearest pick reads a per-palette table over the RGB cube (GRID cells a
 * side, each holding the role nearest its centre in Oklab), so a pixel costs
 * a few adds and a load. The table is built once per palette: call `build`
 * again when the palette changes. Everything is integers (oklab.hpp's
 * srgbToOklabQ), so a host and the device give the same bytes.
 *
 * The Bayer phase is the image's own (its pixel (0, 0) takes the matrix's
 * first entry), so a dithered image looks the same wherever it is drawn.
 */

#include <cstddef>
#include <cstdint>

#include "canvas.hpp"
#include "oklab.hpp"
#include "palette.hpp"

namespace enjin2 {

class RoleDither {
public:
    /// The Bayer offsets span this many 8-bit levels, the same on each
    /// channel (the spread the owner picked in the ADR-0026 mock-ups).
    static constexpr uint8_t DEFAULT_SPREAD = 56;
    /// Table cells per RGB axis.
    static constexpr int GRID_BITS = 4;
    static constexpr int GRID = 1 << GRID_BITS;

    /// An unbuilt table answers `bg` everywhere; call build first.
    RoleDither() = default;

    /// (Re)build for @p pal's 8 roles. @p spread 0 is no dither: the plain
    /// nearest role.
    void build(const Palette& pal, uint8_t spread = DEFAULT_SPREAD);

    /// The role nearest @p r, @p g, @p b in Oklab, worked out exactly (the
    /// table holds this at each cell centre). Ties go to the lower role.
    uint8_t nearestRole(uint8_t r, uint8_t g, uint8_t b) const;

    /// Dither a @p w × @p h RGB565 image (native byte order, red in the high
    /// bits as packRgb565 makes it; @p srcStride pixels per row) into packed
    /// 4-bit roles at @p dst, @p dstRowBytes per row: two pixels a byte, the
    /// even x in the low nibble, as Canvas4 and a packed LayeredAsset frame
    /// store them. An odd width's last high nibble is written 0.
    void dither(const uint16_t* src, uint16_t w, uint16_t h, size_t srcStride, uint8_t* dst,
                size_t dstRowBytes) const;

    /// Dither into a Canvas4's top-left corner, clipped to it, and mark the
    /// touched tiles dirty.
    template <uint16_t W, uint16_t H>
    void dither(const uint16_t* src, uint16_t w, uint16_t h, Canvas4<W, H>& dst) const {
        const uint16_t cw = w < W ? w : W;
        const uint16_t ch = h < H ? h : H;
        if (cw == 0 || ch == 0) return;
        uint8_t* out = reinterpret_cast<uint8_t*>(dst.getBuffer());
        constexpr size_t rowBytes = (W + 1) / 2;
        if ((cw & 1) == 0 || cw == W) {
            dither(src, cw, ch, w, out, rowBytes);
        } else {
            // An odd width inside a wider canvas: the last pixel shares its
            // byte with a canvas pixel that must stay.
            dither(src, uint16_t(cw - 1), ch, w, out, rowBytes);
            for (uint16_t y = 0; y < ch; ++y) {
                const uint16_t p = src[size_t(y) * w + cw - 1];
                dst.setPixel(int16_t(cw - 1), int16_t(y), Pixel4(pick(p, cw - 1, y)));
            }
        }
        dst.invalidate(0, 0, int16_t(cw), int16_t(ch));
    }

private:
    uint8_t pick(uint16_t rgb565, int x, int y) const;

    OklabQ roles_[ROLE_COUNT] = {};
    int16_t offset_[16] = {};
    uint8_t table_[GRID * GRID * GRID] = {};
};

} // namespace enjin2
