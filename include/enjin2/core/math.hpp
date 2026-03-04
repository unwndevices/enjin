#pragma once

#include <cstdint>
#include <cmath>

namespace enjin2 {

/**
 * @file math.hpp
 * @brief Fast math utilities for embedded systems
 *
 * Provides integer math, vector operations, and trigonometry functions
 * optimized for performance on resource-constrained systems.
 */

/**
 * @brief Fast integer math utilities namespace
 */
namespace math {

    // Arduino.h defines PI/TWO_PI as macros — undefine to avoid clash
    #ifdef PI
    #undef PI
    #endif
    #ifdef TWO_PI
    #undef TWO_PI
    #endif

    constexpr float PI = 3.14159265358979323846f;  ///< Pi constant
    constexpr float TWO_PI = 2.0f * PI;              ///< 2*Pi constant

    /**
     * @brief Integer square root via Newton's method
     * @param n Value to take the square root of
     * @return Floored integer square root
     */
    inline uint16_t isqrt(uint32_t n) {
        if (n == 0) return 0;
        uint32_t x = n;
        uint32_t y = (x + 1) / 2;
        while (y < x) { x = y; y = (x + n / x) / 2; }
        return static_cast<uint16_t>(x);
    }

    /**
     * @brief Absolute value
     * @param value Input value
     * @return Non-negative absolute value
     */
    template<typename T>
    constexpr T abs(T value) {
        return value < 0 ? -value : value;
    }

    /**
     * @brief Clamp value to [min_val, max_val]
     * @param value  Value to clamp
     * @param min_val  Lower bound
     * @param max_val  Upper bound
     * @return Clamped value
     */
    template<typename T>
    constexpr T clamp(T value, T min_val, T max_val) {
        return value < min_val ? min_val : (value > max_val ? max_val : value);
    }

    /**
     * @brief Linear interpolation between a and b
     * @param a  Start value
     * @param b  End value
     * @param t  Interpolation factor (0.0 = a, 1.0 = b)
     * @return Interpolated value
     */
    template<typename T>
    constexpr T lerp(T a, T b, float t) {
        return a + (b - a) * t;
    }

    /**
     * @brief Re-map a value from one range to another
     * @param value   Input value
     * @param in_min  Input range lower bound
     * @param in_max  Input range upper bound
     * @param out_min Output range lower bound
     * @param out_max Output range upper bound
     * @return Value mapped to output range
     */
    template<typename T>
    constexpr T map(T value, T in_min, T in_max, T out_min, T out_max) {
        return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
    }

    /**
     * @brief Returns -1, 0, or 1 depending on the sign of value
     * @param value Input value
     * @return Sign of the value
     */
    template<typename T>
    constexpr T sign(T value) {
        return (value > T(0)) - (value < T(0));
    }

    /**
     * @brief Hermite smoothstep (clamped, order-3)
     * @param edge0 Lower edge
     * @param edge1 Upper edge
     * @param x     Input value
     * @return Smooth interpolation result in [0, 1]
     */
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    /**
     * @brief Fast trigonometry using lookup table
     *
     * Uses 256-step table for 0-2pi range.
     * Backed by a precomputed constexpr int16_t[256] sine table.
     * Avoids std::sin/cos at runtime — suitable for embedded targets (ESP32).
     */
    class TrigLUT {
    private:
        static constexpr size_t TABLE_SIZE = 256;
        static constexpr float SCALE = TABLE_SIZE / TWO_PI;

