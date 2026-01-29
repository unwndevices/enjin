#include "../../include/enjin2/utils/drawing_helpers.hpp"
#include "../../include/enjin2/graphics/canvas_extended.hpp"
#include <algorithm>
#include <cmath>

namespace enjin2 {
namespace DrawingHelpers {

void drawCircleStroke(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0, int16_t radius,
                      uint8_t color, uint8_t strokeWidth) {
    // Ensure stroke width is odd and at least 1 (matches original Enjin)
    int16_t width = (strokeWidth % 2 == 0) ? strokeWidth - 1 : strokeWidth;
    width = (width < 1) ? 1 : width;

    // Calculate the stroke radius (half width)
    int16_t strokeRadius = width / 2;

    // Calculate number of points needed to draw a smooth circle
    // More points for larger radii to avoid gaps
    int16_t numPoints = radius * 8; // Matches original Enjin quality

    // Calculate the step size in radians
    float stepSize = 2.0f * 3.14159f / numPoints;

    // Draw filled circles along the path of the main circle
    for (int16_t i = 0; i < numPoints; i++) {
        float angle = i * stepSize;
        // Explicitly round coordinates to nearest integer to fix alignment
        int16_t x = static_cast<int16_t>(roundf(x0 + radius * cosf(angle)));
        int16_t y = static_cast<int16_t>(roundf(y0 + radius * sinf(angle)));

        // For performance, we can skip some points if they're close enough
        // But for thin strokes, we need every point
        if (width <= 3 || i % (width / 2 + 1) == 0) {
            CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x, y, strokeRadius, color);
        }
    }
}

void drawArcStroke(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0, int16_t radius,
                   float startAngle, float endAngle, uint8_t color, uint8_t strokeWidth) {
    // Ensure stroke width is valid
    int16_t width = (strokeWidth % 2 == 0) ? strokeWidth - 1 : strokeWidth;
    width = (width < 1) ? 1 : width;
    int16_t strokeRadius = width / 2;

    // Normalize angles
    while (endAngle < startAngle) endAngle += 2.0f * 3.14159f;
    
    float arcLength = endAngle - startAngle;
    int16_t numPoints = static_cast<int16_t>(radius * arcLength / 0.1f); // Adaptive point count
    if (numPoints < 2) numPoints = 2;

    float stepSize = arcLength / (numPoints - 1);

    for (int16_t i = 0; i < numPoints; i++) {
        float angle = startAngle + i * stepSize;
        int16_t x = static_cast<int16_t>(roundf(x0 + radius * cosf(angle)));
        int16_t y = static_cast<int16_t>(roundf(y0 + radius * sinf(angle)));

        CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x, y, strokeRadius, color);
    }
}

void drawPolygon(ICanvas<uint8_t>& canvas, const Point* points, uint8_t numPoints,
                 uint8_t color, bool filled) {
    if (numPoints < 3) return;

    if (filled) {
        // Simple scanline fill algorithm
        // Find bounding box
        int16_t minY = points[0].y, maxY = points[0].y;
        for (uint8_t i = 1; i < numPoints; i++) {
            minY = std::min(minY, points[i].y);
            maxY = std::max(maxY, points[i].y);
        }

        // For each scanline
        for (int16_t y = minY; y <= maxY; y++) {
            int16_t intersections[32]; // Max 32 intersections
            uint8_t intersectionCount = 0;

            // Find intersections with polygon edges
            for (uint8_t i = 0; i < numPoints; i++) {
                uint8_t next = (i + 1) % numPoints;
                Point p1 = points[i];
                Point p2 = points[next];

                if ((p1.y <= y && p2.y > y) || (p2.y <= y && p1.y > y)) {
                    // Edge crosses scanline
                    if (p2.y != p1.y) {
                        int16_t x = p1.x + (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
                        if (intersectionCount < 32) {
                            intersections[intersectionCount++] = x;
                        }
                    }
                }
            }

            // Sort intersections
            for (uint8_t i = 0; i < intersectionCount - 1; i++) {
                for (uint8_t j = i + 1; j < intersectionCount; j++) {
                    if (intersections[i] > intersections[j]) {
                        int16_t temp = intersections[i];
                        intersections[i] = intersections[j];
                        intersections[j] = temp;
                    }
                }
            }

            // Fill between pairs of intersections
            for (uint8_t i = 0; i < intersectionCount; i += 2) {
                if (i + 1 < intersectionCount) {
                    CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas, 
                        intersections[i], y, intersections[i + 1], y, color);
                }
            }
        }
    } else {
        // Draw outline
        for (uint8_t i = 0; i < numPoints; i++) {
            uint8_t next = (i + 1) % numPoints;
            CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas,
                points[i].x, points[i].y, points[next].x, points[next].y, color);
        }
    }
}

