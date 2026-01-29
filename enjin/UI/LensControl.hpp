#ifndef LENSCONTROL_HPP
#define LENSCONTROL_HPP

#include <algorithm>

#include "../Object.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Components/C_CurvedSlider.hpp"
#include "../Components/C_Canvas.hpp"
#include "../Components/C_PositionAnimator.hpp"
#include "assets/icons.h"
#include "assets/resources.h"

#include "../Components/C_Draw.hpp"
#include "../utils/Dither.hpp"

namespace enjin
{
    static constexpr size_t LUT_SIZE = 63;
    static constexpr size_t LUT_AMOUNT = 2048;

    class LensControl : public Object
    {
    public:
        LensControl()
        {
            generateLUT();
            generateGradientPattern();

            draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                        { Draw(canvas); });
            draw->SetDrawLayer(DrawLayer::Overlay);
            draw->SetBlendMode(BlendMode::Normal);
            draw->SetAnchorPoint(Anchor::CENTER_);
            draw->SetVisibility(false);

            stars = AddComponent<C_Sprite>(127, 64);
            stars->Load((const uint8_t *)star_field, 127, 64);
            stars->SetDrawLayer(DrawLayer::Overlay);
            stars->SetBlendMode(BlendMode::Difference);
            stars->SetAnchorPoint(Anchor::CENTER_BOTTOM);
            stars->AddOffset(Vector2(0, -34));
            stars->SetVisibility(false);

            stars2 = AddComponent<C_Sprite>(127, 64);
            stars2->Load((const uint8_t *)star_field, 127, 64);
            stars2->SetDrawLayer(DrawLayer::Overlay);
            stars2->SetBlendMode(BlendMode::Difference);
            stars2->SetAnchorPoint(Anchor::CENTER_TOP);
            stars2->AddOffset(Vector2(0, -34));
            stars2->SetVisibility(false);

            icon = AddComponent<C_Sprite>(39, 13);
            icon->Load(lens_sat, 39, 13);
            icon->SetAnchorPoint(Anchor::CENTER_);
            icon->SetDrawLayer(DrawLayer::Overlay);
            icon->SetBlendMode(BlendMode::Normal);
            icon->SetVisibility(false);

            InitAnimation();
        };

        void EnterTransition()
        {
            SetVisibility(true);
            pos_transition->SetAnimation(pos_animation_in);
            lens_transition->SetAnimation(lens_animation_in);
            sat_transition->SetAnimation(sat_animation_in);
            pos_transition->StartAnimation(true);
            lens_transition->StartAnimation(true);
            sat_transition->StartAnimation(true);
        }

        void ExitTransition()
        {
            pos_transition->SetAnimation(pos_animation_out);
            lens_transition->SetAnimation(lens_animation_out);
            sat_transition->SetAnimation(sat_animation_out);
            pos_transition->StartAnimation();
            lens_transition->StartAnimation(true);
            sat_transition->StartAnimation();
        };

        void SetVisibility(bool visibility)
        {
            draw->SetVisibility(visibility);
            stars->SetVisibility(visibility);
            stars2->SetVisibility(visibility);
            icon->SetVisibility(visibility);
        }

        void Draw(EiseiCanvas &canvas)
        {
            DrawLens(canvas);
        }

        void Update(uint16_t deltaTime) override
        {
            Object::Update(deltaTime);
            timeElapsed += deltaTime;
            if (timeElapsed >= 300)
            {
                timeElapsed = 0;
                uint8_t id = rand() % 5;
                stars->LoadFrame(id);
                id = rand() % 5;
                stars2->LoadFrame(id);
            }
        };

        void SetLens(float value) { c_value = value; }
        void SetAnimationValue(float value) { animation_value = value; }
        float GetLens() { return c_value; }

        PositionAnimation pos_animation_in, pos_animation_out;
        std::shared_ptr<C_PositionAnimator> pos_transition;

        ParameterAnimation<float> lens_animation_in, lens_animation_out;
        std::shared_ptr<C_ParameterAnimator<float>> lens_transition;

        ParameterAnimation<int16_t> sat_animation_in, sat_animation_out;
        std::shared_ptr<C_ParameterAnimator<int16_t>> sat_transition;

    private:
        std::shared_ptr<C_Sprite> icon;
        std::shared_ptr<C_Draw> draw;
        std::shared_ptr<C_Sprite> stars;
        std::shared_ptr<C_Sprite> stars2;
        unsigned long timeElapsed = 0;

        std::vector<uint8_t> ditheredGradient;
        const int gradientWidth = 8;
        const int gradientHeight = 127;

        float c_value = 0.2f;
        float animation_value = 0.f;
        uint8_t lut[LUT_AMOUNT][LUT_SIZE];

