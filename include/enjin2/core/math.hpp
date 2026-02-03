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

    /**
     * @brief Fast integer square root using Newton's method
     * @param n Non-negative integer to compute square root of
     * @return Integer square root of n
     */
    inline uint16_t isqrt(uint32_t n) {
        if (n == 0) return 0;
        
        uint32_t x = n;
        uint32_t y = (x + 1) / 2;
        
        while (y < x) {
            x = y;
            y = (x + n / x) / 2;
        }
        
        return static_cast<uint16_t>(x);
    }

    /**
     * @brief Fast absolute value for signed types
     * @tparam T Signed numeric type
     * @param value Value to get absolute value of
     * @return Absolute value of input
     */
    template<typename T>
    constexpr T abs(T value) {
        return value < 0 ? -value : value;
    }

    /**
     * @brief Clamp value to specified range
     * @tparam T Numeric type
     * @param value Value to clamp
     * @param min_val Minimum allowed value
     * @param max_val Maximum allowed value
     * @return Clamped value within [min_val, max_val]
     */
    template<typename T>
    constexpr T clamp(T value, T min_val, T max_val) {
        return value < min_val ? min_val : (value > max_val ? max_val : value);
    }

    /**
     * @brief Linear interpolation between two values
     * @tparam T Numeric type
     * @param a Start value (when t=0)
     * @param b End value (when t=1)
     * @param t Interpolation factor (0-1)
     * @return Interpolated value between a and b
     */
    template<typename T>
    constexpr T lerp(T a, T b, float t) {
        return a + (b - a) * t;
    }

    /**
     * @brief Map value from one range to another
     * @tparam T Numeric type
     * @param value Input value to map
     * @param in_min Minimum of input range
     * @param in_max Maximum of input range
     * @param out_min Minimum of output range
     * @param out_max Maximum of output range
     * @return Value mapped from input range to output range
     */
    template<typename T>
    constexpr T map(T value, T in_min, T in_max, T out_min, T out_max) {
        return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
    }

    /**
     * @brief Fast trigonometry using lookup table
     *
     * Provides sine and cosine functions optimized for embedded systems.
     * Uses 256-step lookup table for 0-2π range.
     */
    class TrigLUT {
private:
    static constexpr size_t TABLE_SIZE = 256;
    static constexpr float SCALE = TABLE_SIZE / (2.0f * M_PI);

    // Sine lookup table will be implemented later
    static int16_t getSineValue(uint8_t index);

public:
        /**
         * @brief Fast sine function using 0-255 angle range
         * @param angle Angle value (0-255 representing 0-2π)
         * @return Sine value in range [-32767, 32767] (Q15 fixed-point)
         */
        static int16_t sin(uint16_t angle) {
            // Simple implementation using standard library for now
            float rad = (float)angle * 2.0f * M_PI / 256.0f;
            return (int16_t)(32767.0f * std::sin(rad));
        }

        /**
         * @brief Fast cosine function using 0-255 angle range
         * @param angle Angle value (0-255 representing 0-2π)
         * @return Cosine value in range [-32767, 32767] (Q15 fixed-point)
         */
        static int16_t cos(uint16_t angle) {
            float rad = (float)angle * 2.0f * M_PI / 256.0f;
            return (int16_t)(32767.0f * std::cos(rad));
        }

        /**
         * @brief Convert float radians to lookup table index
         * @param radians Angle in radians
         * @return Index in range [0, 255] for lookup table
         */
        static uint16_t angleToIndex(float radians) {
            float normalized = radians * SCALE;
            return static_cast<uint16_t>(normalized) & 0xFF;
        }
    };

    /**
     * @brief Calculate Euclidean distance between two points
     * @param x1 X coordinate of first point
     * @param y1 Y coordinate of first point
     * @param x2 X coordinate of second point
     * @param y2 Y coordinate of second point
     * @return Euclidean distance between points
     */
    inline uint16_t distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int32_t dx = x2 - x1;
        int32_t dy = y2 - y1;
        return isqrt(dx * dx + dy * dy);
    }

    /**
     * @brief 2D vector with floating-point components
     *
     * Provides basic vector operations for 2D graphics and physics.
     */
    struct Vector2 {
        float x, y;

        /**
         * @brief Default constructor creates zero vector
         */
        Vector2() : x(0), y(0) {}

        /**
         * @brief Constructor with components
         * @param x_ X component
         * @param y_ Y component
         */
        Vector2(float x_, float y_) : x(x_), y(y_) {}

        /**
         * @brief Vector addition
         * @param other Vector to add
         * @return Sum of vectors
         */
        Vector2 operator+(const Vector2& other) const {
            return Vector2(x + other.x, y + other.y);
        }

        /**
         * @brief Vector subtraction
         * @param other Vector to subtract
         * @return Difference of vectors
         */
        Vector2 operator-(const Vector2& other) const {
            return Vector2(x - other.x, y - other.y);
        }

        /**
         * @brief Scalar multiplication
         * @param scalar Value to multiply by
         * @return Scaled vector
         */
        Vector2 operator*(float scalar) const {
            return Vector2(x * scalar, y * scalar);
        }

        /**
         * @brief Calculate vector magnitude
         * @return Length of vector
         */
        float length() const {
            return sqrt(x * x + y * y);
        }

        /**
         * @brief Get unit vector in same direction
         * @return Normalized vector (length = 1), or zero vector if length is 0
         */
        Vector2 normalized() const {
            float len = length();
            return len > 0 ? Vector2(x / len, y / len) : Vector2(0, 0);
        }

        /**
         * @brief Calculate dot product with another vector
         * @param other Vector to compute dot product with
         * @return Dot product (x*x + y*y)
         */
        float dot(const Vector2& other) const {
            return x * other.x + y * other.y;
        }
    };
    
} // namespace math

} // namespace enjin2