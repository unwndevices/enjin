// Lattice deformer (Tomodachi #334, ADR-0016): the N×M spring grid a Shape
// group is bent by, on a fixed 30 Hz tick with 4 sub-steps, that sleeps and
// snaps to rest.

#include <enjin2/shape/lattice.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace enjin2;
using namespace enjin2::shape;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

static FixedVec2 px(int x, int y) { return {Fixed::fromInt(x), Fixed::fromInt(y)}; }

// An 80×80 box, the Star's size: cells of 20 px on the default 5×5 grid.
static Lattice makeLattice() {
    Lattice l;
    l.configure(px(-40, -60), px(40, 20));
    return l;
}

static void testRestIsAsleep() {
    Lattice l = makeLattice();
    ASSERT(l.asleep(), "a configured Lattice starts asleep, at rest");
    for (int i = 0; i < 30; ++i)
        ASSERT(!l.tick(33333), "a resting Lattice never changes");
    ASSERT(l.steps() == 0, "asleep: zero sim steps");
    ASSERT(l.displacementAt(px(0, -20)) == FixedVec2{}, "rest: no displacement inside");
    ASSERT(l.displacementAt(px(70, 50)) == FixedVec2{}, "rest: no displacement outside the box either");
}

static double toPx(Fixed f) { return f.raw / 65536.0; }

static void testImpulseMovesTheJellyThere() {
    // On a node, mid-cell and off-centre: the field there takes exactly v.
    const FixedVec2 points[] = {px(0, -20), px(10, -50), px(-33, 7)};
    for (FixedVec2 at : points) {
        Lattice l = makeLattice();
        l.impulse(at, px(120, -60));
        ASSERT(!l.asleep(), "an impulse wakes the Lattice");
        const FixedVec2 v = l.velocityAt(at);
        ASSERT(std::fabs(toPx(v.x) - 120) < 0.05 && std::fabs(toPx(v.y) + 60) < 0.05,
               "the jelly at the impulse point moves at the impulse velocity");
        ASSERT(l.velocityAt(px(-40, 20)) == FixedVec2{} || at == points[2], "a far corner is untouched");
    }
    Lattice l = makeLattice();
    l.impulse(px(0, -20), px(0, 300));
    ASSERT(l.tick(33334), "a woken Lattice steps on the next frame and its field moves");
    ASSERT(l.steps() == Lattice::kSubSteps, "one 30 Hz tick = 4 sub-steps");
    const FixedVec2 d = l.displacementAt(px(0, -20));
    ASSERT(d.y.raw > 0 && std::abs(d.x.raw) < 64, "it moves the way it was pushed");
    ASSERT(l.displacementAt(px(-40, -60)) == FixedVec2{} || l.displacementAt(px(-40, -60)).y.raw >= 0,
           "a corner two cells away has not moved against the push");
}

// The prototype's makeLattice().step() (#314, look-and-feel.html @ e105b7d),
// in double, with its nonlinear springs: the reference the Fixed sim is held to.
struct ProtoLattice {
    static constexpr int nx = 5, ny = 5, n = nx * ny;
    double rx[n], ry[n], x[n][2], v[n][2];
    struct Spring { int a, b; double kf; };
    Spring springs[4 * n];
    int ns = 0;
    double hz = 3.2, z = 0.55;

