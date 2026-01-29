#include <iostream>
#include <cmath>
#include <algorithm>

#include "C_Satellite.hpp"
#include "../Object.hpp"
#include "utils/DrawingHelpers.hpp"

namespace enjin
{
    uint8_t C_Satellite::amount = 0;
    Vector2 C_Satellite::abs_center(63, 63);
    int8_t C_Satellite::selected = -1;

    float C_Satellite::start_ = 0.f;
    float C_Satellite::end_ = 1.f;
    EditTarget C_Satellite::target_ = EditTarget::NONE;
    C_Satellite *C_Satellite::instances[C_Satellite::MAX_SATELLITES] = {nullptr};
    uint8_t C_Satellite::live_instances = 0;
    bool C_Satellite::chevron_radius_dirty = true;
    float C_Satellite::chevron_radius = 0.0f;

    C_Satellite::C_Satellite(Object *owner, uint8_t from_center, uint8_t animation_distance, uint8_t radius, uint8_t color) : C_Drawable(127, 127), Component(owner),
                                                                                                                              phase(0.0f), prev_phase(0.0f), from_center(from_center), animation_distance(animation_distance),
                                                                                                                              radius(radius),
                                                                                                                              color(color)
    {
        this->position = owner->GetComponent<C_Position>();
        if (!this->position)
        {
            std::cerr << "C_Satellite requires C_Position component.\n";
        }
        identity = C_Satellite::amount;
        C_Satellite::amount++;
        RegisterInstance(this);

        sat_position = RadialToCartesian(phase, from_center, abs_center);
        for (uint8_t i = 0; i < MAX_AFTERIMAGES; i++)
        {
            afterimage_position[i] = sat_position;
            afterimage_phase[i] = phase;
        }
    };

    void C_Satellite::Awake()
    {
    }

    void C_Satellite::DrawSatellite(EiseiCanvas &canvas)
    {
        if (selected != identity)
        {
            // Draw afterimages first (if any)
            if (afterimage_amount > 0)
            {
                for (uint8_t i = 0; i < afterimage_amount; i++)
                {
                    if (!IsPhaseWithinActiveArc(afterimage_phase[i]))
                    {
                        continue;
                    }
                    // Gradually decrease intensity along the trail
                    float intensity_factor = (float)(afterimage_amount - i) / (afterimage_amount + 1);
                    uint8_t fade_color = (uint8_t)std::max<uint8_t>(2, (uint8_t)round(color / 3 * intensity_factor));

                    canvas.fillCircle(afterimage_position[i].x, afterimage_position[i].y, radius, 0);
                    canvas.fillCircle(afterimage_position[i].x, afterimage_position[i].y, radius, fade_color);
                }
            }

            canvas.fillCircle(sat_position.x, sat_position.y, radius + 1, 0);
            canvas.fillCircle(sat_position.x, sat_position.y, radius, color);
        }
        else
        {
            canvas.fillCircle(sat_position.x, sat_position.y, radius + 3, 0);
            canvas.fillCircle(sat_position.x, sat_position.y, radius + 1, color);
        }
    }

