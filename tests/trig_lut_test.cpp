// trig_lut_test.cpp — TrigLUT fine-angle sine/cosine (Tomodachi #325)
//
// The vector pet's sway Features rotate in fixed point (ADR-0016), so the
// trig path must be integer-deterministic and accurate enough that a ~20 px
// lever arm settles without stair-stepping. Expected values come from
// std::sin in double precision, an independent source of truth.
#include <enjin2/core/math.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using enjin2::math::TrigLUT;

#define ASSERT(cond, msg) \
    do { if (!(cond)) { std::printf("FAIL: %s (line %d)\n", msg, __LINE__); std::exit(1); } } while (0)

static constexpr double kTwoPi = 6.283185307179586476925286766559;

// Exact Q15 reference for an angle in 65536ths of a turn.
static double refSin(uint32_t angle) {
    return 32767.0 * std::sin(static_cast<double>(angle) * kTwoPi / 65536.0);
}

static void testSin16MaxErrorOverTheTurn() {
    double worst = 0.0;
    uint32_t worstAngle = 0;
    for (uint32_t a = 0; a < 65536; ++a) {
        double err = std::fabs(TrigLUT::sin16(static_cast<uint16_t>(a)) - refSin(a));
        if (err > worst) { worst = err; worstAngle = a; }
    }
    std::printf("  sin16 max |error| = %.4f LSB at angle %u\n", worst, worstAngle);
    ASSERT(worst <= TrigLUT::kMaxErrorLsb, "sin16 within documented error bound over the full turn");
    std::printf("  PASS: sin16 max error over the turn\n");
}

static void testCos16MaxErrorOverTheTurn() {
    double worst = 0.0;
    for (uint32_t a = 0; a < 65536; ++a) {
        double ref = 32767.0 * std::cos(static_cast<double>(a) * kTwoPi / 65536.0);
        double err = std::fabs(TrigLUT::cos16(static_cast<uint16_t>(a)) - ref);
        if (err > worst) worst = err;
    }
    std::printf("  cos16 max |error| = %.4f LSB\n", worst);
    ASSERT(worst <= TrigLUT::kMaxErrorLsb, "cos16 within documented error bound over the full turn");
    std::printf("  PASS: cos16 max error over the turn\n");
}

// Legacy 256-step API: same accuracy at its own sample points, so callers
// that predate the fine table see no regression (old table truncated, so it
// was up to ~1 LSB off; the new one rounds).
static void testLegacy256StepApiStaysAccurate() {
    for (uint32_t k = 0; k < 256; ++k) {
        double refS = 32767.0 * std::sin(static_cast<double>(k) * kTwoPi / 256.0);
        double refC = 32767.0 * std::cos(static_cast<double>(k) * kTwoPi / 256.0);
        ASSERT(std::fabs(TrigLUT::sin(static_cast<uint16_t>(k)) - refS) <= 0.5, "legacy sin rounds to nearest");
        ASSERT(std::fabs(TrigLUT::cos(static_cast<uint16_t>(k)) - refC) <= 0.5, "legacy cos rounds to nearest");
    }
    // Wraps like before: 256ths of a turn, upper bits ignored.
    ASSERT(TrigLUT::sin(256 + 64) == 32767, "legacy sin wraps at 256");
    std::printf("  PASS: legacy 256-step sin/cos\n");
}

// A sway swings symmetrically about its rest shape and must settle smoothly:
// sin16 is exactly odd, hits the cardinal values exactly, and never steps
// backwards across the rising half (-90°..+90°), so the tail of a settling
// sway cannot jitter.
static void testSin16IsOddExactAndMonotonic() {
    ASSERT(TrigLUT::sin16(0x0000) == 0, "sin16(0) == 0");
    ASSERT(TrigLUT::sin16(0x4000) == 32767, "sin16(quarter) == 32767");
    ASSERT(TrigLUT::sin16(0x8000) == 0, "sin16(half) == 0");
    ASSERT(TrigLUT::sin16(0xC000) == -32767, "sin16(three quarters) == -32767");
    ASSERT(TrigLUT::cos16(0x0000) == 32767, "cos16(0) == 32767");

    for (uint32_t a = 0; a < 65536; ++a) {
        uint16_t neg = static_cast<uint16_t>(0x10000u - a);
        ASSERT(TrigLUT::sin16(neg) == -TrigLUT::sin16(static_cast<uint16_t>(a)), "sin16 is odd");
    }

    int prev = TrigLUT::sin16(0xC000);
    for (uint32_t k = 1; k <= 0x8000; ++k) {
        int cur = TrigLUT::sin16(static_cast<uint16_t>(0xC000u + k));
        ASSERT(cur >= prev, "sin16 non-decreasing from -quarter to +quarter");
        prev = cur;
    }
    std::printf("  PASS: sin16 odd, exact at cardinals, monotonic\n");
}

int main() {
    std::printf("=== trig_lut_test ===\n");
    testSin16MaxErrorOverTheTurn();
    testCos16MaxErrorOverTheTurn();
    testLegacy256StepApiStaysAccurate();
    testSin16IsOddExactAndMonotonic();
    std::printf("=== trig_lut_test: all passed ===\n");
    return 0;
}
