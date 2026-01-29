#pragma once

#include "../core/types.hpp"
#include "../core/memory.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

// Include GFX font support
#ifndef VCV_RACK
#include <Arduino.h>
#endif
#include "../../Libs/Adafruit-GFX-Library/gfxfont.h"

// Include default font data
#include "defaultfont.hpp"

namespace enjin2
{

    /**
     * @brief Abstract canvas interface for drawing operations
     * @tparam TPixel Pixel type (e.g., Pixel4, uint8_t)
     *
     * Provides a hardware-independent interface for all drawing operations.
     * Concrete implementations handle the actual pixel storage and formatting.
     */
    template <typename TPixel>
    class ICanvas
    {
    public:
        using PixelType = TPixel;
        virtual ~ICanvas() = default;

        /**
         * @brief Get canvas width in pixels
         * @return Width in pixels
         */
        virtual uint16_t getWidth() const = 0;

        /**
         * @brief Get canvas height in pixels
         * @return Height in pixels
         */
        virtual uint16_t getHeight() const = 0;

        /**
         * @brief Set pixel color at specified coordinates
         * @param x X coordinate
         * @param y Y coordinate
         * @param color Pixel color to set
         */
        virtual void setPixel(int16_t x, int16_t y, TPixel color) = 0;

        /**
         * @brief Get pixel color at specified coordinates
         * @param x X coordinate
         * @param y Y coordinate
         * @return Pixel color at the specified location
         */
        virtual TPixel getPixel(int16_t x, int16_t y) const = 0;

        /**
         * @brief Clear entire canvas to specified color
         * @param color Color to fill canvas with (default: black)
         */
        virtual void clear(TPixel color = TPixel(0)) = 0;

        /**
         * @brief Fill rectangular region with specified color
         * @param rect Rectangle to fill
         * @param color Color to fill with
         */
        virtual void fill(const Rect &rect, TPixel color) = 0;

        /**
         * @brief Check if coordinates are within canvas bounds
         * @param x X coordinate to check
         * @param y Y coordinate to check
         * @return true if coordinates are valid, false otherwise
         */
        bool inBounds(int16_t x, int16_t y) const
        {
            return x >= 0 && x < getWidth() && y >= 0 && y < getHeight();
        }

        /**
         * @brief Get canvas bounds as rectangle
         * @return Rectangle representing entire canvas area
         */
        Rect getBounds() const
        {
            return Rect(0, 0, getWidth(), getHeight());
        }
    };

    // 4-bit canvas with packed storage
    template <uint16_t WIDTH, uint16_t HEIGHT>
    class Canvas4 : public ICanvas<Pixel4>
    {
    public:
        using PixelType = Pixel4;
        
    private:
        static_assert(WIDTH % 2 == 0, "Width must be even for packed 4-bit storage");

        static constexpr size_t BUFFER_SIZE = (WIDTH * HEIGHT) / 2;
        PackedPixel4 buffer[BUFFER_SIZE];

        size_t getIndex(int16_t x, int16_t y) const
        {
            return (y * WIDTH + x) / 2;
        }

        bool isLowPixel(int16_t x) const
        {
            return (x % 2) == 0;
        }

    public:
        Canvas4()
        {
            clear();
        }

        uint16_t getWidth() const override { return WIDTH; }
        uint16_t getHeight() const override { return HEIGHT; }

        void setPixel(int16_t x, int16_t y, Pixel4 color) override
        {
            if (!inBounds(x, y))
                return;

            size_t index = getIndex(x, y);
            if (isLowPixel(x))
            {
                buffer[index].setLow(color);
            }
            else
            {
                buffer[index].setHigh(color);
            }
        }

        Pixel4 getPixel(int16_t x, int16_t y) const override
        {
            if (!inBounds(x, y))
                return Pixel4(0);

            size_t index = getIndex(x, y);
            return isLowPixel(x) ? buffer[index].getLow() : buffer[index].getHigh();
        }

        void clear(Pixel4 color = Pixel4(0)) override
        {
            uint8_t packed = (color.value << 4) | color.value;
            memset(buffer, packed, BUFFER_SIZE);
        }
        
