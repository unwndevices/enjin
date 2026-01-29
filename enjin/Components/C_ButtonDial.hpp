#ifndef C_BUTTONDIAL_HPP
#define C_BUTTONDIAL_HPP

#include <memory>
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "../Object.hpp"
#include "../utils/Polar.hpp"

namespace enjin
{
    class C_ButtonDial : public C_Drawable
    {
    public:
        C_ButtonDial(Object *owner, uint8_t outerRadius, uint8_t innerRadius, uint8_t buttonAmount)
            : C_Drawable(outerRadius * 2 + 1, outerRadius * 2 + 1), Component(owner),
              internalCanvas(),
              outerRadius(outerRadius), innerRadius(innerRadius), buttonAmount(buttonAmount), color(12), id(-1)
        {
            position = owner->GetComponent<C_Position>();
        };

        void Awake() override {};
        void Update(uint16_t deltaTime) override {};
        void Draw(EiseiCanvas &canvas) override
        {
            internalCanvas.fillScreen(16);

            Vector2 center = {(int16_t)(internalCanvas.width() / 2), (int16_t)(internalCanvas.height() / 2)};

            // Draw the outer and inner circles of the dial
            internalCanvas.drawCircle(center.x, center.y, outerRadius, color);

            // Draw the buttons and dividing lines around the circumference of the dial
            for (int i = 0; i < buttonAmount; i++)
            {
                float phase = (float)i / buttonAmount;
                Vector2 point = enjin::RadialToCartesian(phase, outerRadius, center);
                internalCanvas.drawPixel(point.x, point.y, color);

                // Draw dividing lines
                internalCanvas.drawDottedLine(center.x, center.y, point.x, point.y, color, 1, 1);
            }
            internalCanvas.fillCircle(center.x, center.y, innerRadius, 16U);
            internalCanvas.drawCircle(center.x, center.y, innerRadius, color);

            // Highlight the active button
            if (id >= 0 && id < buttonAmount)
            {
                float activePhase = (float)(id + 0.5) / buttonAmount;
                int activeRadius = (outerRadius + innerRadius) / 2 - 1; // Average of the radii
                Vector2 activePoint = enjin::RadialToCartesian(activePhase, activeRadius, center);
                internalCanvas.fillCircle(activePoint.x, activePoint.y, 7, color);
            }

            canvas.drawGrayscaleBitmap(GetOffsetPosition().x, GetOffsetPosition().y, internalCanvas.getBuffer(), (uint8_t)16, internalCanvas.width(), internalCanvas.height());
        };
        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

        void SetValue(int id) { this->id = id; };

    private:
        int id;
        uint8_t color;
        uint8_t outerRadius;
        uint8_t innerRadius;
        uint8_t buttonAmount;
        EiseiCanvas internalCanvas;
    };
}

#endif // C_BUTTONDIAL_HPP
