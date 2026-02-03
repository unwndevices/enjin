#pragma once

#include "canvas.hpp"

namespace enjin2
{

/**
 * @file canvas_extended.hpp
 * @brief Extended drawing operations for canvases
 *
 * Provides advanced drawing primitives like lines, circles,
 * triangles, and ellipses using optimized algorithms.
 */

/**
 * @brief Extended canvas functionality for advanced graphics operations
 * @tparam TCanvas Canvas type that implements ICanvas<TPixel>
 */
template<typename TCanvas>
class CanvasExtended {
public:
    using PixelType = typename TCanvas::PixelType;

    /**
     * @brief Draw a line using Bresenham's algorithm
     * @param canvas Target canvas
     * @param x0 Starting X coordinate
     * @param y0 Starting Y coordinate
     * @param x1 Ending X coordinate
     * @param y1 Ending Y coordinate
     * @param color Line color
     */
    static void drawLine(TCanvas& canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, PixelType color) {
        int16_t dx = abs(x1 - x0);
        int16_t dy = abs(y1 - y0);
        int16_t sx = x0 < x1 ? 1 : -1;
        int16_t sy = y0 < y1 ? 1 : -1;
        int16_t err = dx - dy;

        while (true) {
            canvas.setPixel(x0, y0, color);

            if (x0 == x1 && y0 == y1) break;

            int16_t e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    /**
     * @brief Draw a horizontal line
     * @param canvas Target canvas
     * @param x Starting X coordinate
     * @param y Y coordinate
     * @param w Line width in pixels
     * @param color Line color
     */
    static void drawHLine(TCanvas& canvas, int16_t x, int16_t y, int16_t w, PixelType color) {
        for (int16_t i = 0; i < w; i++) {
            canvas.setPixel(x + i, y, color);
        }
    }

    /**
     * @brief Draw a vertical line
     * @param canvas Target canvas
     * @param x X coordinate
     * @param y Starting Y coordinate
     * @param h Line height in pixels
     * @param color Line color
     */
    static void drawVLine(TCanvas& canvas, int16_t x, int16_t y, int16_t h, PixelType color) {
        for (int16_t i = 0; i < h; i++) {
            canvas.setPixel(x, y + i, color);
        }
    }

    /**
     * @brief Draw a rectangle outline
     * @param canvas Target canvas
     * @param x X coordinate of top-left corner
     * @param y Y coordinate of top-left corner
     * @param w Width in pixels
     * @param h Height in pixels
     * @param color Rectangle outline color
     */
    static void drawRect(TCanvas& canvas, int16_t x, int16_t y, int16_t w, int16_t h, PixelType color) {
        drawHLine(canvas, x, y, w, color);
        drawHLine(canvas, x, y + h - 1, w, color);
        drawVLine(canvas, x, y, h, color);
        drawVLine(canvas, x + w - 1, y, h, color);
    }

    /**
     * @brief Fill a rectangle
     * @param canvas Target canvas
     * @param x X coordinate of top-left corner
     * @param y Y coordinate of top-left corner
     * @param w Width in pixels
     * @param h Height in pixels
     * @param color Fill color
     */
    static void fillRect(TCanvas& canvas, int16_t x, int16_t y, int16_t w, int16_t h, PixelType color) {
        for (int16_t j = 0; j < h; j++) {
            drawHLine(canvas, x, y + j, w, color);
        }
    }

    /**
     * @brief Draw a circle using midpoint circle algorithm
     * @param canvas Target canvas
     * @param x0 Circle center X coordinate
     * @param y0 Circle center Y coordinate
     * @param r Circle radius in pixels
     * @param color Circle outline color
     */
    static void drawCircle(TCanvas& canvas, int16_t x0, int16_t y0, int16_t r, PixelType color) {
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;

        canvas.setPixel(x0, y0 + r, color);
        canvas.setPixel(x0, y0 - r, color);
        canvas.setPixel(x0 + r, y0, color);
        canvas.setPixel(x0 - r, y0, color);

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;

            canvas.setPixel(x0 + x, y0 + y, color);
            canvas.setPixel(x0 - x, y0 + y, color);
            canvas.setPixel(x0 + x, y0 - y, color);
            canvas.setPixel(x0 - x, y0 - y, color);
            canvas.setPixel(x0 + y, y0 + x, color);
            canvas.setPixel(x0 - y, y0 + x, color);
            canvas.setPixel(x0 + y, y0 - x, color);
            canvas.setPixel(x0 - y, y0 - x, color);
        }
    }

    /**
     * @brief Fill a circle
     */
    static void fillCircle(TCanvas& canvas, int16_t x0, int16_t y0, int16_t r, PixelType color) {
        drawVLine(canvas, x0, y0 - r, 2 * r + 1, color);
        fillCircleHelper(canvas, x0, y0, r, 3, 0, color);
    }

    /**
     * @brief Circle helper for fill operations
     */
    static void fillCircleHelper(TCanvas& canvas, int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, PixelType color) {
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;
        int16_t px = x;
        int16_t py = y;

        delta++; // Avoid some +1's in the loop

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;
            
            if (x < (y + 1)) {
                if (corners & 1) drawVLine(canvas, x0 + x, y0 - y, 2 * y + delta, color);
                if (corners & 2) drawVLine(canvas, x0 - x, y0 - y, 2 * y + delta, color);
            }
            if (y != py) {
                if (corners & 1) drawVLine(canvas, x0 + py, y0 - px, 2 * px + delta, color);
                if (corners & 2) drawVLine(canvas, x0 - py, y0 - px, 2 * px + delta, color);
                py = y;
            }
            px = x;
        }
    }

