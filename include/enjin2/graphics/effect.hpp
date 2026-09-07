#pragma once

#include <cstdint>

#include "remap.hpp"

namespace enjin2 {

/**
 * @file effect.hpp
 * @brief The index-shader data model: a 1-bit tileable @ref Mask × a 16-entry
 *        @ref Remap × integer phase.
 *
 * An *index shader* recolours pixels *inside* a layer. It is three pieces of
 * data, never bespoke code: a tileable 1-bit **mask** that says *where*, a
 * **`Remap`** that says *to what*, and an integer **phase** that slides the mask.
 * The mask is always sampled at **absolute canvas coordinates** (plus phase),
 * never relative to the shaded rect — so a lone dirty tile redraws with the same
 * pattern phase as its neighbours and no seam appears at the tile boundary.
 *
 * The two engine apply sites — @ref Canvas4::shade over a rect and @ref
 * SpriteSheet::draw through a sprite silhouette — funnel every surviving pixel
 * through the one shared stage @ref Effect::shadePixel; the Lua `gfx.drawSprite`
 * binding applies that same stage inline over its flip/rotate blit. Holo, dim,
 * fade, scanline and ghost are all instances built from @ref Remap primitives
 * (see the `Effect::` factories).
 */

/**
 * @brief A tileable 1-bit pattern, period ≤ 32×32, sampled at absolute coords.
 *
 * `rows[r]` holds one row of the pattern as a bitfield: bit `c` (for `c` in
 * `[0, w)`) is the mask value at pattern cell `(c, r)`. The pattern tiles the
 * whole canvas by wrapping sample coordinates into `[0, w) × [0, h)` with a
 * positive modulo, so negative coordinates wrap correctly too.
 */
struct Mask {
    /// Maximum pattern period on each axis (bit width of a `rows[]` entry).
    static constexpr uint8_t MAX_PERIOD = 32;

    uint32_t rows[MAX_PERIOD];  ///< `rows[r]` bit `c` = pattern value at cell (c, r).
    uint8_t w;                  ///< Pattern width (period on x), 1..32.
    uint8_t h;                  ///< Pattern height (period on y), 1..32.

    /// @brief Default constructor — the full mask (every pixel set).
    constexpr Mask() : rows{}, w(1), h(1) { rows[0] = 1u; }

    /**
     * @brief Sample the mask at absolute canvas coordinates.
     * @param ax Absolute x (may be negative; wraps into the period).
     * @param ay Absolute y (may be negative; wraps into the period).
     * @return true where the pattern bit is set.
     */
    constexpr bool sample(int16_t ax, int16_t ay) const {
        const uint8_t cx = static_cast<uint8_t>(((ax % w) + w) % w);
        const uint8_t cy = static_cast<uint8_t>(((ay % h) + h) % h);
        return ((rows[cy] >> cx) & 1u) != 0u;
    }

    /**
     * @brief The full mask: every pixel set (period 1×1).
     * @return A `Mask` whose `sample()` is always true.
     */
    static constexpr Mask full() {
        Mask m{};  // default is already the full mask
        return m;
    }

    /**
     * @brief Diagonal stripes: an `on`-wide band repeating every `period` cells
     *        along the `x + y` diagonal.
     *
     * The moving-band mask behind holo foil — advance the effect's phase and the
     * stripes travel across the surface. Period is clamped to `[1, MAX_PERIOD]`.
     *
     * @param period Stripe repeat, in cells (clamped 1..32).
     * @param on     Width of the set band within each period (clamped 0..period).
     * @return A `period × period` diagonal-stripe mask.
     */
    static constexpr Mask stripe(uint8_t period, uint8_t on = 1) {
        const uint8_t p = period < 1 ? 1 : (period > MAX_PERIOD ? MAX_PERIOD : period);
        const uint8_t band = on > p ? p : on;
        Mask m{};
        m.w = p;
        m.h = p;
        for (uint8_t r = 0; r < p; ++r) {
            uint32_t row = 0;
            for (uint8_t c = 0; c < p; ++c) {
                if (static_cast<uint8_t>((c + r) % p) < band) {
                    row |= (1u << c);
                }
            }
            m.rows[r] = row;
        }
        return m;
    }

    /**
     * @brief Horizontal scanlines: `thickness` set rows every `period` rows.
     * @param period Line repeat, in rows (clamped 1..32).
     * @param thickness Number of set rows per period (clamped 0..period).
     * @return A `1 × period` horizontal-line mask.
     */
    static constexpr Mask hlines(uint8_t period, uint8_t thickness = 1) {
        const uint8_t p = period < 1 ? 1 : (period > MAX_PERIOD ? MAX_PERIOD : period);
        const uint8_t t = thickness > p ? p : thickness;
        Mask m{};
        m.w = 1;
        m.h = p;
        for (uint8_t r = 0; r < p; ++r) {
            m.rows[r] = (r < t) ? 1u : 0u;
        }
        return m;
    }

