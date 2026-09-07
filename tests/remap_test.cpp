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

int main()
{
    printf("remap_test\n");
    printf("==========\n");

    test_default_is_identity();
    test_identity_factory();
    test_solid();
    test_apply_masks_low_nibble();
    test_constexpr_usable();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
