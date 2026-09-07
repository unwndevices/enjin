#pragma once

#include "../core/types.hpp"
#include "../graphics/palette.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief A single visual style — the design tokens one drawable reads at draw
 *        time (colours as ramp indices, plus layout metrics).
 *
 * A `Style` is the per-slot half of the two-level theming model (see @ref
 * StyleSlot). It is a flat constexpr aggregate so the ROM defaults
 * (@ref kDefaultStyles) live in flash and an applet override is a plain copy.
 *
 * Colours are **role indices into the ramp palette**, never hexes: `fill` is a
 * ramp's base shade (`Palette::ramp(role, SHADE_BASE)`), so re-authoring the
 * palette re-skins every style while the roles hold (#12). The bevel/shadow
 * tones are **derived, never stored** — `light()`/`dark()`/`shadow()` shade-step
 * `fill` within its ramp via the #14 `Palette::lighten`/`darken` (clamped ±1),
 * so a tone can never drift out of sync with `fill`.
 */
struct Style {
    // ----- Colours (ramp role indices) -----
    Pixel4 fill;    ///< Background / body fill — a ramp base shade
    Pixel4 text;    ///< Text and icon colour
    Pixel4 border;  ///< Border / outline colour

    // ----- Metrics (pixels / enum) -----
    uint8_t borderWidth;  ///< Outline thickness
    uint8_t radius;       ///< Corner radius
    uint8_t padding;      ///< Inner padding
    uint8_t borderKind;   ///< Border style (0 = line; bevel/shadow variants per #18)
    int8_t  shadowDx;     ///< Drop-shadow x offset (0 = none)
    int8_t  shadowDy;     ///< Drop-shadow y offset (0 = none)
    uint8_t textOutline;  ///< Legibility outline for text drawn through this slot
                          ///< (1 = a dark +/-1 px silhouette; #40). On for the
                          ///< banner so it reads over any scenery, off for panel
                          ///< text. The tone is derived (darken of the text
                          ///< colour), never stored — the type system renders it.

    /// @brief The lighter tone of `fill`, one shade up its ramp (clamped).
    constexpr Pixel4 light() const { return Pixel4(Palette::lighten(fill.value)); }
    /// @brief The darker tone of `fill`, one shade down its ramp (clamped).
    constexpr Pixel4 dark() const { return Pixel4(Palette::darken(fill.value)); }
    /// @brief The drop-shadow tone — `darken(fill)` (the design's shadow == dark).
    constexpr Pixel4 shadow() const { return Pixel4(Palette::darken(fill.value)); }
};

/**
 * @brief The closed set of style slots for v1 (#19).
 *
 * State variants are **distinct slots**, not a (slot × state) matrix — so
 * resolution is a flat id lookup with no state machine. The enum is closed: a
 * fixed override array needs a fixed `Count`; applets override slot *values*,
 * never add slots.
 */
enum class StyleSlot : uint8_t {
    Panel = 0,
    PanelSelected,
    Popup,
    Banner,
    SceneObject,
    SceneObjectSelected,
    Count  ///< Sentinel — number of slots (never a real slot)
};

/// @brief Number of style slots (array size for the override store).
constexpr int kStyleSlotCount = static_cast<int>(StyleSlot::Count);
static_assert(kStyleSlotCount <= 16,
              "the override mask (LuaBindings::m_styleSetMask) is a uint16_t — "
              "at most 16 slots fit");

namespace detail {
/// Base shade of a role's ramp — the conventional `fill` for that role (#12).
constexpr Pixel4 roleBase(uint8_t role) { return Pixel4(Palette::ramp(role, SHADE_BASE)); }
constexpr Pixel4 roleLight(uint8_t role) { return Pixel4(Palette::ramp(role, SHADE_LIGHT)); }
constexpr Pixel4 roleDark(uint8_t role) { return Pixel4(Palette::ramp(role, SHADE_DARK)); }
}  // namespace detail

/**
 * @brief The ROM theme defaults, one `Style` per slot (placeholder palette).
 *
 * Roles follow #12: 0 chrome, 1 hull, 2 floor, 3 scenery+interactive, 4
 * pet+selection accent. These are the placeholder structure — the authored
 * pixel-art palette re-skins them without touching this table (role indices,
 * not hexes). Selected variants take the selection-accent ramp (role 4).
 */
constexpr Style kDefaultStyles[kStyleSlotCount] = {
    // Columns: { fill, text, border, borderWidth, radius, padding, borderKind, shadowDx, shadowDy, textOutline }
    // Panel — chrome body, light chrome text, dark chrome border
    /* Panel               */ { detail::roleBase(0), detail::roleLight(0), detail::roleDark(0), 1, 0, 2, 0, 0, 0, 0 },
    /* PanelSelected       */ { detail::roleBase(4), detail::roleLight(4), detail::roleDark(4), 1, 0, 2, 0, 0, 0, 0 },
    /* Popup               */ { detail::roleBase(0), detail::roleLight(0), detail::roleDark(0), 1, 0, 3, 0, 1, 1, 0 },
    // Banner — the naming label: outline on by default so it reads over scenery (#40).
    /* Banner              */ { detail::roleBase(0), detail::roleLight(0), detail::roleDark(0), 1, 0, 2, 0, 0, 0, 1 },
    /* SceneObject         */ { detail::roleBase(3), detail::roleLight(3), detail::roleDark(3), 1, 0, 1, 0, 0, 0, 0 },
    /* SceneObjectSelected */ { detail::roleBase(4), detail::roleLight(4), detail::roleDark(4), 1, 0, 1, 0, 0, 0, 0 },
};

/**
 * @brief Resolve a slot name to its enum, via strcmp cascade (no allocation).
 *
 * The repo has no `luaL_checkoption`; slot names are matched with the same
 * `{name → enum}` cascade the font registry uses. Names are the lower-camel
 * slot ids: "panel", "panelSelected", "popup", "banner", "sceneObject",
 * "sceneObjectSelected".
 *
 * @param name Slot name (null tolerated → false).
 * @param out  Set to the matched slot on success.
 * @return true if `name` matched a slot, false otherwise.
 */
bool styleSlotFromName(const char* name, StyleSlot& out);

}  // namespace enjin2
