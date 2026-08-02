#ifndef ENJIN2_TESTS_VISUAL_PARITY_BENCH_SUPPORT_HPP
#define ENJIN2_TESTS_VISUAL_PARITY_BENCH_SUPPORT_HPP

// Visual Parity Bench — shared machinery (M3, unwn #166; design §§ 4-9).
//
// Everything the sweep needs that is not a pair definition lives here so the
// sweeps in visual_parity_bench.cpp contain zero waiver/reporting logic and
// the waiver machinery is unit-testable (tests/waiver_machinery_test.cpp)
// against synthetic tables while the real table (tests/waivers.hpp) stays
// empty.
//
//   * comparison planes  — BASE = Canvas8 buffer & 0x0F, HEAD = Canvas4
//     unpacked; byte-exact memcmp (§§ 5-6)
//   * pin hashing        — FNV-1a-64 of the implementing graphics headers,
//     consulted by waiver staleness checks
//   * waiver evaluation  — a failing case is waived only by an Active entry
//     whose predicate covers it, whose pins are fresh, and whose enforced
//     bound holds; anything else stays a failure
//   * signatures         — failures cluster to (pair, divergence axis); one
//     row per signature with the first repro's parameters, the per-parameter
//     failing ranges and the case count behind it (§ 7: the budget caps
//     distinct signatures ~20/run, not cases)
//   * census             — per-pair waived fraction; > 50% waived fails the
//     run (§ 8)
//   * review path        — BASE | HEAD | diff triptych PNGs, x4 nearest
//     neighbour, diff in magenta; written only on --png DIR; advisory,
//     never a gate (§ 9)

#include <enjin2/graphics/canvas.hpp>

#include "../waivers.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace parity
{

    // Eisei's shipped geometry on BOTH branches: EiseiCanvas is 127x127
    // (BASE Canvas8<127,127>, HEAD Canvas4<127,127> post the #155 restore).
    // The odd width is load-bearing — the substrate sweep is the only one
    // that can catch odd-width row-packing aliasing in Canvas4.
    constexpr int16_t W = 127;
    constexpr int16_t H = 127;
    constexpr size_t PLANE = static_cast<size_t>(W) * H;

    using Base8 = enjin2::Canvas8<W, H>;
    using Head4 = enjin2::Canvas4<W, H>;

    // --- comparison planes (design §§ 5-6) --------------------------------

    inline void maskPlane(const Base8 &c, uint8_t *out)
    {
        const uint8_t *buf = c.getBuffer();
        for (size_t i = 0; i < PLANE; ++i)
            out[i] = buf[i] & 0x0F;
    }

    inline void unpackPlane(const Head4 &c, uint8_t *out)
    {
        for (int16_t y = 0; y < H; ++y)
            for (int16_t x = 0; x < W; ++x)
                out[static_cast<size_t>(y) * W + x] = c.getPixel(x, y).value;
    }

    struct PlaneDiff
    {
        bool equal = true;
        size_t count = 0;   // number of differing bytes
        int16_t fx = -1, fy = -1; // first differing pixel
        uint8_t lhs = 0, rhs = 0;
    };

    inline PlaneDiff diffPlanes(const uint8_t *a, const uint8_t *b)
    {
        PlaneDiff d;
        if (memcmp(a, b, PLANE) == 0)
            return d; // the match rule: byte-exact memcmp, no tolerance
        d.equal = false;
        for (size_t i = 0; i < PLANE; ++i)
        {
            if (a[i] != b[i])
            {
                if (d.count == 0)
                {
                    d.fx = static_cast<int16_t>(i % W);
                    d.fy = static_cast<int16_t>(i / W);
                    d.lhs = a[i];
                    d.rhs = b[i];
                }
                d.count++;
            }
        }
        return d;
    }

    // --- implementation pins (design § 8) ---------------------------------

    inline uint64_t fnv1a64(const uint8_t *data, size_t n)
    {
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < n; ++i)
        {
            h ^= data[i];
            h *= 1099511628211ull;
        }
        return h;
    }

