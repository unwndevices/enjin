#pragma once
#ifndef ENJIN2_GRAPHICS_BORDER_HPP
#define ENJIN2_GRAPHICS_BORDER_HPP

#include "../core/types.hpp"
#include "canvas.hpp"
#include "palette.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace enjin2 {

/**
 * @file border.hpp
 * @brief Computed span-walker border stroke (#18 / Tomodachi #39).
 *
 * The selection frame and panel outline are drawn by a single span-walker
 * primitive that replaces the old corner-arc annulus (`Primitives::drawRoundRect`
 * now delegates here). For each row it computes the outer and inner rounded-rect
 * extents from a precomputed **arc table** and inks the ring as one or two
 * horizontal spans via the canvas' row-fill — O(perimeter), no per-pixel virtual
 * calls, and **no cache** (the launcher draws ~2 borders, so a cache would cost
 * more than it saves).
 *
 * Only three kinds survive the v1 vocabulary (#18): **solid**, **bevel**
 * (top/left `lighten`, bottom/right `darken` — a ramp two-tone, not per-side
 * flags), and **drop-shadow** (an offset filled rounded rect painted behind,
 * computed by the caller to track the IMU tilt). Dashed / dotted / double and
 * `border_side` are cut.
 */

/// @brief The closed set of border kinds for v1 (#18): dashed/dotted/double cut.
enum class BorderKind : uint8_t {
    Solid = 0,       ///< One colour on every side.
    Bevel = 1,       ///< Top/left lightened, bottom/right darkened (ramp two-tone).
    DropShadow = 2,  ///< Solid ring plus an offset filled rounded rect behind it.
};

/**
 * @brief The design tokens one border reads at draw time — the graphics-layer
 *        replacement for `Theme::border` (#18), resolved per-applet by style
 *        slots (a `Style` maps `border`/`borderWidth`/`radius`/`borderKind`/
 *        `shadowDx`/`shadowDy` onto this).
 *
 * Colours are ramp indices: the bevel tones and the shadow tone are *derived*
 * from `color` via `Palette::lighten`/`darken` at draw time, never stored, so
 * re-authoring the palette re-skins the border while its role holds.
 */
struct BorderStyle {
    Pixel4 color{Pixel4(15)};        ///< Base stroke colour (a ramp index).
    uint8_t thickness{1};            ///< Stroke thickness in pixels (>=1).
    uint8_t radius{0};               ///< Corner radius (clamped to half the min extent).
    BorderKind kind{BorderKind::Solid};
    int8_t shadowDx{0};              ///< Drop-shadow x offset (DropShadow only).
    int8_t shadowDy{0};              ///< Drop-shadow y offset (DropShadow only).
};