    ProtoLattice(double x0, double y0, double cw, double ch) {
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const int k = j * nx + i;
                rx[k] = x0 + i * cw;
                ry[k] = y0 + j * ch;
                x[k][0] = rx[k];
                x[k][1] = ry[k];
                v[k][0] = v[k][1] = 0;
            }
        auto id = [](int i, int j) { return j * nx + i; };
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                if (i + 1 < nx) springs[ns++] = {id(i, j), id(i + 1, j), 1};
                if (j + 1 < ny) springs[ns++] = {id(i, j), id(i, j + 1), 1};
                if (i + 1 < nx && j + 1 < ny) {
                    springs[ns++] = {id(i, j), id(i + 1, j + 1), 0.7};
                    springs[ns++] = {id(i + 1, j), id(i, j + 1), 0.7};
                }
            }
    }
    void step(double h) {
        const double w = 2 * M_PI * hz, ws = w * 1.2, wh = w * 0.45;
        double acc[n][2];
        for (int k = 0; k < n; ++k)
            for (int a = 0; a < 2; ++a)
                acc[k][a] = wh * wh * ((a ? ry[k] : rx[k]) - x[k][a]) - 2 * z * wh * v[k][a];
        for (int s = 0; s < ns; ++s) {
            const int a = springs[s].a, b = springs[s].b;
            const double L0 = std::hypot(rx[b] - rx[a], ry[b] - ry[a]);
            const double dx = x[b][0] - x[a][0], dy = x[b][1] - x[a][1], l = std::hypot(dx, dy);
            const double ex = dx / l, ey = dy / l;
            const double relv = (v[b][0] - v[a][0]) * ex + (v[b][1] - v[a][1]) * ey;
            const double F = springs[s].kf * (ws * ws * (l - L0) + 2 * z * ws * relv);
            acc[a][0] += F * ex;
            acc[a][1] += F * ey;
            acc[b][0] -= F * ex;
            acc[b][1] -= F * ey;
        }
        for (int k = 0; k < n; ++k)
            for (int a = 0; a < 2; ++a) {
                v[k][a] += acc[k][a] * h;
                x[k][a] += v[k][a] * h;
            }
    }
};

static void testMatchesThePrototypeLattice() {
    // A small kick on node (2, 1), so the linearised springs and the
    // prototype's agree to well under a pixel.
    Lattice l = makeLattice();
    ProtoLattice p(-40, -60, 20, 20);
    l.impulse(px(0, -40), px(60, 100));
    p.v[7][0] = 60;
    p.v[7][1] = 100;
    double worst = 0, peak = 0;
    int ticks = 0;
    for (; ticks < 90; ++ticks) {  // 3 s
        l.tick(33334);
        for (int s = 0; s < 4; ++s) p.step(1.0 / 120);
        if (l.asleep()) break;  // snapped home: the reference is under ε, not 0
        for (int k = 0; k < ProtoLattice::n; ++k) {
            const FixedVec2 d = l.node(uint8_t(k % 5), uint8_t(k / 5));
            const double ex = toPx(d.x) - (p.x[k][0] - p.rx[k]), ey = toPx(d.y) - (p.x[k][1] - p.ry[k]);
            worst = std::fmax(worst, std::fmax(std::fabs(ex), std::fabs(ey)));
            peak = std::fmax(peak, std::fabs(p.x[k][1] - p.ry[k]));
        }
    }
    ASSERT(peak > 0.5, "the kick moves the reference by more than half a pixel");
    ASSERT(worst < 0.02, "every node tracks the prototype's float lattice within 1/50 px");
    ASSERT(ticks > 10 && ticks < 90, "it rings, then sleeps");
    if (getenv("LATTICE_VERBOSE") || worst >= 0.02)
        fprintf(stderr, "  worst %.4f px (peak %.3f), asleep after %d ticks\n", worst, peak, ticks);
}

static bool sameField(const Lattice& a, const Lattice& b) {
    for (int j = -60; j <= 20; j += 10)
        for (int i = -40; i <= 40; i += 10)
            if (a.displacementAt(px(i, j)) != b.displacementAt(px(i, j))) return false;
    return true;
}