        // ========================================
        // PERFORMANCE OPTIMIZATIONS
        // ========================================
        
        /**
         * @brief Optimized horizontal line drawing with batch operations
         * @param x Starting x coordinate  
         * @param y Y coordinate
         * @param width Line width in pixels
         * @param color Line color
         */
        void drawHLine(int16_t x, int16_t y, int16_t width, Pixel4 color) {
            if (y < 0 || y >= HEIGHT || x >= WIDTH || width <= 0) return;
            
            // Clip to canvas bounds
            if (x < 0) {
                width += x;
                x = 0;
            }
            if (x + width > WIDTH) {
                width = WIDTH - x;
            }
            
            // Fast path for even alignment and even width
            if ((x & 1) == 0 && (width & 1) == 0) {
                size_t startIndex = getIndex(x, y);
                uint8_t packed = (color.value << 4) | color.value;
                size_t count = width / 2;
                
                // Batch set using memset for uniform color
                memset(&buffer[startIndex], packed, count);
            } else {
                // Fallback to individual pixel setting
                for (int16_t i = 0; i < width; ++i) {
                    setPixel(x + i, y, color);
                }
            }
        }
        
        /**
         * @brief Optimized vertical line drawing
         * @param x X coordinate
         * @param y Starting y coordinate
         * @param height Line height in pixels  
         * @param color Line color
         */
        void drawVLine(int16_t x, int16_t y, int16_t height, Pixel4 color) {
            if (x < 0 || x >= WIDTH || y >= HEIGHT || height <= 0) return;
            
            // Clip to canvas bounds
            if (y < 0) {
                height += y;
                y = 0;
            }
            if (y + height > HEIGHT) {
                height = HEIGHT - y;
            }
            
            // Vertical lines can't use memset optimization due to stride
            for (int16_t i = 0; i < height; ++i) {
                setPixel(x, y + i, color);
            }
        }
        
        /**
         * @brief Optimized rectangle filling with batch operations
         * @param x Starting x coordinate
         * @param y Starting y coordinate
         * @param width Rectangle width
         * @param height Rectangle height
         * @param color Fill color
         */
        void fillRect(int16_t x, int16_t y, int16_t width, int16_t height, Pixel4 color) {
            // Use optimized horizontal lines for each row
            for (int16_t row = 0; row < height; ++row) {
                drawHLine(x, y + row, width, color);
            }
        }
        
        /**
         * @brief Batch pixel setting for arrays of data
         * @param x Starting x coordinate
         * @param y Y coordinate  
         * @param pixels Array of pixel values
         * @param count Number of pixels to set
         */
        void setPixelBatch(int16_t x, int16_t y, const Pixel4* pixels, int16_t count) {
            if (y < 0 || y >= HEIGHT || x >= WIDTH || count <= 0) return;
            
            // Clip to canvas bounds
            if (x < 0) {
                pixels += -x;
                count += x;
                x = 0;
            }
            if (x + count > WIDTH) {
                count = WIDTH - x;
            }
            
            // Optimized batch setting for even-aligned data
            if ((x & 1) == 0 && (count & 1) == 0) {
                size_t startIndex = getIndex(x, y);
                
                for (int16_t i = 0; i < count; i += 2) {
                    buffer[startIndex + i/2] = PackedPixel4((pixels[i+1].value << 4) | pixels[i].value);
                }
            } else {
                // Fallback for odd alignment
                for (int16_t i = 0; i < count; ++i) {
                    setPixel(x + i, y, pixels[i]);
                }
            }
        }

        void fill(const Rect &rect, Pixel4 color) override
        {
            int16_t x1 = std::max<int16_t>(0, rect.x);
            int16_t y1 = std::max<int16_t>(0, rect.y);
            int16_t x2 = std::min<int16_t>(WIDTH, rect.x + rect.width);
            int16_t y2 = std::min<int16_t>(HEIGHT, rect.y + rect.height);

            for (int16_t y = y1; y < y2; ++y)
            {
                for (int16_t x = x1; x < x2; ++x)
                {
                    setPixel(x, y, color);
                }
            }
        }

        // Direct buffer access for advanced operations
        const PackedPixel4 *getBuffer() const { return buffer; }
        PackedPixel4 *getBuffer() { return buffer; }
        size_t getBufferSize() const { return BUFFER_SIZE; }

