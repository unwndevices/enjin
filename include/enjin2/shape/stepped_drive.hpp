/**
 * @file stepped_drive.hpp
 * @brief A stepped drive: a periodic sequence of rest-pose transforms a Shape
 *        group steps through (Tomodachi #335, the breath channel of #316)
 *
 * Life on a settled jelly at almost no cost: instead of springs moving every
 * frame, the group's whole rest pose steps through N transforms per period,
 * each a squash-stretch about a pivot plus an offset (DriveStep). The group
 * applies the current step to its items' rest positions *before* the Lattice
 * displaces them, so a step moves the Lattice's homes and its (sleeping) nodes
 * together: the group redraws once per step change and the sim stays asleep.
 *
 * The squash-stretch is given as how much a reference box (`extent`, e.g. the
 * Body's) grows, in px: p → pivot + d + d·grow / extent + offset, d = p − pivot,
 * in exact integer ratios (rounded to nearest). A box edge `extent` (or half
 * of it) from the pivot therefore moves by exactly `grow` (or half of it), so
 * whole-pixel steps stay whole: no edge pixel is lost to a rounded scale.
 *
 * Whoever makes the steps chooses them (the pack layer's breathing quantises
 * them so the Body's box changes by whole pixels). tick() advances the drive's
 * own clock (µs, integer) and reports a change only when the step it lands on
 * draws differently from the one before, so equal neighbouring steps cost
 * nothing.
 */
#pragma once

#include <cstdint>

#include "../core/fixed.hpp"

namespace enjin2 {
namespace shape {

/// One step: the reference box grown by `grow` px about the pivot, then moved by `offset`.
struct DriveStep {
    FixedVec2 grow{};
    FixedVec2 offset{};

    bool operator==(const DriveStep& o) const { return grow == o.grow && offset == o.offset; }
    bool operator!=(const DriveStep& o) const { return !(*this == o); }
    bool identity() const { return *this == DriveStep{}; }
};

class SteppedDrive {
public:
    static constexpr uint8_t kMinSteps = 2;
    static constexpr uint8_t kMaxSteps = 16;

    /**
     * @brief Step through `steps[0..count)` once per `periodMicros`, growing
     *        the box `extent` (px, both > 0) about `pivot`, from step 0 now.
     * @return false (and the drive stopped) for a count outside 2..16, a zero
     *         period or an empty extent.
     */
    bool start(const DriveStep* steps, uint8_t count, uint32_t periodMicros, FixedVec2 pivot, FixedVec2 extent)
    {
        stop();
        if (!steps || count < kMinSteps || count > kMaxSteps || periodMicros == 0 || extent.x.raw <= 0 ||
            extent.y.raw <= 0)
            return false;
        for (uint8_t i = 0; i < count; ++i)
            steps_[i] = steps[i];
        count_ = count;
        period_ = periodMicros;
        pivot_ = pivot;
        extent_ = extent;
        return true;
    }
    /// Back to the rest pose (identity), not running.
    void stop()
    {
        count_ = 0;
        period_ = 0;
        clock_ = 0;
        step_ = 0;
    }
    bool running() const { return count_ != 0; }

    /// Advance by a frame of `dtMicros`; true when the current step draws
    /// differently from the one before.
    bool tick(uint32_t dtMicros)
    {
        if (!running())
            return false;
        const DriveStep before = current();
        clock_ = uint32_t((uint64_t(clock_) + dtMicros) % period_);
        step_ = uint8_t(uint64_t(clock_) * count_ / period_);
        return current() != before;
    }

    /// The step drawn now (identity when stopped).
    DriveStep current() const { return running() ? steps_[step_] : DriveStep{}; }
    uint8_t step() const { return step_; }
    uint8_t count() const { return count_; }
    FixedVec2 pivot() const { return pivot_; }

    /// `p` (group coordinates, at rest) moved by the current step.
    FixedVec2 apply(FixedVec2 p) const
    {
        if (!running() || steps_[step_].identity())
            return p;
        const DriveStep& s = steps_[step_];
        return {p.x + along(p.x - pivot_.x, s.grow.x, extent_.x) + s.offset.x,
                p.y + along(p.y - pivot_.y, s.grow.y, extent_.y) + s.offset.y};
    }

private:
    // d · grow / extent, rounded to nearest (half away from zero).
    static Fixed along(Fixed d, Fixed grow, Fixed extent)
    {
        const int64_t n = int64_t(d.raw) * grow.raw, e = extent.raw;
        return Fixed::fromRaw(int32_t(n >= 0 ? (n + e / 2) / e : -((-n + e / 2) / e)));
    }

    DriveStep steps_[kMaxSteps];
    FixedVec2 pivot_{}, extent_{};
    uint32_t  period_ = 0;
    uint32_t  clock_ = 0;  ///< µs into the period.
    uint8_t   count_ = 0;
    uint8_t   step_ = 0;
};

} // namespace shape
} // namespace enjin2
