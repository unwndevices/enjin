#ifndef RATIODIAL_HPP
#define RATIODIAL_HPP
#include <memory>

#include "../Object.hpp"
#include "../Components/C_Tickmarks.hpp"

namespace enjin
{
    class RatioDial : public Object
    {
    public:
        RatioDial()
        {
            tickmarks = AddComponent<C_Tickmarks>(Vector2(63, 63), -82, 87, 6, 9, 61);
            tickmarks->SetDrawLayer(DrawLayer::UI);
            tickmarks->SetBlendMode(BlendMode::Normal);
        }

        void SetValue(float value)
        {
            tickmarks->SetValue(value);
        }
        float GetValue()
        {
            return tickmarks->GetValue();
        }

    private:
        std::shared_ptr<C_Tickmarks> tickmarks;
    };
}
#endif // RATIODIAL_HPP
