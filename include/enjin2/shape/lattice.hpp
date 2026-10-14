/**
 * @file lattice.hpp
 * @brief The Lattice: the springy grid a Shape group is bent by (ADR-0016, Tomodachi #334)
 *
 * An N×M grid of nodes (default 5×5) over a box in the group's own
 * coordinates. Each node is pulled home by a home spring and to its
 * neighbours by structural (row, column) and shear (diagonal) springs; every
 * point of the box (and past it, extrapolated) is displaced by the bilinear
 * blend of its cell's four nodes, so the Body and every `field` Feature bend
 * with it and a `rigid` Feature is carried by the displacement at its anchor.
 *
 * ## The sim
 *
 * Tuning is the prototype's (#314): stiffness 3.2 Hz, damping ζ 0.55, with
 * w = 2π·Hz, home springs at 0.45 w, structural springs at 1.2 w, shear springs
 * at 0.7 of that stiffness, and damping 2ζ·(its own w) on every spring. The
 * springs are the prototype's linearised about rest (the force is the stretch
 * along the rest direction), so rest is the exact equilibrium and no square
 * root is taken. Semi-implicit Euler on a **fixed 30 Hz tick of 4 sub-steps**,
 * driven by an accumulator, so the motion does not depend on the frame rate.
 * All of it is Fixed (integer) arithmetic: bit-identical on every target.
 *
 * The stiffness is **clamped so 4 sub-steps stay stable**: every mode of the
 * grid is a damped oscillator with ω² = wh² + ws²λ and c = 2ζ(wh + ws·λ), λ an
 * eigenvalue of the spring Laplacian, and semi-implicit Euler is stable while
 * h²ω² + 2hc < 4. λ ≤ 4 + 5.6·max(ex², ey²) (ex, ey the cell diagonal's unit
 * direction) on any grid, so configure() lowers the stiffness until the worst
 * mode keeps a margin (≤ 3.6). The default tuning on near-square cells is not
 * clamped; a very elongated grid is.
 *
 * ## Sleep and snap
 *
 * After each tick, if every node's displacement is under ε and its speed under
 * ε per tick (start: 1/16 px), for K ticks in a row (start: 6), the Lattice
 * snaps every node home and sleeps: tick() then steps nothing and reports no
 * change until impulse() or wake().
 *
 * **The snap leaves no pixel pop.** Majority coverage has exact ties (a pixel
 * an edge halves counts 8/16), so any displacement at all, however small, can
 * flip a pixel. The field is therefore *drawn* with a dead zone: a node whose
 * displacement component is under ε counts as home in displacementAt(). Once
 * every node is under ε, the drawn field is exactly the rest field, the quiet
 * ticks before the snap redraw nothing, and the snap itself changes nothing.
 * The sim state is never rounded (only what is drawn).
 */
#pragma once

#include <cstdint>

#include "../core/fixed.hpp"

namespace enjin2 {
namespace shape {

/**
 * @brief How a Feature rides the Lattice (glossary › Binding). Same values as
 *        the Shape pack's ShapeBinding, so a record converts with a cast.
 *
 * `Sway` rides as `Rigid` until its own spring lands (Tomodachi #335).
 */
enum class Binding : uint8_t { Field = 0, Rigid = 1, Sway = 2 };

/// The Lattice's feel and its sleep test.
struct LatticeTuning {
    Fixed   stiffnessHz  = Fixed::fromRatio(32, 10);   ///< 3.2 Hz (clamped, see the file comment).
    Fixed   damping      = Fixed::fromRatio(55, 100);  ///< ζ, 0..2.
    Fixed   sleepEpsilon = Fixed::fromRatio(1, 16);    ///< px, and px per tick for the speed.
    uint8_t sleepTicks   = 6;                          ///< Ticks under ε before the snap.
};

class Lattice {
public:
    static constexpr uint8_t  kMaxSide           = 8;  ///< Nodes per side, at most.
    static constexpr uint8_t  kDefaultSide       = 5;
    static constexpr uint32_t kTickHz            = 30;
    static constexpr uint8_t  kSubSteps          = 4;
    static constexpr uint8_t  kMaxTicksPerUpdate = 4;  ///< A longer frame drops the rest.
    static constexpr int32_t  kMaxStiffnessHz    = 8;

