#ifndef C_PLANET_HPP
#define C_PLANET_HPP

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif // !_USE_MATH_DEFINES
#include <math.h>

#include <memory>
#include <vector>
#include <cmath> // Added for sqrt, floor
#include "../Object.hpp"
#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "../utils/Polar.hpp"
#include "../utils/Types.hpp"

namespace enjin
{
    class C_Planet : public C_Drawable
    {
    public:
        // Note: Constructor now takes ring_radius_offset without a default value
        C_Planet(Object *owner, uint8_t radius, uint8_t standby_radius, uint8_t ring_radius_offset);
        void Awake() override {};

        void Update(uint16_t deltaTime) override;
        void Draw(EiseiCanvas &canvas) override;

        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

        void SetPhase(float amount)
        {
            phase = amount;
        };

        uint8_t GetRadius() const
        {
            return radius;
        }

        void SetRadius(uint8_t radius)
        {
            this->radius = radius;
        }

        void SetSpeed(float speed)
        {
            this->speed = speed / 1000.0f;
        }

        void SetRingSpeed(float speed)
        {
            this->ringSpeed = speed / 1000.0f;
        }

        // Add a setter to adjust noise scaling at runtime
        void SetNoiseScale(float sx, float sy)
        {
            noiseScaleX = sx;
            noiseScaleY = sy;
        }

        void GenerateTerrain();

        void SetColorFactor(float factor)
        {
            // Clamp factor to the range [-1.0, 1.0]
            color_factor = std::max(-1.0f, std::min(1.0f, factor));
        }

    private:
        void GenerateSphericalMap(std::vector<Vector2> &map, uint8_t map_radius);
        void GenerateRingTexture();
        void GenerateDitheredShadingMask(); // Added declaration

        float phase, speed;
        uint8_t radius, standby_radius, zoom_radius;
        uint8_t initial_radius, initial_diameter; // Added for scaling

        EiseiCanvas textureCanvas;
        std::vector<Vector2> sphericalMap;
        std::vector<Vector2> skyMap;

        // Ring members
        float ringPhase, ringSpeed;
        uint8_t initial_ringRadius, initial_ring_diameter; // Added for scaling
        EiseiCanvas ringTextureCanvas;
        std::vector<Vector2> ringSphericalMap;

        // Shading members
        EiseiCanvas ditheredShadingMask; // Canvas for shading mask
        Vector3 lightDir;               // Light direction vector

        // Noise scaling factors for terrain generation
        float noiseScaleX = 4.0f; // Horizontal noise frequency (default)
        float noiseScaleY = 0.1f; // Vertical noise frequency (default)

        bool first_generation = true;
        uint16_t seed = 0;
        // Color factor for adjusting planet brightness/inversion.
        // Range: [-1.0, 1.0]
        // 0.0 = Black
        // (0.0, 1.0] = Scale towards original color (1.0 = original)
        // [-1.0, 0.0) = Scale towards inverted color (-1.0 = fully inverted: 15 - original)
        float color_factor = 1.0f;
    };
}
#endif // C_PLANET_HPP
