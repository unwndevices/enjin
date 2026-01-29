#include "Polar.hpp"
#include <math.h>
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

namespace enjin
{
    Vector2 RadialToCartesian(float phase, uint8_t radius, Vector2 center)
    {

        Vector2 pos;
        // Rotate by 90 degrees CCW by adding 0.25 to the phase
        float rotatedPhase = phase + 0.25f;
        // Ensure the phase stays within [0, 1] range
        if (rotatedPhase >= 1.0f)
            rotatedPhase -= 1.0f;

        float radians = rotatedPhase * PI * 2.0f;
        pos.x = (int16_t)round(radius * -cos(radians) + center.x);
        pos.y = (int16_t)round(radius * -sin(radians) + center.y);
        return pos;
    }
} // namespace enjin
