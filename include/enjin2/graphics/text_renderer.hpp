#ifndef ENJIN2_GRAPHICS_TEXT_RENDERER_HPP
#define ENJIN2_GRAPHICS_TEXT_RENDERER_HPP

#include "canvas.hpp"
#include "gfxfont.h"  // canonical GFXglyph/GFXfont definitions (Adafruit GFX layout)
#include <string>
#include <cstring>

namespace enjin2 {

// GFXglyph / GFXfont are defined once in graphics/gfxfont.h (global namespace,
// matching the Adafruit GFX ABI) and used unqualified here via that include.

// Standard ASCII 5x7 font (from Adafruit GFX)
static const uint8_t default_font[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x5B, 0x4F, 0x5B, 0x3E, 0x3E, 0x6B,
    0x4F, 0x6B, 0x3E, 0x1C, 0x3E, 0x7C, 0x3E, 0x1C, 0x18, 0x3C, 0x7E, 0x3C,
    0x18, 0x1C, 0x57, 0x7D, 0x57, 0x1C, 0x1C, 0x5E, 0x7F, 0x5E, 0x1C, 0x00,
    0x18, 0x3C, 0x18, 0x00, 0xFF, 0xE7, 0xC3, 0xE7, 0xFF, 0x00, 0x18, 0x24,
    0x18, 0x00, 0xFF, 0xE7, 0xDB, 0xE7, 0xFF, 0x30, 0x48, 0x3A, 0x06, 0x0E,
    0x26, 0x29, 0x79, 0x29, 0x26, 0x40, 0x7F, 0x05, 0x05, 0x07, 0x40, 0x7F,
    0x05, 0x25, 0x3F, 0x5A, 0x3C, 0xE7, 0x3C, 0x5A, 0x7F, 0x3E, 0x1C, 0x1C,
    0x08, 0x08, 0x1C, 0x1C, 0x3E, 0x7F, 0x14, 0x22, 0x7F, 0x22, 0x14, 0x5F,
    0x5F, 0x00, 0x5F, 0x5F, 0x06, 0x09, 0x7F, 0x01, 0x7F, 0x00, 0x66, 0x89,
    0x95, 0x6A, 0x60, 0x60, 0x60, 0x60, 0x60, 0x94, 0xA2, 0xFF, 0xA2, 0x94,
    0x08, 0x04, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x7E, 0x20, 0x10, 0x08, 0x08,
    0x2A, 0x1C, 0x08, 0x08, 0x1C, 0x2A, 0x08, 0x08, 0x1E, 0x10, 0x10, 0x10,
    0x10, 0x0C, 0x1E, 0x0C, 0x1E, 0x0C, 0x30, 0x38, 0x3E, 0x38, 0x30, 0x06,
    0x0E, 0x3E, 0x0E, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F,
    0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00, 0x14, 0x7F, 0x14, 0x7F, 0x14,
    0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x23, 0x13, 0x08, 0x64, 0x62, 0x36, 0x49,
    0x56, 0x20, 0x50, 0x00, 0x08, 0x07, 0x03, 0x00, 0x00, 0x1C, 0x22, 0x41,
    0x00, 0x00, 0x41, 0x22, 0x1C, 0x00, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x08,
    0x08, 0x3E, 0x08, 0x08, 0x00, 0x80, 0x70, 0x30, 0x00, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x00, 0x00, 0x60, 0x60, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02,
    0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00, 0x72, 0x49,
    0x49, 0x49, 0x46, 0x21, 0x41, 0x49, 0x4D, 0x33, 0x18, 0x14, 0x12, 0x7F,
    0x10, 0x27, 0x45, 0x45, 0x45, 0x39, 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x41,
    0x21, 0x11, 0x09, 0x07, 0x36, 0x49, 0x49, 0x49, 0x36, 0x46, 0x49, 0x49,
    0x29, 0x1E, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x40, 0x34, 0x00, 0x00,
    0x00, 0x08, 0x14, 0x22, 0x41, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x41,
    0x22, 0x14, 0x08, 0x02, 0x01, 0x59, 0x09, 0x06, 0x3E, 0x41, 0x5D, 0x59,
    0x4E, 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x3E,
    0x41, 0x41, 0x41, 0x22, 0x7F, 0x41, 0x41, 0x22, 0x1C, 0x7F, 0x49, 0x49,
    0x49, 0x41, 0x7F, 0x09, 0x09, 0x09, 0x01, 0x3E, 0x41, 0x49, 0x49, 0x7A,
    0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x41, 0x7F, 0x41, 0x00, 0x20, 0x40,
    0x41, 0x3F, 0x01, 0x7F, 0x08, 0x14, 0x22, 0x41, 0x7F, 0x40, 0x40, 0x40,
    0x40, 0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x3E,
    0x41, 0x41, 0x41, 0x3E, 0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51,
    0x21, 0x5E, 0x7F, 0x09, 0x19, 0x29, 0x46, 0x46, 0x49, 0x49, 0x49, 0x31,
    0x01, 0x01, 0x7F, 0x01, 0x01, 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x1F, 0x20,
    0x40, 0x20, 0x1F, 0x3F, 0x40, 0x38, 0x40, 0x3F, 0x63, 0x14, 0x08, 0x14,
    0x63, 0x07, 0x08, 0x70, 0x08, 0x07, 0x61, 0x51, 0x49, 0x45, 0x43, 0x00,
    0x7F, 0x41, 0x41, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x41, 0x41,
    0x7F, 0x00, 0x04, 0x02, 0x01, 0x02, 0x04, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x01, 0x02, 0x04, 0x00, 0x20, 0x54, 0x54, 0x54, 0x78, 0x7F, 0x48,
    0x44, 0x44, 0x38, 0x38, 0x44, 0x44, 0x44, 0x20, 0x38, 0x44, 0x44, 0x48,
    0x7F, 0x38, 0x54, 0x54, 0x54, 0x18, 0x08, 0x7E, 0x09, 0x01, 0x02, 0x0C,
    0x52, 0x52, 0x52, 0x3E, 0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x44, 0x7D,
    0x40, 0x00, 0x20, 0x40, 0x44, 0x3D, 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00,
    0x00, 0x41, 0x7F, 0x40, 0x00, 0x7C, 0x04, 0x18, 0x04, 0x78, 0x7C, 0x08,
    0x04, 0x04, 0x78, 0x38, 0x44, 0x44, 0x44, 0x38, 0x7C, 0x14, 0x14, 0x14,
    0x08, 0x08, 0x14, 0x14, 0x18, 0x7C, 0x7C, 0x08, 0x04, 0x04, 0x08, 0x48,
    0x54, 0x54, 0x54, 0x20, 0x04, 0x3F, 0x44, 0x40, 0x20, 0x3C, 0x40, 0x40,
    0x20, 0x7C, 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x3C, 0x40, 0x30, 0x40, 0x3C,
    0x44, 0x28, 0x10, 0x28, 0x44, 0x0C, 0x50, 0x50, 0x50, 0x3C, 0x44, 0x64,
    0x54, 0x4C, 0x44, 0x00, 0x08, 0x36, 0x41, 0x00, 0x00, 0x00, 0x7F, 0x00,
    0x00, 0x00, 0x41, 0x36, 0x08, 0x00, 0x10, 0x08, 0x08, 0x10, 0x08, 0x78,
    0x46, 0x41, 0x46, 0x78
};

/**
 * @brief Text alignment options
 */
enum class TextAlign {
    Left,
    Center,
    Right
};

/**
 * @brief Text renderer for drawing text to canvas
 */
template<typename TPixel>
class TextRenderer {
private:
    const GFXfont* gfx_font;
    TPixel text_color;
    TPixel bg_color;
    bool transparent_bg;
    uint8_t text_size_x;
    uint8_t text_size_y;
    bool wrap_text;
    int16_t cursor_x;
    int16_t cursor_y;

public:
    /**
     * @brief Construct a new TextRenderer
     */
    TextRenderer()
        : gfx_font(nullptr)
        , text_color(15)
        , bg_color(0)
        , transparent_bg(true)
        , text_size_x(1)
        , text_size_y(1)
        , wrap_text(true)
        , cursor_x(0)
        , cursor_y(0)
    {}

