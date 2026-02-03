/**
 * @file polar.hpp
 * @brief Polar coordinate utilities for circular UI elements and orbital motion
 *
 * Provides functions for converting between polar and cartesian coordinates,
 * calculating angles and distances, and circle/ellipse calculations.
 */
#ifndef ENJIN2_UTILS_POLAR_HPP
#define ENJIN2_UTILS_POLAR_HPP

#include "../core/types.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief Polar coordinate utilities (matches original Enjin Polar utilities)
 * 
 * Provides functions for converting between polar and cartesian coordinates,
 * useful for circular UI elements and orbital motion.
 */
namespace Polar {

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

/**
 * @brief Convert radial coordinates to cartesian coordinates (matches original Enjin)
 * @param phase Phase value from 0.0 to 1.0 (0.0 = 0 degrees, 1.0 = 360 degrees)
 * @param radius Distance from center
 * @param center Center point for conversion (default is 63,63 for 128x128 displays)
 * @return Cartesian coordinates as Point
 */
Point RadialToCartesian(float phase, uint8_t radius, Point center = Point(63, 63));

/**
 * @brief Convert cartesian coordinates to polar coordinates
 * @param point Cartesian point
 * @param center Center point for conversion
 * @param phase Output phase value (0.0 to 1.0)
 * @param radius Output radius value
 */
void CartesianToRadial(Point point, Point center, float& phase, uint8_t& radius);

/**
 * @brief Get a point on a circle at specified angle and radius
 * @param centerX Center X coordinate
 * @param centerY Center Y coordinate
 * @param angle Angle in radians
 * @param radius Distance from center
 * @return Point on the circle
 */
Point GetCirclePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radius);

/**
 * @brief Get a point on an ellipse
 * @param centerX Center X coordinate
 * @param centerY Center Y coordinate
 * @param angle Angle in radians
 * @param radiusX Horizontal radius
 * @param radiusY Vertical radius
 * @return Point on the ellipse
 */
Point GetEllipsePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radiusX, uint8_t radiusY);

/**
 * @brief Calculate distance between two points
 * @param p1 First point
 * @param p2 Second point
 * @return Distance between points
 */
float CalculateDistance(Point p1, Point p2);

/**
 * @brief Calculate angle between two points
 * @param p1 First point (usually center)
 * @param p2 Second point
 * @return Angle in radians
 */
float CalculateAngle(Point p1, Point p2);

/**
 * @brief Normalize phase value to 0.0-1.0 range
 * @param phase Phase value to normalize
 * @return Normalized phase (0.0 to 1.0)
 */
float NormalizePhase(float phase);

/**
 * @brief Convert phase (0.0-1.0) to radians
 * @param phase Phase value
 * @return Angle in radians
 */
float PhaseToRadians(float phase);

/**
 * @brief Convert radians to phase (0.0-1.0)
 * @param radians Angle in radians
 * @return Phase value
 */
float RadiansToPhase(float radians);

} // namespace Polar

} // namespace enjin2

#endif // ENJIN2_UTILS_POLAR_HPP