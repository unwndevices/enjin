// Fixed (Q16.16) arithmetic (Tomodachi #329, ADR-0016): the deterministic
// number type of the Shape path. Every expected value is a worked literal.

#include <enjin2/core/fixed.hpp>
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

static void testConstruction()
{
    ASSERT(Fixed::fromInt(3).raw == 3 * 65536, "fromInt(3) is 3 << 16");
    ASSERT(Fixed::fromInt(-2).raw == -131072, "fromInt(-2) is -2 << 16");
    // Q8.8 from the Shape pack widens exactly: 0x0180 = 1.5, -0x0040 = -0.25.
    ASSERT(Fixed::fromQ8_8(0x0180).raw == 98304, "fromQ8_8(1.5)");
    ASSERT(Fixed::fromQ8_8(-0x0040).raw == -16384, "fromQ8_8(-0.25)");
    ASSERT(Fixed::fromQ8_8(-32768).raw == -128 * 65536, "fromQ8_8 min is -128 px");
    ASSERT(Fixed::fromRatio(1, 2).raw == 32768, "fromRatio(1, 2) = 0.5");
    ASSERT(Fixed::fromRatio(-1, 4).raw == -16384, "fromRatio(-1, 4) = -0.25");
    static_assert(Fixed::fromInt(1).raw == Fixed::kOne, "constexpr fromInt");
}

static void testArithmetic()
{
    const Fixed a = Fixed::fromRatio(3, 2);   // 1.5
    const Fixed b = Fixed::fromRatio(-1, 4);  // -0.25
    ASSERT((a + b).raw == 81920, "1.5 + -0.25 = 1.25");
    ASSERT((a - b).raw == 114688, "1.5 - -0.25 = 1.75");
    ASSERT((-a).raw == -98304, "-(1.5)");
    ASSERT((a * b).raw == -24576, "1.5 * -0.25 = -0.375");
    ASSERT((a / b).raw == -6 * 65536, "1.5 / -0.25 = -6");
    // Products round toward -inf (arithmetic shift), quotients toward zero.
    ASSERT((Fixed::fromRaw(-1) * Fixed::fromRaw(1)).raw == -1, "mul floors");
    ASSERT((Fixed::fromRaw(1) * Fixed::fromRaw(1)).raw == 0, "mul floors (positive)");
    ASSERT((Fixed::fromInt(1) / Fixed::fromInt(3)).raw == 21845, "1 / 3 truncates");
    ASSERT((Fixed::fromInt(-1) / Fixed::fromInt(3)).raw == -21845, "-1 / 3 truncates toward 0");
    // A product past 32 bits in the intermediate stays exact: 150 * 200.
    ASSERT((Fixed::fromInt(150) * Fixed::fromInt(200)).raw == 30000 * 65536, "wide product");

    Fixed c = a;
    c += b;
    ASSERT(c.raw == 81920, "+=");
    c -= a;
    ASSERT(c == b, "-= and ==");
    ASSERT(b < a && a > b && b <= b && a >= a && a != b, "ordering");
}

static void testRounding()
{
    const Fixed m = Fixed::fromRatio(-3, 2);  // -1.5
    ASSERT(m.floorToInt() == -2, "floor(-1.5) = -2");
    ASSERT(m.ceilToInt() == -1, "ceil(-1.5) = -1");
    ASSERT(m.roundToInt() == -1, "round(-1.5) = -1 (half up)");
    const Fixed p = Fixed::fromRatio(5, 2);   // 2.5
    ASSERT(p.floorToInt() == 2 && p.ceilToInt() == 3 && p.roundToInt() == 3, "2.5 floor/ceil/round");
    ASSERT(Fixed::fromInt(7).ceilToInt() == 7, "ceil of an integer is itself");
}

static void testVec2()
{
    const FixedVec2 v{Fixed::fromInt(1), Fixed::fromRatio(1, 2)};
    const FixedVec2 w{Fixed::fromInt(2), Fixed::fromRatio(-1, 2)};
    const FixedVec2 s = v + w;
    ASSERT(s.x.raw == 3 * 65536 && s.y.raw == 0, "vec2 +");
    const FixedVec2 d = v - w;
    ASSERT(d.x.raw == -65536 && d.y.raw == 65536, "vec2 -");
    ASSERT(v == v && v != w, "vec2 ==");
}

int main()
{
    testConstruction();
    testArithmetic();
    testRounding();
    testVec2();
    printf("fixed_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
