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
 * the field's displacement there; a `rigid` item is moved as a whole by the
 * displacement at its `at`; a `sway` item likewise, then turned about `at` by
 * the angle of the sway arm hung there (none hung: no turn). tick() advances
 * it from the frame's dt and marks the group changed when the field moved; a
 * move of the group (setOrigin) wakes it, as does impulse().
 *
 * ## The stepped drive (#335)
 *
 * The group also owns a SteppedDrive (stepped_drive.hpp): startDrive() runs a
 * periodic sequence of rest-pose transforms, applied to every rest position
 * before the Lattice displaces it (the Lattice is sampled at the rest point,
 * so its homes move with the step). tick() steps it too; a step change marks
 * the group changed without waking the Lattice.
 *
 * ## The retained layer
 *
 * The group's layer is its cache: it is drawn only when something changed. A
 * redraw clears the tiles of the last drawn footprint to transparent (15),
 * draws every item, and leaves the old ∪ new tiles dirty, so the compositor
 * recomposites exactly those. The footprint is the union of the tiles under
 * each drawn mask's bounding box.
 *
 * ## Grab, pull, drag and hit (#336)
 *
 * The group's origin is its Lattice's frame (lattice.hpp): setOrigin() snaps
 * it, moveTo() makes it follow a target through the whole-group position
 * spring so the jelly lags behind (a drag), and tick() moves the origin as
 * the carry goes. grabNode() / grabSway() / moveGrab() / releaseGrab() pull a
 * Lattice node or a sway arm, in layer pixels.
 *
 * Given a ShapeHitStore, each redraw keeps a copy of every mask it painted
 * (fill, inner stroke, open stroke) with the Shape's tag, in paint order.
 * hit() tests those **last drawn masks**, topmost first: pixel-exact, so it
 * follows the jelly, and it answers for what is on screen even when the
 * state has moved on since.
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
#include "stepped_drive.hpp"

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

/**
 * @brief Where a group keeps its last drawn masks for hit() (about 9 KB).
 *        One per group; the owner allocates it (a pet's comes from its
 *        AssetArena). Masks past its room are not kept: they never hit.
 */
struct ShapeHitStore {
    static constexpr uint8_t kMaxMasks = 48;
    static constexpr size_t  kWords = 2048;

    struct Mask {
        int16_t  x = 0, y = 0;
        uint16_t w = 0, h = 0, wpr = 0;
        uint16_t tag = 0;
        uint32_t offset = 0;  ///< Into words.
    };

    Mask     masks[kMaxMasks];
    uint8_t  count = 0;
    uint32_t used = 0;     ///< Words taken.
    bool     full = false; ///< A mask did not fit at the last redraw.
    uint32_t words[kWords];

    void clear()
    {
        count = 0;
        used = 0;
        full = false;
    }

    /// Keep a copy of `m` under `tag` (nothing for an empty mask).
    void keep(const RowMask& m, uint16_t tag)
    {
        if (m.empty())
            return;
        const uint32_t n = uint32_t(m.wordsPerRow()) * m.height();
        if (count == kMaxMasks || used + n > kWords)
        {
            full = true;
            return;
        }
        Mask& k = masks[count++];
        k = Mask{m.x(), m.y(), m.width(), m.height(), m.wordsPerRow(), tag, used};
        std::memcpy(words + used, m.row(0), size_t(n) * 4);
        used += n;
    }

    /// The tag of the topmost kept mask covering pixel (x, y), or -1.
    int32_t hit(int x, int y) const
    {
        for (int i = int(count) - 1; i >= 0; --i)
        {
            const Mask& k = masks[i];
            const int c = x - k.x, r = y - k.y;
            if (c < 0 || r < 0 || c >= k.w || r >= k.h)
                continue;
            if ((words[k.offset + uint32_t(r) * k.wpr + uint32_t(c >> 5)] >> (c & 31)) & 1u)
                return k.tag;
        }
        return -1;
    }
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

    /// Where the items' coordinate origin lands on the layer (a snap: it
    /// stops a moveTo()).
    void setOrigin(FixedVec2 o)
    {
        lattice_.setFrame(o);
        if (o != origin_)
        {
            origin_ = o;
            changed_ = true;
            lattice_.wake();
        }
    }
    FixedVec2 origin() const { return origin_; }

