// Visual Parity Bench — M1 skeleton (unwn #164, epic #163).
//
// One-binary call-path differential harness (Visual-Parity-Bench-Design §§ 1-6):
// this tree contains both sides of the Eisei enjin migration — Canvas8 with every
// BASE drawing algorithm intact, plus Canvas4 / Primitives<TPixel> / TextRenderer —
// so BASE and HEAD run in-process and comparison is an in-process memcmp. The unit
// of test is the *substitution pair*: (what BASE Eisei actually called, what HEAD
// Eisei actually calls) — a call-site substitution, never a name match (§ 2).
//
// Every pixel pair runs the A/B/C decomposition (§ 4):
//   A: BASE algorithm @8bpp (Canvas8 member function)
//   B: HEAD algorithm @8bpp (Primitives<uint8_t> / TextRenderer<uint8_t>)
//   C: HEAD algorithm @4bpp (Primitives<Pixel4> / TextRenderer<Pixel4> on Canvas4)
// so every failure arrives pre-labelled: A!=B is *algorithm*, B!=C is *depth*.
//
// Comparison plane (§ 5): BASE plane = the Canvas8 buffer masked & 0x0F (Eisei
// ships the Canvas8 buffer straight to Adafruit_SSD1327, whose drawPixel at
// _bpp == 4 writes color & 0xF — the reduction is masking, not shifting); HEAD
// plane = the Canvas4 buffer unpacked to one byte per pixel. Color correspondence
// is the identity on 0-15. Match rule (§ 6): raw planes, byte-exact memcmp, no
// tolerance.
//
// M1 ships only the two Tier 2 REGRESSION GUARD pairs from unwn #161 (§ 3):
//   - circle: Canvas8::fillCircle/drawCircle (midpoint-octant) vs
//     Primitives::fillCircle/drawCircle (sqrt-scanline fill / midpoint outline)
//   - text:   Canvas8::getTextBounds/charBounds (Adafruit ink box) vs
//     TextRenderer::getTextBounds (advance-width / yAdvance box)
// These guards are REQUIRED-RED against current HEAD: they are the gate for the
// M2 restore commits and go green when those land. A red guard reads as "M2 gate
// open", never as harness breakage — the SELFCHECK cases prove the instrument.
// They are not evidence the instrument finds unknown regressions (Tier 1 does).
//
// This binary is deliberately NOT registered with add_test (§ 11): the bench
// runs locally, deliberately — it is a tool, not a CI check — and wiring its
// required-red guards into ctest would misread as suite breakage.

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <enjin2/graphics/text_renderer.hpp>

#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

using namespace enjin2;

