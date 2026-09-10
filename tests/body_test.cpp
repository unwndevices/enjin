// body_test.cpp — C_Body / ColliderSet physics step (Tomodachi #80, ADR-0003 §4).
//
// Pins the "Done when" acceptance behaviours natively, at every substep count
// the ADR promises (N=1..4):
//   PHYS-BODY-01  ball rests on a segment / AABB floor without tunnelling
//   PHYS-BODY-02  swept TOI stops a fast ball at a thin wall and a bumper
//                 (and the naive non-swept path does tunnel)
//   PHYS-BODY-03  a flipper's spring-driven ω×r surface velocity launches a
//                 resting ball, and the contact buffer polls kind/relSpeed
//   PHYS-BODY-04  ball slides along a floor; contacts report the surface point
//   PHYS-BODY-05  C_Body (the component) polls contacts from C++
//   PHYS-BODY-06  a flipper moving away from the ball does not stall the TOI loop
#include <enjin2/core/colliders.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/body.hpp>

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
static StepStats run(BodyState& b, ColliderSet& set, float gx, float gy,
                     int frames, int N, bool swept, Fn&& eachFrame) {
    StepStats stats;
    for (int i = 0; i < frames; ++i) {
        std::vector<Contact> contacts;
        stepFrame(b, set, gx, gy, FRAME_DT, N, swept, contacts, stats);
        eachFrame(i, contacts);
    }
    return stats;
}

static void test_rest_no_tunnel() {
    printf("--- PHYS-BODY-01: rest without tunnelling (segment + AABB, N=1..4) ---\n");
    for (int floorKind = 0; floorKind < 2; ++floorKind) {
        for (int N = 1; N <= 4; ++N) {
            BodyState b;
            b.x = 0; b.y = 80; b.radius = 4; b.restitution = 0.2f; b.drag = 0.0f;
            ColliderSet set;
            if (floorKind == 0) set.addSeg(-100, 100, 100, 100, 0.2f);
            else                set.addAabb(-100, 100, 100, 120, 0.2f);

            float maxY = -1e9f;
            run(b, set, 0.0f, 320.0f, 90, N, true, [&](int, const std::vector<Contact>&) {
                if (b.y > maxY) maxY = b.y;
            });

            const float settle = 100.0f - 4.0f;
            const char* what = floorKind == 0 ? "segment" : "AABB";
            char msg[128];
            snprintf(msg, sizeof(msg), "N=%d ball rests on %s floor (y=%.3f)", N, what, b.y);
            ASSERT(b.y <= settle + 0.2f, msg);
            ASSERT(b.y > settle - 8.0f, "ball did not sink far below the floor");
            snprintf(msg, sizeof(msg), "N=%d never tunnelled through %s floor (maxY=%.3f)", N, what, maxY);
            ASSERT(maxY <= settle + 4.0f, msg);
            snprintf(msg, sizeof(msg), "N=%d settles to rest on %s (speed=%.3f)", N, what,
                     std::hypot(b.vx, b.vy));
            ASSERT(std::hypot(b.vx, b.vy) < 5.0f, msg);
        }
    }
}

