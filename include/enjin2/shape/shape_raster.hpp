/**
 * @file shape_raster.hpp
 * @brief Fill warped Shape rings into a 1-bpp mask, paint it (ADR-0016, Tomodachi #329)
 *
 * ## Coverage (the signed-off look, #314)
 *
 * Each pixel holds a 4×4 grid of sub-samples at (x + (i+½)/4, y + (j+½)/4).
 * A sub-sample is inside under the Shape's fill rule; a pixel is set when at
 * least 8 of its 16 are inside (**majority**). Sub-samples follow the
 * pixel-centre + top-left rule: a sample centre exactly on a left or top edge
 * is in, on a right or bottom edge out, so two Shapes sharing an edge never
 * both claim a sample. There is no centre-sampling fallback.
 *
 * Only pixels an edge crosses are counted: a sub-row span marks the pixels it
 * covers whole as words, and a pixel whole in all four sub-rows is set for
 * free. The rest of the touched pixels get their 16-sample count.
 *
 * ## Determinism
 *
 * Integer only: edges step exactly (quotient + remainder, no accumulated
 * error) in 64-bit, so the same rings give the same bits on the S3, WASM and
 * native. Ring coordinates must lie within ±kMaxCoordPx logical pixels; a
 * Shape outside that, or with more edges than kMaxEdges, is refused.
 *
 * ## RowMask
 *
 * A RowMask is bbox-local: its origin (x, y) and size are the pixel bbox of the
 * Shape's closed rings, intersected with the clip. Each row is
 * wordsPerRow() uint32 words; pixel (x() + i, y() + r) is bit (i & 31) of
 * word i >> 5 of row r (LSB = leftmost), and bits past width() are zero. The
 * words live in caller storage, so filling allocates nothing.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "../core/types.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/remap.hpp"
#include "shape.hpp"

namespace enjin2 {
namespace shape {

/// A 1-bpp row-word mask over caller storage. See the file comment.
class RowMask {
public:
    static constexpr int kWordBits = 32;

    RowMask() = default;
    RowMask(uint32_t* words, size_t capacityWords) : words_(words), capacity_(capacityWords) {}

    /// Words a w × h mask needs.
    static constexpr size_t wordsFor(uint16_t w, uint16_t h)
    {
        return size_t((w + kWordBits - 1) / kWordBits) * h;
    }

    int16_t  x() const { return x_; }
    int16_t  y() const { return y_; }
    uint16_t width() const { return w_; }
    uint16_t height() const { return h_; }
    uint16_t wordsPerRow() const { return wpr_; }
    bool     empty() const { return w_ == 0 || h_ == 0; }
    size_t   capacityWords() const { return capacity_; }

    /// Row r (mask-local, 0 <= r < height()).
    const uint32_t* row(uint16_t r) const { return words_ + size_t(r) * wpr_; }
    uint32_t*       row(uint16_t r) { return words_ + size_t(r) * wpr_; }

    /// Is canvas pixel (cx, cy) set? False outside the mask.
    bool test(int cx, int cy) const
    {
        const int i = cx - x_, r = cy - y_;
        if (i < 0 || r < 0 || i >= w_ || r >= h_)
            return false;
        return (row(uint16_t(r))[i >> 5] >> (i & 31)) & 1u;
    }

    /// Set the bounds to (x, y, w, h); false (and empty) if it doesn't fit.
    bool reset(int16_t x, int16_t y, uint16_t w, uint16_t h)
    {
        x_ = x;
        y_ = y;
        if (wordsFor(w, h) > capacity_ || (w != 0 && h != 0 && words_ == nullptr))
        {
            w_ = h_ = wpr_ = 0;
            return false;
        }
        w_ = w;
        h_ = h;
        wpr_ = uint16_t((w + kWordBits - 1) / kWordBits);
        return true;
    }

    /// Make the mask empty (keeps the storage).
    void clear() { w_ = h_ = wpr_ = 0; }

private:
    uint32_t* words_    = nullptr;
    size_t    capacity_ = 0;
    int16_t   x_ = 0, y_ = 0;
    uint16_t  w_ = 0, h_ = 0, wpr_ = 0;
};

/**
 * @brief Fills Shapes into RowMasks. Holds the working memory (about 16 KB),
 *        so keep one and reuse it; it is not reentrant.
 */
class Rasterizer {
public:
    static constexpr uint16_t kMaxEdges     = 256;  ///< Non-horizontal edges per Shape.
    static constexpr uint16_t kMaxMaskWidth = 512;  ///< Widest mask (px) after the clip.
    static constexpr int32_t  kMaxCoordPx   = 4096; ///< |ring coordinate| bound (px).

    /**
     * @brief Fill `shape`'s closed rings into `out`, clipped to `clip`.
     *
     * `out` is re-bounded to the Shape's pixel bbox ∩ clip; an empty result
     * (nothing closed, nothing in the clip) is a success with an empty mask.
     * @return false, with `out` empty, if the Shape is refused (too many
     *         edges, coordinates out of range, mask wider than kMaxMaskWidth)
     *         or `out`'s storage is too small.
     */
    bool fill(const Shape& shape, const Rect& clip, RowMask& out);

