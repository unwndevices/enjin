#pragma once

#include "spring.hpp"
#include "component.hpp"
#include "system.hpp"

/**
 * @file spring_component.hpp
 * @brief ECS form of the @ref enjin2::Spring on the ui World
 *
 * The scalar integrator, presets and settle thresholds live in @ref spring.hpp
 * (UI-ECS-free so `scripting/bindings.hpp` can hold a `Spring` by value). This
 * header adds the ECS wrapper for C++ consumers, mirroring
 * @ref AnimatorComponent / @ref AnimatorSystem, and is only included in
 * translation units that use the ui `World` (enjin defines a second, unrelated
 * `enjin2::Component` under `core/`, so the two cannot mix in one TU).
 */

namespace enjin2 {

/**
 * @brief ECS spring for a single animated value, generic in T
 * @tparam T Animated value type (float, or an affine type with `T±T`, `T*float`)
 *
 * Data-only, like @ref AnimatorComponent: @ref SpringSystem advances it, the host
 * reads @ref value and applies it wherever it belongs (an anchor offset, a
 * transform), never a position directly on the scene object. @ref retarget moves
 * the goal while keeping x+v so in-flight velocity is inherited (R1).
 */
template<typename T>
struct SpringComponent : public Component<SpringComponent<T>> {
    T x{};                     ///< Current position
    T v{};                     ///< Current velocity
    T target{};                ///< Goal position
    float stiffness = 0.0f;    ///< k
    float dampingRatio = 0.0f; ///< ζ

    /// @brief Set (k, ζ) from Apple's perceptual (duration, bounce).
    void setPerceptual(float durationS, float bounce) {
        const SpringParams p = springFromPerceptual(durationS, bounce);
        stiffness = p.stiffness;
        dampingRatio = p.dampingRatio;
    }

    /// @brief Set (k, ζ) from a named @ref SpringSpec preset.
    void setSpec(const SpringSpec& spec) { setPerceptual(spec.durationS, spec.bounce); }

    /// @brief Retarget without restarting — change the goal, keep x and v.
    void retarget(const T& newTarget) { target = newTarget; }

    /// @brief Advance one frame (sub-stepped), @p dtS in **seconds**.
    void advance(float dtS, int substeps = kSpringSubsteps) {
        springIntegrate(x, v, target, stiffness, dampingRatio, dtS, substeps);
    }

    /// @brief Current value seam (pure), read by the host each frame.
    const T& value() const { return x; }
};

/**
 * @brief Advances every SpringComponent<T> in a world each frame
 * @tparam TWorld World composing SpringComponent<T>
 * @tparam T Animated value type
 *
 * The spring analogue of @ref AnimatorSystem: it only integrates; reading
 * @ref SpringComponent::value and applying it stays with the host. Ticks at
 * priority 100 (before drawing) so the frame sees fresh values, exactly like the
 * animator. Instantiate one per animated value type in play.
 */
template<typename TWorld, typename T>
class SpringSystem : public System<SpringSystem<TWorld, T>> {
public:
    /// @brief Construct against the world whose springs it drives (borrowed).
    explicit SpringSystem(TWorld* world) : world_(world) {}

    /// @brief Advance every spring's integrator; @p dt in seconds.
    void update(float dt) override {
        if (!world_) return;
        for (auto entry : world_->template components<SpringComponent<T>>()) {
            entry.second->advance(dt); // entry is {Entity, SpringComponent<T>*}
        }
    }

    /// @brief Springs tick before the drawing systems, like the animator.
    int getPriority() const override { return 100; }

private:
    TWorld* world_;
};

} // namespace enjin2
