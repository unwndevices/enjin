#pragma once

/**
 * @file oklab.hpp
 * @brief sRGB to Oklab, in float and in integers (ADR-0026, Tomodachi #459).
 *
 * Björn Ottosson's Oklab: its Euclidean distance is close to how far apart two
 * colours look, so "the nearest palette colour" means what the eye means. The
 * float form is the reference (a port of `enjin_assets/oklab.py`, moved here
 * from the music player's removed cover pipeline, #227). The integer form is
 * what the role dither runs: tables and integer multiplies only, so a host and
 * the device give the same bits.
 */

#include <cstdint>

namespace enjin2 {

/// A colour in Oklab: L is lightness (0 black .. 1 white), a and b the
/// green-red and blue-yellow axes (about -0.4 .. 0.4 for sRGB).
struct Oklab {
    float L = 0.0f;
    float a = 0.0f;
    float b = 0.0f;
};

/// sRGB bytes to Oklab (sRGB -> linear -> LMS -> cube root -> Oklab).
Oklab srgbToOklab(uint8_t r, uint8_t g, uint8_t b);

/// Squared distance between two Oklab colours: the nearest-colour search only
/// needs the order, so no square root.
inline float oklabDist2(const Oklab& p, const Oklab& q) {
    const float dL = p.L - q.L, da = p.a - q.a, db = p.b - q.b;
    return dL * dL + da * da + db * db;
}

/// The integer Oklab scale: OklabQ holds each component times this.
constexpr int32_t OKLAB_Q_ONE = 65535;

/// Oklab in integers, each component times OKLAB_Q_ONE (L 0..65535, a and b
/// about -26000..26000). Within 0.001 of srgbToOklab per component.
struct OklabQ {
    int32_t L = 0;
    int32_t a = 0;
    int32_t b = 0;
};

/// sRGB bytes to integer Oklab: the same result on every target.
OklabQ srgbToOklabQ(uint8_t r, uint8_t g, uint8_t b);

} // namespace enjin2
