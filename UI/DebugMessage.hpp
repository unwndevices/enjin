#ifndef DEBUGMESSAGE_HPP
#define DEBUGMESSAGE_HPP

#include "../Object.hpp"
#include "../Components/C_Draw.hpp"
#include "../Components/C_Label.hpp"

#include "assets/icons.h"
#include "assets/orbit.h"
#include "../utils/Utils.hpp"

namespace enjin
{
    class DebugMessage : public Object
    {
    public:
        DebugMessage()
        {
            position->SetPosition(63, 63);
            // the C_Draw component adds the object to the draw list
            draw = AddComponent<C_Draw>([this](EiseiCanvas &canvas)
                                        { Draw(canvas); });
            draw->SetDrawLayer(DrawLayer::UI);
            draw->SetBlendMode(BlendMode::Normal);
            draw->SetAnchorPoint(Anchor::CENTER_);
            draw->SetVisibility(false);
            // Initialize the message label
            line_1 = AddComponent<C_Label>(0, 0);
            line_1->SetDrawLayer(DrawLayer::UI);
            line_1->SetAnchorPoint(Anchor::CENTER_TOP);
            line_1->AddOffset(Vector2(0, -6));
            line_1->SetString("Debug Message");
            line_2 = AddComponent<C_Label>(0, 0);
            line_2->SetDrawLayer(DrawLayer::UI);
            line_2->SetAnchorPoint(Anchor::CENTER_TOP);
            line_2->AddOffset(Vector2(0, 6));
            line_2->SetString("f: 20.13");
            SetVisibility(false);
        }

        void Update(uint16_t deltaTime) override
        {
            Object::Update(deltaTime);
        };
        void Draw(EiseiCanvas &canvas)
        {
            canvas.fillRoundRect(position->GetPosition().x - 54, position->GetPosition().y - 14, 108, 28, 8, 0);
            canvas.drawRoundRect(position->GetPosition().x - 54, position->GetPosition().y - 14, 108, 28, 8, 6);
        };

        void SetMessage(const std::string &line_1, const std::string &line_2)
        {
            this->line_1->SetString(line_1.substr(0, 14));
            this->line_2->SetString(line_2.substr(0, 14));
        }

        void EnterTransition()
        {
            SetVisibility(true);
        }

        void ExitTransition()
        {
            SetVisibility(false);
        }

        void SetVisibility(bool visibility)
        {
            draw->SetVisibility(visibility);
            line_1->SetVisibility(visibility);
            line_2->SetVisibility(visibility);
        }

    private:
        std::shared_ptr<C_Label> line_1;
        std::shared_ptr<C_Label> line_2;
        std::shared_ptr<C_Draw> draw;
    };
}
#endif