    /**
     * @brief A 4×4 ordered-dither (Bayer) mask at a given coverage `level`.
     *
     * `level` is the threshold into the 16-entry Bayer matrix: 0 sets no pixels,
     * 16 sets all of them, and intermediate levels give an even dither. Animating
     * `level` (not phase) is how @ref Effect::fade dissolves a region.
     *
     * @param level Coverage threshold 0..16 (clamped).
     * @return A `4 × 4` ordered-dither mask.
     */
    static constexpr Mask bayer(uint8_t level) {
        // Standard 4×4 Bayer threshold matrix, values 0..15.
        constexpr uint8_t B[4][4] = {
            {0, 8, 2, 10},
            {12, 4, 14, 6},
            {3, 11, 1, 9},
            {15, 7, 13, 5},
        };
        const uint8_t lv = level > 16 ? 16 : level;
        Mask m{};
        m.w = 4;
        m.h = 4;
        for (uint8_t r = 0; r < 4; ++r) {
            uint32_t row = 0;
            for (uint8_t c = 0; c < 4; ++c) {
                if (B[r][c] < lv) {
                    row |= (1u << c);
                }
            }
            m.rows[r] = row;
        }
        return m;
    }
};

/**
 * @brief An index shader: @ref Mask × @ref Remap × integer phase.
 *
 * The mask decides *where* the remap applies; unmasked pixels pass through
 * untouched. The colour stage is the shared @ref Remap primitive, so an effect
 * clips by hue for free (`Remap::onRamp`) and stacks passes for free
 * (`Remap::compose`). Sampling is at absolute coordinates plus phase, the R4
 * "one grid" rule that keeps dirty tiles seamless.
 */
struct Effect {
    Mask mask;               ///< Where the remap applies.
    Remap remap;             ///< What masked indices map to.
    int16_t phaseX = 0;      ///< Absolute x offset added before sampling the mask.
    int16_t phaseY = 0;      ///< Absolute y offset added before sampling the mask.

    /// @brief Default constructor — full mask, identity remap (a no-op shader).
    constexpr Effect() : mask(Mask::full()), remap(Remap::identity()) {}

    /// @brief Construct from a mask, remap and optional phase.
    constexpr Effect(const Mask& m, const Remap& r, int16_t px = 0, int16_t py = 0)
        : mask(m), remap(r), phaseX(px), phaseY(py) {}

    /**
     * @brief The one shared shader stage both apply sites call.
     *
     * `(remap.apply(src) & m) | (src & ~m)` per pixel: where the mask is set
     * (sampled at absolute coords + phase), the source index is remapped;
     * elsewhere it passes through. A shader honours `remap`'s entry 15, so it
     * may legitimately paint the transparent slot (unlike a compositor tint).
     *
     * @param src Source palette index.
     * @param ax  Absolute canvas x of the pixel.
     * @param ay  Absolute canvas y of the pixel.
     * @return The shaded index.
     */
    constexpr uint8_t shadePixel(uint8_t src, int16_t ax, int16_t ay) const {
        // Branch-free by construction: `m` is 0xFF where the mask is set, 0x00
        // elsewhere, so the blend selects the remapped index or the source
        // with no data-dependent branch. (A packed two-pixels-per-byte fast
        // path is left out on purpose — `shade` stays on the same per-pixel
        // apply as the compositor's tint.)
        const bool set = mask.sample(static_cast<int16_t>(ax + phaseX),
                                     static_cast<int16_t>(ay + phaseY));
        const uint8_t m = static_cast<uint8_t>(-static_cast<int8_t>(set));
        return static_cast<uint8_t>((remap.apply(src) & m) | (src & ~m));
    }

    // --- Presets: each is a Mask + a Remap primitive, never bespoke code. ---

    /**
     * @brief Holo foil: a diagonal lighten band that travels with `phase`.
     * @param phase Absolute x phase; advance it per frame for the shimmer.
     */
    static constexpr Effect holo(int16_t phase = 0) {
        return Effect(Mask::stripe(6, 2), Remap::lighten(), phase, 0);
    }

    /// @brief Dim: darken the whole shaded region by one shade.
    static constexpr Effect dim() {
        return Effect(Mask::full(), Remap::darken());
    }

    /**
     * @brief Dither fade: darken an ordered-dither fraction of the region.
     * @param level Bayer coverage 0..16 — animate this (not phase) to dissolve.
     */
    static constexpr Effect fade(uint8_t level) {
        return Effect(Mask::bayer(level), Remap::darken());
    }

    /// @brief Scanline: darken every other horizontal line.
    static constexpr Effect scanline() {
        return Effect(Mask::hlines(2, 1), Remap::darken());
    }

    /**
     * @brief Ghost: a displaced half-tone lighten smear.
     * @param dx Phase x offset of the smear.
     * @param dy Phase y offset of the smear.
     */
    static constexpr Effect ghost(int16_t dx = 1, int16_t dy = 1) {
        return Effect(Mask::bayer(8), Remap::lighten(), dx, dy);
    }
};

} // namespace enjin2
