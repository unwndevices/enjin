#include "C_Tickmarks.hpp"
#include "../Object.hpp"
#include <math.h>

#ifndef radians
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define radians(deg) ((deg) * DEG_TO_RAD)
#endif

namespace enjin
{
    C_Tickmarks::C_Tickmarks(Object *owner, Vector2 center, int16_t startAngle, int16_t stopAngle, uint8_t spacing, uint8_t length, uint8_t radius) : C_Drawable(127, 127),
                                                                                                                                                      Component(owner),
                                                                                                                                                      center(center),
                                                                                                                                                      startAngle(startAngle),
                                                                                                                                                      stopAngle(stopAngle),
                                                                                                                                                      spacing(spacing),
                                                                                                                                                      length(length),
                                                                                                                                                      radius(radius),
                                                                                                                                                      internalCanvas()
    {
        position = owner->GetComponent<C_Position>();

        if (!position)
        {
            std::cerr << "C_Tooltip requires C_Position component.\n";
        }
    };

    void C_Tickmarks::Awake()
    {
        // Initialize the internal canvas
    }
    void C_Tickmarks::Draw(EiseiCanvas &canvas)
    {
        // Clear the internal canvas
        internalCanvas.fillScreen(16U);

        for (int16_t i = startAngle; i <= stopAngle; i += spacing)
        {
            int angle = i - ((int)(currentValue * 100.0f) * spacing / 10) % spacing - 90;  // final angle for the tickmark
            float tick_value = floorf((currentValue * 10.0f) + (angle + 90.0f) / spacing); // get number value for each tickmark        //  Determine the length of this tickmark
            uint8_t tickLength = ((int)tick_value % 10 == 0) ? length : length / 2;
            // Calculate the start and end points of the tickmark
            uint8_t startX = center.x + (radius - tickLength) * sin(radians(angle));
            uint8_t startY = center.y + (radius - tickLength) * cos(radians(angle));
            uint8_t endX = startX + tickLength * sin(radians(angle));
            uint8_t endY = startY + tickLength * cos(radians(angle));

            // Draw the tickmark
            internalCanvas.drawLine(startX, startY, endX, endY, 0xFFFF);
        }
        // Draw the internal canvas onto the main canvas
        canvas.drawGrayscaleBitmap(GetOffsetPosition().x, GetOffsetPosition().y, internalCanvas.getBuffer(), (uint8_t)16U, width, height);
    }

    void C_Tickmarks::SetValue(float value)
    {
        currentValue = value;
    }

    float C_Tickmarks::GetValue() const
    {
        return currentValue;
    }

    bool C_Tickmarks::ContinueToDraw() const
    {
        return !owner->IsQueuedForRemoval();
    };
}