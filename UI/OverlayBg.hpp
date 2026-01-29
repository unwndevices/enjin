#ifndef OVERLAYBG_HPP
#define OVERLAYBG_HPP
#include "../Object.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_Label.hpp"
#include "../Components/C_ParameterAnimator.hpp"
#include "assets/resources.h"

namespace enjin
{
    class OverlayBg : public Object
    {
    public:
        OverlayBg(uint8_t gradient = 5)
        {
            opacity = 0;
            frame = gradient;
            overlay_fill = AddComponent<C_Canvas>(128, 128);
            overlay_fill->SetDrawLayer(DrawLayer::Overlay);
            overlay_fill->SetBlendMode(BlendMode::Sub);

            overlay_graphic = AddComponent<C_Sprite>(128, 128);
            overlay_graphic->SetDrawLayer(DrawLayer::Overlay);
            overlay_graphic->SetBlendMode(BlendMode::Normal);
            overlay_graphic->Load((const uint8_t *)overlay_gradient, 128, 128);
            overlay_graphic->LoadFrame(frame);

            InitAnimations();
        };

        void SetVisibility(bool visibility)
        {
            overlay_fill->SetVisibility(visibility);
            overlay_graphic->SetVisibility(visibility);
        }

        void SetDrawLayer(DrawLayer layer)
        {
            overlay_fill->SetDrawLayer(layer);
            overlay_graphic->SetDrawLayer(layer);
        }

        void EnterTransition(bool reset = false)
        {
            SetVisibility(true);
            graphic_transition->StartAnimation(reset);
            fill_transition->StartAnimation(reset);
        }

        void SetFrame(uint8_t frame)
        {
            this->frame = frame;
            overlay_graphic->LoadFrame(frame);
        }

        void SetOpacity(uint8_t opacity)
        {
            this->opacity = opacity;
            overlay_fill->_canvas.fillScreen(opacity);
        }

        uint8_t GetFrame()
        {
            return this->frame;
        }

        uint8_t GetOpacity()
        {
            return this->opacity;
        }

        std::shared_ptr<C_ParameterAnimator<uint8_t>> fill_transition;
        std::shared_ptr<C_ParameterAnimator<uint8_t>> graphic_transition;
        ParameterAnimation<uint8_t> fill_in_animation;
        ParameterAnimation<uint8_t> graphic_in_animation;

    private:
        void InitAnimations()
        {
            fill_transition = AddComponent<C_ParameterAnimator<uint8_t>>();
            fill_transition->SetParameterGetter(std::bind(&OverlayBg::GetOpacity, this));
            fill_transition->SetParameterSetter(std::bind(&OverlayBg::SetOpacity, this, std::placeholders::_1));
            fill_in_animation.AddKeyframe({0, 0U, Easing::Step});
            fill_in_animation.AddKeyframe({250, 5U, Easing::Linear});
            fill_transition->SetAnimation(fill_in_animation);

            graphic_transition = AddComponent<C_ParameterAnimator<uint8_t>>();
            graphic_transition->SetParameterGetter(std::bind(&OverlayBg::GetFrame, this));
            graphic_transition->SetParameterSetter(std::bind(&OverlayBg::SetFrame, this, std::placeholders::_1));
            graphic_in_animation.AddKeyframe({0, 0U, Easing::Step});
            graphic_in_animation.AddKeyframe({250, 3U, Easing::Linear});
            graphic_transition->SetAnimation(graphic_in_animation);
        };

        std::shared_ptr<C_Canvas> overlay_fill;
        std::shared_ptr<C_Sprite> overlay_graphic;

        uint8_t frame, opacity;
    };
}
#endif // OVERLAYBG_HPP
