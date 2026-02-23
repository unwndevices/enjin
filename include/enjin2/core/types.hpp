#pragma once

#include <cstdint>
#include <cstring>

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
 * Represents a pixel value in the range 0-15, using only 4 bits of storage.
 * Provides automatic conversion to/from 8-bit values for compatibility.
 */
struct Pixel4 {
    uint8_t value : 4; ///< 4-bit pixel value (0-15)
    
    /** @brief Default constructor initializes to black (value 0) */
    constexpr Pixel4() : value(0) {}
    
    /**
     * @brief Constructor from 8-bit value
     * @param v Value to convert, automatically clamped to 4-bit range
     */
    constexpr Pixel4(uint8_t v) : value(v & 0x0F) {}
    
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
     * @param byte Raw byte containing packed pixel data
     */
    PackedPixel4(uint8_t byte) : data(byte) {}
    
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

/**
 * @brief Predefined color constants for 4-bit grayscale graphics
 */
namespace Colors {
    constexpr Pixel4 BLACK = Pixel4(0);      ///< Black (value 0)
    constexpr Pixel4 DARK_GRAY = Pixel4(4);  ///< Dark gray (value 4)
    constexpr Pixel4 GRAY = Pixel4(8);       ///< Medium gray (value 8)
    constexpr Pixel4 LIGHT_GRAY = Pixel4(12); ///< Light gray (value 12)
    constexpr Pixel4 WHITE = Pixel4(15);     ///< White (value 15)
}

} // namespace enjin2