#ifndef SAVEPROMPTUI_HPP
#define SAVEPROMPTUI_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../Components/C_Draw.hpp"
#include "../Components/C_Sprite.hpp"
#include "../Object.hpp"

#include "../utils/Utils.hpp"
#include "assets/datum_save.h"
#include "assets/nonnina_save.h"

namespace enjin
{
    enum class SavePromptState
    {
        Save,    // Initial prompt state
        Saving,  // User confirmed, saving in progress
        Success, // Save succeeded
        Error    // Save failed
    };

    class SavePromptUI : public Object
    {
    public:
        SavePromptUI()
        {
            position->SetPosition(63, 63);
            draw_bg = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                           { DrawBg(canvas); });
            draw_bg->SetDrawLayer(DrawLayer::Foreground);
            draw_bg->SetBlendMode(BlendMode::Normal);
            draw_bg->SetAnchorPoint(Anchor::CENTER_);
            draw_bg->SetVisibility(false);

            arrow_fill = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                              { DrawArrowFill(canvas); });
            arrow_fill->SetDrawLayer(DrawLayer::UI);
            arrow_fill->SetBlendMode(BlendMode::Normal);
            arrow_fill->SetAnchorPoint(Anchor::CENTER_);
            arrow_fill->SetVisibility(false);

            // Top icon (hand/tool/ok) - dynamically loaded based on state
            top_icon = AddComponent<C_Sprite>(static_cast<uint8_t>(hand_width),
                                              static_cast<uint8_t>(hand_height));
            top_icon->SetDrawLayer(DrawLayer::UI);
            top_icon->SetBlendMode(BlendMode::Normal);
            top_icon->SetAnchorPoint(Anchor::CENTER_);
            top_icon->AddOffset(Vector2(0, -21));
            top_icon->SetVisibility(false);
            top_icon->setMatte(0x0);
            top_icon->setMonochromePalette(0xFF, 0x00);
            top_icon->loadMonochrome(hand_data_1bit, hand_width, hand_height);

            // State-dependent text (SAVE?/SAVING/SUCCESS/ERROR)
            state_text = AddComponent<C_Sprite>(static_cast<uint8_t>(save_width),
                                                static_cast<uint8_t>(save_height));
            state_text->SetDrawLayer(DrawLayer::UI);
            state_text->SetBlendMode(BlendMode::Normal);
            state_text->SetAnchorPoint(Anchor::CENTER_);
            state_text->AddOffset(Vector2(0, 0));
            state_text->SetVisibility(false);
            state_text->setMatte(0x0);
            state_text->setMonochromePalette(0xFF, 0x00);
            state_text->loadMonochrome(save_data_1bit, save_width, save_height);

            // Left arrow (CCW = cancel)
            arrow_left = AddComponent<C_Sprite>(static_cast<uint8_t>(arrow_l_width),
                                                static_cast<uint8_t>(arrow_l_height));
            arrow_left->SetDrawLayer(DrawLayer::Foreground);
            arrow_left->SetBlendMode(BlendMode::Normal);
            arrow_left->SetAnchorPoint(Anchor::CENTER_);
            arrow_left->AddOffset(Vector2(-24, -5));
            arrow_left->SetVisibility(false);
            arrow_left->setMatte(0x0);
            arrow_left->setMonochromePalette(0x01, 0x00); // Dark color for background
            arrow_left->loadMonochrome(arrow_l_data_1bit, arrow_l_width, arrow_l_height);

            // Right arrow (CW = confirm)
            arrow_right = AddComponent<C_Sprite>(static_cast<uint8_t>(arrow_r_width),
                                                 static_cast<uint8_t>(arrow_r_height));
            arrow_right->SetDrawLayer(DrawLayer::Foreground);
            arrow_right->SetBlendMode(BlendMode::Normal);
            arrow_right->SetAnchorPoint(Anchor::CENTER_);
            arrow_right->AddOffset(Vector2(24, -5));
            arrow_right->SetVisibility(false);
            arrow_right->setMatte(0x0);
            arrow_right->setMonochromePalette(0x01, 0x00); // Dark color for background
            arrow_right->loadMonochrome(arrow_r_data_1bit, arrow_r_width, arrow_r_height);

            // No icon (cancel)
            no_icon = AddComponent<C_Sprite>(static_cast<uint8_t>(no_icon_width),
                                             static_cast<uint8_t>(no_icon_height));
            no_icon->SetDrawLayer(DrawLayer::UI);
            no_icon->SetBlendMode(BlendMode::Normal);
            no_icon->SetAnchorPoint(Anchor::CENTER_);
            no_icon->AddOffset(Vector2(-12, 22));
            no_icon->SetVisibility(false);
            no_icon->setMatte(0x0);
            no_icon->setMonochromePalette(0xFF, 0x00);
            no_icon->loadMonochrome(no_icon_data_1bit, no_icon_width, no_icon_height);

