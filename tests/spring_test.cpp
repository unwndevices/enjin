// Spring substrate test (#38, part 2/5 of the ship-interior launcher, decision
// #17). The spring is the chrome motion primitive beside AnimatorComponent: it
// holds position AND velocity, so a retarget inherits the in-flight velocity
// (R1, retarget-never-queue), it is fixed-dt sub-stepped so semi-implicit Euler
// does not diverge at high stiffness, and it settles below the integer-grid
// round so it cannot dirty a tile forever (R4). These are pure numeric seams —
// pinned without a world, canvas, or Lua — plus one pass over a real world to
// prove SpringSystem ticks every spring.
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

static bool nearly(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }

static constexpr float kFrame30 = 1.0f / 30.0f; // 33.3 ms

// Apple's perceptual pair maps to the documented raw pair: k = (2π/d)², ζ = 1−b.
// The pop default lands at k≈341, ζ≈0.45 (spec's chosen selection spring).
static void test_perceptual_mapping() {
    SpringParams p = springFromPerceptual(0.34f, 0.55f);
    ASSERT(nearly(p.stiffness, (2.0f * kSpringPi / 0.34f) * (2.0f * kSpringPi / 0.34f), 1.0f),
           "spring: stiffness is (2pi/duration)^2");
    ASSERT(nearly(p.dampingRatio, 0.45f), "spring: damping ratio is 1 - bounce");
    // The published pop numbers, to two figures.
    ASSERT(nearly(p.stiffness, 341.0f, 2.0f), "spring: pop preset k is ~341");
    ASSERT(nearly(p.dampingRatio, 0.45f, 0.01f), "spring: pop preset zeta is ~0.45");
}

// The settle *window*: R2's "400 ms" is the perceptual duration parameter, not
// the time for the overshoot tail to cross the 0.5 px / ~31 px·s threshold. With
// the locked presets (#17) that tail lands ~500 ms in, so the tests allow ~600 ms
// (18 frames) and separately assert the perceptual duration is ~400 ms or under.
static constexpr int kSettleFrames = 18; // ≈ 600 ms at 30 fps

// A critically damped spring (bounce 0) approaches the target monotonically —
// never overshooting or reversing — and settles inside the window.
static void test_smooth_monotonic_settle() {
    Spring s;
    s.setSpec(springpreset::Smooth); // ζ = 1, no overshoot
    s.target = 100.0f;
    ASSERT(springpreset::Smooth.durationS <= 0.4f, "spring: smooth perceptual duration is <= 400 ms");

    float prev = s.x;
    bool overshot = false;
    bool settledInWindow = false;
    for (int frame = 0; frame < kSettleFrames; ++frame) {
        s.advance(kFrame30);
        if (s.x > s.target + 0.001f) overshot = true; // must never exceed target
        if (s.x < prev - 0.001f) overshot = true;     // must never reverse
        prev = s.x;
        if (s.settled()) settledInWindow = true;
    }
    ASSERT(!overshot, "spring: smooth (critically damped) never overshoots or reverses");
    ASSERT(settledInWindow, "spring: smooth settles inside the window");
}

// The pop spring is springy (underdamped): it overshoots, then settles inside the
// window and lands essentially on the target.
static void test_pop_settles_in_window() {
    Spring s;
    s.setSpec(springpreset::Pop);
    s.target = 100.0f;
    ASSERT(springpreset::Pop.durationS <= 0.4f, "spring: pop perceptual duration is <= 400 ms");

    bool sawOvershoot = false;
    int settleFrame = -1;
    for (int frame = 0; frame < kSettleFrames; ++frame) {
        s.advance(kFrame30);
        if (s.x > s.target + 0.5f) sawOvershoot = true;
        if (settleFrame < 0 && s.settled()) settleFrame = frame;
    }
    ASSERT(sawOvershoot, "spring: pop overshoots the target (toy register)");
    ASSERT(settleFrame >= 0 && settleFrame < kSettleFrames, "spring: pop settles inside the window");
    ASSERT(nearly(s.x, 100.0f, 0.5f), "spring: pop lands within half a pixel of target");
}

