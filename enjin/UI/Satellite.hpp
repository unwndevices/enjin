#ifndef SATELLITE_HPP
#define SATELLITE_HPP

#include <memory>

#include "../Object.hpp"
#include "../Components/C_Satellite.hpp"
#include "../Components/C_Transition.hpp"
#include "../Components/C_ParameterAnimator.hpp"

namespace enjin
{
    class Satellite : public Object
    {

    public:
        Satellite(uint8_t from_center, uint8_t animation_distance, uint8_t radius = 5, uint8_t color = 15)
        {
            position->SetPosition(Vector2(63, 63));
            satellite = AddComponent<C_Satellite>(from_center, animation_distance, radius, color);
            satellite->SetDrawLayer(DrawLayer::Entities);
            satellite->SetBlendMode(BlendMode::Normal);
            InitAnimation();
        };

        void SetVisibility(bool visibility)
        {
            satellite->SetVisibility(visibility);
        }

        void drawBackground(EiseiCanvas &canvas)
        {
            satellite->DrawBackground(canvas);
        };

        void SetRadius(uint8_t radius)
        {
            satellite->SetRadius(radius);
        }

        void SetPhase(float phase) { satellite->SetPhase(phase); };

        void SetDistance(uint8_t distance)
        {
            satellite->SetDistance(distance);
        }

        void SetSpeed(float speed)
        {
            satellite->SetSpeed(speed);
        }

        void SetAbsCenterX(float x)
        {
            C_Satellite::SetAbsCenterX(x);
        }
        void ZoomIn(bool reset = false)
        {
            distance_transition->SetAnimation(distance_animation_out);
            radius_transition->SetAnimation(radius_animation_out);
            distance_transition->StartAnimation(reset);
            radius_transition->StartAnimation(reset);
        }

        void ZoomOut()
        {
            distance_transition->SetAnimation(distance_animation_in);
            radius_transition->SetAnimation(radius_animation_in);
            distance_transition->StartAnimation(true);
            radius_transition->StartAnimation(true);
        }

        static void SetLimit(EditTarget target, float value)
        {
            C_Satellite::SetTarget(target, value);
        }

        ParameterAnimation<uint8_t> distance_animation_in, distance_animation_out;
        std::shared_ptr<C_ParameterAnimator<uint8_t>> distance_transition;
        ParameterAnimation<uint8_t> radius_animation_in, radius_animation_out;
        std::shared_ptr<C_ParameterAnimator<uint8_t>> radius_transition;

    private:
        void InitAnimation()
        {
            distance_transition = AddComponent<C_ParameterAnimator<uint8_t>>();
            distance_transition->SetParameterGetter([this]()
                                                    { return this->satellite->GetDistance(); });
            distance_transition->SetParameterSetter([this](uint8_t pos)
                                                    { this->satellite->SetDistance(pos); });

            uint8_t initial_position = satellite->GetAnimationDistance();
            uint8_t final_position = satellite->GetDistance();

            distance_animation_out.AddKeyframe({0, final_position, Easing::Step});
            distance_animation_out.AddKeyframe({200, initial_position, Easing::EaseOutQuad});
            distance_transition->SetAnimation(distance_animation_out);

            distance_animation_in.AddKeyframe({0, initial_position, Easing::Step});
            distance_animation_in.AddKeyframe({200, final_position, Easing::EaseOutQuad});

            radius_transition = AddComponent<C_ParameterAnimator<uint8_t>>();
            radius_transition->SetParameterGetter([this]()
                                                  { return this->satellite->GetRadius(); });
            radius_transition->SetParameterSetter([this](uint8_t pos)
                                                  { this->satellite->SetRadius(pos); });

            uint8_t initial_radius = 1;
            uint8_t final_radius = 2;

            radius_animation_out.AddKeyframe({0, final_radius, Easing::Step});
            radius_animation_out.AddKeyframe({200, initial_radius, Easing::Linear});
            radius_transition->SetAnimation(radius_animation_out);

            radius_animation_in.AddKeyframe({0, initial_radius, Easing::Step});
            radius_animation_in.AddKeyframe({200, final_radius, Easing::Linear});
        }

        std::shared_ptr<C_Satellite> satellite;
    };
}
#endif // !SATELLITE_HPP