    /**
     * @brief Set the GFX font
     * @param font Pointer to GFX font structure (nullptr for default built-in font)
     */
    void setFont(const GFXfont* font) {
        gfx_font = font;
    }

    /**
     * @brief Set text color (transparent background)
     * @param color Text color
     */
    void setTextColor(TPixel color) {
        text_color = color;
        transparent_bg = true;
    }

    /**
     * @brief Set text color with background
     * @param color Text color
     * @param bgcolor Background color
     */
    void setTextColor(TPixel color, TPixel bgcolor) {
        text_color = color;
        bg_color = bgcolor;
        transparent_bg = false;
    }

    /**
     * @brief Set text size
     * @param size Size multiplier (1 = normal, 2 = double, etc.)
     */
    void setTextSize(uint8_t size) {
        text_size_x = text_size_y = (size > 0) ? size : 1;
    }

    /**
     * @brief Set text size with separate X/Y scaling
     * @param sx X size multiplier
     * @param sy Y size multiplier
     */
    void setTextSize(uint8_t sx, uint8_t sy) {
        text_size_x = (sx > 0) ? sx : 1;
        text_size_y = (sy > 0) ? sy : 1;
    }

    /**
     * @brief Set cursor position
     * @param x X coordinate
     * @param y Y coordinate
     */
    void setCursor(int16_t x, int16_t y) {
        cursor_x = x;
        cursor_y = y;
    }