        // Copy operations
        void copyFrom(const Canvas4 &other, int16_t dst_x = 0, int16_t dst_y = 0)
        {
            for (int16_t y = 0; y < HEIGHT && y < other.getHeight(); ++y)
            {
                for (int16_t x = 0; x < WIDTH && x < other.getWidth(); ++x)
                {
                    setPixel(dst_x + x, dst_y + y, other.getPixel(x, y));
                }
            }
        }

        // Blit operation for sprites
        void blit(const Canvas4 &sprite, int16_t x, int16_t y, Pixel4 transparent = Pixel4(0))
        {
            for (int16_t sy = 0; sy < sprite.getHeight(); ++sy)
            {
                for (int16_t sx = 0; sx < sprite.getWidth(); ++sx)
                {
                    Pixel4 pixel = sprite.getPixel(sx, sy);
                    if (pixel.value != transparent.value)
                    {
                        setPixel(x + sx, y + sy, pixel);
                    }
                }
            }
        }
    };
    
    // 8-bit canvas for compatibility and higher precision
    template <uint16_t WIDTH, uint16_t HEIGHT>
    class Canvas8 : public ICanvas<uint8_t>
    {
    public:
        using PixelType = uint8_t;
        
    private:
        uint8_t buffer[WIDTH * HEIGHT];
        
        // Text cursor and color state
        int16_t cursor_x = 0;
        int16_t cursor_y = 0;
        uint8_t textcolor = 15;     // Default to white in 4-bit grayscale
        uint8_t textbgcolor = 0;    // Default to black background
        uint8_t textsize_x = 1;     // Text size scaling
        uint8_t textsize_y = 1;
        bool wrap = true;           // Text wrapping
        bool _cp437 = false;        // Character set compatibility
        const GFXfont* gfx_font = &defaultFont8pt7b;  // Current GFX font (default to included font)

    public:
        Canvas8()
        {
            clear();
        }

        uint16_t getWidth() const override { return WIDTH; }
        uint16_t getHeight() const override { return HEIGHT; }

        void setPixel(int16_t x, int16_t y, uint8_t color) override
        {
            if (!inBounds(x, y))
                return;
            buffer[y * WIDTH + x] = color;
        }

        uint8_t getPixel(int16_t x, int16_t y) const override
        {
            if (!inBounds(x, y))
                return 0;
            return buffer[y * WIDTH + x];
        }

        void clear(uint8_t color = 0) override
        {
            std::fill(buffer, buffer + WIDTH * HEIGHT, color);
        }

        void fill(const Rect &rect, uint8_t color) override
        {
            int16_t x1 = std::max<int16_t>(0, rect.x);
            int16_t y1 = std::max<int16_t>(0, rect.y);
            int16_t x2 = std::min<int16_t>(WIDTH, rect.x + rect.width);
            int16_t y2 = std::min<int16_t>(HEIGHT, rect.y + rect.height);

            for (int16_t y = y1; y < y2; ++y)
            {
                for (int16_t x = x1; x < x2; ++x)
                {
                    setPixel(x, y, color);
                }
            }
        }

        // Convert to 4-bit
        void convertTo4bit(Canvas4<WIDTH, HEIGHT> &dst) const
        {
            for (int16_t y = 0; y < HEIGHT; ++y)
            {
                for (int16_t x = 0; x < WIDTH; ++x)
                {
                    dst.setPixel(x, y, Pixel4::from8bit(getPixel(x, y)));
                }
            }
        }

        const uint8_t *getBuffer() const { return buffer; }
        uint8_t *getBuffer() { return buffer; }
        
        // === Adafruit_GFX Compatibility Methods ===
        
        /**
         * @brief Fill entire canvas (Adafruit_GFX compatibility)
         */
        void fillScreen(uint8_t color) {
            clear(color);
        }
        
        /**
         * @brief Draw single pixel (Adafruit_GFX compatibility)
         */
        void drawPixel(int16_t x, int16_t y, uint8_t color) {
            setPixel(x, y, color);
        }
        