    /**
     * @brief Draw circle helper for rounded rectangles
     */
    static void drawCircleHelper(TCanvas& canvas, int16_t x0, int16_t y0, int16_t r, uint8_t cornername, PixelType color) {
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;
            
            if (cornername & 0x4) {
                canvas.setPixel(x0 + x, y0 + y, color);
                canvas.setPixel(x0 + y, y0 + x, color);
            }
            if (cornername & 0x2) {
                canvas.setPixel(x0 + x, y0 - y, color);
                canvas.setPixel(x0 + y, y0 - x, color);
            }
            if (cornername & 0x8) {
                canvas.setPixel(x0 - y, y0 + x, color);
                canvas.setPixel(x0 - x, y0 + y, color);
            }
            if (cornername & 0x1) {
                canvas.setPixel(x0 - y, y0 - x, color);
                canvas.setPixel(x0 - x, y0 - y, color);
            }
        }
    }

    /**
     * @brief Draw a triangle outline
     */
    static void drawTriangle(TCanvas& canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, PixelType color) {
        drawLine(canvas, x0, y0, x1, y1, color);
        drawLine(canvas, x1, y1, x2, y2, color);
        drawLine(canvas, x2, y2, x0, y0, color);
    }

    /**
     * @brief Fill a triangle with scanline algorithm
     */
    static void fillTriangle(TCanvas& canvas, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, PixelType color) {
        // Sort vertices by y-coordinate
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
        if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
        
        // Fill triangle using scanline algorithm
        auto drawScanline = [&](int16_t y, int16_t x_left, int16_t x_right) {
            if (x_left > x_right) std::swap(x_left, x_right);
            drawHLine(canvas, x_left, y, x_right - x_left + 1, color);
        };
        
        // Top part of triangle
        for (int16_t y = y0; y <= y1; y++) {
            if (y1 != y0) {
                int16_t x_left = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
                int16_t x_right = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
                drawScanline(y, x_left, x_right);
            }
        }
        
        // Bottom part of triangle
        for (int16_t y = y1; y <= y2; y++) {
            if (y2 != y1) {
                int16_t x_left = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
                int16_t x_right = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
                drawScanline(y, x_left, x_right);
            }
        }
    }

