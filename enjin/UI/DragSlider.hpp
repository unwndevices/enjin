#ifndef DRAGSLIDER_HPP
#define DRAGSLIDER_HPP

#include "../Object.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Components/C_CurvedSlider.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_PositionAnimator.hpp"
#include "assets/slider_sprite.h"

namespace enjin
{
    class DragSlider : public Object
    {
    public:
        DragSlider(const uint8_t *icon_texture, bool side = false)
        {
            this->side = side;

            curved_slider = AddComponent<C_CurvedSlider>(127, 127, side);
            curved_slider->SetDrawLayer(DrawLayer::Overlay);
            curved_slider->SetBlendMode(BlendMode::Normal);
            curved_slider->SetAnchorPoint(Anchor::CENTER_);

            slider = AddComponent<C_Sprite>(48, 119);
            if (side == 0)
            {
                slider->Load(slider_graphic_l, 48, 119);
                slider->SetAnchorPoint(Anchor::CENTER_LEFT);
                slider->AddOffset(Vector2(-62, 0));
            }
            else
            {
                slider->Load(slider_graphic_r, 48, 119);
                slider->SetAnchorPoint(Anchor::CENTER_RIGHT);
                slider->AddOffset(Vector2(63, 0));
            }

            slider->SetDrawLayer(DrawLayer::Overlay);
            slider->SetBlendMode(BlendMode::Normal);

            icon_bg = AddComponent<C_Canvas>(21, 21);
            icon_bg->SetDrawLayer(DrawLayer::Overlay);
            icon_bg->SetBlendMode(BlendMode::Normal);
            icon_bg->_canvas.fillScreen(16U);
            icon_bg->_canvas.fillCircle(icon_bg->GetWidth() / 2, icon_bg->GetHeight() / 2, icon_bg->GetWidth() / 2, 1);
            icon_bg->_canvas.drawCircle(icon_bg->GetWidth() / 2, icon_bg->GetHeight() / 2, icon_bg->GetWidth() / 2, 14);
            icon_bg->SetAnchorPoint(Anchor::CENTER_);

            // Load the icon using C_Sprite
            icon = AddComponent<C_Sprite>(15, 15);
            icon->Load(icon_texture, 15, 15);
            icon->SetDrawLayer(DrawLayer::Overlay);
            icon->SetBlendMode(BlendMode::Normal);
            icon->SetAnchorPoint(Anchor::CENTER_);

            top = AddComponent<C_Sprite>(11, 11);
            top->SetDrawLayer(DrawLayer::Overlay);
            top->SetBlendMode(BlendMode::Normal);
            top->SetAnchorPoint(Anchor::CENTER_);
            top->AddOffset(Vector2(0, -55));
            top->Load((const uint8_t *)plus_11, 11, 11);

            bottom = AddComponent<C_Sprite>(11, 11);
            bottom->SetDrawLayer(DrawLayer::Overlay);
            bottom->SetBlendMode(BlendMode::Normal);
            bottom->SetAnchorPoint(Anchor::CENTER_);
            bottom->AddOffset(Vector2(0, 55));

            if (side == 0)
            {
                icon_bg->AddOffset(Vector2(-41, 0));
                icon->AddOffset(Vector2(-41, 0));
                top->Load((const uint8_t *)plus_11, 11, 11);
                bottom->Load((const uint8_t *)minus_11, 11, 11);
            }
            else
            {
                icon_bg->AddOffset(Vector2(41, 0));
                icon->AddOffset(Vector2(41, 0));
                top->Load((const uint8_t *)minus_11, 11, 11);
                bottom->Load((const uint8_t *)plus_11, 11, 11);
            }
            SetActive(false);
            InitAnimation();
        }

        // Method to change the sprite texture at runtime
        void SetIcon(const uint8_t *new_icon_texture, uint8_t width, uint8_t height)
        {
            icon->Load(new_icon_texture, width, height);
        }

        void SetPosition(int16_t x, int16_t y)
        {
            position->SetPosition(x, y);
        }

        // implement other methods here
        void SetVisibility(bool visible)
        {
            icon->SetVisibility(visible);
            icon_bg->SetVisibility(visible);
            top->SetVisibility(visible);
            bottom->SetVisibility(visible);
            slider->SetVisibility(visible);
            curved_slider->SetVisibility(visible);
        }
        void SetActive(bool active)
        {
            if (active)
            {
                icon->SetBlendMode(BlendMode::Normal);
                icon_bg->SetBlendMode(BlendMode::Normal);
                slider->SetBlendMode(BlendMode::Normal);
                curved_slider->SetBlendMode(BlendMode::Normal);
            }
            else
            {
                icon->SetBlendMode(BlendMode::Opacity25);
                icon_bg->SetBlendMode(BlendMode::Opacity25);
                slider->SetBlendMode(BlendMode::Opacity25);
                curved_slider->SetBlendMode(BlendMode::Opacity25);
            }
            top->SetVisibility(active);
            bottom->SetVisibility(active);
        }

        void SetValue(float value)
        {
            curved_slider->SetValue(value);
        }

        void EnterTransition(bool reset = false)
        {
            SetVisibility(true);
            pos_transition->StartAnimation(reset);
        }

        PositionAnimation pos_animation_in, pos_animation_out;
        std::shared_ptr<C_PositionAnimator> pos_transition;

    private:
        std::shared_ptr<C_Sprite> icon, top, bottom, slider;
        std::shared_ptr<C_Canvas> icon_bg;
        std::shared_ptr<C_CurvedSlider> curved_slider;
        bool side;

        void InitAnimation()
        {
            pos_transition = AddComponent<C_PositionAnimator>();
            if (side == 0)
            {
                pos_animation_in.AddKeyframe({0, Vector2(-63, 63), Easing::Step});
                pos_animation_in.AddKeyframe({350, Vector2(63, 63), Easing::EaseOutQuad});
            }
            else
            {
                pos_animation_in.AddKeyframe({0, Vector2(127 + 63, 63), Easing::Step});
                pos_animation_in.AddKeyframe({350, Vector2(63, 63), Easing::EaseOutQuad});
            }
            pos_transition->SetAnimation(pos_animation_in);
        }
    };
}
#endif // DRAGSLIDER_HPP
