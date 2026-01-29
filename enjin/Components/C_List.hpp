#ifndef C_LIST_HPP
#define C_LIST_HPP

#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "../enjin2_compat.hpp"

#include "../Object.hpp"
#include <memory>
#include <iostream>

namespace enjin
{
    template <typename T>
    class C_List : public C_Drawable
    {
    public:
        using GetStringFunc = std::function<std::string(const T &)>;

        // Text alignment options
        enum class TextAlign
        {
            LEFT,
            CENTER,
            RIGHT
        };

        C_List(Object *owner, const std::vector<T> &items, GetStringFunc getString, uint8_t width, uint8_t height, TextAlign textAlign = TextAlign::LEFT) : C_Drawable(width, height), Component(owner), marqueeCanvas(), items(items), getString(getString), selectedIndex(0), previousIndex(0), textAlign(textAlign)
        {
            position = owner->GetComponent<C_Position>();

            if (!position)
            {
                std::cerr << "C_Tooltip requires C_Position component.\n";
            }
        };
        void Awake() override
        {
            // position->SetPosition(Vector2(0, 0));
            marqueeOffset = 0;
            marqueeTimer = 0;
            marqueeStartDelay = 600;
            marqueeSpeed = 50;      // Delay before the start of the scroll, in milliseconds
            marqueeEndDelay = 1000; // Delay after the end of the scroll, in milliseconds
            maxTextWidth = width - 5;
        };
        void Update(uint16_t deltaTime) override
        {

            if (textWidth > maxTextWidth)
            {
                if (marqueeOffset > (int)textWidth - maxTextWidth)
                {
                    marqueeTimer += deltaTime;

                    // If the delay has passed, reset the marquee
                    if (marqueeTimer > marqueeEndDelay)
                    {
                        marqueeOffset = 0;
                        marqueeTimer = 0; // Reset the timer
                    }
                }
                // Otherwise, update the marquee offset after the start delay
                else
                {
                    marqueeTimer += deltaTime;
                    if (!marqueeOffset && marqueeTimer < marqueeStartDelay)
                    {
                        return;
                    }

                    else if (marqueeTimer > marqueeSpeed)
                    {
                        marqueeOffset++;
                        marqueeTimer = 0; // Reset the timer
                    }
                }
            }
        };

        void Draw(EiseiCanvas &canvas) override
        {
            // If the marquee has scrolled past the end of the text, start the delay
            // Check if selectedIndex has changed
            if (selectedIndex != previousIndex)
            {
                // Reset marqueeOffset
                marqueeOffset = 0;
                marqueeTimer = 0;

                // Update previousIndex
                previousIndex = selectedIndex;
            }
            std::string selected_item = getString(items[selectedIndex]);
            textWidth = canvas.getTextWidth(selected_item.c_str());

            // Calculate the start index and end index for the items to be displayed
            int start = std::max(0, selectedIndex - 2);
            int end = std::min((int)items.size(), selectedIndex + 3);

            // Calculate the y offset for drawing the items
            int yOffset = GetOffsetPosition().y + (canvas.height() / 2) + 4 - ((selectedIndex - start) * 22);

            // Draw the items with alignment
            for (int i = start; i < end; ++i)
            {
                // Determine text to draw (full for selected, truncated otherwise)
                std::string text = getString(items[i]);
                if (i != selectedIndex)
                {
                    text = text.substr(0, 12);
                }

                // Compute text width and alignment offset
                int w = canvas.getTextWidth(text.c_str());
                int alignOffset = 0;
                switch (textAlign)
                {
                case TextAlign::LEFT:
                    alignOffset = 0;
                    break;
                case TextAlign::CENTER:
                    alignOffset = (width - w) / 2;
                    break;
                case TextAlign::RIGHT:
                    alignOffset = (width - w);
                    break;
                }

                // Set the color based on selection
                canvas.setTextColor(i == selectedIndex ? 0xffff : 0x4);

                // Compute final cursor position with marquee offset for selected
                int x = GetOffsetPosition().x + alignOffset - (i == selectedIndex ? marqueeOffset : 0);
                int y = yOffset + (i - start) * 22;
                canvas.setCursor(x, y);
                canvas.print(text.c_str());
            }
        };

        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };
        void SetPosition(Vector2 pos) { position->SetPosition(pos); };

        ///////////////
        void MoveUp()
        {
            if (selectedIndex > 0)
            {
                --selectedIndex;
            }
        }

        void MoveDown()
        {
            if (selectedIndex < items.size() - 1)
            {
                ++selectedIndex;
            }
        }

        T GetCurrentSelection() const
        {
            return items[selectedIndex];
        }

        uint8_t GetCurrentSelectionIndex() const
        {
            return selectedIndex;
        }

        void SetCurrentSelection(uint8_t index)
        {
            selectedIndex = index;
        }

        void UpdateItems(const std::vector<T> &newItems)
        {
            items = newItems;
        }

        // Set text alignment
        void SetTextAlignment(TextAlign align) { textAlign = align; }

    private:
        EiseiCanvas marqueeCanvas;
        std::vector<T> items;
        GetStringFunc getString;

        int selectedIndex, previousIndex;

        int marqueeOffset = 0, marqueeTimer = 0, marqueeSpeed = 0, marqueeStartDelay = 0, marqueeEndDelay = 0, textWidth = 0,
            maxTextWidth = 0;

        // Text alignment state
        TextAlign textAlign;
    };
}
#endif // C_LIST_HPP
