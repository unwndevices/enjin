#ifndef LABEL_HPP
#define LABEL_HPP

#include "Object.hpp"
#include "../Components/C_Label.hpp"
#include "../Components/C_PositionAnimator.hpp"

namespace enjin
{
    class Label : public Object
    {

    public:
        Label(uint8_t width, uint8_t height, int precision = 0, const GFXfont *font = &absolute8pt7b, uint8_t font_size = 1, uint8_t labelColor = 14U, uint8_t bgColor = 0, uint8_t pointer = 0)
        {
            this->precision = precision;
            position->SetPosition(Vector2(63, 63));
            label = AddComponent<C_Label>(width, height, font, font_size, labelColor, bgColor, pointer);
            label->SetDrawLayer(DrawLayer::UI);
            label->SetBlendMode(BlendMode::Normal);
            label->SetAnchorPoint(Anchor::CENTER_);

            transition = AddComponent<C_PositionAnimator>();
        };

        void SetValue(float value, const std::string &unitText = "", const std::string &prefix = "")
        {
            std::string valueString;
            if (value < 1000.0f)
                valueString = std::string(value, precision);
            else
                valueString = std::string(value, 1);

            if (unitText != "")
            {
                valueString = std::string(unitText);
            }

            if (prefix != "")
            {
                valueString = prefix + valueString;
            }

            label->SetString(valueString);
        }

        void SetText(const std::string &text)
        {
            label->SetString(text);
        }

        void SetVisibility(bool visibility)
        {
            label->SetVisibility(visibility);
        }

        void EnterTransition(bool reset = false)
        {
            label->SetVisibility(true);
            transition->StartAnimation(reset);
        }

        void SetPosition(Vector2 pos)
        {
            position->SetPosition(pos);
        }

        std::shared_ptr<C_PositionAnimator> transition;
        PositionAnimation in_transition;

    private:
        std::shared_ptr<C_Label> label;
        int precision = 0;
    };
}
#endif // LABEL_HPP
