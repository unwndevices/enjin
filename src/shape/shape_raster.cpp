// Shape fill rasterizer (ADR-0016, Tomodachi #329). See shape_raster.hpp for
// the coverage rule and the mask layout.
//
// Sub-sample grid: 4 × 4 per pixel. Sub-row sy has its centres at
// y = (sy + ½) / 4 px, i.e. raw Q16.16 (2·sy + 1)·8192; sub-column sx at
// x = (sx + ½) / 4. An edge crossing a sub-row at x covers, on its right,
// the sub-columns from c = ceil(4x − ½) on: a centre exactly on the crossing
// is on the right side (in for a left edge, out for a right edge). Edges are
// active on the sub-rows whose centre is in [top, bottom). Integer only.

#include "enjin2/shape/shape_raster.hpp"

#include "enjin2/graphics/pixel_stroke.hpp"

#include <cstring>

namespace enjin2 {
namespace shape {

namespace {

constexpr int64_t kSubRow  = 16384;  // one sub-row (¼ px) in raw Q16.16
constexpr int64_t kHalfSub = 8192;   // half a sub-row / sub-column

// Floor / ceil division, b > 0. (One divide: the remainder is a - q·b, so the
// S3 makes one __divdi3 call, not __divdi3 + __moddi3.)
inline int64_t floorDiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if (a - q * b != 0 && a < 0)
        --q;
    return q;
}
inline int64_t ceilDiv(int64_t a, int64_t b) { return -floorDiv(-a, b); }

inline int32_t floorPx(int32_t raw) { return raw >> Fixed::kFracBits; }
inline int32_t ceilPx(int32_t raw) { return int32_t((int64_t(raw) + Fixed::kOne - 1) >> Fixed::kFracBits); }

inline void setBit(uint32_t* words, int p) { words[p >> 5] |= 1u << (p & 31); }

// Set bits [from, to) of a row, to > from.
inline void setBits(uint32_t* words, int from, int to)
{
    int wi = from >> 5;
    const int we = (to - 1) >> 5;
    const uint32_t head = ~0u << (from & 31);
    const uint32_t tail = ~0u >> (31 - ((to - 1) & 31));
    if (wi == we)
    {
        words[wi] |= head & tail;
        return;
    }
    words[wi++] |= head;
    while (wi < we)
        words[wi++] = ~0u;
    words[we] |= tail;
}

} // namespace

bool Rasterizer::fill(const Shape& shape, const Rect& clip, RowMask& out)
{
    out.clear();

    // 1. Pixel bbox of the closed rings, and the coordinate bound.
    const int32_t lim = kMaxCoordPx * Fixed::kOne;
    int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool any = false;
    for (uint16_t ri = 0; ri < shape.ringCount; ++ri)
    {
        const Ring& ring = shape.rings[ri];
        if (!ring.closed || ring.count < 3)
            continue;
        for (uint16_t i = 0; i < ring.count; ++i)
        {
            const int32_t x = ring.points[i].x.raw, y = ring.points[i].y.raw;
            if (x < -lim || x > lim || y < -lim || y > lim)
                return false;
            if (!any)
            {
                minX = maxX = x;
                minY = maxY = y;
                any = true;
                continue;
            }
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    int32_t px0 = floorPx(minX), px1 = ceilPx(maxX);
    int32_t py0 = floorPx(minY), py1 = ceilPx(maxY);
    if (px0 < clip.x) px0 = clip.x;
    if (py0 < clip.y) py0 = clip.y;
    if (px1 > int32_t(clip.x) + clip.width) px1 = int32_t(clip.x) + clip.width;
    if (py1 > int32_t(clip.y) + clip.height) py1 = int32_t(clip.y) + clip.height;
    if (!any || px1 <= px0 || py1 <= py0)
    {
        out.reset(int16_t(clip.x), int16_t(clip.y), 0, 0);
        return true;
    }
    const int w = px1 - px0, h = py1 - py0;
    if (w > kMaxMaskWidth)
        return false;
    if (!out.reset(int16_t(px0), int16_t(py0), uint16_t(w), uint16_t(h)))
        return false;

    // 2. Edges, clipped to the mask's sub-rows, each set up at its first one.
    const int32_t syClip0 = py0 * 4, syClip1 = py1 * 4;
    uint16_t n = 0;
    for (uint16_t ri = 0; ri < shape.ringCount; ++ri)
    {
        const Ring& ring = shape.rings[ri];
        if (!ring.closed || ring.count < 3)
            continue;
        for (uint16_t i = 0; i < ring.count; ++i)
        {
            const FixedVec2& a = ring.points[i];
            const FixedVec2& b = ring.points[i + 1 == ring.count ? 0 : i + 1];
            if (a.y.raw == b.y.raw)
                continue;
            const bool down = a.y.raw < b.y.raw;
            const FixedVec2& t = down ? a : b;
            const FixedVec2& u = down ? b : a;
            const int64_t Y0 = t.y.raw, Y1 = u.y.raw, X0 = t.x.raw;
            const int64_t dX = int64_t(u.x.raw) - X0, D = Y1 - Y0;
            int64_t syStart = ceilDiv(Y0 - kHalfSub, kSubRow);
            int64_t syEnd = ceilDiv(Y1 - kHalfSub, kSubRow);
            if (syStart < syClip0) syStart = syClip0;
            if (syEnd > syClip1) syEnd = syClip1;
            if (syStart >= syEnd)
                continue;
            if (n == kMaxEdges)
            {
                out.clear();
                return false;
            }
            // Crossing column at sub-row sy: c = ceil(M / den), with
            // M = (X0 − ½ sub)·D + (Yc − Y0)·dX and den = sub·D (raw units).
            const int64_t den = kSubRow * D;
            const int64_t yc = (2 * syStart + 1) * kHalfSub;
            const int64_t M = (X0 - kHalfSub) * D + (yc - Y0) * dX;
            const int64_t c = ceilDiv(M, den);
            const int64_t step = kSubRow * dX;
            const int64_t q = floorDiv(step, den);
            Edge& e = edges_[n++];
            e.syStart = int32_t(syStart);
            e.syEnd = int32_t(syEnd);
            e.c = int32_t(c);
            e.r = c * den - M;
            e.q = int32_t(q);
            e.s = step - q * den;
            e.den = den;
            e.dir = down ? 1 : -1;
        }
    }

    // Edges by first sub-row (stable insertion sort: n is small).
    for (uint16_t i = 1; i < n; ++i)
    {
        const Edge e = edges_[i];
        uint16_t j = i;
        while (j > 0 && edges_[j - 1].syStart > e.syStart)
        {
            edges_[j] = edges_[j - 1];
            --j;
        }
        edges_[j] = e;
    }

    // 3. Scan, one pixel row (four sub-rows) at a time.
    // Inside: winding & ruleMask != 0 (non-zero: any bit; even-odd: the low bit).
    const int ruleMask = shape.paint.rule == FillRule::EvenOdd ? 1 : ~0;
    const int32_t cx0 = px0 * 4, cw = w * 4;
    const uint16_t wpr = out.wordsPerRow();
    std::memset(counts_, 0, size_t(w));
    uint16_t next = 0, nActive = 0;

    for (int r = 0; r < h; ++r)
    {
        for (uint16_t wi = 0; wi < wpr; ++wi)
            full_[0][wi] = full_[1][wi] = full_[2][wi] = full_[3][wi] = touched_[wi] = 0;

        for (int k = 0; k < 4; ++k)
        {
            const int32_t sy = (py0 + r) * 4 + k;
            while (next < n && edges_[next].syStart <= sy)
                active_[nActive++] = next++;

            // Crossings of the live edges; step them, drop the finished.
            uint16_t nc = 0, keep = 0;
            for (uint16_t ai = 0; ai < nActive; ++ai)
            {
                Edge& e = edges_[active_[ai]];
                if (sy >= e.syEnd)
                    continue;
                active_[keep++] = active_[ai];
                int32_t c = e.c - cx0;
                if (c < 0) c = 0;
                if (c > cw) c = cw;
                cross_[nc].c = c;
                cross_[nc].dir = e.dir;
                ++nc;
                e.c += e.q;
                e.r -= e.s;
                if (e.r < 0)
                {
                    e.r += e.den;
                    e.c += 1;
                }
            }
            nActive = keep;

            for (uint16_t i = 1; i < nc; ++i)
            {
                const Crossing x = cross_[i];
                uint16_t j = i;
                while (j > 0 && cross_[j - 1].c > x.c)
                {
                    cross_[j] = cross_[j - 1];
                    --j;
                }
                cross_[j] = x;
            }

            // Inside spans [a, b) in sub-columns; a span covers a pixel whole
            // (a word bit) or part of it (a count on an edge pixel).
            uint32_t* full = full_[k];
            int wind = 0;
            int32_t a = 0;
            for (uint16_t i = 0; i < nc; ++i)
            {
                const bool was = (wind & ruleMask) != 0;
                wind += cross_[i].dir;
                const bool now = (wind & ruleMask) != 0;
                if (!was && now)
                {
                    a = cross_[i].c;
                    continue;
                }
                if (!(was && !now))
                    continue;
                const int32_t b = cross_[i].c;
                if (b <= a)
                    continue;
                const int pa = a >> 2, pl = (b - 1) >> 2;
                if (pa == pl)
                {
                    if (b - a == 4)
                        setBit(full, pa);
                    else
                    {
                        counts_[pa] = uint8_t(counts_[pa] + (b - a));
                        setBit(touched_, pa);
                    }
                    continue;
                }
                int fs = pa, fe = pl + 1;
                if (a & 3)
                {
                    counts_[pa] = uint8_t(counts_[pa] + (4 - (a & 3)));
                    setBit(touched_, pa);
                    fs = pa + 1;
                }
                if (b & 3)
                {
                    counts_[pl] = uint8_t(counts_[pl] + (b & 3));
                    setBit(touched_, pl);
                    fe = pl;
                }
                if (fs < fe)
                    setBits(full, fs, fe);
            }
        }

        // Whole in all four sub-rows: 16/16, set. Otherwise count the edge
        // pixel: 4 per whole sub-row plus its partial samples.
        uint32_t* dst = out.row(uint16_t(r));
        for (uint16_t wi = 0; wi < wpr; ++wi)
        {
            const uint32_t f0 = full_[0][wi], f1 = full_[1][wi], f2 = full_[2][wi], f3 = full_[3][wi];
            const uint32_t all = f0 & f1 & f2 & f3;
            uint32_t edge = (f0 | f1 | f2 | f3 | touched_[wi]) & ~all;
            uint32_t res = all;
            while (edge)
            {
                const int bit = __builtin_ctz(edge);
                const int p = (wi << 5) + bit;
                const int cnt = counts_[p] + 4 * int(((f0 >> bit) & 1) + ((f1 >> bit) & 1) +
                                                     ((f2 >> bit) & 1) + ((f3 >> bit) & 1));
                counts_[p] = 0;
                if (cnt >= 8)
                    res |= 1u << bit;
                edge &= edge - 1;
            }
            dst[wi] = res;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Paint (Tomodachi #332): word operations on masks
// ---------------------------------------------------------------------------

namespace {
constexpr uint16_t kMaxRowWords = Rasterizer::kMaxMaskWidth / 32;

// 32 bits of a mask row starting at mask-local column c (zeros off the row).
inline uint32_t window(const uint32_t* row, int wpr, int c)
{
    const int wi = c >= 0 ? c >> 5 : -((31 - c) >> 5);
    const int sh = c - wi * 32;
    const uint32_t lo = (wi >= 0 && wi < wpr) ? row[wi] : 0u;
    if (sh == 0)
        return lo;
    const uint32_t hi = (wi + 1 >= 0 && wi + 1 < wpr) ? row[wi + 1] : 0u;
    return (lo >> sh) | (hi << (32 - sh));
}
} // namespace

bool Rasterizer::stroke(const Shape& shape, const Rect& clip, RowMask& out)
{
    out.clear();
    const int w = shape.paint.strokeWidth;
    const int32_t lim = kMaxCoordPx * Fixed::kOne;
    int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool any = false;
    for (uint16_t ri = 0; w > 0 && ri < shape.ringCount; ++ri)
    {
        const Ring& ring = shape.rings[ri];
        if (ring.closed)
            continue;
        for (uint16_t i = 0; i < ring.count; ++i)
        {
            const int32_t x = ring.points[i].x.raw, y = ring.points[i].y.raw;
            if (x < -lim || x > lim || y < -lim || y > lim)
                return false;
            const int32_t cx = floorPx(x), cy = floorPx(y);
            if (!any)
            {
                minX = maxX = cx;
                minY = maxY = cy;
                any = true;
                continue;
            }
            if (cx < minX) minX = cx;
            if (cx > maxX) maxX = cx;
            if (cy < minY) minY = cy;
            if (cy > maxY) maxY = cy;
        }
    }
    // The stamp of pixel (x, y) covers [x − o, x − o + w) × [y − o, y − o + w).
    const int o = (w - 1) / 2;
    int32_t px0 = minX - o, px1 = maxX - o + w, py0 = minY - o, py1 = maxY - o + w;
    if (px0 < clip.x) px0 = clip.x;
    if (py0 < clip.y) py0 = clip.y;
    if (px1 > int32_t(clip.x) + clip.width) px1 = int32_t(clip.x) + clip.width;
    if (py1 > int32_t(clip.y) + clip.height) py1 = int32_t(clip.y) + clip.height;
    if (!any || px1 <= px0 || py1 <= py0)
    {
        out.reset(int16_t(clip.x), int16_t(clip.y), 0, 0);
        return true;
    }
    if (px1 - px0 > kMaxMaskWidth || !out.reset(int16_t(px0), int16_t(py0), uint16_t(px1 - px0), uint16_t(py1 - py0)))
    {
        out.clear();
        return false;
    }
    for (uint16_t r = 0; r < out.height(); ++r)
        std::memset(out.row(r), 0, size_t(out.wordsPerRow()) * 4);

    struct Stamp {
        RowMask* m;
        int w, o;
        void operator()(int64_t x, int64_t y) const
        {
            int64_t x0 = x - o - m->x(), x1 = x0 + w, y0 = y - o - m->y(), y1 = y0 + w;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > m->width()) x1 = m->width();
            if (y1 > m->height()) y1 = m->height();
            if (x0 >= x1)
                return;
            for (int64_t r = y0; r < y1; ++r)
                setBits(m->row(uint16_t(r)), int(x0), int(x1));
        }
    };
    for (uint16_t ri = 0; ri < shape.ringCount; ++ri)
    {
        const Ring& ring = shape.rings[ri];
        if (ring.closed || ring.count == 0)
            continue;
        PixelStroke<Stamp> walk(Stamp{&out, w, o});
        int64_t lx = floorPx(ring.points[0].x.raw), ly = floorPx(ring.points[0].y.raw);
        walk.plot(lx, ly);
        for (uint16_t i = 1; i < ring.count; ++i)
        {
            const int64_t x = floorPx(ring.points[i].x.raw), y = floorPx(ring.points[i].y.raw);
            walk.line(lx, ly, x, y);
            lx = x;
            ly = y;
        }
        walk.finish();
    }
    return true;
}

void clipMask(RowMask& dst, const RowMask& clip)
{
    const uint16_t wpr = dst.wordsPerRow();
    for (uint16_t r = 0; r < dst.height(); ++r)
    {
        uint32_t* row = dst.row(r);
        const int cr = int(dst.y()) + r - clip.y();
        if (clip.empty() || cr < 0 || cr >= clip.height())
        {
            std::memset(row, 0, size_t(wpr) * 4);
            continue;
        }
        const uint32_t* src = clip.row(uint16_t(cr));
        const int base = int(dst.x()) - clip.x();
        for (uint16_t wi = 0; wi < wpr; ++wi)
            row[wi] &= window(src, clip.wordsPerRow(), base + wi * 32);
    }
}

void uniteMask(RowMask& dst, const RowMask& src)
{
    if (dst.empty() || src.empty())
        return;
    const uint16_t wpr = dst.wordsPerRow();
    const int tailBits = dst.width() & 31;
    const uint32_t tail = tailBits ? ~0u >> (32 - tailBits) : ~0u;
    for (uint16_t r = 0; r < src.height(); ++r)
    {
        const int dr = int(src.y()) + r - dst.y();
        if (dr < 0 || dr >= dst.height())
            continue;
        uint32_t* row = dst.row(uint16_t(dr));
        const uint32_t* s = src.row(r);
        const int base = int(dst.x()) - src.x();
        for (uint16_t wi = 0; wi < wpr; ++wi)
            row[wi] |= window(s, src.wordsPerRow(), base + wi * 32);
        row[wpr - 1] &= tail;
    }
}

bool innerStroke(const RowMask& fill, uint8_t width, RowMask& out)
{
    out.clear();
    if (fill.empty() || width == 0)
    {
        out.reset(fill.x(), fill.y(), 0, 0);
        return true;
    }
    const uint16_t wpr = fill.wordsPerRow(), h = fill.height();
    if (wpr > kMaxRowWords || !out.reset(fill.x(), fill.y(), fill.width(), h))
    {
        out.clear();
        return false;
    }
    // out = the fill, eroded in place `width` times (one row of look-back).
    for (uint16_t r = 0; r < h; ++r)
        std::memcpy(out.row(r), fill.row(r), size_t(wpr) * 4);
    uint32_t prev[kMaxRowWords], cur[kMaxRowWords];
    for (uint8_t k = 0; k < width; ++k)
    {
        bool any = false;
        std::memset(prev, 0, sizeof prev);
        for (uint16_t r = 0; r < h; ++r)
        {
            uint32_t* row = out.row(r);
            std::memcpy(cur, row, size_t(wpr) * 4);
            const uint32_t* below = r + 1 < h ? out.row(uint16_t(r + 1)) : nullptr;
            for (uint16_t wi = 0; wi < wpr; ++wi)
            {
                const uint32_t c = cur[wi];
                const uint32_t left = (c << 1) | (wi > 0 ? cur[wi - 1] >> 31 : 0u);
                const uint32_t right = (c >> 1) | (wi + 1 < wpr ? cur[wi + 1] << 31 : 0u);
                const uint32_t e = c & left & right & prev[wi] & (below ? below[wi] : 0u);
                row[wi] = e;
                any = any || e != 0;
            }
            std::memcpy(prev, cur, size_t(wpr) * 4);
        }
        if (!any)
            break;
    }
    // Stroke = fill minus its erosion.
    for (uint16_t r = 0; r < h; ++r)
    {
        uint32_t* row = out.row(r);
        const uint32_t* f = fill.row(r);
        for (uint16_t wi = 0; wi < wpr; ++wi)
            row[wi] = f[wi] & ~row[wi];
    }
    return true;
}

} // namespace shape
} // namespace enjin2