    /**
     * @brief Set text wrapping
     * @param wrap True to enable text wrapping
     */
    void setTextWrap(bool wrap) {
        wrap_text = wrap;
    }

    /**
     * @brief Draw a single character (Adafruit GFX style)
     * @param canvas Canvas to draw to
     * @param x X position
     * @param y Y position
     * @param c Character to draw
     */
    void drawChar(ICanvas<TPixel>& canvas, int16_t x, int16_t y, unsigned char c) {
        if (!gfx_font) {
            // Built-in font (classic 5x7)
            if (c < 32 || c > 126) return; // Only printable ASCII
            
            for (int8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
                uint8_t line = default_font[c * 5 + i];
                for (int8_t j = 0; j < 8; j++, line >>= 1) {
                    if (line & 1) {
                        if (text_size_x == 1 && text_size_y == 1) {
                            canvas.setPixel(x + i, y + j, text_color);
                        } else {
                            // Draw scaled pixel
                            for (uint8_t sy = 0; sy < text_size_y; sy++) {
                                for (uint8_t sx = 0; sx < text_size_x; sx++) {
                                    canvas.setPixel(x + i * text_size_x + sx, 
                                                  y + j * text_size_y + sy, text_color);
                                }
                            }
                        }
                    } else if (!transparent_bg) {
                        if (text_size_x == 1 && text_size_y == 1) {
                            canvas.setPixel(x + i, y + j, bg_color);
                        } else {
                            // Draw scaled background pixel
                            for (uint8_t sy = 0; sy < text_size_y; sy++) {
                                for (uint8_t sx = 0; sx < text_size_x; sx++) {
                                    canvas.setPixel(x + i * text_size_x + sx, 
                                                  y + j * text_size_y + sy, bg_color);
                                }
                            }
                        }
                    }
                }
            }
            // Draw spacing column
            if (!transparent_bg) {
                for (int8_t j = 0; j < 8; j++) {
                    if (text_size_x == 1 && text_size_y == 1) {
                        canvas.setPixel(x + 5, y + j, bg_color);
                    } else {
                        for (uint8_t sy = 0; sy < text_size_y; sy++) {
                            for (uint8_t sx = 0; sx < text_size_x; sx++) {
                                canvas.setPixel(x + 5 * text_size_x + sx, 
                                              y + j * text_size_y + sy, bg_color);
                            }
                        }
                    }
                }
            }
        } else {
            // Custom GFX font
            if (c < gfx_font->first || c > gfx_font->last) return;
            
            c -= gfx_font->first;
            const GFXglyph* glyph = (const GFXglyph*)gfx_font->glyph;
            const GFXglyph& g = glyph[c];
            
            uint16_t bo = g.bitmapOffset;
            uint8_t w = g.width, h = g.height;
            int8_t xo = g.xOffset, yo = g.yOffset;
            uint8_t xx, yy, bits = 0, bit = 0;
            
            // Draw glyph bitmap
            for (yy = 0; yy < h; yy++) {
                for (xx = 0; xx < w; xx++) {
                    if (!(bit++ & 7)) {
                        bits = gfx_font->bitmap[bo++];
                    }
                    if (bits & 0x80) {
                        if (text_size_x == 1 && text_size_y == 1) {
                            canvas.setPixel(x + xo + xx, y + yo + yy, text_color);
                        } else {
                            for (uint8_t sy = 0; sy < text_size_y; sy++) {
                                for (uint8_t sx = 0; sx < text_size_x; sx++) {
                                    canvas.setPixel(x + (xo + xx) * text_size_x + sx, 
                                                  y + (yo + yy) * text_size_y + sy, text_color);
                                }
                            }
                        }
                    }
                    bits <<= 1;
                }
            }
        }
    }