        /**
         * @brief Get canvas width (Adafruit_GFX compatibility)
         */
        uint16_t width() const { return WIDTH; }
        
        /**
         * @brief Get canvas height (Adafruit_GFX compatibility)
         */
        uint16_t height() const { return HEIGHT; }
        
        /**
         * @brief Set text color 
         */
        void setTextColor(uint16_t color) { 
            textcolor = color & 0xFF; // Convert to 8-bit
            textbgcolor = textcolor; // Set both to same for transparent background
        }
        
        /**
         * @brief Set text color with background
         */
        void setTextColor(uint16_t color, uint16_t bg) { 
            textcolor = color & 0xFF;
            textbgcolor = bg & 0xFF;
        }
        
        /**
         * @brief Set cursor position
         */
        void setCursor(int16_t x, int16_t y) { 
            cursor_x = x;
            cursor_y = y;
        }
        
        /**
         * @brief Get cursor X position
         */
        int16_t getCursorX() const { return cursor_x; }
        
        /**
         * @brief Get cursor Y position  
         */
        int16_t getCursorY() const { return cursor_y; }
        
        /**
         * @brief Write a single character (Adafruit_GFX compatible)
         */
        size_t write(uint8_t c) {
            if (!gfx_font) return 1; // Safety check
            
            // GFX font handling
            if (c == '\n') {
                cursor_x = 0;
                cursor_y += (int16_t)textsize_y * gfx_font->yAdvance;
            } else if (c != '\r') {
                uint8_t first = gfx_font->first;
                if ((c >= first) && (c <= gfx_font->last)) {
                    GFXglyph* glyph = &gfx_font->glyph[c - first];
                    uint8_t w = glyph->width;
                    uint8_t h = glyph->height;
                    if ((w > 0) && (h > 0)) {
                        int16_t xo = glyph->xOffset;
                        if (wrap && ((cursor_x + textsize_x * (xo + w)) > WIDTH)) {
                            cursor_x = 0;
                            cursor_y += (int16_t)textsize_y * gfx_font->yAdvance;
                        }
                        drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
                    }
                    cursor_x += glyph->xAdvance * (int16_t)textsize_x;
                }
            }
            return 1;
        }
        
        /**
         * @brief Print text at cursor position (Adafruit_GFX compatible)
         */
        void print(const char* text) { 
            if (!text) return;
            while (*text) {
                write(*text++);
            }
        }
        
        /**
         * @brief Print text with newline (basic implementation)
         */
        void println(const char* text) {
            print(text);
            cursor_x = 0;
            cursor_y += 8;
        }
        
        /**
         * @brief Draw a single character (Adafruit_GFX compatible)
         */
        void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size_x, uint8_t size_y) {
            // GFX font rendering (based on Adafruit_GFX implementation)
            if (!gfx_font) return; // Safety check
            
            c -= gfx_font->first;
            if (c >= (gfx_font->last - gfx_font->first + 1)) return;
            
            GFXglyph* glyph = &gfx_font->glyph[c];
            uint8_t* bitmap = gfx_font->bitmap;
            
            uint16_t bo = glyph->bitmapOffset;
            uint8_t w = glyph->width;
            uint8_t h = glyph->height;
            int8_t xo = glyph->xOffset;
            int8_t yo = glyph->yOffset;
            uint8_t xx, yy, bits = 0, bit = 0;
            int16_t xo16 = 0, yo16 = 0;
            
            if (size_x > 1 || size_y > 1) {
                xo16 = xo;
                yo16 = yo;
            }
            
            // Render glyph bitmap
            for (yy = 0; yy < h; yy++) {
                for (xx = 0; xx < w; xx++) {
                    if (!(bit++ & 7)) {
                        bits = bitmap[bo++];
                    }
                    if (bits & 0x80) {
                        if (size_x == 1 && size_y == 1) {
                            setPixel(x + xo + xx, y + yo + yy, color);
                        } else {
                            fillRect(x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y,
                                    size_x, size_y, color);
                        }
                    }
                    bits <<= 1;
                }
            }
        }
        