static void testFrameRateIndependent() {
    // 60, 30 and 45 fps over the same 2 s: the same ticks, the same bits.
    Lattice a = makeLattice(), b = makeLattice(), c = makeLattice();
    const FixedVec2 at = px(12, -55), v = px(-200, 240);
    a.impulse(at, v);
    b.impulse(at, v);
    c.impulse(at, v);
    for (int f = 0; f < 120; ++f) a.tick(16667);
    for (int f = 0; f < 60; ++f) b.tick(33333);
    for (int f = 0; f < 90; ++f) c.tick(f % 9 == 0 ? 22224 : 22222);
    ASSERT(a.steps() == b.steps() && b.steps() == c.steps(), "the same sub-steps at any frame rate");
    ASSERT(sameField(a, b) && sameField(b, c), "the same field at any frame rate");
    ASSERT(a.steps() > 0 && a.steps() % Lattice::kSubSteps == 0, "whole ticks of 4 sub-steps");

    // A long frame runs at most 4 ticks and drops the rest.
    Lattice d = makeLattice();
    d.impulse(at, v);
    d.tick(1000000);
    ASSERT(d.steps() == Lattice::kMaxTicksPerUpdate * Lattice::kSubSteps, "a 1 s hitch runs 4 ticks, not 30");
    d.tick(16667);
    ASSERT(d.steps() == (Lattice::kMaxTicksPerUpdate + 1) * Lattice::kSubSteps, "and the backlog is gone");
}

static void testSettlesSnapsAndSleeps() {
    Lattice l = makeLattice();
    l.impulse(px(0, -60), px(0, 400));  // a hard kick on the top edge
    int ticks = 0, changes = 0;
    Fixed peak{};
    while (!l.asleep() && ticks < 300) {
        if (l.tick(33334)) ++changes;
        if (l.maxDisplacement() > peak) peak = l.maxDisplacement();
        ++ticks;
    }
    ASSERT(l.asleep(), "a kicked Lattice settles and sleeps");
    ASSERT(peak > Fixed::fromInt(2), "the kick moves the jelly by pixels");
    ASSERT(ticks < 90, "within 3 s at 3.2 Hz, ζ 0.55");
    ASSERT(changes > 10 && changes < ticks, "it redraws while it moves, not through the quiet ticks before the snap");
    ASSERT(l.maxDisplacement() == Fixed{} && l.velocityAt(px(0, -20)) == FixedVec2{}, "snapped exactly home");
    const uint32_t steps = l.steps();
    for (int f = 0; f < 300; ++f) ASSERT(!l.tick(16667), "asleep: no change");
    ASSERT(l.steps() == steps, "asleep: zero sim steps");

    // The sleep test needs K quiet ticks in a row: a 7th tick keeps it awake.
    LatticeTuning t;
    t.sleepTicks = 200;
    Lattice k;
    k.configure(px(-40, -60), px(40, 20), 5, 5, t);
    k.impulse(px(0, -60), px(0, 400));
    for (int f = 0; f < 150; ++f) k.tick(33334);
    ASSERT(!k.asleep(), "fewer quiet ticks than K: still awake");

    // A wake from rest steps but changes nothing, and sleeps again after K.
    Lattice r = makeLattice();
    r.wake();
    bool changed = false;
    for (int f = 0; f < 6; ++f) changed = r.tick(33334) || changed;
    ASSERT(!changed && r.asleep() && r.steps() == 6 * Lattice::kSubSteps, "a woken resting Lattice redraws nothing");
}

static void testStiffnessClampedStable() {
    Lattice l = makeLattice();
    ASSERT(l.stiffnessHz() == LatticeTuning{}.stiffnessHz, "3.2 Hz on square cells is not clamped");

    // An 80 × 10 box (cells 20 × 2.5): the tuned stiffness would diverge at
    // 4 sub-steps (|eig| > 1 in the explicit scheme), so it is lowered.
    Lattice thin;
    thin.configure(px(-40, 0), px(40, 10));
    ASSERT(thin.stiffnessHz() < LatticeTuning{}.stiffnessHz, "an elongated grid clamps the stiffness");

    // The stiffest, most damped, most elongated cases still settle.
    LatticeTuning hard;
    hard.stiffnessHz = Fixed::fromInt(50);
    hard.damping = Fixed::fromInt(2);
    const FixedVec2 boxes[][2] = {{px(-40, -60), px(40, 20)}, {px(-80, 0), px(80, 6)}, {px(-3, -70), px(3, 70)}};
    for (const auto& box : boxes)
        for (Fixed zeta : {Fixed::fromRatio(5, 100), Fixed::fromRatio(55, 100), Fixed::fromInt(2)}) {
            hard.damping = zeta;
            Lattice s;
            s.configure(box[0], box[1], 5, 5, hard);
            ASSERT(s.stiffnessHz() <= Fixed::fromInt(Lattice::kMaxStiffnessHz), "clamped to at most 8 Hz");
            s.impulse(box[0], px(500, -500));
            Fixed peak{};
            for (int f = 0; f < 600 && !s.asleep(); ++f) {
                s.tick(33334);
                if (s.maxDisplacement() > peak) peak = s.maxDisplacement();
            }
            ASSERT(s.asleep(), "it settles within 20 s");
            ASSERT(peak < Fixed::fromInt(100), "and never blows up");
        }
}