#ifndef ENJIN_GRAPHICS_DIR
#define ENJIN_GRAPHICS_DIR "."
#endif

    // Hash of one graphics header (file name relative to the enjin graphics
    // include dir). 0 = unreadable, which can never match a stored pin, so a
    // missing file reads as STALE — the safe direction.
    inline uint64_t pinHash(const char *headerName)
    {
        static std::map<std::string, uint64_t> cache;
        auto it = cache.find(headerName);
        if (it != cache.end())
            return it->second;

        std::string path = std::string(ENJIN_GRAPHICS_DIR) + "/" + headerName;
        std::ifstream f(path, std::ios::binary);
        uint64_t h = 0;
        if (f)
        {
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                       std::istreambuf_iterator<char>());
            h = fnv1a64(bytes.data(), bytes.size());
        }
        cache[headerName] = h;
        return h;
    }

    // --- waiver evaluation (design § 8) -----------------------------------

    enum class WaiverVerdict
    {
        NotCovered,    // no active waiver covers this case — plain failure
        Waived,        // covered, pins fresh, diff within the enforced bound
        BoundExceeded, // covered and pins fresh, but the difference GREW
        StalePin       // covered, but an implementing header changed
    };

    struct WaiverHit
    {
        WaiverVerdict verdict = WaiverVerdict::NotCovered;
        const Waiver *waiver = nullptr;
    };

    inline WaiverHit evaluateWaiver(const Waiver *table, size_t tableCount,
                                    const char *pairId,
                                    const Param *params, size_t paramCount,
                                    size_t diffMagnitude)
    {
        for (size_t i = 0; i < tableCount; ++i)
        {
            const Waiver &w = table[i];
            if (w.status != WaiverStatus::Active)
                continue;
            if (strcmp(w.pair_id, pairId) != 0)
                continue;
            if (!w.covers || !w.covers(params, paramCount))
                continue; // predicate is mandatory; null covers nothing

            if (pinHash(w.base_pin_file) != w.base_pin_hash ||
                pinHash(w.head_pin_file) != w.head_pin_hash)
                return {WaiverVerdict::StalePin, &w};

            if (diffMagnitude > w.max_diff)
                return {WaiverVerdict::BoundExceeded, &w};
            return {WaiverVerdict::Waived, &w};
        }
        return {};
    }

    inline const Waiver *retiredEntry(const Waiver *table, size_t tableCount,
                                      const char *pairId)
    {
        for (size_t i = 0; i < tableCount; ++i)
            if (table[i].status == WaiverStatus::Retired &&
                strcmp(table[i].pair_id, pairId) == 0)
                return &table[i];
        return nullptr;
    }

    // --- pair bookkeeping, signatures, census ------------------------------

    struct PairInfo
    {
        const char *id;       // short id waivers/signatures reference
        const char *tier;     // "TIER1" | "TIER2 GUARD" | "TIER3"
        const char *baseCall; // what BASE Eisei actually called
        const char *headCall; // what HEAD Eisei actually calls
    };

    struct PairStats
    {
        PairInfo info{};
        size_t cases = 0;
        size_t failed = 0;        // unwaived failures (incl. bound/stale)
        size_t waived = 0;        // failures absorbed by a fresh, bounded waiver
        size_t boundExceeded = 0;
        size_t stalePin = 0;
        bool retired = false;
        // Every value each parameter axis was swept over (pass or fail) —
        // the reference the #158 signature discriminator compares failing
        // value sets against.
        std::map<std::string, std::set<int32_t>> sweptValues;
    };

    // Axis mask bits. Compositing pairs have no HEAD-@8bpp form (the blit
    // family is Pixel4-native), so their failures carry NoB: the A/B/C
    // decomposition cannot split algorithm from depth there.
    enum : uint8_t
    {
        AxisAlgo = 1,  // A != B — algorithm divergence
        AxisDepth = 2, // B != C — depth divergence
        AxisNoB = 4,   // pair has no B side; verdict is A vs C only
        AxisWaiverMeta = 8 // stale-pin / bound-exceeded meta-failure
    };

    inline const char *axisLabel(uint8_t mask)
    {
        if (mask & AxisWaiverMeta)
            return "WAIVER META (stale pin / bound exceeded)";
        if (mask & AxisNoB)
            return "ALGO+DEPTH entangled (no 8bpp HEAD form)";
        switch (mask & (AxisAlgo | AxisDepth))
        {
        case AxisAlgo:
            return "ALGORITHM (A!=B), depth-clean (B==C)";
        case AxisDepth:
            return "DEPTH (B!=C), algorithm-clean (A==B)";
        case AxisAlgo | AxisDepth:
            return "ALGORITHM (A!=B) + DEPTH (B!=C)";
        default:
            return "verdict red, both axes clean (harness bug?)";
        }
    }

    struct Signature
    {
        std::string pairId;
        uint8_t axisMask = 0;
        size_t cases = 0;
        // First failing case = the reproduction the row reports (sweeps run
        // simple -> complex, so the first failure is the minimal repro).
        std::vector<Param> firstParams;
        std::string firstDetail;
        size_t maxDiff = 0; // largest per-case diff magnitude in the cluster
        // Failing range per parameter axis (min..max over failing cases).
        std::map<std::string, std::pair<int32_t, int32_t>> paramRange;
        // Distinct failing values per parameter axis, compared against the
        // pair's swept values to compute the #158 discriminating axis: the
        // first parameter along which results diverge (i.e. whose failing
        // value set is a proper subset of what was swept).
        std::map<std::string, std::set<int32_t>> failingValues;
        std::vector<std::string> paramOrder; // declaration order of the first repro
        // Repro planes for the PNG review path (empty for metric pairs).
        std::vector<uint8_t> planeA, planeC;
    };

    struct Bench
    {
        std::vector<PairStats> pairs;
        std::vector<Signature> signatures;
        const Waiver *waivers = kWaivers.data();
        size_t waiverCount = kWaivers.size();
        int selfcheckFailures = 0;
        PairStats *cur = nullptr;

        PairStats &beginPair(const PairInfo &info)
        {
            pairs.push_back(PairStats{info});
            cur = &pairs.back();
            if (retiredEntry(waivers, waiverCount, info.id))
                cur->retired = true;
            return *cur;
        }

        Signature &signatureFor(const char *pairId, uint8_t axisMask)
        {
            for (auto &s : signatures)
                if (s.pairId == pairId && s.axisMask == axisMask)
                    return s;
            signatures.push_back(Signature{});
            signatures.back().pairId = pairId;
            signatures.back().axisMask = axisMask;
            return signatures.back();
        }

        void recordFailure(const std::vector<Param> &params, uint8_t axisMask,
                           size_t diffMagnitude, const std::string &detail,
                           const uint8_t *planeA, const uint8_t *planeC)
        {
            cur->failed++;
            Signature &s = signatureFor(cur->info.id, axisMask);
            s.cases++;
            if (diffMagnitude > s.maxDiff)
                s.maxDiff = diffMagnitude;
            if (s.cases == 1)
            {
                s.firstParams = params;
                s.firstDetail = detail;
                for (const Param &p : params)
                    s.paramOrder.push_back(p.name);
                if (planeA && planeC)
                {
                    s.planeA.assign(planeA, planeA + PLANE);
                    s.planeC.assign(planeC, planeC + PLANE);
                }
            }
            for (const Param &p : params)
            {
                s.failingValues[p.name].insert(p.value);
                auto it = s.paramRange.find(p.name);
                if (it == s.paramRange.end())
                    s.paramRange[p.name] = {p.value, p.value};
                else
                {
                    it->second.first = std::min(it->second.first, p.value);
                    it->second.second = std::max(it->second.second, p.value);
                }
            }
        }

        // Shared verdict path for one executed case: consult the waiver table,
        // then record. `axisMask` should be the pure divergence axes; waiver
        // meta-failures get AxisWaiverMeta added here.
        void settleCase(const std::vector<Param> &params, bool equal,
                        uint8_t axisMask, size_t diffMagnitude,
                        const std::string &detail,
                        const uint8_t *planeA, const uint8_t *planeC)
        {
            cur->cases++;
            for (const Param &p : params)
                cur->sweptValues[p.name].insert(p.value);
            if (equal)
                return;
            WaiverHit hit = evaluateWaiver(waivers, waiverCount, cur->info.id,
                                           params.data(), params.size(),
                                           diffMagnitude);
            switch (hit.verdict)
            {
            case WaiverVerdict::Waived:
                cur->waived++;
                return;
            case WaiverVerdict::BoundExceeded:
                cur->boundExceeded++;
                recordFailure(params, static_cast<uint8_t>(axisMask | AxisWaiverMeta),
                              diffMagnitude,
                              detail + " [WAIVER BOUND EXCEEDED: " +
                                  std::to_string(diffMagnitude) + " > " +
                                  std::to_string(hit.waiver->max_diff) + ", ticket " +
                                  hit.waiver->ticket + "]",
                              planeA, planeC);
                return;
            case WaiverVerdict::StalePin:
                cur->stalePin++;
                recordFailure(params, static_cast<uint8_t>(axisMask | AxisWaiverMeta),
                              diffMagnitude,
                              detail + " [STALE WAIVER: implementing header changed, "
                                       "re-adjudicate ticket " +
                                  std::string(hit.waiver->ticket) + "]",
                              planeA, planeC);
                return;
            case WaiverVerdict::NotCovered:
                recordFailure(params, axisMask, diffMagnitude, detail, planeA, planeC);
                return;
            }
        }

        size_t totalCases() const
        {
            size_t n = 0;
            for (const auto &p : pairs)
                n += p.cases;
            return n;
        }

        size_t totalFailed() const
        {
            size_t n = 0;
            for (const auto &p : pairs)
                n += p.failed;
            return n;
        }

        // Census hard gate (§ 8): pairs whose waived fraction exceeds 50%.
        std::vector<const PairStats *> censusViolations() const
        {
            std::vector<const PairStats *> v;
            for (const auto &p : pairs)
                if (p.cases > 0 && p.waived * 2 > p.cases)
                    v.push_back(&p);
            return v;
        }
    };

    // --- case runner (A/B/C decomposition, design § 4) ---------------------

    struct Canvases
    {
        std::unique_ptr<Base8> a = std::make_unique<Base8>();
        std::unique_ptr<Base8> b = std::make_unique<Base8>();
        std::unique_ptr<Head4> c = std::make_unique<Head4>();
        std::vector<uint8_t> pa = std::vector<uint8_t>(PLANE);
        std::vector<uint8_t> pb = std::vector<uint8_t>(PLANE);
        std::vector<uint8_t> pc = std::vector<uint8_t>(PLANE);
    };

    inline Canvases &canvases()
    {
        static Canvases cv;
        return cv;
    }

    inline std::string firstDiffDetail(const PlaneDiff &d)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%zu px differ, first (%d,%d) BASE=%u HEAD=%u",
                 d.count, d.fx, d.fy, d.lhs, d.rhs);
        return buf;
    }

    // One pixel pair case. `head8` may be null for the Pixel4-native
    // compositing pairs (AxisNoB). Prefill is applied to all three canvases
    // before the ops run (blend-op sweeps drive it 0..15).
    inline void runPixelCase(Bench &bench, const std::vector<Param> &params,
                             const std::function<void(Base8 &)> &base,
                             const std::function<void(Base8 &)> &head8,
                             const std::function<void(Head4 &)> &head4,
                             uint8_t prefill = 0)
    {
        Canvases &cv = canvases();
        cv.a->clear(prefill);
        cv.c->clear(enjin2::Pixel4(prefill));
        base(*cv.a);
        head4(*cv.c);
        maskPlane(*cv.a, cv.pa.data());
        unpackPlane(*cv.c, cv.pc.data());

        PlaneDiff verdict = diffPlanes(cv.pa.data(), cv.pc.data());
        if (verdict.equal)
        {
            bench.settleCase(params, true, 0, 0, "", nullptr, nullptr);
            return;
        }

        uint8_t axis = AxisNoB;
        if (head8)
        {
            cv.b->clear(prefill);
            head8(*cv.b);
            maskPlane(*cv.b, cv.pb.data());
            axis = 0;
            if (memcmp(cv.pa.data(), cv.pb.data(), PLANE) != 0)
                axis |= AxisAlgo;
            if (memcmp(cv.pb.data(), cv.pc.data(), PLANE) != 0)
                axis |= AxisDepth;
        }
        bench.settleCase(params, false, axis, verdict.count,
                         firstDiffDetail(verdict), cv.pa.data(), cv.pc.data());
    }

    // One metric case (bounds/width tuples instead of pixels). Diff magnitude
    // = sum of |component deltas|; no repro planes.
    inline void runMetricCase(Bench &bench, const std::vector<Param> &params,
                              bool aEqB, bool bEqC, bool aEqC,
                              size_t diffMagnitude, const std::string &detail)
    {
        if (aEqC)
        {
            bench.settleCase(params, true, 0, 0, "", nullptr, nullptr);
            return;
        }
        uint8_t axis = 0;
        if (!aEqB)
            axis |= AxisAlgo;
        if (!bEqC)
            axis |= AxisDepth;
        bench.settleCase(params, false, axis, diffMagnitude, detail, nullptr, nullptr);
    }

    // --- reporting ---------------------------------------------------------

    constexpr size_t kSignatureBudget = 20; // § 7: reviewer-time ceiling

    inline std::string paramString(const std::vector<Param> &params)
    {
        std::string s;
        for (const Param &p : params)
        {
            if (!s.empty())
                s += " ";
            s += p.name;
            s += "=";
            s += std::to_string(p.value);
        }
        return s.empty() ? "(none)" : s;
    }

    inline void printPairLine(const PairStats &p)
    {
        char status[64];
        if (p.retired)
            snprintf(status, sizeof(status), "RETIRED (skipped)");
        else if (p.failed == 0 && p.waived == 0)
            snprintf(status, sizeof(status), "GREEN");
        else if (p.failed == 0)
            snprintf(status, sizeof(status), "GREEN (%zu waived)", p.waived);
        else
            snprintf(status, sizeof(status), "RED  %zu/%zu failed", p.failed, p.cases);
        printf("[%-11s] %-22s %6zu cases  %s\n", p.info.tier, p.info.id, p.cases, status);
    }

    // The #158 discriminating axis: the first parameter (in the repro's
    // declaration order) whose failing value set is a PROPER subset of the
    // values the pair swept it over — i.e. the axis along which results
    // diverge. Empty when every swept value of every axis fails (the
    // divergence is systematic across the whole sweep).
    inline std::string discriminatingAxis(const Bench &bench, const Signature &s)
    {
        const PairStats *pair = nullptr;
        for (const auto &p : bench.pairs)
            if (s.pairId == p.info.id)
                pair = &p;
        if (!pair)
            return "";
        for (const std::string &name : s.paramOrder)
        {
            auto fail = s.failingValues.find(name);
            auto swept = pair->sweptValues.find(name);
            if (fail == s.failingValues.end() || swept == pair->sweptValues.end())
                continue;
            if (fail->second.size() < swept->second.size())
            {
                char buf[96];
                snprintf(buf, sizeof(buf), "%s (%zu of %zu swept values fail)",
                         name.c_str(), fail->second.size(), swept->second.size());
                return buf;
            }
        }
        return "";
    }

    inline void printSignatures(const Bench &bench)
    {
        if (bench.signatures.empty())
        {
            printf("\nfailure signatures: none — every pair byte-exact on the swept ranges\n");
            return;
        }
        printf("\nfailure signatures: %zu distinct (budget ~%zu/run)\n",
               bench.signatures.size(), kSignatureBudget);
        if (bench.signatures.size() > kSignatureBudget)
            printf("  *** BUDGET EXCEEDED — triage these before growing the sweep or the waiver table ***\n");
        int i = 0;
        for (const auto &s : bench.signatures)
        {
            printf("\n  #%-2d %s — %s\n", ++i, s.pairId.c_str(), axisLabel(s.axisMask));
            printf("      cases: %zu   max diff: %zu\n", s.cases, s.maxDiff);
            const std::string axis = discriminatingAxis(bench, s);
            printf("      diverges along: %s\n",
                   axis.empty() ? "(entire sweep — systematic)" : axis.c_str());
            printf("      repro: %s\n", paramString(s.firstParams).c_str());
            printf("      first: %s\n", s.firstDetail.c_str());
            if (!s.paramRange.empty())
            {
                printf("      failing ranges:");
                for (const auto &kv : s.paramRange)
                {
                    if (kv.second.first == kv.second.second)
                        printf(" %s=%d", kv.first.c_str(), kv.second.first);
                    else
                        printf(" %s=[%d..%d]", kv.first.c_str(), kv.second.first,
                               kv.second.second);
                }
                printf("\n");
            }
        }
    }

    // Age of a waiver in days from its YYYY-MM-DD date field. Printed for
    // humans at natural checkpoints only — nothing fires on age (the
    // implementation pin fires on actual risk, #159).
    inline std::string waiverAge(const char *date)
    {
        int y = 0, m = 0, d = 0;
        if (!date || sscanf(date, "%d-%d-%d", &y, &m, &d) != 3)
            return "unknown";
        std::tm tm{};
        tm.tm_year = y - 1900;
        tm.tm_mon = m - 1;
        tm.tm_mday = d;
        const time_t then = mktime(&tm);
        if (then == static_cast<time_t>(-1))
            return "unknown";
        const double days = difftime(time(nullptr), then) / 86400.0;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f days", days < 0 ? 0.0 : days);
        return buf;
    }

    inline void printCensus(const Bench &bench)
    {
        printf("\nwaiver census (design § 8):\n");
        if (bench.waiverCount == 0)
        {
            printf("  table EMPTY — 0 active, 0 retired (as #161 intended)\n");
        }
        else
        {
            size_t active = 0, retired = 0;
            for (size_t i = 0; i < bench.waiverCount; ++i)
            {
                if (bench.waivers[i].status == WaiverStatus::Active)
                    active++;
                else
                    retired++;
            }
            printf("  %zu active, %zu retired\n", active, retired);
            for (size_t i = 0; i < bench.waiverCount; ++i)
            {
                const Waiver &w = bench.waivers[i];
                printf("  - %-22s %s  [%s]  ticket %s  by %s on %s (age %s)\n",
                       w.pair_id,
                       w.status == WaiverStatus::Active ? "ACTIVE " : "RETIRED",
                       w.subrange ? w.subrange : "(whole pair)",
                       w.ticket, w.author, w.date, waiverAge(w.date).c_str());
            }
        }
        for (const auto &p : bench.pairs)
        {
            if (p.retired)
                printf("  ! %-22s RETIRED — tests nothing; the retirement ticket must name what guards HEAD now\n",
                       p.info.id);
            else if (p.waived > 0)
                printf("  ~ %-22s %zu/%zu cases waived (%.1f%%)\n", p.info.id,
                       p.waived, p.cases, 100.0 * p.waived / p.cases);
        }
        auto violations = bench.censusViolations();
        for (const auto *p : violations)
            printf("  *** CENSUS GATE: %s is >50%% waived — a pair mostly waived mostly tests nothing. FIX OR RETIRE. ***\n",
                   p->info.id);
    }

} // namespace parity

#endif // ENJIN2_TESTS_VISUAL_PARITY_BENCH_SUPPORT_HPP