    /**
     * @brief Draw a string
     * @param canvas Canvas to draw to
     * @param x X position
     * @param y Y position
     * @param str String to draw
     */
    void drawString(ICanvas<TPixel>& canvas, int16_t x, int16_t y, const char* str) {
        setCursor(x, y);
        while (*str) {
            writeChar(canvas, *str++);
        }
    }

    /**
     * @brief Draw a string with automatic wrapping
     * @param canvas Canvas to draw to
     * @param x X position
     * @param y Y position
     * @param width Maximum width before wrapping
     * @param str String to draw
     */
    void drawStringWrapped(ICanvas<TPixel>& canvas, int16_t x, int16_t y, 
                          uint16_t width, const char* str) {
        setCursor(x, y);
        setTextWrap(true);
        
        // Simple word wrapping implementation
        const char* word_start = str;
        const char* current = str;
        int16_t line_x = x;
        
        while (*current) {
            if (*current == ' ' || *current == '\n' || *(current + 1) == '\0') {
                // End of word or line
                
                // Calculate word width
                uint16_t word_width = getTextWidth(std::string(word_start, current).c_str());
                
                // Check if word fits on current line
                if (line_x + word_width > x + width && line_x > x) {
                    // Move to next line
                    cursor_y += getCharHeight() * text_size_y;
                    cursor_x = x;
                    line_x = x;
                }
                
                // Draw the word
                while (word_start <= current) {
                    if (*word_start != '\n') {
                        writeChar(canvas, *word_start);
                    } else {
                        cursor_y += getCharHeight() * text_size_y;
                        cursor_x = x;
                        line_x = x;
                    }
                    word_start++;
                }
                
                line_x = cursor_x;
                word_start = current + 1;
            }
            current++;
        }
    }

