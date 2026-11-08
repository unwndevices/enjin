#ifndef ENJIN2_GRAPHICS_BLIT_HPP
#define ENJIN2_GRAPHICS_BLIT_HPP

// Pixel4-native compositing helpers.
//
// Free functions straight against any ICanvas<Pixel4> framebuffer (Canvas4 is
// deliberately lean, so these are not member API): the nine-slice panel blit,
// filled ellipses (solid or a Remap of what is under them), the warp blit and
// the column-scaled strip blit.
// They copy or remap palette indices; none does arithmetic on them. The
// Eisei-era grey-level blits (8-bit grey sources, additive/subtractive and
// opacity blends) were deleted with the 8-role palette (ADR-0022, Tomodachi
// #426): sums and quotients of indices are no Palette role.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "../core/types.hpp"
#include "canvas.hpp"
#include "palette.hpp"
#include "remap.hpp"

namespace enjin2
{
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

    // ---- Filled ellipse (Tomodachi #43) ----

    // Scanline-filled axis-aligned ellipse centred at (cx, cy) with radii
    // (rx, ry). A free immediate draw meant to run BEFORE Scene::renderObjects
    // — the launcher's optional ground shadow is one of these under the pet,
    // toggled by a style slot (off by default). Cheap: one span per row via the
    // ellipse equation, no per-pixel divide, no allocation.
    //
    // Two overloads mirror the compositor's colour/Remap split:
    //   * colour  — paint a solid Pixel4 (a hard blob).
    //   * Remap   — remap the pixels ALREADY under the ellipse (read dst, write
    //               remap.apply(dst)). `Remap::towardBg()` gives a soft
    //               drop-shadow that tints the floor rather than stamping ink.
    // Pixels equal to PALETTE_TRANSPARENT are left untouched by the Remap form
    // so a shadow never paints into a hole.

    namespace detail {
        // For row dy (0-based from the top of the ellipse's bounding rows),
        // invoke `plot(x)` across the ellipse's horizontal span. Shared by both
        // fillEllipse overloads so the rasterisation lives in one place.
        template <typename Plot>
        inline void ellipseSpans(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                                 Plot plot)
        {
            if (rx <= 0 || ry <= 0)
                return;
            const float rxf = static_cast<float>(rx);
            const float ryf = static_cast<float>(ry);
            for (int16_t dy = -ry; dy <= ry; ++dy) {
                // half-width where (dy/ry)^2 + (hx/rx)^2 = 1
                const float ny = static_cast<float>(dy) / ryf;
                const float inside = 1.0f - ny * ny;
                if (inside < 0.0f)
                    continue;
                const int16_t hx = static_cast<int16_t>(rxf * std::sqrt(inside));
                const int16_t y = static_cast<int16_t>(cy + dy);
                for (int16_t x = static_cast<int16_t>(cx - hx);
                     x <= static_cast<int16_t>(cx + hx); ++x) {
                    plot(x, y);
                }
            }
        }
    }

    inline void fillEllipse(ICanvas<Pixel4> &dst, int16_t cx, int16_t cy,
                            int16_t rx, int16_t ry, Pixel4 color)
    {
        detail::ellipseSpans(cx, cy, rx, ry,
                             [&](int16_t x, int16_t y) { dst.setPixel(x, y, color); });
    }

    inline void fillEllipse(ICanvas<Pixel4> &dst, int16_t cx, int16_t cy,
                            int16_t rx, int16_t ry, const Remap &remap)
    {
        detail::ellipseSpans(cx, cy, rx, ry, [&](int16_t x, int16_t y) {
            const uint8_t d = dst.getPixel(x, y).value;
            if (d == PALETTE_TRANSPARENT)
                return;  // a shadow never paints into a hole
            dst.setPixel(x, y, Pixel4(remap.apply(d)));
        });
    }

    // ---- bilinear quad keystone warp (Tomodachi #44) ----

