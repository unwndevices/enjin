// Waiver machinery unit test (M3, unwn #166; design § 8 / unwn #159).
//
// The visual parity bench's REAL waiver table (tests/waivers.hpp) starts —
// and should stay — empty, so the machinery that makes waiving harder than
// fixing is proven here against synthetic tables instead: the mandatory
// sub-range predicate, the enforced expected-diff bound, the implementation
// pin's staleness trigger, retirement, signature clustering, and the > 50%
// census hard gate. This test IS registered with ctest: it checks the
// instrument, not the pairs, and must always be green.

#include "visual_parity/bench_support.hpp"

#include <cstdio>

using namespace parity;

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
    } while (0)

// A predicate over the case parameter list: covers r in [2, 4].
static bool coversR2to4(const Param *p, size_t n)
{
    const int32_t r = paramValue(p, n, "r");
    return r >= 2 && r <= 4;
}

static Waiver makeWaiver(uint64_t baseHash, uint64_t headHash)
{
    Waiver w{};
    w.pair_id = "guard.fillCircle";
    w.status = WaiverStatus::Active;
    w.subrange = "r in [2,4]";
    w.covers = coversR2to4;
    w.max_diff = 8;
    w.base_pin_file = "canvas.hpp";
    w.base_pin_hash = baseHash;
    w.head_pin_file = "primitives.hpp";
    w.head_pin_hash = headHash;
    w.ticket = "unwn#0000-test";
    w.author = "bench-test";
    w.date = "2026-08-02";
    return w;
}

// ============================================================
// a. The real table starts empty
// ============================================================
static void test_real_table_empty()
{
    printf("--- Real table ---\n");
    ASSERT(kWaivers.empty(),
           "tests/waivers.hpp table starts EMPTY (#161: restores removed every known divergence)");

    const Param p[] = {{"r", 3}};
    WaiverHit hit = evaluateWaiver(kWaivers.data(), kWaivers.size(),
                                   "guard.fillCircle", p, 1, 5);
    ASSERT(hit.verdict == WaiverVerdict::NotCovered,
           "empty table waives nothing");
}

// ============================================================
// b. Sub-range predicate + enforced bound
// ============================================================
static void test_predicate_and_bound()
{
    printf("--- Predicate + bound ---\n");
    const uint64_t baseHash = pinHash("canvas.hpp");
    const uint64_t headHash = pinHash("primitives.hpp");
    ASSERT(baseHash != 0 && headHash != 0,
           "pin hashing reads the real graphics headers (ENJIN_GRAPHICS_DIR)");

    Waiver w = makeWaiver(baseHash, headHash);

    const Param inRange[] = {{"cx", 63}, {"cy", 63}, {"r", 3}};
    const Param outOfRange[] = {{"cx", 63}, {"cy", 63}, {"r", 9}};

    WaiverHit hit = evaluateWaiver(&w, 1, "guard.fillCircle", inRange, 3, 5);
    ASSERT(hit.verdict == WaiverVerdict::Waived,
           "covered case within bound is Waived (memcmp==0 swapped for the bound)");

    hit = evaluateWaiver(&w, 1, "guard.fillCircle", inRange, 3, 8);
    ASSERT(hit.verdict == WaiverVerdict::Waived, "bound is inclusive");

    hit = evaluateWaiver(&w, 1, "guard.fillCircle", inRange, 3, 9);
    ASSERT(hit.verdict == WaiverVerdict::BoundExceeded,
           "a GROWN difference inside the waived sub-range is detected");

    hit = evaluateWaiver(&w, 1, "guard.fillCircle", outOfRange, 3, 1);
    ASSERT(hit.verdict == WaiverVerdict::NotCovered,
           "outside the sub-range stays byte-exact");

    hit = evaluateWaiver(&w, 1, "guard.drawCircle", inRange, 3, 1);
    ASSERT(hit.verdict == WaiverVerdict::NotCovered,
           "a waiver attaches to one pair, never a family");

    Waiver nullPredicate = w;
    nullPredicate.covers = nullptr;
    hit = evaluateWaiver(&nullPredicate, 1, "guard.fillCircle", inRange, 3, 1);
    ASSERT(hit.verdict == WaiverVerdict::NotCovered,
           "a null predicate covers NOTHING (no accidental whole-pair waiver)");
}

// ============================================================
// c. Implementation pin: either side changes -> stale
// ============================================================
static void test_stale_pin()
{
    printf("--- Implementation pin ---\n");
    const uint64_t baseHash = pinHash("canvas.hpp");
    const uint64_t headHash = pinHash("primitives.hpp");
    const Param p[] = {{"r", 3}};

    Waiver staleBase = makeWaiver(baseHash ^ 1, headHash);
    WaiverHit hit = evaluateWaiver(&staleBase, 1, "guard.fillCircle", p, 1, 1);
    ASSERT(hit.verdict == WaiverVerdict::StalePin,
           "BASE-side header change makes the waiver stale");

    Waiver staleHead = makeWaiver(baseHash, headHash ^ 1);
    hit = evaluateWaiver(&staleHead, 1, "guard.fillCircle", p, 1, 1);
    ASSERT(hit.verdict == WaiverVerdict::StalePin,
           "HEAD-side header change makes the waiver stale");

    Waiver missingFile = makeWaiver(baseHash, headHash);
    missingFile.base_pin_file = "no_such_header.hpp";
    hit = evaluateWaiver(&missingFile, 1, "guard.fillCircle", p, 1, 1);
    ASSERT(hit.verdict == WaiverVerdict::StalePin,
           "an unreadable pin file reads as stale (the safe direction)");
}