    void C_Satellite::Update(uint16_t deltaTime)
    {
        if (target_ != EditTarget::NONE || selected != -1)
        {
            elapsedTime += deltaTime;

            if (elapsedTime > 200)
            {
                elapsedTime = 0;
                if (color_active == 10)
                {
                    color_active = 3;
                }
                else
                {
                    color_active = 10;
                }
            }
        }
        else
            color_active = 10;

        sat_position = RadialToCartesian(phase, from_center, abs_center);

        // Calculate afterimage positions based on actual phase change with time-based smoothing
        if (afterimage_amount > 0)
        {
            // Signed phase delta in range (-0.5, 0.5]
            float delta_signed = phase - prev_phase;
            if (delta_signed > 0.5f)
                delta_signed -= 1.0f;
            else if (delta_signed < -0.5f)
                delta_signed += 1.0f;

            float delta_abs = fabsf(delta_signed);

            // Convert delta to phase speed (cycles per second) using deltaTime (ms)
            float dtSec = (deltaTime > 0) ? (deltaTime / 1000.0f) : 0.001f; // prevent divide-by-zero
            float inst_speed = delta_abs / dtSec;                           // cycles per second

            // Exponential smoothing of speed
            const float alpha = 0.3f; // smoothing factor (0..1)
            smoothed_speed = smoothed_speed * (1.0f - alpha) + inst_speed * alpha;

            // Non-linear mapping speed → span_angle (more gradual growth)
            const float MAX_SPAN = 0.45f; // radian fraction of full circle (0..1)
            const float k = 0.25f;        // curve steepness constant (higher = more sensitive)
            float span_angle = MAX_SPAN * (1.0f - expf(-smoothed_speed * k));

  
            float direction = (delta_signed >= 0.0f) ? 1.0f : -1.0f;

            for (uint8_t i = 0; i < afterimage_amount; i++)
            {
                float fraction = static_cast<float>(i + 1) / afterimage_amount;
                float afterimage_phase_value = phase - direction * span_angle * fraction;

                // Keep phase within [0,1)
                if (afterimage_phase_value < 0.0f)
                    afterimage_phase_value += 1.0f;
                else if (afterimage_phase_value >= 1.0f)
                    afterimage_phase_value -= 1.0f;

                afterimage_position[i] = RadialToCartesian(afterimage_phase_value, from_center, abs_center);
                this->afterimage_phase[i] = afterimage_phase_value;
            }

            // Store current phase for next frame comparison
            prev_phase = phase;
        }
    };
    void C_Satellite::Draw(EiseiCanvas &canvas)
    {
        DrawBackground(canvas);
        DrawLimits(canvas);
        DrawSatellite(canvas);
    };

    bool C_Satellite::ContinueToDraw() const
    {
        return !owner->IsQueuedForRemoval();
    }

    void C_Satellite::DrawOrbit(EiseiCanvas &canvas)
    {
        if (selected != identity)
        {
            canvas.drawCircle(abs_center.x, abs_center.y, from_center - 1, 0);
            canvas.drawCircle(abs_center.x, abs_center.y, from_center, 2);
            canvas.drawCircle(abs_center.x, abs_center.y, from_center + 1, 0);
        }
        else
        {
            drawCircleStroke(&canvas, abs_center.x, abs_center.y, from_center, color_active, 3);
        }
    }

    void C_Satellite::DrawLimits(EiseiCanvas &canvas)
    {
        Vector2 start_a = RadialToCartesian(start_, from_center - 4, abs_center);
        Vector2 start_b = RadialToCartesian(start_, from_center + 4, abs_center);
        Vector2 end_a = RadialToCartesian(end_, from_center - 4, abs_center);
        Vector2 end_b = RadialToCartesian(end_, from_center + 4, abs_center);

        uint8_t color_start = 6, color_end = 6;
        if (target_ == EditTarget::START)
        {
            color_start = color_active;
            color_end = 6;
        }
        else if (target_ == EditTarget::END)
        {
            color_start = 6;
            color_end = color_active;
        }

        auto draw_selection_marker = [&](float phase_value, bool draw_greater_symbol, uint8_t color)
        {
            float radius_value = GetChevronRadius();
            auto clamp_radius = [](float value) -> uint8_t
            {
                long rounded = std::lround(value);
                if (rounded < 0)
                {
                    return 0;
                }
                if (rounded > 255)
                {
                    return 255;
                }
                return static_cast<uint8_t>(rounded);
            };

            uint8_t radial_distance = clamp_radius(radius_value);
            Vector2 radial_point = RadialToCartesian(phase_value, radial_distance, abs_center);
            float rad_x = static_cast<float>(radial_point.x - abs_center.x);
            float rad_y = static_cast<float>(radial_point.y - abs_center.y);
            float length = std::sqrt(rad_x * rad_x + rad_y * rad_y);
            if (length < 1.0f)
            {
                return;
            }

            rad_x /= length;
            rad_y /= length;
            float tan_x = -rad_y;
            float tan_y = rad_x;
            float direction = draw_greater_symbol ? 1.0f : -1.0f;

            const float chevron_gap = -1.0f;
            const float chevron_length = 6.0f;
            const float chevron_half_width = 3.2f;

            float tip_x = static_cast<float>(radial_point.x) + tan_x * direction * chevron_gap;
            float tip_y = static_cast<float>(radial_point.y) + tan_y * direction * chevron_gap;

            float base_center_x = tip_x - tan_x * direction * chevron_length;
            float base_center_y = tip_y - tan_y * direction * chevron_length;

            float base1_x = base_center_x + rad_x * chevron_half_width;
            float base1_y = base_center_y + rad_y * chevron_half_width;
            float base2_x = base_center_x - rad_x * chevron_half_width;
            float base2_y = base_center_y - rad_y * chevron_half_width;

            auto to_int = [](float value) -> int16_t
            {
                return static_cast<int16_t>(std::lround(value));
            };

            canvas.drawLine(to_int(base1_x), to_int(base1_y), to_int(tip_x), to_int(tip_y), color);
            canvas.drawLine(to_int(base2_x), to_int(base2_y), to_int(tip_x), to_int(tip_y), color);
        };

        canvas.drawLine(start_a.x, start_a.y, start_b.x, start_b.y, color_start);
        canvas.drawLine(end_a.x, end_a.y, end_b.x, end_b.y, color_end);

        bool should_draw_chevron = (identity == 0);

        if (should_draw_chevron && target_ == EditTarget::START)
        {
            draw_selection_marker(start_, false, color_active);
        }
        else if (should_draw_chevron && target_ == EditTarget::END)
        {
            draw_selection_marker(end_, true, color_active);
        }
    }

