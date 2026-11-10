// Visual Parity Bench — full pair sweep (M3, unwn #166, epic #163).
//
// One-binary call-path differential harness (Visual-Parity-Bench-Design):
// this tree contains both sides of the Eisei enjin migration — Canvas8 with
// every BASE drawing algorithm intact, plus Canvas4 / Primitives<TPixel> —
// so BASE and HEAD run in-process and
// comparison is an in-process memcmp. The unit of test is the *substitution
// pair*: (what BASE Eisei actually called, what HEAD Eisei actually calls) —
// a call-site substitution, never a name match (§ 2).
//
// Pixel pairs run the A/B/C decomposition (§ 4) where a HEAD-@8bpp form
// exists:
//   A: BASE algorithm @8bpp (Canvas8 member)
//   B: HEAD algorithm @8bpp (Primitives<uint8_t>)
//   C: HEAD algorithm @4bpp (Primitives<Pixel4>)
// A!=B labels ALGORITHM, B!=C labels DEPTH. The text pairs are gone with
// Canvas8's text (Tomodachi #476, ADR-0029: one text route, no BASE left). The blit compositor's grey-level
// pairs are gone with the blits themselves (Tomodachi #426: the palette is 8
// roles, ADR-0022, so 16-grey arithmetic on Pixel4 has no meaning).
//
// Comparison plane (§ 5): BASE = Canvas8 buffer & 0x0F (Eisei ships the
// Canvas8 buffer straight to Adafruit_SSD1327, whose drawPixel at _bpp == 4
// writes color & 0xF — masking, not shifting); HEAD = Canvas4 unpacked.
// Color correspondence is the identity on 0-15. Match rule (§ 6): raw
// planes, byte-exact memcmp, no tolerance — except inside an Active waiver's
// sub-range (tests/waivers.hpp), where the comparison swaps to that waiver's
// enforced bound. The waiver table starts empty.
//
// Coverage (§ 7) is a parameter sweep, not a corpus: ~60k cases across the
// #158-locked ranges, every pair with its mandatory edge set (off-canvas,
// partial clip, zero/negative extent, r=0, empty string, degenerate
// vertices). The budget caps DISTINCT FAILURE SIGNATURES (~20/run), not
// cases: failures cluster to (pair, divergence axis) and report one row per
// signature with the first repro's parameters and the count behind it.
//
// Canvas geometry is 127x127 — what both branches actually ship
// (BASE Canvas8<127,127>, HEAD Canvas4<127,127> post the #155 restore). The
// substrate sweep is exhaustive over it on purpose: it is the only sweep
// that can catch odd-width row-packing aliasing in Canvas4.
//
// Usage:
//   visual_parity_bench                 run the sweep, text report
//   visual_parity_bench --png DIR       also write BASE|HEAD|diff triptych
//                                       PNGs (x4 NN, diff magenta) per
//                                       signature — advisory, never a gate;
//                                       delivery to the phone stays a human
//                                       `tailscale file cp` command (§ 9)
//   visual_parity_bench --hash          print the graphics-header pin hashes
//                                       a new waiver entry must carry
//
// Exit codes: 0 all pairs byte-exact (or bounded-waived) · 1 failure
// signatures present · 2 harness selfcheck failure · 3 waiver census gate
// (a pair > 50% waived) · 4 usage error.
//
// Deliberately NOT registered with add_test (§ 11): the bench is a local
// instrument, and its first M3 runs are EXPECTED red — the #158 derivation
// already flagged the >>4-vs-&0xF asset reduction, the blend saturation
// ceiling and four structural compositor divergences for adjudication on
// the execution epic. The waiver machinery itself is CI-tested in
// tests/waiver_machinery_test.cpp.

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>

#include "visual_parity/bench_support.hpp"
#include "visual_parity/png_review.hpp"


#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace enjin2;
using namespace parity;

namespace
{

    Bench bench;

    void endPair() { printPairLine(*bench.cur); }

    // --- selfchecks: prove the instrument, not the pairs -------------------

