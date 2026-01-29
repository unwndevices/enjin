#ifndef PROBE_FIELDS_HPP
#define PROBE_FIELDS_HPP

#include <memory>

#include "Scope.hpp"

#include "../Components/C_Probe.hpp"
#include "../Components/C_ProbeRange.hpp"

namespace enjin
{
    class ProbeFields : public Scope
    {
    public:
        ProbeFields(uint8_t from_center, uint8_t radius = 10, uint8_t amount = 4, uint8_t color = 7) : from_center(from_center), radius(radius), color(color), amount(amount)
        {
            position = AddComponent<C_Position>();
            SetProbeAmount(amount);
            SetMode(0);
        };

        void SetPrimary(float amount) override
        {
            uint8_t radius = (uint8_t)(amount * 94.0f);
            for (auto &range : ranges)
            {
                range->SetRadius(radius);
            }
        };

        void SetSecondary(float amount) override
        {
            SetPosition(amount);
        };

        void SetMode(uint8_t mode) override
        {
            if (mode)
            {
                for (uint8_t i = 0; i < amount; i++)
                {
                    probes[i]->SetBlendMode(BlendMode::Normal);
                    ranges[i]->SetBlendMode(BlendMode::Normal);
                }
            }
            else
            {
                for (uint8_t i = 0; i < amount; i++)
                {
                    probes[i]->SetBlendMode(BlendMode::Opacity50);
                    ranges[i]->SetBlendMode(BlendMode::Opacity50);
                }
            }
        }
        void Update() override {};

        void SetPosition(float pos)
        {
            for (uint8_t i = 0; i < amount; i++)
            {
                probes[i]->SetPhase(((float)i / (float)amount) * pos + 0.125f);
                ranges[i]->SetPhase(((float)i / (float)amount) * pos + 0.125f);
            }
        };

        void SetProbeAmount(uint8_t amount)
        {
            this->amount = amount;
            probes.clear();
            for (uint8_t i = 0; i < amount; i++)
            {
                probes.push_back(AddComponent<C_Probe>(from_center, radius, color));
                probes[i]->SetDrawLayer(DrawLayer::Entities);
                probes[i]->SetPhase((float)i / amount);
                probes[i]->SetLabel(std::to_string(labels_text[i]));

                ranges.push_back(AddComponent<C_ProbeRange>(from_center, radius, color));
                ranges[i]->SetDrawLayer(DrawLayer::Background);
                ranges[i]->SetPhase((float)i / amount);
            }
        };

    private:
        std::vector<std::shared_ptr<C_Probe>> probes;
        std::vector<std::shared_ptr<C_ProbeRange>> ranges;

        uint8_t from_center, radius, color, amount;

        char labels_text[4] = {'a', 'b', 'c', 'd'};
    };
}
#endif // PROBE_HPP
