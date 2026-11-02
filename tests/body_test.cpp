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
//   PHYS-BODY-07  every collider has an id: contacts carry it, remove(id) and
//                 setEnabled(id, on) take one collider out (#384)
//   PHYS-BODY-08  rolling resistance: a constant deceleration that holds a ball
//                 on a slope gentler than it, takes it off a steeper one, and
//                 stops a rolling ball without turning it round (Tomodachi #393)
//   PHYS-BODY-09  a ball already touching a wall, box or post (a resting
//                 contact leaves it within SLOP of the face) and then driven
//                 hard into it does not pass through it (Tomodachi #393)
//   PHYS-BODY-10  a ball starting near a box's corner, within the radius of
//                 both faces' planes but clear of the rounded corner, still
//                 hits the box (Tomodachi #393, a Marble Maze tunnel)
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

// A ball dropped onto two stacked floors; the upper one (a brick) is what the
// ball hits, until it is removed or disabled.
static void test_collider_ids() {
    printf("--- PHYS-BODY-07: collider ids in contacts, remove and setEnabled (#384) ---\n");
    ColliderSet set;
    const uint32_t brick = set.addAabb(-100, 100, 100, 104, 0.2f, ColliderKinds::Wall);
    const uint32_t floor = set.addSeg(-100, 140, 100, 140, 0.2f);
    const uint32_t post  = set.addCircle(300, 300, 4, 0.5f);
    ASSERT(brick != 0 && floor != 0 && post != 0, "addAabb/addSeg/addCircle return a non-zero id");
    ASSERT(brick != floor && floor != post && brick != post, "ids are distinct across kinds");

    auto dropOnto = [&](uint32_t& firstHitId, float& restY) {
        BodyState b;
        b.x = 0; b.y = 80; b.radius = 4; b.restitution = 0.2f;
        firstHitId = 0;
        run(b, set, 0.0f, 320.0f, 60, 4, true, [&](int, const std::vector<Contact>& cs) {
            if (firstHitId == 0 && !cs.empty()) firstHitId = cs.front().id;
        });
        restY = b.y;
    };

    uint32_t hit = 0;
    float restY = 0;
    dropOnto(hit, restY);
    ASSERT(hit == brick, "the contact carries the id of the collider hit (the brick)");
    ASSERT(std::fabs(restY - 96.0f) < 1.0f, "the ball rests on the brick");

    ASSERT(set.setEnabled(brick, false), "setEnabled(id, false) finds the brick");
    dropOnto(hit, restY);
    ASSERT(hit == floor, "a disabled brick is passed through: the floor is hit");
    ASSERT(std::fabs(restY - 136.0f) < 1.0f, "the ball rests on the floor below the disabled brick");
    ASSERT(set.count() == 3, "a disabled collider is still in the set");

    ASSERT(set.setEnabled(brick, true), "setEnabled(id, true) brings it back");
    dropOnto(hit, restY);
    ASSERT(hit == brick, "the re-enabled brick is hit again");

    ASSERT(set.remove(brick), "remove(id) finds the brick");
    ASSERT(!set.remove(brick), "a second remove of the same id finds nothing");
    ASSERT(!set.setEnabled(brick, true), "setEnabled of a removed id finds nothing");
    ASSERT(set.count() == 2, "remove takes the collider out of the set");
    dropOnto(hit, restY);
    ASSERT(hit == floor, "after remove the floor is hit");

    // Ids are never reused, even after clear(): a stale id cannot hit a new collider.
    set.clear();
    const uint32_t fresh = set.addAabb(0, 0, 1, 1);
    ASSERT(fresh != brick && fresh != floor && fresh != post, "ids are not reused after clear()");
    ASSERT(!set.remove(floor), "a stale id from before clear() removes nothing");
    ASSERT(set.count() == 1, "the new collider survived the stale remove");

    // addSolidRects reports the ids it gave out.
    std::vector<uint32_t> ids;
    std::vector<Rect> rects(2);
    rects[0].x = 0;  rects[0].y = 0; rects[0].width = 16; rects[0].height = 16;
    rects[1].x = 32; rects[1].y = 0; rects[1].width = 16; rects[1].height = 16;
    const size_t n = set.addSolidRects(rects, 0.1f, ColliderKinds::Wall, &ids);
    ASSERT(n == 2 && ids.size() == 2, "addSolidRects hands back one id per rect");
    ASSERT(ids.size() == 2 && ids[0] != ids[1] && ids[0] != fresh, "solid-rect ids are distinct");
    ASSERT(ids.size() == 2 && set.remove(ids[1]) && set.count() == 2,
           "a solid rect can be removed by id");

    // Flipper contacts carry id 0: a flipper is addressed by its index.
    BodyState b;
    b.x = 65; b.y = 146; b.radius = 4; b.restitution = 0.2f;
    ColliderSet fs = makeFlipperSet();
    bool sawFlipper = false, flipperIdZero = true;
    run(b, fs, 0.0f, 320.0f, 30, 4, true, [&](int, const std::vector<Contact>& cs) {
        for (const auto& c : cs) {
            if (c.kind != ColliderKinds::Flipper) continue;
            sawFlipper = true;
            if (c.id != 0) flipperIdZero = false;
        }
    });
    ASSERT(sawFlipper && flipperIdZero, "a flipper's contacts carry id 0");
}