static void test_swept_vs_naive() {
    printf("--- PHYS-BODY-02: swept vs naive tunnelling (N=1..4) ---\n");
    auto makeWall = []() {
        BodyState b;
        b.x = 70; b.y = 0; b.vx = 2000.0f; b.vy = 0.0f;
        b.radius = 2; b.restitution = 0.5f; b.drag = 0.0f;
        ColliderSet set;
        set.addAabb(80, -100, 82, 100, 0.5f);
        return std::make_pair(b, set);
    };

    for (int N = 1; N <= 4; ++N) {
        auto [b, set] = makeWall();
        run(b, set, 0.0f, 0.0f, 1, N, true, [](int, const std::vector<Contact>&) {});
        char msg[128];
        snprintf(msg, sizeof(msg), "N=%d swept stops the ball at the thin wall (x=%.3f)", N, b.x);
        ASSERT(b.x <= 78.0f + 0.05f, msg);
        ASSERT(b.vx < 0.0f, "ball bounced back off the wall");
    }
    {
        auto [b2, set2] = makeWall();
        run(b2, set2, 0.0f, 0.0f, 1, 1, false, [](int, const std::vector<Contact>&) {});
        char msg[128];
        snprintf(msg, sizeof(msg), "naive path tunnels through the wall (x=%.3f)", b2.x);
        ASSERT(b2.x > 84.0f, msg);
    }

    // Fast ball straight into a bumper circle: never ends the frame inside it.
    for (int N = 1; N <= 4; ++N) {
        BodyState b;
        b.x = 0; b.y = 50; b.vx = 2400.0f; b.radius = 3; b.restitution = 0.5f;
        ColliderSet set;
        set.addCircle(60, 50, 9, 1.15f, ColliderKinds::Bouncy);
        bool sawBouncy = false;
        run(b, set, 0.0f, 0.0f, 1, N, true, [&](int, const std::vector<Contact>& cs) {
            for (const auto& c : cs) if (c.kind == ColliderKinds::Bouncy) sawBouncy = true;
        });
        const float d = std::hypot(b.x - 60.0f, b.y - 50.0f);
        char msg[128];
        snprintf(msg, sizeof(msg), "N=%d ball not inside the bumper after the hit (d=%.3f)", N, d);
        ASSERT(d >= 12.0f - 0.05f, msg);
        ASSERT(b.vx < 0.0f, "ball bounced back off the bumper");
        ASSERT(sawBouncy, "bumper hit buffered as a Bouncy contact");
    }
}

static ColliderSet makeFlipperSet() {
    ColliderSet set;
    // Horizontal flipper (angle 0, pointing +x) so a frictionless ball can rest
    // on it; active angle −1.2 rad rotates it up.
    set.addFlipper(50, 150, 30, 0.0f, -1.2f, 0.2f);
    return set;
}

static void test_flipper_launch() {
    printf("--- PHYS-BODY-03: flipper impulse + contact buffer ---\n");
    // Flipping rotates the segment up (negative angle); the ω×r surface velocity
    // launches the resting ball — the "moved-into" contact the step must resolve
    // (depenetration + surface impulse).
    BodyState b;
    b.x = 65; b.y = 146; b.radius = 4; b.restitution = 0.2f; b.drag = 0.0f;
    ColliderSet set = makeFlipperSet();

    // Settle: drop the ball until it comes to rest on the flipper.
    run(b, set, 0.0f, 320.0f, 50, 4, true, [](int, const std::vector<Contact>&) {});
    char msg[128];
    snprintf(msg, sizeof(msg), "ball rests on flipper (y=%.3f)", b.y);
    ASSERT(b.y > 143.0f && b.y < 147.0f, msg);
    ASSERT(std::fabs(b.vy) < 5.0f, "ball settles to rest before the flip");

    ASSERT(set.setFlipperActive(0, true), "setFlipperActive accepts index 0");
    ASSERT(!set.setFlipperActive(1, true), "setFlipperActive rejects an out-of-range index");

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
    ASSERT(set.flippers[0].angle < -0.5f, "spring drove the flipper toward its active angle");
}

static void test_slide_and_contact_point() {
    printf("--- PHYS-BODY-04: slide along a floor, contact point on the surface (N=1..4) ---\n");
    for (int N = 1; N <= 4; ++N) {
        BodyState b;
        b.x = 0; b.y = 96; b.vx = 60.0f; b.radius = 4; b.restitution = 0.0f; b.drag = 0.0f;
        ColliderSet set;
        set.addSeg(-100, 100, 400, 100, 0.0f);

        float minY = 1e9f, maxY = -1e9f, lastX = b.x;
        bool monotonic = true;
        bool pointOnSurface = true;
        int contactsSeen = 0;
        run(b, set, 0.0f, 320.0f, 60, N, true, [&](int, const std::vector<Contact>& cs) {
            if (b.y < minY) minY = b.y;
            if (b.y > maxY) maxY = b.y;
            if (b.x < lastX) monotonic = false;
            lastX = b.x;
            for (const auto& c : cs) {
                ++contactsSeen;
                // Surface point = centre − n·radius → on the floor line y=100,
                // not at the ball centre (y≈96).
                if (std::fabs(c.py - 100.0f) > 0.5f) pointOnSurface = false;
                if (std::fabs(c.ny + 1.0f) > 1e-3f) pointOnSurface = false;
            }
        });

        char msg[160];
        snprintf(msg, sizeof(msg), "N=%d ball keeps sliding +x (x=%.1f)", N, b.x);
        ASSERT(monotonic && b.x > 100.0f, msg);
        snprintf(msg, sizeof(msg), "N=%d ball stays on the floor while sliding (y in [%.2f, %.2f])", N, minY, maxY);
        ASSERT(maxY <= 96.2f && minY >= 90.0f, msg);
        ASSERT(std::fabs(b.vx - 60.0f) < 1e-3f, "frictionless slide keeps its tangential speed");
        snprintf(msg, sizeof(msg), "N=%d contacts report the surface point (%d seen)", N, contactsSeen);
        ASSERT(contactsSeen > 0 && pointOnSurface, msg);
    }
}

