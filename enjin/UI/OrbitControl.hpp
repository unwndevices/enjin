#ifndef ORBITCONTROL_HPP
#define ORBITCONTROL_HPP

#include "../Object.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_PositionAnimator.hpp"
#include "../Components/C_Draw.hpp"
#include "../Components/C_Label.hpp"

#include "assets/icons.h"
#include "assets/orbit.h"
#include "../utils/Utils.hpp"

namespace enjin
{
    class OrbitControl : public Object
    {
    public:
        OrbitControl()
        {
            position->SetPosition(63, 63);
            draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                        { Draw(canvas); });
            draw->SetDrawLayer(DrawLayer::UI);
            draw->SetBlendMode(BlendMode::Normal);
            draw->SetAnchorPoint(Anchor::CENTER_);
            draw->SetVisibility(false);

            bg = AddComponent<C_Sprite>(73, 31);
            bg->SetDrawLayer(DrawLayer::UI);
            bg->SetAnchorPoint(Anchor::CENTER_BOTTOM);
            bg->AddOffset(Vector2(0, -9));
            bg->Load((const uint8_t *)orbit_bg, 73, 31);
            bg->SetVisibility(false);

            base_speed = AddComponent<C_Label>(47, 11);
            base_speed->SetDrawLayer(DrawLayer::UI);
            base_speed->SetAnchorPoint(Anchor::CENTER_BOTTOM);
            base_speed->AddOffset(Vector2(0, -3));
            base_speed->SetVisibility(false);
            SetBaseHz(1.0);

            InitIcons();

            multiplier = AddComponent<C_Label>(47, 11);
            multiplier->SetDrawLayer(DrawLayer::UI);
            multiplier->SetAnchorPoint(Anchor::CENTER_TOP);
            multiplier->AddOffset(Vector2(0, 19));
            multiplier->SetVisibility(false);
            multiplier->SetString("1.00x");

            InitAnimation();
        };

        void EnterTransition()
        {
            SetVisibility(true);
        }

        void ExitTransition() { SetVisibility(false); };

        void SetVisibility(bool visibility)
        {
            draw->SetVisibility(visibility);
            bg->SetVisibility(visibility);
            base_speed->SetVisibility(visibility);

            for (int i = 0; i < 4; i++)
            {
                icons[i]->SetVisibility(visibility);
            }

            multiplier->SetVisibility(visibility);
        }

        void Draw(EiseiCanvas &canvas)
        {
            // Draw the background
            canvas.fillCircle(position->x, position->y, 40, 0);
            canvas.drawCircle(position->x, position->y, 40, 14);
            // draw center line
            canvas.drawLine((position->x - 37U), (position->y - 7U), (position->x + 37U), (position->y - 7U), 14);
        }

        void Update(uint16_t deltaTime) override
        {
            Object::Update(deltaTime);
        };

        void SetBaseHz(float hz)
        {
            std::string hz_string = floatToString(hz);
            hz_string.append("hz");
            base_speed->SetString(hz_string);
        };

        void SetMultiplier(float mult)
        {
            std::string multiplier_string = floatToString(mult);
            multiplier_string.append("x");
            multiplier->SetString(multiplier_string);
        };

        void SetSelectedIcon(int id)
        {
            if (id == -1)
            {
                for (int i = 0; i < 4; i++)
                {
                    icons[i]->SetBlendMode(BlendMode::Opacity25);
                }
                multiplier->SetBlendMode(BlendMode::Opacity25);
            }
            else
            {
                for (int i = 0; i < 4; i++)
                {
                    if (i == id)
                    {
                        icons[i]->SetBlendMode(BlendMode::Normal);
                    }
                    else
                    {
                        icons[i]->SetBlendMode(BlendMode::Opacity25);
                    }
                }
                multiplier->SetBlendMode(BlendMode::Normal);
            }
        }

        void ToggleBaseSpeed()
        {
            base_speed_enabled = !base_speed_enabled;
            base_speed->SetBlendMode(base_speed_enabled ? BlendMode::Normal : BlendMode::Opacity25);
        }

        PositionAnimation pos_animation_in, pos_animation_out;
        std::shared_ptr<C_PositionAnimator> pos_transition;

    private:
        std::shared_ptr<C_Sprite> icons[4];
        std::shared_ptr<C_Draw> draw;
        std::shared_ptr<C_Sprite> bg;
        std::shared_ptr<C_Label> base_speed;
        bool base_speed_enabled = true;

        std::shared_ptr<C_Label> multiplier;

        int8_t icon_x[4] = {-27, -9, 9, 27};

        void InitIcons()
        {
            for (int i = 0; i < 4; i++)
            {
                icons[i] = AddComponent<C_Sprite>(9, 9);
                icons[i]->SetDrawLayer(DrawLayer::UI);
                icons[i]->SetBlendMode(BlendMode::Opacity25);
                icons[i]->SetAnchorPoint(Anchor::CENTER_);
                icons[i]->AddOffset(Vector2(icon_x[i], 2));
                icons[i]->Load((const uint8_t *)sats_icons[i], 9, 9);
                icons[i]->SetVisibility(false);
            }
        }

        void InitAnimation()
        {
        }
    };
}

#endif // OrbitControl_HPP
