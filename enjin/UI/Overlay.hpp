#ifndef OVERLAY_HPP
#define OVERLAY_HPP

#include "../Object.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_Label.hpp"

#include "assets/resources.h"

namespace enjin
{
    class Overlay : public Object
    {
    public:
        Overlay(uint8_t gradient = 3)
        {
            overlay_fill = AddComponent<C_Canvas>(128, 128);
            overlay_fill->SetDrawLayer(DrawLayer::Overlay);
            overlay_fill->SetBlendMode(BlendMode::Sub);
            overlay_fill->_canvas.fillScreen(6);

            overlay_graphic = AddComponent<C_Sprite>(128, 128);
            overlay_graphic->SetDrawLayer(DrawLayer::Overlay);
            overlay_graphic->SetBlendMode(BlendMode::Normal);
            overlay_graphic->Load((const uint8_t *)overlay_gradient, 128, 128);
            overlay_graphic->LoadFrame(gradient);

            label_left = AddComponent<C_Label>(50, 20, 14, 7);
            label_left->SetDrawLayer(DrawLayer::Overlay);
            label_left->SetBlendMode(BlendMode::Normal);

            label_right = AddComponent<C_Label>(50, 20, 14, 7);
            label_right->SetDrawLayer(DrawLayer::Overlay);
            label_right->SetBlendMode(BlendMode::Normal);

            overlay_turnlr = AddComponent<C_Sprite>(127, 127);
            overlay_turnlr->SetOffset(Vector2(1, 1));
            overlay_turnlr->SetDrawLayer(DrawLayer::Overlay);
            overlay_turnlr->SetBlendMode(BlendMode::Normal);
            overlay_turnlr->Load((const uint8_t *)overlay_lr, 127, 127);
        };

        void SetStringLeft(std::string string)
        {
            label_left->SetString(string);
        }
        void SetStringRight(std::string string)
        {
            label_right->SetString(string);
        }
        void SetVisibility(bool visibility)
        {
            overlay_fill->SetVisibility(visibility);
            overlay_graphic->SetVisibility(visibility);
            overlay_turnlr->SetVisibility(visibility);
            label_left->SetVisibility(visibility);
            label_right->SetVisibility(visibility);
        }

    private:
        std::shared_ptr<C_Canvas> overlay_fill;
        std::shared_ptr<C_Sprite> overlay_graphic;
        std::shared_ptr<C_Sprite> overlay_turnlr;
        std::shared_ptr<C_Label> label_left;
        std::shared_ptr<C_Label> label_right;
    };
}
#endif // OVERLAY_HPP
