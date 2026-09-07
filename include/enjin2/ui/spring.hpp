#pragma once

#include "../core/physics.hpp"
#include <cmath>
#include <cstdint>

/**
 * @file spring.hpp
 * @brief Retargetable spring motion primitive for the ui ECS (wayfinder #17 / #30)
 *
 * The chrome needs to move like a toy: punchy, overshooting, settling fast, and
 * above all **retargetable** — three fast encoder detents must produce one
 * continuous arc, not three queued animations. A stateless tween (@ref
 * animator.hpp) cannot inherit velocity into a new target; a spring can, because
 * its state *is* position + velocity. This is the motion substrate the launcher
 * chrome and scene pop are built on; @ref animator.hpp keeps its place for
 * one-shots.
 *
 * The integrator is **semi-implicit (symplectic) Euler** at mass 1:
 *   `a = -k·(x - target) - c·v`,  then  `v += a·dt`,  then  `x += v·dt`.
 * Damping is `c = ζ·2·√k`, so ζ (the damping *ratio*) reads directly: ζ<1
 * underdamped/overshoots, ζ=1 critically damped, ζ>1 sluggish. The velocity half
 * reuses @ref physics::springForce (the same Hooke's-law step) so there is one
 * copy of the force law in the engine.
 *
 * Two design facts pin the defaults:
 *   - **Perceptual authoring.** The struct stores raw (k, ζ); presets and the Lua
 *     surface speak Apple's perceptual pair (duration, bounce) via
 *     @ref springParamsFromPerceptual. `k = (2π/duration)²`, `ζ = 1 − bounce`.
 *   - **Sub-stepping.** Semi-implicit Euler diverges once `ω·dt > 2` (here
 *     `k > ~3600` at 30 fps). @ref SpringSystem integrates each fixed 30 fps frame
 *     in `N = 4` micro-steps, shrinking the effective dt so a stiff/springy spring
 *     stays stable — the item-6 finding from the #17 playground.
 *
 * The settle threshold (@ref Spring::settled) is deliberately below the
 * integer-pixel round: `posEps = 0.5 px` with `velEps = posEps × 62.5 ≈ 31 px/s`.
 * Half a pixel is *below* R4's round-to-integer, so once a spring reports settled
 * it is snapped to rest by its driver (@ref SpringSystem) — the jitter-forever
 * tile-dirtying bug is structurally impossible.
 */

