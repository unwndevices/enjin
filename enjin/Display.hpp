#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include <cstdio>

namespace enjin
{
    class Display
    {
    public:
        EiseiCanvas canvas;
        Display() {}
        void Draw(uint16_t x, uint16_t y, const uint8_t *buffer, uint16_t w, uint16_t h)
        {
            canvas.drawGrayscaleBitmap(x, y, (uint8_t *)buffer, (int16_t)w, (int16_t)h);
        }
        
        void Clear(uint8_t color = 0)
        {
            canvas.clear(color);
        }

        EiseiCanvas *GetCanvas()
        {
            return &canvas;
        }
    };
}

#endif // DISPLAY_HPP
