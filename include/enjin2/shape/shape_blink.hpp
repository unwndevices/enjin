/**
 * @file shape_blink.hpp
 * @brief Blinks: the pet shutting its eyes on its own now and then (ADR-0033, Tomodachi #365)
 *
 * The pet demo's scheduler, made the group's (glossary › Blink): a go every
 * `everyMin..everyMax` µs, each blink `shut` µs long, and `twicePermille` of
 * the goes are two blinks `gap` µs apart. The randomness is a seeded
 * xorshift32 of its own, so a run is the same run on every host and a test
 * can count closed-eye frames; a seed of 0 asks the group for one.
 *
 * Pure over a clock (the group's, in µs): tick() says when the shut state
 * changed, and the group shows the eyes' `closed` pose while shut() is true.
 * Off until started, so a still pet (and the Shape corpus) stays still.
 */
#pragma once

#include <cstdint>

namespace enjin2 {
namespace shape {

struct Blink {
    uint32_t everyMin = 2000000;  ///< µs between goes, at least.
    uint32_t everyMax = 6000000;  ///< µs between goes, at most.
    uint32_t shut = 120000;       ///< µs the eyes stay shut.
    uint32_t gap = 180000;        ///< µs between the two blinks of a double.
    uint16_t twicePermille = 300; ///< How often a go is a double (0..1000).
    uint32_t seed = 0;            ///< 0: the group picks one.
};

class BlinkScheduler {
public:
    /// Start blinking at `now`; the first go comes after everyMin..everyMax.
    void start(const Blink& b, uint64_t now)
    {
        spec_ = b;
        state_ = b.seed ? b.seed : 0x9E3779B9u;
        on_ = true;
        shut_ = false;
        end_ = 0;
        schedule(now);
    }
    /// Stop; the eyes open at once.
    void stop()
    {
        on_ = false;
        shut_ = false;
    }
    bool enabled() const { return on_; }
    bool shut() const { return shut_; }
    const Blink& spec() const { return spec_; }

    /// Advance to `now`: true when shut() changed.
    bool tick(uint64_t now)
    {
        if (!on_) return false;
        const bool was = shut_;
        if (now >= next_ && now >= end_) {
            end_ = now + spec_.shut;
            --left_;
            if (left_ > 0)
                next_ = end_ + spec_.gap;
            else
                schedule(now);
        }
        shut_ = now < end_;
        return shut_ != was;
    }

private:
    uint32_t rnd()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    /// A draw in [lo, hi].
    uint32_t between(uint32_t lo, uint32_t hi)
    {
        if (hi <= lo) return lo;
        return lo + uint32_t((uint64_t(rnd()) * (uint64_t(hi - lo) + 1)) >> 32);
    }
    void schedule(uint64_t now)
    {
        next_ = now + between(spec_.everyMin, spec_.everyMax);
        left_ = (rnd() % 1000) < spec_.twicePermille ? 2 : 1;
    }

    Blink    spec_;
    bool     on_ = false, shut_ = false;
    uint64_t next_ = 0, end_ = 0;
    uint8_t  left_ = 0;
    uint32_t state_ = 1;
};

} // namespace shape
} // namespace enjin2
