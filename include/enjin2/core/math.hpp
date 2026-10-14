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
     * @brief Fixed-point trigonometry from a lookup table
     *
     * 1024 steps per turn with linear interpolation between steps, integer
     * only, so the same angle gives bit-identical values on the device, WASM
     * and native (ADR-0016: no libm on the Shape path). Outputs are Q15-ish,
     * scaled to [-32767, 32767].
     *
     * Two angle units:
     * - sin16/cos16: 65536ths of a turn (a uint16_t binary angle). The top 10
     *   bits pick the table step, the low 6 bits interpolate within it.
     * - sin/cos: 256ths of a turn (legacy API), the same curve sampled every
     *   256 fine units.
     *
     * Accuracy: max |error| against 32767*sin over every uint16_t angle is
     * 1.03 LSB (about 3.1e-5 of full scale); kMaxErrorLsb is the asserted
     * bound (trig_lut_test). sin16 is exactly odd (sin16(-a) == -sin16(a)).
     *
     * Storage: a quarter-wave table of 257 int16_t (514 bytes), const, so
     * it lives in flash (.rodata), not internal RAM.
     */
    class TrigLUT {
    public:
        static constexpr uint32_t kStepsPerTurn = 1024;  ///< Table steps per full turn
        static constexpr double kMaxErrorLsb = 1.05;     ///< Asserted max |error| of sin16/cos16 in output LSBs

    private:
        static constexpr uint32_t kQuarterSteps = kStepsPerTurn / 4;  // 256
        static constexpr uint32_t kFracBits = 6;                      // 65536 / 1024 = 2^6

        // kQuarterSine[i] = round(32767 * sin(i * 2*PI / 1024)) for i = 0..256
        // (first quadrant, endpoints included). The other three quadrants are
        // mirrored from it, which keeps every intermediate non-negative.
        static constexpr int16_t kQuarterSine[kQuarterSteps + 1] = {
                0,   201,   402,   603,   804,  1005,  1206,  1407,
             1608,  1809,  2009,  2210,  2410,  2611,  2811,  3012,
             3212,  3412,  3612,  3811,  4011,  4210,  4410,  4609,
             4808,  5007,  5205,  5404,  5602,  5800,  5998,  6195,
             6393,  6590,  6786,  6983,  7179,  7375,  7571,  7767,
             7962,  8157,  8351,  8545,  8739,  8933,  9126,  9319,
             9512,  9704,  9896, 10087, 10278, 10469, 10659, 10849,
            11039, 11228, 11417, 11605, 11793, 11980, 12167, 12353,
            12539, 12725, 12910, 13094, 13279, 13462, 13645, 13828,
            14010, 14191, 14372, 14553, 14732, 14912, 15090, 15269,
            15446, 15623, 15800, 15976, 16151, 16325, 16499, 16673,
            16846, 17018, 17189, 17360, 17530, 17700, 17869, 18037,
            18204, 18371, 18537, 18703, 18868, 19032, 19195, 19357,
            19519, 19680, 19841, 20000, 20159, 20317, 20475, 20631,
            20787, 20942, 21096, 21250, 21403, 21554, 21705, 21856,
            22005, 22154, 22301, 22448, 22594, 22739, 22884, 23027,
            23170, 23311, 23452, 23592, 23731, 23870, 24007, 24143,
            24279, 24413, 24547, 24680, 24811, 24942, 25072, 25201,
            25329, 25456, 25582, 25708, 25832, 25955, 26077, 26198,
            26319, 26438, 26556, 26674, 26790, 26905, 27019, 27133,
            27245, 27356, 27466, 27575, 27683, 27790, 27896, 28001,
            28105, 28208, 28310, 28411, 28510, 28609, 28706, 28803,
            28898, 28992, 29085, 29177, 29268, 29358, 29447, 29534,
            29621, 29706, 29791, 29874, 29956, 30037, 30117, 30195,
            30273, 30349, 30424, 30498, 30571, 30643, 30714, 30783,
            30852, 30919, 30985, 31050, 31113, 31176, 31237, 31297,
            31356, 31414, 31470, 31526, 31580, 31633, 31685, 31736,
            31785, 31833, 31880, 31926, 31971, 32014, 32057, 32098,
            32137, 32176, 32213, 32250, 32285, 32318, 32351, 32382,
            32412, 32441, 32469, 32495, 32521, 32545, 32567, 32589,
            32609, 32628, 32646, 32663, 32678, 32692, 32705, 32717,
            32728, 32737, 32745, 32752, 32757, 32761, 32765, 32766,
            32767
        };

    public:
        /**
         * @brief Fixed-point sine of a 65536-step binary angle
         * @param angle  Angle in 65536ths of a full turn (wraps naturally)
         * @return Sine value scaled to [-32767, 32767]
         */
        static inline int16_t sin16(uint16_t angle) {
            const bool negative = (angle & 0x8000u) != 0;
            uint32_t p = angle & 0x7FFFu;                  // position in the half turn
            if (p > 0x4000u) p = 0x8000u - p;              // mirror into the first quadrant
            const uint32_t i = p >> kFracBits;             // 0..256
            const uint32_t f = p & ((1u << kFracBits) - 1);
            int32_t v = kQuarterSine[i];
            if (f != 0) {                                  // i < 256 here
                const int32_t d = kQuarterSine[i + 1] - v; // >= 0 in the first quadrant
                v += (d * static_cast<int32_t>(f) + (1 << (kFracBits - 1))) >> kFracBits;
            }
            return static_cast<int16_t>(negative ? -v : v);
        }

        /**
         * @brief Fixed-point cosine of a 65536-step binary angle
         *
         * cos(x) = sin(x + quarter turn)
         *
         * @param angle  Angle in 65536ths of a full turn (wraps naturally)
         * @return Cosine value scaled to [-32767, 32767]
         */
        static inline int16_t cos16(uint16_t angle) {
            return sin16(static_cast<uint16_t>(angle + 0x4000u));
        }

        /**
         * @brief Fixed-point sine of a 256-step angle
         * @param angle  Angle in 256ths of a full turn (0-255, wraps)
         * @return Sine value scaled to [-32767, 32767]
         */
        static inline int16_t sin(uint16_t angle) {
            return sin16(static_cast<uint16_t>(angle << 8));
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
            return sin(static_cast<uint16_t>(angle + 64));
        }

        /**
         * @brief Convert radians to 256-step LUT index
         * @param radians Angle in radians
         * @return LUT index (0-255)
         */
        static uint16_t angleToIndex(float radians) {
            float normalized = radians * (256.0f / TWO_PI);
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