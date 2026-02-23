/**
 * @file animation_track.hpp
 * @brief Template animation track for keyframe-based animations
 *
 * Provides keyframe interpolation, playback control, and animation events
 * for position, float, and color animations.
 */
#pragma once

#include "keyframe.hpp"
#include "../core/signal.hpp"
#include <array>

namespace enjin2 {

/**
 * @brief Template animation track for keyframe-based animations
 * @tparam T Value type (Point, float, Pixel4, etc.)
 * @tparam KeyframeType Keyframe type (PositionKeyframe, FloatKeyframe, etc.)
 */
template<typename T, typename KeyframeType>
class AnimationTrack {
private:
    static constexpr size_t MAX_KEYFRAMES = 16;  ///< Maximum keyframes per track
    
    std::array<KeyframeType, MAX_KEYFRAMES> keyframes;
    size_t keyframeCount;
    
    uint16_t currentTime;       ///< Current playback time in milliseconds
    uint16_t duration;          ///< Total duration in milliseconds
    AnimationState state;       ///< Current animation state
    LoopMode loopMode;          ///< Loop behavior
    bool reversed;              ///< True when playing in reverse (ping-pong)
    
    // Animation events
    Signal<> onStartSignal;     ///< Emitted when animation starts
    Signal<> onCompleteSignal;  ///< Emitted when animation completes
    Signal<T> onUpdateSignal;   ///< Emitted when value updates
    
public:
    /**
     * @brief Constructor
     */
    AnimationTrack() 
        : keyframeCount(0), currentTime(0), duration(0), 
          state(AnimationState::STOPPED), loopMode(LoopMode::NONE), reversed(false) {}
    
    /**
     * @brief Add a keyframe to the track
     * @param keyframe Keyframe to add
     * @return True if keyframe was added successfully
     */
    bool addKeyframe(const KeyframeType& keyframe) {
        if (keyframeCount >= MAX_KEYFRAMES) {
            return false;
        }
        
        // Insert keyframe in sorted order by time
        size_t insertIndex = 0;
        for (size_t i = 0; i < keyframeCount; ++i) {
            if (keyframes[i].time <= keyframe.time) {
                insertIndex = i + 1;
            } else {
                break;
            }
        }
        
        // Shift existing keyframes
        for (size_t i = keyframeCount; i > insertIndex; --i) {
            keyframes[i] = keyframes[i - 1];
        }
        
        keyframes[insertIndex] = keyframe;
        keyframeCount++;
        
        // Update duration
        if (keyframeCount > 0) {
            duration = keyframes[keyframeCount - 1].time;
        }
        
        return true;
    }
    
    /**
     * @brief Clear all keyframes
     */
    void clearKeyframes() {
        keyframeCount = 0;
        duration = 0;
        currentTime = 0;
        state = AnimationState::STOPPED;
    }
    
    /**
     * @brief Start animation playback
     */
    void play() {
        if (keyframeCount == 0) return;
        
        state = AnimationState::PLAYING;
        if (currentTime >= duration && loopMode == LoopMode::NONE) {
            currentTime = 0;  // Restart if at end
        }
        onStartSignal.emit();
    }
    
    /**
     * @brief Pause animation playback
     */
    void pause() {
        if (state == AnimationState::PLAYING) {
            state = AnimationState::PAUSED;
        }
    }
    
    /**
     * @brief Stop animation playback and reset to beginning
     */
    void stop() {
        state = AnimationState::STOPPED;
        currentTime = 0;
        reversed = false;
    }
    
    /**
     * @brief Set loop mode
     * @param mode Loop mode to set
     */
    void setLoopMode(LoopMode mode) {
        loopMode = mode;
    }
    
    /**
     * @brief Update animation by time delta
     * @param deltaTime Time elapsed since last update in milliseconds
     */
    void update(uint16_t deltaTime) {
        if (state != AnimationState::PLAYING || keyframeCount == 0) {
            return;
        }
        
        // Update time based on direction
        if (reversed) {
            if (currentTime >= deltaTime) {
                currentTime -= deltaTime;
            } else {
                currentTime = 0;
                handleLoopBoundary();
                return;
            }
        } else {
            currentTime += deltaTime;
            if (currentTime >= duration) {
                handleLoopBoundary();
                return;
            }
        }
        
        // Calculate current value and emit update
        T currentValue = evaluateAtTime(currentTime);
        onUpdateSignal.emit(currentValue);
    }
    
    /**
     * @brief Get current animation value
     * @return Current interpolated value
     */
    T getCurrentValue() const {
        return evaluateAtTime(currentTime);
    }
    
