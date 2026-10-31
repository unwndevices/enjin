/**
 * @file shape_feel.hpp
 * @brief The Feel: how the pet answers a finger (ADR-0019, Tomodachi #365)
 *
 * One pointer a frame, in layer pixels, and the pet decides what the finger
 * meant. The machine is the pet demo's, made the group's (glossary › Feel):
 *
 * - **A touch down on the pet takes hold of it** (one grab, at the point;
 *   nothing drawn there and the touch is ignored until it lifts). While it is
 *   held, the held spot follows the finger (a pull) and the finger's velocity
 *   is smoothed over frames (half the old, half the new).
 * - **Past `loose` px of stretch from where it landed, the pet comes loose**
 *   and follows the finger (a drag): the feet line is carried to the finger
 *   less the offset it landed with, within `bounds`; the pull goes on.
 * - **Letting go**: a drag is thrown at the finger's velocity (clamped to
 *   `maxThrow`) and the caller hears `Throw` with it (a fall, a floor and a
 *   landing are the caller's); a short, still touch (within `tapSlop` px and
 *   `tapMicros`) is a tap, poked straight down at `poke` px/s with a sideways
 *   kick away from the feet; anything else just lets go at the velocity.
 *
 * The machine is pure over a PetActions (what it does to the pet) and a clock
 * (the group's, in µs), so Enjin's own tests drive it with a fake; the group
 * implements the actions and is the only caller in production.
 */
#pragma once

#include <cstdint>

#include "../core/fixed.hpp"

namespace enjin2 {
namespace shape {

/// The Feel's tunables: the pet demo's values, tuned on the device (#336).
struct Feel {
    Fixed     tapSlop   = Fixed::fromInt(3);    ///< px a tap may move.
    uint32_t  tapMicros = 400000;               ///< The longest a tap may last.
    Fixed     loose     = Fixed::fromInt(12);   ///< px of stretch before a pull comes loose into a drag.
    Fixed     maxThrow  = Fixed::fromInt(600);  ///< px/s: the fastest a release throws.
    Fixed     poke      = Fixed::fromInt(300);  ///< px/s a tap pokes the jelly with, downward.
    FixedVec2 boundsMin = {Fixed::fromInt(24), Fixed::fromInt(0)};    ///< Where a drag may carry the feet line:
    FixedVec2 boundsMax = {Fixed::fromInt(136), Fixed::fromInt(160)}; ///< the layer, 24 px off its sides.
};

enum class PetEventKind : uint8_t { None, Tap, Drag, Throw };

/// What a pointer sample decided. `velocity` goes with Throw (px/s).
struct PetEvent {
    PetEventKind kind = PetEventKind::None;
    FixedVec2    velocity{};
};

/// What the machine does to the pet (an internal seam: the group is the one
/// adapter in production, tests fake it).
class PetActions {
public:
    virtual ~PetActions() = default;
    /// Take hold of the pet at `at` (one grab; lets go of any other). False
    /// when nothing of it is drawn there.
    virtual bool hold(FixedVec2 at) = 0;
    /// Pull the held spot toward `target`.
    virtual void pullTo(FixedVec2 target) = 0;
    /// Let go of the held spot at `velocity` px/s.
    virtual void letGo(FixedVec2 velocity) = 0;
    /// Carry the feet line toward `feet` (a drag).
    virtual void carryTo(FixedVec2 feet) = 0;
    /// Push the jelly at `at` to `velocity` px/s.
    virtual void poke(FixedVec2 at, FixedVec2 velocity) = 0;
    /// Where the feet line is.
    virtual FixedVec2 feet() const = 0;
};

class PetPointer {
public:
    /// One pointer sample at `now` µs: what it meant.
    PetEvent sample(FixedVec2 at, bool down, uint64_t now, const Feel& feel, PetActions& pet)
    {
        PetEvent ev;
        switch (phase_) {
            case Phase::Idle:
                if (!down) break;
                if (!pet.hold(at)) {
                    phase_ = Phase::Off;
                    break;
                }
                phase_ = Phase::Held;
                at0_ = last_ = at;
                offset_ = at - pet.feet();
                vel_ = {};
                downAt_ = lastAt_ = now;
                movedSq_ = 0;
                break;
            case Phase::Off:
                if (!down) phase_ = Phase::Idle;
                break;
            case Phase::Held:
            case Phase::Drag:
                if (down) {
                    const uint64_t dt = now - lastAt_;
                    if (dt > 0) {
                        vel_.x = smooth(vel_.x, at.x - last_.x, dt);
                        vel_.y = smooth(vel_.y, at.y - last_.y, dt);
                    }
                    last_ = at;
                    lastAt_ = now;
                    const int64_t dx = int64_t(at.x.raw) - at0_.x.raw, dy = int64_t(at.y.raw) - at0_.y.raw;
                    const int64_t sq = dx * dx + dy * dy;
                    if (sq > movedSq_) movedSq_ = sq;
                    if (phase_ == Phase::Held && sq > int64_t(feel.loose.raw) * feel.loose.raw) {
                        phase_ = Phase::Drag;
                        ev.kind = PetEventKind::Drag;
                    }
                    if (phase_ == Phase::Drag) pet.carryTo(clamp(at - offset_, feel));
                    pet.pullTo(at);
                    break;
                }
                {
                    const FixedVec2 v = {clampSpeed(vel_.x, feel.maxThrow), clampSpeed(vel_.y, feel.maxThrow)};
                    if (phase_ == Phase::Drag) {
                        pet.letGo(v);
                        ev.kind = PetEventKind::Throw;
                        ev.velocity = v;
                    } else if (movedSq_ <= int64_t(feel.tapSlop.raw) * feel.tapSlop.raw && now - downAt_ <= feel.tapMicros) {
                        pet.letGo({});
                        // Straight down, kicked sideways away from the feet.
                        const Fixed kick = Fixed::fromRaw(int32_t((int64_t(at0_.x.raw) - pet.feet().x.raw) * 8));
                        pet.poke(at0_, {kick, feel.poke});
                        ev.kind = PetEventKind::Tap;
                    } else {
                        pet.letGo(v);
                    }
                    phase_ = Phase::Idle;
                }
                break;
        }
        return ev;
    }

