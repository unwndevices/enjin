#pragma once

#include <cstdint>

namespace enjin2 {

/**
 * @brief 24-bit RGB color value
 *
 * Stores three 8-bit channels for red, green, and blue. No alpha component —
 * transparency is managed at the palette index level (index 15 = transparent).
 */
struct RGB {
    uint8_t r; ///< Red channel (0-255)
    uint8_t g; ///< Green channel (0-255)
    uint8_t b; ///< Blue channel (0-255)

    /** @brief Default constructor initializes to black (0, 0, 0) */
    constexpr RGB() : r(0), g(0), b(0) {}

    /**
     * @brief Constructor with explicit channel values
     * @param r_ Red channel value (0-255)
     * @param g_ Green channel value (0-255)
     * @param b_ Blue channel value (0-255)
     */
    constexpr RGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}

    /**
     * @brief Equality comparison
     * @param other RGB value to compare against
     * @return true if all channels match
     */
    constexpr bool operator==(const RGB& other) const {
        return r == other.r && g == other.g && b == other.b;
    }

    /**
     * @brief Inequality comparison
     * @param other RGB value to compare against
     * @return true if any channel differs
     */
    constexpr bool operator!=(const RGB& other) const {
        return !(*this == other);
    }
};

/** @brief Index value that always represents transparency — never resolves to a color */
constexpr uint8_t PALETTE_TRANSPARENT = 15;

/** @brief Maximum number of color entries in the palette (indices 0-14) */
constexpr uint8_t PALETTE_MAX_ENTRIES = 15;

/**
 * @name Palette roles (ADR-0019)
 * @brief The palette is 8 colours named by their job, plus transparent (15).
 *
 * Art and code draw in roles, never in hues: a palette preset fills the roles,
 * so switching it recolours every screen. The role contract: content (text,
 * icons, glyphs) on `bg`/`surface`/`muted` is `text`; content goes only on
 * `accent` and `accent4` fills and is `bg` there; `accent2`/`accent3` are
 * art and game hues that never carry content. Nothing computes or checks
 * contrast. Indices 8-14 are temporary: they keep the old green ramp so art
 * not yet re-indexed by job still draws (removed when the contract is sealed).
 * @{
 */
constexpr uint8_t ROLE_BG = 0;       ///< Canvas background, the empty screen
constexpr uint8_t ROLE_SURFACE = 1;  ///< Panels, list rows, the Menu panel, cards
constexpr uint8_t ROLE_MUTED = 2;    ///< Secondary and disabled text, borders, dividers, shadows
constexpr uint8_t ROLE_TEXT = 3;     ///< Main text and icons
constexpr uint8_t ROLE_ACCENT = 4;   ///< Selection, focus, the main action
constexpr uint8_t ROLE_ACCENT2 = 5;  ///< Second hue for art and games
constexpr uint8_t ROLE_ACCENT3 = 6;  ///< Third hue for art and games
constexpr uint8_t ROLE_ACCENT4 = 7;  ///< Alerts by convention: low battery, recording, errors
constexpr uint8_t ROLE_COUNT = 8;    ///< Number of Palette roles (indices 0-7)
/** @} */

/**
 * @brief Runtime color palette for Canvas4 pixel-to-RGB mapping
 *
 * Maps 4-bit pixel indices (0-14) to 24-bit RGB colors at display time.
 * Index 15 is always transparent — callers should check `isTransparent()` before
 * calling `resolve()`. Supports named presets and index wrapping for smaller palettes.
 *
 * The transparency check always happens BEFORE modulo wrapping, so index 15 is
 * transparent regardless of palette size (e.g., a 4-color palette does not map
 * index 15 to index 3 via 15 % 4).
 *
 * Usage:
 * @code
 * uint8_t idx = canvas.getPixel(x, y);
 * if (!g_palette.isTransparent(idx)) {
 *     RGB color = g_palette.resolve(idx);
 *     display.drawPixel(x, y, color.r, color.g, color.b);
 * }
 * @endcode
 */
struct Palette {
    RGB colors[PALETTE_MAX_ENTRIES]; ///< Color entries for indices 0-14
    uint8_t size;                    ///< Active palette size — used for index wrapping
    bool debugTransparent;           ///< When true, renders transparent pixels as bright magenta