    // Warp `src` into an arbitrary destination quad given by its four corners,
    // clockwise from the top-left: `quad[0]=TL, quad[1]=TR, quad[2]=BR,
    // quad[3]=BL`. This is the banner->panel IMU shear: a cached panel `Canvas4`
    // stroked once (9-slice) and re-blitted every frame into the tilt quad.
    //
    // A keystone is affine **per horizontal strip**, so no per-pixel perspective
    // divide is needed — each source texel is forward-mapped through the
    // bilinear of the four corners (all multiplies) and filled over its integer
    // destination footprint. Neighbouring texels' footprints overlap by design
    // (the "downward + rightward overlap"), so a stretched quad leaves no
    // hairline seams; a shrunk quad (opening panel) collapses many texels onto
    // one pixel, nearest-neighbour. Every source pixel is read exactly once
    // (a full 384x384 panel ≈ 147k reads ≈ 4.3 ms @240 MHz). Source pixels equal
    // to `transparent` are skipped so the rounded panel's outside stays clear.
    //
    // The caller keys the warp on the integer quad corners: an unchanged quad
    // (a still hand) re-blits the cached output and never calls this at all —
    // the zero-read still-frame path lives at the call site, not here.
    inline void warpCanvas(ICanvas<Pixel4> &dst, const ICanvas<Pixel4> &src,
                           const Point quad[4],
                           Pixel4 transparent = Pixel4(PALETTE_TRANSPARENT))
    {
        const int16_t sw = static_cast<int16_t>(src.getWidth());
        const int16_t sh = static_cast<int16_t>(src.getHeight());
        if (sw <= 0 || sh <= 0)
            return;

        const float TLx = quad[0].x, TLy = quad[0].y;
        const float TRx = quad[1].x, TRy = quad[1].y;
        const float BRx = quad[2].x, BRy = quad[2].y;
        const float BLx = quad[3].x, BLy = quad[3].y;

        const float invW = 1.0f / static_cast<float>(sw);
        const float invH = 1.0f / static_cast<float>(sh);

        // Bilinear of the four corners at (u,v) in [0,1]^2 — no per-pixel divide.
        auto mapX = [&](float u, float v) {
            const float tx = TLx + (TRx - TLx) * u;
            const float bx = BLx + (BRx - BLx) * u;
            return tx + (bx - tx) * v;
        };
        auto mapY = [&](float u, float v) {
            const float ty = TLy + (TRy - TLy) * u;
            const float by = BLy + (BRy - BLy) * u;
            return ty + (by - ty) * v;
        };

        for (int16_t sy = 0; sy < sh; ++sy) {
            const float v0 = static_cast<float>(sy) * invH;
            const float v1 = static_cast<float>(sy + 1) * invH;
            for (int16_t sx = 0; sx < sw; ++sx) {
                const Pixel4 sp = src.getPixel(sx, sy);
                if (sp.value == transparent.value)
                    continue;
                const float u0 = static_cast<float>(sx) * invW;
                const float u1 = static_cast<float>(sx + 1) * invW;
                // Dest footprint = the quad of this texel's four (u,v) corners;
                // fill its integer bbox nearest-neighbour.
                const float xs[4] = {mapX(u0, v0), mapX(u1, v0), mapX(u1, v1), mapX(u0, v1)};
                const float ys[4] = {mapY(u0, v0), mapY(u1, v0), mapY(u1, v1), mapY(u0, v1)};
                float fminx = xs[0], fmaxx = xs[0], fminy = ys[0], fmaxy = ys[0];
                for (int i = 1; i < 4; ++i) {
                    fminx = std::min(fminx, xs[i]);
                    fmaxx = std::max(fmaxx, xs[i]);
                    fminy = std::min(fminy, ys[i]);
                    fmaxy = std::max(fmaxy, ys[i]);
                }
                const int16_t minx = static_cast<int16_t>(std::floor(fminx));
                const int16_t maxx = static_cast<int16_t>(std::floor(fmaxx));
                const int16_t miny = static_cast<int16_t>(std::floor(fminy));
                const int16_t maxy = static_cast<int16_t>(std::floor(fmaxy));
                for (int16_t dy = miny; dy <= maxy; ++dy)
                    for (int16_t dx = minx; dx <= maxx; ++dx)
                        dst.setPixel(dx, dy, sp);
            }
        }
    }

    // ---- column-scaled strip blit (Tomodachi #459, ADR-0026) ----

    // Draw `src` as a strip `w` px wide whose left edge is `hL` px tall and
    // whose right edge is `hR`, both centred on one line: the cover flow's Side
    // covers (an inner edge that recedes) and its Flip (a width that narrows to
    // nothing). (x, y) is the top-left of the strip's bounding box, so its
    // centre line is max(hL, hR) / 2 rows below y.
    //
    // Column i (0..w-1) is hL + (hR - hL) * i / (w - 1) px tall, rounded half
    // away from zero, and starts (max(hL, hR) - its height) / 2 rows below y.
    // It reads source column i * srcW / w, and its row j reads source row
    // j * srcH / height: nearest by column, then nearest by row within the
    // column. With w, hL and hR the source's own size it is a plain blit.
    //
    // Source pixels equal to `transparent` are skipped, and the strip is
    // clipped to the canvas; a zero or negative width or height draws nothing.
    inline void stripBlit(ICanvas<Pixel4> &dst, const ICanvas<Pixel4> &src, int16_t x,
                          int16_t y, int16_t w, int16_t hL, int16_t hR,
                          Pixel4 transparent = Pixel4(PALETTE_TRANSPARENT))
    {
        const int srcW = src.getWidth();
        const int srcH = src.getHeight();
        if (w <= 0 || srcW <= 0 || srcH <= 0 || (hL <= 0 && hR <= 0))
            return;
        const int dstW = dst.getWidth();
        const int dstH = dst.getHeight();
        const int tallest = std::max<int>(hL, hR);
        const int rise = hR - hL;
        const int steps = w - 1;

        // Only the columns inside the canvas, in int so a far-off start can't wrap.
        const int i0 = std::max(0, -int(x));
        const int i1 = std::min(int(w), dstW - int(x));
        for (int i = i0; i < i1; ++i) {
            int hc = hL;
            if (steps > 0) {
                const int64_t num = int64_t(2) * rise * i;  // past int at full int16 range
                hc += int((num >= 0 ? num + steps : num - steps) / (2 * steps));
            }
            if (hc <= 0)
                continue;
            const int top = int(y) + (tallest - hc) / 2;
            const int sx = i * srcW / w;
            const int dx = int(x) + i;
            const int j0 = std::max(0, -top);
            const int j1 = std::min(hc, dstH - top);
            for (int j = j0; j < j1; ++j) {
                const Pixel4 sp = src.getPixel(static_cast<int16_t>(sx),
                                               static_cast<int16_t>(j * srcH / hc));
                if (sp.value == transparent.value)
                    continue;
                dst.setPixel(static_cast<int16_t>(dx), static_cast<int16_t>(top + j), sp);
            }
        }
    }
}

#endif // ENJIN2_GRAPHICS_BLIT_HPP
