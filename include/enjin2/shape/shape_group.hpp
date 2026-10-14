/**
 * @file shape_group.hpp
 * @brief A Shape group: Shapes drawn together on a retained layer of their own
 *        (ADR-0016, Tomodachi #331)
 *
 * The generic group knows Shapes, an origin and a colour Remap; it never sees
 * slots, sockets or poses (the pack layer, shape_pack_group.hpp, does). What it
 * draws comes from a ShapeItemSource, in paint order: each item is a Shape in
 * its own rest coordinates plus where it sits (`at`, relative to the group's
 * origin) and whether it is mirrored (x → −x about `at`). This slice places
 * items rigidly; the Lattice (#334) bends them later.
 *
 * ## The retained layer
 *
 * The group's layer is its cache: it is drawn only when something changed. A
 * redraw clears the tiles of the last drawn footprint to transparent (15),
 * draws every item, and leaves the old ∪ new tiles dirty, so the compositor
 * recomposites exactly those. The footprint is the union of the tiles under
 * each drawn mask's bounding box.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../core/fixed.hpp"
#include "../graphics/canvas.hpp"
#include "../graphics/remap.hpp"
#include "shape.hpp"
#include "shape_raster.hpp"

namespace enjin2 {
namespace shape {

/**
 * @brief Working memory a group redraws with (about 60 KB): the one
 *        Rasterizer, its masks and the warped points. Keep one per applet and
 *        share it between groups; it is not reentrant.
 */
struct ShapeDrawScratch {
    static constexpr uint16_t kMaxPoints     = 512;  ///< Points per Shape (all its rings).
    static constexpr uint16_t kMaxRings      = 64;   ///< Rings per Shape.
    static constexpr uint16_t kMaxClipShapes = 8;    ///< Shapes one clip is the union of.
    static constexpr size_t   kMaskWords = RowMask::wordsFor(256, 256);

    Rasterizer raster;
    uint32_t   mask[kMaskWords];        ///< One item's fill.
    uint32_t   strokeMask[kMaskWords];  ///< One item's stroke (and union scratch).
    uint32_t   clipMask[kMaskWords];    ///< One item's own-space clip.
    uint32_t   liveMask[kMaskWords];    ///< The group-space (live Body) clip, once per redraw.
    FixedVec2  points[kMaxPoints];      ///< One item's warped points.
    Ring       rings[kMaxRings];        ///< One item's warped rings.
    FixedVec2  clipPoints[kMaxPoints];  ///< One clip Shape's warped points.
    Ring       clipRings[kMaxRings];    ///< One clip Shape's warped rings.

    // Filled by the source (ShapeItemSource::forEach):
    Ring  restRings[kMaxRings];      ///< One item's rest rings.
    Ring  clipRestRings[kMaxRings];  ///< One item's own clip Shape's rest rings.
    Shape clipRest;                  ///< That clip Shape.
    Ring  liveRestRings[kMaxRings];  ///< The live clip Shapes' rest rings (all of them).
    Shape liveRest[kMaxClipShapes];  ///< The live clip Shapes (the Body).
};

/// Called once per item, in paint order. Returns false if the item was refused.
using ShapeEmit = bool (*)(void* ctx, const Shape& rest, FixedVec2 at, bool mirror);

/// What a group draws. forEach() emits every item; false if any was refused.
class ShapeItemSource {
public:
    virtual ~ShapeItemSource() = default;
    virtual bool forEach(ShapeDrawScratch& scratch, ShapeEmit emit, void* ctx) const = 0;
};

class ShapeGroup {
public:
    static constexpr uint16_t kMaxTiles = 256;  ///< Footprint bitmap size (a 256×256 canvas).

    /// Where the items' coordinate origin lands on the layer.
    void setOrigin(FixedVec2 o)
    {
        if (o != origin_)
        {
            origin_ = o;
            changed_ = true;
        }
    }
    FixedVec2 origin() const { return origin_; }

    /// Role → palette index (ColourRole values index the LUT).
    void setRemap(const Remap& r)
    {
        if (std::memcmp(r.lut, remap_.lut, sizeof r.lut) != 0)
        {
            remap_ = r;
            changed_ = true;
        }
    }
    const Remap& remap() const { return remap_; }