    /** @brief Default constructor initializes to the house default palette */
    Palette();

    /**
     * @brief Set a color entry by index
     *
     * Index 15 (PALETTE_TRANSPARENT) is silently ignored. For indices
     * 0-14 that exceed the current `size`, wrapping is applied via `index % size`.
     *
     * @param index Palette index (0-14, index 15 is transparent and ignored)
     * @param r Red channel (0-255)
     * @param g Green channel (0-255)
     * @param b Blue channel (0-255)
     */
    void setColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Get a color entry by index
     *
     * Returns `RGB{0,0,0}` for the transparent index (15). For indices that
     * exceed the current `size`, wrapping is applied via `index % size`.
     *
     * @param index Palette index (0-15)
     * @return RGB color for the given index, or black if transparent
     */
    RGB getColor(uint8_t index) const;

    /**
     * @brief Resolve a palette index to an RGB color
     *
     * Callers MUST check `isTransparent(index)` before calling this method.
     * For in-range indices, wrapping is applied via `index % size`.
     *
     * @param index Palette index (0-14; behavior undefined for index 15)
     * @return Resolved RGB color
     */
    RGB resolve(uint8_t index) const;

    /**
     * @brief Test whether a palette index represents transparency
     *
     * Must be checked BEFORE calling `resolve()`. Transparency is checked
     * before modulo wrapping — index 15 is always transparent.
     *
     * @param index Palette index to test
     * @return true if index is PALETTE_TRANSPARENT (15), false otherwise
     */
    bool isTransparent(uint8_t index) const;

    /**
     * @brief Replace the active palette with a named preset
     *
     * The only built-in preset is "default", the house default. The palette
     * `size` is updated to match the preset's entry count.
     *
     * @param name Null-terminated preset name string
     * @return true if preset was found and loaded, false if name is unknown
     */
    bool loadPreset(const char* name);

    /**
     * @brief Get the current active palette size
     * @return Number of active color entries (used for index wrapping)
     */
    uint8_t getSize() const;

    /**
     * @brief One step along the shade chain toward `bg`.
     *
     * The chain is `bg <-> surface <-> muted <-> text`; `bg` stays put. Any
     * other index (an accent, or a temporary entry 8-14) steps into `muted`.
     * Index 15 (transparent) is returned unchanged. Three steps reach `bg`
     * from anywhere.
     *
     * @param i Source palette index.
     * @return The index one step toward `bg`.
     */
    static constexpr uint8_t towardBg(uint8_t i) {
        if (i == PALETTE_TRANSPARENT) return PALETTE_TRANSPARENT;
        if (i == ROLE_BG) return ROLE_BG;
        if (i == ROLE_SURFACE) return ROLE_BG;
        if (i == ROLE_MUTED) return ROLE_SURFACE;
        return ROLE_MUTED;  // text, the accents, temporary 8-14
    }

    /**
     * @brief One step along the shade chain toward `text`.
     *
     * The mirror of @ref towardBg: `bg -> surface -> muted -> text`; `text`
     * stays put. Any other index (an accent, or a temporary entry 8-14) steps
     * into `text`. Index 15 (transparent) is returned unchanged.
     *
     * @param i Source palette index.
     * @return The index one step toward `text`.
     */
    static constexpr uint8_t towardText(uint8_t i) {
        if (i == PALETTE_TRANSPARENT) return PALETTE_TRANSPARENT;
        if (i == ROLE_BG) return ROLE_SURFACE;
        if (i == ROLE_SURFACE) return ROLE_MUTED;
        return ROLE_TEXT;  // muted, text, the accents, temporary 8-14
    }
};

/**
 * @brief Parse a hex color string into RGB components
 *
 * Accepts both `#rrggbb` and plain `rrggbb` formats. Hex digits may be upper or
 * lower case. Returns false if `hex` is null or cannot be parsed.
 *
 * @param hex Null-terminated hex color string (e.g., "#ff0000" or "ff0000")
 * @param r Output red channel
 * @param g Output green channel
 * @param b Output blue channel
 * @return true if parsing succeeded, false otherwise
 */
bool parseHexColor(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b);

/** @brief Engine-level global palette instance shared across all translation units */
extern Palette g_palette;

} // namespace enjin2
