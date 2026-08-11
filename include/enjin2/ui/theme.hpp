#pragma once

#include "reflect.hpp"
#include "../core/types.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief Visual theme for ui widgets on a 4-bit display
 *
 * A theme is a flat bundle of design tokens — a small palette plus a handful of
 * layout metrics — that widgets read instead of hard-coding colours and spacing.
 * It is a constexpr aggregate so a theme can live in ROM and be shared by value.
 *
 * Colours are @ref Pixel4 (4-bit grayscale, 0 = black … 15 = white); metrics are
 * in pixels.
 */
struct Theme {
    // ----- Palette -----
    Pixel4 background;  ///< Window / canvas background
    Pixel4 surface;     ///< Panel / list background (raised above the background)
    Pixel4 foreground;  ///< Primary text and icons
    Pixel4 muted;       ///< Secondary / disabled text
    Pixel4 accent;      ///< Selection and focus highlight fill
    Pixel4 accentText;  ///< Text/icons drawn on top of @ref accent

    // ----- Metrics (pixels) -----
    uint8_t padding;    ///< Inner padding of a container
    uint8_t spacing;    ///< Gap between adjacent items
    uint8_t itemHeight; ///< Height of a list row / single-line control
    uint8_t border;     ///< Outline / border thickness

    /**
     * @brief The default dark theme for a 4-bit display
     * @return A theme with a black background, white text and a bright highlight
     */
    static constexpr Theme dark() {
        return Theme{
            /* background */ Pixel4(0),
            /* surface    */ Pixel4(2),
            /* foreground */ Pixel4(15),
            /* muted      */ Pixel4(7),
            /* accent     */ Pixel4(15),
            /* accentText */ Pixel4(0),
            /* padding    */ 2,
            /* spacing    */ 1,
            /* itemHeight */ 10,
            /* border     */ 1,
        };
    }
};

/// @brief Serializable properties of @ref Theme (see reflect.hpp).
/// Not an ECS component — a scene file carries at most one theme at the root.
#define ENJIN2_THEME_FIELDS(FIELD, PROP) \
    FIELD(background)                    \
    FIELD(surface)                       \
    FIELD(foreground)                    \
    FIELD(muted)                         \
    FIELD(accent)                        \
    FIELD(accentText)                    \
    FIELD(padding)                       \
    FIELD(spacing)                       \
    FIELD(itemHeight)                    \
    FIELD(border)

ENJIN2_REFLECT_COMPONENT(Theme, 3, "theme", ENJIN2_THEME_FIELDS)

/// @brief Process-wide default theme (dark, 4-bit).
inline constexpr Theme kDefaultTheme = Theme::dark();

} // namespace enjin2