// The prototype's updateSecondaries() (#314, look-and-feel.html @ e105b7d), in
// double, without the field's local rotation (a sway arm is carried like a
// rigid Feature, then swings on its own spring): driven by the same anchor
// displacements, it is the reference the Fixed arm is held to.
struct ProtoSway {
    double hz, zeta, gain = 0.35, L, dir[2], th = 0, om = 0, a1[2] = {0, 0}, a2[2] = {0, 0};
    ProtoSway(double armX, double armY, double hz_, double zeta_) : hz(hz_), zeta(zeta_) {
        L = std::fmax(3, std::hypot(armX, armY));
        dir[0] = armX / L;
        dir[1] = armY / L;
    }
    void tick(double ax0, double ay0) {  // the anchor's displacement this tick
        const double dt = 1.0 / 30;
        double ax = (ax0 - 2 * a1[0] + a2[0]) / (dt * dt), ay = (ay0 - 2 * a1[1] + a2[1]) / (dt * dt);
        const double am = std::hypot(ax, ay);
        if (am > 4000) { ax *= 4000 / am; ay *= 4000 / am; }
        a2[0] = a1[0]; a2[1] = a1[1];
        a1[0] = ax0; a1[1] = ay0;
        const double ws = 2 * M_PI * hz, h = dt / 4;
        for (int k = 0; k < 4; ++k) {
            const double c = std::cos(th), s = std::sin(th);
            const double d[2] = {dir[0] * c - dir[1] * s, dir[0] * s + dir[1] * c};
            const double torque = (d[0] * -ay - d[1] * -ax) / L * gain;
            om += (-ws * ws * th - 2 * zeta * ws * om + torque) * h;
            th = std::fmax(-1, std::fmin(1, th + om * h));
        }
    }
};