namespace enjin2 {

/// @brief Two-π, for the perceptual `duration → stiffness` map.
inline constexpr float kSpringTwoPi = 6.28318530717958647692f;

/// @brief Fixed integration step in seconds (30 fps chunk drained by the driver).
inline constexpr float kSpringFixedDt = 1.0f / 30.0f;
/// @brief Micro-steps per fixed step — keeps `ω·dt ≤ 2` for every chrome preset.
inline constexpr int kSpringSubSteps = 4;
/// @brief Effective integrator dt (`kSpringFixedDt / kSpringSubSteps`).
inline constexpr float kSpringSubDt = kSpringFixedDt / kSpringSubSteps;
/// @brief Accumulator cap in seconds — never chase a huge hitch (spiral-of-death guard).
inline constexpr float kSpringMaxAccum = 0.25f;

/// @brief Settle position tolerance in pixels — below the integer-pixel round.
inline constexpr float kSpringPosEps = 0.5f;
/// @brief velEps-per-posEps factor (Android's `1000/16`); velEps = posEps × this.
inline constexpr float kSpringVelEpsFactor = 62.5f;
/// @brief Settle velocity tolerance in px/s (`kSpringPosEps × kSpringVelEpsFactor`).
inline constexpr float kSpringVelEps = kSpringPosEps * kSpringVelEpsFactor;

/// @brief Shortest perceptual duration the sub-stepped integrator stays stable at.
/// Semi-implicit Euler diverges once `ω·dt > 2`; with `ω = 2π/duration` and the
/// @ref kSpringSubDt micro-step, that bound is `duration ≥ 2π·kSpringSubDt/2 ≈ 26 ms`.
/// 50 ms leaves comfortable margin (`ω·dt ≈ 1.05`) — clamp Lua input to it.
inline constexpr float kSpringMinDurationS = 0.05f;
/// @brief Largest bounce a spring is allowed (ζ = 1 − bounce). bounce ≥ 1 gives
/// ζ ≤ 0 — a divergent, energy-adding spring; even ζ near 0 rings for seconds and
/// keeps dirtying tiles. Capped at the #17 sim's explored max (0.7 → ζ 0.3), which
/// overshoots lively yet still settles promptly.
inline constexpr float kSpringMaxBounce = 0.7f;

/**
 * @brief Raw spring parameters: stiffness k and damping ratio ζ
 *
 * The integrator's native pair. Author with @ref springParamsFromPerceptual or a
 * @ref SpringPresets factory rather than filling these by hand.
 */
struct SpringParams {
    float stiffness{0.0f};    ///< k — higher = stiffer/faster
    float dampingRatio{0.0f}; ///< ζ — <1 overshoots, 1 critical, >1 sluggish
};

/**
 * @brief Apple perceptual (duration, bounce) → raw (k, ζ)
 * @param durationS Perceptual settle duration in seconds (must be > 0)
 * @param bounce Bounce in [0, 1); 0 is critically damped, higher overshoots more
 * @return The raw @ref SpringParams the integrator uses
 *
 * `k = (2π/duration)²` and `ζ = 1 − bounce`, matching the #17 playground so the
 * presets land on the numbers the sim was tuned against (e.g. 0.34 s / 0.55 →
 * k ≈ 341, ζ ≈ 0.45).
 */
inline SpringParams springParamsFromPerceptual(float durationS, float bounce) {
    const float omega = kSpringTwoPi / durationS;
    return SpringParams{omega * omega, 1.0f - bounce};
}

/**
 * @brief Chrome-only spring presets (the world uses no springs — R3)
 *
 * Tuned in the #17 proving ground, not lifted from Android. Perceptual pairs are
 * resolved to raw (k, ζ) on demand.
 */
namespace SpringPresets {
    /// @brief Selection pop — big springy overshoot, the toy register (0.34 s / 0.55).
    inline SpringParams pop()    { return springParamsFromPerceptual(0.34f, 0.55f); }
    /// @brief Border / small nudge — quick with a real kick (0.22 s / 0.42).
    inline SpringParams snap()   { return springParamsFromPerceptual(0.22f, 0.42f); }
    /// @brief Scene lean — critically damped, no overshoot (0.35 s / 0.0).
    inline SpringParams smooth() { return springParamsFromPerceptual(0.35f, 0.0f); }
}

/**
 * @brief A single retargetable 1-D spring (position + velocity + target)
 *
 * Pure state and a pure step; no world, no clock of its own. **Retarget is
 * "change @ref target, keep @ref x and @ref v"** — the in-flight velocity is
 * inherited for free, which is what lets rapid detents read as one continuous arc
 * (R1). Drive it from @ref SpringComponent on the ECS, or directly in a test.
 */
struct Spring {
    float x{0.0f};            ///< Current position
    float v{0.0f};            ///< Current velocity
    float target{0.0f};       ///< Rest position the spring pulls toward
    float stiffness{0.0f};    ///< k (read-only mirror; set via @ref setParams)
    float dampingRatio{0.0f}; ///< ζ (read-only mirror; set via @ref setParams)

    /**
     * @brief Adopt raw (k, ζ), leaving x/v/target untouched.
     *
     * Caches the damping coefficient `c = ζ·2·√k` so @ref step never recomputes the
     * √k on the per-micro-step hot path. Because `c` is cached, change stiffness /
     * dampingRatio *only* through this method — never by writing the fields directly.
     */
    void setParams(const SpringParams& p) {
        stiffness = p.stiffness;
        dampingRatio = p.dampingRatio;
        damping_ = dampingRatio * 2.0f * std::sqrt(stiffness);
    }

    /**
     * @brief Retarget: change the goal, keep position and velocity
     * @param newTarget The new rest position
     *
     * The whole point of a spring over a tween — no restart, no queue. The
     * existing velocity carries into the new goal, so a reversal mid-flight bends
     * the arc instead of snapping it.
     */
    void retarget(float newTarget) { target = newTarget; }

    /**
     * @brief Advance one micro-step of semi-implicit Euler
     * @param dt Sub-step duration in seconds (keep `ω·dt ≤ 2` for stability)
     *
     * Uses the cached `c = ζ·2·√k` (see @ref setParams); the velocity half is
     * @ref physics::springForce (Hooke's law + velocity damping), then position
     * integrates against the *updated* velocity.
     */
    void step(float dt) {
        physics::springForce(x, target, v, stiffness, damping_, dt, &v);
        x += v * dt;
    }

    /**
     * @brief True once motion is within the settle box (rest is safe to snap)
     * @param posEps Position tolerance (default @ref kSpringPosEps)
     * @param velEps Velocity tolerance (default @ref kSpringVelEps)
     *
     * Both conditions must hold: near the target *and* nearly stopped. The driver
     * snaps to exact rest on the first true so sub-pixel spiral cannot dirty a tile.
     */
    bool settled(float posEps = kSpringPosEps, float velEps = kSpringVelEps) const {
        return std::fabs(v) < velEps && std::fabs(x - target) < posEps;
    }

    /// @brief Snap to exact rest (x = target, v = 0) — the structural settle latch.
    void snapToRest() {
        x = target;
        v = 0.0f;
    }

private:
    float damping_{0.0f}; ///< Cached `c = ζ·2·√k`, kept in sync by @ref setParams.
};

} // namespace enjin2
