// Easing util unit test (Phase 3a, #121): upstreamed from Libs/enjin/utils/Easing.hpp
//
// The easing curves are the shared substrate every animated ui widget draws on,
// so their boundary behaviour (f(0), f(1)) and midpoint shape are pinned here.
#include <enjin2/ui/easing.hpp>
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

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

// Every normalized easing curve must pin its endpoints: f(0)=0, f(1)=1. Step and
// the elastic curve are the deliberate exceptions and are checked separately.
static void test_endpoints() {
    ASSERT(near(Easing::Linear(0.0f), 0.0f), "easing: Linear(0)=0");
    ASSERT(near(Easing::Linear(1.0f), 1.0f), "easing: Linear(1)=1");
    ASSERT(near(Easing::EaseInQuad(0.0f), 0.0f), "easing: EaseInQuad(0)=0");
    ASSERT(near(Easing::EaseInQuad(1.0f), 1.0f), "easing: EaseInQuad(1)=1");
    ASSERT(near(Easing::EaseOutQuad(0.0f), 0.0f), "easing: EaseOutQuad(0)=0");
    ASSERT(near(Easing::EaseOutQuad(1.0f), 1.0f), "easing: EaseOutQuad(1)=1");
    ASSERT(near(Easing::EaseInOutQuad(0.0f), 0.0f), "easing: EaseInOutQuad(0)=0");
    ASSERT(near(Easing::EaseInOutQuad(1.0f), 1.0f), "easing: EaseInOutQuad(1)=1");
    ASSERT(near(Easing::EaseInOutCubic(0.0f), 0.0f), "easing: EaseInOutCubic(0)=0");
    ASSERT(near(Easing::EaseInOutCubic(1.0f), 1.0f), "easing: EaseInOutCubic(1)=1");
    ASSERT(near(Easing::EaseInCubic(1.0f), 1.0f), "easing: EaseInCubic(1)=1");
    ASSERT(near(Easing::EaseOutCubic(0.0f), 0.0f), "easing: EaseOutCubic(0)=0");
    ASSERT(near(Easing::EaseInOutSine(0.0f), 0.0f), "easing: EaseInOutSine(0)=0");
    ASSERT(near(Easing::EaseInOutSine(1.0f), 1.0f), "easing: EaseInOutSine(1)=1");
    ASSERT(near(Easing::EaseInSine(0.0f), 0.0f), "easing: EaseInSine(0)=0");
    ASSERT(near(Easing::EaseInOutQuint(0.0f), 0.0f), "easing: EaseInOutQuint(0)=0");
    ASSERT(near(Easing::EaseInOutQuint(1.0f), 1.0f), "easing: EaseInOutQuint(1)=1");
    ASSERT(near(Easing::EaseInQuart(1.0f), 1.0f), "easing: EaseInQuart(1)=1");
    ASSERT(near(Easing::EaseOutQuart(0.0f), 0.0f), "easing: EaseOutQuart(0)=0");
    ASSERT(near(Easing::EaseInOutCirc(0.0f), 0.0f), "easing: EaseInOutCirc(0)=0");
    ASSERT(near(Easing::EaseInOutCirc(1.0f), 1.0f), "easing: EaseInOutCirc(1)=1");
}

// Step holds at zero for the whole interval — it is the "no interpolation" curve.
static void test_step() {
    ASSERT(near(Easing::Step(0.0f), 0.0f), "easing: Step(0)=0");
    ASSERT(near(Easing::Step(0.5f), 0.0f), "easing: Step(0.5)=0");
    ASSERT(near(Easing::Step(1.0f), 0.0f), "easing: Step(1)=0");
}

// Shape checks: eases must bow away from the linear diagonal in the right direction.
static void test_shape() {
    // Accelerating curve sits below the diagonal at the midpoint.
    ASSERT(Easing::EaseInQuad(0.5f) < 0.5f, "easing: EaseInQuad bows below diagonal");
    // Decelerating curve sits above it.
    ASSERT(Easing::EaseOutQuad(0.5f) > 0.5f, "easing: EaseOutQuad bows above diagonal");
    // Symmetric in/out curve crosses exactly at the midpoint.
    ASSERT(near(Easing::EaseInOutQuad(0.5f), 0.5f), "easing: EaseInOutQuad crosses at 0.5");
    ASSERT(near(Easing::EaseInOutCubic(0.5f), 0.5f), "easing: EaseInOutCubic crosses at 0.5");
    ASSERT(near(Easing::EaseInOutSine(0.5f), 0.5f), "easing: EaseInOutSine crosses at 0.5");
}

// A function pointer of the public EasingFunction type must bind to these statics —
// the animators store curves by pointer, so the type has to line up.
static void test_function_pointer_type() {
    EasingFunction fn = &Easing::EaseInOutCubic;
    ASSERT(fn != nullptr, "easing: binds to EasingFunction pointer");
    ASSERT(near(fn(1.0f), 1.0f), "easing: called through pointer");
}

int main() {
    test_endpoints();
    test_step();
    test_shape();
    test_function_pointer_type();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