    void reportSelfcheck(const char *name, bool pass, const char *detail)
    {
        printf("[SELFCHECK  ] %-58s %s\n", name, pass ? "PASS" : "FAIL");
        if (!pass)
        {
            printf("              HARNESS BUG: %s\n", detail);
            bench.selfcheckFailures++;
        }
    }

    // The same setPixel loop writing every color 0..15 through both
    // substrates must compare equal — proves the mask/unpack/memcmp plumbing
    // (nibble order, odd-width row stride, 0-15 identity) with zero
    // algorithm in play.
    void selfcheckSubstrateIdentity()
    {
        Canvases &cv = canvases();
        cv.a->clear(0);
        cv.c->clear(Pixel4(0));
        for (int16_t y = 0; y < H; ++y)
            for (int16_t x = 0; x < W; ++x)
            {
                const uint8_t v = static_cast<uint8_t>((x + y * 3) % 16);
                cv.a->setPixel(x, y, v);
                cv.c->setPixel(x, y, Pixel4(v));
            }
        maskPlane(*cv.a, cv.pa.data());
        unpackPlane(*cv.c, cv.pc.data());
        PlaneDiff d = diffPlanes(cv.pa.data(), cv.pc.data());
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "identity pattern diverged: %zu px, first (%d,%d) %u vs %u",
                 d.count, d.fx, d.fy, d.lhs, d.rhs);
        reportSelfcheck("substrate identity (setPixel 0..15 pattern, 127x127)",
                        d.equal, detail);
    }

    // Canvas8::drawLine and Primitives::drawLine are the same Bresenham walk,
    // so a BASE-member vs HEAD-static pair must be green end to end — proves
    // the runner exercises both call paths correctly.
    void selfcheckCrossPathLine()
    {
        Canvases &cv = canvases();
        cv.a->clear(0);
        cv.c->clear(Pixel4(0));
        cv.a->drawLine(3, 5, 120, 97, 15);
        cv.a->drawLine(120, 5, 3, 97, 9);
        cv.a->drawLine(0, 63, 126, 63, 4);
        Primitives<Pixel4>::drawLine(*cv.c, 3, 5, 120, 97, Pixel4(15));
        Primitives<Pixel4>::drawLine(*cv.c, 120, 5, 3, 97, Pixel4(9));
        Primitives<Pixel4>::drawLine(*cv.c, 0, 63, 126, 63, Pixel4(4));
        maskPlane(*cv.a, cv.pa.data());
        unpackPlane(*cv.c, cv.pc.data());
        PlaneDiff d = diffPlanes(cv.pa.data(), cv.pc.data());
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "byte-identical Bresenham diverged: %zu px, first (%d,%d)",
                 d.count, d.fx, d.fy);
        reportSelfcheck("cross-path identity (drawLine, byte-identical Bresenham)",
                        d.equal, detail);
    }

    // A deliberately-different pair of draws must produce a red verdict with
    // a sane diff count — proves the bench can actually fail.
    void selfcheckDetectsDivergence()
    {
        Canvases &cv = canvases();
        cv.a->clear(0);
        cv.c->clear(Pixel4(0));
        cv.a->fillRect(10, 10, 8, 8, 15);
        Primitives<Pixel4>::fillRect(*cv.c, Rect(12, 10, 8, 8), Pixel4(15));
        maskPlane(*cv.a, cv.pa.data());
        unpackPlane(*cv.c, cv.pc.data());
        PlaneDiff d = diffPlanes(cv.pa.data(), cv.pc.data());
        reportSelfcheck("divergence detection (2px-shifted fillRect reads red)",
                        !d.equal && d.count == 32,
                        "a knowingly-different draw compared equal (or wrong count)");
    }

    // --- Tier 3: pixel substrate ------------------------------------------

    void sweepSubstrateSetPixel()
    {
        bench.beginPair({"substrate.setPixel", "TIER3",
                         "Canvas8::setPixel", "Canvas4::setPixel"});
        if (!bench.cur->retired)
        {
            // Every in-bounds (x,y) plus a 2-px out-of-bounds ring.
            for (int16_t y = -2; y <= H + 1; ++y)
                for (int16_t x = -2; x <= W + 1; ++x)
                {
                    const uint8_t v = static_cast<uint8_t>(((x & 7) + (y & 7)) % 15 + 1);
                    runPixelCase(bench, {{"x", x}, {"y", y}},
                                 [=](Base8 &c) { c.setPixel(x, y, v); },
                                 [=](Base8 &c) { c.setPixel(x, y, v); },
                                 [=](Head4 &c) { c.setPixel(x, y, Pixel4(v)); });
                }
        }
        endPair();
    }

    void sweepSubstrateDrawPixel()
    {
        bench.beginPair({"substrate.drawPixel", "TIER3",
                         "Canvas8::drawPixel", "Canvas4::setPixel"});
        if (!bench.cur->retired)
        {
            // Canvas8::drawPixel is a verbatim one-line alias of setPixel, so
            // the exhaustive position pass lives in substrate.setPixel
            // (#158's ~17,100-case row); this pair pins the alias itself on
            // the diagonal plus the out-of-bounds boundary.
            std::vector<std::pair<int16_t, int16_t>> pts;
            for (int16_t t = 0; t < W; ++t)
                pts.push_back({t, t});
            for (int16_t y = -1; y <= H; ++y)
            {
                pts.push_back({-1, y});
                pts.push_back({W, y});
            }
            for (int16_t x = 0; x < W; ++x)
            {
                pts.push_back({x, -1});
                pts.push_back({x, H});
            }
            for (auto [x, y] : pts)
            {
                const uint8_t v = static_cast<uint8_t>(((x & 3) + (y & 3)) % 15 + 1);
                runPixelCase(bench, {{"x", x}, {"y", y}},
                             [=](Base8 &c) { c.drawPixel(x, y, v); },
                             [=](Base8 &c) { c.drawPixel(x, y, v); },
                             [=](Head4 &c) { c.setPixel(x, y, Pixel4(v)); });
            }
        }
        endPair();
    }

    void sweepSubstrateGetPixel()
    {
        bench.beginPair({"substrate.getPixel", "TIER3",
                         "Canvas8::getPixel", "Canvas4::getPixel"});
        if (!bench.cur->retired)
        {
            // A written gradient must read back identically through both
            // substrates at EVERY (x,y), including the out-of-bounds ring
            // (both sides define 0 there). Canvas8::getPixel is both the
            // BASE call and the HEAD-@8bpp form, so A == B by construction
            // and any divergence here is the depth axis (Canvas4 packing).
            Canvases &cv = canvases();
            cv.a->clear(0);
            cv.c->clear(Pixel4(0));
            for (int16_t y = 0; y < H; ++y)
                for (int16_t x = 0; x < W; ++x)
                {
                    const uint8_t v = static_cast<uint8_t>((x * 7 + y * 13) & 15);
                    cv.a->setPixel(x, y, v);
                    cv.c->setPixel(x, y, Pixel4(v));
                }
            for (int16_t y = -2; y <= H + 1; ++y)
                for (int16_t x = -2; x <= W + 1; ++x)
                {
                    const uint8_t a = cv.a->getPixel(x, y) & 0x0F;
                    const uint8_t c = cv.c->getPixel(x, y).value;
                    char detail[96];
                    snprintf(detail, sizeof(detail),
                             "getPixel(%d,%d): BASE %u HEAD %u", x, y, a, c);
                    runMetricCase(bench, {{"x", x}, {"y", y}}, true, a == c, a == c,
                                  static_cast<size_t>(a > c ? a - c : c - a), detail);
                }
        }
        endPair();
    }

    void sweepSubstrateFillScreen()
    {
        bench.beginPair({"substrate.fillScreen", "TIER3",
                         "Canvas8::fillScreen", "Canvas4::clear"});
        if (!bench.cur->retired)
        {
            for (int v = 0; v < 16; ++v)
            {
                runPixelCase(bench, {{"level", v}},
                             [=](Base8 &c) { c.fillScreen(static_cast<uint8_t>(v)); },
                             [=](Base8 &c) { c.fillScreen(static_cast<uint8_t>(v)); },
                             [=](Head4 &c) { c.clear(Pixel4(static_cast<uint8_t>(v))); });
            }
            // The two gradient planes exercise every packed-nibble phase.
            for (int mode = 0; mode < 2; ++mode)
            {
                auto val = [mode](int16_t x, int16_t y) -> uint8_t
                {
                    return mode == 0 ? static_cast<uint8_t>((x + y) & 15)
                                     : static_cast<uint8_t>((x * 7 + y * 13) & 15);
                };
                runPixelCase(bench, {{"gradient", mode}},
                             [=](Base8 &c)
                             {
                                 for (int16_t y = 0; y < H; ++y)
                                     for (int16_t x = 0; x < W; ++x)
                                         c.setPixel(x, y, val(x, y));
                             },
                             [=](Base8 &c)
                             {
                                 for (int16_t y = 0; y < H; ++y)
                                     for (int16_t x = 0; x < W; ++x)
                                         c.setPixel(x, y, val(x, y));
                             },
                             [=](Head4 &c)
                             {
                                 for (int16_t y = 0; y < H; ++y)
                                     for (int16_t x = 0; x < W; ++x)
                                         c.setPixel(x, y, Pixel4(val(x, y)));
                             });
            }
        }
        endPair();
    }

    // --- Tier 1: geometry --------------------------------------------------

    void sweepDrawLine()
    {
        bench.beginPair({"geom.drawLine", "TIER1",
                         "Canvas8::drawLine", "Primitives<Pixel4>::drawLine"});
        if (!bench.cur->retired)
        {
            auto runLine = [](int16_t x0, int16_t y0, int16_t x1, int16_t y1)
            {
                runPixelCase(bench,
                             {{"x0", x0}, {"y0", y0}, {"x1", x1}, {"y1", y1}},
                             [=](Base8 &c) { c.drawLine(x0, y0, x1, y1, 15); },
                             [=](Base8 &c) { Primitives<uint8_t>::drawLine(c, x0, y0, x1, y1, 15); },
                             [=](Head4 &c) { Primitives<Pixel4>::drawLine(c, x0, y0, x1, y1, Pixel4(15)); });
            };
            // Center to every perimeter pixel (all slopes, all octants).
            for (int16_t x = 0; x < W; ++x)
            {
                runLine(63, 63, x, 0);
                runLine(63, 63, x, H - 1);
            }
            for (int16_t y = 1; y < H - 1; ++y)
            {
                runLine(63, 63, 0, y);
                runLine(63, 63, W - 1, y);
            }
            // Every eighth perimeter target also reversed (direction symmetry).
            for (int16_t x = 0; x < W; x += 8)
            {
                runLine(x, 0, 63, 63);
                runLine(x, H - 1, 63, 63);
            }
            // Every axis-aligned row and column.
            for (int16_t y = 0; y < H; ++y)
                runLine(0, y, W - 1, y);
            for (int16_t x = 0; x < W; ++x)
                runLine(x, 0, x, H - 1);
            // Degenerate + off-canvas endpoints (mandatory edge set).
            runLine(10, 10, 10, 10);
            runLine(0, 0, 0, 0);
            runLine(126, 126, 126, 126);
            runLine(-10, -10, 140, 140);
            runLine(-5, 63, 131, 63);
            runLine(63, -5, 63, 131);
            runLine(-20, 5, 5, -20);
            runLine(140, 63, 63, 140);
            runLine(-10, 140, 140, -10);
            runLine(-30, -30, -5, -5);
            runLine(130, 130, 140, 140);
            runLine(63, 63, 200, 63);
        }
        endPair();
    }

    const int16_t kRectXY[] = {-5, 0, 1, 63, 125, 126, 130};
    const int16_t kRectWH[] = {-1, 0, 1, 2, 3, 63, 126, 127, 128};

    void sweepFillRect()
    {
        bench.beginPair({"geom.fillRect", "TIER1",
                         "Canvas8::fillRect", "Primitives<Pixel4>::fillRect"});
        if (!bench.cur->retired)
        {
            for (int16_t x : kRectXY)
                for (int16_t y : kRectXY)
                    for (int16_t w : kRectWH)
                        for (int16_t h : kRectWH)
                        {
                            // HEAD call sites hand int16 extents to Rect's
                            // uint16 fields; the cast below is that exact
                            // conversion, negative extents included.
                            const Rect r(x, y, static_cast<uint16_t>(w),
                                         static_cast<uint16_t>(h));
                            runPixelCase(bench,
                                         {{"x", x}, {"y", y}, {"w", w}, {"h", h}},
                                         [=](Base8 &c) { c.fillRect(x, y, w, h, 15); },
                                         [=](Base8 &c) { Primitives<uint8_t>::fillRect(c, r, 15); },
                                         [=](Head4 &c) { Primitives<Pixel4>::fillRect(c, r, Pixel4(15)); });
                        }
        }
        endPair();
    }

    void sweepDrawRect()
    {
        bench.beginPair({"geom.drawRect", "TIER1",
                         "Canvas8::drawRect", "Primitives<Pixel4>::drawRect"});
        if (!bench.cur->retired)
        {
            for (int16_t x : kRectXY)
                for (int16_t y : kRectXY)
                    for (int16_t w : kRectWH)
                        for (int16_t h : kRectWH)
                        {
                            const Rect r(x, y, static_cast<uint16_t>(w),
                                         static_cast<uint16_t>(h));
                            runPixelCase(bench,
                                         {{"x", x}, {"y", y}, {"w", w}, {"h", h}},
                                         [=](Base8 &c) { c.drawRect(x, y, w, h, 15); },
                                         [=](Base8 &c) { Primitives<uint8_t>::drawRect(c, r, 15); },
                                         [=](Head4 &c) { Primitives<Pixel4>::drawRect(c, r, Pixel4(15)); });
                        }
        }
        endPair();
    }

    const int16_t kRRXY[] = {0, 1, 63, 120};
    const int16_t kRRWH[] = {4, 8, 63, 127};
    const int16_t kRRRad[] = {0, 1, 2, 3, 4, 8, 16, 31, 63};

    // Mandatory edge set for the round-rect pairs, on top of the base grid
    // (#158: fully off-canvas, partial clip, zero/negative extent).
    struct RREdgeCase
    {
        int16_t x, y, w, h, r;
    };
    const RREdgeCase kRREdgeCases[] = {
        {-5, -5, 4, 4, 2},     {-5, -5, 63, 63, 8},  // partial clip, top-left
        {125, 125, 8, 8, 2},   {125, 63, 63, 63, 8}, // partial clip, right/bottom
        {-5, 63, 63, 8, 4},    {63, -5, 8, 63, 4},   // partial clip, one edge
        {-140, -140, 63, 63, 8}, {200, 200, 63, 63, 8}, // fully off-canvas
        {10, 10, 0, 0, 0},     {10, 10, 0, 8, 2},    // zero extent
        {10, 10, 8, 0, 2},     {10, 10, -1, 8, 2},   // negative extent
        {10, 10, 8, -1, 2},    {10, 10, -1, -1, 0}};

    template <typename Fn>
    void forEachRoundRectCase(Fn &&run)
    {
        for (int16_t x : kRRXY)
            for (int16_t y : kRRXY)
                for (int16_t w : kRRWH)
                    for (int16_t h : kRRWH)
                        for (int16_t r : kRRRad)
                            run(x, y, w, h, r);
        for (const RREdgeCase &e : kRREdgeCases)
            run(e.x, e.y, e.w, e.h, e.r);
    }

    void sweepFillRoundRect()
    {
        bench.beginPair({"geom.fillRoundRect", "TIER1",
                         "Canvas8::fillRoundRect", "Primitives<Pixel4>::fillRoundRect"});
        if (!bench.cur->retired)
        {
            forEachRoundRectCase([](int16_t x, int16_t y, int16_t w, int16_t h, int16_t r)
            {
                const Rect rect(x, y, static_cast<uint16_t>(w),
                                static_cast<uint16_t>(h));
                runPixelCase(bench,
                             {{"x", x}, {"y", y}, {"w", w}, {"h", h}, {"r", r}},
                             [=](Base8 &c) { c.fillRoundRect(x, y, w, h, r, 15); },
                             [=](Base8 &c) { Primitives<uint8_t>::fillRoundRect(c, rect, r, 15); },
                             [=](Head4 &c) { Primitives<Pixel4>::fillRoundRect(c, rect, r, Pixel4(15)); });
            });
        }
        endPair();
    }

    void sweepDrawRoundRect()
    {
        bench.beginPair({"geom.drawRoundRect", "TIER1",
                         "Canvas8::drawRoundRect", "Primitives<Pixel4>::drawRoundRect"});
        if (!bench.cur->retired)
        {
            forEachRoundRectCase([](int16_t x, int16_t y, int16_t w, int16_t h, int16_t r)
            {
                const Rect rect(x, y, static_cast<uint16_t>(w),
                                static_cast<uint16_t>(h));
                runPixelCase(bench,
                             {{"x", x}, {"y", y}, {"w", w}, {"h", h}, {"r", r}},
                             [=](Base8 &c) { c.drawRoundRect(x, y, w, h, r, 15); },
                             [=](Base8 &c) { Primitives<uint8_t>::drawRoundRect(c, rect, r, 15); },
                             [=](Head4 &c) { Primitives<Pixel4>::drawRoundRect(c, rect, r, Pixel4(15)); });
            });
        }
        endPair();
    }

    void sweepFillTriangle()
    {
        bench.beginPair({"geom.fillTriangle", "TIER1",
                         "Canvas8::fillTriangle", "Primitives<Pixel4>::fillTriangle"});
        if (!bench.cur->retired)
        {
            auto runTri = [](int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             int16_t x2, int16_t y2)
            {
                runPixelCase(bench,
                             {{"x0", x0}, {"y0", y0}, {"x1", x1}, {"y1", y1}, {"x2", x2}, {"y2", y2}},
                             [=](Base8 &c) { c.fillTriangle(x0, y0, x1, y1, x2, y2, 15); },
                             [=](Base8 &c) { Primitives<uint8_t>::fillTriangle(c, x0, y0, x1, y1, x2, y2, 15); },
                             [=](Head4 &c) { Primitives<Pixel4>::fillTriangle(c, x0, y0, x1, y1, x2, y2, Pixel4(15)); });
            };
            // Mandatory degenerate/edge set.
            runTri(50, 50, 50, 50, 50, 50);    // all vertices coincident
            runTri(10, 10, 60, 10, 30, 10);    // collinear horizontal
            runTri(10, 10, 10, 60, 10, 30);    // collinear vertical
            runTri(0, 0, 40, 40, 80, 80);      // collinear diagonal
            runTri(20, 20, 20, 20, 90, 40);    // two coincident
            runTri(20, 20, 90, 40, 90, 40);    // two coincident (other pair)
            runTri(0, 0, 126, 0, 63, 126);     // spans the canvas
            runTri(0, 0, 0, 126, 126, 63);     // spans, rotated
            runTri(-20, -20, 60, 10, 10, 60);  // one vertex off-canvas
            runTri(63, 63, 150, 40, 140, 90);  // two vertices off-canvas
            runTri(-30, 63, 160, -30, 160, 150); // all off, crosses canvas
            runTri(-50, -50, -10, -30, -30, -10); // fully off-canvas
            runTri(5, 5, 6, 5, 5, 6);          // minimal area
            runTri(0, 0, 1, 0, 0, 1);          // minimal at origin
            runTri(125, 125, 126, 125, 125, 126); // minimal at far corner
            runTri(63, 0, 0, 126, 126, 126);   // tall isoceles
            runTri(0, 63, 126, 0, 126, 126);   // wide isoceles
            runTri(10, 120, 60, 5, 110, 120);  // tall, unsorted vertex order
            runTri(110, 120, 10, 120, 60, 5);  // same triangle, rotated order
            runTri(60, 5, 110, 120, 10, 120);  // same triangle, third order
            // 500 fixed-seed vertex triples (deterministic LCG), coordinates
            // in [-20, 146] so clipping paths stay exercised.
            uint32_t seed = 0x1234ABCD;
            auto next = [&seed]() -> int16_t
            {
                seed = seed * 1664525u + 1013904223u;
                return static_cast<int16_t>((seed >> 16) % 167) - 20;
            };
            for (int i = 0; i < 500; ++i)
            {
                int16_t x0 = next(), y0 = next(), x1 = next(), y1 = next(),
                        x2 = next(), y2 = next();
                runTri(x0, y0, x1, y1, x2, y2);
            }
        }
        endPair();
    }

    // --- Tier 2: the #161 regression guards --------------------------------

    const std::pair<int16_t, int16_t> kCircleCenters[] = {
        {63, 63}, {0, 0}, {126, 0}, {0, 126}, {126, 126},
        {63, 0}, {0, 63}, {126, 63}, {63, 126}, {150, 63}};

    void sweepCircleGuards()
    {
        bench.beginPair({"guard.fillCircle", "TIER2 GUARD",
                         "Canvas8::fillCircle", "Primitives<Pixel4>::fillCircle"});
        if (!bench.cur->retired)
        {
            for (auto [cx, cy] : kCircleCenters)
                for (int16_t r = 0; r <= 90; ++r)
                    runPixelCase(bench, {{"cx", cx}, {"cy", cy}, {"r", r}},
                                 [=](Base8 &c) { c.fillCircle(cx, cy, r, 15); },
                                 [=](Base8 &c) { Primitives<uint8_t>::fillCircle(c, cx, cy, r, 15); },
                                 [=](Head4 &c) { Primitives<Pixel4>::fillCircle(c, cx, cy, r, Pixel4(15)); });
        }
        endPair();

        bench.beginPair({"guard.drawCircle", "TIER2 GUARD",
                         "Canvas8::drawCircle", "Primitives<Pixel4>::drawCircle"});
        if (!bench.cur->retired)
        {
            for (auto [cx, cy] : kCircleCenters)
                for (int16_t r = 0; r <= 90; ++r)
                    runPixelCase(bench, {{"cx", cx}, {"cy", cy}, {"r", r}},
                                 [=](Base8 &c) { c.drawCircle(cx, cy, r, 15); },
                                 [=](Base8 &c) { Primitives<uint8_t>::drawCircle(c, cx, cy, r, 15); },
                                 [=](Head4 &c) { Primitives<Pixel4>::drawCircle(c, cx, cy, r, Pixel4(15)); });
        }
        endPair();
    }

    // --- Tier 3: drawArc (the depth-only calibration reference) ------------

    void sweepDrawArc()
    {
        bench.beginPair({"depth.drawArc", "TIER3",
                         "Primitives<uint8_t>::drawArc", "Primitives<Pixel4>::drawArc"});
        if (!bench.cur->retired)
        {
            auto runArc = [](int16_t cx, int16_t cy, int16_t r, int start, int sweep)
            {
                const float a0 = static_cast<float>(start) * static_cast<float>(M_PI) / 180.0f;
                const float a1 = static_cast<float>(start + sweep) * static_cast<float>(M_PI) / 180.0f;
                runPixelCase(bench,
                             {{"cx", cx}, {"cy", cy}, {"r", r}, {"start", start}, {"sweep", sweep}},
                             [=](Base8 &c) { Primitives<uint8_t>::drawArc(c, cx, cy, r, a0, a1, 15); },
                             [=](Base8 &c) { Primitives<uint8_t>::drawArc(c, cx, cy, r, a0, a1, 15); },
                             [=](Head4 &c) { Primitives<Pixel4>::drawArc(c, cx, cy, r, a0, a1, Pixel4(15)); });
            };
            const std::pair<int16_t, int16_t> centers[] = {
                {63, 63}, {20, 20}, {120, 63}, {63, 0}};
            const int16_t radii[] = {5, 30, 62};
            for (auto [cx, cy] : centers)
                for (int16_t r : radii)
                    for (int start = 0; start < 360; start += 10)
                        for (int sweep = 10; sweep <= 360; sweep += 10)
                            runArc(cx, cy, r, start, sweep);
            // Mandatory edge set: r = 0/1 (step = 1/r degenerates) and
            // fully/partially off-canvas centers.
            const std::pair<int16_t, int16_t> edgeCenters[] = {
                {63, 63}, {140, 80}, {-10, -10}};
            for (auto [cx, cy] : edgeCenters)
                for (int16_t r : {0, 1})
                    for (int start : {0, 90})
                        for (int sweep : {90, 360})
                            runArc(cx, cy, r, start, sweep);
            for (auto [cx, cy] : {std::pair<int16_t, int16_t>{140, 80}, {-10, -10}})
                for (int start : {0, 180})
                    for (int sweep : {180, 360})
                        runArc(cx, cy, 62, start, sweep);
        }
        endPair();
    }

} // namespace

