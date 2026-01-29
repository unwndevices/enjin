#pragma once

#include <cstdint>
#include <cmath>

namespace enjin2 {

// Fast integer math utilities
namespace math {
    
    // Fast integer square root
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
    
    // Fast absolute value
    template<typename T>
    constexpr T abs(T value) {
        return value < 0 ? -value : value;
    }
    
    // Clamp value to range
    template<typename T>
    constexpr T clamp(T value, T min_val, T max_val) {
        return value < min_val ? min_val : (value > max_val ? max_val : value);
    }
    
    // Linear interpolation
    template<typename T>
    constexpr T lerp(T a, T b, float t) {
        return a + (b - a) * t;
    }
    
    // Map value from one range to another
    template<typename T>
    constexpr T map(T value, T in_min, T in_max, T out_min, T out_max) {
        return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
    }
    
    // Fast sine/cosine using lookup table (for embedded systems)
    class TrigLUT {
    private:
        static constexpr size_t TABLE_SIZE = 256;
        static constexpr float SCALE = TABLE_SIZE / (2.0f * M_PI);
        
        // Sine lookup table will be implemented later
        static int16_t getSineValue(uint8_t index);
        
    public:
        // Fast sine (-32767 to 32767 for input 0 to 2π)
        static int16_t sin(uint16_t angle) {
            // Simple implementation using standard library for now
            float rad = (float)angle * 2.0f * M_PI / 256.0f;
            return (int16_t)(32767.0f * std::sin(rad));
        }
        
        // Fast cosine
        static int16_t cos(uint16_t angle) {
            float rad = (float)angle * 2.0f * M_PI / 256.0f;
            return (int16_t)(32767.0f * std::cos(rad));
        }
        
        // Convert float angle to lookup table index
        static uint16_t angleToIndex(float radians) {
            float normalized = radians * SCALE;
            return static_cast<uint16_t>(normalized) & 0xFF;
        }
    };
    
    // Distance calculation
    inline uint16_t distance(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int32_t dx = x2 - x1;
        int32_t dy = y2 - y1;
        return isqrt(dx * dx + dy * dy);
    }
    
    // 2D vector operations
    struct Vector2 {
        float x, y;
        
        Vector2() : x(0), y(0) {}
        Vector2(float x_, float y_) : x(x_), y(y_) {}
        
        Vector2 operator+(const Vector2& other) const {
            return Vector2(x + other.x, y + other.y);
        }
        
        Vector2 operator-(const Vector2& other) const {
            return Vector2(x - other.x, y - other.y);
        }
        
        Vector2 operator*(float scalar) const {
            return Vector2(x * scalar, y * scalar);
        }
        
        float length() const {
            return sqrt(x * x + y * y);
        }
        
        Vector2 normalized() const {
            float len = length();
            return len > 0 ? Vector2(x / len, y / len) : Vector2(0, 0);
        }
        
        float dot(const Vector2& other) const {
            return x * other.x + y * other.y;
        }
    };
    
} // namespace math

} // namespace enjin2