    /**
     * @brief Draw a rounded rectangle
     */
    static void drawRoundRect(TCanvas& canvas, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, PixelType color) {
        // Draw corners as quarter circles
        drawCircleHelper(canvas, x + radius, y + radius, radius, 1, color);
        drawCircleHelper(canvas, x + w - radius - 1, y + radius, radius, 2, color);
        drawCircleHelper(canvas, x + w - radius - 1, y + h - radius - 1, radius, 4, color);
        drawCircleHelper(canvas, x + radius, y + h - radius - 1, radius, 8, color);
        
        // Draw straight edges
        drawHLine(canvas, x + radius, y, w - 2 * radius, color);
        drawHLine(canvas, x + radius, y + h - 1, w - 2 * radius, color);
        drawVLine(canvas, x, y + radius, h - 2 * radius, color);
        drawVLine(canvas, x + w - 1, y + radius, h - 2 * radius, color);
    }

    /**
     * @brief Fill a rounded rectangle
     */
    static void fillRoundRect(TCanvas& canvas, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, PixelType color) {
        // Fill center rectangle
        fillRect(canvas, x + radius, y, w - 2 * radius, h, color);
        
        // Fill corner quarter circles
        fillCircleHelper(canvas, x + radius, y + radius, radius, 1, 0, color);
        fillCircleHelper(canvas, x + w - radius - 1, y + radius, radius, 2, 0, color);
        fillCircleHelper(canvas, x + w - radius - 1, y + h - radius - 1, radius, 4, 0, color);
        fillCircleHelper(canvas, x + radius, y + h - radius - 1, radius, 8, 0, color);
        
        // Fill side rectangles
        fillRect(canvas, x, y + radius, radius, h - 2 * radius, color);
        fillRect(canvas, x + w - radius, y + radius, radius, h - 2 * radius, color);
    }

    /**
     * @brief Draw bitmap at specified location
     */
    static void drawBitmap(TCanvas& canvas, int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, PixelType color) {
        for (int16_t j = 0; j < h; j++) {
            for (int16_t i = 0; i < w; i++) {
                if (bitmap[j * ((w + 7) / 8) + i / 8] & (128 >> (i & 7))) {
                    canvas.setPixel(x + i, y + j, color);
                }
            }
        }
    }

    /**
     * @brief Draw grayscale bitmap (Enjin-style)
     */
    static void drawGrayscaleBitmap(TCanvas& canvas, int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h) {
        for (int16_t j = 0; j < h; j++) {
            for (int16_t i = 0; i < w; i++) {
                uint8_t pixel_value = bitmap[j * w + i];
                if (pixel_value != 16) { // 16 is transparent in Enjin
                    canvas.setPixel(x + i, y + j, static_cast<PixelType>(pixel_value));
                }
            }
        }
    }

    /**
     * @brief Draw grayscale bitmap with mask
     */
    static void drawGrayscaleBitmap(TCanvas& canvas, int16_t x, int16_t y, const uint8_t* bitmap, const uint8_t* mask, int16_t w, int16_t h) {
        for (int16_t j = 0; j < h; j++) {
            for (int16_t i = 0; i < w; i++) {
                if (mask[j * w + i]) {
                    uint8_t pixel_value = bitmap[j * w + i];
                    if (pixel_value != 16) { // 16 is transparent in Enjin
                        canvas.setPixel(x + i, y + j, static_cast<PixelType>(pixel_value));
                    }
                }
            }
        }
    }

    /**
     * @brief Blit (copy) from one canvas to another
     */
    template<typename TSrcCanvas>
    static void blit(TCanvas& dst, const TSrcCanvas& src, int16_t dx, int16_t dy) {
        blit(dst, src, dx, dy, src.getWidth(), src.getHeight(), 0, 0);
    }

    /**
     * @brief Blit with specified dimensions and source offset
     */
    template<typename TSrcCanvas>
    static void blit(TCanvas& dst, const TSrcCanvas& src, int16_t dx, int16_t dy, int16_t w, int16_t h, int16_t sx = 0, int16_t sy = 0) {
        for (int16_t y = 0; y < h; y++) {
            for (int16_t x = 0; x < w; x++) {
                auto pixel = src.getPixel(sx + x, sy + y);
                dst.setPixel(dx + x, dy + y, pixel);
            }
        }
    }
};

} // namespace enjin2