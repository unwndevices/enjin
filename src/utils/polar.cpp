#include "../../include/enjin2/utils/polar.hpp"
#include <cmath>

namespace enjin2 {
namespace Polar {

Point RadialToCartesian(float phase, uint8_t radius, Point center) {
    // Rotate by 90 degrees CCW by adding 0.25 to the phase (matches original Enjin exactly)
    float rotatedPhase = phase + 0.25f;
    
    // Ensure the phase stays within [0, 1] range
    if (rotatedPhase >= 1.0f) {
        rotatedPhase -= 1.0f;
    }

    float radians = rotatedPhase * PI * 2.0f;
    
    // Apply the same transformations as original Enjin
    Point pos;
    pos.x = static_cast<int16_t>(round(radius * -cos(radians) + center.x));
    pos.y = static_cast<int16_t>(round(radius * -sin(radians) + center.y));
    
    return pos;
}

void CartesianToRadial(Point point, Point center, float& phase, uint8_t& radius) {
    float dx = point.x - center.x;
    float dy = point.y - center.y;
    
    radius = static_cast<uint8_t>(round(sqrt(dx * dx + dy * dy)));
    
    float angle = atan2(-dy, -dx); // Negative to match the RadialToCartesian transform
    phase = angle / (2.0f * PI);
    
    // Adjust for the 90-degree rotation
    phase -= 0.25f;
    
    // Normalize to [0, 1] range
    phase = NormalizePhase(phase);
}

Point GetCirclePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radius) {
    Point pos;
    pos.x = centerX + static_cast<int16_t>(round(radius * cos(angle)));
    pos.y = centerY + static_cast<int16_t>(round(radius * sin(angle)));
    return pos;
}

Point GetEllipsePoint(int16_t centerX, int16_t centerY, float angle, uint8_t radiusX, uint8_t radiusY) {
    Point pos;
    pos.x = centerX + static_cast<int16_t>(round(radiusX * cos(angle)));
    pos.y = centerY + static_cast<int16_t>(round(radiusY * sin(angle)));
    return pos;
}

float CalculateDistance(Point p1, Point p2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return sqrt(dx * dx + dy * dy);
}

float CalculateAngle(Point p1, Point p2) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    return atan2(dy, dx);
}

float NormalizePhase(float phase) {
    while (phase < 0.0f) {
        phase += 1.0f;
    }
    while (phase >= 1.0f) {
        phase -= 1.0f;
    }
    return phase;
}

float PhaseToRadians(float phase) {
    return NormalizePhase(phase) * 2.0f * PI;
}

float RadiansToPhase(float radians) {
    float phase = radians / (2.0f * PI);
    return NormalizePhase(phase);
}

} // namespace Polar
} // namespace enjin2