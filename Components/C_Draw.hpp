#ifndef C_DRAW_HPP
#define C_DRAW_HPP

#include <memory>
#include <functional>

#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include "../utils/Polar.hpp"

#include "../Object.hpp"

#include <Adafruit_GFX.h>
#include "../enjin2_compat.hpp"

namespace enjin
{
    // Define the DrawFunction type
    using DrawFunction = std::function<void(EiseiCanvas &canvas)>;

    class C_Draw : public C_Drawable
    {
    public:
        C_Draw(Object *owner, DrawFunction drawFunc = nullptr) : Component(owner), C_Drawable(127, 127), draw(drawFunc)
        {
            position = owner->GetComponent<C_Position>();
        }

        void Draw(EiseiCanvas &canvas) override
        {
            if (draw)
            {
                draw(canvas);
            }
        }

        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        }

        void SetDrawFunction(DrawFunction drawFunc)
        {
            draw = drawFunc;
        }

    private:
        DrawFunction draw;
    };
}
#endif // C_DRAW_HPP
