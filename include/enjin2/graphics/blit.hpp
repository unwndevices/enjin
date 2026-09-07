#ifndef ENJIN2_GRAPHICS_BLIT_HPP
#define ENJIN2_GRAPHICS_BLIT_HPP

// Pixel4-native sprite compositor.
//
// Free helpers implementing the handful of blend modes domain widgets need,
// straight against any ICanvas<Pixel4> framebuffer. Canvas4 is deliberately
// lean, so these live as free functions rather than member API.
//
// Two source families:
//   * raw 8-bit grayscale asset bitmaps (values 0-255)  -> *Gray8 helpers
//   * offscreen ICanvas<Pixel4> buffers                 -> *Canvas helpers
//
// Moved in from Eisei's ui/utils/Blit.hpp as a pure move (unwn #165), then
// adjudicated against the Canvas8 originals at the first sweep (unwn #168):
// every helper emulates BASE's pipeline — arithmetic at 8 bits, then the
// SSD1327's low-nibble display mask (& 0x0F). Shipped Eisei assets store
// 4-bit values in bytes, so any >>4 reduction renders them black; and the
// panel mask, not saturation, is what shipped blends actually did (8 + 8
// wraps to 0, 15 + 10 to 9). The mask is part of the design.

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "canvas.hpp"
#include "palette.hpp"

namespace enjin2
{
    // ---- 8-bit grayscale source (asset bitmaps, 0-255) -> Pixel4 canvas ----

    // Opaque copy (every source pixel written).
    inline void blitGray8(ICanvas<Pixel4> &dst, int16_t x, int16_t y,
                          const uint8_t *src, int16_t w, int16_t h)
    {
        for (int16_t py = 0; py < h; ++py)
            for (int16_t px = 0; px < w; ++px)
                dst.setPixel(x + px, y + py, Pixel4(src[py * w + px] & 0x0F));
    }

    // Source pixels equal to `matte` are treated as transparent and skipped.
    inline void blitGray8Matte(ICanvas<Pixel4> &dst, int16_t x, int16_t y,
                               const uint8_t *src, uint8_t matte,
                               int16_t w, int16_t h)
    {
        for (int16_t py = 0; py < h; ++py)
            for (int16_t px = 0; px < w; ++px)
            {
                const uint8_t g = src[py * w + px];
                if (g == matte)
                    continue;
                dst.setPixel(x + px, y + py, Pixel4(g & 0x0F));
            }
    }

    // Matte + opacity: fade the source toward black by 1/divisor (divisor 2 ~=
    // half brightness, divisor 4 ~= quarter). This is the BASE source-fade
    // semantics (`s / divisor`, absolute) — not a dest-lerp; identical on a dark
    // background but faithful over bright fills.
    inline void blitGray8Opacity(ICanvas<Pixel4> &dst, int16_t x, int16_t y,
                                 const uint8_t *src, uint8_t matte,
                                 int16_t w, int16_t h, uint8_t divisor)
    {
        if (divisor < 1)
            divisor = 1;
        for (int16_t py = 0; py < h; ++py)
            for (int16_t px = 0; px < w; ++px)
            {
                const uint8_t g = src[py * w + px];
                if (g == matte)
                    continue;
                // BASE fades at 8 bits, the panel masks the quotient.
                dst.setPixel(x + px, y + py, Pixel4(uint8_t((g / divisor) & 0x0F)));
            }
    }

    // Additive / subtractive full-texture blends (source anchored at origin).
    inline void addGray8(ICanvas<Pixel4> &dst, const uint8_t *src, int16_t w, int16_t h)
    {
        for (int16_t py = 0; py < h; ++py)
            for (int16_t px = 0; px < w; ++px)
            {
                const int s = src[py * w + px];
                const int d = dst.getPixel(px, py);
                // 8-bit accumulate (clamped at 255 like Canvas8::add), then
                // the panel mask — sums past 15 wrap, they do not saturate.
                dst.setPixel(px, py, Pixel4(uint8_t(std::min(255, d + s) & 0x0F)));
            }
    }