        void InitAnimation()
        {
            position->SetPosition(63, -63);
            pos_transition = AddComponent<C_PositionAnimator>();
            pos_animation_in.AddKeyframe({0, Vector2(63, -63), Easing::Step});
            pos_animation_in.AddKeyframe({200, Vector2(63, 100), Easing::EaseOutQuad});

            pos_animation_out.AddKeyframe({0, Vector2(63, 100), Easing::Step});
            pos_animation_out.AddKeyframe({200, Vector2(63, -63), Easing::EaseInQuad});

            lens_transition = AddComponent<C_ParameterAnimator<float>>();
            lens_transition->SetParameterGetter(std::bind(&LensControl::GetLens, this));
            lens_transition->SetParameterSetter(std::bind(&LensControl::SetAnimationValue, this, std::placeholders::_1));

            lens_animation_in.AddKeyframe({0, 0.f, Easing::Step});
            lens_animation_in.AddKeyframe({30, 0.f, Easing::Step});
            lens_animation_in.AddKeyframe({300, 1.f, Easing::EaseInOutSine});

            lens_animation_out.AddKeyframe({0, 1.f, Easing::Step});
            lens_animation_out.AddKeyframe({100, 0.f, Easing::EaseInOutSine});
            lens_animation_out.SetEndCallback([this]()
                                              { this->draw->SetVisibility(false); });

            sat_transition = AddComponent<C_ParameterAnimator<int16_t>>();
            sat_transition->SetParameterGetter([this]()
                                               { return this->icon->GetOffsetPosition().y; });
            sat_transition->SetParameterSetter([this](int16_t pos)
                                               { this->icon->SetYOffset(pos); });

            sat_animation_in.AddKeyframe({0, -200, Easing::Step});
            sat_animation_in.AddKeyframe({200, -1, Easing::EaseOutQuad});

            sat_animation_out.AddKeyframe({0, 0, Easing::Step});
            sat_animation_out.AddKeyframe({200, 0, Easing::Linear});
            sat_animation_out.SetEndCallback([this]()
                                             { this->icon->SetVisibility(false); });
        }

        void generateGradientPattern()
        {
            ditheredGradient.resize(gradientWidth * gradientHeight);
            Utils::generateDitheredGradient(
                ditheredGradient.data(),
                gradientWidth, gradientHeight,
                Utils::BayerPatternType::BAYER_8x8,
                190, 0,
                8U, 0);
        }

        void generateLUT()
        {
            for (size_t x = 0; x < LUT_AMOUNT; ++x)
            {
                for (size_t y = 0; y < LUT_SIZE; ++y)
                {
                    float nx = static_cast<float>(x) / (LUT_AMOUNT) * 2.0f - 1.0f;
                    float ny = static_cast<float>(y) / (LUT_SIZE);
                    float width = 1.0f;
                    float exponent = 1.0f;
                    float value = 1.0f;
                    if (nx >= 0.0f)
                    {
                        width = nx <= 0.5f ? 0.7f : 0.7f * ((nx - 0.5f) * 1.2f + 1.f);
                        exponent = nx <= 0.5f ? (std::abs(nx) * 12.0f + 0.1f) : 6.0f + 0.1f;
                        value = (1.f - std::pow(ny / width, exponent));
                    }
                    else if (nx < 0.0f)
                    {
                        nx *= 0.95f;
                        width = 0.7f;
                        exponent = std::abs(sqrt(1.0f - std::pow(1.f - std::abs(nx), 2.f))) * 0.35f + 0.1f;
                        float period = 1.0f - std::abs(nx);
                        period = std::round(period * 20.f) / 20.f / 2.f;

                        ny = period - std::abs(std::fmod(ny, 2.0f * period) - period);

                        value = (1.f - std::pow(ny / width, exponent));
                    }

                    value = std::max(0.0f, std::min(1.0f, value));

                    lut[x][y] = static_cast<uint8_t>(value * 100.0f);
                }
            }
        }

        void DrawLens(EiseiCanvas &canvas)
        {
            uint16_t id = static_cast<uint16_t>((c_value * animation_value * 0.5f + 0.5f) * (LUT_AMOUNT - 1));

            const int16_t draw_width = 2 * LUT_SIZE - 1;
            const int16_t draw_height = 100;
            const int16_t draw_x = position->x - (LUT_SIZE - 1);
            const int16_t draw_y = position->y - draw_height;

            GFXcanvas1 maskCanvas(draw_width, draw_height);
            maskCanvas.fillScreen(0);

            for (size_t i = 0; i < LUT_SIZE; ++i)
            {
                uint8_t h = lut[id][i];
                if (h > 0)
                {
                    h = std::min((uint8_t)draw_height, h);
                    int16_t mask_x_pos = (LUT_SIZE - 1) + i;
                    int16_t mask_x_neg = (LUT_SIZE - 1) - i;

                    maskCanvas.drawFastVLine(mask_x_pos, 0, h, 1);
                    if (i != 0)
                    {
                        maskCanvas.drawFastVLine(mask_x_neg, 0, h, 1);
                    }
                }
            }

            EiseiCanvas internalCanvas;
            internalCanvas.fillScreen(16U);
            internalCanvas.fillRectWithPattern(0, 0, draw_width, draw_height, ditheredGradient.data(), gradientWidth, gradientHeight);

            canvas.drawGrayscaleBitmap(draw_x, draw_y, internalCanvas.getBuffer(), maskCanvas.getBuffer(), draw_width, draw_height);
        }
    };
}

#endif // LENSCONTROL_HPP