static void test_rolling_resistance() {
    printf("--- PHYS-BODY-08: rolling resistance (N=1..8) ---\n");
    ColliderSet open;  // no colliders: an open, level floor
    for (int N : {1, 4, 8}) {
        char msg[160];
        // A pull of 90 px/s² against a resistance of 100: the ball never moves,
        // not even by a creep, over 10 s.
        {
            BodyState b;
            b.x = 10; b.y = 10; b.radius = 2; b.rollingResistance = 100.0f;
            run(b, open, 90.0f, 0.0f, 300, N, true, [](int, const std::vector<Contact>&) {});
            snprintf(msg, sizeof(msg), "N=%d a pull under the resistance holds the ball (x=%.6f, vx=%.6f)",
                     N, b.x, b.vx);
            ASSERT(b.x == 10.0f && b.vx == 0.0f && b.y == 10.0f, msg);
        }
        // A pull of 300 against 100 rolls it at the difference: 200 px/s², so
        // 1 s from rest is 200 px/s and 100 px (s = a t² / 2).
        {
            BodyState b;
            b.x = 0; b.y = 0; b.radius = 2; b.rollingResistance = 100.0f;
            run(b, open, 0.0f, 300.0f, 30, N, true, [](int, const std::vector<Contact>&) {});
            snprintf(msg, sizeof(msg), "N=%d a steeper pull rolls at its excess (vy=%.3f, y=%.3f)", N, b.vy, b.y);
            ASSERT(std::fabs(b.vy - 200.0f) < 0.5f && std::fabs(b.y - 100.0f) < 200.0f / 30 / N + 0.5f, msg);
            ASSERT(b.vx == 0.0f, "the resistance has no sideways part");
        }
        // Rolling at 120 px/s on the level, against 60 px/s²: it stops after 2 s
        // and 120 px (v² / 2a), and stays stopped rather than rolling back.
        {
            BodyState b;
            b.x = 0; b.y = 0; b.vx = 72.0f; b.vy = 96.0f; b.radius = 2; b.rollingResistance = 60.0f;
            run(b, open, 0.0f, 0.0f, 90, N, true, [](int, const std::vector<Contact>&) {});
            const float d = std::hypot(b.x, b.y);
            snprintf(msg, sizeof(msg), "N=%d a rolling ball stops in v²/2a (d=%.3f, v=%.4f)", N, d,
                     std::hypot(b.vx, b.vy));
            ASSERT(b.vx == 0.0f && b.vy == 0.0f, msg);
            ASSERT(std::fabs(d - 120.0f) < 120.0f / 30 / N + 0.5f, msg);
            ASSERT(std::fabs(b.y / b.x - 96.0f / 72.0f) < 1e-3f, "it slows along its own heading");
        }
    }
    // 0 (the default) is no resistance: a body left alone keeps its speed.
    BodyState b;
    b.vx = 50.0f;
    ASSERT(b.rollingResistance == 0.0f, "rolling resistance defaults to 0");
    run(b, open, 0.0f, 0.0f, 30, 4, true, [](int, const std::vector<Contact>&) {});
    ASSERT(b.vx == 50.0f, "no resistance, no slowing");

    // The component carries it.
    Object o;
    C_Body* body = o.addComponent<C_Body>();
    body->setRollingResistance(42.0f);
    ASSERT(body->getRollingResistance() == 42.0f && body->state().rollingResistance == 42.0f,
           "C_Body sets and reads its rolling resistance");
}

