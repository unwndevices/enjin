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
 *
 * ## Sway arms (Tomodachi #335)
 *
 * A `sway` Feature is carried at its anchor like a `rigid` one, then swings
 * about the anchor on a damped angle spring of its own (hz, ζ from the pack;
 * 2.4 Hz, ζ 0.27 by default), driven by the anchor's acceleration: the
 * prototype's secondary sway (#314), without its field-rotation term. Each
 * tick, after the grid's sub-steps, an arm takes the second difference of the
 * field's displacement at its anchor (clamped to 4000 px/s²) and steps its
 * angle 4 times: θ'' = −ws²θ − 2ζ·ws·θ' + gain·(d × −a)/L, with d the arm's
 * direction turned by θ (the 1024-step TrigLUT) and L its length (≥ 3 px);
 * θ stays within ±1 rad. Arms take part in sleep: the Lattice sleeps only
 * when every arm's tip is also within ε (|θ| < ε/L, |θ'| < ε/L per tick), and
 * the snap puts the arms home too. The drawn angle has the same dead zone as
 * the nodes, so the snap never pops a swinging Feature either.
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
 * `Sway` is `Rigid` plus an angle spring of its own about the anchor.
 */
enum class Binding : uint8_t { Field = 0, Rigid = 1, Sway = 2 };

/// A sway arm's own spring (the pack's sway fields; 2.4 Hz, ζ 0.27 by default).
struct SwaySpring {
    Fixed hz      = Fixed::fromRatio(24, 10);   ///< 0..8 Hz (clamped).
    Fixed damping = Fixed::fromRatio(27, 100);  ///< ζ, 0..2 (clamped).
};

/// The Lattice's feel and its sleep test.
struct LatticeTuning {
    Fixed   stiffnessHz  = Fixed::fromRatio(32, 10);   ///< 3.2 Hz (clamped, see the file comment).
    Fixed   damping      = Fixed::fromRatio(55, 100);  ///< ζ, 0..2.
    Fixed   sleepEpsilon = Fixed::fromRatio(1, 16);    ///< px, and px per tick for the speed.
    uint8_t sleepTicks   = 6;                          ///< Ticks under ε before the snap.
    Fixed   swayGain     = Fixed::fromRatio(35, 100);  ///< How hard the anchor's acceleration swings an arm.
};

class Lattice {
public:
    static constexpr uint8_t  kMaxSide           = 8;  ///< Nodes per side, at most.
    static constexpr uint8_t  kDefaultSide       = 5;
    static constexpr uint32_t kTickHz            = 30;
    static constexpr uint8_t  kSubSteps          = 4;
    static constexpr uint8_t  kMaxTicksPerUpdate = 4;  ///< A longer frame drops the rest.
    static constexpr int32_t  kMaxStiffnessHz    = 8;
    static constexpr uint8_t  kMaxSway           = 16; ///< Sway arms per Lattice.

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

    /**
     * @brief Hang a sway arm at `anchor` (group coordinates). `arm` runs from
     *        the anchor to the swinging Feature's centre; its length is the
     *        lever. The arm starts at rest. configure() drops every arm.
     * @return the arm's index, or -1 when unconfigured or full.
     */
    int addSway(FixedVec2 anchor, FixedVec2 arm, const SwaySpring& spring = SwaySpring{});
    void clearSway() { armCount_ = 0; }
    uint8_t swayCount() const { return armCount_; }
    /// Arm k's angle as simulated (radians; + turns x toward y, clockwise on screen).
    Fixed swayAngle(uint8_t k) const { return k < armCount_ ? arms_[k].th : Fixed{}; }
    /// The drawn angle of the arm hung at exactly `anchor` (dead zone as for
    /// the nodes); zero when no arm hangs there.
    Fixed swayAngleAt(FixedVec2 anchor) const;
    /// `v` turned by `radians` (TrigLUT, integer only): how a sway Feature's
    /// points turn about its anchor.
    static FixedVec2 turn(FixedVec2 v, Fixed radians);

    /// Sub-steps simulated since configure() (a test counter).
    uint32_t steps() const { return steps_; }

private:
    static constexpr uint8_t kMaxNodes = kMaxSide * kMaxSide;

    struct Arm {
        FixedVec2 anchor{}, dir{};  ///< dir = arm / L.
        Fixed     invL{}, eps{}, vEps{};
        Fixed     kh{}, ch{};       ///< ws²·h and 2ζ·ws·h.
        Fixed     th{}, om{}, shown{};
        FixedVec2 d1{}, d2{};       ///< The anchor's displacement one and two ticks ago.
    };

    bool      stepTick();
    bool      stepSway();
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

    Arm     arms_[kMaxSway] = {};
    uint8_t armCount_ = 0;

    uint64_t acc_ = 0;     ///< µs × kTickHz; a tick is due at 1e6.
    uint32_t steps_ = 0;
    uint8_t  still_ = 0;
    bool     asleep_ = true;
};

} // namespace shape
} // namespace enjin2
