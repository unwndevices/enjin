#ifndef C_SLIDER_HPP
#define C_SLIDER_HPP

#include <memory>
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "../Object.hpp"

namespace enjin
{
    class C_Slider : public C_Drawable
    {
    public:
        C_Slider(Object *owner, uint8_t slider_width, uint8_t slider_height)
            : C_Drawable(slider_width, slider_height), Component(owner),
              internalCanvas(),
              slider_width(slider_width), slider_height(slider_height), color(12), value(0.0f)
        {
            position = owner->GetComponent<C_Position>();
        };

        void Awake() override {};
        void Update(uint16_t deltaTime) override {};
        void Draw(EiseiCanvas &canvas) override
        {
            internalCanvas.fillScreen(16);

            int midX = internalCanvas.width() / 2;
            int midY = internalCanvas.height() / 2;

            int sliderLength = std::max((int)(slider_width * value), 1);

            int leftX = midX - sliderLength / 2;
            internalCanvas.drawFastVLine(midX - slider_width / 2, 0, slider_height, color);
            internalCanvas.drawFastVLine(midX + slider_width / 2, 0, slider_height, color);
            internalCanvas.drawDottedLine(midX - slider_width / 2, midY, midX + slider_width / 2, midY, color / 2);
            internalCanvas.fillRect(leftX, midY - slider_height / 2, sliderLength, slider_height, 16);
            internalCanvas.drawRect(leftX, midY - slider_height / 2, sliderLength, slider_height, color);

            canvas.drawGrayscaleBitmap(GetOffsetPosition().x, GetOffsetPosition().y, internalCanvas.getBuffer(), (uint8_t)16, internalCanvas.width(), internalCanvas.height());
        };
        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

        void SetValue(float value) { this->value = value; };

    private:
        float value;
        uint8_t color;
        uint8_t slider_width;
        uint8_t slider_height;
        EiseiCanvas internalCanvas;
    };
}
#endif // C_SLIDER_HPP
