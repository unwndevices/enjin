#ifndef C_PROBE_HPP
#define C_PROBE_HPP

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
    class C_Probe : public C_Drawable
    {
    public:
        C_Probe(Object *owner, uint8_t from_center, uint8_t radius, uint8_t color);
        void Awake() override;
        void Update(uint16_t deltaTime) override;
        void Draw(EiseiCanvas &canvas) override;
        bool ContinueToDraw() const override;

        void DrawBackground(EiseiCanvas &canvas);
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
        uint8_t from_center, radius, identity;
        Vector2 _position, probe_position;
        static uint8_t amount;
        uint8_t color;

        std::string label;
    };
}
#endif // C_PROBE_HPP
