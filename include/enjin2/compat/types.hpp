/**
 * @file types.hpp
 * @brief Compatibility layer for enjin1 type aliases
 *
 * Provides Vector3 and other type aliases for enjin1 compatibility.
 * Deprecated after enjin1 deletion.
 */
#pragma once

// Migration support - deprecated after enjin1 deletion

#include "enjin2/core/types.hpp"

namespace enjin {

/// @brief 2D vector alias mapping to enjin2::Point
using Vector2 = enjin2::Point;
/// @brief Size alias mapping to enjin2::Size
using Size = enjin2::Size;

/**
 * @brief 3D vector for compatibility with enjin1
 *
 * enjin1 used Vector3 extensively. enjin2 focuses on 2D graphics,
 * so this is provided as a simple compatibility struct.
 */
struct Vector3 {
    float x; ///< X coordinate
    float y; ///< Y coordinate
    float z; ///< Z coordinate

    /** @brief Default constructor initializes to zero */
    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

    /**
     * @brief Constructor with coordinates
     * @param x_ X coordinate
     * @param y_ Y coordinate
     * @param z_ Z coordinate
     */
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    /**
     * @brief Addition operator
     * @param other Vector to add
     * @return Sum of both vectors
     */
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    /**
     * @brief Subtraction operator
     * @param other Vector to subtract
     * @return Difference of both vectors
     */
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
};

} // namespace enjin
