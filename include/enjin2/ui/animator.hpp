#pragma once

#include "component.hpp"
#include "system.hpp"
#include "easing.hpp"
#include "../core/types.hpp"
#include <algorithm>
#include <type_traits>
#include <vector>

/**
 * @file animator.hpp
 * @brief Generic keyframe animator on the ui ECS
 *
 * Collapses the *value interpolation* of Eisei's three animators —
 * `C_PositionAnimator` (Vector2), `C_ParameterAnimator<T>` (scalar) and
 * `C_KeyframeAnimator` (Vector2) — into one data-only @ref AnimatorComponent
 * parameterised on the animated value type, driven by a single
 * @ref AnimatorSystem (#121).
 *
 * The rewrite keeps the timeline and the clock together on the component and
 * exposes the interpolated value through a pure @ref AnimatorComponent::value
 * seam. The system only advances the clock each frame; **applying** the value
 * (writing it onto a PositionComponent, an opacity, a frame index) stays at the
 * host edge, matching the presentation-only split the rest of the widgets use.
 * Easing curves come from @ref easing.hpp; the destination keyframe's curve
 * shapes each segment, exactly as the old animators did.
 *
 * Two side-channels of the old animators are deliberately **not** carried here,
 * because they are no longer this type's concern:
 *   - `C_KeyframeAnimator`'s per-keyframe `AnimationState` (a sprite-frame index)
 *     is a sprite concern; the host advances the sprite alongside reading @ref
 *     value, rather than the animator reaching into an animation component.
 *   - The completion callback (`SetEndCallback`) becomes the pollable
 *     @ref AnimatorComponent::finished flag; the host reacts to it in its own
 *     update rather than the animator invoking a stored `std::function`.
 */

namespace enjin2 {

/**
 * @brief One point on an animation timeline
 * @tparam T Animated value type (e.g. Vec2, float, uint8_t)
 *
 * The @ref easing curve belongs to the segment that *ends* at this keyframe —
 * it shapes the interpolation from the previous keyframe up to this one.
 */
template<typename T>
struct Keyframe {
    uint32_t timeMs;         ///< Timeline position in milliseconds
    T value;                 ///< Value held at this keyframe
    EasingFunction easing;   ///< Curve for the segment ending here
};

/**
 * @brief Linear blend between two animated values
 * @tparam T Value type (arithmetic scalars or Vec2)
 * @param a Start value
 * @param b End value
 * @param t Normalized blend factor in [0, 1]
 * @return `a + (b - a) * t`
 *
 * Scalars blend in float and cast back (so integer parameters such as opacity
 * round through a continuous curve rather than snapping); Vec2 uses its own
 * vector arithmetic. Extend with an overload to animate a new value type.
 */
template<typename T>
inline T lerpValue(const T& a, const T& b, float t) {
    if constexpr (std::is_arithmetic_v<T>) {
        return static_cast<T>(static_cast<float>(a) +
                              (static_cast<float>(b) - static_cast<float>(a)) * t);
    } else {
        return a + (b - a) * t; // Vec2 and any type modelling an affine space
    }
}

/**
 * @brief Data-only keyframe timeline for a single animated value
 * @tparam T Animated value type
 *
 * Holds a time-sorted keyframe list plus the playback clock. The @ref value it
 * yields is a pure function of the elapsed clock, so the interpolation can be
 * pinned in a test without a world or a canvas. The host reads @ref value each
 * frame and applies it wherever it belongs.
 */
template<typename T>
struct AnimatorComponent : public Component<AnimatorComponent<T>> {
    std::vector<Keyframe<T>> keyframes; ///< Time-sorted timeline (see @ref addKeyframe)

    /// @brief Add a keyframe, keeping the timeline sorted by @ref Keyframe::timeMs.
    void addKeyframe(Keyframe<T> kf) {
        keyframes.push_back(kf);
        std::sort(keyframes.begin(), keyframes.end(),
                  [](const Keyframe<T>& a, const Keyframe<T>& b) { return a.timeMs < b.timeMs; });
    }

    /// @brief Drop every keyframe and reset the clock.
    void clearKeyframes() {
        keyframes.clear();
        elapsedMs_ = 0.0f;
        playing_ = false;
    }

    /**
     * @brief Set how many times the timeline repeats
     * @param count Loop count; a negative value disables looping (plays once)
     */
    void setLoops(int count) {
        if (count < 0) {
            looping_ = false;
            loopsLeft_ = 0;
        } else {
            looping_ = true;
            loopsLeft_ = count;
        }
    }