    /**
     * @brief Stroke `shape`'s open rings into `out`, clipped to `clip`
     *        (Tomodachi #332): a pixel-perfect line `shape.paint.strokeWidth`
     *        px wide.
     *
     * Each point is the pixel it lies in; consecutive pixels are joined by
     * Bresenham and walked by PixelStroke, which drops the corner of every
     * "L". Each walked pixel stamps a w × w square whose top-left is
     * floor((w − 1) / 2) px up and left of it. Closed rings are not walked
     * (their stroke is innerStroke() of the fill). `out` is re-bounded to the
     * stamped bbox ∩ clip; nothing to stroke is a success with an empty mask.
     * @return false, with `out` empty, if refused (as fill()).
     */
    bool stroke(const Shape& shape, const Rect& clip, RowMask& out);

private:
    /// One non-horizontal edge, stepping exactly from sub-row to sub-row.
    struct Edge {
        int32_t syStart, syEnd;  ///< Active sub-rows [syStart, syEnd).
        int32_t c;               ///< First sub-sample column at/after the crossing.
        int32_t q;               ///< c step per sub-row (floor).
        int64_t r;               ///< Remainder: crossing numerator = c * den - r, 0 <= r < den.
        int64_t s;               ///< Remainder step, 0 <= s < den.
        int64_t den;
        int8_t  dir;             ///< +1 downward, -1 upward.
    };
    struct Crossing {
        int32_t c;
        int8_t  dir;
    };
    static constexpr uint16_t kRowWords = kMaxMaskWidth / 32;

    Edge     edges_[kMaxEdges];
    uint16_t active_[kMaxEdges];
    Crossing cross_[kMaxEdges];
    uint32_t full_[4][kRowWords];
    uint32_t touched_[kRowWords];
    uint8_t  counts_[kMaxMaskWidth];
};

/**
 * @brief The inner stroke of `fill`, `width` px wide (Tomodachi #332): the
 *        fill's pixels within `width` 4-neighbour steps of its outside,
 *        made as `width` rounds of word erosion. A pixel outside the mask
 *        (past its bounds) is outside. `out` gets `fill`'s bounds (empty for
 *        width 0).
 * @return false, with `out` empty, if `out`'s storage is too small or the
 *         mask is wider than Rasterizer::kMaxMaskWidth.
 */
bool innerStroke(const RowMask& fill, uint8_t width, RowMask& out);

/// Clip: `dst &= clip`, in canvas space (a pixel outside `clip` is cleared). `dst` keeps its bounds.
void clipMask(RowMask& dst, const RowMask& clip);

/// Union: `dst |= src`, in canvas space, within `dst`'s bounds.
void uniteMask(RowMask& dst, const RowMask& src);

/**
 * @brief Paint `mask` onto `canvas` in `role`'s colour (`look.apply(role)`),
 *        one span per run. ColourRole::None or Bayer level 0 paints nothing.
 *
 * Below kDitherSolid each run goes through Canvas4::drawHLineDither: a pixel
 * is painted where Mask::bayer(dither) is set at its canvas coordinates
 * (screen-anchored), the others keep what is under them.
 */
template <uint16_t W, uint16_t H>
void paintFill(Canvas4<W, H>& canvas, const RowMask& mask, ColourRole role, const Remap& look,
               uint8_t dither = kDitherSolid)
{
    if (role == ColourRole::None || mask.empty() || dither == 0)
        return;
    const Pixel4 colour(look.apply(static_cast<uint8_t>(role)));
    const int w = mask.width();
    for (uint16_t r = 0; r < mask.height(); ++r)
    {
        const uint32_t* row = mask.row(r);
        const int16_t   y = static_cast<int16_t>(mask.y() + r);
        int i = 0;
        while (i < w)
        {
            // Next set bit at or after i.
            int wi = i >> 5;
            uint32_t word = row[wi] & (~0u << (i & 31));
            while (word == 0 && ++wi < mask.wordsPerRow())
                word = row[wi];
            if (word == 0)
                break;
            const int start = (wi << 5) + __builtin_ctz(word);
            // Next clear bit after start (bits past width() are zero).
            wi = start >> 5;
            word = ~row[wi] & (~0u << (start & 31));
            while (word == 0 && ++wi < mask.wordsPerRow())
                word = ~row[wi];
            const int end = word == 0 ? w : (wi << 5) + __builtin_ctz(word);
            if (dither >= kDitherSolid)
                canvas.drawHLine(static_cast<int16_t>(mask.x() + start), y,
                                 static_cast<int16_t>(end - start), colour);
            else
                canvas.drawHLineDither(static_cast<int16_t>(mask.x() + start), y,
                                       static_cast<int16_t>(end - start), colour, dither);
            i = end;
        }
    }
}

} // namespace shape
} // namespace enjin2
