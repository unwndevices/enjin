#ifndef WARPUI_HPP
#define WARPUI_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../Components/C_Canvas.hpp"
#include "../Components/C_Draw.hpp"
#include "../Components/C_Label.hpp"
#include "../Components/C_PositionAnimator.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Object.hpp"

#include "../utils/Utils.hpp"
#include "assets/shift.h"
#include "assets/slider.h"
#include "assets/shape.h"
#include "assets/tanh.h"
#include "assets/blur.h"
#include "assets/warp_name.h"
#include "../../unwnlib/SharedData.hpp"

namespace enjin
{
    class WarpUI : public Object
    {
    public:
        WarpUI()
        {
            position->SetPosition(63, 63);
            draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                        { Draw(canvas); });
            draw->SetDrawLayer(DrawLayer::Foreground);
            draw->SetBlendMode(BlendMode::Normal);
            draw->SetAnchorPoint(Anchor::CENTER_);
            draw->SetVisibility(false);

            // Icon for warp type - positioned at top of circle
            warp_icon = AddComponent<C_Sprite>(static_cast<uint8_t>(shift_frame_width),
                                               static_cast<uint8_t>(shift_frame_height));
            warp_icon->SetDrawLayer(DrawLayer::UI);
            warp_icon->SetBlendMode(BlendMode::Normal);
            warp_icon->SetAnchorPoint(Anchor::CENTER_);
            warp_icon->SetVisibility(false);
            warp_icon->setMatte(0x0);
            warp_icon->setMonochromePalette(0xFF, 0x00);

            // Slider for warp amount - positioned at bottom of circle
            warp_slider = AddComponent<C_Sprite>(static_cast<uint8_t>(slider_frame_width),
                                                 static_cast<uint8_t>(slider_frame_height));
            warp_slider->SetDrawLayer(DrawLayer::UI);
            warp_slider->SetBlendMode(BlendMode::Normal);
            warp_slider->SetAnchorPoint(Anchor::CENTER_);
            warp_slider->AddOffset(Vector2(0, 24));
            warp_slider->SetVisibility(false);
            warp_slider->Load(&slider_frames_1bit[0][0],
                              static_cast<uint8_t>(slider_frame_width),
                              static_cast<uint8_t>(slider_frame_height),
                              SpriteFormat::Monochrome1);
            warp_slider->setMatte(0x0);
            warp_slider->setMonochromePalette(0xFF, 0x00);

            view_name = AddComponent<C_Sprite>(static_cast<uint8_t>(warp_names_frame_width),
                                               static_cast<uint8_t>(warp_names_frame_height));
            view_name->SetDrawLayer(DrawLayer::UI);
            view_name->SetBlendMode(BlendMode::Normal);
            view_name->SetAnchorPoint(Anchor::CENTER_);
            view_name->AddOffset(Vector2(0, -20));
            view_name->SetVisibility(false);
            view_name->setMatte(0x0);
            view_name->setMonochromePalette(0xFF, 0x00);
            view_name->loadMonochrome(warp_names_frames_1bit[0], warp_names_frame_width, warp_names_frame_height);

            // Initialize with default values
            SetWarpType(0, 0.0f);

            InitAnimation();
        };

        void EnterTransition() { SetVisibility(true); }

        void ExitTransition() { SetVisibility(false); };

        void SetVisibility(bool visibility)
        {
            is_visible = visibility;
            draw->SetVisibility(visibility);
            warp_icon->SetVisibility(visibility);
            warp_slider->SetVisibility(visibility);
            view_name->SetVisibility(visibility);
            UpdateWarpIconFrame();
        }

        void Draw(EiseiCanvas &canvas)
        {
            // Draw the background circle - same as OrbitUI
            canvas.fillCircle(position->x, position->y, 40, 0);
            canvas.drawCircle(position->x, position->y, 40, 14);
            canvas.drawLine(position->x - 31, position->y - 13, position->x + 31, position->y - 13, 4);
            canvas.drawLine(position->x - 31, position->y + 15, position->x + 31, position->y + 15, 4);
        }

        void Update(uint16_t deltaTime) override { Object::Update(deltaTime); };

        void SetWarpType(uint8_t type, float amount)
        {
            this->current_type = type;
            LoadWarpIcon(this->current_type);
            if (view_name)
            {
                const uint8_t frameIndex = (type < 4) ? type : 0;
                view_name->LoadFrame(frameIndex);
            }
            SetAmount(amount);
        };

        void SetAmount(float amount)
        {
            this->current_amount = amount;
            UpdateWarpIconFrame();
        };

