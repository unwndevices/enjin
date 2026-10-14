// The Lattice deformer (ADR-0016, Tomodachi #334). See lattice.hpp.
//
// Integer arithmetic only (Fixed and int64): the S3 object must carry no FP
// instructions or soft-float calls, so the device, WASM and native tests step
// the same bits.

#include "enjin2/shape/lattice.hpp"

#include "enjin2/core/math.hpp"
#include <cstdint>

namespace enjin2 {
namespace shape {

namespace {

constexpr uint64_t kTickUnit = 1000000;  // µs × Hz per tick
constexpr int32_t  kSubHz = int32_t(Lattice::kTickHz) * Lattice::kSubSteps;  // 1/h = 120
constexpr Fixed    kTwoPi = Fixed::fromRaw(411775);                       // 2π
constexpr Fixed    kHomeW = Fixed::fromRatio(45, 100);    // home springs: 0.45 w
constexpr Fixed    kAxisW = Fixed::fromRatio(120, 100);   // structural springs: 1.2 w
constexpr Fixed    kShear = Fixed::fromRatio(70, 100);    // shear springs: 0.7 of structural
constexpr Fixed    kStableLimit = Fixed::fromRatio(36, 10);  // h²ω² + 2hc ≤ 3.6 (< 4)

// x × h, h = 1 / 120 s.
constexpr Fixed perSub(Fixed x) { return Fixed::fromRaw(x.raw / kSubHz); }
// v × h for a position step, rounded to nearest (half away from zero):
// truncating would lose up to a whole raw unit per sub-step and slow the tail.
constexpr int32_t timesH(int32_t raw)
{
    return raw >= 0 ? (raw + kSubHz / 2) / kSubHz : -((-raw + kSubHz / 2) / kSubHz);
}
// Exact Q32.32 product of two Fixed.
constexpr int64_t mul(Fixed a, Fixed b) { return int64_t(a.raw) * b.raw; }
// Q32.32 → Fixed, rounded to nearest (no drift toward −∞).
constexpr int32_t toRaw(int64_t q32) { return int32_t((q32 + 0x8000) >> 16); }
constexpr Fixed absF(Fixed f) { return f.raw < 0 ? -f : f; }
constexpr Fixed minF(Fixed a, Fixed b) { return a < b ? a : b; }
constexpr Fixed maxF(Fixed a, Fixed b) { return a < b ? b : a; }

// Integer square root of a non-negative int64 (floor).
uint64_t isqrt(uint64_t v)
{
    uint64_t r = 0, bit = uint64_t(1) << 62;
    while (bit > v)
        bit >>= 2;
    while (bit)
    {
        if (v >= r + bit)
        {
            v -= r + bit;
            r = (r >> 1) + bit;
        }
        else
            r >>= 1;
        bit >>= 2;
    }
    return r;
}

// Radians (Fixed) → the TrigLUT's 65536ths of a turn: × 65536 / 2π.
uint16_t turn16(Fixed radians)
{
    return uint16_t(int32_t((int64_t(radians.raw) * 683565276LL) >> 32));
}

constexpr Fixed    kSwayAccMax = Fixed::fromInt(4000);  // px/s², the anchor's acceleration clamp
constexpr Fixed    kSwayMinArm = Fixed::fromInt(3);     // px, the shortest lever
constexpr Fixed    kOneRad = Fixed::fromInt(1);         // the angle clamp
constexpr Fixed    kPullHz = Fixed::fromInt(5);         // the arm pull spring
constexpr Fixed    kPullZeta = Fixed::fromRatio(70, 100);
constexpr Fixed    kMaxThrowRad = Fixed::fromInt(30);   // rad/s, a thrown arm's spin

// reach · tanh(L / reach) for L, reach in raw Fixed units: Padé (3, 2),
// x(27 + x²) / (27 + 9x²), which is exactly 1 at x = 3 and held there.
int64_t rubbery(int64_t L, int64_t reach)
{
    if (reach <= 0)
        return 0;
    const int64_t x = (L << 16) / reach;  // Q16
    if (x >= 3 * 65536)
        return reach;
    const int64_t x2 = (x * x) >> 16;
    const int64_t t = (x * (27 * 65536 + x2)) / (27 * 65536 + 9 * x2);  // Q16
    return (reach * t) >> 16;
}

// The worst mode's h²ω² + 2hc, for spring-Laplacian eigenvalue `lam`.
Fixed stabilityScore(Fixed hz, Fixed zeta, Fixed lam)
{
    const Fixed hw = perSub(kTwoPi * hz);  // h·w
    const Fixed omega2 = kHomeW * kHomeW + kAxisW * kAxisW * lam;  // (ω/w)²
    const Fixed damp = kHomeW + kAxisW * lam;                      // c / (2ζw)
    return hw * hw * omega2 + Fixed::fromInt(4) * zeta * hw * damp;
}

} // namespace

bool Lattice::configure(FixedVec2 min, FixedVec2 max, uint8_t nx, uint8_t ny, const LatticeTuning& tuning)
{
    // The frame (and a carry under way) outlives a new grid; grabs do not.
    const FixedVec2 pos = framePos_, vel = frameVel_, target = frameTarget_, shownFrame = frameShown_;
    const bool carrying = carrying_;
    const uint16_t serial = grabSerial_;
    *this = Lattice{};
    framePos_ = pos;
    frameVel_ = vel;
    frameTarget_ = target;
    frameShown_ = shownFrame;
    carrying_ = carrying;
    grabSerial_ = serial;
    if (nx < 2 || ny < 2 || nx > kMaxSide || ny > kMaxSide || !(min.x < max.x) || !(min.y < max.y))
        return false;
    const Fixed cw = Fixed::fromRaw(int32_t((int64_t(max.x.raw) - min.x.raw) / (nx - 1)));
    const Fixed ch = Fixed::fromRaw(int32_t((int64_t(max.y.raw) - min.y.raw) / (ny - 1)));
    // The cell diagonal's direction, squared terms (Q8.8 sides keep them in range).
    const int64_t ca = cw.raw >> 8, cb = ch.raw >> 8, l2 = ca * ca + cb * cb;
    if (ca <= 0 || cb <= 0)
        return false;
    const Fixed exx = Fixed::fromRaw(int32_t((ca * ca << 16) / l2));
    const Fixed eyy = Fixed::fromRaw(int32_t((cb * cb << 16) / l2));
    const Fixed exy = Fixed::fromRaw(int32_t((ca * cb << 16) / l2));

    nx_ = nx;
    ny_ = ny;
    min_ = min;
    max_ = max;
    tuning_ = tuning;
    invCw_ = Fixed::fromInt(1) / cw;
    invCh_ = Fixed::fromInt(1) / ch;
    cw_ = cw;
    ch_ = ch;

    const Fixed zeta = minF(maxF(tuning.damping, Fixed{}), Fixed::fromInt(2));
    Fixed hz = minF(maxF(tuning.stiffnessHz, Fixed{}), Fixed::fromInt(kMaxStiffnessHz));
    // λ ≤ 4 + 4·0.7·2·max(ex², ey²) on any grid (see the header).
    const Fixed lam = Fixed::fromInt(4) + Fixed::fromInt(4) * kShear * Fixed::fromInt(2) * maxF(exx, eyy);
    if (stabilityScore(hz, zeta, lam) > kStableLimit)
    {
        int32_t lo = 0, hi = hz.raw;
        while (hi - lo > 1)
        {
            const int32_t mid = lo + (hi - lo) / 2;
            if (stabilityScore(Fixed::fromRaw(mid), zeta, lam) > kStableLimit)
                hi = mid;
            else
                lo = mid;
        }
        hz = Fixed::fromRaw(lo);
    }
    hz_ = hz;

    const Fixed w = kTwoPi * hz, wh = w * kHomeW, ws = w * kAxisW;
    const Fixed twoZeta = zeta + zeta;
    khHome_ = perSub(wh * wh);
    chHome_ = perSub(twoZeta * wh);
    khAxis_ = perSub(ws * ws);
    chAxis_ = perSub(twoZeta * ws);
    const Fixed ks = khAxis_ * kShear, cs = chAxis_ * kShear;
    kxx_ = ks * exx;
    kxy_ = ks * exy;
    kyy_ = ks * eyy;
    cxx_ = cs * exx;
    cxy_ = cs * exy;
    cyy_ = cs * eyy;

    // Pull and carry (#336).
    const Fixed carryHz = minF(maxF(tuning.carryHz, Fixed{}), Fixed::fromInt(kMaxStiffnessHz));
    const Fixed carryZeta = minF(maxF(tuning.carryDamping, Fixed{}), Fixed::fromInt(2));
    const Fixed wc = kTwoPi * carryHz, wp = kTwoPi * kPullHz;
    kCarry_ = perSub(wc * wc);
    cCarry_ = perSub((carryZeta + carryZeta) * wc);
    kPull_ = perSub(wp * wp);
    cPull_ = perSub((kPullZeta + kPullZeta) * wp);
    if (carrying_)
        wake();
    return true;
}

bool Lattice::subStep(uint8_t s)
{
    const int n = nx_ * ny_;

    // The carry: the frame steps on its position spring; its velocity change
    // is an inertial push back on every free node (below).
    int32_t dvx = 0, dvy = 0;
    if (carrying_)
    {
        dvx = toRaw(mul(kCarry_, frameTarget_.x - framePos_.x) - mul(cCarry_, frameVel_.x));
        dvy = toRaw(mul(kCarry_, frameTarget_.y - framePos_.y) - mul(cCarry_, frameVel_.y));
        frameVel_.x.raw += dvx;
        frameVel_.y.raw += dvy;
        framePos_.x.raw += timesH(frameVel_.x.raw);
        framePos_.y.raw += timesH(frameVel_.y.raw);
    }

    // Held nodes move kinematically to their goal over the tick's sub-steps:
    // grabbed ones to their pull, and while anything pulls or carries the
    // pet, the feet (the bottom row) home, so they stay put.
    bool held[kMaxNodes] = {};
    FixedVec2 goal[kMaxNodes];
    bool holdFeet = carrying_;
    for (const Grab& g : grabs_)
        if (g.kind == GrabKind::Node)
        {
            held[g.index] = true;
            goal[g.index] = pullGoal(g.index, g.target);
            holdFeet = true;
        }
    const int32_t left = int32_t(kSubSteps - s);
    for (int k = 0; k < n; ++k)
    {
        if (!held[k])
        {
            if (!holdFeet || k < n - nx_)
                continue;
            held[k] = true;
            goal[k] = {};
        }
        const FixedVec2 next = {Fixed::fromRaw(d_[k].x.raw + (goal[k].x.raw - d_[k].x.raw) / left),
                                Fixed::fromRaw(d_[k].y.raw + (goal[k].y.raw - d_[k].y.raw) / left)};
        v_[k] = {Fixed::fromRaw((next.x.raw - d_[k].x.raw) * kSubHz), Fixed::fromRaw((next.y.raw - d_[k].y.raw) * kSubHz)};
        d_[k] = next;
    }

    int64_t f[2 * kMaxNodes];  // Q32.32 velocity change (force × h)
    for (int k = 0; k < n; ++k)
    {
        f[2 * k] = -mul(khHome_, d_[k].x) - mul(chHome_, v_[k].x);
        f[2 * k + 1] = -mul(khHome_, d_[k].y) - mul(chHome_, v_[k].y);
    }
    // Structural: the stretch along the row (x) or the column (y).
    auto axis = [&](int a, int b, int ax) {
        const Fixed dd = ax ? d_[b].y - d_[a].y : d_[b].x - d_[a].x;
        const Fixed dv = ax ? v_[b].y - v_[a].y : v_[b].x - v_[a].x;
        const int64_t s = mul(khAxis_, dd) + mul(chAxis_, dv);
        f[2 * a + ax] += s;
        f[2 * b + ax] -= s;
    };
    // Shear: the stretch along the cell diagonal a→b (sign −1: the anti-diagonal).
    auto shear = [&](int a, int b, bool anti) {
        const FixedVec2 dd = d_[b] - d_[a], dv = v_[b] - v_[a];
        const Fixed kxy = anti ? -kxy_ : kxy_, cxy = anti ? -cxy_ : cxy_;
        const int64_t fx = mul(kxx_, dd.x) + mul(kxy, dd.y) + mul(cxx_, dv.x) + mul(cxy, dv.y);
        const int64_t fy = mul(kxy, dd.x) + mul(kyy_, dd.y) + mul(cxy, dv.x) + mul(cyy_, dv.y);
        f[2 * a] += fx;
        f[2 * a + 1] += fy;
        f[2 * b] -= fx;
        f[2 * b + 1] -= fy;
    };
    for (int j = 0; j < ny_; ++j)
        for (int i = 0; i < nx_; ++i)
        {
            if (i + 1 < nx_)
                axis(index(i, j), index(i + 1, j), 0);
            if (j + 1 < ny_)
                axis(index(i, j), index(i, j + 1), 1);
            if (i + 1 < nx_ && j + 1 < ny_)
            {
                shear(index(i, j), index(i + 1, j + 1), false);
                shear(index(i + 1, j), index(i, j + 1), true);
            }
        }
    bool moved = false;
    for (int k = 0; k < n; ++k)
    {
        if (!held[k])
        {
            // The carry's inertial push: the frame sped up, the node did not.
            v_[k].x.raw += toRaw(f[2 * k]) - dvx;
            v_[k].y.raw += toRaw(f[2 * k + 1]) - dvy;
            d_[k].x.raw += timesH(v_[k].x.raw);
            d_[k].y.raw += timesH(v_[k].y.raw);
        }
        const FixedVec2 drawn = shown(d_[k]);
        moved = moved || drawn != s_[k];
        s_[k] = drawn;
    }
    ++steps_;
    return moved;
}

bool Lattice::stepTick()
{
    bool changed = false;
    const FixedVec2 before = framePos_;
    for (uint8_t s = 0; s < kSubSteps; ++s)
        changed = subStep(s) || changed;
    // The carry's acceleration over the tick, for the arms (px/s²).
    const FixedVec2 step = framePos_ - before;
    constexpr int32_t kTick2 = int32_t(kTickHz * kTickHz);
    const int64_t fax = (int64_t(step.x.raw) - frameStep_.x.raw) * kTick2;
    const int64_t fay = (int64_t(step.y.raw) - frameStep_.y.raw) * kTick2;
    frameStep_ = step;
    changed = stepSway(fax, fay) || changed;
    const Fixed eps = tuning_.sleepEpsilon;
    const Fixed vEps = Fixed::fromRaw(eps.raw * int32_t(kTickHz));  // ε per tick, in px/s
    if (carrying_)
    {
        // The frame is drawn on its target once within ε of it (no pop at the snap).
        const FixedVec2 off = frameTarget_ - framePos_;
        const FixedVec2 drawn = {absF(off.x) < eps ? frameTarget_.x : framePos_.x,
                                 absF(off.y) < eps ? frameTarget_.y : framePos_.y};
        changed = changed || drawn != frameShown_;
        frameShown_ = drawn;
    }
    bool quiet = !grabbed();
    if (carrying_)
    {
        const FixedVec2 off = frameTarget_ - framePos_;
        quiet = quiet && absF(off.x) < eps && absF(off.y) < eps && absF(frameVel_.x) < vEps &&
                absF(frameVel_.y) < vEps;
    }
    for (int k = 0, n = nx_ * ny_; k < n && quiet; ++k)
        quiet = absF(d_[k].x) < eps && absF(d_[k].y) < eps && absF(v_[k].x) < vEps && absF(v_[k].y) < vEps;
    for (uint8_t k = 0; k < armCount_ && quiet; ++k)
        quiet = absF(arms_[k].th) < arms_[k].eps && absF(arms_[k].om) < arms_[k].vEps;
    still_ = quiet ? uint8_t(still_ + 1) : uint8_t(0);
    if (quiet && still_ >= tuning_.sleepTicks)
        changed = snap() || changed;
    return changed;
}

bool Lattice::tick(uint32_t dtMicros)
{
    if (!configured() || asleep_)
        return false;
    acc_ += uint64_t(dtMicros) * kTickHz;
    bool changed = false;
    uint8_t ticks = 0;
    while (acc_ >= kTickUnit)
    {
        if (ticks == kMaxTicksPerUpdate)
        {
            acc_ %= kTickUnit;
            break;
        }
        acc_ -= kTickUnit;
        ++ticks;
        changed = stepTick() || changed;
        if (asleep_)
            break;
    }
    return changed;
}

void Lattice::wake()
{
    if (!configured())
        return;
    still_ = 0;
    if (asleep_)
    {
        asleep_ = false;
        // The sim clock starts half a tick in: a 60 Hz frame steps at once,
        // a 30 Hz frame steps once (not twice).
        acc_ = kTickUnit / 2;
    }
}

bool Lattice::snap()
{
    bool moved = false;
    for (int k = 0, n = nx_ * ny_; k < n; ++k)
    {
        moved = moved || s_[k] != FixedVec2{};
        d_[k] = {};
        s_[k] = {};
        v_[k] = {};
    }
    for (uint8_t k = 0; k < armCount_; ++k)
    {
        Arm& a = arms_[k];
        moved = moved || a.shown != Fixed{};
        a.th = a.om = a.shown = Fixed{};
        a.d1 = a.d2 = FixedVec2{};
    }
    for (Grab& g : grabs_)
        g.kind = GrabKind::Free;
    if (carrying_)
    {
        moved = moved || frameShown_ != frameTarget_;
        framePos_ = frameShown_ = frameTarget_;
        frameVel_ = {};
        carrying_ = false;
    }
    frameStep_ = {};
    asleep_ = true;
    still_ = 0;
    acc_ = 0;
    return moved;
}

void Lattice::impulse(FixedVec2 at, FixedVec2 velocity)
{
    if (!configured())
        return;
    at.x = minF(maxF(at.x, min_.x), max_.x);
    at.y = minF(maxF(at.y, min_.y), max_.y);
    const Fixed u = (at.x - min_.x) * invCw_, v = (at.y - min_.y) * invCh_;
    int i = u.floorToInt(), j = v.floorToInt();
    i = i < 0 ? 0 : i > nx_ - 2 ? nx_ - 2 : i;
    j = j < 0 ? 0 : j > ny_ - 2 ? ny_ - 2 : j;
    const Fixed one = Fixed::fromInt(1);
    const Fixed fu = minF(maxF(u - Fixed::fromInt(i), Fixed{}), one);
    const Fixed fv = minF(maxF(v - Fixed::fromInt(j), Fixed{}), one);
    const Fixed w[4] = {(one - fu) * (one - fv), fu * (one - fv), (one - fu) * fv, fu * fv};
    const int k[4] = {index(i, j), index(i + 1, j), index(i, j + 1), index(i + 1, j + 1)};
    // Minimum-norm split: Σ wᵢ·Δvᵢ = velocity with Δvᵢ = wᵢ·velocity / Σ w².
    const Fixed s = w[0] * w[0] + w[1] * w[1] + w[2] * w[2] + w[3] * w[3];
    if (s.raw <= 0)
        return;
    wake();
    for (int q = 0; q < 4; ++q)
    {
        v_[k[q]].x += (w[q] * velocity.x) / s;
        v_[k[q]].y += (w[q] * velocity.y) / s;
    }
}

int Lattice::addSway(FixedVec2 anchor, FixedVec2 arm, const SwaySpring& spring)
{
    if (!configured() || armCount_ >= kMaxSway)
        return -1;
    Arm a;
    a.anchor = anchor;
    const int64_t ax = arm.x.raw, ay = arm.y.raw;
    const Fixed len = maxF(Fixed::fromRaw(int32_t(isqrt(uint64_t(ax * ax + ay * ay)))), kSwayMinArm);
    a.dir = {arm.x / len, arm.y / len};
    a.invL = Fixed::fromInt(1) / len;
    a.eps = tuning_.sleepEpsilon / len;  // the tip within ε
    a.vEps = Fixed::fromRaw(a.eps.raw * int32_t(kTickHz));
    const Fixed hz = minF(maxF(spring.hz, Fixed{}), Fixed::fromInt(kMaxStiffnessHz));
    const Fixed zeta = minF(maxF(spring.damping, Fixed{}), Fixed::fromInt(2));
    const Fixed ws = kTwoPi * hz;
    a.kh = perSub(ws * ws);
    a.ch = perSub((zeta + zeta) * ws);
    arms_[armCount_] = a;
    return armCount_++;
}

bool Lattice::stepSway(int64_t frameAccX, int64_t frameAccY)
{
    bool moved = false;
    const int64_t lim = kSwayAccMax.raw;
    for (uint8_t k = 0; k < armCount_; ++k)
    {
        Arm& a = arms_[k];
        // The anchor's acceleration over the last tick (px/s²), with the
        // carried frame's, clamped.
        const FixedVec2 d = sample(d_, a.anchor);
        constexpr int64_t kTick2 = int64_t(kTickHz) * kTickHz;
        int64_t accX = (int64_t(d.x.raw) - 2 * int64_t(a.d1.x.raw) + a.d2.x.raw) * kTick2 + frameAccX;
        int64_t accY = (int64_t(d.y.raw) - 2 * int64_t(a.d1.y.raw) + a.d2.y.raw) * kTick2 + frameAccY;
        while (accX > lim * 64 || accX < -lim * 64 || accY > lim * 64 || accY < -lim * 64)
        {
            // Far past the clamp: shrink first so the squares stay in range.
            accX /= 64;
            accY /= 64;
        }
        const int64_t m = int64_t(isqrt(uint64_t(accX * accX + accY * accY)));
        if (m > lim)
        {
            accX = accX * lim / m;
            accY = accY * lim / m;
        }
        a.d2 = a.d1;
        a.d1 = d;
        const Fixed ax = Fixed::fromRaw(int32_t(accX)), ay = Fixed::fromRaw(int32_t(accY));
        // A pull: toward the target, seen from the anchor where it is now.
        int64_t relX = 0, relY = 0, relL = 0;
        for (const Grab& g : grabs_)
            if (g.kind == GrabKind::Arm && g.index == k)
            {
                relX = int64_t(g.target.x.raw) - framePos_.x.raw - a.anchor.x.raw - d.x.raw;
                relY = int64_t(g.target.y.raw) - framePos_.y.raw - a.anchor.y.raw - d.y.raw;
                relL = int64_t(isqrt(uint64_t(relX * relX + relY * relY)));
            }
        const bool pulled = relL >= 65536;  // a target under a pixel from the anchor pulls nowhere
        for (uint8_t s = 0; s < kSubSteps; ++s)
        {
            const uint16_t turn = turn16(a.th);
            const int64_t c = math::TrigLUT::cos16(turn), sn = math::TrigLUT::sin16(turn);
            const Fixed dx = Fixed::fromRaw(int32_t((a.dir.x.raw * c - a.dir.y.raw * sn) >> 15));
            const Fixed dy = Fixed::fromRaw(int32_t((a.dir.x.raw * sn + a.dir.y.raw * c) >> 15));
            // The pseudo-force −a on the tip, as a torque (rad/s²).
            const Fixed torque = tuning_.swayGain * ((dy * ax - dx * ay) * a.invL);
            int64_t f = -mul(a.kh, a.th) - mul(a.ch, a.om) + int64_t(torque.raw) * 65536 / kSubHz;
            if (pulled)
            {
                // The sine of the angle from the arm to the target (±1 past a right angle).
                Fixed sine = Fixed::fromRaw(int32_t((dx.raw * relY - dy.raw * relX) / relL));
                if (dx.raw * relX + dy.raw * relY < 0)
                    sine = Fixed::fromInt(sine.raw < 0 ? -1 : 1);
                f += mul(kPull_, sine) - mul(cPull_, a.om);
            }
            a.om.raw += toRaw(f);
            a.th.raw += timesH(a.om.raw);
            a.th = minF(maxF(a.th, -kOneRad), kOneRad);
        }
        const Fixed drawn = absF(a.th) < a.eps ? Fixed{} : a.th;
        moved = moved || drawn != a.shown;
        a.shown = drawn;
    }
    return moved;
}

FixedVec2 Lattice::turn(FixedVec2 v, Fixed radians)
{
    if (radians == Fixed{})
        return v;
    const uint16_t t = turn16(radians);
    const int64_t c = math::TrigLUT::cos16(t), s = math::TrigLUT::sin16(t);
    // Rounded to nearest: Q15 × raw, half away from zero.
    auto q15 = [](int64_t x) { return int32_t(x >= 0 ? (x + 16384) >> 15 : -((-x + 16384) >> 15)); };
    return {Fixed::fromRaw(q15(v.x.raw * c - v.y.raw * s)), Fixed::fromRaw(q15(v.x.raw * s + v.y.raw * c))};
}

Fixed Lattice::swayAngleAt(FixedVec2 anchor) const
{
    for (uint8_t k = 0; k < armCount_; ++k)
        if (arms_[k].anchor == anchor)
            return arms_[k].shown;
    return Fixed{};
}

FixedVec2 Lattice::shown(FixedVec2 d) const
{
    const Fixed eps = tuning_.sleepEpsilon;
    return {absF(d.x) < eps ? Fixed{} : d.x, absF(d.y) < eps ? Fixed{} : d.y};
}

FixedVec2 Lattice::sample(const FixedVec2* field, FixedVec2 p) const
{
    if (!configured())
        return {};
    const Fixed u = (p.x - min_.x) * invCw_, v = (p.y - min_.y) * invCh_;
    int i = u.floorToInt(), j = v.floorToInt();
    i = i < 0 ? 0 : i > nx_ - 2 ? nx_ - 2 : i;
    j = j < 0 ? 0 : j > ny_ - 2 ? ny_ - 2 : j;
    // Outside the box fu / fv run past 0..1: the edge cells extrapolate.
    const Fixed fu = u - Fixed::fromInt(i), fv = v - Fixed::fromInt(j);
    const FixedVec2 a = field[index(i, j)], b = field[index(i + 1, j)];
    const FixedVec2 c = field[index(i, j + 1)], d = field[index(i + 1, j + 1)];
    const Fixed fuv = fu * fv;
    return {a.x + fu * (b.x - a.x) + fv * (c.x - a.x) + fuv * (a.x - b.x - c.x + d.x),
            a.y + fu * (b.y - a.y) + fv * (c.y - a.y) + fuv * (a.y - b.y - c.y + d.y)};
}

FixedVec2 Lattice::displacementAt(FixedVec2 p) const { return sample(s_, p); }
FixedVec2 Lattice::velocityAt(FixedVec2 p) const { return sample(v_, p); }

FixedVec2 Lattice::node(uint8_t i, uint8_t j) const
{
    return i < nx_ && j < ny_ ? d_[index(i, j)] : FixedVec2{};
}

Fixed Lattice::maxDisplacement() const
{
    Fixed m{};
    for (int k = 0, n = nx_ * ny_; k < n; ++k)
        m = maxF(m, maxF(absF(d_[k].x), absF(d_[k].y)));
    return m;
}

// ---------------------------------------------------------------------------
// Pull (#336)
// ---------------------------------------------------------------------------

FixedVec2 Lattice::home(int k) const
{
    const int i = k % nx_, j = k / nx_;
    return {Fixed::fromRaw(min_.x.raw + cw_.raw * i), Fixed::fromRaw(min_.y.raw + ch_.raw * j)};
}

// Where held node `k` goes for `target`: its stretch from home, made rubbery.
FixedVec2 Lattice::pullGoal(uint8_t k, FixedVec2 target) const
{
    const FixedVec2 h = home(k);
    const int64_t rx = int64_t(target.x.raw) - framePos_.x.raw - h.x.raw;
    const int64_t ry = int64_t(target.y.raw) - framePos_.y.raw - h.y.raw;
    const int64_t L = int64_t(isqrt(uint64_t(rx * rx + ry * ry)));
    if (L == 0)
        return {};
    const int64_t Lc = rubbery(L, tuning_.pullReach.raw);
    return {Fixed::fromRaw(int32_t(rx * Lc / L)), Fixed::fromRaw(int32_t(ry * Lc / L))};
}

int Lattice::newGrab(GrabKind kind, uint8_t index, FixedVec2 target)
{
    for (uint8_t i = 0; i < kMaxGrabs; ++i)
    {
        Grab& g = grabs_[i];
        if (g.kind != GrabKind::Free)
            continue;
        grabSerial_ = uint16_t(grabSerial_ % 0x3FFF + 1);
        g.kind = kind;
        g.index = index;
        g.serial = grabSerial_;
        g.target = target;
        wake();
        return int(g.serial) << 2 | i;
    }
    return -1;
}

Lattice::Grab* Lattice::findGrab(int handle)
{
    if (handle < 0)
        return nullptr;
    Grab& g = grabs_[handle & 3];
    return g.kind != GrabKind::Free && g.serial == uint16_t(handle >> 2) ? &g : nullptr;
}

int Lattice::grabNode(FixedVec2 at)
{
    if (!configured())
        return -1;
    const FixedVec2 local = at - framePos_;
    int best = 0;
    int64_t bestD = INT64_MAX;
    for (int k = 0, n = nx_ * ny_; k < n; ++k)
    {
        const FixedVec2 p = home(k) + s_[k];
        const int64_t dx = int64_t(local.x.raw) - p.x.raw, dy = int64_t(local.y.raw) - p.y.raw;
        const int64_t d2 = dx * dx + dy * dy;
        if (d2 < bestD)
        {
            bestD = d2;
            best = k;
        }
    }
    // It heads for where it is: grabbing alone moves nothing.
    return newGrab(GrabKind::Node, uint8_t(best), framePos_ + home(best) + d_[best]);
}

int Lattice::grabSway(uint8_t arm)
{
    if (!configured() || arm >= armCount_)
        return -1;
    const Arm& a = arms_[arm];
    const FixedVec2 tip = framePos_ + a.anchor + sample(d_, a.anchor) + turn(a.dir, a.th);
    return newGrab(GrabKind::Arm, arm, tip);
}

void Lattice::moveGrab(int handle, FixedVec2 target)
{
    Grab* g = findGrab(handle);
    if (!g || g->target == target)
        return;
    g->target = target;
    wake();
}

void Lattice::releaseGrab(int handle, FixedVec2 velocity)
{
    Grab* g = findGrab(handle);
    if (!g)
        return;
    if (g->kind == GrabKind::Node)
        v_[g->index] = velocity;
    else
    {
        // The tip's speed across the arm, as spin.
        Arm& a = arms_[g->index];
        const FixedVec2 d = turn(a.dir, a.th);
        const Fixed across = Fixed::fromRaw(int32_t((int64_t(d.x.raw) * velocity.y.raw - int64_t(d.y.raw) * velocity.x.raw) >> 16));
        a.om = minF(maxF(across * a.invL, -kMaxThrowRad), kMaxThrowRad);
    }
    g->kind = GrabKind::Free;
    wake();
}

uint8_t Lattice::grabCount() const
{
    uint8_t n = 0;
    for (const Grab& g : grabs_)
        n = uint8_t(n + (g.kind != GrabKind::Free));
    return n;
}

int Lattice::swayIndexAt(FixedVec2 anchor) const
{
    for (uint8_t k = 0; k < armCount_; ++k)
        if (arms_[k].anchor == anchor)
            return k;
    return -1;
}

// ---------------------------------------------------------------------------
// The frame and the carry (#336)
// ---------------------------------------------------------------------------

void Lattice::setFrame(FixedVec2 at)
{
    framePos_ = frameTarget_ = frameShown_ = at;
    frameVel_ = frameStep_ = {};
    carrying_ = false;
}

void Lattice::carryTo(FixedVec2 target)
{
    if (!configured())
    {
        setFrame(target);
        return;
    }
    // The same target again (moveTo every frame) is nothing new: the settle
    // count keeps running.
    if (carrying_ ? target == frameTarget_ : target == framePos_)
        return;
    frameTarget_ = target;
    carrying_ = true;
    wake();
}

} // namespace shape
} // namespace enjin2