    void C_Satellite::DrawBackground(EiseiCanvas &canvas)
    {
        DrawOrbit(canvas);
    }

    void C_Satellite::RegisterInstance(C_Satellite *instance)
    {
        if (instance->identity >= MAX_SATELLITES)
        {
            return;
        }

        instances[instance->identity] = instance;
        if (instance->identity + 1 > live_instances)
        {
            live_instances = instance->identity + 1;
        }

        MarkChevronRadiusDirty();
    }

    void C_Satellite::MarkChevronRadiusDirty()
    {
        chevron_radius_dirty = true;
    }

    void C_Satellite::RecomputeChevronRadius()
    {
        uint8_t min_radius = 255;
        uint8_t max_radius = 0;
        bool found_any = false;

        for (uint8_t i = 0; i < live_instances; ++i)
        {
            C_Satellite *satellite = instances[i];
            if (!satellite)
            {
                continue;
            }

            min_radius = std::min<uint8_t>(min_radius, satellite->from_center);
            max_radius = std::max<uint8_t>(max_radius, satellite->from_center);
            found_any = true;
        }

        if (found_any)
        {
            chevron_radius = 0.5f * (static_cast<float>(min_radius) + static_cast<float>(max_radius));
        }
        else
        {
            chevron_radius = 0.0f;
        }

        chevron_radius_dirty = false;
    }

    float C_Satellite::GetChevronRadius()
    {
        if (chevron_radius_dirty)
        {
            RecomputeChevronRadius();
        }

        return chevron_radius;
    }

    bool C_Satellite::IsPhaseWithinActiveArc(float phase_value)
    {
        auto normalize = [](float value) -> float
        {
            value = std::fmod(value, 1.0f);
            if (value < 0.0f)
            {
                value += 1.0f;
            }
            return value;
        };

        float phase_norm = normalize(phase_value);

        // Special case: full circle (start=0, end=1)
        const float epsilon = 0.001f;
        if (std::fabs(start_) < epsilon && std::fabs(end_ - 1.0f) < epsilon)
        {
            return true;
        }

        float start_norm = normalize(start_);
        float end_norm = normalize(end_);

        float span = 0.0f;
        if (start_norm <= end_norm)
        {
            span = end_norm - start_norm;
        }
        else
        {
            span = (1.0f - start_norm) + end_norm;
        }

        if (span >= 1.0f - epsilon)
        {
            return true;
        }

        if (start_norm <= end_norm)
        {
            return phase_norm >= start_norm && phase_norm <= end_norm;
        }

        return phase_norm >= start_norm || phase_norm <= end_norm;
    }
}
