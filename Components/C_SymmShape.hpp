#ifndef C_SYMMSHAPE_HPP
#define C_SYMMSHAPE_HPP

#include <memory>
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"

namespace enjin
{
    class C_SymmShape : public C_Drawable
    {
    public:
        C_SymmShape(Object *owner, uint8_t width, uint8_t height, uint8_t shape_width)
            : C_Drawable(width, height), Component(owner),
              internalCanvas(),
              attack(0.0f), decay(0.0f), hold(0.0f), shape_width(shape_width), color(12)
        {
            position = owner->GetComponent<C_Position>();
        };

        void Awake() override {};
        void Update(uint16_t deltaTime) override {};
        void Draw(EiseiCanvas &canvas) override
        {
            internalCanvas.fillScreen(16);

            int shapeH = 32;
            int midX = internalCanvas.width() / 2;
            int midY = internalCanvas.height() / 2 + shapeH / 2;

            int holdLength = shape_width * hold;
            int minBaseDistance = holdLength / 2;

            int leftX = midX - std::max(minBaseDistance, (int)((shape_width / 2) * attack));
            int rightX = midX + std::max(minBaseDistance, (int)((shape_width / 2) * decay));

            internalCanvas.drawDottedLine(midX - (shape_width / 2), midY, midX + (shape_width / 2), midY, color / 2);
            internalCanvas.drawLine(midX - holdLength / 2, midY - shapeH, midX + holdLength / 2, midY - shapeH, color);
            internalCanvas.drawLine(midX - holdLength / 2, midY - shapeH, leftX, midY, color);
            internalCanvas.drawLine(midX + holdLength / 2, midY - shapeH, rightX, midY, color);
            internalCanvas.drawDottedLine(midX, midY - shapeH, midX, midY, color / 2);

            canvas.drawGrayscaleBitmap(GetOffsetPosition().x, GetOffsetPosition().y, internalCanvas.getBuffer(), (uint8_t)16, internalCanvas.width(), internalCanvas.height());
        };
        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

        void SetAttack(float attack) { this->attack = attack; };
        void SetDecay(float decay) { this->decay = decay; };
        void SetHold(float hold) { this->hold = hold; };

    private:
        float attack, decay, hold;
        uint8_t color;
        uint8_t shape_width;
        EiseiCanvas internalCanvas;
    };
}
#endif // C_SYMMSHAPE_HPP
