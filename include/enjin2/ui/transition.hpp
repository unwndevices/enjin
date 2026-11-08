#pragma once

#include "easing.hpp"

/**
 * @file transition.hpp
 * @brief A progress in [0, 1] that the clock or the input drives (#455)
 *
 * The state behind a screen transition. The raw progress moves linearly; what
 * the screen shows is its eased value. Two drivers, which can hand over at any
 * moment:
 *
 *   - the clock: animateTo(target, seconds) runs toward a target at a fixed
 *     rate, seconds being a full 0↔1 run (a partial run takes its share, as an
 *     interrupted transition finishing or cancelling from where it is);
 *   - the input: seek(p) puts the progress where a gesture says (a hold's
 *     elapsed time, a swipe's distance) and stops the clock.
 *
 * Re-targeting never restarts or snaps. Running an ease-in curve backwards is
 * a decelerating return: a cancelled charge drains fast, then settles.
 *
 * What the progress means (a disc's covered share, a slide's offset) is the
 * owner's: a Transition draws nothing.
 */

namespace enjin2 {

class Transition {
public:
    explicit Transition(EasingFunction ease = &Easing::Linear) : ease_(ease) {}

    /// Run toward `target` (clamped to [0, 1]); `seconds` is a whole 0↔1 run
    /// (<= 0 jumps there).
    void animateTo(float target, float seconds) {
        target_ = clamp01(target);
        if (seconds <= 0.0f) {
            p_ = target_;
            rate_ = 0.0f;
            return;
        }
        rate_ = 1.0f / seconds;
    }

    /// Put the progress at `p` (clamped) and stop the clock there.
    void seek(float p) {
        p_ = clamp01(p);
        target_ = p_;
        rate_ = 0.0f;
    }

    /// Advance the clock by `dt` seconds. Returns whether the progress moved.
    bool update(float dt) {
        if (p_ == target_ || dt <= 0.0f) return false;
        const float step = rate_ * dt;
        if (p_ < target_) {
            p_ = p_ + step >= target_ ? target_ : p_ + step;
        } else {
            p_ = p_ - step <= target_ ? target_ : p_ - step;
        }
        return true;
    }

    void setEase(EasingFunction ease) { ease_ = ease; }

    /// The raw, linear progress.
    float progress() const { return p_; }
    /// The eased progress: what the screen shows.
    float value() const { return ease_ ? ease_(p_) : p_; }
    float target() const { return target_; }
    /// Whether the clock still has somewhere to go.
    bool running() const { return p_ != target_; }
    bool atStart() const { return p_ <= 0.0f && !running(); }
    bool atEnd() const { return p_ >= 1.0f && !running(); }

private:
    static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

    EasingFunction ease_;
    float p_ = 0.0f;
    float target_ = 0.0f;
    float rate_ = 0.0f;
};

} // namespace enjin2
