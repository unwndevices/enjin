#include "../../include/enjin2/animation/keyframe.hpp"
#include <cmath>

namespace enjin2 {

float EasingFunctions::ease(float t, EaseType easeType) {
    t = clamp01(t);
    
    switch (easeType) {
        case EaseType::LINEAR:
            return t;
            
        case EaseType::EASE_IN:
            return t * t;
            
        case EaseType::EASE_OUT:
            return 1.0f - (1.0f - t) * (1.0f - t);
            
        case EaseType::EASE_IN_OUT:
            if (t < 0.5f) {
                return 2.0f * t * t;
            } else {
                return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
            }
            
        case EaseType::EASE_IN_QUAD:
            return t * t;
            
        case EaseType::EASE_OUT_QUAD:
            return 1.0f - (1.0f - t) * (1.0f - t);
            
        case EaseType::EASE_IN_CUBIC:
            return t * t * t;
            
        case EaseType::EASE_OUT_CUBIC:
            return 1.0f - std::pow(1.0f - t, 3.0f);
            
        case EaseType::EASE_BOUNCE: {
            if (t < 1.0f / 2.75f) {
                return 7.5625f * t * t;
            } else if (t < 2.0f / 2.75f) {
                t -= 1.5f / 2.75f;
                return 7.5625f * t * t + 0.75f;
            } else if (t < 2.5f / 2.75f) {
                t -= 2.25f / 2.75f;
                return 7.5625f * t * t + 0.9375f;
            } else {
                t -= 2.625f / 2.75f;
                return 7.5625f * t * t + 0.984375f;
            }
        }
            
        case EaseType::EASE_ELASTIC: {
            if (t == 0.0f || t == 1.0f) return t;
            
            float p = 0.3f;
            float s = p / 4.0f;
            return std::pow(2.0f, -10.0f * t) * std::sin((t - s) * (2.0f * 3.14159f) / p) + 1.0f;
        }
            
        default:
            return t;
    }
}

} // namespace enjin2