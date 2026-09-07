#pragma once

#include <cmath>

/**
 * @file spring.hpp
 * @brief Chrome motion substrate — a retargetable spring beside @ref animator.hpp
 *
 * The keyframe @ref AnimatorComponent is stateless about its target: a new
 * destination restarts a timeline and cannot inherit the value's in-flight
 * velocity, so three fast encoder detents queue three animations instead of
 * bending one arc. A spring is the opposite — it holds position *and* velocity
 * as state, so **retarget is "change the `target`, keep x+v"** and the velocity
 * carries into the new goal for free. That is the feel-spec's R1
 * (retarget-never-queue). Springs are chrome-only; the world uses none (R3).
 *
 * The integrator is **semi-implicit (symplectic) Euler** at mass 1 — `v += a·dt`
 * *then* `x += v·dt`. It is **fixed-dt sub-stepped** (@ref kSpringSubsteps = 4 at
 * 30 fps) because plain semi-implicit Euler diverges once `ω·dt > 2` (stiffness
 * past ~3600 at a 33 ms frame); the sub-steps keep a stiff `pop` stable where the
 * first, un-substepped cut exploded.
 *
 * Authoring speaks **Apple's perceptual pair** (duration, bounce) while the
 * struct stores the **raw pair** (stiffness k, damping ratio ζ): `k = (2π/d)²`,
 * `ζ = 1 − bounce`. Damping is `c = ζ·2·√k` so ζ=1 is critically damped (no
 * overshoot) and ζ<1 overshoots. Presets are in @ref springpreset.
 *
 * The **settle threshold** is what makes R4 hold after motion ends: a spring is
 * settled once `|v| < velEps` *and* `|x − target| < posEps`, with
 * `posEps = 0.5 px` (below the integer-grid round, so a settled spring cannot
 * dirty a tile) and `velEps = posEps × 1000/16 ≈ 31 px/s` (Android's rule). Half
 * a pixel under the round makes the jitter-forever bug structurally impossible.
 *
 * Two seams: the scalar @ref Spring here is the per-key integrator the Lua tween
 * pool drives (`engine.tween.spring`), and it is deliberately free of any UI-ECS
 * include so `scripting/bindings.hpp` can hold a `Spring` by value. The ECS form
 * — `SpringComponent<T>` + `SpringSystem`, on the same priority-100 tick as
 * @ref AnimatorSystem — lives in the sibling `spring_component.hpp`, which pulls
 * in `ui/component.hpp`. They are split because enjin defines two distinct
 * `enjin2::Component` types (`core/` vs `ui/`) that cannot share a translation
 * unit; keeping the scalar core UI-free lets both the bindings and the ECS layer
 * include it.
 */