    /// Drag: the origin follows `o` through the position spring and the jelly
    /// lags behind (call it every frame while dragging). With no Lattice it
    /// snaps.
    void moveTo(FixedVec2 o)
    {
        if (!lattice_.configured())
            setOrigin(o);
        else
            lattice_.carryTo(o);
    }

    /// Pull (layer pixels): the Lattice node nearest `at`, or sway arm `arm`.
    /// @return a handle, or -1.
    int grabNode(FixedVec2 at) { return lattice_.grabNode(at); }
    int grabSway(uint8_t arm) { return lattice_.grabSway(arm); }
    void moveGrab(int handle, FixedVec2 target) { lattice_.moveGrab(handle, target); }
    void releaseGrab(int handle, FixedVec2 velocity) { lattice_.releaseGrab(handle, velocity); }

    /// Keep the last drawn masks in `store` for hit() (nullptr: none).
    void setHitStore(ShapeHitStore* store)
    {
        hits_ = store;
        if (hits_)
            hits_->clear();
    }
    /// The tag of the topmost Shape drawn at layer pixel (x, y) by the last
    /// redraw, or -1 (nothing there, or no hit store).
    int32_t hit(int x, int y) const { return hits_ ? hits_->hit(x, y) : -1; }

    /// The Lattice the items ride (its box is in item coordinates).
    Lattice& lattice() { return lattice_; }
    const Lattice& lattice() const { return lattice_; }

    /// Advance the Lattice and the stepped drive by a frame of `dtMicros`;
    /// true (and changed()) when the field moved or the drive stepped.
    bool tick(uint32_t dtMicros)
    {
        const bool field = lattice_.tick(dtMicros);
        const bool stepped = drive_.tick(dtMicros);
        if (lattice_.configured())
            origin_ = lattice_.frame();  // a drag carries it
        if (!field && !stepped)
            return false;
        changed_ = true;
        return true;
    }

    /// Run a stepped drive over the rest pose (see stepped_drive.hpp). It
    /// never wakes the Lattice; the group redraws when the drawn step changes.
    bool startDrive(const DriveStep* steps, uint8_t count, uint32_t periodMicros, FixedVec2 pivot, FixedVec2 extent)
    {
        const DriveStep before = drive_.current();
        const bool ok = drive_.start(steps, count, periodMicros, pivot, extent);
        if (drive_.current() != before)
            changed_ = true;
        return ok;
    }
    /// Stop the drive: back to the rest pose.
    void stopDrive()
    {
        if (!drive_.current().identity())
            changed_ = true;
        drive_.stop();
    }
    const SteppedDrive& drive() const { return drive_; }

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
        if (hits_)
            hits_->clear();
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
        if (hits_)
            hits_->clear();
        changed_ = true;
    }