        void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size) {
            drawChar(x, y, c, color, bg, size, size);
        }
        
        /**
         * @brief Set text size scaling
         */
        void setTextSize(uint8_t s) { setTextSize(s, s); }
        void setTextSize(uint8_t s_x, uint8_t s_y) { textsize_x = s_x; textsize_y = s_y; }
        
        /**
         * @brief Set text wrap mode
         */
        void setTextWrap(bool w) { wrap = w; }
        
        /**
         * @brief Get text width (6 pixels per character times size)
         */
        int16_t getTextWidth(const char* text) { 
            if (!text) return 0;
            return strlen(text) * 6 * textsize_x; 
        }
        
        /**
         * @brief Fill rectangle
         */
        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
            for (int16_t py = y; py < y + h && py < HEIGHT; py++) {
                for (int16_t px = x; px < x + w && px < WIDTH; px++) {
                    if (px >= 0 && py >= 0) {
                        setPixel(px, py, color);
                    }
                }
            }
        }
        
        /**
         * @brief Draw rectangle outline
         */
        void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
            drawLine(x, y, x + w - 1, y, color);           // Top
            drawLine(x, y + h - 1, x + w - 1, y + h - 1, color); // Bottom
            drawLine(x, y, x, y + h - 1, color);           // Left
            drawLine(x + w - 1, y, x + w - 1, y + h - 1, color); // Right
        }
        
        /**
         * @brief Draw a line using Bresenham's algorithm
         */
        void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
            int16_t dx = std::abs(x1 - x0);
            int16_t dy = std::abs(y1 - y0);
            int16_t sx = (x0 < x1) ? 1 : -1;
            int16_t sy = (y0 < y1) ? 1 : -1;
            int16_t err = dx - dy;
            
            while (true) {
                setPixel(x0, y0, color);
                
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
         * @brief Draw a filled circle using midpoint circle algorithm
         */
        void fillCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color) {
            int16_t x = radius;
            int16_t y = 0;
            int16_t radiusError = 1 - x;
            
            while (x >= y) {
                // Draw horizontal lines for each octant
                for (int16_t i = x0 - x; i <= x0 + x; i++) {
                    setPixel(i, y0 + y, color);
                    setPixel(i, y0 - y, color);
                }
                for (int16_t i = x0 - y; i <= x0 + y; i++) {
                    setPixel(i, y0 + x, color);
                    setPixel(i, y0 - x, color);
                }
                
                y++;
                if (radiusError < 0) {
                    radiusError += 2 * y + 1;
                } else {
                    x--;
                    radiusError += 2 * (y - x + 1);
                }
            }
        }
        
        /**
         * @brief Draw circle outline using midpoint circle algorithm
         */
        void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint8_t color) {
            int16_t x = radius;
            int16_t y = 0;
            int16_t radiusError = 1 - x;
            
            while (x >= y) {
                setPixel(x0 + x, y0 + y, color);
                setPixel(x0 + y, y0 + x, color);
                setPixel(x0 - y, y0 + x, color);
                setPixel(x0 - x, y0 + y, color);
                setPixel(x0 - x, y0 - y, color);
                setPixel(x0 - y, y0 - x, color);
                setPixel(x0 + y, y0 - x, color);
                setPixel(x0 + x, y0 - y, color);
                
                y++;
                if (radiusError < 0) {
                    radiusError += 2 * y + 1;
                } else {
                    x--;
                    radiusError += 2 * (y - x + 1);
                }
            }
        }
        
        /**
         * @brief Draw rounded rectangle outline
         */
        void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color) {
            // Draw straight edges
            drawLine(x + radius, y, x + w - radius - 1, y, color);              // Top
            drawLine(x + radius, y + h - 1, x + w - radius - 1, y + h - 1, color); // Bottom
            drawLine(x, y + radius, x, y + h - radius - 1, color);              // Left
            drawLine(x + w - 1, y + radius, x + w - 1, y + h - radius - 1, color); // Right
            
            // Draw corner arcs (simplified to quarter circles)
            for (int16_t i = 0; i <= radius; i++) {
                for (int16_t j = 0; j <= radius; j++) {
                    if (i*i + j*j <= radius*radius && i*i + j*j >= (radius-1)*(radius-1)) {
                        setPixel(x + radius - i, y + radius - j, color);         // Top-left
                        setPixel(x + w - radius - 1 + i, y + radius - j, color); // Top-right
                        setPixel(x + radius - i, y + h - radius - 1 + j, color); // Bottom-left
                        setPixel(x + w - radius - 1 + i, y + h - radius - 1 + j, color); // Bottom-right
                    }
                }
            }
        }
        
        /**
         * @brief Fill rounded rectangle
         */
        void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color) {
            // Fill main rectangle areas
            fillRect(x + radius, y, w - 2 * radius, h, color);         // Center
            fillRect(x, y + radius, radius, h - 2 * radius, color);    // Left edge
            fillRect(x + w - radius, y + radius, radius, h - 2 * radius, color); // Right edge
            
            // Fill corner arcs (simplified to quarter circles)
            for (int16_t i = 0; i <= radius; i++) {
                for (int16_t j = 0; j <= radius; j++) {
                    if (i*i + j*j <= radius*radius) {
                        setPixel(x + radius - i, y + radius - j, color);         // Top-left
                        setPixel(x + w - radius - 1 + i, y + radius - j, color); // Top-right
                        setPixel(x + radius - i, y + h - radius - 1 + j, color); // Bottom-left
                        setPixel(x + w - radius - 1 + i, y + h - radius - 1 + j, color); // Bottom-right
                    }
                }
            }
        }
        
        /**
         * @brief Fill rectangle with repeating pattern
         */
        void fillRectWithPattern(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *pattern, int16_t patternWidth, int16_t patternHeight) {
            if (!pattern) return;
            
            for (int16_t i = 0; i < w; i++) {
                for (int16_t j = 0; j < h; j++) {
                    // Calculate the corresponding pixel in the pattern
                    int16_t patternX = i % patternWidth;
                    int16_t patternY = j % patternHeight;
                    
                    // Get the color from the pattern
                    uint8_t color = pattern[patternY * patternWidth + patternX];
                    
                    // Draw the pixel
                    setPixel(x + i, y + j, color);
                }
            }
        }
        
        // === GFXcanvas8 Compatibility Methods ===
        
        /**
         * @brief Draw grayscale bitmap with basic parameters
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h) {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++) {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++) {
                    uint8_t pixel = bitmap[py * w + px];
                    // Draw ALL pixels when no matte specified (no transparency)
                    setPixel(x + px, y + py, pixel);
                }
            }
        }
        
        /**
         * @brief Draw grayscale bitmap with matte threshold
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h) {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++) {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++) {
                    uint8_t pixel = bitmap[py * w + px];
                    if (pixel != matte) { // Skip pixels that match the matte color
                        setPixel(x + px, y + py, pixel);
                    }
                }
            }
        }
        
        /**
         * @brief Draw grayscale bitmap with mask
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t w, uint8_t h) {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++) {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++) {
                    uint8_t pixel = bitmap[py * w + px];
                    uint8_t maskValue = mask[py * w + px];
                    if (maskValue > 0) { // Only draw where mask allows
                        setPixel(x + px, y + py, pixel);
                    }
                }
            }
        }
        
        /**
         * @brief Draw grayscale bitmap with opacity
         */
        void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t matte, uint8_t w, uint8_t h, uint8_t opacity_divisor) {
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++) {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++) {
                    uint8_t pixel = bitmap[py * w + px];
                    if (pixel != matte) { // Skip pixels that match the matte color
                        uint8_t opacity_pixel = pixel / opacity_divisor; // Apply opacity
                        setPixel(x + px, y + py, opacity_pixel);
                    }
                }
            }
        }
        
        /**
         * @brief Add blending operation with another canvas
         */
        void add(Canvas8* over) {
            if (!over) return;
            for (uint16_t y = 0; y < HEIGHT && y < over->getHeight(); y++) {
                for (uint16_t x = 0; x < WIDTH && x < over->getWidth(); x++) {
                    uint8_t base = getPixel(x, y);
                    uint8_t overlay = over->getPixel(x, y);
                    uint8_t result = std::min(255, static_cast<int>(base) + static_cast<int>(overlay));
                    setPixel(x, y, result);
                }
            }
        }
        
        /**
         * @brief Add blending operation with texture data
         */
        void add(const uint8_t* texture) {
            if (!texture) return;
            for (uint16_t y = 0; y < HEIGHT; y++) {
                for (uint16_t x = 0; x < WIDTH; x++) {
                    uint8_t base = getPixel(x, y);
                    uint8_t overlay = texture[y * WIDTH + x];
                    uint8_t result = std::min(255, static_cast<int>(base) + static_cast<int>(overlay));
                    setPixel(x, y, result);
                }
            }
        }
        
        /**
         * @brief Subtract blending operation with another canvas
         */
        void subtract(Canvas8* over) {
            if (!over) return;
            for (uint16_t y = 0; y < HEIGHT && y < over->getHeight(); y++) {
                for (uint16_t x = 0; x < WIDTH && x < over->getWidth(); x++) {
                    uint8_t base = getPixel(x, y);
                    uint8_t overlay = over->getPixel(x, y);
                    uint8_t result = std::max(0, static_cast<int>(base) - static_cast<int>(overlay));
                    setPixel(x, y, result);
                }
            }
        }
        
        /**
         * @brief Subtract blending operation with texture data
         */
        void subtract(const uint8_t* texture) {
            if (!texture) return;
            for (uint16_t y = 0; y < HEIGHT; y++) {
                for (uint16_t x = 0; x < WIDTH; x++) {
                    uint8_t base = getPixel(x, y);
                    uint8_t overlay = texture[y * WIDTH + x];
                    uint8_t result = std::max(0, static_cast<int>(base) - static_cast<int>(overlay));
                    setPixel(x, y, result);
                }
            }
        }
        
        /**
         * @brief Difference blending operation with texture data
         */
        void difference(int16_t x, int16_t y, const uint8_t* texture, uint8_t w, uint8_t h) {
            if (!texture) return;
            for (int16_t py = 0; py < h && (y + py) < HEIGHT; py++) {
                for (int16_t px = 0; px < w && (x + px) < WIDTH; px++) {
                    uint8_t base = getPixel(x + px, y + py);
                    uint8_t overlay = texture[py * w + px];
                    uint8_t result = static_cast<uint8_t>(std::abs(static_cast<int>(base) - static_cast<int>(overlay)));
                    setPixel(x + px, y + py, result);
                }
            }
        }

        /**
         * @brief Export canvas to PGM format with proper color scaling
         * @param filename Output filename
         */
        void exportToPGM(const char *filename) const
        {
            FILE *file = fopen(filename, "w");
            if (!file)
                return;

            // PGM header
            fprintf(file, "P2\n");
            fprintf(file, "%d %d\n", WIDTH, HEIGHT);
            fprintf(file, "255\n"); // Max gray value

            // Export pixels with proper scaling
            for (int16_t y = 0; y < HEIGHT; y++)
            {
                for (int16_t x = 0; x < WIDTH; x++)
                {
                    uint8_t pixel = getPixel(x, y);
                    // Scale 4-bit (0-15) to 8-bit (0-255)
                    uint8_t scaled = (pixel * 255) / 15;
                    fprintf(file, "%d ", scaled);
                }
                fprintf(file, "\n");
            }

            fclose(file);
        }
        
        /**
         * @brief Set GFX font for text rendering
         * @param font Pointer to GFXfont structure (nullptr for built-in font)
         */
        void setFont(const GFXfont* font = nullptr) {
            if (font && !gfx_font) {
                // Switching from classic to new font behavior
                cursor_y += 6;
            } else if (!font && gfx_font) {
                // Switching from new to classic font behavior  
                cursor_y -= 6;
            }
            gfx_font = font;
        }
        
        /**
         * @brief Helper to determine character bounds (Adafruit_GFX compatible)
         */
        void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy) {
            if (gfx_font) {
                // Custom font
                if (c == '\n') {
                    *x = 0; // Reset x to zero, advance y by one line
                    *y += textsize_y * gfx_font->yAdvance;
                } else if (c != '\r') { // Not a carriage return; is normal char
                    uint8_t first = gfx_font->first,
                            last = gfx_font->last;
                    if ((c >= first) && (c <= last)) { // Char present in this font?
                        GFXglyph *glyph = &gfx_font->glyph[c - first];
                        uint8_t gw = glyph->width,
                                gh = glyph->height,
                                xa = glyph->xAdvance;
                        int8_t xo = glyph->xOffset,
                               yo = glyph->yOffset;
                        if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > WIDTH)) {
                            *x = 0; // Reset x to zero, advance y by one line
                            *y += textsize_y * gfx_font->yAdvance;
                        }
                        int16_t tsx = (int16_t)textsize_x, tsy = (int16_t)textsize_y,
                                x1 = *x + xo * tsx, y1 = *y + yo * tsy, x2 = x1 + gw * tsx - 1,
                                y2 = y1 + gh * tsy - 1;
                        if (x1 < *minx)
                            *minx = x1;
                        if (y1 < *miny)
                            *miny = y1;
                        if (x2 > *maxx)
                            *maxx = x2;
                        if (y2 > *maxy)
                            *maxy = y2;
                        *x += xa * tsx;
                    }
                }
            } else {
                // Default font (built-in)
                if (c == '\n') {
                    *x = 0;               // Reset x to zero,
                    *y += textsize_y * 8; // advance y one line
                } else if (c != '\r') {
                    if (wrap && ((*x + textsize_x * 6) > WIDTH)) {
                        *x = 0;               // Reset x to zero,
                        *y += textsize_y * 8; // advance y one line
                    }
                    int16_t x1 = *x, y1 = *y, x2 = x1 + textsize_x * 6 - 1, y2 = y1 + textsize_y * 8 - 1;
                    if (x1 < *minx)
                        *minx = x1;
                    if (y1 < *miny)
                        *miny = y1;
                    if (x2 > *maxx)
                        *maxx = x2;
                    if (y2 > *maxy)
                        *maxy = y2;
                    *x += textsize_x * 6; // Advance x one char
                }
            }
        }

        /**
         * @brief Get text bounds (Adafruit_GFX compatible)
         */
        void getTextBounds(const char* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
            if (!str || !x1 || !y1 || !w || !h) return;
            
            uint8_t c;                                                  // Current character
            int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1; // Bound rect
            // Bound rect is intentionally initialized inverted, so 1st char sets it

            *x1 = x; // Initial position is value passed in
            *y1 = y;
            *w = *h = 0; // Initial size is zero

            while ((c = *str++)) {
                // charBounds() modifies x/y to advance for each character,
                // and min/max x/y are updated to incrementally build bounding rect.
                charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);
            }

            if (maxx >= minx) {                       // If legit string bounds were found...
                *x1 = minx;           // Update x1 to least X coord,
                *w = maxx - minx + 1; // And w to bound rect width
            }
            if (maxy >= miny) { // Same for height
                *y1 = miny;
                *h = maxy - miny + 1;
            }
        }
        
        /**
         * @brief Fill triangle (basic implementation)
         */
        void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
            // Sort vertices by Y coordinate (y0 <= y1 <= y2)
            if (y0 > y1) {
                std::swap(x0, x1);
                std::swap(y0, y1);
            }
            if (y1 > y2) {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            if (y0 > y1) {
                std::swap(x0, x1);
                std::swap(y0, y1);
            }
            
            // Fill triangle using horizontal lines
            for (int16_t y = y0; y <= y2; y++) {
                int16_t xa, xb;
                
                if (y <= y1) {
                    // Upper part of triangle
                    if (y1 - y0 != 0) {
                        xa = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
                    } else {
                        xa = x0;
                    }
                } else {
                    // Lower part of triangle
                    if (y2 - y1 != 0) {
                        xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
                    } else {
                        xa = x1;
                    }
                }
                
                if (y2 - y0 != 0) {
                    xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
                } else {
                    xb = x0;
                }
                
                if (xa > xb) std::swap(xa, xb);
                
                for (int16_t x = xa; x <= xb; x++) {
                    setPixel(x, y, color);
                }
            }
        }
    };

    // Type aliases for common canvas sizes
    using Canvas4_128x64 = Canvas4<128, 64>;
    using Canvas4_128x128 = Canvas4<128, 128>;
    using Canvas8_128x64 = Canvas8<128, 64>;
    using Canvas8_128x128 = Canvas8<128, 128>;

} // namespace enjin2