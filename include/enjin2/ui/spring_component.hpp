#pragma once

#include "spring.hpp"
#include "component.hpp"
#include "system.hpp"
#include <type_traits>

/**
 * @file spring_component.hpp
 * @brief ECS wrappers for the spring primitive (wayfinder #17 / spec #30)
 *
 * The motion math lives in @ref spring.hpp (a @ref Spring is pure state + a pure
 * step, with no ECS dependency, so the Lua binding can reuse it without dragging in
 * the ui component graph). This header is the ui-ECS face of it: a @ref
 * SpringComponent<T> data component and the priority-100 @ref SpringSystem that
 * drives every spring in a world — the same seam @ref AnimatorComponent /
 * @ref AnimatorSystem use for keyframe tweens.
 */

namespace enjin2 {

/**
 * @brief Data-only spring on a single animated value of type T
 * @tparam T Animated value type (arithmetic; the value the host applies)
 *
 * Wraps a @ref Spring and exposes the position as T, mirroring @ref
 * AnimatorComponent's pure-value seam: @ref SpringSystem advances the physics, the
 * host reads @ref value() and applies it wherever it belongs (an anchor_offset, a
 * transform). The world uses no springs (R3), so T is a chrome scalar in practice.
 */
template<typename T>
struct SpringComponent : public Component<SpringComponent<T>> {
    static_assert(std::is_arithmetic_v<T>, "SpringComponent<T> animates a scalar value");

    Spring spring; ///< The underlying 1-D spring state

    /// @brief Adopt raw (k, ζ) — see @ref Spring::setParams.
    void setParams(const SpringParams& p) { spring.setParams(p); }

    /// @brief Snap position to @p value without motion (v stays as-is).
    void setValue(T value) { spring.x = static_cast<float>(value); }

    /// @brief Retarget, keeping position + velocity — see @ref Spring::retarget.
    void setTarget(T target) { spring.retarget(static_cast<float>(target)); }

    /// @brief Current position as T (the host rounds at the raster edge — R4).
    T value() const { return static_cast<T>(spring.x); }

    /// @brief True once within the settle box — see @ref Spring::settled.
    bool settled(float posEps = kSpringPosEps, float velEps = kSpringVelEps) const {
        return spring.settled(posEps, velEps);
    }
};

/**
 * @brief Advances every SpringComponent<T> with a fixed-dt, sub-stepped tick
 * @tparam TWorld World composing SpringComponent<T>
 * @tparam T Animated value type
 *
 * Sits on the same priority-100 tick as @ref AnimatorSystem, before drawing. Real
 * frame time is accumulated and drained in fixed 30 fps chunks, each integrated in
 * @ref kSpringSubSteps micro-steps — semi-implicit Euler diverges at `ω·dt > 2`, and
 * the sub-step shrinks the effective dt below that bound for every chrome preset. A
 * spring that reaches the settle box is snapped to exact rest, so no settled spring
 * can dirty a tile (the jitter-forever bug is structurally impossible).
 */
template<typename TWorld, typename T>
class SpringSystem : public System<SpringSystem<TWorld, T>> {
public:
    /**
     * @brief Construct against the world whose springs it drives
     * @param world World holding the SpringComponent<T> entities (borrowed)
     */
    explicit SpringSystem(TWorld* world) : world_(world) {}

    /**
     * @brief Drain accumulated time in fixed, sub-stepped chunks
     * @param dt Time since last update in seconds
     */
    void update(float dt) override {
        if (!world_) return;
        accumulator_ += dt;
        if (accumulator_ > kSpringMaxAccum) accumulator_ = kSpringMaxAccum; // never chase a huge hitch
        while (accumulator_ >= kSpringFixedDt) {
            // Springs are independent, so one pass over the component view (sub-stepping
            // each in place, then settling it) is identical to N passes — and builds the
            // view once per fixed step instead of once per sub-step.
            for (auto entry : world_->template components<SpringComponent<T>>()) {
                Spring& s = entry.second->spring;
                for (int n = 0; n < kSpringSubSteps; ++n) s.step(kSpringSubDt);
                if (s.settled()) s.snapToRest();
            }
            accumulator_ -= kSpringFixedDt;
        }
    }

    /// @brief Springs tick before the drawing systems so the frame sees fresh values.
    int getPriority() const override { return 100; }

private:
    TWorld* world_;
    float accumulator_{0.0f};
};

} // namespace enjin2
