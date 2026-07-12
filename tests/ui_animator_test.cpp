// Keyframe animator test (Phase 3a, #121): C_PositionAnimator / C_ParameterAnimator
// / C_KeyframeAnimator collapsed into one data-only AnimatorComponent<T> + an
// AnimatorSystem that only advances the clock.
//
// The interpolation and the timeline are pure seams pinned without a world: value
// holds at the endpoints, eases across a segment with the destination keyframe's
// curve, and the clock clamps (or wraps, when looping) at the end. A final pass
// runs the AnimatorSystem over a real world to prove it ticks every animator.
#include <enjin2/ui/animator.hpp>
#include <enjin2/ui/easing.hpp>
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/core/types.hpp>
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

// A scalar timeline holds its endpoints and eases linearly across the middle. The
// clock never reads past the last value, and the value() seam is pure (no clock
// advance of its own).
static void test_scalar_endpoints_and_midpoint() {
    AnimatorComponent<float> anim;
    anim.addKeyframe({0, 0.0f, &Easing::Linear});
    anim.addKeyframe({100, 10.0f, &Easing::Linear});

    ASSERT(nearly(anim.value(), 0.0f), "animator: holds first value before playing");

    anim.play();
    anim.advance(50.0f);
    ASSERT(nearly(anim.value(), 5.0f), "animator: linear midpoint is halfway");

    anim.advance(100.0f); // overshoot past the end
    ASSERT(nearly(anim.value(), 10.0f), "animator: clamps to the last value");
    ASSERT(anim.finished(), "animator: latches finished at the end");
    ASSERT(!anim.playing(), "animator: stops playing once finished");
}

// Keyframes arrive out of order but the timeline sorts them, and the destination
// keyframe's easing curve (not the source's) shapes each segment.
static void test_sorts_and_uses_destination_easing() {
    AnimatorComponent<float> anim;
    anim.addKeyframe({100, 10.0f, &Easing::EaseInQuad}); // added first, later in time
    anim.addKeyframe({0, 0.0f, &Easing::Linear});

    anim.play();
    anim.advance(50.0f);
    // EaseInQuad(0.5) = 0.25 -> value = 0 + 0.25*(10-0) = 2.5, proving the segment
    // took the *destination* keyframe's curve, and that sorting placed t=0 first.
    ASSERT(nearly(anim.value(), 2.5f), "animator: destination easing shapes the segment");
}

// Vec2 timelines interpolate componentwise through the same machinery (the old
// C_PositionAnimator case).
static void test_vec2_interpolates() {
    AnimatorComponent<Vec2> anim;
    anim.addKeyframe({0, Vec2(0.0f, 0.0f), &Easing::Linear});
    anim.addKeyframe({200, Vec2(20.0f, -40.0f), &Easing::Linear});

    anim.play();
    anim.advance(100.0f);
    Vec2 v = anim.value();
    ASSERT(nearly(v.x, 10.0f) && nearly(v.y, -20.0f), "animator: Vec2 blends componentwise");
}

// retargetStart rebases keyframe 0 so the timeline eases out of the host's current
// value instead of snapping (the old StartAnimation getter behaviour).
static void test_retarget_start() {
    AnimatorComponent<float> anim;
    anim.addKeyframe({0, 0.0f, &Easing::Linear});
    anim.addKeyframe({100, 10.0f, &Easing::Linear});

    anim.retargetStart(4.0f);
    anim.play();
    ASSERT(nearly(anim.value(), 4.0f), "animator: retargetStart rebases keyframe 0");
    anim.advance(50.0f);
    ASSERT(nearly(anim.value(), 7.0f), "animator: eases from the rebased start");
}

// A looping timeline wraps the clock and carries the overshoot instead of latching
// finished.
static void test_looping_wraps() {
    AnimatorComponent<float> anim;
    anim.addKeyframe({0, 0.0f, &Easing::Linear});
    anim.addKeyframe({100, 10.0f, &Easing::Linear});

    anim.play(/*loops*/ 1);
    anim.advance(120.0f); // 20ms past the end wraps into the second loop
    ASSERT(anim.playing(), "animator: still playing while loops remain");
    ASSERT(nearly(anim.value(), 2.0f), "animator: carried overshoot into the next loop");

    anim.advance(120.0f); // spends the last loop
    ASSERT(!anim.playing(), "animator: stops once the loop budget is spent");
}

// The AnimatorSystem is only a clock: it advances every animator in the world each
// frame, leaving the value application to the host.
static void test_system_ticks_all() {
    using AnimWorld = World<8, AnimatorComponent<float>>;
    AnimWorld world;
    AnimatorSystem<AnimWorld, float> system(&world);

    Entity a = world.create();
    Entity b = world.create();
    for (Entity e : {a, b}) {
        auto* anim = world.add<AnimatorComponent<float>>(e);
        anim->addKeyframe({0, 0.0f, &Easing::Linear});
        anim->addKeyframe({1000, 100.0f, &Easing::Linear});
        anim->play();
    }

    system.update(0.5f); // 500ms -> halfway
    ASSERT(nearly(world.get<AnimatorComponent<float>>(a)->value(), 50.0f),
           "animator: system advanced the first animator");
    ASSERT(nearly(world.get<AnimatorComponent<float>>(b)->value(), 50.0f),
           "animator: system advanced the second animator");
}

int main() {
    test_scalar_endpoints_and_midpoint();
    test_sorts_and_uses_destination_easing();
    test_vec2_interpolates();
    test_retarget_start();
    test_looping_wraps();
    test_system_ticks_all();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
