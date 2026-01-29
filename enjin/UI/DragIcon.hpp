#ifndef DRAGICON_HPP
#define DRAGICON_HPP
#include "../Object.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Components/C_CurvedSlider.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_PositionAnimator.hpp"

namespace enjin
{
    class DragIcon : public Object
    {
    public:
        DragIcon(const uint8_t *icon_texture, int from_center = 41, bool side = false)
        {
            this->side = side;
            this->from_center = from_center;
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

            if (side == 0)
            {
                icon_bg->AddOffset(Vector2(0, -from_center));
                icon->AddOffset(Vector2(0, -from_center));
                // icon_bg->AddOffset(Vector2(-from_center, 0));
                // icon->AddOffset(Vector2(-from_center, 0));
            }
            else
            {
                icon_bg->AddOffset(Vector2(0, from_center));
                icon->AddOffset(Vector2(0, from_center));
                // icon_bg->AddOffset(Vector2(from_center, 0));
                // icon->AddOffset(Vector2(from_center, 0));
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
        }
        void SetActive(bool active)
        {
            if (active)
            {
                icon->SetBlendMode(BlendMode::Normal);
                icon_bg->SetBlendMode(BlendMode::Normal);
            }
            else
            {
                icon->SetBlendMode(BlendMode::Opacity25);
                icon_bg->SetBlendMode(BlendMode::Opacity25);
            }
        }

        void EnterTransition(bool reset = false)
        {
            SetVisibility(true);
            pos_transition->StartAnimation(reset);
        }

        PositionAnimation pos_animation_in, pos_animation_out;
        std::shared_ptr<C_PositionAnimator> pos_transition;

    private:
        std::shared_ptr<C_Sprite> icon;
        std::shared_ptr<C_Canvas> icon_bg;
        bool side;
        int from_center;

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
#endif // DRAGICON_HPP
