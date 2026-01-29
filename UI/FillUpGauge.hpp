#ifndef FILLUPGAUGE_HPP
#define FILLUPGAUGE_HPP

#include "../Object.hpp"
#include "../Components/C_Position.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Components/C_FillUpGauge.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_Label.hpp"

#include "../Components/C_PositionAnimator.hpp"

#include "assets/resources.h"
#include "../utils/Utils.hpp"

namespace enjin
{
    class FillUpGauge : public Object
    {
    public:
        FillUpGauge()
        {
            position->SetPosition(63, 63);

            gauge = AddComponent<C_FillUpGauge>(111, 111, 13, GaugeMode::Unidirectional);
            gauge->SetDrawLayer(DrawLayer::Overlay);
            gauge->SetBlendMode(BlendMode::Normal);
            gauge->SetAnchorPoint(Anchor::CENTER_);

            value_bg = AddComponent<C_Canvas>(55, 35);
            value_bg->SetDrawLayer(DrawLayer::Overlay);
            value_bg->SetBlendMode(BlendMode::Normal);
            value_bg->SetAnchorPoint(Anchor::CENTER_);

            value_bg->_canvas.fillScreen(16U);
            value_bg->_canvas.fillRoundRect(0, 0, 55, 35, 9, 1);
            value_bg->_canvas.drawRoundRect(0, 0, 55, 35, 9, 8);
            value_bg->_canvas.drawLine(3, 17, 51, 17, 7);

            label = AddComponent<C_Label>(55, 15);
            label->SetDrawLayer(DrawLayer::Overlay);
            label->SetBlendMode(BlendMode::Normal);
            label->SetAnchorPoint(Anchor::CENTER_);
            label->AddOffset(Vector2(0, -9));

            value_label = AddComponent<C_Label>(55, 15);
            value_label->SetDrawLayer(DrawLayer::Overlay);
            value_label->SetBlendMode(BlendMode::Normal);
            value_label->SetAnchorPoint(Anchor::CENTER_);
            value_label->AddOffset(Vector2(0, 9));
            value_label->SetString("0.0");

            plus = AddComponent<C_Sprite>(11, 11);
            plus->SetDrawLayer(DrawLayer::Overlay);
            plus->SetBlendMode(BlendMode::Normal);
            plus->SetAnchorPoint(Anchor::CENTER_);
            plus->AddOffset(Vector2(0, -45));
            plus->Load((const uint8_t *)plus_11, 11, 11);

            minus = AddComponent<C_Sprite>(11, 11);
            minus->SetDrawLayer(DrawLayer::Overlay);
            minus->SetBlendMode(BlendMode::Normal);
            minus->SetAnchorPoint(Anchor::CENTER_);
            minus->AddOffset(Vector2(0, 45));
            minus->Load((const uint8_t *)minus_11, 11, 11);

            InitAnimations();
        };

        void SetValue(float value)
        {
            gauge->SetValue(value);
            value_label->SetString(floatToString(value));
        }

        void SetString(std::string string)
        {
            label->SetString(string);
        }

        void SetMode(GaugeMode mode)
        {
            gauge->SetMode(mode);
        }

        void SetVisibility(bool visibility)
        {
            gauge->SetVisibility(visibility);
            value_bg->SetVisibility(visibility);
            label->SetVisibility(visibility);
            value_label->SetVisibility(visibility);
            plus->SetVisibility(visibility);
            minus->SetVisibility(visibility);
        }

        void EnterTransition(bool reset = false)
        {
            SetVisibility(true);
            pos_transition->StartAnimation(reset);
        }

        std::shared_ptr<C_PositionAnimator> pos_transition;
        PositionAnimation from_top, from_bottom, from_left, from_right;

        void InitAnimations()
        {
            pos_transition = AddComponent<C_PositionAnimator>();
            from_top.AddKeyframe({0, Vector2(64, 64 - 150), Easing::Step});
            from_top.AddKeyframe({350, Vector2(63, 63), Easing::EaseOutQuad});

            from_bottom.AddKeyframe({0, Vector2(64, 64 + 150), Easing::Step});
            from_bottom.AddKeyframe({350, Vector2(63, 63), Easing::EaseOutQuad});

            from_left.AddKeyframe({0, Vector2(64 - 150, 64), Easing::Step});
            from_left.AddKeyframe({350, Vector2(63, 63), Easing::EaseOutQuad});

            from_right.AddKeyframe({0, Vector2(64 + 150, 64), Easing::Step});
            from_right.AddKeyframe({350, Vector2(63, 63), Easing::EaseOutQuad});

            pos_transition->SetAnimation(from_top);
        }

    private:
        std::shared_ptr<C_FillUpGauge> gauge;
        std::shared_ptr<C_Canvas> value_bg;
        std::shared_ptr<C_Label> label;
        std::shared_ptr<C_Label> value_label;

        std::shared_ptr<C_Sprite> plus;
        std::shared_ptr<C_Sprite> minus;
    };
}
#endif// FILLUPGAUGE_HPP