namespace enjin2 {

/// @brief π, for the perceptual→raw stiffness mapping.
inline constexpr float kSpringPi = 3.14159265358979323846f;

/// @brief Sub-steps per frame; N=4 keeps semi-implicit Euler stable at high k.
inline constexpr int kSpringSubsteps = 4;

/// @brief Settle position tolerance in pixels — below the integer-grid round.
inline constexpr float kSpringPosEps = 0.5f;

/// @brief Velocity-tolerance factor (Android's rule): `velEps = posEps × factor`.
inline constexpr float kSpringVelEpsFactor = 1000.0f / 16.0f; // 62.5

/// @brief Default settle velocity tolerance ≈ 31 px/s.
inline constexpr float kSpringVelEps = kSpringPosEps * kSpringVelEpsFactor;

/**
 * @brief Raw spring coefficients (stiffness, damping ratio)
 *
 * The integrator's native pair. @ref springFromPerceptual derives it from the
 * authoring pair; the struct and system store this, never (duration, bounce).
 */
struct SpringParams {
    float stiffness;    ///< k — restoring force per unit displacement
    float dampingRatio; ///< ζ — 1 critically damped, <1 overshoots, >1 overdamped
};

/**
 * @brief Apple's perceptual (duration, bounce) → raw (k, ζ)
 * @param durationS Perceptual settle duration in seconds (must be > 0)
 * @param bounce Overshoot amount; 0 critically damped, →1 very springy
 * @return `{ (2π/duration)², 1 − bounce }`
 */
inline SpringParams springFromPerceptual(float durationS, float bounce) {
    const float d = (durationS > 0.0001f) ? durationS : 0.0001f;
    const float omega = (2.0f * kSpringPi) / d;
    return { omega * omega, 1.0f - bounce };
}

/**
 * @brief A named perceptual spring shape (authoring units)
 *
 * Presets carry (duration, bounce), not (k, ζ), so they read the way an author
 * thinks; @ref Spring::setSpec derives the raw pair.
 */
struct SpringSpec {
    float durationS; ///< Perceptual settle duration in seconds
    float bounce;    ///< Overshoot amount in [0, 1)
};

/**
 * @brief Chrome-only spring presets (the world uses none — R3)
 *
 * `pop` is the selection default: a big, toy-register overshoot. `snap` is a
 * quicker border/nudge kick. `smooth` is critically damped for the scene lean,
 * which must not overshoot.
 */
namespace springpreset {
inline constexpr SpringSpec Pop{0.34f, 0.55f};    ///< selection pop (k≈341, ζ≈0.45)
inline constexpr SpringSpec Snap{0.22f, 0.42f};   ///< border / small nudge
inline constexpr SpringSpec Smooth{0.35f, 0.0f};  ///< scene lean, no overshoot
} // namespace springpreset

/**
 * @brief One fixed-dt sub-stepped semi-implicit Euler advance, generic in T
 * @tparam T Value type (float, or any affine type with `T±T` and `T*float`)
 * @param x [in,out] position, integrated in place
 * @param v [in,out] velocity, integrated in place
 * @param target Goal position
 * @param k Stiffness
 * @param zeta Damping ratio
 * @param dtS Frame time in **seconds**
 * @param substeps Sub-steps this frame (clamped to ≥1)
 *
 * `a = −k·(x−target) − c·v` with `c = ζ·2·√k`, applied `v += a·subDt` then
 * `x += v·subDt` per sub-step. Velocity survives the call, which is what lets a
 * retarget (just moving @p target) inherit the in-flight velocity.
 */
template<typename T>
inline void springIntegrate(T& x, T& v, const T& target,
                            float k, float zeta, float dtS, int substeps) {
    if (substeps < 1) substeps = 1;
    const float subDt = dtS / static_cast<float>(substeps);
    const float c = zeta * 2.0f * std::sqrt(k);
    for (int i = 0; i < substeps; ++i) {
        const T a = (x - target) * (-k) - v * c; // −k(x−target) − c·v
        v = v + a * subDt;
        x = x + v * subDt;
    }
}

/**
 * @brief Scalar retargetable spring — the Lua tween pool's per-key integrator
 *
 * Stores the raw (k, ζ) plus live (x, v, target). Authoring goes through
 * @ref setPerceptual / @ref setSpec; @ref retarget moves the goal while keeping
 * x+v (the R1 velocity inheritance); @ref advance sub-steps one frame;
 * @ref settled is the R4-safe stop test.
 */
struct Spring {
    float x = 0.0f;             ///< Current position
    float v = 0.0f;             ///< Current velocity
    float target = 0.0f;        ///< Goal position
    float stiffness = 0.0f;     ///< k
    float dampingRatio = 0.0f;  ///< ζ

    /// @brief Set (k, ζ) from Apple's perceptual (duration, bounce).
    void setPerceptual(float durationS, float bounce) {
        const SpringParams p = springFromPerceptual(durationS, bounce);
        stiffness = p.stiffness;
        dampingRatio = p.dampingRatio;
    }

    /// @brief Set (k, ζ) from a named @ref SpringSpec preset.
    void setSpec(const SpringSpec& spec) { setPerceptual(spec.durationS, spec.bounce); }

    /**
     * @brief Retarget without restarting — change the goal, keep x and v
     * @param newTarget New goal position
     *
     * The whole point of a spring over a tween: in-flight velocity is inherited
     * for free, so rapid retargets bend one continuous arc (R1).
     */
    void retarget(float newTarget) { target = newTarget; }

    /**
     * @brief Advance one frame (sub-stepped)
     * @param dtS Frame time in **seconds**
     * @param substeps Sub-steps (default @ref kSpringSubsteps)
     */
    void advance(float dtS, int substeps = kSpringSubsteps) {
        springIntegrate(x, v, target, stiffness, dampingRatio, dtS, substeps);
    }

    /**
     * @brief True once the spring has settled below the R4-safe thresholds
     * @param posEps Position tolerance (default @ref kSpringPosEps = 0.5 px)
     * @param velEps Velocity tolerance (default ≈ 31 px/s)
     *
     * Both conditions must hold. `posEps` under the integer-grid round means a
     * settled spring cannot dirty a tile — the jitter-forever bug is impossible.
     */
    bool settled(float posEps = kSpringPosEps, float velEps = kSpringVelEps) const {
        return std::fabs(v) < velEps && std::fabs(x - target) < posEps;
    }

    /// @brief Snap exactly onto the target and kill velocity (called on settle).
    void snap() { x = target; v = 0.0f; }
};

} // namespace enjin2