// ============================================================
// d. Bench integration: settleCase routes verdicts correctly
// ============================================================
static void test_settle_case()
{
    printf("--- settleCase routing ---\n");
    const uint64_t baseHash = pinHash("canvas.hpp");
    const uint64_t headHash = pinHash("primitives.hpp");
    static Waiver w = makeWaiver(baseHash, headHash);

    Bench b;
    b.waivers = &w;
    b.waiverCount = 1;
    b.beginPair({"guard.fillCircle", "TIER2 GUARD", "Canvas8::fillCircle",
                 "Primitives<Pixel4>::fillCircle"});

    b.settleCase({{"r", 1}}, true, 0, 0, "", nullptr, nullptr);
    ASSERT(b.cur->cases == 1 && b.cur->failed == 0, "equal case counts, no failure");

    b.settleCase({{"r", 3}}, false, AxisAlgo, 5, "5 px", nullptr, nullptr);
    ASSERT(b.cur->waived == 1 && b.cur->failed == 0,
           "in-range, in-bound failure is absorbed as waived");

    b.settleCase({{"r", 3}}, false, AxisAlgo, 20, "20 px", nullptr, nullptr);
    ASSERT(b.cur->boundExceeded == 1 && b.cur->failed == 1,
           "bound-exceeded case FAILS");
    ASSERT(!b.signatures.empty() &&
               (b.signatures.back().axisMask & AxisWaiverMeta) != 0,
           "bound-exceeded signature carries the waiver-meta axis");

    b.settleCase({{"r", 9}}, false, AxisAlgo, 2, "2 px", nullptr, nullptr);
    ASSERT(b.cur->failed == 2, "out-of-range failure stays a plain failure");

    static Waiver stale = makeWaiver(baseHash ^ 1, headHash);
    Bench b2;
    b2.waivers = &stale;
    b2.waiverCount = 1;
    b2.beginPair({"guard.fillCircle", "TIER2 GUARD", "", ""});
    b2.settleCase({{"r", 3}}, false, AxisAlgo, 2, "2 px", nullptr, nullptr);
    ASSERT(b2.cur->stalePin == 1 && b2.cur->failed == 1,
           "stale waiver reverts the sub-range to byte-exact and fails");
}

// ============================================================
// e. Signature clustering
// ============================================================
static void test_signature_clustering()
{
    printf("--- Signature clustering ---\n");
    Bench b; // empty real table
    b.waivers = nullptr;
    b.waiverCount = 0;
    b.beginPair({"geom.drawLine", "TIER1", "", ""});

    b.settleCase({{"x0", 5}, {"y0", 5}}, false, AxisAlgo, 10, "first", nullptr, nullptr);
    b.settleCase({{"x0", 90}, {"y0", 2}}, false, AxisAlgo, 30, "second", nullptr, nullptr);
    b.settleCase({{"x0", 7}, {"y0", 7}}, false, AxisDepth, 4, "third", nullptr, nullptr);

    ASSERT(b.signatures.size() == 2,
           "failures cluster to (pair, axis): 2 signatures from 3 failures");
    const Signature &s = b.signatures[0];
    ASSERT(s.cases == 2 && s.firstDetail == "first" && s.maxDiff == 30,
           "cluster keeps the first repro and the worst diff");
    ASSERT(s.paramRange.at("x0").first == 5 && s.paramRange.at("x0").second == 90,
           "failing parameter ranges accumulate per axis");
}

// ============================================================
// f. Census: > 50% waived fails the run; retirement is visible
// ============================================================
static void test_census_gate()
{
    printf("--- Census gate ---\n");
    Bench b;
    b.beginPair({"blit.gray8", "TIER1", "", ""});
    b.cur->cases = 10;
    b.cur->waived = 6;
    ASSERT(b.censusViolations().size() == 1,
           "a pair > 50% waived violates the census hard gate");

    b.cur->waived = 5;
    ASSERT(b.censusViolations().empty(), "exactly 50% does not trip the gate");

    static Waiver retired{};
    retired.pair_id = "blit.gray8";
    retired.status = WaiverStatus::Retired;
    retired.subrange = nullptr;
    retired.covers = nullptr;
    retired.max_diff = 0;
    retired.base_pin_file = "canvas.hpp";
    retired.base_pin_hash = 0;
    retired.head_pin_file = "blit.hpp";
    retired.head_pin_hash = 0;
    retired.ticket = "unwn#0000-test";
    retired.author = "bench-test";
    retired.date = "2026-08-02";

    Bench b2;
    b2.waivers = &retired;
    b2.waiverCount = 1;
    b2.beginPair({"blit.gray8", "TIER1", "", ""});
    ASSERT(b2.cur->retired, "a Retired entry marks the pair skipped");

    const Param p[] = {{"x", 1}};
    WaiverHit hit = evaluateWaiver(&retired, 1, "blit.gray8", p, 1, 1);
    ASSERT(hit.verdict == WaiverVerdict::NotCovered,
           "a Retired entry never waives a case (retirement is not a waiver)");
}

int main()
{
    printf("waiver_machinery_test — bench waiver/census machinery (unwn #166, design § 8)\n\n");

    test_real_table_empty();
    test_predicate_and_bound();
    test_stale_pin();
    test_settle_case();
    test_signature_clustering();
    test_census_gate();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
