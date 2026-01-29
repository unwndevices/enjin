#include <iostream>

#include "C_Tooltip.hpp"
#include "../Object.hpp"

namespace enjin
{
    C_Tooltip::C_Tooltip(Object *owner, int8_t precision, uint8_t width, uint8_t height) : C_Drawable(width, height), Component(owner),
                                                                                           precision(precision),
                                                                                           value(220.0f),
                                                                                           width(31),
                                                                                           origin(0, 0),
                                                                                           canvas()
    {
        position = owner->GetComponent<C_Position>();

        if (!position)
        {
            std::cerr << "C_Tooltip requires C_Position component.\n";
        }
    }

    void C_Tooltip::Draw(EiseiCanvas &canvas)
    {

        std::string value_string = std::to_string(value);
        // draw value
        if (!precision)
        {
            value_string = std::to_string((int)value);
        }
        width = 20; // todo temporary!this->canvas.getTextWidth(value_string);
        this->canvas.fillScreen(16U);
        this->canvas.fillRoundRect(3, 0, width + 2 + 8, 15 + 2, 4, 0);
        this->canvas.fillRoundRect(4, 1, width + 8, 15, 3, 1);
        this->canvas.drawRoundRect(4, 1, width + 8, 15, 3, 15);
        this->canvas.setCursor(8, 12);
        this->canvas.println(value_string.c_str());
        // draw canvas
        canvas.drawGrayscaleBitmap(origin.x, origin.y, this->canvas.getBuffer(), (uint8_t)16U, this->canvas.width(), this->canvas.height());
    }

    bool C_Tooltip::ContinueToDraw() const
    {
        return !owner->IsQueuedForRemoval();
    }

    void C_Tooltip::Awake()
    {
        position->SetPosition(origin);
    }

    void C_Tooltip::SetValue(float val)
    {
        value = val;
    }

    void C_Tooltip::Update(uint16_t deltaTime)
    {
    }
}