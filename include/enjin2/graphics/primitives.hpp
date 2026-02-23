#pragma once

#include "../core/types.hpp"
#include "canvas.hpp"
#include <algorithm>
#include <cmath>
namespace enjin2
{

/**
 * @file primitives.hpp
 * @brief Drawing primitives for geometric shapes
 *
 * Provides optimized algorithms for drawing lines, circles,
 * triangles, ellipses, and polygons on any canvas type.
 */

/**
 * @brief Drawing primitives for geometric shapes
 * @tparam TPixel Pixel type (e.g., Pixel4, uint8_t)
 */
template<typename TPixel>
class Primitives {
public:
    /**
     * @brief Draw a line using Bresenham's algorithm
     * @param canvas Target canvas
     * @param x0 Starting X coordinate
     * @param y0 Starting Y coordinate
     * @param x1 Ending X coordinate
     * @param y1 Ending Y coordinate
     * @param color Line color
     */
    static void drawLine(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, TPixel color) {
        int16_t dx = abs(x1 - x0);
        int16_t dy = abs(y1 - y0);
        int16_t sx = x0 < x1 ? 1 : -1;
        int16_t sy = y0 < y1 ? 1 : -1;
        int16_t err = dx - dy;
        
        int16_t x = x0, y = y0;
        
        while (true) {
            canvas.setPixel(x, y, color);
            
            if (x == x1 && y == y1) break;
            
            int16_t e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }
        }
    }
    
    /**
     * @brief Draw rectangle outline
     * @param canvas Target canvas
     * @param rect Rectangle bounds
     * @param color Outline color
     */
    static void drawRect(ICanvas<TPixel>& canvas, const Rect& rect, TPixel color) {
        int16_t x1 = rect.x;
        int16_t y1 = rect.y;
        int16_t x2 = rect.x + rect.width - 1;
        int16_t y2 = rect.y + rect.height - 1;
        
        // Top and bottom
        drawLine(canvas, x1, y1, x2, y1, color);
        drawLine(canvas, x1, y2, x2, y2, color);
        
        // Left and right
        drawLine(canvas, x1, y1, x1, y2, color);
        drawLine(canvas, x2, y1, x2, y2, color);
    }
    
    /**
     * @brief Fill a rectangle
     * @param canvas Target canvas
     * @param rect Rectangle bounds
     * @param color Fill color
     */
    static void fillRect(ICanvas<TPixel>& canvas, const Rect& rect, TPixel color) {
        canvas.fill(rect, color);
    }
    
    /**
     * @brief Draw circle outline using midpoint algorithm
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Circle radius
     * @param color Outline color
     */
    static void drawCircle(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                          int16_t radius, TPixel color) {
        int16_t x = 0;
        int16_t y = radius;
        int16_t d = 1 - radius;
        
        while (x <= y) {
            // 8-way symmetry
            canvas.setPixel(cx + x, cy + y, color);
            canvas.setPixel(cx - x, cy + y, color);
            canvas.setPixel(cx + x, cy - y, color);
            canvas.setPixel(cx - x, cy - y, color);
            canvas.setPixel(cx + y, cy + x, color);
            canvas.setPixel(cx - y, cy + x, color);
            canvas.setPixel(cx + y, cy - x, color);
            canvas.setPixel(cx - y, cy - x, color);
            
            if (d < 0) {
                d += 2 * x + 3;
            } else {
                d += 2 * (x - y) + 5;
                y--;
            }
            x++;
        }
    }
    
    /**
     * @brief Fill circle using scanline algorithm
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     */
    static void fillCircle(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                          int16_t radius, TPixel color) {
        for (int16_t y = -radius; y <= radius; ++y) {
            int32_t y_sq = y * y;
            int32_t radius_sq = radius * radius;
            if (y_sq <= radius_sq) {
                int16_t x_extent = static_cast<int16_t>(std::sqrt(radius_sq - y_sq));
                drawLine(canvas, cx - x_extent, cy + y, cx + x_extent, cy + y, color);
            }
        }
    }
    
    /**
     * @brief Draw triangle outline
     * @param canvas Target canvas
     * @param x0 First vertex X
     * @param y0 First vertex Y
     * @param x1 Second vertex X
     * @param y1 Second vertex Y
     * @param x2 Third vertex X
     * @param y2 Third vertex Y
     * @param color Outline color
     */
    static void drawTriangle(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color) {
        drawLine(canvas, x0, y0, x1, y1, color);
        drawLine(canvas, x1, y1, x2, y2, color);
        drawLine(canvas, x2, y2, x0, y0, color);
    }
    
