#include <iostream>
#include "C_Canvas.hpp"
#include "../Object.hpp"

namespace enjin
{
    C_Canvas::C_Canvas(Object *owner, uint8_t width, uint8_t height)
        : Component(owner), C_Drawable(width, height), _canvas()
    {
        position = owner->GetComponent<C_Position>();
        if (!position)
        {
            std::cerr << "C_Satellite requires C_Position component.\n";
        }
    }

    void C_Canvas::Draw(EiseiCanvas &canvas)
    {
        switch (GetBlendMode())
        {
        case BlendMode::Normal:
            canvas.drawGrayscaleBitmap(GetOffsetPosition().x, GetOffsetPosition().y, _canvas.getBuffer(), 16U, _canvas.width(), _canvas.height());
            break;
        case BlendMode::Opacity50:
            canvas.drawGrayscaleBitmap(GetOffsetPosition().x, GetOffsetPosition().y, _canvas.getBuffer(), 16U, _canvas.width(), _canvas.height(), 2U);
            break;
        case BlendMode::Opacity25:
            canvas.drawGrayscaleBitmap(GetOffsetPosition().x, GetOffsetPosition().y, _canvas.getBuffer(), 16U, _canvas.width(), _canvas.height(), 4U);
            break;
        case BlendMode::Add:
            canvas.add(_canvas.getBuffer());
            break;
        case BlendMode::Sub:
            canvas.subtract(_canvas.getBuffer());
            break;
        default:
            break;
        }
    }

    bool C_Canvas::ContinueToDraw() const
    {
        return !owner->IsQueuedForRemoval();
    }

    void C_Canvas::LateUpdate(uint16_t deltaTime)
    {
    }
}