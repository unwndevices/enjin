// Spring motion primitive test (wayfinder #17 / spec #30): the retargetable
// spring that the launcher chrome moves on. Pure numeric behaviour, pinned
// without a canvas — the integrator, the sub-stepping that keeps it stable at
// high k, retarget velocity inheritance, the settle threshold below the pixel
// round, and a final pass running SpringSystem over a real world.
#include <enjin2/ui/spring.hpp>
#include <enjin2/ui/spring_component.hpp>
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/world.hpp>
#include <cmath>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

static bool nearly(float a, float b, float eps = 0.5f) { return std::fabs(a - b) < eps; }

// One 30 fps frame: kSubSteps micro-steps at the effective integrator dt. Mirrors
// what SpringSystem does per fixed chunk, but on a bare Spring so a test can watch
// the trajectory step by step.
static void frame(Spring& s) {
    const float subDt = (1.0f / 30.0f) / 4.0f;
    for (int n = 0; n < 4; ++n) s.step(subDt);
}

// Perceptual (duration, bounce) resolves to the raw (k, ζ) the #17 sim was tuned
// against, and the three chrome presets land on those numbers.
static void test_perceptual_and_presets() {
    SpringParams pop = springParamsFromPerceptual(0.34f, 0.55f);
    ASSERT(nearly(pop.stiffness, 341.0f, 2.0f), "spring: pop stiffness ~341");
    ASSERT(nearly(pop.dampingRatio, 0.45f, 0.01f), "spring: pop damping ratio ~0.45");

    ASSERT(nearly(SpringPresets::pop().stiffness, 341.0f, 2.0f), "spring: preset pop k~341");
    ASSERT(nearly(SpringPresets::snap().dampingRatio, 0.58f, 0.01f), "spring: preset snap zeta~0.58");
    ASSERT(nearly(SpringPresets::smooth().dampingRatio, 1.0f, 0.01f),
           "spring: preset smooth is critically damped (zeta=1)");
}

// AC1: at a stiffness where the full 30 fps step would blow up (ω·dt > 2), the
// N=4 sub-step keeps the integrator bounded and converging.
static void test_no_divergence_high_k() {
    const SpringParams stiff{8000.0f, 0.15f}; // omega ~89.4; omega*(1/30)=2.98 > 2

    // Full step (no sub-stepping) diverges — this is the first cut's explosion.
    Spring bad;
    bad.setParams(stiff);
    bad.target = 100.0f;
    for (int i = 0; i < 40; ++i) bad.step(1.0f / 30.0f);
    ASSERT(!std::isfinite(bad.x) || std::fabs(bad.x) > 1000.0f,
           "spring: full 30fps step diverges at high k (why sub-stepping exists)");

    // N=4 sub-stepped: bounded and converging to the target.
    Spring good;
    good.setParams(stiff);
    good.target = 100.0f;
    float peak = 0.0f;
    for (int i = 0; i < 40; ++i) { frame(good); peak = std::fmax(peak, std::fabs(good.x)); }
    ASSERT(std::isfinite(good.x) && peak < 200.0f, "spring: N=4 sub-step stays bounded at high k");
    ASSERT(nearly(good.x, 100.0f), "spring: high-k spring converges to target");
}

// AC1: every chrome preset settles inside 400 ms (12 frames at 30 fps); the
// critically-damped 'smooth' preset settles monotonically (no overshoot).
static void test_settles_within_400ms() {
    struct Case { const char* name; SpringParams p; } cases[] = {
        {"pop", SpringPresets::pop()},
        {"snap", SpringPresets::snap()},
        {"smooth", SpringPresets::smooth()},
    };
    // 18 px is the anchor-lift amplitude the pop is actually used at (the #17 sim's
    // selection lift). Settle time against an *absolute* 0.5 px threshold is
    // amplitude-relative, so the 400 ms budget (R2) is asserted at that real move,
    // not an arbitrary large one.
    const float kLift = 18.0f;
    for (auto& c : cases) {
        Spring s;
        s.setParams(c.p);
        s.target = kLift;
        int settledFrame = -1;
        for (int f = 0; f < 12 && settledFrame < 0; ++f) {
            frame(s);
            if (s.settled()) settledFrame = f;
        }
        char msg[96];
        snprintf(msg, sizeof(msg), "spring: preset %s settles within 400ms", c.name);
        ASSERT(settledFrame >= 0 && nearly(s.x, kLift), msg);
    }

    // 'smooth' is critically damped — it must not overshoot past the target.
    Spring smooth;
    smooth.setParams(SpringPresets::smooth());
    smooth.target = kLift;
    float maxX = 0.0f;
    for (int f = 0; f < 12; ++f) { frame(smooth); maxX = std::fmax(maxX, smooth.x); }
    ASSERT(maxX <= kLift + kSpringPosEps, "spring: smooth preset settles monotonically (no overshoot)");
}