    /**
     * @brief Lay an nx × ny grid over the box [min, max], at rest and asleep.
     * @return false (and an unconfigured Lattice) for a side outside 2..8 or
     *         an empty box.
     */
    bool configure(FixedVec2 min, FixedVec2 max, uint8_t nx = kDefaultSide, uint8_t ny = kDefaultSide,
                   const LatticeTuning& tuning = LatticeTuning{});
    bool configured() const { return nx_ != 0; }
    FixedVec2 boxMin() const { return min_; }
    FixedVec2 boxMax() const { return max_; }
    /// The stiffness the sim runs at, after the stability clamp.
    Fixed stiffnessHz() const { return hz_; }

    /**
     * @brief Advance by a frame of `dtMicros`: as many 30 Hz ticks as are due
     *        (at most kMaxTicksPerUpdate). Asleep, it does nothing.
     * @return true when the drawn field changed (the group redraws).
     */
    bool tick(uint32_t dtMicros);

    /// Give the jelly at `at` (clamped into the box) the velocity `velocity`
    /// (px/s): its cell's four nodes take it so the field there moves at
    /// exactly `velocity`. Wakes the Lattice.
    void impulse(FixedVec2 at, FixedVec2 velocity);
    /// Start simulating again (a pose swap, a move). The sim clock starts half
    /// a tick in, so the next frame of ≥ 1/60 s steps.
    void wake();
    /// Put every node home and sleep. True if anything was displaced.
    bool snap();
    bool asleep() const { return asleep_; }

    /// The field's drawn displacement at `p` (group coordinates): nodes under
    /// ε count as home, so it is exactly zero once every node is.
    FixedVec2 displacementAt(FixedVec2 p) const;
    /// Node (i, j)'s displacement as simulated (no dead zone).
    FixedVec2 node(uint8_t i, uint8_t j) const;
    /// The field's velocity at `p` (px/s).
    FixedVec2 velocityAt(FixedVec2 p) const;
    /// Largest |component| of any node's displacement.
    Fixed maxDisplacement() const;

    /// Sub-steps simulated since configure() (a test counter).
    uint32_t steps() const { return steps_; }

private:
    static constexpr uint8_t kMaxNodes = kMaxSide * kMaxSide;

    bool      stepTick();
    bool      subStep();
    FixedVec2 shown(FixedVec2 d) const;
    FixedVec2 sample(const FixedVec2* field, FixedVec2 p) const;
    int       index(int i, int j) const { return j * nx_ + i; }

    uint8_t   nx_ = 0, ny_ = 0;
    FixedVec2 min_{}, max_{};
    Fixed     invCw_{}, invCh_{};
    Fixed     hz_{};
    LatticeTuning tuning_{};

    // Per sub-step coefficients (already × h): home, structural, shear.
    Fixed khHome_{}, chHome_{}, khAxis_{}, chAxis_{};
    Fixed kxx_{}, kxy_{}, kyy_{}, cxx_{}, cxy_{}, cyy_{};

    FixedVec2 d_[kMaxNodes] = {};  ///< Displacement from home, px.
    FixedVec2 v_[kMaxNodes] = {};  ///< Velocity, px/s.
    FixedVec2 s_[kMaxNodes] = {};  ///< Drawn displacement: d_ with the ε dead zone.

    uint64_t acc_ = 0;     ///< µs × kTickHz; a tick is due at 1e6.
    uint32_t steps_ = 0;
    uint8_t  still_ = 0;
    bool     asleep_ = true;
};

} // namespace shape
} // namespace enjin2