    /// Something the source draws changed: redraw on the next flush.
    void markChanged() { changed_ = true; }
    bool changed() const { return changed_; }

    /// Is tile (tx, ty) in the last drawn footprint?
    bool inFootprint(uint16_t tx, uint16_t ty, uint16_t tilesX) const
    {
        const uint32_t t = uint32_t(ty) * tilesX + tx;
        return t < kMaxTiles && ((footprint_[t >> 3] >> (t & 7)) & 1u);
    }

    /**
     * @brief Clear the old footprint to transparent, draw every item, mark the
     *        old ∪ new tiles dirty. Clears changed().
     * @return false if the source refused an item (the rest are drawn).
     */
    template <uint16_t W, uint16_t H>
    bool redraw(Canvas4<W, H>& layer, ShapeDrawScratch& scratch, const ShapeItemSource& source)
    {
        static_assert(Canvas4<W, H>::TILE_COUNT <= kMaxTiles, "canvas too large for the footprint bitmap");
        static_assert(size_t(((W + 31) / 32)) * H <= ShapeDrawScratch::kMaskWords, "canvas too large for the mask");
        eraseFootprint(layer);
        DrawCtx<W, H> ctx{layer, scratch, *this, true};
        const bool sourced = source.forEach(scratch, &DrawCtx<W, H>::emit, &ctx);
        changed_ = false;
        return sourced && ctx.ok;
    }

    /// Clear the drawn footprint to transparent (marking it dirty) and forget it.
    template <uint16_t W, uint16_t H>
    void erase(Canvas4<W, H>& layer)
    {
        eraseFootprint(layer);
        changed_ = true;
    }

private:
    template <uint16_t W, uint16_t H>
    struct DrawCtx {
        Canvas4<W, H>&    layer;
        ShapeDrawScratch& s;
        ShapeGroup&       g;
        bool              ok;
        const Shape*      live = nullptr;  ///< The group-space clip liveMask holds (this redraw).
        uint16_t          liveCount = 0;

        /// Rest Shape → layer coordinates: placed at origin + at, mirrored x → −x.
        static bool warp(const Shape& rest, FixedVec2 base, bool mirror, FixedVec2* points, Ring* rings, Shape& out)
        {
            if (rest.ringCount > ShapeDrawScratch::kMaxRings)
                return false;
            uint32_t total = 0;
            for (uint16_t r = 0; r < rest.ringCount; ++r)
                total += rest.rings[r].count;
            if (total > ShapeDrawScratch::kMaxPoints)
                return false;
            uint16_t n = 0;
            for (uint16_t r = 0; r < rest.ringCount; ++r)
            {
                const Ring& in = rest.rings[r];
                rings[r] = Ring{points + n, in.count, in.closed};
                for (uint16_t i = 0; i < in.count; ++i)
                {
                    const FixedVec2 p = in.points[i];
                    points[n++] = {mirror ? base.x - p.x : base.x + p.x, base.y + p.y};
                }
            }
            out = rest;
            out.rings = rings;
            return true;
        }

        /// The union of `count` clip Shapes' fills, warped by (base, mirror), into `dst`.
        bool clipUnion(const Shape* clip, uint16_t count, FixedVec2 base, bool mirror, uint32_t* dst, RowMask& out)
        {
            out = RowMask(dst, ShapeDrawScratch::kMaskWords);
            Shape w;
            if (count == 1)
                return warp(clip[0], base, mirror, s.clipPoints, s.clipRings, w) &&
                       s.raster.fill(w, Rect(0, 0, W, H), out);
            if (count > ShapeDrawScratch::kMaxClipShapes || !out.reset(0, 0, W, H))
                return false;
            for (uint16_t r = 0; r < H; ++r)
                std::memset(out.row(r), 0, size_t(out.wordsPerRow()) * 4);
            for (uint16_t k = 0; k < count; ++k)
            {
                RowMask one(s.strokeMask, ShapeDrawScratch::kMaskWords);
                if (!warp(clip[k], base, mirror, s.clipPoints, s.clipRings, w) ||
                    !s.raster.fill(w, Rect(0, 0, W, H), one))
                    return false;
                uniteMask(out, one);
            }
            return true;
        }

