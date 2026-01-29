#ifndef ENJIN2_UTILS_DRAWING_HELPERS_HPP
#define ENJIN2_UTILS_DRAWING_HELPERS_HPP

#include "../graphics/canvas.hpp"
#include "../core/types.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief Drawing helper utilities for advanced shapes and effects
 * 
 * Provides optimized drawing functions for complex shapes that aren't
 * covered by the basic canvas primitives.
 */
namespace DrawingHelpers {

/**
 * @brief Draw a circle with a stroke of specified width (matches original Enjin)
 * @param canvas The canvas to draw on
 * @param x0 The x-coordinate of the center of the circle
 * @param y0 The y-coordinate of the center of the circle
 * @param radius The radius of the circle
 * @param color The color of the stroke
 * @param strokeWidth The width of the stroke (will be rounded down to nearest odd number)
 */
void drawCircleStroke(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0, int16_t radius,
                      uint8_t color, uint8_t strokeWidth);

/**
 * @brief Draw an arc with stroke
 * @param canvas The canvas to draw on
 * @param x0 Center X coordinate
 * @param y0 Center Y coordinate
 * @param radius Arc radius
 * @param startAngle Start angle in radians
 * @param endAngle End angle in radians
 * @param color Stroke color
 * @param strokeWidth Stroke width
 */
void drawArcStroke(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0, int16_t radius,
                   float startAngle, float endAngle, uint8_t color, uint8_t strokeWidth);

/**
 * @brief Draw a polygon from a set of points
 * @param canvas The canvas to draw on
 * @param points Array of points defining the polygon
 * @param numPoints Number of points in the array
 * @param color Line color
 * @param filled Whether to fill the polygon
 */
void drawPolygon(ICanvas<uint8_t>& canvas, const Point* points, uint8_t numPoints,
                 uint8_t color, bool filled = false);

/**
 * @brief Draw a rounded rectangle
 * @param canvas The canvas to draw on
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param width Rectangle width
 * @param height Rectangle height
 * @param radius Corner radius
 * @param color Draw color
 * @param filled Whether to fill the rectangle
 */
void drawRoundedRect(ICanvas<uint8_t>& canvas, int16_t x, int16_t y, 
                     uint16_t width, uint16_t height, uint8_t radius,
                     uint8_t color, bool filled = false);

/**
 * @brief Draw a thick line with rounded end caps
 * @param canvas The canvas to draw on
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @param color Line color
 * @param thickness Line thickness
 */
void drawThickLine(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0,
                   int16_t x1, int16_t y1, uint8_t color, uint8_t thickness);

/**
 * @brief Draw a bezier curve
 * @param canvas The canvas to draw on
 * @param x0 Start point X
 * @param y0 Start point Y
 * @param x1 Control point 1 X
 * @param y1 Control point 1 Y
 * @param x2 Control point 2 X
 * @param y2 Control point 2 Y
 * @param x3 End point X
 * @param y3 End point Y
 * @param color Line color
 * @param segments Number of line segments to approximate the curve
 */
void drawBezierCurve(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0,
                     int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                     int16_t x3, int16_t y3, uint8_t color, uint8_t segments = 20);

/**
 * @brief Draw a star shape
 * @param canvas The canvas to draw on
 * @param centerX Center X coordinate
 * @param centerY Center Y coordinate
 * @param outerRadius Outer radius of star points
 * @param innerRadius Inner radius between points
 * @param numPoints Number of star points
 * @param color Draw color
 * @param filled Whether to fill the star
 */
void drawStar(ICanvas<uint8_t>& canvas, int16_t centerX, int16_t centerY,
              uint8_t outerRadius, uint8_t innerRadius, uint8_t numPoints,
              uint8_t color, bool filled = false);

} // namespace DrawingHelpers

} // namespace enjin2

#endif // ENJIN2_UTILS_DRAWING_HELPERS_HPP