    /// Let go of what is held (a new Look); the finger does nothing more until it lifts.
    void release(PetActions& pet)
    {
        if (!holding()) return;
        pet.letGo({});
        phase_ = Phase::Off;
    }

    bool holding() const { return phase_ == Phase::Held || phase_ == Phase::Drag; }
    bool dragging() const { return phase_ == Phase::Drag; }

private:
    enum class Phase : uint8_t { Idle, Off, Held, Drag };

    // Half the old velocity, half the finger's over this frame (px/s).
    static Fixed smooth(Fixed v, Fixed d, uint64_t dtMicros)
    {
        const int64_t now = int64_t(d.raw) * 1000000 / int64_t(dtMicros);
        return Fixed::fromRaw(int32_t((int64_t(v.raw) + now) / 2));
    }
    static Fixed clampSpeed(Fixed v, Fixed max)
    {
        return v > max ? max : v < -max ? -max : v;
    }
    static FixedVec2 clamp(FixedVec2 p, const Feel& f)
    {
        if (p.x < f.boundsMin.x) p.x = f.boundsMin.x;
        if (p.x > f.boundsMax.x) p.x = f.boundsMax.x;
        if (p.y < f.boundsMin.y) p.y = f.boundsMin.y;
        if (p.y > f.boundsMax.y) p.y = f.boundsMax.y;
        return p;
    }

    Phase     phase_ = Phase::Idle;
    FixedVec2 at0_{}, offset_{}, last_{}, vel_{};
    uint64_t  downAt_ = 0, lastAt_ = 0;
    int64_t   movedSq_ = 0;  ///< The farthest the finger got from where it landed, squared (raw²).
};

} // namespace shape
} // namespace enjin2
