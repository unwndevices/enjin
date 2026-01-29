#ifndef C_MORPHING_SHAPE_HPP
#define C_MORPHING_SHAPE_HPP

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath> // For M_PI, cos, sin, lerp
#include <vector>
#include <algorithm> // For std::lerp

#include "../Object.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include "../utils/Types.hpp" // For Vector2
#include <Adafruit_GFX.h>     // For EiseiCanvas
#include "enjin2_compat.hpp"

namespace enjin
{
    class C_MorphingShape : public C_Drawable
    {
    public:
        C_MorphingShape(Object *owner, uint16_t max_radius, uint16_t inner_radius_4pt, int num_vertices = 64, uint8_t color = 15);

        void Awake() override {};
        void Draw(EiseiCanvas &canvas) override;
        bool ContinueToDraw() const override;

        // Sets the morph position (0.0 = circle, 1.0 = 4-point star)
        void SetMorph(float position);
        void SetShape(float shape);

        // Setters for parameters if needed later
        void SetColor(uint8_t color) { this->color = color; }
        void SetRadii(uint16_t max_r, uint16_t inner_r4);

    private:
        // Calculates the radius for a given angle and morph position
        float getRadius(float theta) const;
        // Helper for linear interpolation if std::lerp isn't available/suitable
        // static float Lerp(float a, float b, float t) { return a + t * (b - a); }

        std::shared_ptr<C_Position> position; // Position component of the owner object
        uint16_t max_radius;
        uint16_t inner_radius_4pt;
        int num_vertices; // Number of vertices to sample the shape
        uint8_t color;
        float morph_position; // Current morph state (0.0 to 1.0)
        float morph_shape;
    };

} // namespace enjin

#endif // C_MORPHING_SHAPE_HPP