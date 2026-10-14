#pragma once

#include <cstdint>

namespace enjin2
{

/**
 * @file pixel_stroke.hpp
 * @brief The pixel-perfect 8-connected walker behind Primitives' curves and
 *        the Shape rasterizer's open-path strokes (Tomodachi #332)
 */

/**
 * @brief Pixel sink for one curve walk that drops doubled "L" corners
 * @tparam Sink Callable `void(int64_t x, int64_t y)` that draws one pixel
 *
 * Holds the last two pixels back. When the next one touches the older,
 * the newer is dropped: the path stays 8-connected without it. That is
 * the corner of an "L" (the exact curve can pass through a pixel corner
 * where its slope crosses 45°, and the walk then takes an x step and a
 * y step one after the other), or a one-pixel spur where a curve dips
 * less than a pixel past an end and comes back.
 */
template<typename Sink>
struct PixelStroke {
    Sink sink;
    int64_t ax = 0, ay = 0, bx = 0, by = 0;
    int held = 0;

    explicit PixelStroke(Sink s) : sink(s) {}

    static int64_t absI64(int64_t v) { return v < 0 ? -v : v; }

    void plot(int64_t x, int64_t y) {
        if (held == 2 && x == bx && y == by) return;
        if (held == 2 && x == ax && y == ay) { held = 1; return; }
        if (held == 2 && absI64(x - ax) <= 1 && absI64(y - ay) <= 1) { bx = x; by = y; return; }
        if (held == 2) { set(ax, ay); ax = bx; ay = by; bx = x; by = y; return; }
        if (held == 1) { if (x == ax && y == ay) return; bx = x; by = y; held = 2; return; }
        ax = x; ay = y; held = 1;
    }

    /// @brief Bresenham from (x0,y0) to (x1,y1): Primitives::drawLine's steps.
    void line(int64_t x0, int64_t y0, int64_t x1, int64_t y1) {
        const int64_t dx = absI64(x1 - x0), dy = absI64(y1 - y0);
        const int64_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int64_t err = dx - dy;
        while (true) {
            plot(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int64_t e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx)  { err += dx; y0 += sy; }
        }
    }

    void finish() {
        if (held >= 1) set(ax, ay);
        if (held == 2) set(bx, by);
    }

    /// @brief Finish where the path runs on into (x,y), drawn by another
    /// stroke: (x,y) still decides which corner to drop, but isn't drawn.
    void finishBefore(int64_t x, int64_t y) {
        plot(x, y);
        if (held == 2) set(ax, ay);
    }

    /// @brief Finish two strokes walked toward each other, whose ends are
    /// distinct neighbouring pixels: each drops its "L" corner against the
    /// other's end, so the join is treated the same from either side.
    static void meet(PixelStroke& one, PixelStroke& two) {
        int64_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        const bool has1 = one.newest(x1, y1), has2 = two.newest(x2, y2);
        if (has2) one.finishBefore(x2, y2); else one.finish();
        if (has1) two.finishBefore(x1, y1); else two.finish();
    }

    bool newest(int64_t& x, int64_t& y) const {
        if (held == 0) return false;
        x = held == 2 ? bx : ax;
        y = held == 2 ? by : ay;
        return true;
    }

    void set(int64_t x, int64_t y) { sink(x, y); }
};

} // namespace enjin2
