#ifndef BEAMSHAPER_HPP
#define BEAMSHAPER_HPP

#include "../Object.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_SymmShape.hpp"
#include "../Components/C_Tickmarks.hpp"
#include "../Components/C_PositionAnimator.hpp"
#include "../Components/C_ParameterAnimator.hpp"
#include "Constants.hpp"

namespace enjin
{
    class BeamShaper : public Object
    {
    public:
        BeamShaper()
        {
            position->SetPosition(63, 63);

            bg = AddComponent<C_Canvas>(127, 64);
            bg->SetDrawLayer(DrawLayer::UI);
            bg->SetAnchorPoint(Anchor::CENTER_BOTTOM);
            bg->_canvas.fillScreen(0);
            bg->_canvas.drawFastHLine(0, 63, 127, 10);

            shape = AddComponent<C_SymmShape>(127, 64, 83);
            shape->SetDrawLayer(DrawLayer::UI);
            shape->SetAnchorPoint(Anchor::CENTER_BOTTOM);
            shape->AddOffset(Vector2(0, 10));

            tickmarks = AddComponent<C_Tickmarks>(Vector2(63, 63), 195, 350, 6, 5, 60);
            tickmarks->SetDrawLayer(DrawLayer::UI);
            tickmarks->SetBlendMode(BlendMode::Normal);
            tickmarks->SetAnchorPoint(Anchor::CENTER_);
            tickmarks->AddOffset(Vector2(1, 1));

            par_transition = AddComponent<C_ParameterAnimator<float>>();
            par_transition->SetParameterSetter(std::bind(&BeamShaper::SetShape, this, std::placeholders::_1));
            // par_transition->AddKeyframe({0, 4.0f, Easing::Step});
            // par_transition->AddKeyframe({700, 0.0f, Easing::EaseOutQuad});
            // par_transition->AddKeyframe({1800, 0.5f, Easing::EaseInOutSine});

            transition = AddComponent<C_PositionAnimator>();
            in_transition.AddKeyframe({0, Vector2(64, -67), Easing::Step});
            in_transition.AddKeyframe({350, Vector2(63, 63), Easing::EaseOutQuad});
            transition->SetAnimation(in_transition);
        }

        void EnterTransition()
        {
            transition->StartAnimation();
            par_transition->StartAnimation();
        }

        void SetShape(float value)
        {
            shape->SetAttack(1.0f - value);
            shape->SetDecay(value);
            tickmarks->SetValue(value * 10.0f);
        }

        void SetHold(float value)
        {
            shape->SetHold(value);
        }

        void SetVisibility(bool visibility)
        {
            bg->SetVisibility(visibility);
            shape->SetVisibility(visibility);
            tickmarks->SetVisibility(visibility);
        }

    private:
        std::shared_ptr<C_Canvas> bg;
        std::shared_ptr<C_SymmShape> shape;
        std::shared_ptr<C_Tickmarks> tickmarks;

        std::shared_ptr<C_PositionAnimator> transition;
        PositionAnimation in_transition;

        std::shared_ptr<C_ParameterAnimator<float>> par_transition;
    };
}
#endif // BEAMSHAPER_HPP