            // OK icon (confirm)
            ok_icon = AddComponent<C_Sprite>(static_cast<uint8_t>(ok_icon_width),
                                             static_cast<uint8_t>(ok_icon_height));
            ok_icon->SetDrawLayer(DrawLayer::UI);
            ok_icon->SetBlendMode(BlendMode::Normal);
            ok_icon->SetAnchorPoint(Anchor::CENTER_);
            ok_icon->AddOffset(Vector2(12, 22));
            ok_icon->SetVisibility(false);
            ok_icon->setMatte(0x0);
            ok_icon->setMonochromePalette(0xFF, 0x00);
            ok_icon->loadMonochrome(ok_icon_data_1bit, ok_icon_width, ok_icon_height);

            // Status animation sprite (nonnina_save)
            status_animation = AddComponent<C_Sprite>(static_cast<uint8_t>(nonnina_save_frame_width),
                                                      static_cast<uint8_t>(nonnina_save_frame_height));
            status_animation->SetDrawLayer(DrawLayer::UI);
            status_animation->SetBlendMode(BlendMode::Normal);
            status_animation->SetAnchorPoint(Anchor::CENTER_);
            status_animation->AddOffset(Vector2(0, 22)); // Positioned at ok/no icon level
            status_animation->SetVisibility(false);
            status_animation->setMatte(0x0);
            status_animation->setMonochromePalette(0xFF, 0x00);
            status_animation->Load(&nonnina_save_frames_1bit[0][0],
                                   static_cast<uint8_t>(nonnina_save_frame_width),
                                   static_cast<uint8_t>(nonnina_save_frame_height),
                                   SpriteFormat::Monochrome1);

