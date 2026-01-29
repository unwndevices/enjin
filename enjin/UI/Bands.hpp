#ifndef BANDS_HPP
#define BANDS_HPP

#include "Object.hpp"
#include "Components/C_Draw.hpp"
#include "Components/C_Transition.hpp"
#include "Components/C_ParameterAnimator.hpp"

namespace enjin
{
    class Bands : public Object
    {
    public:
        Bands()
        {
            position->SetPosition(Vector2(63, 63));
            draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                        { Draw(canvas); });
            draw->SetDrawLayer(DrawLayer::Overlay);
            draw->SetBlendMode(BlendMode::Normal);
            draw->SetAnchorPoint(Anchor::CENTER_);
        }

        void SetVisibility(bool visibility)
        {
            draw->SetVisibility(visibility);
        }

        void Draw(EiseiCanvas &canvas)
        {
            int16_t total_width = (band_count * band_width) + ((band_count > 0 ? band_count - 1 : 0) * band_spacing);
            int16_t start_x = position->x - total_width / 2;

            for (uint8_t i = 0; i < band_count; i++)
            {
                int16_t current_x = start_x + i * (band_width + band_spacing);
                uint8_t current_height = band_heights[i];
                int16_t current_y = position->y - current_height / 2;
                canvas.fillRect(current_x, current_y, band_width, current_height, color);
            }
        }

        void SetBandHeights(uint8_t *heights)
        {
            for (uint8_t i = 0; i < band_count; i++)
            {
                band_heights[i] = heights[i];
            }
        }

    private:
        std::shared_ptr<C_Draw> draw;

        uint8_t band_count = 20;
        uint8_t band_width = 3;
        uint8_t band_max_height = 63;
        uint8_t band_spacing = 3;
        uint8_t color = 10;

        uint8_t band_heights[20] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    };
}

#endif