        // Precomputed sine table: sin_table[i] = round(32767 * sin(i * 2*PI / 256))
        // for i = 0..255. Generated once at compile time (constexpr).
        static constexpr int16_t sin_table[256] = {
               0,   804,  1607,  2410,  3211,  4011,  4807,  5601,
            6392,  7179,  7961,  8739,  9511, 10278, 11038, 11793,
           12539, 13278, 14009, 14732, 15446, 16150, 16845, 17530,
           18204, 18867, 19519, 20159, 20787, 21402, 22004, 22594,
           23169, 23731, 24278, 24811, 25329, 25832, 26319, 26790,
           27244, 27683, 28105, 28510, 28897, 29268, 29621, 29955,
           30272, 30571, 30851, 31113, 31356, 31580, 31785, 31970,
           32137, 32284, 32412, 32520, 32609, 32678, 32727, 32757,
           32767, 32757, 32727, 32678, 32609, 32520, 32412, 32284,
           32137, 31970, 31785, 31580, 31356, 31113, 30851, 30571,
           30272, 29955, 29621, 29268, 28897, 28510, 28105, 27683,
           27244, 26790, 26319, 25832, 25329, 24811, 24278, 23731,
           23169, 22594, 22004, 21402, 20787, 20159, 19519, 18867,
           18204, 17530, 16845, 16150, 15446, 14732, 14009, 13278,
           12539, 11793, 11038, 10278,  9511,  8739,  7961,  7179,
            6392,  5601,  4807,  4011,  3211,  2410,  1607,   804,
               0,  -804, -1607, -2410, -3211, -4011, -4807, -5601,
           -6392, -7179, -7961, -8739, -9511,-10278,-11038,-11793,
          -12539,-13278,-14009,-14732,-15446,-16150,-16845,-17530,
          -18204,-18867,-19519,-20159,-20787,-21402,-22004,-22594,
          -23169,-23731,-24278,-24811,-25329,-25832,-26319,-26790,
          -27244,-27683,-28105,-28510,-28897,-29268,-29621,-29955,
          -30272,-30571,-30851,-31113,-31356,-31580,-31785,-31970,
          -32137,-32284,-32412,-32520,-32609,-32678,-32727,-32757,
          -32767,-32757,-32727,-32678,-32609,-32520,-32412,-32284,
          -32137,-31970,-31785,-31580,-31356,-31113,-30851,-30571,
          -30272,-29955,-29621,-29268,-28897,-28510,-28105,-27683,
          -27244,-26790,-26319,-25832,-25329,-24811,-24278,-23731,
          -23169,-22594,-22004,-21402,-20787,-20159,-19519,-18867,
          -18204,-17530,-16845,-16150,-15446,-14732,-14009,-13278,
          -12539,-11793,-11038,-10278, -9511, -8739, -7961, -7179,
           -6392, -5601, -4807, -4011, -3211, -2410, -1607,  -804
        };

        /**
         * @brief Look up sine value for a given 8-bit table index
         * @param index  Table index (0-255, wraps automatically)
         * @return Sine value scaled to [-32767, 32767]
         */
        static inline int16_t getSineValue(uint8_t index) {
            return sin_table[index];
        }

    public:
        /**
         * @brief Fixed-point sine of a 256-step angle
         * @param angle  Angle in 256ths of a full turn (0-255, wraps)
         * @return Sine value scaled to [-32767, 32767]
         */
        static inline int16_t sin(uint16_t angle) {
            return getSineValue(static_cast<uint8_t>(angle & 0xFF));
        }

        /**
         * @brief Fixed-point cosine of a 256-step angle
         *
         * cos(x) = sin(x + 64) (quarter-turn phase offset)
         *
         * @param angle  Angle in 256ths of a full turn (0-255, wraps)
         * @return Cosine value scaled to [-32767, 32767]
         */
        static inline int16_t cos(uint16_t angle) {
            return getSineValue(static_cast<uint8_t>((angle + 64) & 0xFF));
        }

        /**
         * @brief Convert radians to 256-step LUT index
         * @param radians Angle in radians
         * @return LUT index (0-255)
         */
        static uint16_t angleToIndex(float radians) {
            float normalized = radians * SCALE;
            return static_cast<uint16_t>(normalized) & 0xFF;
        }
    };

    /**
     * @brief Integer Euclidean distance between two points
     * @param x1 First point X
     * @param y1 First point Y
     * @param x2 Second point X
     * @param y2 Second point Y
     * @return Distance (integer square root)
     */
    inline uint16_t distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int32_t dx = x2 - x1;
        int32_t dy = y2 - y1;
        return isqrt(dx * dx + dy * dy);
    }

} // namespace math

} // namespace enjin2