    /**
     * @brief Get animation state
     * @return Current animation state
     */
    AnimationState getState() const {
        return state;
    }
    
    /**
     * @brief Get current time
     * @return Current playback time in milliseconds
     */
    uint16_t getCurrentTime() const {
        return currentTime;
    }
    
    /**
     * @brief Get animation duration
     * @return Total duration in milliseconds
     */
    uint16_t getDuration() const {
        return duration;
    }
    
    /**
     * @brief Get normalized progress (0.0 to 1.0)
     * @return Animation progress
     */
    float getProgress() const {
        return duration > 0 ? static_cast<float>(currentTime) / static_cast<float>(duration) : 0.0f;
    }

    /**
     * @brief Connect to animation start event
     * @param callback Function to call when animation starts
     * @return Signal connection for disconnecting callback
     */
    SignalConnection<> connectOnStart(std::function<void()> callback) {
        return SignalConnection<>(&onStartSignal, callback);
    }

    /**
     * @brief Connect to animation complete event
     * @param callback Function to call when animation completes
     * @return Signal connection for disconnecting callback
     */
    SignalConnection<> connectOnComplete(std::function<void()> callback) {
        return SignalConnection<>(&onCompleteSignal, callback);
    }

    /**
     * @brief Connect to animation update event
     * @param callback Function to call when animation value updates
     * @return Signal connection for disconnecting callback
     */
    SignalConnection<T> connectOnUpdate(std::function<void(T)> callback) {
        return SignalConnection<T>(&onUpdateSignal, callback);
    }

private:
    /**
     * @brief Handle reaching loop boundary
     */
    void handleLoopBoundary() {
        switch (loopMode) {
            case LoopMode::NONE:
                currentTime = duration;
                state = AnimationState::FINISHED;
                onCompleteSignal.emit();
                break;
                
            case LoopMode::LOOP:
                currentTime = currentTime % duration;
                break;
                
            case LoopMode::PING_PONG:
                reversed = !reversed;
                if (reversed) {
                    currentTime = duration - (currentTime - duration);
                } else {
                    currentTime = 0;
                }
                break;
        }
    }
    
    /**
     * @brief Evaluate animation value at specific time
     * @param time Time to evaluate at
     * @return Interpolated value at time
     */
    T evaluateAtTime(uint16_t time) const {
        if (keyframeCount == 0) {
            return T{};  // Default constructed value
        }
        
        if (keyframeCount == 1 || time <= keyframes[0].time) {
            return getValue(keyframes[0]);
        }
        
        if (time >= keyframes[keyframeCount - 1].time) {
            return getValue(keyframes[keyframeCount - 1]);
        }
        
        // Find keyframes to interpolate between
        for (size_t i = 0; i < keyframeCount - 1; ++i) {
            if (time >= keyframes[i].time && time <= keyframes[i + 1].time) {
                return interpolateBetween(keyframes[i], keyframes[i + 1], time);
            }
        }
        
        return getValue(keyframes[keyframeCount - 1]);
    }
    
    /**
     * @brief Get value from keyframe (specialized for each keyframe type)
     */
    T getValue(const KeyframeType& keyframe) const;
    
    /**
     * @brief Interpolate between two keyframes
     */
    T interpolateBetween(const KeyframeType& from, const KeyframeType& to, uint16_t time) const {
        if (from.time == to.time) {
            return getValue(to);
        }
        
        float t = static_cast<float>(time - from.time) / static_cast<float>(to.time - from.time);
        float easedT = EasingFunctions::ease(t, from.easing);
        
        return EasingFunctions::lerp(getValue(from), getValue(to), easedT);
    }
};

// Template specializations for getValue
template<>
inline Point AnimationTrack<Point, PositionKeyframe>::getValue(const PositionKeyframe& keyframe) const {
    return keyframe.position;
}

template<>
inline float AnimationTrack<float, FloatKeyframe>::getValue(const FloatKeyframe& keyframe) const {
    return keyframe.value;
}

template<>
inline Pixel4 AnimationTrack<Pixel4, ColorKeyframe>::getValue(const ColorKeyframe& keyframe) const {
    return keyframe.color;
}

/// @brief Animation track for 2D position keyframes
using PositionTrack = AnimationTrack<Point, PositionKeyframe>;
/// @brief Animation track for scalar float keyframes
using FloatTrack = AnimationTrack<float, FloatKeyframe>;
/// @brief Animation track for 4-bit color keyframes
using ColorTrack = AnimationTrack<Pixel4, ColorKeyframe>;

} // namespace enjin2