#include <enjin2/graphics/remap.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

static void test_default_is_identity()
{
    printf("--- default is identity ---\n");
    Remap r;
    ASSERT(r.isIdentity(), "default-constructed Remap should be identity");
    for (uint8_t i = 0; i < 16; ++i) {
        ASSERT(r.apply(i) == i, "identity: apply(i) == i");
    }
}

static void test_identity_factory()
{
    printf("--- identity() factory ---\n");
    Remap r = Remap::identity();
    ASSERT(r.isIdentity(), "identity() should be identity");
    ASSERT(r.apply(7) == 7, "identity() apply(7) == 7");
}

static void test_solid()
{
    printf("--- solid() ---\n");
    Remap r = Remap::solid(3);
    ASSERT(!r.isIdentity(), "solid(3) is not identity");
    for (uint8_t i = 0; i < 15; ++i) {
        ASSERT(r.apply(i) == 3, "solid(3): 0-14 map to 3");
    }
    ASSERT(r.apply(15) == 15, "solid(3): entry 15 stays transparent (15)");
}

static void test_apply_masks_low_nibble()
{
    printf("--- apply masks to low nibble ---\n");
    Remap r = Remap::identity();
    // 0x13 -> low nibble 3 -> lut[3] == 3
    ASSERT(r.apply(0x13) == 3, "apply masks high bits: apply(0x13)==3");
}

static void test_constexpr_usable()
{
    printf("--- constexpr usability ---\n");
    constexpr Remap r = Remap::solid(5);
    static_assert(r.lut[0] == 5, "solid(5) constexpr lut[0]==5");
    static_assert(r.lut[15] == 15, "solid(5) constexpr lut[15]==15");
    static_assert(Remap::identity().isIdentity(), "identity() constexpr isIdentity");
    ASSERT(true, "constexpr usage compiled");
}

static void test_lighten()
{
    printf("--- lighten() ---\n");
    Remap r = Remap::lighten();
    // shade 1 (base) and 2 (dark) step toward light; shade 0 holds.
    ASSERT(r.apply(0) == 0, "lighten: ramp0 light holds");
    ASSERT(r.apply(1) == 0, "lighten: ramp0 base -> light");
    ASSERT(r.apply(2) == 1, "lighten: ramp0 dark -> base");
    ASSERT(r.apply(3) == 3, "lighten: ramp1 light holds");
    ASSERT(r.apply(14) == 13, "lighten: ramp4 dark -> base");
    ASSERT(r.apply(15) == 15, "lighten: transparency passes through");
}

static void test_darken()
{
    printf("--- darken() ---\n");
    Remap r = Remap::darken();
    ASSERT(r.apply(0) == 1, "darken: ramp0 light -> base");
    ASSERT(r.apply(1) == 2, "darken: ramp0 base -> dark");
    ASSERT(r.apply(2) == 2, "darken: ramp0 dark holds");
    ASSERT(r.apply(12) == 13, "darken: ramp4 light -> base");
    ASSERT(r.apply(14) == 14, "darken: ramp4 dark holds");
    ASSERT(r.apply(15) == 15, "darken: transparency passes through");
}

static void test_on_ramp()
{
    printf("--- onRamp() ---\n");
    // Lighten, but only ramp 3 (indices 9,10,11); everything else identity.
    Remap r = Remap::onRamp(3, Remap::lighten());
    ASSERT(r.apply(9) == 9, "onRamp(3,lighten): ramp3 light holds");
    ASSERT(r.apply(10) == 9, "onRamp(3,lighten): ramp3 base -> light");
    ASSERT(r.apply(11) == 10, "onRamp(3,lighten): ramp3 dark -> base");
    // Off-ramp indices are untouched even though lighten() would move them.
    ASSERT(r.apply(1) == 1, "onRamp(3,lighten): ramp0 base untouched");
    ASSERT(r.apply(14) == 14, "onRamp(3,lighten): ramp4 dark untouched");
    ASSERT(r.apply(15) == 15, "onRamp: transparency passes through");
}

static void test_ramp_ctors_constexpr()
{
    printf("--- ramp ctors constexpr ---\n");
    static_assert(Remap::lighten().lut[2] == 1, "lighten() constexpr");
    static_assert(Remap::darken().lut[0] == 1, "darken() constexpr");
    static_assert(Remap::onRamp(3, Remap::lighten()).lut[10] == 9, "onRamp() constexpr");
    static_assert(Remap::onRamp(3, Remap::lighten()).lut[1] == 1, "onRamp() off-ramp identity constexpr");
    ASSERT(true, "ramp ctors usable at compile time");
}

int main()
{
    printf("remap_test\n");
    printf("==========\n");

    test_default_is_identity();
    test_identity_factory();
    test_solid();
    test_apply_masks_low_nibble();
    test_constexpr_usable();
    test_lighten();
    test_darken();
    test_on_ramp();
    test_ramp_ctors_constexpr();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
