#ifndef C_CURVEDSLIDER_HPP
#define C_CURVEDSLIDER_HPP

#include <memory>
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include "../Object.hpp"
#include "../utils/Polar.hpp"

namespace enjin
{

    class C_CurvedSlider : public C_Drawable
    {
    public:
        C_CurvedSlider(Object *owner, uint8_t slider_width, uint8_t slider_height, bool side = false)
            : Component(owner), C_Drawable(slider_width, slider_height),
              slider_width(slider_width), slider_height(slider_height), color(8), value(0.0f)
        {
            position = owner->GetComponent<C_Position>();
            this->side = side;
        };

        void Awake() override {};
        void Update(uint16_t deltaTime) override {};

        void Draw(EiseiCanvas &canvas) override
        {
            uint8_t _color = color;

            if (GetBlendMode() == BlendMode::Opacity50)
            {
                _color = color / 2;
            }
            else if (GetBlendMode() == BlendMode::Opacity25)
            {
                _color = color / 4;
            }
            Vector2 position;
            int steps = value * 71;
            if (value > 0.0f)
            {
                for (int i = 1; i <= steps; i++)
                {
                    float normalizedAngle = ((float)i) / 360.0f;
                    if (side != false) // if on the left side
                    {
                        normalizedAngle += 0.5f; // Rotate by 180 degrees
                    }
                    normalizedAngle = fmod(normalizedAngle, 1.0f); // Ensure the value is within [0, 1)

                    position = RadialToCartesian(normalizedAngle, 58);
                    canvas.fillCircle(GetOffsetPosition().x + position.x, GetOffsetPosition().y + position.y, 3, _color);
                }
            }
            else if (value < 0.0f)
            {
                for (int i = 1; i <= -steps; i++)
                {
                    float normalizedAngle = 1.0f - ((float)i) / 360.0f;
                    if (side != false) // if on the left side
                    {
                        normalizedAngle -= 0.5f; // Rotate by 180 degrees
                    }
                    normalizedAngle = fmod(normalizedAngle, 1.0f); // Ensure the value is within [0, 1)

                    position = RadialToCartesian(normalizedAngle, 58);
                    canvas.fillCircle(GetOffsetPosition().x + position.x, GetOffsetPosition().y + position.y, 3, _color);
                }
            }
        }

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
        bool side;
    };
}
#endif // C_CURVEDSLIDER_HPP
