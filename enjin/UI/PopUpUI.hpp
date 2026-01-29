#ifndef POPUPUI_HPP
#define POPUPUI_HPP

#include "../Object.hpp"
#include "../Components/C_Draw.hpp"
#include "../Components/C_Label.hpp"
#include "Fonts/awkward.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace enjin
{
    class PopUpUI : public Object
    {
    public:
        enum class Icon : uint8_t
        {
            None = 0,
            Save,
            Info,
            Warning
        };

        PopUpUI()
        {
            position->SetPosition(63, 63);

            background_ = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                               { Draw(canvas); });
            background_->SetDrawLayer(DrawLayer::UI);
            background_->SetBlendMode(BlendMode::Normal);
            background_->SetAnchorPoint(Anchor::CENTER_);
            background_->SetVisibility(false);

            line1_ = AddComponent<C_Label>(100, 16, &Awkward8pt7b, 2, 14);
            line1_->SetDrawLayer(DrawLayer::UI);
            line1_->SetAnchorPoint(Anchor::CENTER_);
            line1_->AddOffset(Vector2(0, 0));
            line1_->SetVisibility(false);

            line2_ = AddComponent<C_Label>(100, 16, &Awkward8pt7b, 2, 14);
            line2_->SetDrawLayer(DrawLayer::UI);
            line2_->SetAnchorPoint(Anchor::CENTER_);
            line2_->AddOffset(Vector2(0, 16));
            line2_->SetVisibility(false);
        }

        void Update(uint16_t deltaTime) override
        {
            Object::Update(deltaTime);
            if (!visible_ || autoHideMs_ == 0)
            {
                return;
            }

            if (elapsedMs_ >= autoHideMs_)
            {
                Hide();
            }
            else
            {
                elapsedMs_ = static_cast<uint16_t>(std::min<uint32_t>(elapsedMs_ + deltaTime, 0xFFFF));
            }
        }

        void SetLines(const std::string &line1, const std::string &line2)
        {
            line1_->SetString(line1.substr(0, 18));
            line2_->SetString(line2.substr(0, 18));
        }

        void SetIcon(Icon icon) { icon_ = icon; }

        void Show(uint16_t autoHideMs = 0)
        {
            autoHideMs_ = autoHideMs;
            elapsedMs_ = 0;
            SetVisibility(true);
        }

        void Hide()
        {
            autoHideMs_ = 0;
            elapsedMs_ = 0;
            SetVisibility(false);
        }

        bool IsVisible() const { return visible_; }

    private:
        void SetVisibility(bool visible)
        {
            visible_ = visible;
            background_->SetVisibility(visible);
            line1_->SetVisibility(visible);
            line2_->SetVisibility(visible);
        }

        void Draw(EiseiCanvas &canvas)
        {
            if (!visible_)
            {
                return;
            }

            constexpr int16_t radius = 44;
            canvas.fillCircle(position->x, position->y, radius, 0);
            canvas.drawCircle(position->x, position->y, radius, 6);

            DrawIcon(canvas);
        }

        void DrawIcon(EiseiCanvas &canvas)
        {
            const int16_t iconSize = 12;
            const int16_t left = position->x - iconSize / 2;
            const int16_t top = position->y - 28;
            switch (icon_)
            {
            case Icon::Save:
                canvas.fillRect(left, top, iconSize, iconSize + 2, 12);
                canvas.fillRect(left + 2, top + 2, iconSize - 4, 4, 0);
                canvas.fillRect(left + 3, top + 7, iconSize - 6, 3, 8);
                canvas.drawRect(left, top, iconSize, iconSize + 2, 6);
                break;
            case Icon::Info:
                canvas.fillCircle(position->x, top + 7, iconSize / 2 + 1, 10);
                canvas.fillCircle(position->x, top + 3, 1, 0);
                canvas.fillRect(position->x - 1, top + 6, 3, 6, 0);
                break;
            case Icon::Warning:
                canvas.fillTriangle(position->x - 6, top + iconSize + 4,
                                    position->x + 6, top + iconSize + 4,
                                    position->x, top - 2, 10);
                canvas.fillRect(position->x - 1, top + 3, 2, 6, 0);
                canvas.fillRect(position->x - 1, top + 11, 2, 2, 0);
                break;
            case Icon::None:
            default:
                break;
            }
        }

        std::shared_ptr<C_Draw> background_;
        std::shared_ptr<C_Label> line1_;
        std::shared_ptr<C_Label> line2_;

        Icon icon_ = Icon::None;
        bool visible_ = false;
        uint16_t autoHideMs_ = 0;
        uint16_t elapsedMs_ = 0;
    };
}

#endif // POPUPUI_HPP