        /// Mark the tiles under `m`'s bbox dirty and in the footprint.
        void touch(const RowMask& m)
        {
            if (m.empty())
                return;
            constexpr uint16_t TS = Canvas4<W, H>::TILE_SIZE, TX = Canvas4<W, H>::TILES_X;
            const uint16_t tx0 = uint16_t(m.x() / TS), ty0 = uint16_t(m.y() / TS);
            const uint16_t tx1 = uint16_t((m.x() + m.width() - 1) / TS);
            const uint16_t ty1 = uint16_t((m.y() + m.height() - 1) / TS);
            for (uint16_t ty = ty0; ty <= ty1; ++ty)
                for (uint16_t tx = tx0; tx <= tx1; ++tx)
                {
                    const uint32_t t = uint32_t(ty) * TX + tx;
                    g.footprint_[t >> 3] |= uint8_t(1u << (t & 7));
                    layer.invalidateTile(tx, ty);
                }
        }

        static bool emit(void* p, const Shape& rest, FixedVec2 at, bool mirror)
        {
            auto& c = *static_cast<DrawCtx*>(p);
            ShapeDrawScratch& s = c.s;
            const Paint& paint = rest.paint;
            const bool stroked = paint.strokeWidth > 0 && paint.stroke != ColourRole::None;
            if (paint.fill == ColourRole::None && !stroked)
                return true;
            const FixedVec2 base = c.g.origin_ + at;
            Shape warped;
            if (!warp(rest, base, mirror, s.points, s.rings, warped))
                return c.ok = false;
            const Rect canvasRect(0, 0, W, H);
            RowMask fill(s.mask, ShapeDrawScratch::kMaskWords);
            if (!s.raster.fill(warped, canvasRect, fill))
                return c.ok = false;

            // The clip: the item's own (warped with it) or the group's live one.
            const RowMask* clip = nullptr;
            RowMask own;
            if (rest.clipSpace == ClipSpace::Item && rest.clipCount > 0)
            {
                if (!c.clipUnion(rest.clip, rest.clipCount, base, mirror, s.clipMask, own))
                    return c.ok = false;
                clip = &own;
            }
            else if (rest.clipSpace == ClipSpace::Group && rest.clipCount > 0)
            {
                if (c.live != rest.clip || c.liveCount != rest.clipCount)
                {
                    c.live = nullptr;
                    if (!c.clipUnion(rest.clip, rest.clipCount, c.g.origin_, false, s.liveMask, c.liveView))
                        return c.ok = false;
                    c.live = rest.clip;
                    c.liveCount = rest.clipCount;
                }
                clip = &c.liveView;
            }
            if (clip)
                clipMask(fill, *clip);

            paintFill(c.layer, fill, paint.fill, c.g.remap_, paint.dither);
            c.touch(fill);
            if (!stroked)
                return true;
            RowMask stroke(s.strokeMask, ShapeDrawScratch::kMaskWords);
            if (!innerStroke(fill, paint.strokeWidth, stroke))
                return c.ok = false;
            paintFill(c.layer, stroke, paint.stroke, c.g.remap_);
            if (!s.raster.stroke(warped, canvasRect, stroke))
                return c.ok = false;
            if (clip)
                clipMask(stroke, *clip);
            paintFill(c.layer, stroke, paint.stroke, c.g.remap_);
            c.touch(stroke);
            return true;
        }

        RowMask liveView;
    };

    template <uint16_t W, uint16_t H>
    void eraseFootprint(Canvas4<W, H>& layer)
    {
        constexpr uint16_t TX = Canvas4<W, H>::TILES_X, TY = Canvas4<W, H>::TILES_Y;
        for (uint16_t ty = 0; ty < TY; ++ty)
            for (uint16_t tx = 0; tx < TX; ++tx)
                if (inFootprint(tx, ty, TX))
                {
                    layer.fillTileNoMark(tx, ty, Pixel4(15));
                    layer.invalidateTile(tx, ty);
                }
        std::memset(footprint_, 0, sizeof footprint_);
    }

    FixedVec2 origin_{};
    Remap     remap_ = Remap::identity();
    bool      changed_ = true;
    uint8_t   footprint_[kMaxTiles / 8] = {};
};

} // namespace shape
} // namespace enjin2
