#pragma once

#include <cstdint>

namespace enjin2 {

/**
 * @brief A 16-entry index→index lookup table — the one LUT type shared by the
 *        compositor's per-layer *tint* and the index *shader*.
 *
 * A `Remap` maps each of the 16 palette indices (0-15) to another index. It is
 * the single colour-remap primitive in Enjin: layer-wide tints (dim, fade,
 * recolour) and in-layer index shaders (holo, scanline, ghost) are both just a
 * `Remap` applied at different sites.
 *
 * Entry 15 is special. Index 15 is the transparency passthrough value. A
 * *shader* may remap entry 15 to a real colour (painting into holes); the
 * *compositor tint* always ignores entry 15 and treats it as transparent,
 * regardless of `lut[15]`. So `apply()` returns the raw table value and callers
 * that must honour transparency check the source index for 15 first (the
 * compositor does exactly this).
 *
 * This ticket ships the *skeleton* constructors — `identity()` and `solid()`.
 * The ramp-derived constructors (`lighten`, `darken`, `recolor`, `onRamp`,
 * `compose`) land with the ramp palette (see the ramp ticket); they build on
 * this same `lut[16]` representation.
 */
struct Remap {
    /// The lookup table: `lut[i]` is the index that source index `i` maps to.
    uint8_t lut[16];

    /// @brief Default constructor — the identity map (`lut[i] == i`).
    constexpr Remap() : lut{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} {}

    /**
     * @brief The identity remap: every index maps to itself.
     * @return A `Remap` where `lut[i] == i` for all i.
     */
    static constexpr Remap identity() { return Remap(); }

    /**
     * @brief A solid remap: every non-transparent index maps to `index`.
     *
     * Entry 15 is left as transparent passthrough (`lut[15] == 15`), so a solid
     * tint recolours a whole layer to one index without filling its holes.
     *
     * @param index The palette index (0-15) every source 0-14 maps to.
     * @return A `Remap` mapping 0-14 → `index`, 15 → 15.
     */
    static constexpr Remap solid(uint8_t index) {
        Remap r{};
        const uint8_t v = index & 0x0F;
        for (uint8_t i = 0; i < 15; ++i) {
            r.lut[i] = v;
        }
        r.lut[15] = 15;
        return r;
    }

    /**
     * @brief Apply the remap to a single index.
     * @param i Source palette index (only the low 4 bits are used).
     * @return The remapped index, `lut[i & 0x0F]`.
     */
    constexpr uint8_t apply(uint8_t i) const { return lut[i & 0x0F]; }

    /**
     * @brief Test whether this remap is the identity map.
     * @return true if `lut[i] == i` for every i, false otherwise.
     */
    constexpr bool isIdentity() const {
        for (uint8_t i = 0; i < 16; ++i) {
            if (lut[i] != i) {
                return false;
            }
        }
        return true;
    }
};

} // namespace enjin2