// AC2: retarget is "change target, keep x + v" — velocity is inherited, and three
// detents inside one settle window read as one continuous arc (no position jump).
static void test_retarget_inherits_velocity() {
    Spring s;
    s.setParams(SpringPresets::pop());
    s.target = 100.0f;
    frame(s); frame(s); // build up some in-flight velocity toward 100
    const float vBefore = s.v;
    const float xBefore = s.x;
    ASSERT(std::fabs(vBefore) > 1.0f, "spring: has in-flight velocity before retarget");

    s.retarget(0.0f);
    ASSERT(s.v == vBefore && s.x == xBefore,
           "spring: retarget keeps position and velocity (velocity inherited free)");

    // Three detents in one settle window: retarget repeatedly, assert the position
    // stays continuous frame-to-frame (one arc, never a snap/queue) — R1.
    Spring arc;
    arc.setParams(SpringPresets::pop());
    arc.target = 18.0f;
    float prevX = arc.x;
    bool continuous = true;
    const int detentTargets[] = {36, 54, 72};
    for (int d = 0; d < 3; ++d) {
        arc.retarget(static_cast<float>(detentTargets[d]));
        for (int f = 0; f < 2; ++f) {
            frame(arc);
            if (std::fabs(arc.x - prevX) > 40.0f) continuous = false; // no teleport
            prevX = arc.x;
        }
    }
    ASSERT(continuous, "spring: three detents in one window make one continuous arc (R1)");
}

// AC3: the settle threshold is below the integer round, so once a spring settles
// its driver snaps it to rest and no further tick can dirty the rounded pixel.
static void test_settle_no_dirty() {
    using SpringWorld = World<8, SpringComponent<float>>;
    SpringWorld world;
    SpringSystem<SpringWorld, float> system(&world);

    Entity e = world.create();
    auto* sc = world.add<SpringComponent<float>>(e);
    sc->setParams(SpringPresets::pop());
    sc->setValue(0.0f);
    sc->setTarget(18.0f); // integer target — round settles on it

    // Tick until settled.
    int guard = 0;
    while (!sc->settled() && guard++ < 120) system.update(1.0f / 30.0f);
    ASSERT(sc->settled(), "spring: system reaches settle");

    // Once settled, the rounded pixel must never move again.
    int firstPix = static_cast<int>(std::lround(sc->value()));
    bool dirtied = false;
    for (int f = 0; f < 60; ++f) {
        system.update(1.0f / 30.0f);
        if (static_cast<int>(std::lround(sc->value())) != firstPix) dirtied = true;
    }
    ASSERT(!dirtied, "spring: no tile dirtied after settle (jitter-forever impossible)");
    ASSERT(sc->value() == 18.0f, "spring: settled spring snapped to exact rest");
}

// The SpringSystem is the priority-100 driver: it advances every spring in the
// world and settles each to its own target.
static void test_system_ticks_all() {
    using SpringWorld = World<8, SpringComponent<float>>;
    SpringWorld world;
    SpringSystem<SpringWorld, float> system(&world);
    ASSERT(system.getPriority() == 100, "spring: system ticks at priority 100 (with animators)");

    Entity a = world.create();
    Entity b = world.create();
    auto* sa = world.add<SpringComponent<float>>(a);
    auto* sb = world.add<SpringComponent<float>>(b);
    sa->setParams(SpringPresets::snap());
    sb->setParams(SpringPresets::snap());
    sa->setTarget(25.0f);
    sb->setTarget(-25.0f);

    for (int f = 0; f < 30; ++f) system.update(1.0f / 30.0f);
    ASSERT(nearly(sa->value(), 25.0f), "spring: system advanced the first spring to its target");
    ASSERT(nearly(sb->value(), -25.0f), "spring: system advanced the second spring to its target");
}

int main() {
    test_perceptual_and_presets();
    test_no_divergence_high_k();
    test_settles_within_400ms();
    test_retarget_inherits_velocity();
    test_settle_no_dirty();
    test_system_ticks_all();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
