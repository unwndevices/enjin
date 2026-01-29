#ifndef TRANSMISSIONBEAM_HPP
#define TRANSMISSIONBEAM_HPP

#include <memory>

#include "Satellite.hpp"
#include "Scope.hpp"
#include "../Components/C_TransmissionBeam.hpp"

namespace enjin
{
    class TransmissionBeam : public Scope
    {
    public:
        TransmissionBeam()
        {
            position = AddComponent<C_Position>();
            beam = AddComponent<C_TransmissionBeam>(127, 127);
            beam->SetDrawLayer(DrawLayer::Background);
        }

        void Update()
        {
        }

        void SetPrimary(float amount) override
        {
            beam->SetWidth(amount);
        }
        void SetSecondary(float amount) override
        {
            beam->SetPhase(amount);
        }

        void SetMode(uint8_t mode) override
        {
            beam->SetMode(mode);
        }

    private:
        std::shared_ptr<C_TransmissionBeam> beam;
    };
}
#endif// TRANSMISSIONBEAM_HPP