static void test_touching_then_driven_in() {
    printf("--- PHYS-BODY-09: touching, then driven hard into the same collider ---\n");
    // Each case: the ball overlaps the collider by `overlap` px (0.004: inside
    // the depenetration SLOP, so a resting contact can leave it there; 0: just
    // touching), then moves 16.7 px in one substep, mostly into the collider.
    for (float overlap : {0.0f, 0.004f}) {
        char msg[160];
        {   // an 8 px box: the far face is closer than the travel
            BodyState b;
            b.x = 8.0f + overlap; b.y = 0; b.radius = 2; b.restitution = 0.3f;
            b.vx = 2000.0f; b.vy = 300.0f;
            ColliderSet set;
            set.addAabb(10, -50, 18, 50, 0.1f);
            std::vector<Contact> cs;
            StepStats st;
            stepFrame(b, set, 0.0f, 0.0f, 1.0f / 120, 1, true, cs, st);
            snprintf(msg, sizeof(msg), "overlap %.3f: the box keeps the ball on its side (x=%.3f)", overlap, b.x);
            ASSERT(b.x <= 8.0f + 0.01f, msg);
            ASSERT(!cs.empty(), "the push into the box is a contact");
        }
        {   // a box's corner region, driven in diagonally
            BodyState b;
            const float k = (2.0f - overlap) / std::sqrt(2.0f);
            b.x = 10.0f - k; b.y = -50.0f - k; b.radius = 2; b.restitution = 0.3f;
            b.vx = 1500.0f; b.vy = 1500.0f;
            ColliderSet set;
            set.addAabb(10, -50, 18, 50, 0.1f);
            std::vector<Contact> cs;
            StepStats st;
            stepFrame(b, set, 0.0f, 0.0f, 1.0f / 120, 1, true, cs, st);
            const bool inside = b.x > 10.0f - 2.0f + 0.05f && b.y > -50.0f - 2.0f + 0.05f &&
                                b.x < 18.0f + 2.0f && b.y < 50.0f + 2.0f &&
                                !(b.x < 10.0f && b.y < -50.0f &&
                                  std::hypot(10.0f - b.x, -50.0f - b.y) >= 2.0f - 0.05f);
            snprintf(msg, sizeof(msg), "overlap %.3f: driven into a corner, not through it (%.3f, %.3f)",
                     overlap, b.x, b.y);
            ASSERT(!inside && b.x < 18.0f, msg);
        }
        {   // a thin segment wall
            BodyState b;
            b.x = 8.0f + overlap; b.y = 0; b.radius = 2; b.restitution = 0.3f;
            b.vx = 2000.0f; b.vy = 300.0f;
            ColliderSet set;
            set.addSeg(10, -50, 10, 50, 0.1f);
            std::vector<Contact> cs;
            StepStats st;
            stepFrame(b, set, 0.0f, 0.0f, 1.0f / 120, 1, true, cs, st);
            snprintf(msg, sizeof(msg), "overlap %.3f: the segment keeps the ball on its side (x=%.3f)", overlap, b.x);
            ASSERT(b.x <= 8.0f + 0.01f, msg);
        }
        {   // a post
            BodyState b;
            b.x = -(5.0f - overlap); b.y = 0; b.radius = 2; b.restitution = 0.3f;
            b.vx = 2000.0f; b.vy = 0.0f;
            ColliderSet set;
            set.addCircle(0, 0, 3, 0.1f);
            std::vector<Contact> cs;
            StepStats st;
            stepFrame(b, set, 0.0f, 0.0f, 1.0f / 120, 1, true, cs, st);
            snprintf(msg, sizeof(msg), "overlap %.3f: the post keeps the ball on its side (x=%.3f)", overlap, b.x);
            ASSERT(b.x <= -5.0f + 0.01f, msg);
        }
    }
    // Touching and moving away, or along the face, is free: no contact.
    {
        BodyState b;
        b.x = 8.0f; b.y = 0; b.radius = 2; b.vx = 0.0f; b.vy = 600.0f;
        ColliderSet set;
        set.addAabb(10, -50, 18, 50, 0.1f);
        std::vector<Contact> cs;
        StepStats st;
        stepFrame(b, set, 0.0f, 0.0f, 1.0f / 120, 1, true, cs, st);
        ASSERT(std::fabs(b.y - 5.0f) < 1e-3f && std::fabs(b.vy - 600.0f) < 1e-3f && cs.empty(),
               "sliding along a touched face keeps the whole move and makes no contact");
    }
}

static void test_corner_region_start() {
    printf("--- PHYS-BODY-10: a start in a box's corner region ---\n");
    // Marble Maze, stage 3: the ball at (46.06, 39.34) skims an 8 px wall
    // tile's top-left corner at (48, 40): 2.05 px off it, so not touching,
    // yet inside the box grown by the radius. It moves (8.7, 2.25) px.
    BodyState b;
    b.x = 46.063f; b.y = 39.335f; b.radius = 2; b.restitution = 0.3f;
    b.vx = 694.3f; b.vy = 180.0f;
    ColliderSet set;
    set.addAabb(48, 40, 56, 48, 0.1f);
    std::vector<Contact> cs;
    StepStats st;
    stepFrame(b, set, 0.0f, 0.0f, 0.0125f, 1, true, cs, st);
    const float cx = std::max(48.0f, std::min(b.x, 56.0f));
    const float cy = std::max(40.0f, std::min(b.y, 48.0f));
    char msg[160];
    snprintf(msg, sizeof(msg), "the ball meets the corner instead of passing through the box (%.3f, %.3f)",
             b.x, b.y);
    ASSERT(std::hypot(b.x - cx, b.y - cy) >= 2.0f - 0.05f && b.x < 50.0f, msg);
    ASSERT(!cs.empty(), "the corner hit is a contact");
}

int main() {
    test_rest_no_tunnel();
    test_swept_vs_naive();
    test_flipper_launch();
    test_slide_and_contact_point();
    test_component_contact_poll();
    test_flipper_moving_away();
    test_collider_ids();
    test_rolling_resistance();
    test_touching_then_driven_in();
    test_corner_region_start();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