    inline void subtractGray8(ICanvas<Pixel4> &dst, const uint8_t *src, int16_t w, int16_t h)
    {
        for (int16_t py = 0; py < h; ++py)
            for (int16_t px = 0; px < w; ++px)
            {
                // Full 8-bit source magnitude subtracts, like Canvas8: any
                // source byte >= 16 floors a 4-bit destination to 0.
                const int s = src[py * w + px];
                const int d = dst.getPixel(px, py);
                dst.setPixel(px, py, Pixel4(uint8_t(std::max(0, d - s))));
            }
    }

    inline void differenceGray8(ICanvas<Pixel4> &dst, int16_t x, int16_t y,
                                const uint8_t *src, int16_t w, int16_t h)
    {
        for (int16_t py = 0; py < h; ++py)
            for (int16_t px = 0; px < w; ++px)
            {
                const int s = src[py * w + px];
                const int d = dst.getPixel(x + px, y + py);
                dst.setPixel(x + px, y + py, Pixel4(uint8_t(std::abs(d - s) & 0x0F)));
            }
    }

    // Tiled dither-pattern fill: `pattern` is a pw*ph block of 8-bit grayscale
    // values (0-255) tiled across the rect; each sample is mapped to 4-bit.
    //
    // The pattern is sampled at ABSOLUTE canvas coordinates (`x + px`, `y + py`),
    // not relative to the rect's top-left. This is the R4 "one grid" rule the
    // index shader also follows: two rects that abut across a tile boundary
    // carry one continuous pattern phase, so a lone dirty-tile repaint never
    // seams. (Rect-relative anchoring was retired — see the `blit.pattern`
    // entry in tests/waivers.hpp, Tomodachi #36.)
    inline void fillRectWithPattern(ICanvas<Pixel4> &dst, int16_t x, int16_t y,
                                    int16_t w, int16_t h, const uint8_t *pattern,
                                    int16_t pw, int16_t ph)
    {
        if (pw <= 0 || ph <= 0)
            return;
        for (int16_t py = 0; py < h; ++py)
            for (int16_t px = 0; px < w; ++px)
            {
                const int16_t ax = static_cast<int16_t>(x + px);
                const int16_t ay = static_cast<int16_t>(y + py);
                const uint8_t sx = static_cast<uint8_t>(((ax % pw) + pw) % pw);
                const uint8_t sy = static_cast<uint8_t>(((ay % ph) + ph) % ph);
                const uint8_t g = pattern[sy * pw + sx];
                dst.setPixel(ax, ay, Pixel4(g & 0x0F));
            }
    }

    // ---- offscreen ICanvas<Pixel4> source -> Pixel4 canvas ----

    inline void blitCanvasOpacity(ICanvas<Pixel4> &dst, const ICanvas<Pixel4> &src,
                                  int16_t x, int16_t y, uint8_t divisor,
                                  Pixel4 transparent = Pixel4(0))
    {
        if (divisor < 1)
            divisor = 1;
        for (int16_t sy = 0; sy < static_cast<int16_t>(src.getHeight()); ++sy)
            for (int16_t sx = 0; sx < static_cast<int16_t>(src.getWidth()); ++sx)
            {
                const Pixel4 sp = src.getPixel(sx, sy);
                if (sp.value == transparent.value)
                    continue;
                const int s = sp.value;
                dst.setPixel(x + sx, y + sy, Pixel4(uint8_t(s / divisor)));
            }
    }

    // Masked copy: write each source pixel only where the 1-bpp `mask` bit is
    // set — the geometric clip BASE got from `drawGrayscaleBitmap(..., mask,
    // ...)`. Templated on the mask type so this header stays free of Adafruit-
    // GFX; anything exposing `bool getPixel(x, y) const` (e.g. GFXcanvas1) fits.
    // Mask shares the source's geometry, both anchored at (x,y) on dst.
    template <typename Mask1bpp>
    inline void blitCanvasMasked(ICanvas<Pixel4> &dst, const ICanvas<Pixel4> &src,
                                 const Mask1bpp &mask, int16_t x, int16_t y)
    {
        for (int16_t sy = 0; sy < static_cast<int16_t>(src.getHeight()); ++sy)
            for (int16_t sx = 0; sx < static_cast<int16_t>(src.getWidth()); ++sx)
            {
                if (!mask.getPixel(sx, sy))
                    continue;
                dst.setPixel(x + sx, y + sy, src.getPixel(sx, sy));
            }
    }

