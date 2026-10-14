/**
 * @file fixed.hpp
 * @brief Fixed: a Q16.16 fixed-point number (ADR-0016, Tomodachi #329)
 *
 * The number type of the vector Shape path: ring coordinates, edge stepping,
 * and later the Lattice and the warp. Integer operations only, so the same
 * inputs give the same bits on the S3, in WASM and in native tests (float
 * can't promise that: GCC fuses `a*b+c` on the LX7, libms differ).
 *
 * Q16.16 in an int32_t: 16 integer bits (±32768 logical px) and 1/65536 px
 * resolution. The Shape pack stores Q8.8; fromQ8_8() widens it exactly.
 *
 * Rounding is part of the contract (golden tests depend on it):
 * - `a * b` is the exact 64-bit product shifted right 16 (rounds toward -inf);
 * - `a / b` is `(a << 16) / b` in 64 bits (rounds toward zero; b != 0);
 * - `+` / `-` wrap like int32_t would, callers keep values in range.
 *
 * Right shifts of negative values are arithmetic on every compiler Enjin
 * targets (GCC, Clang, Xtensa GCC, Emscripten); C++20 makes it standard.
 */
#pragma once

#include <cstdint>

namespace enjin2 {

struct Fixed {
    static constexpr int     kFracBits = 16;
    static constexpr int32_t kOne      = int32_t(1) << kFracBits;

    int32_t raw = 0;  ///< value × 65536

    static constexpr Fixed fromRaw(int32_t r) { Fixed f; f.raw = r; return f; }
    static constexpr Fixed fromInt(int32_t i) { return fromRaw(int32_t(uint32_t(i) << kFracBits)); }
    /// Widen a Shape pack Q8.8 coordinate (exact).
    static constexpr Fixed fromQ8_8(int16_t q) { return fromRaw(int32_t(q) * 256); }
    /// num / den, truncated toward zero. For constants; den != 0.
    static constexpr Fixed fromRatio(int32_t num, int32_t den)
    {
        return fromRaw(int32_t((int64_t(num) * kOne) / den));
    }

    constexpr int32_t floorToInt() const { return raw >> kFracBits; }
    constexpr int32_t ceilToInt() const { return int32_t((int64_t(raw) + (kOne - 1)) >> kFracBits); }
    /// Nearest integer, halves rounded up (toward +inf).
    constexpr int32_t roundToInt() const { return int32_t((int64_t(raw) + kOne / 2) >> kFracBits); }

    constexpr Fixed operator-() const { return fromRaw(-raw); }
    constexpr Fixed operator+(Fixed o) const { return fromRaw(int32_t(uint32_t(raw) + uint32_t(o.raw))); }
    constexpr Fixed operator-(Fixed o) const { return fromRaw(int32_t(uint32_t(raw) - uint32_t(o.raw))); }
    constexpr Fixed operator*(Fixed o) const { return fromRaw(int32_t((int64_t(raw) * o.raw) >> kFracBits)); }
    constexpr Fixed operator/(Fixed o) const { return fromRaw(int32_t((int64_t(raw) * kOne) / o.raw)); }
    constexpr Fixed& operator+=(Fixed o) { *this = *this + o; return *this; }
    constexpr Fixed& operator-=(Fixed o) { *this = *this - o; return *this; }

    constexpr bool operator==(Fixed o) const { return raw == o.raw; }
    constexpr bool operator!=(Fixed o) const { return raw != o.raw; }
    constexpr bool operator<(Fixed o) const { return raw < o.raw; }
    constexpr bool operator<=(Fixed o) const { return raw <= o.raw; }
    constexpr bool operator>(Fixed o) const { return raw > o.raw; }
    constexpr bool operator>=(Fixed o) const { return raw >= o.raw; }
};

/// A point or offset in Fixed logical pixels (y down).
struct FixedVec2 {
    Fixed x, y;

    constexpr FixedVec2 operator+(FixedVec2 o) const { return {x + o.x, y + o.y}; }
    constexpr FixedVec2 operator-(FixedVec2 o) const { return {x - o.x, y - o.y}; }
    constexpr bool operator==(FixedVec2 o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(FixedVec2 o) const { return !(*this == o); }
};

} // namespace enjin2
