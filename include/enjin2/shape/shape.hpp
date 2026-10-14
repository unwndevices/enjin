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
 * This slice carries the fill only: its colour role and fill rule. Dither,
 * stroke and clip land with Tomodachi #332.
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
 * only ever stroked (Tomodachi #332), so the fill skips it.
 */
struct Ring {
    const FixedVec2* points = nullptr;
    uint16_t         count  = 0;
    bool             closed = true;
};

/// How a Shape is painted (this slice: the fill).
struct Paint {
    ColourRole fill = ColourRole::Body;
    FillRule   rule = FillRule::NonZero;
};

/// A Shape: its rings (all filled together under one fill rule) and paint.
struct Shape {
    const Ring* rings     = nullptr;
    uint16_t    ringCount = 0;
    Paint       paint;
};

} // namespace shape
} // namespace enjin2