namespace {

// Eisei's panel geometry. (M2 restores the 127 logical region on the Eisei
// side; the bench plane stays the full even-width canvas both branches allocate.)
constexpr uint16_t W = 128;
constexpr uint16_t H = 128;
constexpr size_t PLANE = static_cast<size_t>(W) * H;

using Base8 = Canvas8<W, H>;
using Head4 = Canvas4<W, H>;

int selfcheck_failures = 0;
int guard_red = 0;
int guard_green = 0;

// --- comparison planes (design § 5) --------------------------------------

void maskPlane(const Base8& c, uint8_t* out) {
    const uint8_t* buf = c.getBuffer();
    for (size_t i = 0; i < PLANE; ++i) out[i] = buf[i] & 0x0F;
}

void unpackPlane(const Head4& c, uint8_t* out) {
    for (int16_t y = 0; y < H; ++y)
        for (int16_t x = 0; x < W; ++x)
            out[static_cast<size_t>(y) * W + x] = c.getPixel(x, y).value;
}

struct PlaneDiff {
    bool equal;
    size_t count;   // number of differing bytes
    int16_t fx, fy; // first differing pixel
    uint8_t lhs, rhs;
};

PlaneDiff diffPlanes(const uint8_t* a, const uint8_t* b) {
    PlaneDiff d{true, 0, -1, -1, 0, 0};
    if (memcmp(a, b, PLANE) == 0) return d; // the verdict rule: byte-exact memcmp
    d.equal = false;
    for (size_t i = 0; i < PLANE; ++i) {
        if (a[i] != b[i]) {
            if (d.count == 0) {
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

size_t inkCount(const uint8_t* plane) {
    size_t n = 0;
    for (size_t i = 0; i < PLANE; ++i)
        if (plane[i] != 0) n++;
    return n;
}

// --- pair runner (A/B/C decomposition, design § 4) ------------------------

struct PairOutcome {
    PlaneDiff verdict; // A vs C — the pair verdict
    bool algoAxis;     // A != B → algorithm divergence
    bool depthAxis;    // B != C → depth divergence
    size_t inkA, inkC;
};

PairOutcome runPixelPair(const std::function<void(Base8&)>& base,
                         const std::function<void(Base8&)>& head8,
                         const std::function<void(Head4&)>& head4) {
    auto a8 = std::make_unique<Base8>();
    auto b8 = std::make_unique<Base8>();
    auto c4 = std::make_unique<Head4>();
    a8->clear(0);
    b8->clear(0);
    c4->clear(Pixel4(0));

    base(*a8);
    head8(*b8);
    head4(*c4);

    std::vector<uint8_t> pa(PLANE), pb(PLANE), pc(PLANE);
    maskPlane(*a8, pa.data());
    maskPlane(*b8, pb.data());
    unpackPlane(*c4, pc.data());

    PairOutcome out;
    out.verdict = diffPlanes(pa.data(), pc.data());
    out.algoAxis = !diffPlanes(pa.data(), pb.data()).equal;
    out.depthAxis = !diffPlanes(pb.data(), pc.data()).equal;
    out.inkA = inkCount(pa.data());
    out.inkC = inkCount(pc.data());
    return out;
}

const char* axisLabel(const PairOutcome& o) {
    if (o.algoAxis && o.depthAxis) return "ALGORITHM (A!=B) + DEPTH (B!=C)";
    if (o.algoAxis) return "ALGORITHM (A!=B), depth-clean (B==C)";
    if (o.depthAxis) return "DEPTH (B!=C), algorithm-clean (A==B)";
    return "clean on both axes";
}

// --- reporting ------------------------------------------------------------

void reportSelfcheck(const char* name, bool pass, const char* detail) {
    printf("[SELFCHECK       ] %-58s %s\n", name, pass ? "PASS" : "FAIL");
    if (!pass) {
        printf("                   HARNESS BUG: %s\n", detail);
        selfcheck_failures++;
    }
}

void reportGuard(const char* name, const PairOutcome& o, bool printInk) {
    printf("[REGRESSION GUARD] %-58s %s\n", name, o.verdict.equal ? "GREEN" : "RED");
    if (o.verdict.equal) {
        guard_green++;
        return;
    }
    guard_red++;
    printf("                   A/C memcmp: %zu px differ, first at (%d,%d) BASE=%u HEAD=%u | axis: %s\n",
           o.verdict.count, o.verdict.fx, o.verdict.fy, o.verdict.lhs, o.verdict.rhs, axisLabel(o));
    if (printInk)
        printf("                   ink: BASE %zu px, HEAD %zu px\n", o.inkA, o.inkC);
}

// --- selfchecks: prove the instrument, not the pairs ----------------------

// The same setPixel loop writing every color 0..15 through all three
// substrates must compare equal — proves the mask/unpack/memcmp plumbing
// (nibble order, plane layout, 0-15 identity) with zero algorithm in play.
void selfcheckSubstrateIdentity() {
    auto paint8 = [](Base8& c) {
        for (int16_t y = 0; y < H; ++y)
            for (int16_t x = 0; x < W; ++x)
                c.setPixel(x, y, static_cast<uint8_t>((x + y * 3) % 16));
    };
    auto paint4 = [](Head4& c) {
        for (int16_t y = 0; y < H; ++y)
            for (int16_t x = 0; x < W; ++x)
                c.setPixel(x, y, Pixel4(static_cast<uint8_t>((x + y * 3) % 16)));
    };
    PairOutcome o = runPixelPair(paint8, paint8, paint4);
    char detail[128];
    snprintf(detail, sizeof(detail), "identity pattern diverged: %zu px, first (%d,%d) %u vs %u",
             o.verdict.count, o.verdict.fx, o.verdict.fy, o.verdict.lhs, o.verdict.rhs);
    reportSelfcheck("substrate identity (setPixel 0..15 pattern, A==B==C)", o.verdict.equal && !o.algoAxis && !o.depthAxis, detail);
}

// Cross-path identity: Canvas8::drawLine and Primitives::drawLine are the same
// Bresenham walk, so a BASE-member vs HEAD-static pair must be green end to
// end — proves the pair runner exercises both call paths correctly.
void selfcheckCrossPathLine() {
    auto base = [](Base8& c) {
        c.drawLine(3, 5, 120, 97, 15);
        c.drawLine(120, 5, 3, 97, 9);
        c.drawLine(0, 64, 127, 64, 4);
    };
    auto head8 = [](Base8& c) {
        Primitives<uint8_t>::drawLine(c, 3, 5, 120, 97, 15);
        Primitives<uint8_t>::drawLine(c, 120, 5, 3, 97, 9);
        Primitives<uint8_t>::drawLine(c, 0, 64, 127, 64, 4);
    };
    auto head4 = [](Head4& c) {
        Primitives<Pixel4>::drawLine(c, 3, 5, 120, 97, Pixel4(15));
        Primitives<Pixel4>::drawLine(c, 120, 5, 3, 97, Pixel4(9));
        Primitives<Pixel4>::drawLine(c, 0, 64, 127, 64, Pixel4(4));
    };
    PairOutcome o = runPixelPair(base, head8, head4);
    char detail[128];
    snprintf(detail, sizeof(detail), "byte-identical Bresenham diverged: %zu px, first (%d,%d)",
             o.verdict.count, o.verdict.fx, o.verdict.fy);
    reportSelfcheck("cross-path identity (drawLine, byte-identical Bresenham)", o.verdict.equal, detail);
}

// --- Tier 2 guard: circle pair (unwn #161 swap 1) -------------------------

// The substitution: Eisei's calls moved from Canvas8::fillCircle (midpoint-
// octant fill) to Primitives<Pixel4>::fillCircle (sqrt-scanline fill), and from
// Canvas8::drawCircle to Primitives<Pixel4>::drawCircle. HEAD's fill is a
// strict subset of BASE at every radius (#161) — the "circles rendering
// smaller" symptom. Radii below are #161's evidence set: r=2 satellite dot
// (21 px vs 13 px), r=33 planet outline (-88 px), r=63 StatusUI blackout
// (184 px unmasked).
void guardCircleFill() {
    const int16_t radii[] = {2, 33, 63};
    for (int16_t r : radii) {
        auto o = runPixelPair(
            [r](Base8& c) { c.fillCircle(64, 64, r, 15); },
            [r](Base8& c) { Primitives<uint8_t>::fillCircle(c, 64, 64, r, 15); },
            [r](Head4& c) { Primitives<Pixel4>::fillCircle(c, 64, 64, r, Pixel4(15)); });
        char name[96];
        snprintf(name, sizeof(name), "circle fill r=%d  Canvas8::fillCircle vs Primitives::fillCircle", r);
        reportGuard(name, o, true);
    }
}

void guardCircleDraw() {
    // Sweep the first 65 radii; #161 measured drawCircle diverging at 33 of them.
    int divergent = 0;
    PairOutcome firstRed{};
    int16_t firstRedR = -1;
    for (int16_t r = 0; r <= 64; ++r) {
        auto o = runPixelPair(
            [r](Base8& c) { c.drawCircle(64, 64, r, 15); },
            [r](Base8& c) { Primitives<uint8_t>::drawCircle(c, 64, 64, r, 15); },
            [r](Head4& c) { Primitives<Pixel4>::drawCircle(c, 64, 64, r, Pixel4(15)); });
        if (!o.verdict.equal) {
            if (divergent == 0) {
                firstRed = o;
                firstRedR = r;
            }
            divergent++;
        }
    }
    char name[96];
    snprintf(name, sizeof(name), "circle draw r=0..64  Canvas8::drawCircle vs Primitives::drawCircle");
    printf("[REGRESSION GUARD] %-58s %s\n", name, divergent == 0 ? "GREEN" : "RED");
    if (divergent == 0) {
        guard_green++;
    } else {
        guard_red++;
        printf("                   %d of 65 radii diverge; first at r=%d: %zu px differ, first px (%d,%d) BASE=%u HEAD=%u | axis: %s\n",
               divergent, firstRedR, firstRed.verdict.count, firstRed.verdict.fx, firstRed.verdict.fy,
               firstRed.verdict.lhs, firstRed.verdict.rhs, axisLabel(firstRed));
    }
}

// --- Tier 2 guard: text-metrics pair (unwn #161 swap 2) -------------------

// The substitution: Eisei's getTextBounds moved from Canvas8::getTextBounds
// (Adafruit ink box via charBounds: x1=minx, y1=miny) to
// TextRenderer::getTextBounds (advance-width / yAdvance with x1=x, y1=y).
// Centered text lands ~10-26 px high as a result (#161). Metrics pairs
// compare the returned (x1,y1,w,h) tuple; A/B/C still applies — B and C are
// TextRenderer<uint8_t> and TextRenderer<Pixel4>.
struct Bounds {
    int16_t x1, y1;
    uint16_t w, h;
    bool operator==(const Bounds& o) const { return x1 == o.x1 && y1 == o.y1 && w == o.w && h == o.h; }
};

void guardTextBounds(const char* str, uint8_t size) {
    const int16_t x = 10, y = 64;

    Bounds a, b, c;
    {
        auto canvas = std::make_unique<Base8>(); // default font: defaultFont8pt7b, wrap=true
        canvas->setTextSize(size);
        canvas->getTextBounds(str, x, y, &a.x1, &a.y1, &a.w, &a.h);
    }
    {
        TextRenderer<uint8_t> tr;
        tr.setFont(&defaultFont8pt7b); // same font as Canvas8's default
        tr.setTextSize(size);
        tr.getTextBounds(str, x, y, &b.x1, &b.y1, &b.w, &b.h);
    }
    {
        TextRenderer<Pixel4> tr;
        tr.setFont(&defaultFont8pt7b);
        tr.setTextSize(size);
        tr.getTextBounds(str, x, y, &c.x1, &c.y1, &c.w, &c.h);
    }

    const bool green = (a == c);
    char name[96];
    snprintf(name, sizeof(name), "text bounds \"%s\" size %u  Canvas8::getTextBounds vs TextRenderer", str, size);
    printf("[REGRESSION GUARD] %-58s %s\n", name, green ? "GREEN" : "RED");
    if (green) {
        guard_green++;
        return;
    }
    guard_red++;
    const bool algoAxis = !(a == b);
    const bool depthAxis = !(b == c);
    printf("                   BASE ink box (x1,y1,w,h)=(%d,%d,%u,%u)  HEAD advance box=(%d,%d,%u,%u) | axis: %s\n",
           a.x1, a.y1, a.w, a.h, c.x1, c.y1, c.w, c.h,
           algoAxis && depthAxis ? "ALGORITHM (A!=B) + DEPTH (B!=C)"
           : algoAxis            ? "ALGORITHM (A!=B), depth-clean (B==C)"
           : depthAxis           ? "DEPTH (B!=C), algorithm-clean (A==B)"
                                 : "clean on both axes");
}

} // namespace

int main() {
    printf("visual_parity_bench — call-path differential bench, M1 skeleton (unwn #164, epic #163)\n");
    printf("A: BASE algo @8bpp (Canvas8)  B: HEAD algo @8bpp  C: HEAD algo @4bpp (Canvas4)\n");
    printf("verdict = memcmp(Canvas8 & 0x0F, Canvas4 unpacked); A!=B labels ALGORITHM, B!=C labels DEPTH\n\n");

    selfcheckSubstrateIdentity();
    selfcheckCrossPathLine();
    printf("\n");

    guardCircleFill();
    guardCircleDraw();
    guardTextBounds("SATURN", 1);
    guardTextBounds("Datum 12", 2);

    printf("\nsummary: selfchecks %s | guards: %d GREEN, %d RED\n",
           selfcheck_failures == 0 ? "PASS" : "FAIL (harness bug)", guard_green, guard_red);
    if (guard_red > 0)
        printf("RED guards are the EXPECTED M1 state: they are Tier 2 regression guards from unwn #161\n"
               "and gate the M2 restore commits — they go green when the restores land. They are not\n"
               "evidence of harness breakage (the SELFCHECK lines prove the instrument).\n");

    if (selfcheck_failures > 0) return 2; // harness bug — never acceptable
    return guard_red > 0 ? 1 : 0;         // red guards: expected until M2 lands
}
