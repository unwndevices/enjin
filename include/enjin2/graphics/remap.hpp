#pragma once

#include <cstdint>

#include "palette.hpp"

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
 * Constructors: `identity()`, `solid()`, `recolor()`, `compose()`, and the
 * shade-chain steps `towardBg()` / `towardText()` built on the Palette roles
 * (@ref Palette::towardBg). All share this same `lut[16]` representation.
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
     * @brief One shade-chain step toward `bg` for every index.
     *
     * Each entry is @ref Palette::towardBg of its source index: `text ->
     * muted -> surface -> bg`, `bg` holds, an accent steps into `muted`,
     * transparency stays transparent. Compose it to step further; three steps
     * reach `bg` from anywhere.
     *
     * @return A `Remap` where `lut[i] == Palette::towardBg(i)`.
     */
    static constexpr Remap towardBg() {
        Remap r{};
        for (uint8_t i = 0; i < 16; ++i) {
            r.lut[i] = Palette::towardBg(i);
        }
        return r;
    }

    /**
     * @brief One shade-chain step toward `text` for every index.
     *
     * The mirror of @ref towardBg — each entry is @ref Palette::towardText of
     * its source index (`bg -> surface -> muted -> text`, an accent steps
     * into `text`).
     *
     * @return A `Remap` where `lut[i] == Palette::towardText(i)`.
     */
    static constexpr Remap towardText() {
        Remap r{};
        for (uint8_t i = 0; i < 16; ++i) {
            r.lut[i] = Palette::towardText(i);
        }
        return r;
    }

    /**
     * @brief Recolour a single index; pass everything else through unchanged.
     *
     * `lut[from] == to`, every other entry identity. The one-index counterpart
     * of @ref solid — a shader that repaints just one palette slot (e.g. an
     * accent flash) without disturbing the rest of the frame.
     *
     * @param from Source index (0-15) to remap.
     * @param to   Index it maps to.
     * @return An otherwise-identity `Remap` with `lut[from & 0x0F] == to & 0x0F`.
     */
    static constexpr Remap recolor(uint8_t from, uint8_t to) {
        Remap r{};
        r.lut[from & 0x0F] = to & 0x0F;
        return r;
    }

    /**
     * @brief Compose two remaps into one — `first` applied, then `second`.
     *
     * Sequential composition in call order: the result maps `i` to
     * `second.apply(first.apply(i))`, so `compose(a, b)` reads "do a, then b".
     * This is how passes stack — e.g. two `towardBg` steps for a deeper dim —
     * as a single `Remap` with no per-pixel double lookup.
     *
     * @param first  The remap applied first.
     * @param second The remap applied to `first`'s output.
     * @return A `Remap` where `lut[i] == second.lut[first.lut[i]]`.
     */
    static constexpr Remap compose(const Remap& first, const Remap& second) {
        Remap r{};
        for (uint8_t i = 0; i < 16; ++i) {
            r.lut[i] = second.lut[first.lut[i] & 0x0F];
        }
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
