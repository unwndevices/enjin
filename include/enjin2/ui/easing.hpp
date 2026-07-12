#pragma once

#include <cmath>

/**
 * @file easing.hpp
 * @brief Normalized easing curves for ui animation
 *
 * Upstreamed from the Eisei widget layer (`Libs/enjin/utils/Easing.hpp`, #121).
 * Every function maps a normalized time @c t in [0, 1] to an eased position; the
 * animators store these by @ref EasingFunction pointer and drive keyframe
 * interpolation with them.
 *
 * Unless noted, each curve pins its endpoints (f(0)=0, f(1)=1) so an animation
 * starts and ends exactly on its keyframe values.
 */

namespace enjin2 {

/// @brief Pointer to a normalized easing curve `float(float t)`.
using EasingFunction = float (*)(float t);

/**
 * @brief Collection of normalized easing curves as static functions
 *
 * A stateless bag of curves — nothing to construct. Reference a member as a
 * function pointer (`&Easing::EaseInOutCubic`) to hand a curve to an animator.
 */
class Easing {
public:
    static constexpr float kPi = 3.14159265358979323846f;   ///< π
    static constexpr float kHalfPi = 1.57079632679489661923f; ///< π/2

    /// @brief Holds at zero for the whole interval (no interpolation).
    static float Step(float /*t*/) { return 0.0f; }

    /// @brief Identity curve — constant velocity.
    static float Linear(float t) { return t; }

    /// @brief Starts slow and accelerates (quadratic).
    static float EaseInQuad(float t) { return t * t; }

    /// @brief Starts slow and accelerates (sinusoidal).
    static float EaseInSine(float t) { return 1.0f - std::cos((t * kPi) / 2.0f); }

    /// @brief Starts fast and decelerates (quadratic).
    static float EaseOutQuad(float t) { return t * (2.0f - t); }

    /// @brief Accelerates then decelerates, symmetric about the midpoint (quadratic).
    static float EaseInOutQuad(float t) {
        return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
    }

    /// @brief Accelerates then decelerates, symmetric about the midpoint (cubic).
    static float EaseInOutCubic(float t) {
        return (t < 0.5f) ? (4.0f * t * t * t)
                          : ((t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f);
    }

    /// @brief Accelerates then decelerates, symmetric about the midpoint (quintic).
    static float EaseInOutQuint(float t) {
        return (t < 0.5f) ? (16.0f * t * t * t * t * t)
                          : (1.0f + 16.0f * (t - 1.0f) * t * t * t * t);
    }

    /// @brief Starts slow and accelerates (cubic).
    static float EaseInCubic(float t) { return t * t * t; }

    /// @brief Starts fast and decelerates (cubic).
    static float EaseOutCubic(float t) {
        float t1 = t - 1.0f;
        return t1 * t1 * t1 + 1.0f;
    }

    /// @brief Accelerates then decelerates along a circular arc.
    static float EaseInOutCirc(float t) {
        return (t < 0.5f)
                   ? (1.0f - std::sqrt(1.0f - 4.0f * t * t)) * 0.5f
                   : (std::sqrt(-(2.0f * t - 3.0f) * (2.0f * t - 1.0f)) + 1.0f) * 0.5f;
    }

    /// @brief Starts slow and accelerates (quartic).
    static float EaseInQuart(float t) { return t * t * t * t; }

    /// @brief Starts fast and decelerates (quartic).
    static float EaseOutQuart(float t) {
        float t1 = t - 1.0f;
        return 1.0f - t1 * t1 * t1 * t1;
    }

    /// @brief Accelerates then decelerates, symmetric about the midpoint (sinusoidal).
    static float EaseInOutSine(float t) { return -0.5f * (std::cos(kPi * t) - 1.0f); }

    /// @brief Springy overshoot on both ends (does not pin endpoints exactly).
    static float EaseInOutElastic(float t) {
        if (t <= 0.5f) {
            return 0.5f * std::sin(13.0f * kHalfPi * t) * std::pow(2.0f, 10.0f * (t - 1.0f));
        }
        return 0.5f * (std::sin(-13.0f * kHalfPi * (t + 1.0f)) *
                           std::pow(2.0f, -10.0f * (t + 1.0f)) +
                       2.0f);
    }
};

} // namespace enjin2