    /**
     * @brief Fill triangle using scanline algorithm
     * @param canvas Target canvas
     * @param x0 First vertex X
     * @param y0 First vertex Y
     * @param x1 Second vertex X
     * @param y1 Second vertex Y
     * @param x2 Third vertex X
     * @param y2 Third vertex Y
     * @param color Fill color
     */
    static void fillTriangle(ICanvas<TPixel>& canvas, int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1, int16_t x2, int16_t y2, TPixel color) {
        // Sort vertices by y-coordinate
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
        if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
        
        // Avoid division by zero
        int16_t dy02 = y2 - y0;
        int16_t dy01 = y1 - y0;
        int16_t dy12 = y2 - y1;
        
        // Fill upper half
        for (int16_t y = y0; y <= y1; ++y) {
            int16_t xa = (dy02 != 0) ? x0 + (x2 - x0) * (y - y0) / dy02 : x0;
            int16_t xb = (dy01 != 0) ? x0 + (x1 - x0) * (y - y0) / dy01 : x0;
            if (xa > xb) std::swap(xa, xb);
            drawLine(canvas, xa, y, xb, y, color);
        }
        
        // Fill lower half
        for (int16_t y = y1; y <= y2; ++y) {
            int16_t xa = (dy02 != 0) ? x0 + (x2 - x0) * (y - y0) / dy02 : x0;
            int16_t xb = (dy12 != 0) ? x1 + (x2 - x1) * (y - y1) / dy12 : x1;
            if (xa > xb) std::swap(xa, xb);
            drawLine(canvas, xa, y, xb, y, color);
        }
    }
    
    /**
     * @brief Draw ellipse using midpoint algorithm
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param rx Horizontal radius
     * @param ry Vertical radius
     * @param color Outline color
     */
    static void drawEllipse(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                           int16_t rx, int16_t ry, TPixel color) {
        int32_t rx2 = rx * rx;
        int32_t ry2 = ry * ry;
        int32_t two_rx2 = 2 * rx2;
        int32_t two_ry2 = 2 * ry2;
        
        int16_t x = 0;
        int16_t y = ry;
        int32_t px = 0;
        int32_t py = two_rx2 * y;
        
        // Region 1
        int32_t p = ry2 - (rx2 * ry) + (rx2 / 4);
        while (px < py) {
            canvas.setPixel(cx + x, cy + y, color);
            canvas.setPixel(cx - x, cy + y, color);
            canvas.setPixel(cx + x, cy - y, color);
            canvas.setPixel(cx - x, cy - y, color);
            
            x++;
            px += two_ry2;
            if (p < 0) {
                p += ry2 + px;
            } else {
                y--;
                py -= two_rx2;
                p += ry2 + px - py;
            }
        }
        
        // Region 2
        p = ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
        while (y > 0) {
            canvas.setPixel(cx + x, cy + y, color);
            canvas.setPixel(cx - x, cy + y, color);
            canvas.setPixel(cx + x, cy - y, color);
            canvas.setPixel(cx - x, cy - y, color);
            
            y--;
            py -= two_rx2;
            if (p > 0) {
                p += rx2 - py;
            } else {
                x++;
                px += two_ry2;
                p += rx2 - py + px;
            }
        }
    }
    
    /**
     * @brief Draw arc segment
     * @param canvas Target canvas
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Arc radius
     * @param start_angle Start angle in radians
     * @param end_angle End angle in radians
     * @param color Outline color
     */
    static void drawArc(ICanvas<TPixel>& canvas, int16_t cx, int16_t cy,
                       int16_t radius, float start_angle, float end_angle, TPixel color) {
        // Convert angles to 0-2π range
        while (start_angle < 0) start_angle += 2 * M_PI;
        while (end_angle < 0) end_angle += 2 * M_PI;
        while (start_angle >= 2 * M_PI) start_angle -= 2 * M_PI;
        while (end_angle >= 2 * M_PI) end_angle -= 2 * M_PI;
        
        float step = 1.0f / radius; // Adaptive step size
        for (float angle = start_angle; 
             (end_angle > start_angle) ? (angle <= end_angle) : (angle <= end_angle + 2 * M_PI); 
             angle += step) {
            int16_t x = cx + static_cast<int16_t>(radius * cos(angle));
            int16_t y = cy + static_cast<int16_t>(radius * sin(angle));
            canvas.setPixel(x, y, color);
        }
    }
    
    /**
     * @brief Draw polygon outline
     * @param canvas Target canvas
     * @param vertices Array of polygon vertices
     * @param vertex_count Number of vertices
     * @param color Outline color
     */
    static void drawPolygon(ICanvas<TPixel>& canvas, const Point* vertices,
                           size_t vertex_count, TPixel color) {
        if (vertex_count < 3) return;
        
        for (size_t i = 0; i < vertex_count; ++i) {
            size_t next = (i + 1) % vertex_count;
            drawLine(canvas, vertices[i].x, vertices[i].y,
                    vertices[next].x, vertices[next].y, color);
        }
    }
};

/// @brief Drawing primitives for 4-bit pixel type
using Primitives4 = Primitives<Pixel4>;
/// @brief Drawing primitives for 8-bit pixel type
using Primitives8 = Primitives<uint8_t>;

} // namespace enjin2