    /**
     * @brief Rebase the first keyframe so the animation starts from @p value
     * @param value New value for keyframe 0
     *
     * The presentation-only stand-in for the old animators' "read the current
     * value through a getter on StartAnimation" behaviour: the host, which owns
     * the live value, hands it in before @ref play so the timeline eases out of
     * wherever the target currently sits rather than snapping to keyframe 0.
     */
    void retargetStart(const T& value) {
        if (!keyframes.empty()) keyframes.front().value = value;
    }

    /**
     * @brief Start (or restart) playback from the top of the timeline
     * @param loops Loop count forwarded to @ref setLoops (default: play once)
     */
    void play(int loops = -1) {
        setLoops(loops);
        elapsedMs_ = 0.0f;
        playing_ = !keyframes.empty();
    }

    /// @brief Halt playback and rewind the clock to the start.
    void stop() {
        playing_ = false;
        elapsedMs_ = 0.0f;
    }

    /// @brief True while the clock is advancing.
    bool playing() const { return playing_; }

    /// @brief True once a non-looping timeline has run past its last keyframe.
    bool finished() const { return !playing_ && elapsedMs_ > 0.0f; }

    /// @brief Total timeline length in milliseconds (0 if empty).
    float durationMs() const {
        return keyframes.empty() ? 0.0f : static_cast<float>(keyframes.back().timeMs);
    }

    /**
     * @brief Advance the playback clock (time-based seam)
     * @param dtMs Elapsed time in milliseconds
     *
     * Steps the clock while playing, wrapping it for a looping timeline until the
     * loop budget is spent and clamping it to the end otherwise (which latches
     * @ref finished). Extracted from the animators' `Update` so it can be pinned
     * without a system.
     */
    void advance(float dtMs) {
        if (!playing_ || keyframes.empty()) return;
        elapsedMs_ += dtMs;

        const float duration = durationMs();
        if (elapsedMs_ < duration) return;

        if (looping_ && loopsLeft_ > 0 && duration > 0.0f) {
            --loopsLeft_;
            elapsedMs_ -= duration; // carry the overshoot into the next loop
        } else {
            elapsedMs_ = duration;
            playing_ = false; // latch finished() at the final value
        }
    }

    /**
     * @brief Interpolated value at the current clock position (pure)
     * @return The eased blend for the active segment; the endpoint value outside it
     *
     * Before the first keyframe's time the first value holds; after the last, the
     * last value holds. Within a segment the *destination* keyframe's easing curve
     * shapes the blend, matching the source animators.
     */
    T value() const {
        if (keyframes.empty()) return T{};
        if (keyframes.size() == 1) return keyframes.front().value;

        const float t = elapsedMs_;
        if (t <= static_cast<float>(keyframes.front().timeMs)) return keyframes.front().value;
        if (t >= static_cast<float>(keyframes.back().timeMs)) return keyframes.back().value;

        // Find the segment [i, i+1] containing the clock.
        size_t i = 0;
        while (i + 1 < keyframes.size() &&
               static_cast<float>(keyframes[i + 1].timeMs) <= t) {
            ++i;
        }
        const Keyframe<T>& from = keyframes[i];
        const Keyframe<T>& to = keyframes[i + 1];

        const float span = static_cast<float>(to.timeMs) - static_cast<float>(from.timeMs);
        float local = (span > 0.0f) ? (t - static_cast<float>(from.timeMs)) / span : 1.0f;
        const float eased = to.easing ? to.easing(local) : local;
        return lerpValue(from.value, to.value, eased);
    }

    /// @brief Current clock position in milliseconds (mostly for tests).
    float elapsedMs() const { return elapsedMs_; }

private:
    float elapsedMs_ = 0.0f;
    bool playing_ = false;
    bool looping_ = false;
    int loopsLeft_ = 0;
};

/**
 * @brief Advances every AnimatorComponent<T> in a world each frame
 * @tparam TWorld World composing AnimatorComponent<T>
 * @tparam T Animated value type
 *
 * The animators' shared clock, lifted out of the per-object `Update`. It only
 * ticks time forward; reading @ref AnimatorComponent::value and applying it stays
 * with the host. Instantiate one per animated value type in play.
 */
template<typename TWorld, typename T>
class AnimatorSystem : public System<AnimatorSystem<TWorld, T>> {
public:
    /**
     * @brief Construct against the world whose animators it drives
     * @param world World holding the AnimatorComponent<T> entities (borrowed)
     */
    explicit AnimatorSystem(TWorld* world) : world_(world) {}

    /**
     * @brief Advance every animator's clock
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        if (!world_) return;
        const float dtMs = dt * 1000.0f;
        for (auto entry : world_->template components<AnimatorComponent<T>>()) {
            entry.second->advance(dtMs); // entry is {Entity, AnimatorComponent<T>*}
        }
    }

    /// @brief Animators tick before the drawing systems so the frame sees fresh values.
    int getPriority() const override { return 100; }

private:
    TWorld* world_;
};

} // namespace enjin2
