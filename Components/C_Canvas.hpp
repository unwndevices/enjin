#ifndef C_CANVAS_HPP
#define C_CANVAS_HPP
#include <memory>

#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
namespace enjin
{

    class C_Canvas : public C_Drawable
    {
    public:
        C_Canvas(Object *owner, uint8_t width, uint8_t height);
        void Draw(EiseiCanvas &canvas) override;
        bool ContinueToDraw() const override;
        void LateUpdate(uint16_t deltaTime) override;

        EiseiCanvas _canvas;
    };
}
#endif // C_CANVAS_HPP
