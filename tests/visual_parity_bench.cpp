// Visual Parity Bench — full pair sweep (M3, unwn #166, epic #163).
//
// One-binary call-path differential harness (Visual-Parity-Bench-Design):
// this tree contains both sides of the Eisei enjin migration — Canvas8 with
// every BASE drawing algorithm intact, plus Canvas4 / Primitives<TPixel> /
// TextRenderer / the blit compositor — so BASE and HEAD run in-process and
// comparison is an in-process memcmp. The unit of test is the *substitution
// pair*: (what BASE Eisei actually called, what HEAD Eisei actually calls) —
// a call-site substitution, never a name match (§ 2).
//
// Pixel pairs run the A/B/C decomposition (§ 4) where a HEAD-@8bpp form
// exists:
//   A: BASE algorithm @8bpp (Canvas8 member)
//   B: HEAD algorithm @8bpp (Primitives<uint8_t> / TextRenderer<uint8_t>)
//   C: HEAD algorithm @4bpp (Primitives<Pixel4> / TextRenderer<Pixel4> /
//      enjin::blit* on Canvas4)
// A!=B labels ALGORITHM, B!=C labels DEPTH. The blit compositor is
// Pixel4-native, so its 11 pairs compare A vs C only and their failures are
// labelled accordingly (axes entangled).
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
// (a pair > 50% waived).
//
// Deliberately NOT registered with add_test (§ 11): the bench is a local
// instrument, and its first M3 runs are EXPECTED red — the #158 derivation
// already flagged the >>4-vs-&0xF asset reduction, the blend saturation
// ceiling and four structural compositor divergences for adjudication on
// the execution epic. The waiver machinery itself is CI-tested in
// tests/waiver_machinery_test.cpp.

#include <enjin2/graphics/blit.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/primitives.hpp>
#include <enjin2/graphics/text_renderer.hpp>

#include "visual_parity/bench_support.hpp"
#include "visual_parity/png_review.hpp"

