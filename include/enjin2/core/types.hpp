#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "../graphics/palette.hpp"

/**
 * @brief Pixel4's debug check (ADR-0019): 1 makes `Pixel4(v)` assert that `v` is a
 * Palette role (0-7) or transparent (15).
 *
 * On by default in debug host builds (native tests, a debug WASM build); off when
 * NDEBUG is set and on the device, where it would sit in every pixel write.
 * Define it to 0 or 1 to override.
 */
#ifndef ENJIN2_PIXEL4_ASSERT
#  if !defined(NDEBUG) && !defined(ESP_PLATFORM)
#    define ENJIN2_PIXEL4_ASSERT 1
#  else
#    define ENJIN2_PIXEL4_ASSERT 0
#  endif
#endif

namespace enjin2 {

/**
 * @brief 2D point with integer coordinates
 */
struct Point {
    int16_t x;  ///< X coordinate
    int16_t y;  ///< Y coordinate
    
    /** @brief Default constructor initializes to origin (0,0) */
    Point() : x(0), y(0) {}
    
    /** 
     * @brief Constructor with coordinates
     * @param x_ X coordinate
     * @param y_ Y coordinate
     */
    Point(int16_t x_, int16_t y_) : x(x_), y(y_) {}
    
    /**
     * @brief Addition operator
     * @param other Point to add
     * @return Sum of both points
     */
    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y);
    }

    /**
     * @brief Subtraction operator
     * @param other Point to subtract
     * @return Difference of both points
     */
    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y);
    }

    /**
     * @brief Addition assignment operator
     * @param other Point to add
     * @return Reference to this point
     */
    Point& operator+=(const Point& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    /**
     * @brief Subtraction assignment operator
     * @param other Point to subtract
     * @return Reference to this point
     */
    Point& operator-=(const Point& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
};

/**
 * @brief 2D vector with floating-point components
 *
 * Provides vector operations for 2D graphics and physics.
 */
struct Vec2 {
    float x; ///< X component
    float y; ///< Y component

    /** @brief Default constructor initializes to zero vector */
    Vec2() : x(0), y(0) {}
    /**
     * @brief Constructor with components
     * @param x_ X component
     * @param y_ Y component
     */
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    /** @brief Component-wise addition
     *  @param other Vector to add
     *  @return Sum vector */
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    /** @brief Component-wise subtraction
     *  @param other Vector to subtract
     *  @return Difference vector */
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    /** @brief Scalar multiplication
     *  @param s Scalar factor
     *  @return Scaled vector */
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    /** @brief Scalar division
     *  @param s Scalar divisor
     *  @return Scaled vector */
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
    /** @brief Unary negation
     *  @return Negated vector */
    Vec2 operator-() const { return Vec2(-x, -y); }

    /** @brief In-place addition
     *  @param other Vector to add
     *  @return Reference to this */
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    /** @brief In-place subtraction
     *  @param other Vector to subtract
     *  @return Reference to this */
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    /** @brief In-place scalar multiplication
     *  @param s Scalar factor
     *  @return Reference to this */
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    /** @brief In-place scalar division
     *  @param s Scalar divisor
     *  @return Reference to this */
    Vec2& operator/=(float s) { x /= s; y /= s; return *this; }

    /** @brief Equality comparison
     *  @param other Vector to compare
     *  @return true if both components match */
    bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
    /** @brief Inequality comparison
     *  @param other Vector to compare
     *  @return true if any component differs */
    bool operator!=(const Vec2& other) const { return !(*this == other); }

    /** @brief Euclidean length of this vector
     *  @return sqrt(x² + y²) */
    float length() const { return std::sqrt(x * x + y * y); }
    /** @brief Squared length (avoids sqrt)
     *  @return x² + y² */
    float lengthSquared() const { return x * x + y * y; }
    /** @brief Dot product with another vector
     *  @param other Vector to dot with
     *  @return Dot product scalar */
    float dot(const Vec2& other) const { return x * other.x + y * other.y; }

    /** @brief 2D cross product (z-component of the 3D cross)
     *  @param other Vector to cross with
     *  @return Cross product scalar */
    float cross(const Vec2& other) const { return x * other.y - y * other.x; }

    /** @brief Angle of this vector in radians via atan2(y, x)
     *  @return Angle in radians */
    float angle() const { return std::atan2(y, x); }

    /** @brief Return a unit-length copy (zero vector if length is 0)
     *  @return Normalized vector */
    Vec2 normalized() const {
        float len = length();
        return len > 0 ? Vec2(x / len, y / len) : Vec2(0, 0);
    }

    /** @brief Return a copy rotated by the given angle
     *  @param radians Rotation angle in radians
     *  @return Rotated vector */
    Vec2 rotated(float radians) const {
        float c = std::cos(radians);
        float s = std::sin(radians);
        return Vec2(x * c - y * s, x * s + y * c);
    }

    /** @brief Euclidean distance between two points
     *  @param a First point
     *  @param b Second point
     *  @return Distance */
    static float distance(const Vec2& a, const Vec2& b) { return (a - b).length(); }
};

/**
 * @brief 2D size with width and height
 */
struct Size {
    uint16_t width;  ///< Width dimension
    uint16_t height; ///< Height dimension
    
    /** @brief Default constructor initializes to zero size */
    Size() : width(0), height(0) {}
    
    /**
     * @brief Constructor with dimensions
     * @param w Width
     * @param h Height
     */
    Size(uint16_t w, uint16_t h) : width(w), height(h) {}
};

/**
 * @brief Rectangle defined by position and size
 */
struct Rect {
    int16_t x;              ///< Top-left X coordinate
    int16_t y;              ///< Top-left Y coordinate
    uint16_t width;         ///< Rectangle width
    uint16_t height;        ///< Rectangle height
    
    /** @brief Default constructor initializes to empty rectangle at origin */
    Rect() : x(0), y(0), width(0), height(0) {}
    
    /**
     * @brief Constructor with position and size
     * @param x_ X coordinate of top-left corner
     * @param y_ Y coordinate of top-left corner
     * @param w Width of rectangle
     * @param h Height of rectangle
     */
    Rect(int16_t x_, int16_t y_, uint16_t w, uint16_t h) 
        : x(x_), y(y_), width(w), height(h) {}
    
    /**
     * @brief Check if a point is inside the rectangle
     * @param px X coordinate of point to test
     * @param py Y coordinate of point to test
     * @return true if point is inside rectangle, false otherwise
     */
    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
    
    /**
     * @brief Check if this rectangle intersects with another
     * @param other Rectangle to test intersection with
     * @return true if rectangles intersect, false otherwise
     */
    bool intersects(const Rect& other) const {
        return !(x >= other.x + other.width || 
                 other.x >= x + width ||
                 y >= other.y + other.height ||
                 other.y >= y + height);
    }
};

/**
 * @brief 4-bit pixel type for memory-efficient graphics
 * 
 * Holds a palette index in 4 bits of storage: a Palette role (0-7) or
 * transparent (15). 8-14 are invalid (ADR-0019); constructing one asserts in
 * debug builds (ENJIN2_PIXEL4_ASSERT).
 */
struct Pixel4 {
    uint8_t value : 4; ///< Palette index: a role 0-7, or 15 = transparent
    
    /** @brief Default constructor initializes to bg (value 0) */
    constexpr Pixel4() : value(0) {}
    
    /**
     * @brief Constructor from a palette index
     * @param v A Palette role (0-7) or PALETTE_TRANSPARENT (15); stored in the
     *          low 4 bits. Anything else asserts in debug builds.
     */
    constexpr Pixel4(uint8_t v) : value(v & 0x0F) {
#if ENJIN2_PIXEL4_ASSERT
        assert(isPaletteIndex(v) && "Pixel4: not a Palette role (0-7) or transparent (15)");
#endif
    }
    
    /**
     * @brief Implicit conversion to uint8_t
     * @return 4-bit value as uint8_t
     */
    constexpr operator uint8_t() const { return value; }
    
    /**
     * @brief Convert to 8-bit grayscale value
     * @return 8-bit equivalent (0-255 range)
     */
    constexpr uint8_t to8bit() const { return value * 17; } // 0-15 -> 0-255
    
    /**
     * @brief Create Pixel4 from 8-bit grayscale value
     * @param gray 8-bit grayscale value (0-255)
     * @return Pixel4 with equivalent 4-bit value
     */
    static constexpr Pixel4 from8bit(uint8_t gray) { return Pixel4(gray >> 4); }
};

/**
 * @brief Packed storage for two 4-bit pixels in a single byte
 * 
 * Efficiently stores two Pixel4 values in one byte, achieving 50% memory
 * savings compared to storing each pixel in a separate byte.
 */
class PackedPixel4 {
private:
    uint8_t data; ///< Packed data containing two 4-bit pixels
    
public:
    /** @brief Default constructor initializes both pixels to 0 */
    PackedPixel4() : data(0) {}
    
    /**
     * @brief Constructor from raw byte data
     * @param raw Raw byte containing packed pixel data
     */
    PackedPixel4(uint8_t raw) : data(raw) {}
    
    /**
     * @brief Get the low nibble pixel (bits 0-3)
     * @return Pixel stored in lower 4 bits
     */
    Pixel4 getLow() const { return Pixel4(data & 0x0F); }
    
    /**
     * @brief Get the high nibble pixel (bits 4-7)
     * @return Pixel stored in upper 4 bits
     */
    Pixel4 getHigh() const { return Pixel4((data >> 4) & 0x0F); }
    
    /**
     * @brief Set the low nibble pixel (bits 0-3)
     * @param pixel Pixel value to store in lower 4 bits
     */
    void setLow(Pixel4 pixel) { data = (data & 0xF0) | pixel.value; }
    
    /**
     * @brief Set the high nibble pixel (bits 4-7)
     * @param pixel Pixel value to store in upper 4 bits
     */
    void setHigh(Pixel4 pixel) { data = (data & 0x0F) | (pixel.value << 4); }
    
    /**
     * @brief Get raw byte containing both pixels
     * @return Raw byte data
     */
    uint8_t getByte() const { return data; }
};

} // namespace enjin2