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

    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;

    inline uint16_t isqrt(uint32_t n) {
        if (n == 0) return 0;
        uint32_t x = n;
        uint32_t y = (x + 1) / 2;
        while (y < x) { x = y; y = (x + n / x) / 2; }
        return static_cast<uint16_t>(x);
    }

    template<typename T>
    constexpr T abs(T value) {
        return value < 0 ? -value : value;
    }

    template<typename T>
    constexpr T clamp(T value, T min_val, T max_val) {
        return value < min_val ? min_val : (value > max_val ? max_val : value);
    }

    template<typename T>
    constexpr T lerp(T a, T b, float t) {
        return a + (b - a) * t;
    }

    template<typename T>
    constexpr T map(T value, T in_min, T in_max, T out_min, T out_max) {
        return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
    }

    /** @brief Returns -1, 0, or 1 */
    template<typename T>
    constexpr T sign(T value) {
        return (value > T(0)) - (value < T(0));
    }

    /** @brief Hermite smoothstep (clamped, order-3) */
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    /**
     * @brief Fast trigonometry using lookup table
     *
     * Uses 256-step table for 0-2pi range.
     * Currently delegates to std sin/cos; will be replaced with actual LUT.
     */
    class TrigLUT {
    private:
        static constexpr size_t TABLE_SIZE = 256;
        static constexpr float SCALE = TABLE_SIZE / TWO_PI;

        static int16_t getSineValue(uint8_t index);

    public:
        static int16_t sin(uint16_t angle) {
            float rad = static_cast<float>(angle) * TWO_PI / 256.0f;
            return static_cast<int16_t>(32767.0f * std::sin(rad));
        }

        static int16_t cos(uint16_t angle) {
            float rad = static_cast<float>(angle) * TWO_PI / 256.0f;
            return static_cast<int16_t>(32767.0f * std::cos(rad));
        }

        static uint16_t angleToIndex(float radians) {
            float normalized = radians * SCALE;
            return static_cast<uint16_t>(normalized) & 0xFF;
        }
    };

    inline uint16_t distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int32_t dx = x2 - x1;
        int32_t dy = y2 - y1;
        return isqrt(dx * dx + dy * dy);
    }

} // namespace math

} // namespace enjin2