#include "visual_parity/assets/advanced_icons.h"
#include "visual_parity/assets/lock.h"
#include "visual_parity/fonts/fonts.hpp"

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
            // Written value must read back identically through both
            // substrates, including out-of-bounds reads (both sides define 0).
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
            for (int16_t y = -2; y <= H + 1; y += (y == 20 ? 87 : 1)) // edges + two interior bands
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

    void sweepFillRoundRect()
    {
        bench.beginPair({"geom.fillRoundRect", "TIER1",
                         "Canvas8::fillRoundRect", "Primitives<Pixel4>::fillRoundRect"});
        if (!bench.cur->retired)
        {
            for (int16_t x : kRRXY)
                for (int16_t y : kRRXY)
                    for (int16_t w : kRRWH)
                        for (int16_t h : kRRWH)
                            for (int16_t r : kRRRad)
                            {
                                const Rect rect(x, y, static_cast<uint16_t>(w),
                                                static_cast<uint16_t>(h));
                                runPixelCase(bench,
                                             {{"x", x}, {"y", y}, {"w", w}, {"h", h}, {"r", r}},
                                             [=](Base8 &c) { c.fillRoundRect(x, y, w, h, r, 15); },
                                             [=](Base8 &c) { Primitives<uint8_t>::fillRoundRect(c, rect, r, 15); },
                                             [=](Head4 &c) { Primitives<Pixel4>::fillRoundRect(c, rect, r, Pixel4(15)); });
                            }
        }
        endPair();
    }

    void sweepDrawRoundRect()
    {
        bench.beginPair({"geom.drawRoundRect", "TIER1",
                         "Canvas8::drawRoundRect", "Primitives<Pixel4>::drawRoundRect"});
        if (!bench.cur->retired)
        {
            for (int16_t x : kRRXY)
                for (int16_t y : kRRXY)
                    for (int16_t w : kRRWH)
                        for (int16_t h : kRRWH)
                            for (int16_t r : kRRRad)
                            {
                                const Rect rect(x, y, static_cast<uint16_t>(w),
                                                static_cast<uint16_t>(h));
                                runPixelCase(bench,
                                             {{"x", x}, {"y", y}, {"w", w}, {"h", h}, {"r", r}},
                                             [=](Base8 &c) { c.drawRoundRect(x, y, w, h, r, 15); },
                                             [=](Base8 &c) { Primitives<uint8_t>::drawRoundRect(c, rect, r, 15); },
                                             [=](Head4 &c) { Primitives<Pixel4>::drawRoundRect(c, rect, r, Pixel4(15)); });
                            }
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
            const std::pair<int16_t, int16_t> centers[] = {
                {63, 63}, {20, 20}, {120, 63}, {63, 0}};
            const int16_t radii[] = {5, 30, 62};
            for (auto [cx, cy] : centers)
                for (int16_t r : radii)
                    for (int start = 0; start < 360; start += 10)
                        for (int sweep = 10; sweep <= 360; sweep += 10)
                        {
                            const float a0 = static_cast<float>(start) * static_cast<float>(M_PI) / 180.0f;
                            const float a1 = static_cast<float>(start + sweep) * static_cast<float>(M_PI) / 180.0f;
                            runPixelCase(bench,
                                         {{"cx", cx}, {"cy", cy}, {"r", r}, {"start", start}, {"sweep", sweep}},
                                         [=](Base8 &c) { Primitives<uint8_t>::drawArc(c, cx, cy, r, a0, a1, 15); },
                                         [=](Base8 &c) { Primitives<uint8_t>::drawArc(c, cx, cy, r, a0, a1, 15); },
                                         [=](Head4 &c) { Primitives<Pixel4>::drawArc(c, cx, cy, r, a0, a1, Pixel4(15)); });
                        }
        }
        endPair();
    }

    // --- Tier 1: the 11 compositing pairs ----------------------------------
    //
    // BASE composites raw 8-bit values into Canvas8 (the panel's & 0xF is the
    // only reduction); HEAD's blit family is Pixel4-native and reduces every
    // source sample >> 4 on ingestion. The #158 derivation flagged exactly
    // this: for Eisei's shipped 4-bit-range assets (advanced_icons max 0x01,
    // lock max 0x0a) BASE yields n where HEAD yields 0, and for 0-255 data
    // BASE yields n & 15 where HEAD yields n >> 4. These sweeps put numbers
    // and pixels on that finding; adjudication happens on the epic.

    struct Src
    {
        const char *name;
        const uint8_t *data;
        int16_t w, h;
    };

    uint8_t enum256[256];
    uint8_t synth255[23 * 19];

    std::vector<Src> sources()
    {
        for (int i = 0; i < 256; ++i)
            enum256[i] = static_cast<uint8_t>(i);
        for (int y = 0; y < 19; ++y)
            for (int x = 0; x < 23; ++x)
                synth255[y * 23 + x] =
                    static_cast<uint8_t>((x * 255 / 22 + y * 255 / 18) / 2);
        return {
            {"enum256", enum256, 16, 16},        // every 8-bit value once
            {"adv_icon", adv_icon_off, 16, 16},  // shipped asset, values {0,1}
            {"lock", lock_18x18, 18, 18},        // shipped asset, values {0,1,10}
            {"synth255", synth255, 23, 19},      // full-range gradient
        };
    }

    // 9 fully on-canvas, 8 partial-clip (each edge + each corner), 4 fully off.
    const std::pair<int16_t, int16_t> kPlacements[] = {
        {2, 2}, {54, 2}, {104, 2}, {2, 54}, {54, 54}, {104, 54}, {2, 104}, {54, 104}, {104, 104},
        {-8, 30}, {120, 30}, {30, -8}, {30, 118}, {-8, -8}, {120, -8}, {-8, 118}, {120, 118},
        {-40, -40}, {200, 10}, {10, 200}, {-40, 140}};

    const uint8_t kPrefills[] = {0, 5};
    const int kMattes[] = {0, 1, 16, 255};
    const int kDivisors[] = {1, 2, 3, 4, 8};

    // HEAD-side source ingestion: on HEAD, offscreen Pixel4 sources are built
    // from the same 8-bit data through blitGray8 (>> 4) — that is the call
    // path, so the pair drives it, not a hand-made conversion.
    struct SmallCanvas4 : ICanvas<Pixel4>
    {
        int16_t w_, h_;
        std::vector<uint8_t> px;
        SmallCanvas4(int16_t w, int16_t h) : w_(w), h_(h), px(static_cast<size_t>(w) * h, 0) {}
        uint16_t getWidth() const override { return w_; }
        uint16_t getHeight() const override { return h_; }
        void setPixel(int16_t x, int16_t y, Pixel4 c) override
        {
            if (x < 0 || x >= w_ || y < 0 || y >= h_)
                return;
            px[static_cast<size_t>(y) * w_ + x] = c.value;
        }
        Pixel4 getPixel(int16_t x, int16_t y) const override
        {
            if (x < 0 || x >= w_ || y < 0 || y >= h_)
                return Pixel4(0);
            return Pixel4(px[static_cast<size_t>(y) * w_ + x]);
        }
        void clear(Pixel4 c = Pixel4(0)) override
        {
            std::fill(px.begin(), px.end(), c.value);
        }
        void fill(const Rect &, Pixel4) override {}
    };

    SmallCanvas4 ingest(const Src &s)
    {
        SmallCanvas4 c(s.w, s.h);
        blitGray8(c, 0, 0, s.data, s.w, s.h);
        return c;
    }

    // 1-bpp mask adapter over BASE's 8-bit mask plane (maskValue > 0 draws) —
    // the shape blitCanvasMasked templates on (GFXcanvas1-like getPixel).
    struct MaskView
    {
        const uint8_t *data;
        int16_t w, h;
        bool getPixel(int16_t x, int16_t y) const
        {
            if (x < 0 || x >= w || y < 0 || y >= h)
                return false;
            return data[static_cast<size_t>(y) * w + x] > 0;
        }
    };

    std::vector<uint8_t> makeMask(int kind, int16_t w, int16_t h)
    {
        std::vector<uint8_t> m(static_cast<size_t>(w) * h, 0);
        for (int16_t y = 0; y < h; ++y)
            for (int16_t x = 0; x < w; ++x)
            {
                bool on = false;
                if (kind == 0)
                    on = true; // full
                else if (kind == 1)
                    on = ((x + y) & 1) != 0; // checkerboard
                else
                {
                    const int dx = x - w / 2, dy = y - h / 2;
                    const int r2 = dx * dx + dy * dy;
                    const int lim = (w / 2) * (w / 2);
                    on = r2 <= lim && r2 >= lim / 4; // ring
                }
                m[static_cast<size_t>(y) * w + x] = on ? 255 : 0;
            }
        return m;
    }

    void sweepBlitGray8()
    {
        auto srcs = sources();

        bench.beginPair({"blit.gray8", "TIER1",
                         "Canvas8::drawGrayscaleBitmap(x,y,bmp,w,h)", "enjin::blitGray8"});
        if (!bench.cur->retired)
        {
            for (size_t si = 0; si < srcs.size(); ++si)
                for (size_t pi = 0; pi < std::size(kPlacements); ++pi)
                {
                    const Src &s = srcs[si];
                    auto [x, y] = kPlacements[pi];
                    runPixelCase(bench,
                                 {{"src", static_cast<int32_t>(si)}, {"x", x}, {"y", y}},
                                 [=](Base8 &c) { c.drawGrayscaleBitmap(x, y, s.data, s.w, s.h); },
                                 nullptr,
                                 [=](Head4 &c) { blitGray8(c, x, y, s.data, s.w, s.h); });
                }
        }
        endPair();

        bench.beginPair({"blit.gray8Matte", "TIER1",
                         "Canvas8::drawGrayscaleBitmap(..,matte,..)", "enjin::blitGray8Matte"});
        if (!bench.cur->retired)
        {
            for (size_t si = 0; si < srcs.size(); ++si)
                for (size_t pi = 0; pi < std::size(kPlacements); ++pi)
                    for (int matte : kMattes)
                        for (uint8_t pre : kPrefills)
                        {
                            const Src &s = srcs[si];
                            auto [x, y] = kPlacements[pi];
                            const uint8_t m = static_cast<uint8_t>(matte);
                            runPixelCase(bench,
                                         {{"src", static_cast<int32_t>(si)}, {"x", x}, {"y", y}, {"matte", matte}, {"prefill", pre}},
                                         [=](Base8 &c) { c.drawGrayscaleBitmap(x, y, s.data, m, static_cast<uint8_t>(s.w), static_cast<uint8_t>(s.h)); },
                                         nullptr,
                                         [=](Head4 &c) { blitGray8Matte(c, x, y, s.data, m, s.w, s.h); },
                                         pre);
                        }
        }
        endPair();

        bench.beginPair({"blit.gray8Opacity", "TIER1",
                         "Canvas8::drawGrayscaleBitmap(..,matte,..,divisor)", "enjin::blitGray8Opacity"});
        if (!bench.cur->retired)
        {
            for (size_t si = 0; si < srcs.size(); ++si)
                for (size_t pi = 0; pi < std::size(kPlacements); ++pi)
                    for (int matte : kMattes)
                        for (int div : kDivisors)
                            for (uint8_t pre : kPrefills)
                            {
                                const Src &s = srcs[si];
                                auto [x, y] = kPlacements[pi];
                                const uint8_t m = static_cast<uint8_t>(matte);
                                const uint8_t d = static_cast<uint8_t>(div);
                                runPixelCase(bench,
                                             {{"src", static_cast<int32_t>(si)}, {"x", x}, {"y", y}, {"matte", matte}, {"divisor", div}, {"prefill", pre}},
                                             [=](Base8 &c) { c.drawGrayscaleBitmap(x, y, s.data, m, static_cast<uint8_t>(s.w), static_cast<uint8_t>(s.h), d); },
                                             nullptr,
                                             [=](Head4 &c) { blitGray8Opacity(c, x, y, s.data, m, s.w, s.h, d); },
                                             pre);
                            }
        }
        endPair();
    }

    void sweepBlitCanvas()
    {
        auto srcs = sources();

        bench.beginPair({"blit.canvasMasked", "TIER1",
                         "Canvas8::drawGrayscaleBitmap(..,mask,..)", "enjin::blitCanvasMasked"});
        if (!bench.cur->retired)
        {
            for (size_t si = 0; si < srcs.size(); ++si)
            {
                const Src &s = srcs[si];
                SmallCanvas4 srcCanvas = ingest(s);
                for (int maskKind = 0; maskKind < 3; ++maskKind)
                {
                    std::vector<uint8_t> mask = makeMask(maskKind, s.w, s.h);
                    const MaskView mv{mask.data(), s.w, s.h};
                    for (size_t pi = 0; pi < std::size(kPlacements); ++pi)
                        for (uint8_t pre : kPrefills)
                        {
                            auto [x, y] = kPlacements[pi];
                            runPixelCase(bench,
                                         {{"src", static_cast<int32_t>(si)}, {"mask", maskKind}, {"x", x}, {"y", y}, {"prefill", pre}},
                                         [=, &mask](Base8 &c) { c.drawGrayscaleBitmap(x, y, s.data, mask.data(), static_cast<uint8_t>(s.w), static_cast<uint8_t>(s.h)); },
                                         nullptr,
                                         [=, &srcCanvas](Head4 &c) { blitCanvasMasked(c, srcCanvas, mv, x, y); },
                                         pre);
                        }
                }
            }
        }
        endPair();

        bench.beginPair({"blit.canvasOpacity", "TIER1",
                         "Canvas8::drawGrayscaleBitmap(..,matte,..,divisor)", "enjin::blitCanvasOpacity"});
        if (!bench.cur->retired)
        {
            // BASE tested transparency against the 8-bit matte; HEAD tests the
            // ingested Pixel4 source against a Pixel4(0) default (#158
            // structural finding: the transparency plane moved).
            for (size_t si = 0; si < srcs.size(); ++si)
            {
                const Src &s = srcs[si];
                SmallCanvas4 srcCanvas = ingest(s);
                for (size_t pi = 0; pi < std::size(kPlacements); ++pi)
                    for (int matte : {0, 16})
                        for (int div : kDivisors)
                            for (uint8_t pre : kPrefills)
                            {
                                auto [x, y] = kPlacements[pi];
                                const uint8_t m = static_cast<uint8_t>(matte);
                                const uint8_t d = static_cast<uint8_t>(div);
                                runPixelCase(bench,
                                             {{"src", static_cast<int32_t>(si)}, {"x", x}, {"y", y}, {"matte", matte}, {"divisor", div}, {"prefill", pre}},
                                             [=](Base8 &c) { c.drawGrayscaleBitmap(x, y, s.data, m, static_cast<uint8_t>(s.w), static_cast<uint8_t>(s.h), d); },
                                             nullptr,
                                             [=, &srcCanvas](Head4 &c) { blitCanvasOpacity(c, srcCanvas, x, y, d); },
                                             pre);
                            }
            }
        }
        endPair();
    }

    // Full-canvas textures for the origin-anchored blend pairs.
    std::vector<uint8_t> blendTexture(int kind)
    {
        std::vector<uint8_t> t(PLANE);
        for (int16_t y = 0; y < H; ++y)
            for (int16_t x = 0; x < W; ++x)
            {
                uint8_t v = 0;
                switch (kind)
                {
                case 0: v = static_cast<uint8_t>((x + y) & 15); break;          // 0-15 identity range
                case 1: v = static_cast<uint8_t>((x * 2 + y) & 0xFF); break;    // full 8-bit range
                case 2: v = static_cast<uint8_t>((y % 16) * 16 + (x % 16)); break; // tiled enum256
                default: v = lock_18x18[(y % 18) * 18 + (x % 18)]; break;       // tiled shipped asset
                }
                t[static_cast<size_t>(y) * W + x] = v;
            }
        return t;
    }

    void sweepBlendGray8()
    {
        bench.beginPair({"blit.addGray8", "TIER1",
                         "Canvas8::add(const uint8_t*)", "enjin::addGray8"});
        if (!bench.cur->retired)
        {
            for (int kind = 0; kind < 4; ++kind)
            {
                std::vector<uint8_t> tex = blendTexture(kind);
                for (int pre = 0; pre < 16; ++pre)
                    runPixelCase(bench, {{"texture", kind}, {"prefill", pre}},
                                 [&tex](Base8 &c) { c.add(tex.data()); },
                                 nullptr,
                                 [&tex](Head4 &c) { addGray8(c, tex.data(), W, H); },
                                 static_cast<uint8_t>(pre));
            }
        }
        endPair();

        bench.beginPair({"blit.subtractGray8", "TIER1",
                         "Canvas8::subtract(const uint8_t*)", "enjin::subtractGray8"});
        if (!bench.cur->retired)
        {
            for (int kind = 0; kind < 4; ++kind)
            {
                std::vector<uint8_t> tex = blendTexture(kind);
                for (int pre = 0; pre < 16; ++pre)
                    runPixelCase(bench, {{"texture", kind}, {"prefill", pre}},
                                 [&tex](Base8 &c) { c.subtract(tex.data()); },
                                 nullptr,
                                 [&tex](Head4 &c) { subtractGray8(c, tex.data(), W, H); },
                                 static_cast<uint8_t>(pre));
            }
        }
        endPair();

        bench.beginPair({"blit.differenceGray8", "TIER1",
                         "Canvas8::difference(x,y,tex,w,h)", "enjin::differenceGray8"});
        if (!bench.cur->retired)
        {
            auto srcs = sources();
            for (size_t si = 0; si < srcs.size(); ++si)
                for (size_t pi = 0; pi < std::size(kPlacements); ++pi)
                    for (int pre = 0; pre < 16; ++pre)
                    {
                        const Src &s = srcs[si];
                        auto [x, y] = kPlacements[pi];
                        runPixelCase(bench,
                                     {{"src", static_cast<int32_t>(si)}, {"x", x}, {"y", y}, {"prefill", pre}},
                                     [=](Base8 &c) { c.difference(x, y, s.data, static_cast<uint8_t>(s.w), static_cast<uint8_t>(s.h)); },
                                     nullptr,
                                     [=](Head4 &c) { differenceGray8(c, x, y, s.data, s.w, s.h); },
                                     static_cast<uint8_t>(pre));
                    }
        }
        endPair();
    }

    // Overlay canvases for the canvas-flavoured blends. On both branches the
    // overlays Eisei blends are drawn with 0-15 literals, so the pair paints
    // both sides with the identical 0-15 pattern (identity correspondence,
    // § 5) and the divergence under test is the blend arithmetic itself:
    // BASE blends at 8-bit and lets the panel mask the sum (15+10=25 -> 9);
    // HEAD clamps at 15.
    void paintOverlayPattern(int kind, Base8 &o8, Head4 &o4)
    {
        for (int16_t y = 0; y < H; ++y)
            for (int16_t x = 0; x < W; ++x)
            {
                uint8_t v = 0;
                switch (kind)
                {
                case 0: v = 8; break;
                case 1: v = static_cast<uint8_t>((x + y) & 15); break;
                case 2: v = static_cast<uint8_t>((x * 7 + y * 13) & 15); break;
                default:
                {
                    const int dx = x - 63, dy = y - 63;
                    v = (dx * dx + dy * dy <= 40 * 40) ? 15 : 0;
                    break;
                }
                }
                o8.setPixel(x, y, v);
                o4.setPixel(x, y, Pixel4(v));
            }
    }

    void sweepBlendCanvas()
    {
        static auto overlay8 = std::make_unique<Base8>();
        static auto overlay4 = std::make_unique<Head4>();

        bench.beginPair({"blit.addCanvas", "TIER1",
                         "Canvas8::add(Canvas8*)", "enjin::addCanvas"});
        if (!bench.cur->retired)
        {
            for (int kind = 0; kind < 4; ++kind)
            {
                paintOverlayPattern(kind, *overlay8, *overlay4);
                for (int pre = 0; pre < 16; ++pre)
                    runPixelCase(bench, {{"pattern", kind}, {"prefill", pre}},
                                 [](Base8 &c) { c.add(overlay8.get()); },
                                 nullptr,
                                 [](Head4 &c) { addCanvas(c, *overlay4); },
                                 static_cast<uint8_t>(pre));
            }
        }
        endPair();

        bench.beginPair({"blit.subtractCanvas", "TIER1",
                         "Canvas8::subtract(Canvas8*)", "enjin::subtractCanvas"});
        if (!bench.cur->retired)
        {
            for (int kind = 0; kind < 4; ++kind)
            {
                paintOverlayPattern(kind, *overlay8, *overlay4);
                for (int pre = 0; pre < 16; ++pre)
                    runPixelCase(bench, {{"pattern", kind}, {"prefill", pre}},
                                 [](Base8 &c) { c.subtract(overlay8.get()); },
                                 nullptr,
                                 [](Head4 &c) { subtractCanvas(c, *overlay4); },
                                 static_cast<uint8_t>(pre));
            }
        }
        endPair();
    }

    void sweepFillRectWithPattern()
    {
        bench.beginPair({"blit.pattern", "TIER1",
                         "Canvas8::fillRectWithPattern", "enjin::fillRectWithPattern"});
        if (!bench.cur->retired)
        {
            static const uint8_t checker15[4] = {0, 15, 15, 0};
            static const uint8_t checker255[4] = {0, 255, 255, 0};
            static const uint8_t dither4[4] = {0, 8, 4, 12};
            for (int i = 0; i < 256; ++i)
                enum256[i] = static_cast<uint8_t>(i);
            const Src patterns[] = {
                {"checker15", checker15, 2, 2},
                {"checker255", checker255, 2, 2},
                {"enum256", enum256, 16, 16},
                {"dither4", dither4, 2, 2}};
            const std::pair<int16_t, int16_t> sizes[] = {
                {1, 1}, {8, 8}, {16, 16}, {48, 32}, {127, 127}, {0, 0}, {-3, 5}, {5, -3}, {130, 130}};
            for (size_t pi = 0; pi < std::size(patterns); ++pi)
                for (auto [x, y] : kPlacements)
                    for (auto [w, h] : sizes)
                    {
                        const Src &p = patterns[pi];
                        runPixelCase(bench,
                                     {{"pattern", static_cast<int32_t>(pi)}, {"x", x}, {"y", y}, {"w", w}, {"h", h}},
                                     [=](Base8 &c) { c.fillRectWithPattern(x, y, w, h, p.data, p.w, p.h); },
                                     nullptr,
                                     [=](Head4 &c) { fillRectWithPattern(c, x, y, w, h, p.data, p.w, p.h); });
                    }
        }
        endPair();
    }

    // --- Tier 1 + Tier 2: text ---------------------------------------------

    struct FontFixture
    {
        const char *name;
        const GFXfont *font; // nullptr = built-in glcd
    };

    const FontFixture kFonts[] = {
        {"absolute8pt7b", &absolute8pt7b},
        {"MN80P1", &MN80P1},
        {"Awkward8pt7b", &Awkward8pt7b},
        {"glcd", nullptr}};

    // The 40 string fixtures (#158): empty, single glyphs, the M2 "Ag"
    // probe, real UI strings, exactly-at/one-past-wrap runs, all-spaces,
    // newlines/carriage returns, punctuation.
    const char *kStrings[] = {
        "", " ", "   ", "A", "g", ".", "0", "~",
        "Ag", "AV", "SATURN", "Datum 12", "SPEED", "IN TUNE", "128", "-12.5",
        "0.00", "ORBIT 4", "WARP", "SAVE PRESET", "PRESS SET TO CONFIRM",
        "ADVANCED SETTINGS", "DATUM MANAGER 000", "THE QUICK BROWN FOX",
        "the quick brown fox jumps", "MMMMMMMMMMMMMMMMMMMM",
        "iiiiiiiiiiiiiiiiiiiiiiiiiiiiii", "WWWWWWWWWW", "A B C D E F G H I J K",
        "\n", "A\nB", "AB\nCD\nEF", "\r", "A\rB", "  leading", "trailing  ",
        "mid  dle", "!@#$%^&*()", "[]{};:'\",<>/?\\|",
        "0123456789 0123456789 0123456789"};

    void sweepTextDrawChar()
    {
        bench.beginPair({"text.drawChar", "TIER1",
                         "Canvas8::drawChar", "TextRenderer<Pixel4>::drawChar"});
        if (!bench.cur->retired)
        {
            for (int fi = 0; fi < 4; ++fi)
                for (int size = 1; size <= 2; ++size)
                    for (int ch = 32; ch <= 126; ++ch)
                    {
                        const GFXfont *font = kFonts[fi].font;
                        const auto c = static_cast<unsigned char>(ch);
                        const auto sz = static_cast<uint8_t>(size);
                        runPixelCase(bench,
                                     {{"font", fi}, {"size", size}, {"glyph", ch}},
                                     [=](Base8 &cv)
                                     {
                                         cv.setFont(font);
                                         cv.drawChar(30, 60, c, 15, 15, sz);
                                     },
                                     [=](Base8 &cv)
                                     {
                                         TextRenderer<uint8_t> tr;
                                         tr.setFont(font);
                                         tr.setTextSize(sz);
                                         tr.setTextColor(15);
                                         tr.drawChar(cv, 30, 60, c);
                                     },
                                     [=](Head4 &cv)
                                     {
                                         TextRenderer<Pixel4> tr;
                                         tr.setFont(font);
                                         tr.setTextSize(sz);
                                         tr.setTextColor(Pixel4(15));
                                         tr.drawChar(cv, 30, 60, c);
                                     });
                    }
        }
        endPair();
    }

    void sweepTextPrint()
    {
        bench.beginPair({"text.print", "TIER1",
                         "Canvas8::print", "TextRenderer<Pixel4>::drawString"});
        if (!bench.cur->retired)
        {
            const int16_t x = 4, y = 60;
            for (int fi = 0; fi < 4; ++fi)
                for (int size = 1; size <= 2; ++size)
                    for (size_t si = 0; si < std::size(kStrings); ++si)
                    {
                        const GFXfont *font = kFonts[fi].font;
                        const char *str = kStrings[si];
                        const auto sz = static_cast<uint8_t>(size);
                        runPixelCase(bench,
                                     {{"font", fi}, {"size", size}, {"string", static_cast<int32_t>(si)}},
                                     [=](Base8 &cv)
                                     {
                                         cv.setFont(font);
                                         cv.setTextSize(sz);
                                         cv.setTextColor(15);
                                         cv.setCursor(x, y);
                                         cv.print(str);
                                     },
                                     [=](Base8 &cv)
                                     {
                                         TextRenderer<uint8_t> tr;
                                         tr.setFont(font);
                                         tr.setTextSize(sz);
                                         tr.setTextColor(15);
                                         tr.drawString(cv, x, y, str);
                                     },
                                     [=](Head4 &cv)
                                     {
                                         TextRenderer<Pixel4> tr;
                                         tr.setFont(font);
                                         tr.setTextSize(sz);
                                         tr.setTextColor(Pixel4(15));
                                         tr.drawString(cv, x, y, str);
                                     });
                    }
        }
        endPair();
    }

    void sweepTextPrintln()
    {
        bench.beginPair({"text.println", "TIER1",
                         "Canvas8::println + print", "TextRenderer<Pixel4>::drawString(\"a\\nb\")"});
        if (!bench.cur->retired)
        {
            // HEAD has no println: migrated call sites collapse to a '\n' in
            // the string, so the pair drives exactly that substitution. BASE
            // println advances a hardcoded 8 px; HEAD's writeChar('\n')
            // advances yAdvance * size.
            const std::pair<const char *, const char *> lines[] = {
                {"A", "B"}, {"SATURN", "ORBIT"}, {"Datum 12", "ready"},
                {"", "below-empty"}, {"WARP", ""}, {"128", "-12.5"},
                {"g", "y"}, {"SAVE", "PRESET"}, {"IN", "TUNE"}, {"MM", "WW"}};
            const int16_t x = 4, y = 40;
            for (int fi = 0; fi < 4; ++fi)
                for (int size = 1; size <= 2; ++size)
                    for (size_t li = 0; li < std::size(lines); ++li)
                    {
                        const GFXfont *font = kFonts[fi].font;
                        const auto sz = static_cast<uint8_t>(size);
                        const char *l1 = lines[li].first;
                        const char *l2 = lines[li].second;
                        const std::string joined = std::string(l1) + "\n" + l2;
                        runPixelCase(bench,
                                     {{"font", fi}, {"size", size}, {"lines", static_cast<int32_t>(li)}},
                                     [=](Base8 &cv)
                                     {
                                         cv.setFont(font);
                                         cv.setTextSize(sz);
                                         cv.setTextColor(15);
                                         cv.setCursor(x, y);
                                         cv.println(l1);
                                         cv.print(l2);
                                     },
                                     [=](Base8 &cv)
                                     {
                                         TextRenderer<uint8_t> tr;
                                         tr.setFont(font);
                                         tr.setTextSize(sz);
                                         tr.setTextColor(15);
                                         tr.drawString(cv, x, y, joined.c_str());
                                     },
                                     [=](Head4 &cv)
                                     {
                                         TextRenderer<Pixel4> tr;
                                         tr.setFont(font);
                                         tr.setTextSize(sz);
                                         tr.setTextColor(Pixel4(15));
                                         tr.drawString(cv, x, y, joined.c_str());
                                     });
                    }
        }
        endPair();
    }

    void sweepTextGetTextWidth()
    {
        bench.beginPair({"text.getTextWidth", "TIER1",
                         "Canvas8::getTextWidth", "TextRenderer<Pixel4>::getTextWidth"});
        if (!bench.cur->retired)
        {
            for (int fi = 0; fi < 4; ++fi)
                for (int size = 1; size <= 2; ++size)
                    for (size_t si = 0; si < std::size(kStrings); ++si)
                    {
                        const GFXfont *font = kFonts[fi].font;
                        const char *str = kStrings[si];
                        const auto sz = static_cast<uint8_t>(size);

                        Base8 &c8 = *canvases().a; // font/size state carrier only
                        c8.setFont(font);
                        c8.setTextSize(sz);
                        const int32_t a = c8.getTextWidth(str);
                        c8.setFont(&defaultFont8pt7b); // restore default state

                        TextRenderer<uint8_t> trB;
                        trB.setFont(font);
                        trB.setTextSize(sz);
                        const int32_t b = trB.getTextWidth(str);

                        TextRenderer<Pixel4> trC;
                        trC.setFont(font);
                        trC.setTextSize(sz);
                        const int32_t c = trC.getTextWidth(str);

                        char detail[128];
                        snprintf(detail, sizeof(detail),
                                 "width BASE=%d HEAD=%d (str %zu)", a, c, si);
                        runMetricCase(bench,
                                      {{"font", fi}, {"size", size}, {"string", static_cast<int32_t>(si)}},
                                      a == b, b == c, a == c,
                                      static_cast<size_t>(std::abs(a - c)), detail);
                    }
        }
        endPair();
    }

    void sweepTextBoundsGuard()
    {
        bench.beginPair({"guard.getTextBounds", "TIER2 GUARD",
                         "Canvas8::getTextBounds", "TextRenderer<Pixel4>::getTextBounds(wrap=127)"});
        if (!bench.cur->retired)
        {
            struct B
            {
                int16_t x1 = 0, y1 = 0;
                uint16_t w = 0, h = 0;
                bool operator==(const B &o) const
                {
                    return x1 == o.x1 && y1 == o.y1 && w == o.w && h == o.h;
                }
            };
            const int16_t x = 10, y = 64;
            for (int fi = 0; fi < 4; ++fi)
                for (int size = 1; size <= 2; ++size)
                    for (size_t si = 0; si < std::size(kStrings); ++si)
                    {
                        const GFXfont *font = kFonts[fi].font;
                        const char *str = kStrings[si];
                        const auto sz = static_cast<uint8_t>(size);

                        B a, b, c;
                        Base8 &c8 = *canvases().a;
                        c8.setFont(font);
                        c8.setTextSize(sz);
                        c8.getTextBounds(str, x, y, &a.x1, &a.y1, &a.w, &a.h);
                        c8.setFont(&defaultFont8pt7b);

                        TextRenderer<uint8_t> trB;
                        trB.setFont(font);
                        trB.setTextSize(sz);
                        trB.getTextBounds(str, x, y, &b.x1, &b.y1, &b.w, &b.h, W);

                        TextRenderer<Pixel4> trC;
                        trC.setFont(font);
                        trC.setTextSize(sz);
                        trC.getTextBounds(str, x, y, &c.x1, &c.y1, &c.w, &c.h, W);

                        const size_t mag =
                            static_cast<size_t>(std::abs(a.x1 - c.x1)) +
                            static_cast<size_t>(std::abs(a.y1 - c.y1)) +
                            static_cast<size_t>(std::abs(static_cast<int>(a.w) - static_cast<int>(c.w))) +
                            static_cast<size_t>(std::abs(static_cast<int>(a.h) - static_cast<int>(c.h)));
                        char detail[160];
                        snprintf(detail, sizeof(detail),
                                 "BASE ink box (%d,%d,%u,%u) HEAD (%d,%d,%u,%u) (str %zu)",
                                 a.x1, a.y1, a.w, a.h, c.x1, c.y1, c.w, c.h, si);
                        runMetricCase(bench,
                                      {{"font", fi}, {"size", size}, {"string", static_cast<int32_t>(si)}},
                                      a == b, b == c, a == c, mag, detail);
                    }
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
            return 2;
        }
    }

    if (printHashes)
    {
        printf("implementation-pin hashes (FNV-1a-64 of the graphics headers) —\n"
               "copy the relevant pair into a new tests/waivers.hpp entry:\n");
        for (const char *f : {"canvas.hpp", "primitives.hpp", "text_renderer.hpp", "blit.hpp"})
            printf("  %-18s 0x%016llx\n", f,
                   static_cast<unsigned long long>(pinHash(f)));
        return 0;
    }

    printf("visual_parity_bench — full pair sweep, M3 (unwn #166, epic #163)\n");
    printf("plane: memcmp(Canvas8 & 0x0F, Canvas4 unpacked) on 127x127; identity on 0-15\n");
    printf("A: BASE algo @8bpp   B: HEAD algo @8bpp   C: HEAD algo @4bpp\n");
    printf("A!=B labels ALGORITHM, B!=C labels DEPTH; blit pairs are Pixel4-native (A vs C only)\n\n");

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
    sweepBlitGray8();
    sweepBlitCanvas();
    sweepBlendGray8();
    sweepBlendCanvas();
    sweepFillRectWithPattern();
    sweepTextDrawChar();
    sweepTextPrint();
    sweepTextPrintln();
    sweepTextGetTextWidth();
    sweepTextBoundsGuard();

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