        void SetWarpCvAmount(float amount)
        {
            current_cv_amount = std::max(-1.0f, std::min(1.0f, amount));
            UpdateWarpIconFrame();
        };

        void SetWarpCvDestination(int8_t destination)
        {
            current_cv_destination = destination;
            UpdateWarpIconFrame();
        };

        PositionAnimation pos_animation_in, pos_animation_out;
        std::shared_ptr<C_PositionAnimator> pos_transition;

    private:
        struct WarpIconAsset
        {
            const uint8_t *frameBase;
            uint16_t width;
            uint16_t height;
            uint16_t frame_count;
            SpriteFormat format;
        };

        static constexpr int16_t warp_icon_vertical_offset = -2;

        std::shared_ptr<C_Draw> draw;
        std::shared_ptr<C_Sprite> warp_icon;
        std::shared_ptr<C_Sprite> warp_slider;
        std::shared_ptr<C_Sprite> view_name;

        uint8_t current_type = 0;
        float current_amount = 0.0f;
        float current_cv_amount = 0.0f;
        int8_t current_cv_destination = -1;
        bool is_visible = false;
        uint16_t current_icon_frame_count = shift_frame_count;

        void InitAnimation() {}
        void LoadWarpIcon(uint8_t type)
        {
            if (!warp_icon)
            {
                return;
            }

            const WarpIconAsset asset = GetWarpAsset(type);

            warp_icon->Load(asset.frameBase,
                            static_cast<uint8_t>(asset.width),
                            static_cast<uint8_t>(asset.height),
                            asset.format);

            warp_icon->SetOffset(Vector2(static_cast<int16_t>(asset.width / 2),
                                         static_cast<int16_t>(asset.height / 2 + warp_icon_vertical_offset)));

            current_icon_frame_count = asset.frame_count;
        }

        WarpIconAsset GetWarpAsset(uint8_t type) const
        {
            switch (type)
            {
            case 1:
                return {&tanh_frames_1bit[0][0], tanh_frame_width, tanh_frame_height, tanh_frame_count, SpriteFormat::Monochrome1};
            case 2:
                return {&shape_frames_1bit[0][0], shape_frame_width, shape_frame_height, shape_frame_count, SpriteFormat::Monochrome1};
            case 3:
                return {&blur_frames_1bit[0][0], blur_frame_width, blur_frame_height, blur_frame_count, SpriteFormat::Monochrome1};
            default:
                return {&shift_frames_1bit[0][0], shift_frame_width, shift_frame_height, shift_frame_count, SpriteFormat::Monochrome1};
            }
        }

        void UpdateWarpIconFrame()
        {
            if (!warp_icon || !warp_slider)
            {
                return;
            }

            const uint16_t frame_count = current_icon_frame_count > 0 ? current_icon_frame_count : 1;
            const float effective_amount = GetEffectiveAmount();

            // Map icon animation from native range (kWarpMin to kWarpMax) to 0-1 for full frame range
            const float min = kWarpMin[current_type];
            const float max = kWarpMax[current_type];
            float icon_normalized = 0.0f;
            if (max > min)
            {
                const float clamped = std::max(min, std::min(max, effective_amount));
                icon_normalized = (clamped - min) / (max - min);
            }
            const float icon_scaled = icon_normalized * static_cast<float>(frame_count - 1);
            const uint8_t iconFrameIndex = static_cast<uint8_t>(std::round(icon_scaled));
            warp_icon->LoadFrame(iconFrameIndex);

            const float slider_normalized = std::min(1.0f, std::max(0.0f, (current_amount * 0.5f) + 0.5f));
            const float slider_scaled = slider_normalized * static_cast<float>(slider_frame_count - 1);
            const uint8_t sliderFrameIndex = static_cast<uint8_t>(std::round(slider_scaled));
            warp_slider->LoadFrame(std::min<uint8_t>(sliderFrameIndex, static_cast<uint8_t>(slider_frame_count - 1)));
        }

        float GetEffectiveAmount() const
        {
            float amount = current_amount;
            if (is_visible && current_cv_destination >= 0 && static_cast<uint8_t>(current_cv_destination) == current_type)
            {
                amount = current_amount + current_cv_amount;
            }
            // Clamp to the warp type's native range
            const float min = kWarpMin[current_type];
            const float max = kWarpMax[current_type];
            return std::max(min, std::min(max, amount));
        }

        std::string formatPercentage(float value)
        {
            // Convert -1.0 to 1.0 range to -100.0% to 100.0%
            float percentage = value * 100.0f;
            char buffer[16];
            snprintf(buffer, sizeof(buffer), "%.1f", percentage);
            return std::string(buffer);
        }
    };
} // namespace enjin

#endif // WARPUI_HPP
