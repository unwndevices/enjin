/**
 * @file shape.hpp
 * @brief Runtime Shape data: rings and paint (ADR-0016, Tomodachi #329)
 *
 * A **Shape** is a vector outline drawn as pixel art in the device palette
 * (glossary › The pet). At runtime it is a view over rings that are already
 * warped (by a Lattice, a sway rotation, or nothing) in Fixed logical canvas
 * pixels, plus its paint. The rasterizer (shape_raster.hpp) fills it into a
 * 1-bpp mask; the views own nothing, so a Shape group keeps the warped points
 * wherever it likes and rebuilds no Shape per frame.
 *
 * Paint (dither, inner and open-path stroke) and clip: Tomodachi #332.
 */
#pragma once

#include <cstdint>

#include "../core/fixed.hpp"

namespace enjin2 {
namespace shape {

/**
 * @brief What a colour means on the pet. A Look's Remap says which palette
 *        index each role is: `remap.apply(uint8_t(role))`.
 *
 * Same values as the Shape pack's ShapeRole (Body, Accent, Ink, None), so a
 * pack record converts with a cast.
 */
enum class ColourRole : uint8_t { Body = 0, Accent = 1, Ink = 2, None = 3 };

/// Which points are inside: winding != 0, or winding odd.
enum class FillRule : uint8_t { NonZero = 0, EvenOdd = 1 };

/**
 * @brief One ring of a Shape: `count` points, in Fixed canvas pixels.
 *
 * A closed ring is filled (its last point joins its first); an open ring is
 * only ever stroked (a line), so the fill skips it.
 */
struct Ring {
    const FixedVec2* points = nullptr;
    uint16_t         count  = 0;
    bool             closed = true;
};

/// Bayer level of a solid fill (Effect / Mask::bayer levels are 0..16).
static constexpr uint8_t kDitherSolid = 16;

/**
 * @brief How a Shape is painted (Tomodachi #332).
 *
 * The fill covers the closed rings in `fill`'s colour, through the
 * screen-anchored Bayer pattern of `dither` (a pixel is painted where
 * Mask::bayer(dither) is set at its canvas coordinates; 16 is solid), so what
 * is under a dithered fill shows through. The stroke is painted solid in
 * `stroke`'s colour over the fill: on closed rings the inner stroke
 * (innerStroke() of the fill mask), on open rings a pixel-perfect line,
 * `strokeWidth` px either way. A fill role of None still makes the fill mask
 * an inner stroke is cut from.
 */
struct Paint {
    ColourRole fill        = ColourRole::Body;
    FillRule   rule        = FillRule::NonZero;
    uint8_t    dither      = kDitherSolid;
    uint8_t    strokeWidth = 0;
    ColourRole stroke      = ColourRole::None;
};

/// Where a Shape's clip lives (Tomodachi #332).
enum class ClipSpace : uint8_t {
    None,   ///< Not clipped.
    Item,   ///< The clip Shapes are in the item's own coordinates and move with it.
    Group,  ///< The clip Shapes are in the group's coordinates: the live Body.
};

/// A Shape: its rings (all filled together under one fill rule), paint and clip.
struct Shape {
    const Ring* rings     = nullptr;
    uint16_t    ringCount = 0;
    Paint       paint;
    /// Fill and stroke are cut to the union of these Shapes' fills (a word AND).
    const Shape* clip      = nullptr;
    uint16_t     clipCount = 0;
    ClipSpace    clipSpace = ClipSpace::None;
    /// The owner's name for what this Shape belongs to; a Shape group's hit
    /// test answers with it (Tomodachi #336).
    uint16_t     tag = 0;
};

} // namespace shape
} // namespace enjin2