static void test_component_contact_poll() {
    printf("--- PHYS-BODY-05: C_Body polls contacts from C++ ---\n");
    Object obj;
    C_Body* body = obj.addComponent<C_Body>();
    ASSERT(body != nullptr, "addComponent<C_Body>");
    ASSERT(body->getSubsteps() == 4, "default substeps N=4 (ADR-0003 §4)");

    ColliderSet set;
    set.addCircle(80, 120, 9, 1.15f, ColliderKinds::Bouncy);
    body->setPosition(80, 80);
    body->setRadius(4);
    body->setRestitution(0.5f);
    body->setGravity(0, 320);

    body->step(FRAME_DT);
    ASSERT(body->numContacts() == 0, "step() without a collider set is a no-op");

    body->setColliders(&set);
    bool sawBouncy = false;
    float maxRel = -1.0f;
    bool pointOnRim = true;
    for (int i = 0; i < 40; ++i) {
        body->step(FRAME_DT);
        for (size_t k = 0; k < body->numContacts(); ++k) {
            const Contact* c = body->contact(k);
            if (c->kind == ColliderKinds::Bouncy) sawBouncy = true;
            if (c->relSpeed > maxRel) maxRel = c->relSpeed;
            const float d = std::hypot(c->px - 80.0f, c->py - 120.0f);
            if (std::fabs(d - 9.0f) > 0.5f) pointOnRim = false;
        }
    }
    ASSERT(sawBouncy, "C_Body buffered a Bouncy contact");
    ASSERT(maxRel >= 0.0f, "relSpeed polled through C_Body is non-negative");
    ASSERT(pointOnRim, "contact point lies on the bumper's rim");
    ASSERT(body->contact(body->numContacts()) == nullptr, "contact(i) out of range → nullptr");
}

static void test_flipper_moving_away() {
    printf("--- PHYS-BODY-06: flipper moving away does not stall the TOI loop ---\n");
    BodyState b;
    b.x = 65; b.y = 146; b.radius = 4; b.restitution = 0.2f; b.drag = 0.0f;
    ColliderSet set = makeFlipperSet();
    run(b, set, 0.0f, 320.0f, 50, 4, true, [](int, const std::vector<Contact>&) {});
    const float restY = b.y;

    // Flip DOWN (+y on screen): the surface runs away from the resting ball.
    set.setFlipperTarget(0, 1.2f);
    const StepStats stats = run(b, set, 0.0f, 320.0f, 10, 4, true,
                                [](int, const std::vector<Contact>&) {});

    char msg[128];
    snprintf(msg, sizeof(msg), "ball falls after the flipper drops away (y %.2f → %.2f)", restY, b.y);
    ASSERT(b.y > restY + 5.0f, msg);
    snprintf(msg, sizeof(msg), "TOI budget not burned by separating hits (%d iters / 40 substeps)",
             stats.toiIters);
    ASSERT(stats.toiIters < 40, msg);
}

int main() {
    test_rest_no_tunnel();
    test_swept_vs_naive();
    test_flipper_launch();
    test_slide_and_contact_point();
    test_component_contact_poll();
    test_flipper_moving_away();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
