#ifndef C_SATELLITE_HPP
#define C_SATELLITE_HPP

#include <memory>
#include "../Object.hpp"
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "../utils/Polar.hpp"
#include "../utils/Utils.hpp"

namespace enjin
{
    const uint8_t MAX_AFTERIMAGES = 30;
    const float AFTERIMAGE_SPAN_FACTOR = 0.05f; // Controls how much the afterimage trail spans based on speed

    enum class EditTarget
    {
        NONE = -1,
        START,
        END
    };

    class C_Satellite : public C_Drawable
    {
    public:
        C_Satellite(Object *owner, uint8_t from_center, uint8_t animation_distance, uint8_t radius, uint8_t color);
        void Awake() override;
        void Update(uint16_t deltaTime) override;
        void Draw(EiseiCanvas &canvas) override;
        bool ContinueToDraw() const override;

        void DrawOrbit(EiseiCanvas &canvas);

        void DrawLimits(EiseiCanvas &canvas);

        void DrawBackground(EiseiCanvas &canvas);
        void SetPhase(float amount)
        {
            phase = amount;
        };
        float GetPhase() { return phase; };
        static void SetAbsCenterX(float x)
        {
            abs_center.x = x;
        }

        void SetDistance(uint8_t distance)
        {
            from_center = distance;
            MarkChevronRadiusDirty();
        }
        void SetRadius(uint8_t radius)
        {
            this->radius = radius;
        }
        uint8_t GetRadius()
        {
            return radius;
        }

        uint8_t GetDistance()
        {
            return from_center;
        }

        uint8_t GetAnimationDistance()
        {
            return animation_distance;
        }

        int16_t GetAbsCenterX()
        {
            return abs_center.x;
        }

        static void SetTarget(EditTarget target, float value)
        {
            target_ = target;
            if (target == EditTarget::START)
                start_ = value;
            else if (target == EditTarget::END)
                end_ = value;
        }

        static void SetSelected(int8_t selected)
        {
            C_Satellite::selected = selected;
        }

        int8_t identity;

        void SetSpeed(float speed)
        {
            this->speed = speed;
        }

    private:
        float phase;
        // Stores the previous phase to measure phase change between frames
        float prev_phase = 0.0f;
        // Exponentially smoothed phase speed (cycles per second)
        float smoothed_speed = 0.0f;
        uint8_t from_center, animation_distance, radius;
        Vector2 sat_position;
        static uint8_t amount;
        uint8_t color;

        uint8_t color_active = 12;
        uint64_t elapsedTime = 0;

        static Vector2 abs_center;

        void DrawSatellite(EiseiCanvas &canvas);
        static EditTarget target_;
        static float start_, end_;

        static int8_t selected;

        // Styling variables
        float speed = 0.0f; // used to calculate the "afterimages" amount of each satellite
        Vector2 afterimage_position[MAX_AFTERIMAGES];
        float afterimage_phase[MAX_AFTERIMAGES];
        uint8_t afterimage_amount = MAX_AFTERIMAGES;

        static constexpr uint8_t MAX_SATELLITES = 16;
        static C_Satellite *instances[MAX_SATELLITES];
        static uint8_t live_instances;
        static bool chevron_radius_dirty;
        static float chevron_radius;

        static void RegisterInstance(C_Satellite *instance);
        static void RecomputeChevronRadius();
        static float GetChevronRadius();
        static void MarkChevronRadiusDirty();
        static bool IsPhaseWithinActiveArc(float phase_value);
    };
}
#endif // !C_SATELLITE_HPP