static void testSwayArmSwingsAndSettles() {
    // An antenna on the top edge's middle node (2, 0), 12 px long, pointing up;
    // the top edge kicked sideways.
    const FixedVec2 anchor = px(0, -60), arm = px(0, -12);
    Lattice l = makeLattice();
    ASSERT(l.addSway(anchor, arm) == 0, "the first sway arm is arm 0");
    ASSERT(l.swayCount() == 1, "one arm");
    ASSERT(l.swayAngleAt(anchor) == Fixed{} && l.swayAngle(0) == Fixed{}, "an arm hangs at rest");
    ASSERT(l.swayAngleAt(px(5, -60)) == Fixed{}, "no arm elsewhere: no angle");
    l.impulse(anchor, px(900, 0));
    ProtoSway p(0, -12, 2.4, 0.27);
    double worst = 0, peak = 0;
    int ticks = 0;
    for (; ticks < 150 && !l.asleep(); ++ticks) {
        l.tick(33334);
        if (l.asleep()) break;
        const FixedVec2 d = l.node(2, 0);
        p.tick(toPx(d.x), toPx(d.y));
        const double th = l.swayAngle(0).raw / 65536.0;
        worst = std::fmax(worst, std::fabs(th - p.th));
        peak = std::fmax(peak, std::fabs(p.th));
    }
    ASSERT(peak > 0.05, "a hard kick swings the arm by more than 0.05 rad (0.6 px at its tip)");
    ASSERT(worst < 0.005, "the arm tracks the prototype's float spring within 0.005 rad");
    ASSERT(l.asleep() && ticks < 150, "and it settles and sleeps within 5 s");
    ASSERT(l.swayAngle(0) == Fixed{} && l.swayAngleAt(anchor) == Fixed{}, "snapped exactly home");
    if (getenv("LATTICE_VERBOSE") || worst >= 0.005)
        fprintf(stderr, "  sway worst %.5f rad (peak %.3f), asleep after %d ticks\n", worst, peak, ticks);

    // The arm takes part in sleep: a slow, loose arm keeps the Lattice awake
    // after the grid itself has gone quiet.
    Lattice bare = makeLattice(), loose = makeLattice();
    SwaySpring slow;
    slow.hz = Fixed::fromInt(1);
    slow.damping = Fixed::fromRatio(5, 100);
    loose.addSway(anchor, arm, slow);
    bare.impulse(anchor, px(300, 0));
    loose.impulse(anchor, px(300, 0));
    int bareTicks = 0, looseTicks = 0;
    while (!bare.asleep() && bareTicks < 900) {
        bare.tick(33334);
        ++bareTicks;
    }
    bool swungLate = false;
    while (!loose.asleep() && looseTicks < 900) {
        loose.tick(33334);
        ++looseTicks;
        if (looseTicks > bareTicks && loose.swayAngleAt(anchor) != Fixed{}) swungLate = true;
    }
    ASSERT(loose.asleep(), "a loose arm settles too");
    ASSERT(looseTicks > bareTicks + 15 && swungLate, "the Lattice stays awake while its arm still swings");
    ASSERT(loose.swayAngle(0) == Fixed{}, "and snaps the arm home with the grid");

    // Arms are cleared, and a full set refuses more.
    l.clearSway();
    ASSERT(l.swayCount() == 0, "clearSway drops every arm");
    for (int k = 0; k < Lattice::kMaxSway; ++k) l.addSway(px(k, -60), arm);
    ASSERT(l.addSway(px(0, 0), arm) < 0, "past kMaxSway arms: refused");
}

// ---------------------------------------------------------------------------
// Pull and drag (Tomodachi #336)
// ---------------------------------------------------------------------------

static void testPullANode() {
    Lattice l = makeLattice();
    // The top edge's middle node is (2, 0), home at (0, -60). Grab just off it.
    const int h = l.grabNode(px(2, -57));
    ASSERT(h >= 0, "a grab on a configured Lattice gives a handle");
    ASSERT(l.grabbed() && !l.asleep(), "a grab wakes it");
    const int other = l.grabNode(px(0, 0));
    ASSERT(other >= 0 && other != h && l.grabCount() == 2, "the C++ keeps the handle form: two grabs at once");
    l.releaseGrab(other, FixedVec2{});
    ASSERT(l.grabCount() == 1, "a release frees its handle only");

    // Pull it 10 px up: it gets there and the feet (bottom row) barely move.
    l.moveGrab(h, px(0, -70));
    for (int t = 0; t < 30; ++t) l.tick(33334);
    const FixedVec2 top = l.node(2, 0);
    ASSERT(std::fabs(toPx(top.x)) < 0.01 && std::fabs(toPx(top.y) + 10) < 0.6, "the pulled node sits near its target");
    ASSERT(std::fabs(toPx(l.node(2, 1).y)) > 1, "its neighbours stretch along");
    ASSERT(std::fabs(toPx(l.node(2, 4).y)) < 0.5, "the feet stay (the bottom row is held)");
    ASSERT(!l.asleep(), "held, it never sleeps");

    // Pull far: the stretch is rubbery, never past the reach (26 px).
    l.moveGrab(h, px(0, -160));
    for (int t = 0; t < 30; ++t) l.tick(33334);
    const double far = -toPx(l.node(2, 0).y);
    ASSERT(far > 20 && far <= 26, "a 100 px pull stretches 26 px at most");
    ASSERT(l.maxDisplacement() < Fixed::fromInt(30), "and the grid stays bounded");

    // Release with a throw: it springs back, settles and sleeps at rest.
    l.releaseGrab(h, px(0, 300));
    ASSERT(!l.grabbed(), "released");
    ASSERT(std::fabs(toPx(l.velocityAt(px(0, -60)).y) - 300) < 0.5, "the release velocity throws the node");
    int ticks = 0;
    while (!l.asleep() && ticks < 300) {
        l.tick(33334);
        ++ticks;
    }
    ASSERT(l.asleep() && ticks < 120, "it settles within 4 s");
    ASSERT(l.maxDisplacement() == Fixed{}, "snapped home");
    l.moveGrab(h, px(0, -80));
    ASSERT(l.asleep(), "a stale handle moves nothing");
    Lattice none;
    ASSERT(none.grabNode(px(0, 0)) < 0, "an unconfigured Lattice has nothing to grab");
}