            // Initialize state
            current_state = SavePromptState::Save;
            fill_left = 0.0f;
            fill_right = 0.0f;
            wheel_touched = false;
            is_visible = false;
            status_display_timer_ms = 0;
        }

        void EnterTransition() { SetVisibility(true); }

        void ExitTransition() { SetVisibility(false); }

        void SetVisibility(bool visibility)
        {
            is_visible = visibility;
            draw_bg->SetVisibility(visibility);
            arrow_fill->SetVisibility(visibility);
            UpdateElementVisibility();
        }

        void UpdateElementVisibility()
        {
            if (!is_visible)
            {
                // Hide everything when not visible
                top_icon->SetVisibility(false);
                state_text->SetVisibility(false);
                arrow_left->SetVisibility(false);
                arrow_right->SetVisibility(false);
                no_icon->SetVisibility(false);
                ok_icon->SetVisibility(false);
                status_animation->SetVisibility(false);
                return;
            }

            // state_text is always visible when UI is visible
            state_text->SetVisibility(true);

            // Show/hide elements based on current state
            switch (current_state)
            {
            case SavePromptState::Save:
                top_icon->SetVisibility(true);
                arrow_left->SetVisibility(true);
                arrow_right->SetVisibility(true);
                no_icon->SetVisibility(true);
                ok_icon->SetVisibility(true);
                status_animation->SetVisibility(false);
                break;
            case SavePromptState::Saving:
                top_icon->SetVisibility(true);
                arrow_left->SetVisibility(true);
                arrow_right->SetVisibility(true);
                no_icon->SetVisibility(false);
                ok_icon->SetVisibility(false);
                status_animation->SetVisibility(true);
                LoadStatusFrame();
                break;
            case SavePromptState::Success:
                top_icon->SetVisibility(true);
                arrow_left->SetVisibility(true);
                arrow_right->SetVisibility(true);
                no_icon->SetVisibility(false);
                ok_icon->SetVisibility(false);
                status_animation->SetVisibility(true);
                LoadStatusFrame();
                break;
            case SavePromptState::Error:
                // Hide top icon for error state
                top_icon->SetVisibility(true);
                arrow_left->SetVisibility(true);
                arrow_right->SetVisibility(true);
                no_icon->SetVisibility(false);
                ok_icon->SetVisibility(false);
                status_animation->SetVisibility(true);
                LoadStatusFrame();
                break;
            }
        }

        void DrawBg(EiseiCanvas &canvas)
        {
            // Draw the background circle - same as WarpUI/OrbitUI
            canvas.fillCircle(position->x, position->y, 40, 0);
            canvas.drawCircle(position->x, position->y, 40, 14);
        }

        void DrawArrowFill(EiseiCanvas &canvas)
        {
            if (current_state == SavePromptState::Save)
            {
                DrawArrowFill(canvas, arrow_l_data_1bit, arrow_l_width, arrow_l_height,
                              position->x - 24, position->y - 5, fill_left, false);
                DrawArrowFill(canvas, arrow_r_data_1bit, arrow_r_width, arrow_r_height,
                              position->x + 24, position->y - 5, fill_right, true);
            }
        }

        void Update(uint16_t deltaTime) override
        {
            Object::Update(deltaTime);
            UpdateStatusDisplayTimer(deltaTime);
        }

        void AccumulateRotation(float delta)
        {
            if (current_state != SavePromptState::Save)
            {
                return;
            }

            if (delta > 0.0f)
            {
                // Clockwise rotation
                if (fill_left > 0.0f)
                {
                    // Drain left arrow first before right can fill
                    fill_left = std::max(0.0f, fill_left - delta);
                }
                else
                {
                    // Fill right arrow
                    fill_right += delta;
                    fill_right = std::min(1.0f, fill_right);
                }
            }
            else if (delta < 0.0f)
            {
                // Counter-clockwise rotation
                const float abs_delta = std::abs(delta);
                if (fill_right > 0.0f)
                {
                    // Drain right arrow first before left can fill
                    fill_right = std::max(0.0f, fill_right - abs_delta);
                }
                else
                {
                    // Fill left arrow
                    fill_left += abs_delta;
                    fill_left = std::min(1.0f, fill_left);
                }
            }
        }

        void SetWheelTouched(bool touched)
        {
            // Reset fill on finger release
            if (wheel_touched && !touched)
            {
                fill_left = 0.0f;
                fill_right = 0.0f;
            }
            wheel_touched = touched;
        }

        bool IsConfirmed() const
        {
            return fill_right >= 1.0f;
        }

        bool IsCancelled() const
        {
            return fill_left >= 1.0f;
        }

        void Reset()
        {
            fill_left = 0.0f;
            fill_right = 0.0f;
            wheel_touched = false;
            current_state = SavePromptState::Save;
            LoadStateText(SavePromptState::Save);
            LoadStateIcon(SavePromptState::Save);
        }

        void SetState(SavePromptState state)
        {
            if (current_state != state)
            {
                current_state = state;
                LoadStateText(state);
                LoadStateIcon(state);
                // Start timer for Success/Error states
                if (state == SavePromptState::Success || state == SavePromptState::Error)
                {
                    status_display_timer_ms = STATUS_DISPLAY_DURATION_MS;
                }
                else
                {
                    status_display_timer_ms = 0;
                }
                UpdateElementVisibility();
            }
        }

        SavePromptState GetState() const
        {
            return current_state;
        }

        bool ShouldDismiss() const
        {
            return status_display_timer_ms == 0 &&
                   (current_state == SavePromptState::Success || current_state == SavePromptState::Error);
        }

    private:
        static constexpr uint16_t STATUS_DISPLAY_DURATION_MS = 1500; // 1.5 seconds

        struct TextAsset
        {
            const uint8_t *data;
            uint16_t width;
            uint16_t height;
        };

        struct IconAsset
        {
            const uint8_t *data;
            uint16_t width;
            uint16_t height;
        };

        TextAsset GetTextAsset(SavePromptState state) const
        {
            switch (state)
            {
            case SavePromptState::Saving:
                return {saving_data_1bit, saving_width, saving_height};
            case SavePromptState::Success:
                return {success_data_1bit, success_width, success_height};
            case SavePromptState::Error:
                return {error_data_1bit, error_width, error_height};
            default: // Save
                return {save_data_1bit, save_width, save_height};
            }
        }

        IconAsset GetIconAsset(SavePromptState state) const
        {
            switch (state)
            {
            case SavePromptState::Saving:
                return {tool_data_1bit, tool_width, tool_height};
            case SavePromptState::Success:
                return {ok_icon_data_1bit, ok_icon_width, ok_icon_height};
            case SavePromptState::Error:
                return {no_icon_data_1bit, no_icon_width, no_icon_height};
            default: // Save
                return {hand_data_1bit, hand_width, hand_height};
            }
        }

        void LoadStateText(SavePromptState state)
        {
            if (!state_text)
                return;
            const TextAsset asset = GetTextAsset(state);
            state_text->loadMonochrome(asset.data, asset.width, asset.height);
            // Recalculate anchor offset for CENTER_ anchor based on new dimensions
            // The anchor offset should be width/2, height/2 for CENTER_
            state_text->SetOffset(Vector2(static_cast<int16_t>(asset.width / 2),
                                          static_cast<int16_t>(asset.height / 2)));
        }

        void LoadStateIcon(SavePromptState state)
        {
            if (!top_icon)
                return;
            const IconAsset asset = GetIconAsset(state);
            top_icon->loadMonochrome(asset.data, asset.width, asset.height);
            // Recalculate anchor offset for CENTER_ anchor based on new dimensions
            // SetAnchorPoint recalculates the anchor offset, then we reapply the position offset
            top_icon->SetAnchorPoint(Anchor::CENTER_);
            top_icon->AddOffset(Vector2(0, -21)); // Reapply the position offset
        }

        std::shared_ptr<C_Draw> draw_bg;
        std::shared_ptr<C_Draw> arrow_fill;
        std::shared_ptr<C_Sprite> top_icon;   // Dynamic icon: hand/tool/ok based on state
        std::shared_ptr<C_Sprite> state_text; // Dynamic text: SAVE?/SAVING/SUCCESS/ERROR
        std::shared_ptr<C_Sprite> arrow_left;
        std::shared_ptr<C_Sprite> arrow_right;
        std::shared_ptr<C_Sprite> no_icon;
        std::shared_ptr<C_Sprite> ok_icon;
        std::shared_ptr<C_Sprite> status_animation;

        SavePromptState current_state;
        float fill_left = 0.0f;  // CCW rotation accumulator (0.0 to 1.0)
        float fill_right = 0.0f; // CW rotation accumulator (0.0 to 1.0)
        bool wheel_touched = false;
        bool is_visible = false;
        uint16_t status_display_timer_ms = 0;

        // Helper function to check if a bit is set in 1-bit packed data
        bool IsBitSet(const uint8_t *data, uint16_t width, uint16_t x, uint16_t y) const
        {
            const uint16_t row_stride = (width + 7) / 8;
            const uint8_t *row = data + (y * row_stride);
            const uint8_t byte_index = x / 8;
            const uint8_t bit_index = x % 8;
            return (row[byte_index] & (0x80 >> bit_index)) != 0;
        }

        void DrawArrowFill(EiseiCanvas &canvas, const uint8_t *arrow_data,
                           uint16_t arrow_width, uint16_t arrow_height,
                           int16_t center_x, int16_t center_y,
                           float fill_amount, bool is_right)
        {
            if (fill_amount <= 0.0f)
            {
                return;
            }

            const int16_t arrow_left_x = center_x - static_cast<int16_t>(arrow_width / 2);
            const int16_t arrow_top_y = center_y - static_cast<int16_t>(arrow_height / 2);
            const uint16_t fill_height = static_cast<uint16_t>(fill_amount * static_cast<float>(arrow_height));

            // Draw horizontal lines from top to fill_height
            for (uint16_t y = 0; y < fill_height && y < arrow_height; ++y)
            {
                // Find the leftmost and rightmost pixels in this row that are part of the arrow
                int16_t leftmost = -1;
                int16_t rightmost = -1;

                for (uint16_t x = 0; x < arrow_width; ++x)
                {
                    if (IsBitSet(arrow_data, arrow_width, x, y))
                    {
                        if (leftmost == -1)
                        {
                            leftmost = static_cast<int16_t>(x);
                        }
                        rightmost = static_cast<int16_t>(x);
                    }
                }

                // Draw horizontal line if we found arrow pixels in this row
                if (leftmost >= 0 && rightmost >= 0)
                {
                    const int16_t canvas_x = arrow_left_x + leftmost;
                    const int16_t canvas_y = arrow_top_y + static_cast<int16_t>(y);
                    const int16_t line_width = rightmost - leftmost + 1;
                    canvas.drawLine(canvas_x, canvas_y, canvas_x + line_width - 1, canvas_y, 0xFF);
                }
            }
        }

        void LoadStatusFrame()
        {
            if (!status_animation)
                return;

            uint8_t frame_index = 0;
            switch (current_state)
            {
            case SavePromptState::Saving:
                frame_index = 0; // Saving frame (static)
                break;
            case SavePromptState::Success:
                frame_index = 1; // Success frame (static)
                break;
            case SavePromptState::Error:
                frame_index = 2; // Error frame (static)
                break;
            default:
                return;
            }
            status_animation->LoadFrame(frame_index);
        }

        void UpdateStatusDisplayTimer(uint16_t deltaTime)
        {
            if (status_display_timer_ms == 0)
            {
                return;
            }

            if (deltaTime >= status_display_timer_ms)
            {
                status_display_timer_ms = 0;
            }
            else
            {
                status_display_timer_ms = static_cast<uint16_t>(status_display_timer_ms - deltaTime);
            }
        }
    };
} // namespace enjin

#endif // SAVEPROMPTUI_HPP
