#ifndef ITEMLIST_HPP
#define ITEMLIST_HPP

#include "../Object.hpp"
#include "../Components/C_List.hpp"
#include "../Components/C_PositionAnimator.hpp"

namespace enjin
{
    class ItemList : public Object
    {
    public:
        ItemList(const std::vector<std::string> &items, Vector2 position = Vector2(63, 63), uint8_t width = 64, uint8_t height = 127, C_List<std::string>::TextAlign align = C_List<std::string>::TextAlign::RIGHT)
        {
            this->position->SetPosition(position);

            list = AddComponent<C_List<std::string>>(
                items, [](const std::string &item)
                { return item; },
                width, height, align);
            list->SetDrawLayer(DrawLayer::UI);
            list->SetAnchorPoint(Anchor::CENTER_);

            transition = AddComponent<C_PositionAnimator>();

            in_transition.AddKeyframe({0, Vector2(130, 64), Easing::Step});
            in_transition.AddKeyframe({350, position, Easing::EaseOutQuad});
            transition->SetAnimation(in_transition);
        }

        void UpdateList(const std::vector<std::string> &newItems)
        {
            list->UpdateItems(newItems);
        }

        void MoveUp() { list->MoveUp(); }
        void MoveDown() { list->MoveDown(); }
        void SetCurrentSelection(uint8_t index) { list->SetCurrentSelection(index); }

        uint8_t GetCurrentSelectionIndex() const { return list->GetCurrentSelectionIndex(); }
        std::string GetCurrentSelection() const { return list->GetCurrentSelection(); }

        void SetPosition(Vector2 pos) { list->SetPosition(pos); }

        void EnterTransition()
        {
            list->SetVisibility(true);
            transition->StartAnimation();
        }

        void SetVisibility(bool visibility)
        {
            list->SetVisibility(visibility);
        }

    private:
        std::shared_ptr<C_List<std::string>> list;
        std::shared_ptr<C_PositionAnimator> transition;
        PositionAnimation in_transition;
    };
}
#endif // ITEMLIST_HPP
