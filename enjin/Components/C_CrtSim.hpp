#ifndef C_CRTSIM_HPP
#define C_CRTSIM_HPP

#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include "../utils/Polar.hpp"

#include "../Object.hpp"

#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"

namespace enjin
{
    class C_CrtSim : public C_Drawable
    {
    public:
        C_CrtSim(Object *owner) : Component(owner), C_Drawable(127, 127), crt_buffer()
        {
            position = owner->GetComponent<C_Position>();
        }

        void Awake() override
        {
            crt_buffer.fillScreen(0);
        }

        void Update(uint16_t deltaTime) override
        {
            elapsed_time += deltaTime;
            if (elapsed_time >= 150)
            {
                elapsed_time = 0;
                offset++;
                if (offset > 127)
                {
                    offset = 0;
                }
            }
        }

        void Draw(EiseiCanvas &canvas) override
        {
            // draw a 127x127 pixel canvas
            for (int y = 0; y < 127; y++)
            {
                if ((y + offset) % 2 == 0)
                {
                    crt_buffer.drawLine(0, y, 127, y, color);
                }
                else
                {
                    crt_buffer.drawLine(0, y, 127, y, 0);
                }
            }

            for (int k = 0; k < 400; k++)
            {
                int x = rand() % 127;
                int y = rand() % 127;
                crt_buffer.drawPixel(x, y, rand() % 3 + 1);
            }
            canvas.subtract(crt_buffer.getBuffer());
        };

        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

    private:
        uint8_t color = 2;
        EiseiCanvas crt_buffer;
        uint8_t offset = 0;
        unsigned long elapsed_time = 0;
    };
}

#endif// C_CRTSIM_HPP
