#pragma once

#include <cstdint>
#include <type_traits>

#include "../core/math.hpp"

namespace enjin2 {

/**
 * @brief A shape as one horizontal span per row: row y covers x in [x0[y], x1[y]).
 *
 * The compositor's mask type (LayerCompositor::setMask / setCover). One span a
 * row holds any convex shape (a disc, a rect, an iris from a corner), which is
 * what screen transitions need; a row with x0 >= x1 is empty. Spans are kept
 * clipped to [0, W).
 *
 * @tparam W Canvas width in pixels
 * @tparam H Canvas height in pixels
 */
template <uint16_t W, uint16_t H>
struct SpanMask {
    /// A span edge: a byte while the canvas fits one (a 160-row mask is 320 B).
    using Edge = typename std::conditional<(W <= 255), uint8_t, uint16_t>::type;
    Edge x0[H];
    Edge x1[H];

    /// Every row empty: nothing is inside.
    void clear() {
        for (uint16_t y = 0; y < H; ++y) {
            x0[y] = 0;
            x1[y] = 0;
        }
    }

    /// Every row full: the whole canvas is inside.
    void fill() {
        for (uint16_t y = 0; y < H; ++y) {
            x0[y] = 0;
            x1[y] = static_cast<Edge>(W);
        }
    }

    /// The disc of radius r around (cx, cy): pixel (x, y) is inside when its
    /// centre (x + 0.5, y + 0.5) is within r of (cx, cy). The centre may lie off
    /// the canvas (a corner, an iris from outside); r <= 0 is empty.
    void disc(int cx, int cy, int r) {
        // Doubled coordinates keep the pixel-centre test in integers:
        // (2x + 1 - 2cx)^2 + (2y + 1 - 2cy)^2 <= (2r)^2.
        const int32_t rr = 4 * static_cast<int32_t>(r) * r;
        for (uint16_t y = 0; y < H; ++y) {
            x0[y] = 0;
            x1[y] = 0;
            if (r <= 0) continue;
            const int32_t dy = 2 * static_cast<int32_t>(y) + 1 - 2 * cy;
            const int32_t rest = rr - dy * dy;
            if (rest < 0) continue;
            // |2x + 1 - 2cx| <= h (an odd left side)  <=>  x in [cx - k, cx + k)
            // with k = (h + 1) / 2.
            const int32_t h = math::isqrt(static_cast<uint32_t>(rest));
            const int32_t k = (h + 1) / 2;
            int32_t a = cx - k;
            int32_t b = cx + k;
            if (a < 0) a = 0;
            if (b > W) b = W;
            if (a < b) {
                x0[y] = static_cast<Edge>(a);
                x1[y] = static_cast<Edge>(b);
            }
        }
    }

    /// Whether pixel (x, y) is inside.
    bool contains(int x, int y) const {
        if (y < 0 || y >= H) return false;
        return x >= x0[y] && x < x1[y];
    }

    /// Pixels inside.
    uint32_t area() const {
        uint32_t n = 0;
        for (uint16_t y = 0; y < H; ++y) {
            if (x1[y] > x0[y]) n += static_cast<uint32_t>(x1[y]) - x0[y];
        }
        return n;
    }
};

/**
 * @brief A disc's radius by the share of the canvas it covers, from a table.
 *
 * Built once (the area of every whole radius, from the same spans the
 * compositor masks with), then each frame's radius is a binary search: no
 * geometry per frame. A transition eases the covered share rather than the
 * radius, since a disc's area grows as r² (an ease-in radius sits invisible
 * for half its run, then floods the screen).
 *
 * @tparam W Canvas width in pixels
 * @tparam H Canvas height in pixels
 */
template <uint16_t W, uint16_t H>
class DiscCoverage {
public:
    /// The largest radius any centre on or around the canvas needs.
    static constexpr int kMaxRadius = W + H;

    /// Tabulate the disc around (cx, cy) for every radius up to the one that
    /// covers the whole canvas.
    void build(int cx, int cy) {
        cx_ = cx;
        cy_ = cy;
        SpanMask<W, H> m;
        maxRadius_ = kMaxRadius;
        for (int r = 0; r <= kMaxRadius; ++r) {
            m.disc(cx, cy, r);
            area_[r] = static_cast<Area>(m.area());
            if (area_[r] >= kTotal) {
                maxRadius_ = r;
                break;
            }
        }
    }

    int centreX() const { return cx_; }
    int centreY() const { return cy_; }
    /// The smallest radius that covers the whole canvas.
    int fullRadius() const { return maxRadius_; }

    /// The smallest radius whose disc covers at least `share` (0..1) of the
    /// canvas: 0 at 0, fullRadius() at 1.
    int radiusFor(float share) const {
        if (share <= 0.0f) return 0;
        if (share >= 1.0f) return maxRadius_;
        const uint32_t want = static_cast<uint32_t>(share * static_cast<float>(kTotal) + 0.5f);
        int lo = 0, hi = maxRadius_;
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (area_[mid] >= want) hi = mid;
            else lo = mid + 1;
        }
        return lo;
    }

    /// The share radius r covers (the table's inverse).
    float shareAt(int r) const {
        if (r <= 0) return 0.0f;
        if (r >= maxRadius_) return 1.0f;
        return static_cast<float>(area_[r]) / static_cast<float>(kTotal);
    }

private:
    static constexpr uint32_t kTotal = static_cast<uint32_t>(W) * H;
    /// A pixel count: two bytes while the canvas fits (160x160: a 642 B table).
    using Area = typename std::conditional<(kTotal <= 0xFFFFu), uint16_t, uint32_t>::type;
    int cx_ = 0, cy_ = 0;
    int maxRadius_ = 0;
    Area area_[kMaxRadius + 1] = {};
};

} // namespace enjin2
