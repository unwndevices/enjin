#ifndef PLANET_HPP
#define PLANET_HPP

#include "../Object.hpp"
#include "../Components/C_Satellite.hpp"
#include "../Components/C_Planet.hpp"
#include "../Components/C_Label.hpp"
#include "../Components/C_Transition.hpp"

namespace enjin
{
    class Planet : public Object
    {
    public:
        Planet(uint8_t radius = 30, uint8_t standby_radius = 8, uint8_t ring_offset = 5)
        {
            this->radius = radius;
            this->standby_radius = standby_radius;
            position->SetPosition(Vector2(63, 63));

            planet = AddComponent<C_Planet>(radius, standby_radius, ring_offset);
            planet->SetDrawLayer(DrawLayer::Foreground);
            planet->SetBlendMode(BlendMode::Normal);
            planet->SetAnchorPoint(Anchor::CENTER_);

            SetFrequency(200.0f);
            SetRingFrequency(-140.0f);
            SetVisibility(true);
            InitTransitions();
        };

        void InitTransitions()
        {
            planet_transition = AddComponent<C_ParameterAnimator<uint8_t>>();
            planet_transition->SetParameterGetter([this]()
                                                  { return this->planet->GetRadius(); });
            planet_transition->SetParameterSetter([this](uint8_t radius)
                                                  { this->planet->SetRadius(radius); });

            planet_animation_in.AddKeyframe({0, standby_radius, Easing::Step});  // Start with current radius
            planet_animation_in.AddKeyframe({250, radius, Easing::EaseOutQuad}); // Transition to zoom radius
            planet_transition->SetAnimation(planet_animation_in);

            planet_animation_out.AddKeyframe({0, radius, Easing::Step});                  // Start with current radius
            planet_animation_out.AddKeyframe({250, standby_radius, Easing::EaseOutQuad}); // Transition to standby radius
        }

        void SetRadius(uint8_t radius)
        {
            planet->SetRadius(radius);
        };

        void GenerateTerrain()
        {
            planet->GenerateTerrain();
        };

        void SetFrequency(float frequency)
        {
            planet->SetSpeed(frequency);
        };

        void SetRingFrequency(float frequency)
        {
            planet->SetRingSpeed(frequency);
        };

        void SetVisibility(bool visibility)
        {
            planet->SetVisibility(visibility);
        };

        void SetActive()
        {
            planet->SetVisibility(true);
        };

        void SetColorFactor(float factor)
        {
            planet->SetColorFactor(factor);
        }

        void ZoomIn()
        {
            planet_transition->SetAnimation(planet_animation_in);
            planet_transition->StartAnimation();
        }

        void ZoomOut()
        {
            planet_transition->SetAnimation(planet_animation_out);
            planet_transition->StartAnimation();
        }

        ParameterAnimation<uint8_t> planet_animation_in, planet_animation_out;
        std::shared_ptr<C_ParameterAnimator<uint8_t>> planet_transition;

    private:
        std::shared_ptr<C_Planet> planet;
        uint8_t radius, standby_radius;
    };
}
#endif // PLANET_HPP
