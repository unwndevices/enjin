#include "DrawingHelpers.hpp"
#include <algorithm>
#include <cmath>

namespace enjin
{
    namespace
    {

        // Helper function to draw a filled circle at a point along the circle path
        // Uses partial circle drawing to optimize when possible
        void drawStrokePoint(EiseiCanvas *canvas, int16_t centerX, int16_t centerY, int16_t x, int16_t y,
                             int16_t strokeRadius, uint8_t color)
        {
            // Draw a filled circle with strokeRadius at position x,y
            canvas->fillCircle(x, y, strokeRadius, color);
        }
    }

    void drawCircleStroke(EiseiCanvas *canvas, int16_t x0, int16_t y0, int16_t radius,
                          uint8_t color, uint8_t strokeWidth)
    {
        // Ensure stroke width is odd and at least 1
        int16_t width = (strokeWidth % 2 == 0) ? strokeWidth - 1 : strokeWidth;
        width = (width < 1) ? 1 : width;

        // Calculate the stroke radius (half width)
        int16_t strokeRadius = width / 2;

        // Calculate number of points needed to draw a smooth circle
        // More points for larger radii to avoid gaps
        int16_t numPoints = radius * 8; // Adjust this value for quality vs performance

        // Calculate the step size in radians
        float stepSize = 2.0f * 3.14159f / numPoints;

        // Draw filled circles along the path of the main circle
        for (int16_t i = 0; i < numPoints; i++)
        {
            float angle = i * stepSize;
            // Explicitly round coordinates to nearest integer to fix alignment
            int16_t x = static_cast<int16_t>(roundf(x0 + radius * cos(angle)));
            int16_t y = static_cast<int16_t>(roundf(y0 + radius * sin(angle)));

            // For performance, we can skip some points if they're close enough
            // But for thin strokes, we need every point
            if (width <= 3 || i % (width / 2 + 1) == 0)
            {
                drawStrokePoint(canvas, x0, y0, x, y, strokeRadius, color);
            }
        }
    }
} // namespace enjin