    /**
     * @brief Get text bounds for a string (Adafruit ink box)
     * @param str String to measure
     * @param x X position (cursor start / baseline for GFX fonts)
     * @param y Y position (cursor start / baseline for GFX fonts)
     * @param x1 Output: minimum X of the rendered ink (includes glyph bearing)
     * @param y1 Output: minimum Y of the rendered ink (negative of the ascent
     *           relative to the baseline for GFX fonts)
     * @param w Output: ink width
     * @param h Output: ink height
     * @param wrap_width Wrap boundary consulted when wrapping is enabled;
     *        0 (the default) measures single-line. Pass the target canvas
     *        width to reproduce the wrapped extents a Canvas-resident
     *        measurement would report — the renderer holds no canvas at
     *        measurement time, so the boundary must come from the caller.
     *
     * Restored pre-migration semantics (unwn #161): the true ink bounding box
     * via the Adafruit charBounds walk, not the advance-width / yAdvance box.
     * Centering math of the form `pos - w / 2 - x1` / `pos - h / 2 - y1`
     * depends on these bearings.
     */
    void getTextBounds(const char* str, int16_t x, int16_t y,
                      int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h,
                      uint16_t wrap_width = 0) {
        if (!str || !x1 || !y1 || !w || !h) return;

        int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1;

        *x1 = x; // Initial position is value passed in
        *y1 = y;
        *w = *h = 0; // Initial size is zero

        while (*str) {
            charBounds(static_cast<unsigned char>(*str++), &x, &y,
                       &minx, &miny, &maxx, &maxy, wrap_width);
        }

        if (maxx >= minx) {
            *x1 = minx;
            *w = static_cast<uint16_t>(maxx - minx + 1);
        }
        if (maxy >= miny) {
            *y1 = miny;
            *h = static_cast<uint16_t>(maxy - miny + 1);
        }
    }

