#include "C_MorphingShape.hpp"
#include <vector> // Include vector for storing points

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace enjin
{

    C_MorphingShape::C_MorphingShape(Object *owner, uint16_t max_radius, uint16_t inner_radius_4pt, int num_vertices, uint8_t color)
        : Component(owner),
          // Component canvas must be large enough for the max radius
          C_Drawable(max_radius * 2 + 1, max_radius * 2 + 1),
          max_radius(max_radius),
          inner_radius_4pt(inner_radius_4pt),
          num_vertices(num_vertices),
          color(color),
          morph_position(0.0f), // Start as circle
          morph_shape(8.0f)
    {
        // Get the position component from the owner
        position = owner->GetComponent<C_Position>();
        if (!position)
        {
            // Handle error: Owner object must have a C_Position component
            // Log error or throw exception
        }
    }

    void C_MorphingShape::SetMorph(float position)
    {
        // Clamp position to the valid range [-1.0, 1.0]
        this->morph_position = std::max(-1.0f, std::min(1.0f, position));
    }

    void C_MorphingShape::SetShape(float shape)
    {
        this->morph_shape = 4.0f + 4.0f * std::max(0.0f, std::min(1.0f, shape));
    }

    void C_MorphingShape::SetRadii(uint16_t max_r, uint16_t inner_r4)
    {
        this->max_radius = max_r;
        this->inner_radius_4pt = inner_r4;
        // Recalculate center or resize canvas if necessary, though resizing C_Drawable dynamically is complex.
        // For now, assume radii changes don't exceed initial max_radius used for canvas setup.
        // center.x = this->width / 2;
        // center.y = this->height / 2;
    }

    bool C_MorphingShape::ContinueToDraw() const
    {
        return !owner->IsQueuedForRemoval();
    }

    // --- Core Morphing Logic ---
    float C_MorphingShape::getRadius(float theta) const
    {
        float R = static_cast<float>(max_radius);
        float r4 = static_cast<float>(inner_radius_4pt);

        // Define the base shapes' radius functions
        float radius_circle = R;

        // Parameters for the formula from https://nyjp07.com/index_asteroid_E.html (Example 3 adjusted)
        const float a = 0.83f;
        const float c = 1.0f;              // Kept for formula structure, but is 1
        const float n_param = morph_shape; // n parameter in the formula (number of points)
        const float a_sq = a * a;          // Precompute a^2

        // Theoretical min/max of the raw formula r_formula(theta)
        const float r_formula_min = a;                                       // When sin^2{} = 1
        const float r_formula_max = sqrtf(-logf(2.0f * expf(-a_sq) - 1.0f)); // ~2.338, when sin^2{} = 0

        // Calculate the formula-based radius
        float r_formula;
        float angle_term = (theta - static_cast<float>(M_PI) / 2.0f) * (n_param / 2.0f);
        float sin_angle_term = sinf(angle_term);
        float sin_sq_term = sin_angle_term * sin_angle_term;

        float exponent_term = -a_sq * sin_sq_term;
        float log_arg = 2.0f * expf(-a_sq) - expf(exponent_term);

        if (log_arg <= 0)
        {
            // Logarithm argument must be positive
            // This might happen due to floating point inaccuracies near the minimum
            // Fallback to the minimum theoretical value
            r_formula = r_formula_min;
        }
        else
        {
            float sqrt_arg = -logf(log_arg);
            if (sqrt_arg < 0)
            {
                // Square root argument must be non-negative
                // Fallback to the minimum theoretical value
                r_formula = r_formula_min;
            }
            else
            {
                r_formula = (1.0f / c) * sqrtf(sqrt_arg);
            }
        }

        // Scale the formula result r_formula (range [r_formula_min, r_formula_max])
        // to the desired output range [r4, R]
        float scaled_radius_star4;
        if (fabsf(r_formula_max - r_formula_min) < 1e-6)
        { // Avoid division by zero if min/max are the same
            scaled_radius_star4 = r4;
        }
        else
        {
            float scale_factor = (r_formula - r_formula_min) / (r_formula_max - r_formula_min);
            // Clamp scale_factor to [0, 1] to prevent over/undershooting due to potential float inaccuracies
            scale_factor = std::max(0.0f, std::min(1.0f, scale_factor));
            scaled_radius_star4 = r4 + (R - r4) * scale_factor;
        }

        // Linearly interpolate between circle and the formula-based star
        // C++11 compatible lerp implementation
        return radius_circle + morph_position * (scaled_radius_star4 - radius_circle);
    }

    void C_MorphingShape::Draw(EiseiCanvas &canvas)
    {
        // Calculate center based on max_radius, as the canvas is max_radius*2 wide/high
        int16_t center_x = 63;
        int16_t center_y = 63;

        if (morph_position > 0)
        {
            // --- Morphing Star Outline ---
            if (num_vertices < 3)
                return; // Need at least 3 vertices to draw a shape

            std::vector<Vector2> points(num_vertices);
            float angle_step = 2.0f * M_PI / num_vertices;

            // Calculate vertices
            for (int i = 0; i < num_vertices; ++i)
            {
                float current_angle = i * angle_step;
                float r = getRadius(current_angle); // getRadius handles morph_position > 0

                // Convert polar (r, current_angle) to Cartesian (x, y) relative to center
                // Add 0.5f before casting for rounding
                points[i].x = static_cast<int16_t>(center_x + r * cosf(current_angle) + 0.5f);
                points[i].y = static_cast<int16_t>(center_y + r * sinf(current_angle) + 0.5f);
            }

            // Draw lines connecting the vertices
            for (int i = 0; i < num_vertices; ++i)
            {
                const Vector2 &p1 = points[i];
                const Vector2 &p2 = points[(i + 1) % num_vertices]; // Wrap around for the last line
                canvas.drawLine(p1.x, p1.y, p2.x, p2.y, color);
            }
        }
        else
        {
            // --- Shrinking/Filled Circle ---
            const float fill_threshold = 1e-6; // Tolerance for checking if morph_position is -1.0

            if (fabsf(morph_position + 1.0f) < fill_threshold)
            {
                // Case: morph_position is effectively -1.0, draw filled circle
                canvas.fillCircle(center_x, center_y, max_radius, color);
            }
            else
            {
                // Case: morph_position is between -1.0 (exclusive) and 0.0 (inclusive)
                // Calculate the radius of the inner "hole" circle
                // Radius goes from max_radius (at morph 0) down to 0 (at morph -1)
                uint16_t inner_radius = static_cast<uint16_t>(max_radius * (1.0f + morph_position));

                // Draw the outer filled circle (the visible ring)
                canvas.fillCircle(center_x, center_y, max_radius, color);

                // Draw the inner black filled circle to create the hollow effect
                if (inner_radius > 0) // Avoid drawing a circle with radius 0
                {
                    canvas.fillCircle(center_x, center_y, inner_radius, 0); // Color 0 is typically black
                }
            }
        }
    }

} // namespace enjin