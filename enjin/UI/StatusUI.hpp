#ifndef STATUSUI_HPP
#define STATUSUI_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../Components/C_Draw.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Object.hpp"
#include "../utils/Utils.hpp"
#include "../../unwnlib/SharedData.hpp"
#include "assets/status.h"

namespace enjin
{
    class StatusUI : public Object
    {
    public:
        StatusUI()
        {
            position->SetPosition(63, 63);
            draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                        { Draw(canvas); });
            draw->SetDrawLayer(DrawLayer::UI);
            draw->SetBlendMode(BlendMode::Normal);
            draw->SetAnchorPoint(Anchor::CENTER_);
            draw->SetVisibility(false);

            // Status icon sprite - centered in circle
            status_icon = AddComponent<C_Sprite>(32, 32);
            status_icon->SetDrawLayer(DrawLayer::UI);
            status_icon->SetBlendMode(BlendMode::Normal);
            status_icon->SetAnchorPoint(Anchor::CENTER_);
            status_icon->SetVisibility(false);

            // Initialize with default state
            current_state = DatumSaveState::Idle;
            is_visible = false;
            has_valid_sprite = false;
            current_frame_count = 0;
            saved_state_timer_ms = 0;
            frame_timer_ms = 0;
            current_frame_index = 0;

            InitAnimation();
        }

        void EnterTransition() { SetVisibility(true); }

        void ExitTransition() { SetVisibility(false); }

        void SetVisibility(bool visibility)
        {
            is_visible = visibility;
            draw->SetVisibility(visibility);
            UpdateIconVisibility();
            UpdateStatusIconFrame();
        }

        void SetStatus(DatumSaveState state)
        {
            if (state == DatumSaveState::Saved)
            {
                saved_state_timer_ms = SAVED_STATE_DISPLAY_MS;
            }
            else
            {
                saved_state_timer_ms = 0;
            }

            if (current_state != state)
            {
                current_state = state;
                LoadStatusIcon(state);
                UpdateStatusIconFrame();
            }
        }

        void Draw(EiseiCanvas &canvas)
        {
            // Draw the background circle - same as WarpUI
            canvas.fillCircle(position->x, position->y, 63, 0);
        }

        void Update(uint16_t deltaTime) override
        {
            Object::Update(deltaTime);

            UpdateSavedStateTimer(deltaTime);

            // Update animation frame if visible and not idle
            if (is_visible && current_state != DatumSaveState::Idle && current_state != DatumSaveState::Ready)
            {
                UpdateStatusIconFrame(deltaTime);
            }
        }

    private:
        struct StatusIconAsset
        {
            const uint8_t *frameBase;
            uint16_t width;
            uint16_t height;
            uint16_t frame_count;
            SpriteFormat format;
        };

        static constexpr uint16_t SAVED_STATE_DISPLAY_MS = 1000;
        static constexpr uint16_t FRAME_DURATION_MS = 200;

        std::shared_ptr<C_Draw> draw;
        std::shared_ptr<C_Sprite> status_icon;

        DatumSaveState current_state;
        bool is_visible;
        bool has_valid_sprite;
        uint16_t current_frame_count;
        uint16_t saved_state_timer_ms;
        uint16_t frame_timer_ms;
        uint16_t current_frame_index;

        void InitAnimation() {}

        void LoadStatusIcon(DatumSaveState state)
        {
            if (!status_icon)
            {
                return;
            }

            const StatusIconAsset asset = GetStatusAsset(state);

            if (asset.frameBase != nullptr && asset.frame_count > 0)
            {
                status_icon->Load(asset.frameBase,
                                  static_cast<uint8_t>(asset.width),
                                  static_cast<uint8_t>(asset.height),
                                  asset.format);

                status_icon->SetOffset(Vector2(static_cast<int16_t>(asset.width / 2),
                                               static_cast<int16_t>(asset.height / 2)));

                current_frame_count = asset.frame_count;
                has_valid_sprite = true;
            }
            else
            {
                current_frame_count = 0;
                has_valid_sprite = false;
            }

            frame_timer_ms = 0;
            current_frame_index = 0;

            if (has_valid_sprite)
            {
                status_icon->LoadFrame(0);
            }

            UpdateIconVisibility();
        }

        StatusIconAsset GetStatusAsset(DatumSaveState state) const
        {
            switch (state)
            {
            case DatumSaveState::Saving:
                return {&datum_save_frames_4bit[0][0], datum_save_frame_width, datum_save_frame_height, datum_save_frame_count, SpriteFormat::Grayscale4};
            case DatumSaveState::Saved:
                return {&datum_saved_frames_4bit[0][0], datum_saved_frame_width, datum_saved_frame_height, datum_saved_frame_count, SpriteFormat::Grayscale4};
            case DatumSaveState::Error:
                return {&datum_error_frames_4bit[0][0], datum_error_frame_width, datum_error_frame_height, datum_error_frame_count, SpriteFormat::Grayscale4};
            case DatumSaveState::Idle:
            case DatumSaveState::Ready:
            default:
                return {nullptr, 32, 32, 1, SpriteFormat::Grayscale4};
            }
        }

        void UpdateStatusIconFrame(uint16_t deltaTime = 0)
        {
            if (!status_icon || current_frame_count == 0 || !has_valid_sprite)
            {
                return;
            }

            // Check if current state should show animation
            bool shouldAnimate = (current_state == DatumSaveState::Saving ||
                                  current_state == DatumSaveState::Saved ||
                                  current_state == DatumSaveState::Error);

            if (!shouldAnimate)
            {
                current_frame_index = 0;
                frame_timer_ms = 0;
                status_icon->LoadFrame(0);
                return; // Don't animate for Idle/Ready states
            }

            if (current_frame_count == 1)
            {
                current_frame_index = 0;
                frame_timer_ms = 0;
                status_icon->LoadFrame(0);
                return;
            }

            uint32_t accumulated = static_cast<uint32_t>(frame_timer_ms) + static_cast<uint32_t>(deltaTime);
            while (accumulated >= FRAME_DURATION_MS)
            {
                accumulated -= FRAME_DURATION_MS;
                current_frame_index = static_cast<uint16_t>((current_frame_index + 1U) % current_frame_count);
            }

            frame_timer_ms = static_cast<uint16_t>(accumulated);
            status_icon->LoadFrame(static_cast<uint8_t>(current_frame_index));
        }

        void UpdateIconVisibility()
        {
            if (!status_icon)
            {
                return;
            }

            status_icon->SetVisibility(is_visible && has_valid_sprite);
        }

        void UpdateSavedStateTimer(uint16_t deltaTime)
        {
            if (current_state != DatumSaveState::Saved || saved_state_timer_ms == 0)
            {
                return;
            }

            if (deltaTime >= saved_state_timer_ms)
            {
                saved_state_timer_ms = 0;
                SetStatus(DatumSaveState::Ready);
                ExitTransition();
            }
            else
            {
                saved_state_timer_ms = static_cast<uint16_t>(saved_state_timer_ms - deltaTime);
            }
        }
    };
} // namespace enjin

#endif // STATUSUI_HPP
