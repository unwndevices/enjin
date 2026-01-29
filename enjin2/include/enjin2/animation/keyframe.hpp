#pragma once

#include "../core/types.hpp"
#include <cstdint>

namespace enjin2 {

/**
 * @brief Easing function types for smooth animations
 */
enum class EaseType {
    LINEAR,         ///< Linear interpolation
    EASE_IN,        ///< Slow start, fast end
    EASE_OUT,       ///< Fast start, slow end
    EASE_IN_OUT,    ///< Slow start and end, fast middle
    EASE_IN_QUAD,   ///< Quadratic ease in
    EASE_OUT_QUAD,  ///< Quadratic ease out
    EASE_IN_CUBIC,  ///< Cubic ease in
    EASE_OUT_CUBIC, ///< Cubic ease out
    EASE_BOUNCE,    ///< Bouncing effect
    EASE_ELASTIC    ///< Elastic spring effect
};

/**
 * @brief Keyframe for position animation
 */
struct PositionKeyframe {
    uint16_t time;      ///< Time in milliseconds
    Point position;     ///< Position at this keyframe
    EaseType easing;    ///< Easing to next keyframe
    
    /**
     * @brief Default constructor
     */
    PositionKeyframe() : time(0), position(0, 0), easing(EaseType::LINEAR) {}
    
    /**
     * @brief Constructor
     */
    PositionKeyframe(uint16_t t, Point pos, EaseType ease = EaseType::LINEAR)
        : time(t), position(pos), easing(ease) {}
};

/**
 * @brief Keyframe for float value animation
 */
struct FloatKeyframe {
    uint16_t time;      ///< Time in milliseconds
    float value;        ///< Float value at this keyframe
    EaseType easing;    ///< Easing to next keyframe
    
    /**
     * @brief Default constructor
     */
    FloatKeyframe() : time(0), value(0.0f), easing(EaseType::LINEAR) {}
    
    /**
     * @brief Constructor
     */
    FloatKeyframe(uint16_t t, float val, EaseType ease = EaseType::LINEAR)
        : time(t), value(val), easing(ease) {}
};

/**
 * @brief Keyframe for color animation
 */
struct ColorKeyframe {
    uint16_t time;      ///< Time in milliseconds
    Pixel4 color;       ///< Color at this keyframe
    EaseType easing;    ///< Easing to next keyframe
    
    /**
     * @brief Default constructor
     */
    ColorKeyframe() : time(0), color(0), easing(EaseType::LINEAR) {}
    
    /**
     * @brief Constructor
     */
    ColorKeyframe(uint16_t t, Pixel4 col, EaseType ease = EaseType::LINEAR)
        : time(t), color(col), easing(ease) {}
};

/**
 * @brief Animation state for tracking playback
 */
enum class AnimationState {
    STOPPED,    ///< Animation is stopped
    PLAYING,    ///< Animation is playing
    PAUSED,     ///< Animation is paused
    FINISHED    ///< Animation has finished
};

/**
 * @brief Animation loop modes
 */
enum class LoopMode {
    NONE,       ///< Play once and stop
    LOOP,       ///< Loop continuously
    PING_PONG   ///< Play forward then backward repeatedly
};

/**
 * @brief Easing function utilities
 */
class EasingFunctions {
public:
    /**
     * @brief Apply easing function to normalized time (0.0 to 1.0)
     * @param t Normalized time (0.0 to 1.0)
     * @param easeType Type of easing to apply
     * @return Eased value (0.0 to 1.0)
     */
    static float ease(float t, EaseType easeType);
    
    /**
     * @brief Linear interpolation between two values
     * @param a Start value
     * @param b End value
     * @param t Interpolation factor (0.0 to 1.0)
     * @return Interpolated value
     */
    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    
    /**
     * @brief Linear interpolation between two points
     * @param a Start point
     * @param b End point
     * @param t Interpolation factor (0.0 to 1.0)
     * @return Interpolated point
     */
    static Point lerp(const Point& a, const Point& b, float t) {
        return Point(
            static_cast<int16_t>(lerp(static_cast<float>(a.x), static_cast<float>(b.x), t)),
            static_cast<int16_t>(lerp(static_cast<float>(a.y), static_cast<float>(b.y), t))
        );
    }
    
    /**
     * @brief Linear interpolation between two colors
     * @param a Start color
     * @param b End color
     * @param t Interpolation factor (0.0 to 1.0)
     * @return Interpolated color
     */
    static Pixel4 lerp(const Pixel4& a, const Pixel4& b, float t) {
        float result = lerp(static_cast<float>(a.value), static_cast<float>(b.value), t);
        return Pixel4(static_cast<uint8_t>(result + 0.5f)); // Round to nearest
    }

private:
    /**
     * @brief Clamp value between 0.0 and 1.0
     */
    static float clamp01(float t) {
        return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
};

} // namespace enjin2