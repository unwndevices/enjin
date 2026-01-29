#include <iostream>

#include "C_Probe.hpp"
#include "../Object.hpp"

namespace enjin
{
    uint8_t C_Probe::amount = 0;

    C_Probe::C_Probe(Object *owner, uint8_t from_center, uint8_t radius, uint8_t color) : C_Drawable(127, 127), Component(owner),
                                                                                          phase(0.0f), from_center(from_center),
                                                                                          radius(radius),
                                                                                          color(color)
    {
        position = owner->GetComponent<C_Position>();
        if (!position)
        {
            std::cerr << "C_Probe requires C_Position component.\n";
        }
    };

    void C_Probe::Awake()
    {
        C_Probe::amount++;
        identity = C_Probe::amount;
        SetAnchorPoint(Anchor::CENTER_);
        probe_position = RadialToCartesian(phase, from_center, abs_center);
    }

    void C_Probe::Update(uint16_t deltaTime)
    {
        probe_position = RadialToCartesian(phase, from_center, abs_center);
    };
    void C_Probe::Draw(EiseiCanvas &canvas)
    {
        int8_t scaledColor = 0;
        if (GetBlendMode() == BlendMode::Normal)
        {
            scaledColor = color;
        }
        else if (GetBlendMode() == BlendMode::Opacity50)
        {
            scaledColor = color / 2;
        }
        else if (GetBlendMode() == BlendMode::Opacity25)
        {
            scaledColor = color / 4;
        }

        canvas.fillCircle(probe_position.x, probe_position.y, 6 + 1, 0);
        canvas.fillCircle(probe_position.x, probe_position.y, 6, scaledColor - 2);
        canvas.setTextColor(0);
        uint8_t text_width = 20; // TODO temporary! canvas.getTextWidth(label);
        canvas.setCursor(probe_position.x - (text_width / 2), probe_position.y + 3);
        // canvas.println(label);
    };

    bool C_Probe::ContinueToDraw() const
    {
        return !owner->IsQueuedForRemoval();
    }

    void C_Probe::DrawBackground(EiseiCanvas &canvas)
    {
    }
}