// Item-6: without sub-stepping, semi-implicit Euler diverges once ω·dt > 2
// (k past ~3600 at a 33 ms frame). With N=4 the same stiff spring stays finite
// and converges. Drive a deliberately stiff spring and assert it never blows up.
static void test_high_k_no_divergence() {
    Spring s;
    s.stiffness = 8000.0f; // ω·dt ≈ 2.98 at 30 fps — unstable un-substepped
    s.dampingRatio = 0.5f;
    s.target = 50.0f;

    float maxAbs = 0.0f;
    for (int frame = 0; frame < 30; ++frame) {
        s.advance(kFrame30, kSpringSubsteps); // N = 4
        maxAbs = std::fmax(maxAbs, std::fabs(s.x));
        ASSERT(std::isfinite(s.x) && std::isfinite(s.v), "spring: stays finite at high k");
    }
    ASSERT(maxAbs < 200.0f, "spring: high-k excursion stays bounded (no divergence)");
    ASSERT(s.settled(), "spring: high-k spring settles under N=4 sub-stepping");
}

// Retarget = change the target, keep x and v. The velocity in flight is inherited
// (not reset to zero), which is what bends three fast detents into one arc (R1).
static void test_retarget_inherits_velocity() {
    Spring s;
    s.setSpec(springpreset::Pop);
    s.target = 100.0f;
    s.advance(kFrame30);
    s.advance(kFrame30); // now moving with some velocity toward 100
    const float vBefore = s.v;
    const float xBefore = s.x;
    ASSERT(std::fabs(vBefore) > 1.0f, "spring: has real velocity mid-flight");

    s.retarget(0.0f); // reverse the goal mid-flight
    ASSERT(nearly(s.v, vBefore) && nearly(s.x, xBefore),
           "spring: retarget keeps x and v (velocity inherited, not restarted)");
    ASSERT(nearly(s.target, 0.0f), "spring: retarget moved only the target");
}

// The settle test is both conditions at once, with the R4-safe defaults: a spring
// sitting exactly on target with zero velocity is settled; nudging either the
// position past 0.5 px or the velocity past ~31 px/s unsettles it.
static void test_settle_thresholds() {
    Spring s;
    s.target = 10.0f;
    s.x = 10.0f;
    s.v = 0.0f;
    ASSERT(s.settled(), "spring: on-target and still is settled");

    s.x = 10.0f + 0.6f; // 0.6 px off — past the 0.5 px position eps
    ASSERT(!s.settled(), "spring: position past 0.5 px is not settled");

    s.x = 10.0f;
    s.v = 40.0f;        // 40 px/s — past the ~31 px/s velocity eps
    ASSERT(!s.settled(), "spring: velocity past ~31 px/s is not settled");

    ASSERT(nearly(kSpringVelEps, 31.25f, 0.01f), "spring: default velocity eps is ~31 px/s");
}

// SpringSystem is only an integrator: it advances every SpringComponent in the
// world each frame, leaving the value application to the host (like AnimatorSystem).
static void test_system_ticks_all() {
    using SpringWorld = World<8, SpringComponent<float>>;
    SpringWorld world;
    SpringSystem<SpringWorld, float> system(&world);

    Entity a = world.create();
    Entity b = world.create();
    for (Entity e : {a, b}) {
        auto* sp = world.add<SpringComponent<float>>(e);
        sp->setSpec(springpreset::Smooth);
        sp->target = 100.0f;
    }

    for (int frame = 0; frame < 12; ++frame) system.update(kFrame30);

    ASSERT(nearly(world.get<SpringComponent<float>>(a)->value(), 100.0f, 1.0f),
           "spring: system advanced the first spring to target");
    ASSERT(nearly(world.get<SpringComponent<float>>(b)->value(), 100.0f, 1.0f),
           "spring: system advanced the second spring to target");
}

int main() {
    test_perceptual_mapping();
    test_smooth_monotonic_settle();
    test_pop_settles_in_window();
    test_high_k_no_divergence();
    test_retarget_inherits_velocity();
    test_settle_thresholds();
    test_system_ticks_all();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
