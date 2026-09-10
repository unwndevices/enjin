// body_test.cpp — C_Body / ColliderSet physics step (Tomodachi #80, ADR-0003 §4).
//
// Pins the two "Done when" acceptance behaviours natively:
//   PHYS-BODY-01  ball rests on a floor without tunnelling at N=1 and N=4
//   PHYS-BODY-02  swept TOI stops a fast ball punching through a thin wall
//                 (and the naive non-swept path does tunnel)
//   PHYS-BODY-03  a flipper's spring-driven ω×r surface velocity launches a
//                 resting ball, and the contact buffer polls kind/relSpeed
#include <enjin2/core/colliders.hpp>

#include <cmath>
#include <cstdio>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                       \
    do {                                        \
        if (!(cond)) {                          \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++;                         \
        } else {                                \
            passes++;                           \
        }                                       \
    } while (0)

static const float FRAME_DT = 1.0f / 30.0f;

// Run `frames` frames with gravity; invoke eachFrame(i, contacts) after each.
template <typename Fn>
static void run(BodyState& b, ColliderSet& set, float gx, float gy,
                int frames, int N, bool swept, Fn&& eachFrame) {
    StepStats stats;
    for (int i = 0; i < frames; ++i) {
        std::vector<Contact> contacts;
        stepFrame(b, set, gx, gy, FRAME_DT, N, swept, contacts, stats);
        eachFrame(i, contacts);
    }
}

static void test_rest_no_tunnel() {
    printf("--- PHYS-BODY-01: rest without tunnelling ---\n");
    for (int N = 1; N <= 4; ++N) {
        BodyState b;
        b.x = 0; b.y = 80; b.radius = 4; b.restitution = 0.2f; b.drag = 0.0f;
        ColliderSet set;
        SegCollider floorSeg;
        floorSeg.ax = -100; floorSeg.ay = 100; floorSeg.bx = 100; floorSeg.by = 100;
        floorSeg.restitution = 0.2f;
        set.segments.push_back(floorSeg);

        float maxY = -1e9f;
        run(b, set, 0.0f, 320.0f, 90, N, true, [&](int, const std::vector<Contact>&) {
            if (b.y > maxY) maxY = b.y;
        });

        const float settle = 100.0f - 4.0f;
        char msg[128];
        snprintf(msg, sizeof(msg), "N=%d ball rests on floor (y=%.3f)", N, b.y);
        ASSERT(b.y <= settle + 0.2f, msg);
        ASSERT(b.y > settle - 8.0f, "ball did not sink far below the floor");
        snprintf(msg, sizeof(msg), "N=%d never tunnelled through floor (maxY=%.3f)", N, maxY);
        ASSERT(maxY <= settle + 4.0f, msg);
        snprintf(msg, sizeof(msg), "N=%d settles to rest (speed=%.3f)", N,
                 std::hypot(b.vx, b.vy));
        ASSERT(std::hypot(b.vx, b.vy) < 5.0f, msg);
    }
}

static void test_swept_vs_naive() {
    printf("--- PHYS-BODY-02: swept vs naive tunnelling ---\n");
    auto make = []() {
        BodyState b;
        b.x = 70; b.y = 0; b.vx = 2000.0f; b.vy = 0.0f;
        b.radius = 2; b.restitution = 0.5f; b.drag = 0.0f;
        ColliderSet set;
        AabbCollider wall;
        wall.minx = 80; wall.miny = -100; wall.maxx = 82; wall.maxy = 100;
        wall.restitution = 0.5f;
        set.aabbs.push_back(wall);
        return std::make_pair(b, set);
    };

    {
        auto [b, set] = make();
        run(b, set, 0.0f, 0.0f, 1, 1, true, [](int, const std::vector<Contact>&) {});
        char msg[128];
        snprintf(msg, sizeof(msg), "swept stops the ball (x=%.3f)", b.x);
        ASSERT(b.x <= 84.0f, msg);
    }
    {
        auto [b2, set2] = make();
        run(b2, set2, 0.0f, 0.0f, 1, 1, false, [](int, const std::vector<Contact>&) {});
        char msg[128];
        snprintf(msg, sizeof(msg), "naive path tunnels through the wall (x=%.3f)", b2.x);
        ASSERT(b2.x > 84.0f, msg);
    }
}

static void test_flipper_launch() {
    printf("--- PHYS-BODY-03: flipper impulse + contact buffer ---\n");
    // A horizontal flipper (angle 0, pointing +x) so a frictionless ball can
    // rest on it; flipping rotates it up (negative angle), the ω×r surface
    // velocity launches the resting ball — the "moved-into" contact the step
    // must resolve (depenetration + surface impulse).
    BodyState b;
    b.x = 65; b.y = 146; b.radius = 4; b.restitution = 0.2f; b.drag = 0.0f;
    ColliderSet set;
    Flipper f;
    f.pivotX = 50; f.pivotY = 150; f.length = 30;
    f.restAngle = 0.0f; f.activeAngle = -1.2f;
    f.angle = 0.0f; f.target = 0.0f; f.springVel = 0.0f;
    f.restitution = 0.2f;
    set.flippers.push_back(f);

    // Settle: drop the ball until it comes to rest on the flipper.
    run(b, set, 0.0f, 320.0f, 50, 4, true, [](int, const std::vector<Contact>&) {});
    char msg[128];
    snprintf(msg, sizeof(msg), "ball rests on flipper (y=%.3f)", b.y);
    ASSERT(b.y > 143.0f && b.y < 147.0f, msg);
    ASSERT(std::fabs(b.vy) < 5.0f, "ball settles to rest before the flip");

    set.flippers[0].target = set.flippers[0].activeAngle;

    bool launched = false;
    bool sawFlipperContact = false;
    run(b, set, 0.0f, 320.0f, 30, 4, true, [&](int, const std::vector<Contact>& cs) {
        for (const auto& c : cs) {
            if (c.kind == ColliderKinds::Flipper) {
                sawFlipperContact = true;
                ASSERT(c.relSpeed >= 0.0f, "flipper contact relSpeed is non-negative");
            }
        }
        if (b.vy < -30.0f) launched = true;
    });

    ASSERT(launched, "flipper impulse launched the ball upward");
    ASSERT(sawFlipperContact, "contact buffer polled the flipper hit");
    ASSERT(b.y < 146.0f, "ball leaves the flipper after launch");
}

int main() {
    test_rest_no_tunnel();
    test_swept_vs_naive();
    test_flipper_launch();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