private:
    template <uint16_t W, uint16_t H>
    struct DrawCtx {
        Canvas4<W, H>&    layer;
        ShapeDrawScratch& s;
        ShapeGroup&       g;
        bool              ok;
        const Shape*      live = nullptr;  ///< The group-space clip liveView holds (this redraw).
        uint16_t          liveCount = 0;
        RowMask           liveView{};      ///< Over ShapeDrawScratch::liveMask.

        /// Rest Shape → layer coordinates: placed at `at`, mirrored x → −x,
        /// then moved by the drive's step and the Lattice. `field`: each point
        /// by the field there; else the whole Shape by the field at `at`,
        /// turned about it by `angle` (a sway arm's; 0 for rigid). The drive
        /// moves rest positions first (the Lattice's homes with them), so the
        /// field is sampled at the rest point.
        bool warp(const Shape& rest, FixedVec2 at, bool mirror, bool field, Fixed angle, FixedVec2* points,
                  Ring* rings, Shape& out) const
        {
            const Lattice& lat = g.lattice_;
            const SteppedDrive& drive = g.drive_;
            if (rest.ringCount > ShapeDrawScratch::kMaxRings)
                return false;
            uint32_t total = 0;
            for (uint16_t r = 0; r < rest.ringCount; ++r)
                total += rest.rings[r].count;
            if (total > ShapeDrawScratch::kMaxPoints)
                return false;
            const FixedVec2 base = g.origin_ + (field ? FixedVec2{} : drive.apply(at) + lat.displacementAt(at));
            uint16_t n = 0;
            for (uint16_t r = 0; r < rest.ringCount; ++r)
            {
                const Ring& in = rest.rings[r];
                rings[r] = Ring{points + n, in.count, in.closed};
                for (uint16_t i = 0; i < in.count; ++i)
                {
                    const FixedVec2 p = {mirror ? -in.points[i].x : in.points[i].x, in.points[i].y};
                    if (field)
                    {
                        const FixedVec2 local = at + p;
                        points[n++] = base + drive.apply(local) + lat.displacementAt(local);
                    }
                    else
                        points[n++] = base + Lattice::turn(p, angle);
                }
            }
            out = rest;
            out.rings = rings;
            return true;
        }

        /// The union of `count` clip Shapes' fills, warped as warp() does, into `dst`.
        bool clipUnion(const Shape* clip, uint16_t count, FixedVec2 at, bool mirror, bool field, Fixed angle,
                       uint32_t* dst, RowMask& out)
        {
            out = RowMask(dst, ShapeDrawScratch::kMaskWords);
            Shape w;
            if (count == 1)
                return warp(clip[0], at, mirror, field, angle, s.clipPoints, s.clipRings, w) &&
                       s.raster.fill(w, Rect(0, 0, W, H), out);
            if (count > ShapeDrawScratch::kMaxClipShapes || !out.reset(0, 0, W, H))
                return false;
            for (uint16_t r = 0; r < H; ++r)
                std::memset(out.row(r), 0, size_t(out.wordsPerRow()) * 4);
            for (uint16_t k = 0; k < count; ++k)
            {
                RowMask one(s.strokeMask, ShapeDrawScratch::kMaskWords);
                if (!warp(clip[k], at, mirror, field, angle, s.clipPoints, s.clipRings, w) ||
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

        static bool emit(void* p, const Shape& rest, FixedVec2 at, bool mirror, Binding binding)
        {
            auto& c = *static_cast<DrawCtx*>(p);
            ShapeDrawScratch& s = c.s;
            const Paint& paint = rest.paint;
            const bool stroked = paint.strokeWidth > 0 && paint.stroke != ColourRole::None;
            if (paint.fill == ColourRole::None && !stroked)
                return true;
            // Field: each point moved by the field there; rigid: the whole
            // item by the field at its anchor; sway: that, turned about the
            // anchor by its arm (see warp()).
            const Lattice& lat = c.g.lattice_;
            const bool field = binding == Binding::Field && lat.configured();
            const Fixed angle = binding == Binding::Sway ? lat.swayAngleAt(at) : Fixed{};
            Shape warped;
            if (!c.warp(rest, at, mirror, field, angle, s.points, s.rings, warped))
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
                if (!c.clipUnion(rest.clip, rest.clipCount, at, mirror, field, angle, s.clipMask, own))
                    return c.ok = false;
                clip = &own;
            }
            else if (rest.clipSpace == ClipSpace::Group && rest.clipCount > 0)
            {
                if (c.live != rest.clip || c.liveCount != rest.clipCount)
                {
                    c.live = nullptr;
                    // The live Body is field-bound: clip to it as the field drew it.
                    if (!c.clipUnion(rest.clip, rest.clipCount, {}, false, lat.configured(), Fixed{}, s.liveMask,
                                     c.liveView))
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
            ShapeHitStore* hits = c.g.hits_;
            if (hits && paint.fill != ColourRole::None)
                hits->keep(fill, rest.tag);
            if (!stroked)
                return true;
            RowMask stroke(s.strokeMask, ShapeDrawScratch::kMaskWords);
            if (!innerStroke(fill, paint.strokeWidth, stroke))
                return c.ok = false;
            paintFill(c.layer, stroke, paint.stroke, c.g.remap_);
            if (hits && paint.fill == ColourRole::None)
                hits->keep(stroke, rest.tag);  // within the fill otherwise
            if (!s.raster.stroke(warped, canvasRect, stroke))
                return c.ok = false;
            if (clip)
                clipMask(stroke, *clip);
            paintFill(c.layer, stroke, paint.stroke, c.g.remap_);
            c.touch(stroke);
            if (hits)
                hits->keep(stroke, rest.tag);
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

    FixedVec2    origin_{};
    ShapeHitStore* hits_ = nullptr;
    Lattice      lattice_;
    SteppedDrive drive_;
    Remap     remap_ = Remap::identity();
    bool      changed_ = true;
    uint8_t   footprint_[kMaxTiles / 8] = {};
};

} // namespace shape
} // namespace enjin2
