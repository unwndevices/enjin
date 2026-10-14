/**
 * @file shape_group.hpp
 * @brief A Shape group: Shapes drawn together on a retained layer of their own
 *        (ADR-0016, Tomodachi #331)
 *
 * The generic group knows Shapes, an origin and a colour Remap; it never sees
 * slots, sockets or poses (the pack layer, shape_pack_group.hpp, does). What it
 * draws comes from a ShapeItemSource, in paint order: each item is a Shape in
 * its own rest coordinates plus where it sits (`at`, relative to the group's
 * origin) and whether it is mirrored (x → −x about `at`), and its Binding.
 *
 * ## The Lattice
 *
 * The group owns one Lattice (lattice.hpp) in item coordinates; whoever lays
 * out the items configures its box. A `field` item has every point moved by
 * the field's displacement there; a `rigid` (or, until #335, `sway`) item is
 * moved as a whole by the displacement at its `at`. tick() advances it from
 * the frame's dt and marks the group changed when the field moved; a move of
 * the group (setOrigin) wakes it, as does impulse().
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
#include "lattice.hpp"
#include "shape.hpp"
#include "shape_raster.hpp"

namespace enjin2 {
namespace shape {

/**
 * @brief Working memory a group redraws with (about 30 KB): the one
 *        Rasterizer, its mask words and the warped points. Keep one per
 *        applet and share it between groups; it is not reentrant.
 */
struct ShapeDrawScratch {
    static constexpr uint16_t kMaxPoints = 512;  ///< Points per Shape (all its rings).
    static constexpr uint16_t kMaxRings  = 64;   ///< Rings per Shape.
    static constexpr size_t   kMaskWords = RowMask::wordsFor(256, 256);

    Rasterizer raster;
    uint32_t   mask[kMaskWords];
    FixedVec2  points[kMaxPoints];  ///< One item's warped points.
    Ring       rings[kMaxRings];    ///< One item's warped rings.
    Ring       restRings[kMaxRings];///< One item's rest rings (filled by the source).
};

/// Called once per item, in paint order. Returns false if the item was refused.
using ShapeEmit = bool (*)(void* ctx, const Shape& rest, FixedVec2 at, bool mirror, Binding binding);

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
            lattice_.wake();
        }
    }
    FixedVec2 origin() const { return origin_; }

    /// The Lattice the items ride (its box is in item coordinates).
    Lattice& lattice() { return lattice_; }
    const Lattice& lattice() const { return lattice_; }

    /// Advance the Lattice by a frame of `dtMicros`; true (and changed())
    /// when the field moved.
    bool tick(uint32_t dtMicros)
    {
        if (!lattice_.tick(dtMicros))
            return false;
        changed_ = true;
        return true;
    }

    /// Push the jelly at layer point `at` to `velocity` px/s (wakes it).
    void impulse(FixedVec2 at, FixedVec2 velocity) { lattice_.impulse(at - origin_, velocity); }

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

        static bool emit(void* p, const Shape& rest, FixedVec2 at, bool mirror, Binding binding)
        {
            auto& c = *static_cast<DrawCtx*>(p);
            ShapeDrawScratch& s = c.s;
            if (rest.ringCount > ShapeDrawScratch::kMaxRings)
                return c.ok = false;
            uint32_t total = 0;
            for (uint16_t r = 0; r < rest.ringCount; ++r)
                total += rest.rings[r].count;
            if (total > ShapeDrawScratch::kMaxPoints)
                return c.ok = false;

            // Field: each point moved by the field there; rigid: the whole
            // item by the field at its anchor.
            const Lattice& lat = c.g.lattice_;
            const bool field = binding == Binding::Field && lat.configured();
            const FixedVec2 base = c.g.origin_ + at + (field ? FixedVec2{} : lat.displacementAt(at));
            uint16_t n = 0;
            for (uint16_t r = 0; r < rest.ringCount; ++r)
            {
                const Ring& in = rest.rings[r];
                s.rings[r] = Ring{s.points + n, in.count, in.closed};
                for (uint16_t i = 0; i < in.count; ++i)
                {
                    const FixedVec2 p = in.points[i];
                    const FixedVec2 q = {mirror ? base.x - p.x : base.x + p.x, base.y + p.y};
                    s.points[n++] = field ? q + lat.displacementAt(q - c.g.origin_) : q;
                }
            }
            const Shape warped{s.rings, rest.ringCount, rest.paint};
            RowMask mask(s.mask, ShapeDrawScratch::kMaskWords);
            if (!s.raster.fill(warped, Rect(0, 0, W, H), mask))
                return c.ok = false;
            if (mask.empty() || rest.paint.fill == ColourRole::None)
                return true;
            paintFill(c.layer, mask, rest.paint.fill, c.g.remap_);
            // Footprint: the tiles under the mask's bbox.
            constexpr uint16_t TS = Canvas4<W, H>::TILE_SIZE, TX = Canvas4<W, H>::TILES_X;
            const uint16_t tx0 = uint16_t(mask.x() / TS), ty0 = uint16_t(mask.y() / TS);
            const uint16_t tx1 = uint16_t((mask.x() + mask.width() - 1) / TS);
            const uint16_t ty1 = uint16_t((mask.y() + mask.height() - 1) / TS);
            for (uint16_t ty = ty0; ty <= ty1; ++ty)
                for (uint16_t tx = tx0; tx <= tx1; ++tx)
                {
                    const uint32_t t = uint32_t(ty) * TX + tx;
                    c.g.footprint_[t >> 3] |= uint8_t(1u << (t & 7));
                    c.layer.invalidateTile(tx, ty);
                }
            return true;
        }
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
    Lattice   lattice_;
    Remap     remap_ = Remap::identity();
    bool      changed_ = true;
    uint8_t   footprint_[kMaxTiles / 8] = {};
};

} // namespace shape
} // namespace enjin2
