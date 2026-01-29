#ifndef BUTTONDIAL_HPP
#define BUTTONDIAL_HPP

#include "../Object.hpp"
#include "../Components/C_ButtonDial.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_Label.hpp"
#include "../Components/C_PositionAnimator.hpp"

namespace enjin
{
    class ButtonDial : public Object
    {
    public:
        ButtonDial(std::stringpar_name, uint8_t outerRadius, uint8_t innerRadius, uint8_t buttonAmount)
        {
            position->SetPosition(63, 63);
            name = AddComponent<C_Label>(50, 20);
            name->SetDrawLayer(DrawLayer::Overlay);
            name->SetBlendMode(BlendMode::Normal);
            name->SetAnchorPoint(Anchor::CENTER_);
            name->AddOffset(Vector2(0, -10));
            value = AddComponent<C_Label>(50, 20);
            value->SetDrawLayer(DrawLayer::Overlay);
            value->SetBlendMode(BlendMode::Normal);
            value->SetAnchorPoint(Anchor::CENTER_);
            value->AddOffset(Vector2(0, 10));
            dial = AddComponent<C_ButtonDial>(outerRadius, innerRadius, buttonAmount);
            dial->SetDrawLayer(DrawLayer::Overlay);
            dial->SetBlendMode(BlendMode::Normal);
            dial->SetAnchorPoint(Anchor::CENTER_);
            SetName(par_name);
            SetValue(0);

            transition = AddComponent<C_PositionAnimator>();
            PositionKeyframe kf1 = {0, Vector2(64, 127), Easing::Step};
            PositionKeyframe kf2 = {200, Vector2(63, 63), Easing::EaseInOutCubic};
            transition->AddKeyframe(kf1);
            transition->AddKeyframe(kf2);
        };

        void SetName(std::stringstring)
        {
            name->SetString(string);
        }
        void SetValue(int id)
        {
            dial->SetValue(id);
            std::stringvalue_string = String(id);
            value->SetString(value_string);
        }
        void SetVisibility(bool visibility)
        {
            dial->SetVisibility(visibility);
            name->SetVisibility(visibility);
            value->SetVisibility(visibility);
        }

        void EnterTransition()
        {
            transition->StartAnimation();
        }

    private:
        std::shared_ptr<C_ButtonDial> dial;
        std::shared_ptr<C_Label> name;
        std::shared_ptr<C_Label> value;
        std::shared_ptr<C_PositionAnimator> transition;
    };
}
#endif // BUTTONDIAL_HPP
