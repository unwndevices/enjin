#ifndef DRAWING_HELPER_HPP
#define DRAWING_HELPER_HPP

#include <stdint.h>
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"

namespace enjin
{
    /**
     * Draw a circle with a stroke of specified width
     *
     * @param canvas The GFX canvas to draw on
     * @param x0 The x-coordinate of the center of the circle
     * @param y0 The y-coordinate of the center of the circle
     * @param radius The radius of the circle
     * @param color The color of the stroke
     * @param strokeWidth The width of the stroke (will be rounded down to nearest odd number)
     */
    void drawCircleStroke(EiseiCanvas *canvas, int16_t x0, int16_t y0, int16_t radius,
                          uint8_t color, uint8_t strokeWidth);

} // namespace enjin

#endif // DRAWING_HELPER_HPP