int main(int argc, char **argv)
{
    std::string pngDir;
    bool printHashes = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
            pngDir = argv[++i];
        else if (strcmp(argv[i], "--hash") == 0)
            printHashes = true;
        else
        {
            fprintf(stderr, "usage: visual_parity_bench [--png DIR] [--hash]\n");
            return 4;
        }
    }

    if (printHashes)
    {
        printf("implementation-pin hashes (FNV-1a-64 of the graphics headers) —\n"
               "copy the relevant pair into a new tests/waivers.hpp entry:\n");
        for (const char *f : {"canvas.hpp", "primitives.hpp"})
            printf("  %-18s 0x%016llx\n", f,
                   static_cast<unsigned long long>(pinHash(f)));
        return 0;
    }

    printf("visual_parity_bench — full pair sweep, M3 (unwn #166, epic #163)\n");
    printf("plane: memcmp(Canvas8 & 0x0F, Canvas4 unpacked) on 127x127; identity on 0-15\n");
    printf("A: BASE algo @8bpp   B: HEAD algo @8bpp   C: HEAD algo @4bpp\n");
    printf("A!=B labels ALGORITHM, B!=C labels DEPTH\n\n");

    selfcheckSubstrateIdentity();
    selfcheckCrossPathLine();
    selfcheckDetectsDivergence();
    printf("\n");

    sweepSubstrateSetPixel();
    sweepSubstrateDrawPixel();
    sweepSubstrateGetPixel();
    sweepSubstrateFillScreen();
    sweepDrawLine();
    sweepFillRect();
    sweepDrawRect();
    sweepFillRoundRect();
    sweepDrawRoundRect();
    sweepFillTriangle();
    sweepCircleGuards();
    sweepDrawArc();

    printSignatures(bench);
    printCensus(bench);

    if (!pngDir.empty() && !bench.signatures.empty())
    {
        printf("\nreview PNGs (BASE | HEAD | diff, x4 NN, diff magenta):\n");
        auto files = writeReviewPngs(bench, pngDir);
        for (const auto &f : files)
            printf("  %s\n", f.c_str());
        if (!files.empty())
            printf("deliver to the phone (explicit, advisory): tailscale file cp %s/*.png <phone>:\n",
                   pngDir.c_str());
    }

    const auto violations = bench.censusViolations();
    printf("\nsummary: %zu pairs, %zu cases, %zu failed, %zu signatures | selfchecks %s\n",
           bench.pairs.size(), bench.totalCases(), bench.totalFailed(),
           bench.signatures.size(),
           bench.selfcheckFailures == 0 ? "PASS" : "FAIL");
    if (bench.totalFailed() > 0)
        printf("RED signatures are findings for adjudication on the execution epic (unwn #163):\n"
               "the #158 derivation already predicted the >>4-vs-&0xF asset reduction, the blend\n"
               "saturation ceiling and the compositor's structural divergences. Do not fix-and-\n"
               "rerun to green without a ticket; do not waive without an adjudication ticket.\n");

    if (bench.selfcheckFailures > 0)
        return 2; // harness bug — never acceptable
    if (!violations.empty())
        return 3; // waiver census hard gate
    return bench.totalFailed() > 0 ? 1 : 0;
}
