#ifndef C_PROBE_RANGE_HPP
#define C_PROBE_RANGE_HPP

#include <iostream>
#include <memory>
#include "../Object.hpp"
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "../utils/Polar.hpp"

namespace enjin
{
    class C_ProbeRange : public C_Drawable
    {
    public:
        C_ProbeRange(Object *owner, uint8_t from_center, uint8_t radius, uint8_t color) : Component(owner), C_Drawable(127, 127), phase(0.0f),
                                                                                          radius(radius), from_center(from_center),
                                                                                          color(color)
        {
            position = owner->GetComponent<C_Position>();
            if (!position)
            {
                std::cerr << "C_ProbeRange requires C_Position component.\n";
            }
        };

        void Awake() override
        {
            C_ProbeRange::amount++;
            identity = C_ProbeRange::amount;
            SetAnchorPoint(Anchor::CENTER_);
            probe_position = RadialToCartesian(phase, from_center, abs_center);
        };

        void Update(uint16_t deltaTime) override
        {
            probe_position = RadialToCartesian(phase, from_center, abs_center);
        };

        void Draw(EiseiCanvas &canvas) override
        {
            int8_t scaledColor = 0;
            if (GetBlendMode() == BlendMode::Normal)
            {
                scaledColor = color;
            }
            else if (GetBlendMode() == BlendMode::Opacity50)
            {
                scaledColor = color / 2;
            }
            else if (GetBlendMode() == BlendMode::Opacity25)
            {
                scaledColor = color / 4;
            }

            canvas.drawCircle(probe_position.x, probe_position.y, radius, scaledColor);
        };

        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

        void SetPhase(float amount) { phase = amount; };
        float GetPhase() { return phase; };
        static void SetAbsCenter(Vector2 position)
        {
            abs_center = position;
        }

        void SetDistance(uint8_t distance)
        {
            from_center = distance;
        }
        void SetRadius(uint8_t radius)
        {
            this->radius = radius;
        }

        void SetLabel(std::string label)
        {
            this->label = label;
        }

    private:
        float phase;
        uint8_t radius, from_center, identity;
        Vector2 _position, probe_position;

        static uint8_t amount;
        static Vector2 abs_center;

        uint8_t color;

        std::string label;
    };
}
#endif // C_PROBE_HPP