    inline void addCanvas(ICanvas<Pixel4> &dst, const ICanvas<Pixel4> &src,
                          int16_t x = 0, int16_t y = 0)
    {
        for (int16_t sy = 0; sy < static_cast<int16_t>(src.getHeight()); ++sy)
            for (int16_t sx = 0; sx < static_cast<int16_t>(src.getWidth()); ++sx)
            {
                const int s = src.getPixel(sx, sy).value;
                const int d = dst.getPixel(x + sx, y + sy);
                // Panel-mask wrap, not saturation (see header note): 8+8 -> 0.
                dst.setPixel(x + sx, y + sy, Pixel4(uint8_t((d + s) & 0x0F)));
            }
    }

    inline void subtractCanvas(ICanvas<Pixel4> &dst, const ICanvas<Pixel4> &src,
                               int16_t x = 0, int16_t y = 0)
    {
        for (int16_t sy = 0; sy < static_cast<int16_t>(src.getHeight()); ++sy)
            for (int16_t sx = 0; sx < static_cast<int16_t>(src.getWidth()); ++sx)
            {
                const int s = src.getPixel(sx, sy).value;
                const int d = dst.getPixel(x + sx, y + sy);
                dst.setPixel(x + sx, y + sy, Pixel4(uint8_t(std::max(0, d - s))));
            }
    }

    // ---- Playdate-style 9-slice (Tomodachi #39) ----

    // Draw `src` into the dst rect (x, y, w, h) as a nine-slice: the source is
    // split into nine regions by `cornerW`/`cornerH` margins. The four corners
    // are copied 1:1; the four edges TILE along their axis and the centre tiles
    // in both — edges are **never stretched** (R4). Source pixels equal to
    // `transparent` are skipped, so a rounded panel's outside stays clear.
    //
    // Because it copies palette indices verbatim, the panel art re-skins for
    // free on a palette swap (the indices hold their roles). `cornerW`/`cornerH`
    // are clamped so the corners never overlap in either the source or the dst.
    inline void nineSlice(ICanvas<Pixel4> &dst, int16_t x, int16_t y, int16_t w,
                          int16_t h, const ICanvas<Pixel4> &src, int16_t cornerW,
                          int16_t cornerH, Pixel4 transparent = Pixel4(PALETTE_TRANSPARENT))
    {
        if (w <= 0 || h <= 0)
            return;
        const int16_t srcW = static_cast<int16_t>(src.getWidth());
        const int16_t srcH = static_cast<int16_t>(src.getHeight());
        if (srcW <= 0 || srcH <= 0)
            return;

        int16_t cw = cornerW;
        int16_t ch = cornerH;
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;
        cw = std::min<int16_t>(cw, std::min<int16_t>(srcW / 2, w / 2));
        ch = std::min<int16_t>(ch, std::min<int16_t>(srcH / 2, h / 2));

        const int16_t srcMidW = static_cast<int16_t>(srcW - 2 * cw);
        const int16_t srcMidH = static_cast<int16_t>(srcH - 2 * ch);

        // Map a dst coordinate to a src coordinate along one axis: fixed corners,
        // tiled middle. Returns -1 when the middle has no source strip to tile.
        auto mapAxis = [](int16_t d, int16_t start, int16_t extent, int16_t corner,
                          int16_t srcExtent, int16_t srcMid) -> int16_t {
            const int16_t off = static_cast<int16_t>(d - start);
            if (off < corner)
                return off;  // leading corner, copied 1:1
            if (off >= extent - corner)
                return static_cast<int16_t>(srcExtent - (extent - off));  // trailing corner
            if (srcMid <= 0)
                return -1;
            return static_cast<int16_t>(corner + ((off - corner) % srcMid));  // tiled middle
        };

        for (int16_t dy = y; dy < y + h; ++dy) {
            const int16_t sy = mapAxis(dy, y, h, ch, srcH, srcMidH);
            if (sy < 0) continue;
            for (int16_t dx = x; dx < x + w; ++dx) {
                const int16_t sx = mapAxis(dx, x, w, cw, srcW, srcMidW);
                if (sx < 0) continue;
                const Pixel4 sp = src.getPixel(sx, sy);
                if (sp.value == transparent.value) continue;
                dst.setPixel(dx, dy, sp);
            }
        }
    }
}

#endif // ENJIN2_GRAPHICS_BLIT_HPP
