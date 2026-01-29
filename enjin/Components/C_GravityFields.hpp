#ifndef C_GRAVITYFIELDS_HPP
#define C_GRAVITYFIELDS_HPP
#include <vector>
#include <memory>

#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "../utils/Polar.hpp"

#include "../Object.hpp"
namespace enjin
{

    class C_GravityFields : public C_Drawable
    {
    public:
        C_GravityFields(Object *owner, uint8_t width, uint8_t height) : C_Drawable(width, height), Component(owner), radius(5), color(5)
        {
            position = owner->GetComponent<C_Position>();

            if (!position)
            {
            }
        };
        void Awake() override {};
        void Update(uint16_t deltaTime) override {
        };

        void AddProbe(std::shared_ptr<C_Position> probe)
        {
            probes.push_back(probe);
        };

        void Draw(EiseiCanvas &canvas) override
        {
            for (auto &probe : probes)
            {
                canvas.drawCircle(probe->GetPosition().x, probe->GetPosition().y, radius, color);
            }
        };
        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

        void DrawBackground(EiseiCanvas &canvas) {};

    private:
        uint8_t radius;
        uint8_t color;

        std::vector<std::shared_ptr<C_Position>> probes;
    };
}
#endif // C_GRAVITYFIELDS_HPP