static void testPullASwayArm() {
    const FixedVec2 anchor = px(0, -60), arm = px(0, -12);
    Lattice l = makeLattice();
    l.addSway(anchor, arm);
    ASSERT(l.swayIndexAt(anchor) == 0 && l.swayIndexAt(px(1, -60)) < 0, "the arm hung at an anchor");
    ASSERT(l.grabSway(7) < 0, "no such arm: no grab");
    const int h = l.grabSway(0);
    ASSERT(h >= 0 && !l.asleep(), "grabbing an arm wakes the Lattice");
    // Pull the tip to the right: the arm turns clockwise (+), the grid stays home.
    l.moveGrab(h, px(12, -72));
    for (int t = 0; t < 30; ++t) l.tick(33334);
    const double th = toPx(l.swayAngle(0));
    ASSERT(th > 0.4 && th < 0.8, "pulled 45 degrees over, the arm leans most of the way");
    ASSERT(l.maxDisplacement() == Fixed{}, "pulling an arm leaves the grid alone");
    l.releaseGrab(h, px(-200, 0));
    int ticks = 0;
    while (!l.asleep() && ticks < 300) {
        l.tick(33334);
        ++ticks;
    }
    ASSERT(l.asleep() && l.swayAngle(0) == Fixed{}, "released, it swings back and sleeps at rest");
}

static void testCarryLags() {
    Lattice l = makeLattice();
    l.setFrame(px(80, 140));
    ASSERT(l.frame() == px(80, 140) && l.asleep(), "a snapped frame does not wake it");
    l.carryTo(px(110, 140));
    ASSERT(!l.asleep() && l.carrying(), "a carry wakes it");
    double lag = 0, feet = 0;
    for (int t = 0; t < 15; ++t) {
        l.tick(33334);
        lag = std::fmax(lag, -toPx(l.node(2, 0).x));
        feet = std::fmax(feet, std::fabs(toPx(l.node(2, 4).x)));
    }
    ASSERT(toPx(l.frame().x) > 100, "the frame follows its target through the position spring");
    ASSERT(lag > 2, "the top lags behind");
    ASSERT(feet < 0.01, "the feet ride the frame");
    int ticks = 0;
    while (!l.asleep() && ticks < 300) {
        l.tick(33334);
        ++ticks;
    }
    ASSERT(l.asleep() && ticks < 120, "it arrives, settles and sleeps");
    ASSERT(l.frame() == px(110, 140) && l.maxDisplacement() == Fixed{}, "exactly on the target, at rest");
    // Frame-rate independent: 60 fps steps the 30 fps path.
    Lattice a = makeLattice(), b = makeLattice();
    a.carryTo(px(30, 0));
    b.carryTo(px(30, 0));
    for (int t = 0; t < 20; ++t) {
        a.tick(33334);
        b.tick(16667);
        b.tick(16667);
    }
    ASSERT(a.frame() == b.frame() && a.node(2, 0) == b.node(2, 0), "the same carry at 30 and 60 fps");
}

int main() {
    testSwayArmSwingsAndSettles();
    testRestIsAsleep();
    testImpulseMovesTheJellyThere();
    testMatchesThePrototypeLattice();
    testFrameRateIndependent();
    testSettlesSnapsAndSleeps();
    testStiffnessClampedStable();
    testPullANode();
    testPullASwayArm();
    testCarryLags();
    printf("shape_lattice_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