void drawRoundedRect(ICanvas<uint8_t>& canvas, int16_t x, int16_t y, 
                     uint16_t width, uint16_t height, uint8_t radius,
                     uint8_t color, bool filled) {
    // Clamp radius to reasonable bounds
    uint8_t maxRadius = std::min(width / 2, height / 2);
    radius = std::min(radius, maxRadius);

    if (filled) {
        // Fill main rectangle areas
        CanvasExtended<ICanvas<uint8_t>>::fillRect(canvas, x + radius, y, width - 2 * radius, height, color);
        CanvasExtended<ICanvas<uint8_t>>::fillRect(canvas, x, y + radius, radius, height - 2 * radius, color);
        CanvasExtended<ICanvas<uint8_t>>::fillRect(canvas, x + width - radius, y + radius, radius, height - 2 * radius, color);

        // Fill corner circles
        CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x + radius, y + radius, radius, color);
        CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x + width - radius - 1, y + radius, radius, color);
        CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x + radius, y + height - radius - 1, radius, color);
        CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x + width - radius - 1, y + height - radius - 1, radius, color);
    } else {
        // Draw straight edges
        CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas, x + radius, y, x + width - radius - 1, y, color); // Top
        CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas, x + radius, y + height - 1, x + width - radius - 1, y + height - 1, color); // Bottom
        CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas, x, y + radius, x, y + height - radius - 1, color); // Left
        CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas, x + width - 1, y + radius, x + width - 1, y + height - radius - 1, color); // Right

        // Draw corner arcs
        drawArcStroke(canvas, x + radius, y + radius, radius, 3.14159f, 3.14159f * 1.5f, color, 1);
        drawArcStroke(canvas, x + width - radius - 1, y + radius, radius, 3.14159f * 1.5f, 2.0f * 3.14159f, color, 1);
        drawArcStroke(canvas, x + radius, y + height - radius - 1, radius, 3.14159f * 0.5f, 3.14159f, color, 1);
        drawArcStroke(canvas, x + width - radius - 1, y + height - radius - 1, radius, 0.0f, 3.14159f * 0.5f, color, 1);
    }
}

void drawThickLine(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0,
                   int16_t x1, int16_t y1, uint8_t color, uint8_t thickness) {
    if (thickness <= 1) {
        CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas, x0, y0, x1, y1, color);
        return;
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float length = sqrtf(dx * dx + dy * dy);
    
    if (length == 0) {
        CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x0, y0, thickness / 2, color);
        return;
    }

    // Normalize direction vector
    float nx = dx / length;
    float ny = dy / length;

    // Perpendicular vector for thickness
    float px = -ny * (thickness / 2.0f);
    float py = nx * (thickness / 2.0f);

    // Draw thick line as a filled quadrilateral
    Point quad[4] = {
        {static_cast<int16_t>(x0 + px), static_cast<int16_t>(y0 + py)},
        {static_cast<int16_t>(x0 - px), static_cast<int16_t>(y0 - py)},
        {static_cast<int16_t>(x1 - px), static_cast<int16_t>(y1 - py)},
        {static_cast<int16_t>(x1 + px), static_cast<int16_t>(y1 + py)}
    };

    drawPolygon(canvas, quad, 4, color, true);

    // Add rounded end caps
    CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x0, y0, thickness / 2, color);
    CanvasExtended<ICanvas<uint8_t>>::fillCircle(canvas, x1, y1, thickness / 2, color);
}

void drawBezierCurve(ICanvas<uint8_t>& canvas, int16_t x0, int16_t y0,
                     int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                     int16_t x3, int16_t y3, uint8_t color, uint8_t segments) {
    int16_t prevX = x0, prevY = y0;

    for (uint8_t i = 1; i <= segments; i++) {
        float t = static_cast<float>(i) / segments;
        float t2 = t * t;
        float t3 = t2 * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        float mt3 = mt2 * mt;

        // Cubic Bezier formula
        int16_t x = static_cast<int16_t>(mt3 * x0 + 3 * mt2 * t * x1 + 3 * mt * t2 * x2 + t3 * x3);
        int16_t y = static_cast<int16_t>(mt3 * y0 + 3 * mt2 * t * y1 + 3 * mt * t2 * y2 + t3 * y3);

        CanvasExtended<ICanvas<uint8_t>>::drawLine(canvas, prevX, prevY, x, y, color);
        prevX = x;
        prevY = y;
    }
}

void drawStar(ICanvas<uint8_t>& canvas, int16_t centerX, int16_t centerY,
              uint8_t outerRadius, uint8_t innerRadius, uint8_t numPoints,
              uint8_t color, bool filled) {
    if (numPoints < 3) return;

    Point points[20]; // Max 10 star points = 20 vertices
    uint8_t totalPoints = numPoints * 2;
    if (totalPoints > 20) totalPoints = 20;

    float angleStep = 2.0f * 3.14159f / totalPoints;

    for (uint8_t i = 0; i < totalPoints; i++) {
        float angle = i * angleStep;
        uint8_t radius = (i % 2 == 0) ? outerRadius : innerRadius;

        points[i].x = centerX + static_cast<int16_t>(radius * cosf(angle));
        points[i].y = centerY + static_cast<int16_t>(radius * sinf(angle));
    }

    drawPolygon(canvas, points, totalPoints, color, filled);
}

} // namespace DrawingHelpers
} // namespace enjin2