namespace detail {

/// @brief Fill the arc table `arc[j] = r - round(sqrt(r^2 - (r-j)^2))` for
/// j in [0, r]. `arc[j]` is the horizontal inset of the quarter-circle at
/// row-offset `j` from the corner's outer edge. `arc` must hold `r + 1` entries.
inline void buildArcTable(int r, int16_t* arc) {
    const double rr = static_cast<double>(r) * static_cast<double>(r);
    for (int j = 0; j <= r; ++j) {
        const double d = static_cast<double>(r - j);
        double inside = rr - d * d;
        if (inside < 0.0) inside = 0.0;
        arc[j] = static_cast<int16_t>(r - std::lround(std::sqrt(inside)));
    }
}

/// @brief Fill a rounded rectangle by walking rows and inking each row's outer
/// span — the drop-shadow's backing shape. Kept private to the border stroke so
/// it never competes with `Primitives::fillRoundRect` (whose Canvas8-exact
/// pixels are pinned by the parity bench).
template <typename TPixel>
void fillRoundedSpan(ICanvas<TPixel>& canvas, const Rect& rect, int r, TPixel color) {
    const int16_t x = rect.x;
    const int16_t y = rect.y;
    const int16_t w = static_cast<int16_t>(rect.width);
    const int16_t h = static_cast<int16_t>(rect.height);
    if (w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    int16_t arc[256];
    buildArcTable(r, arc);
    for (int16_t row = y; row < y + h; ++row) {
        int inset = 0;
        const int jt = row - y;
        const int jb = (y + h - 1) - row;
        if (jt < r) inset = arc[jt];
        else if (jb < r) inset = arc[jb];
        const int16_t left = static_cast<int16_t>(x + inset);
        const int16_t len = static_cast<int16_t>(w - 2 * inset);
        if (len > 0) {
            canvas.fill(Rect(left, row, static_cast<uint16_t>(len), 1), color);
        }
    }
}

}  // namespace detail

/**
 * @brief Stroke a (possibly rounded) rectangular border with the span walker.
 *
 * @param canvas Target canvas.
 * @param rect   Outer bounds of the border.
 * @param style  Resolved border tokens (colour, thickness, radius, kind, shadow).
 *
 * The ring is the region between the outer rounded rect (radius `r`) and an
 * inner rounded rect inset by `thickness` on every side (radius
 * `max(0, r - thickness)`). Each row inks a full span (top/bottom bands and
 * corners, where the hole is empty) or a left + right span (the vertical
 * edges). For a bevel, full top-half rows and every left span take the light
 * tone; full bottom-half rows and every right span take the dark tone.
 */
template <typename TPixel>
void strokeBorder(ICanvas<TPixel>& canvas, const Rect& rect, const BorderStyle& style) {
    const int16_t x = rect.x;
    const int16_t y = rect.y;
    const int16_t w = static_cast<int16_t>(rect.width);
    const int16_t h = static_cast<int16_t>(rect.height);
    if (w <= 0 || h <= 0) return;

    int t = style.thickness;
    if (t < 1) t = 1;
    int r = style.radius;
    if (r < 0) r = 0;
    const int maxR = std::min<int>(w, h) / 2;
    if (r > maxR) r = maxR;

    // Drop shadow: an offset filled rounded rect painted behind the ring, in the
    // one-step-darker tone. The caller offsets it to track the IMU tilt.
    if (style.kind == BorderKind::DropShadow &&
        (style.shadowDx != 0 || style.shadowDy != 0)) {
        const TPixel shadowTone = TPixel(Palette::darken(style.color.value));
        detail::fillRoundedSpan(
            canvas,
            Rect(static_cast<int16_t>(x + style.shadowDx),
                 static_cast<int16_t>(y + style.shadowDy), rect.width, rect.height),
            r, shadowTone);
    }

    const bool bevel = (style.kind == BorderKind::Bevel);
    const TPixel light =
        bevel ? TPixel(Palette::lighten(style.color.value)) : TPixel(style.color.value);
    const TPixel dark =
        bevel ? TPixel(Palette::darken(style.color.value)) : TPixel(style.color.value);

    int16_t arcOuter[256];
    int16_t arcInner[256];
    detail::buildArcTable(r, arcOuter);
    const int ri = std::max(0, r - t);
    detail::buildArcTable(ri, arcInner);

    const int16_t innerX = static_cast<int16_t>(x + t);
    const int16_t innerY = static_cast<int16_t>(y + t);
    const int16_t innerW = static_cast<int16_t>(w - 2 * t);
    const int16_t innerH = static_cast<int16_t>(h - 2 * t);
    const int16_t midY = static_cast<int16_t>(y + h / 2);

    auto hspan = [&canvas](int16_t sx, int16_t row, int16_t len, TPixel color) {
        if (len > 0) canvas.fill(Rect(sx, row, static_cast<uint16_t>(len), 1), color);
    };

    for (int16_t row = y; row < y + h; ++row) {
        int outerInset = 0;
        const int jt = row - y;
        const int jb = (y + h - 1) - row;
        if (jt < r) outerInset = arcOuter[jt];
        else if (jb < r) outerInset = arcOuter[jb];
        const int16_t outerL = static_cast<int16_t>(x + outerInset);
        const int16_t outerR = static_cast<int16_t>(x + w - 1 - outerInset);
        if (outerL > outerR) continue;

        bool hasInner = (innerW > 0 && innerH > 0 && row >= innerY &&
                         row <= innerY + innerH - 1);
        int16_t innerL = 0;
        int16_t innerR = -1;
        if (hasInner) {
            int innerInset = 0;
            const int it = row - innerY;
            const int ib = (innerY + innerH - 1) - row;
            if (it < ri) innerInset = arcInner[it];
            else if (ib < ri) innerInset = arcInner[ib];
            innerL = static_cast<int16_t>(innerX + innerInset);
            innerR = static_cast<int16_t>(innerX + innerW - 1 - innerInset);
            if (innerL > innerR) hasInner = false;
        }

        if (!hasInner) {
            hspan(outerL, row, static_cast<int16_t>(outerR - outerL + 1),
                  (row < midY) ? light : dark);
        } else {
            hspan(outerL, row, static_cast<int16_t>(innerL - outerL), light);
            hspan(static_cast<int16_t>(innerR + 1), row,
                  static_cast<int16_t>(outerR - innerR), dark);
        }
    }
}

}  // namespace enjin2

#endif  // ENJIN2_GRAPHICS_BORDER_HPP
