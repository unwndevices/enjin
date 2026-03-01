// physics_test.cpp — tests for physics helper functions and TrigLUT
// Tests C++ stateless physics helpers in enjin2::physics namespace
// Phase 45: PHYS-01..PHYS-08
#include <enjin2/core/physics.hpp>
#include <enjin2/core/math.hpp>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace enjin2;

#define ASSERT(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); exit(1); } } while(0)

#define ASSERT_NEAR(a, b, eps, msg) \
    do { if (std::fabs((a)-(b)) > (eps)) { printf("FAIL: %s — got %f, expected %f (line %d)\n", msg, (double)(a), (double)(b), __LINE__); exit(1); } } while(0)

int main() {
    printf("=== physics_test ===\n");

    // ── applyGravity ──
    {
        float vx, vy;

        // (0,0) + (0,980)*1.0 = (0,980)
        physics::applyGravity(0.0f, 0.0f, 0.0f, 980.0f, 1.0f, &vx, &vy);
        ASSERT_NEAR(vx, 0.0f, 0.01f, "applyGravity: vx from rest");
        ASSERT_NEAR(vy, 980.0f, 0.01f, "applyGravity: vy from rest");
        printf("  PASS: applyGravity (rest, dt=1.0)\n");

        // (100,0) + (0,500)*0.5 = (100,250)
        physics::applyGravity(100.0f, 0.0f, 0.0f, 500.0f, 0.5f, &vx, &vy);
        ASSERT_NEAR(vx, 100.0f, 0.01f, "applyGravity: vx with initial");
        ASSERT_NEAR(vy, 250.0f, 0.01f, "applyGravity: vy with initial");
        printf("  PASS: applyGravity (initial velocity, dt=0.5)\n");

        // null outputs should not crash
        physics::applyGravity(0.0f, 0.0f, 0.0f, 9.8f, 1.0f, nullptr, nullptr);
        printf("  PASS: applyGravity (null outputs)\n");
    }

    // ── bounce ──
    {
        float vx, vy;

        // (10,0) against normal (1,0) with restitution 1.0 = (-10,0) perfect
        physics::bounce(10.0f, 0.0f, 1.0f, 0.0f, 1.0f, &vx, &vy);
        ASSERT_NEAR(vx, -10.0f, 0.01f, "bounce: perfect restitution vx");
        ASSERT_NEAR(vy, 0.0f, 0.01f, "bounce: perfect restitution vy");
        printf("  PASS: bounce (restitution=1.0)\n");

        // (10,0) against normal (1,0) with restitution 0.5 = (-5,0)
        physics::bounce(10.0f, 0.0f, 1.0f, 0.0f, 0.5f, &vx, &vy);
        ASSERT_NEAR(vx, -5.0f, 0.01f, "bounce: half restitution vx");
        ASSERT_NEAR(vy, 0.0f, 0.01f, "bounce: half restitution vy");
        printf("  PASS: bounce (restitution=0.5)\n");

        // (10,0) against normal (1,0) with restitution 0 = (0,0) stop dead
        physics::bounce(10.0f, 0.0f, 1.0f, 0.0f, 0.0f, &vx, &vy);
        ASSERT_NEAR(vx, 0.0f, 0.01f, "bounce: zero restitution vx");
        ASSERT_NEAR(vy, 0.0f, 0.01f, "bounce: zero restitution vy");
        printf("  PASS: bounce (restitution=0.0, stop dead)\n");

        // null outputs should not crash
        physics::bounce(5.0f, 0.0f, 1.0f, 0.0f, 1.0f, nullptr, nullptr);
        printf("  PASS: bounce (null outputs)\n");
    }

    // ── applyDrag ──
    {
        float vx, vy;

        // (100,0) drag=2 dt=0.5 → factor = 1 - 2*0.5 = 0.0 (clamped) → (0,0)
        physics::applyDrag(100.0f, 0.0f, 2.0f, 0.5f, &vx, &vy);
        ASSERT_NEAR(vx, 0.0f, 0.01f, "applyDrag: clamped to zero vx");
        ASSERT_NEAR(vy, 0.0f, 0.01f, "applyDrag: clamped to zero vy");
        printf("  PASS: applyDrag (factor=0 clamp)\n");

        // (100,0) drag=1 dt=0.1 → factor = 1 - 0.1 = 0.9 → (90,0)
        physics::applyDrag(100.0f, 0.0f, 1.0f, 0.1f, &vx, &vy);
        ASSERT_NEAR(vx, 90.0f, 0.01f, "applyDrag: normal reduction vx");
        ASSERT_NEAR(vy, 0.0f, 0.01f, "applyDrag: normal reduction vy");
        printf("  PASS: applyDrag (drag=1, dt=0.1)\n");

        // Overdrag — factor > 1 drag: factor goes negative, clamp at 0 → result 0
        physics::applyDrag(50.0f, 0.0f, 5.0f, 1.0f, &vx, &vy);
        ASSERT(vx >= 0.0f, "applyDrag: no sign flip");
        ASSERT_NEAR(vx, 0.0f, 0.01f, "applyDrag: overdrag clamp");
        printf("  PASS: applyDrag (overdrag clamp, no sign flip)\n");
    }

    // ── springForce ──
    {
        float vel;

        // pos=0, target=10, vel=0, k=100, d=10, dt=0.1
        // force = (10-0)*100 - 0*10 = 1000
        // vel = 0 + 1000*0.1 = 100
        physics::springForce(0.0f, 10.0f, 0.0f, 100.0f, 10.0f, 0.1f, &vel);
        ASSERT_NEAR(vel, 100.0f, 0.01f, "springForce: displacement pulls toward target");
        printf("  PASS: springForce (displaced from target, no initial velocity)\n");

        // pos=10, target=10, vel=5, k=100, d=10, dt=0.1
        // force = (10-10)*100 - 5*10 = -50
        // vel = 5 + (-50)*0.1 = 0.0 (at target, damped)
        physics::springForce(10.0f, 10.0f, 5.0f, 100.0f, 10.0f, 0.1f, &vel);
        ASSERT_NEAR(vel, 0.0f, 0.01f, "springForce: at target, damped to zero");
        printf("  PASS: springForce (at target, damping removes velocity)\n");

        // null output should not crash
        physics::springForce(0.0f, 5.0f, 0.0f, 100.0f, 10.0f, 0.1f, nullptr);
        printf("  PASS: springForce (null output)\n");
    }

    // ── attract ──
    {
        float fx, fy;

        // (0,0) toward (10,0), strength=100 → force in +x direction
        physics::attract(0.0f, 0.0f, 10.0f, 0.0f, 100.0f, 10000.0f, &fx, &fy);
        ASSERT(fx > 0.0f, "attract: force in +x direction");
        ASSERT_NEAR(fy, 0.0f, 0.01f, "attract: no y component for horizontal approach");
        printf("  PASS: attract (force toward attractor +x)\n");

        // Degenerate (0,0) → (0,0): should not produce NaN (epsilon prevents div-by-zero)
        physics::attract(0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 10000.0f, &fx, &fy);
        ASSERT(fx == fx, "attract: degenerate position no NaN fx");
        ASSERT(fy == fy, "attract: degenerate position no NaN fy");
        printf("  PASS: attract (degenerate same position, no NaN)\n");

        // Max force cap: very close approach would exceed maxForce without cap
        physics::attract(0.0f, 0.0f, 0.001f, 0.0f, 1000000.0f, 5.0f, &fx, &fy);
        float mag = std::sqrt(fx * fx + fy * fy);
        ASSERT(mag <= 5.01f, "attract: max force cap limits magnitude");
        printf("  PASS: attract (max force cap applied)\n");

        // null outputs should not crash
        physics::attract(0.0f, 0.0f, 10.0f, 0.0f, 100.0f, 1000.0f, nullptr, nullptr);
        printf("  PASS: attract (null outputs)\n");
    }

    // ── orbitVelocity ──
    {
        float vx, vy;

        // (10,0) around (0,0) at speed 5 → tangent = (0,5)
        // dx=10, dy=0, len=10 → outVx = -0/10 * 5 = 0, outVy = 10/10 * 5 = 5
        physics::orbitVelocity(10.0f, 0.0f, 0.0f, 0.0f, 5.0f, &vx, &vy);
        ASSERT_NEAR(vx, 0.0f, 0.01f, "orbitVelocity: tangent vx for right-side orbit");
        ASSERT_NEAR(vy, 5.0f, 0.01f, "orbitVelocity: tangent vy for right-side orbit");
        printf("  PASS: orbitVelocity (right-side, speed=5)\n");

        // Degenerate: (0,0) around (0,0) → (0,0)
        physics::orbitVelocity(0.0f, 0.0f, 0.0f, 0.0f, 5.0f, &vx, &vy);
        ASSERT_NEAR(vx, 0.0f, 0.01f, "orbitVelocity: degenerate zero vx");
        ASSERT_NEAR(vy, 0.0f, 0.01f, "orbitVelocity: degenerate zero vy");
        printf("  PASS: orbitVelocity (degenerate same position)\n");

        // null outputs should not crash
        physics::orbitVelocity(10.0f, 0.0f, 0.0f, 0.0f, 5.0f, nullptr, nullptr);
        printf("  PASS: orbitVelocity (null outputs)\n");
    }

    // ── applyVelocity ──
    {
        float ox, oy;

        // (10,20) + (5,-3)*2.0 = (20,14)
        physics::applyVelocity(10.0f, 20.0f, 5.0f, -3.0f, 2.0f, &ox, &oy);
        ASSERT_NEAR(ox, 20.0f, 0.01f, "applyVelocity: x integration");
        ASSERT_NEAR(oy, 14.0f, 0.01f, "applyVelocity: y integration");
        printf("  PASS: applyVelocity (10,20) + (5,-3)*2.0\n");

        // Zero dt: position unchanged
        physics::applyVelocity(5.0f, 3.0f, 100.0f, 200.0f, 0.0f, &ox, &oy);
        ASSERT_NEAR(ox, 5.0f, 0.01f, "applyVelocity: zero dt x");
        ASSERT_NEAR(oy, 3.0f, 0.01f, "applyVelocity: zero dt y");
        printf("  PASS: applyVelocity (zero dt)\n");

        // null outputs should not crash
        physics::applyVelocity(0.0f, 0.0f, 1.0f, 1.0f, 1.0f, nullptr, nullptr);
        printf("  PASS: applyVelocity (null outputs)\n");
    }

    // ── TrigLUT ──
    {
        // sin(0) = 0
        int16_t s0 = math::TrigLUT::sin(0);
        ASSERT_NEAR((float)s0, 0.0f, 500.0f, "TrigLUT::sin(0) near zero");
        ASSERT(s0 >= -10 && s0 <= 10, "TrigLUT::sin(0) = 0");
        printf("  PASS: TrigLUT::sin(0) = 0 (got %d)\n", (int)s0);

        // sin(64) = 32767 (quarter turn = peak)
        int16_t s64 = math::TrigLUT::sin(64);
        ASSERT(s64 >= 32000, "TrigLUT::sin(64) near max (+32767)");
        printf("  PASS: TrigLUT::sin(64) near 32767 (got %d)\n", (int)s64);

        // cos(0) = 32767 (cosine peak at 0)
        int16_t c0 = math::TrigLUT::cos(0);
        ASSERT(c0 >= 32000, "TrigLUT::cos(0) near max (+32767)");
        printf("  PASS: TrigLUT::cos(0) near 32767 (got %d)\n", (int)c0);

        // sin(128) near 0 (half turn = sin(pi) = 0)
        int16_t s128 = math::TrigLUT::sin(128);
        ASSERT(s128 >= -200 && s128 <= 200, "TrigLUT::sin(128) near zero");
        printf("  PASS: TrigLUT::sin(128) near 0 (got %d)\n", (int)s128);

        // sin(192) near -32767 (three-quarter turn = negative peak)
        int16_t s192 = math::TrigLUT::sin(192);
        ASSERT(s192 <= -32000, "TrigLUT::sin(192) near -32767");
        printf("  PASS: TrigLUT::sin(192) near -32767 (got %d)\n", (int)s192);
    }

    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