    /**
     * @brief Helper to determine character bounds (Adafruit_GFX compatible)
     * @param c Character to measure
     * @param x Current X cursor position (updated)
     * @param y Current Y cursor position (updated)
     * @param minx Minimum X bound (updated)
     * @param miny Minimum Y bound (updated)
     * @param maxx Maximum X bound (updated)
     * @param maxy Maximum Y bound (updated)
     * @param wrap_width Wrap boundary (0 = no wrapping during measurement)
     */
    void charBounds(unsigned char c, int16_t* x, int16_t* y,
                    int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy,
                    uint16_t wrap_width = 0) {
        if (gfx_font) {
            if (c == '\n') {
                *x = 0; // Reset x to zero, advance y by one line
                *y += text_size_y * gfx_font->yAdvance;
            } else if (c != '\r') { // Not a carriage return; is normal char
                uint8_t first = gfx_font->first,
                        last = gfx_font->last;
                if ((c >= first) && (c <= last)) { // Char present in this font?
                    const GFXglyph* glyph = (const GFXglyph*)gfx_font->glyph;
                    const GFXglyph& g = glyph[c - first];
                    uint8_t gw = g.width,
                            gh = g.height,
                            xa = g.xAdvance;
                    int8_t xo = g.xOffset,
                           yo = g.yOffset;
                    if (wrap_text && wrap_width &&
                        ((*x + (((int16_t)xo + gw) * text_size_x)) > (int16_t)wrap_width)) {
                        *x = 0; // Reset x to zero, advance y by one line
                        *y += text_size_y * gfx_font->yAdvance;
                    }
                    int16_t tsx = (int16_t)text_size_x, tsy = (int16_t)text_size_y,
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
                *x = 0;                // Reset x to zero,
                *y += text_size_y * 8; // advance y one line
            } else if (c != '\r') {
                if (wrap_text && wrap_width && ((*x + text_size_x * 6) > (int16_t)wrap_width)) {
                    *x = 0;                // Reset x to zero,
                    *y += text_size_y * 8; // advance y one line
                }
                int16_t x1 = *x, y1 = *y, x2 = x1 + text_size_x * 6 - 1, y2 = y1 + text_size_y * 8 - 1;
                if (x1 < *minx)
                    *minx = x1;
                if (y1 < *miny)
                    *miny = y1;
                if (x2 > *maxx)
                    *maxx = x2;
                if (y2 > *maxy)
                    *maxy = y2;
                *x += text_size_x * 6; // Advance x one char
            }
        }
    }

    /**
     * @brief Get width of a string in pixels
     * @param str String to measure
     * @return Width in pixels
     *
     * GFX fonts sum glyph advances, then trim the last glyph's trailing
     * bearing (advance past its ink) — Canvas8::getTextWidth byte-for-byte
     * (sweep adjudication, unwn #168). Centering math tuned on the shipped
     * UI depends on the trim. The built-in font stays 6 px per character.
     */
    uint16_t getTextWidth(const char* str) {
        if (!str) return 0;

        if (gfx_font) {
            uint16_t advance = 0;
            uint8_t first = gfx_font->first, last = gfx_font->last;
            const GFXglyph* glyphs = (const GFXglyph*)gfx_font->glyph;
            const GFXglyph* lastGlyph = nullptr;
            unsigned char c;
            while ((c = static_cast<unsigned char>(*str++))) {
                if (c >= first && c <= last) {
                    lastGlyph = &glyphs[c - first];
                    advance += lastGlyph->xAdvance * text_size_x;
                }
            }
            if (lastGlyph) {
                int16_t lastVisual = (int8_t)lastGlyph->xOffset + lastGlyph->width;
                if (lastVisual < (int16_t)lastGlyph->xAdvance)
                    advance -= (lastGlyph->xAdvance - lastVisual) * text_size_x;
            }
            return advance;
        }

        uint16_t width = 0;
        while (*str) {
            width += getCharWidth(*str++) * text_size_x;
        }
        return width;
    }

    /**
     * @brief Write a character at current cursor position
     * @param canvas Canvas to draw to
     * @param c Character to write
     *
     * The GFX-font path is Canvas8::write byte-for-byte (sweep adjudication,
     * unwn #168): the wrap predicate tests the glyph's scaled ink edge
     * (xOffset + width), not its advance; empty glyphs (space) advance the
     * cursor but never trigger a wrap; '\r' is ignored; out-of-range
     * characters do nothing at all. The built-in 5x7 path keeps its own
     * advance-based wrap — it serves the engine's scripting API and has no
     * BASE counterpart (waived sub-range, unwn #168).
     */
    void writeChar(ICanvas<TPixel>& canvas, unsigned char c) {
        if (gfx_font) {
            if (c == '\n') {
                cursor_x = 0;
                cursor_y += (int16_t)text_size_y * gfx_font->yAdvance;
            } else if (c != '\r') {
                uint8_t first = gfx_font->first;
                if ((c >= first) && (c <= gfx_font->last)) {
                    const GFXglyph* glyph = (const GFXglyph*)gfx_font->glyph;
                    const GFXglyph& g = glyph[c - first];
                    if ((g.width > 0) && (g.height > 0)) {
                        int16_t xo = g.xOffset;
                        if (wrap_text &&
                            ((cursor_x + text_size_x * (xo + g.width)) >
                             (int16_t)canvas.getWidth())) {
                            cursor_x = 0;
                            cursor_y += (int16_t)text_size_y * gfx_font->yAdvance;
                        }
                        drawChar(canvas, cursor_x, cursor_y, c);
                    }
                    cursor_x += g.xAdvance * (int16_t)text_size_x;
                }
            }
        } else {
            if (c == '\n') {
                cursor_x = 0;
                cursor_y += getCharHeight() * text_size_y;
            } else if (c == '\r') {
                cursor_x = 0;
            } else {
                uint16_t char_width = getCharWidth(c) * text_size_x;

                // Check for line wrap
                if (wrap_text && cursor_x + char_width > canvas.getWidth()) {
                    cursor_x = 0;
                    cursor_y += getCharHeight() * text_size_y;
                }

                drawChar(canvas, cursor_x, cursor_y, c);
                cursor_x += char_width;
            }
        }
    }


    /**
     * @brief Get character width
     * @param c Character to measure
     * @return Width in pixels
     */
    uint16_t getCharWidth(unsigned char c) {
        if (gfx_font) {
            if (c < gfx_font->first || c > gfx_font->last) return 0;
            const GFXglyph* glyph = (const GFXglyph*)gfx_font->glyph;
            const GFXglyph& g = glyph[c - gfx_font->first];
            return g.xAdvance;
        }
        return 6; // Default character width (5 + 1 spacing)
    }

    /**
     * @brief Get character height
     * @return Height in pixels
     */
    uint8_t getCharHeight() {
        if (gfx_font) {
            return gfx_font->yAdvance;
        }
        return 8; // Default character height
    }
};

} // namespace enjin2

#endif // ENJIN2_GRAPHICS_TEXT_RENDERER_HPP