#pragma once

#include "../core/types.hpp"
#include "../graphics/palette.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief A single visual style — the design tokens one drawable reads at draw
 *        time (colours as Palette roles, plus layout metrics).
 *
 * A `Style` is the per-slot half of the two-level theming model (see
 * `StyleSlot`). It is a flat constexpr aggregate so the ROM defaults
 * (`kDefaultStyles`) live in flash and an applet override is a plain copy.
 *
 * Colours are **Palette roles** (ADR-0019), never hexes, so switching the
 * palette re-skins every style while the jobs hold. The bevel/shadow tones are
 * **derived, never stored** — `towardText()`/`towardBg()`/`shadow()` step
 * `fill` one place along the shade chain (`Palette::towardText`/`towardBg`),
 * so a tone can never drift out of sync with `fill`.
 */
struct Style {
    // ----- Colours (Palette roles) -----
    Pixel4 fill;    ///< Background / body fill
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
                          ///< (1 = a bg +/-1 px silhouette; #40). On for the
                          ///< banner so it reads over any scenery, off for panel
                          ///< text. The tone is the bg role, never stored — the
                          ///< type system renders it.

    /// @brief `fill` one shade-chain step toward `text` (the bevel's top/left).
    constexpr Pixel4 towardText() const { return Pixel4(Palette::towardText(fill.value)); }
    /// @brief `fill` one shade-chain step toward `bg` (the bevel's bottom/right).
    constexpr Pixel4 towardBg() const { return Pixel4(Palette::towardBg(fill.value)); }
    /// @brief The drop-shadow tone — `towardBg(fill)`.
    constexpr Pixel4 shadow() const { return Pixel4(Palette::towardBg(fill.value)); }
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

/**
 * @brief The ROM theme defaults, one `Style` per slot, picked by job
 *        (ADR-0019).
 *
 * Panels and the banner sit on `surface` with `text` content and a `muted`
 * border; a scene object's label sits on `muted`. Selected variants take the
 * `accent` fill, and content on `accent` is `bg` (the role contract). Roles,
 * not hexes, so a palette switch re-skins them without touching this table.
 */
constexpr Style kDefaultStyles[kStyleSlotCount] = {
    // Columns: { fill, text, border, borderWidth, radius, padding, borderKind, shadowDx, shadowDy, textOutline }
    /* Panel               */ { Pixel4(ROLE_SURFACE), Pixel4(ROLE_TEXT), Pixel4(ROLE_MUTED), 1, 0, 2, 0, 0, 0, 0 },
    /* PanelSelected       */ { Pixel4(ROLE_ACCENT), Pixel4(ROLE_BG), Pixel4(ROLE_MUTED), 1, 0, 2, 0, 0, 0, 0 },
    /* Popup               */ { Pixel4(ROLE_SURFACE), Pixel4(ROLE_TEXT), Pixel4(ROLE_MUTED), 1, 0, 3, 0, 1, 1, 0 },
    // Banner — the naming label: outline on by default so it reads over scenery (#40).
    /* Banner              */ { Pixel4(ROLE_SURFACE), Pixel4(ROLE_TEXT), Pixel4(ROLE_MUTED), 1, 0, 2, 0, 0, 0, 1 },
    /* SceneObject         */ { Pixel4(ROLE_MUTED), Pixel4(ROLE_TEXT), Pixel4(ROLE_SURFACE), 1, 0, 1, 0, 0, 0, 0 },
    /* SceneObjectSelected */ { Pixel4(ROLE_ACCENT), Pixel4(ROLE_BG), Pixel4(ROLE_MUTED), 1, 0, 1, 0, 0, 0, 0 },
};

/**
 * @brief Slot names, indexed by StyleSlot: the lower-camel ids Lua passes.
 *
 * styleSlotFromName() matches against this array, and the engine.ui
 * descriptors publish it as the StyleSlot enum.
 */
inline constexpr const char* kStyleSlotNames[kStyleSlotCount] = {
    "panel", "panelSelected", "popup", "banner", "sceneObject", "sceneObjectSelected",
};

/**
 * @brief Resolve a slot name to its enum by matching kStyleSlotNames (no allocation).
 *
 * @param name Slot name (null tolerated → false).
 * @param out  Set to the matched slot on success.
 * @return true if `name` matched a slot, false otherwise.
 */
bool styleSlotFromName(const char* name, StyleSlot& out);

}  // namespace enjin2
