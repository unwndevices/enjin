/**
 * @file numerals_test.cpp
 * @brief HUD numerals — glyph layout, digit-strip draw, RollingCounter, Timer
 *        (ADR-0003 §8, Tomodachi #83).
 *
 * Layout (buildNumberGlyphs / buildTimerGlyphs) is asserted directly on the
 * emitted frame lists; the draw is proven on a synthetic strip whose frame f is
 * a solid fill of palette index (f+1), so reading a cell's centre pixel recovers
 * exactly which frame landed there. The Lua parity of these same paths lives in
 * numerals_lua_test.cpp.
 */
#include <enjin2/graphics/numerals.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <cmath>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                       \
    do {                                        \
        if (!(cond)) {                          \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++;                         \
        } else {                                \
            passes++;                           \
        }                                       \
    } while (0)

// Synthetic strip: 4x4 cells, frames 0..14, frame f filled with index (f+1).
// f+1 is non-zero and never 15 for f <= 13, so a drawn cell is distinguishable
// from the cleared (index 0) background and never transparent.
static uint8_t g_stripPixels[15 * 16];
static SpriteSheet makeSynthStrip() {
    for (int f = 0; f < 15; ++f) {
        const uint8_t fill = static_cast<uint8_t>(f + 1);
        for (int i = 0; i < 16; ++i) g_stripPixels[f * 16 + i] = fill;
    }
    return SpriteSheet(g_stripPixels, 4, 4, 15, 1);
}

// Centre pixel of cell `c` (cellW=4) for a field starting at startX, y=0..3.
static uint8_t cellCentre(const Canvas4<64, 64>& c, int startX, int cell) {
    return c.getPixel(static_cast<int16_t>(startX + cell * 4 + 2), 2).value;
}

int main() {
    printf("--- HUD numerals ---\n");

    uint16_t f[kNumeralMaxCells];

    // --- buildNumberGlyphs: basic decomposition, ms-first order ---
    size_t n = buildNumberGlyphs(f, kNumeralMaxCells, 42u, NumberStyle{});
    ASSERT(n == 2 && f[0] == 4 && f[1] == 2, "42 -> [4,2]");

    n = buildNumberGlyphs(f, kNumeralMaxCells, 0u, NumberStyle{});
    ASSERT(n == 1 && f[0] == 0, "0 -> [0]");

    // --- pad with leading zeros vs blank cells ---
    NumberStyle padZ; padZ.pad = 4;
    n = buildNumberGlyphs(f, kNumeralMaxCells, 7u, padZ);
    ASSERT(n == 4 && f[0] == 0 && f[1] == 0 && f[2] == 0 && f[3] == 7,
           "pad4 zeros: 7 -> [0,0,0,7]");

    NumberStyle padS; padS.pad = 3; padS.padZeros = false;
    n = buildNumberGlyphs(f, kNumeralMaxCells, 5u, padS);
    ASSERT(n == 3 && f[0] == kNumeralBlank && f[1] == kNumeralBlank && f[2] == 5,
           "pad3 spaces: 5 -> [_,_,5]");

    // --- thousands separator every three digits from the right ---
    NumberStyle sepS; sepS.sep = 12;  // separator glyph frame
    n = buildNumberGlyphs(f, kNumeralMaxCells, 1000u, sepS);
    ASSERT(n == 5 && f[0] == 1 && f[1] == 12 && f[2] == 0 && f[3] == 0 && f[4] == 0,
           "1000 sep -> [1,sep,0,0,0]");
    n = buildNumberGlyphs(f, kNumeralMaxCells, 100u, sepS);
    ASSERT(n == 3 && f[0] == 1 && f[1] == 0 && f[2] == 0, "100 sep -> no leading sep");

    // A separator groups only the value's digits, never the zero/space pad.
    NumberStyle padSep; padSep.pad = 6; padSep.sep = 12;
    n = buildNumberGlyphs(f, kNumeralMaxCells, 5u, padSep);
    ASSERT(n == 6 && f[0] == 0 && f[4] == 0 && f[5] == 5,
           "pad6 + sep: 5 -> [0,0,0,0,0,5] (no sep in pad)");

    // --- buildTimerGlyphs: mm:ss with colon at frame 10 ---
    n = buildTimerGlyphs(f, kNumeralMaxCells, 65000u);  // 1:05
    ASSERT(n == 5 && f[0] == 0 && f[1] == 1 && f[2] == kNumeralColonFrame &&
               f[3] == 0 && f[4] == 5,
           "65000ms -> 01:05 glyphs");
    n = buildTimerGlyphs(f, kNumeralMaxCells, 6000000u);  // 100:00 (minutes grow)
    ASSERT(n == 6 && f[0] == 1 && f[1] == 0 && f[2] == 0 &&
               f[3] == kNumeralColonFrame && f[4] == 0 && f[5] == 0,
           "100 min -> minute field grows past 2 digits");

    // --- field width + alignment maths ---
    ASSERT(numeralFieldWidth(4, 2, 0) == 8, "width 2 cells, cellW4 = 8");
    ASSERT(numeralFieldWidth(4, 2, 1) == 9, "width 2 cells + 1 spacing = 9");
    ASSERT(numeralStartX(50, 8, NumberAlign::Left) == 50, "left anchor");
    ASSERT(numeralStartX(50, 8, NumberAlign::Right) == 42, "right anchor");
    ASSERT(numeralStartX(50, 8, NumberAlign::Center) == 46, "center anchor");

    // --- drawNumber onto a canvas via the synthetic strip ---
    const SpriteSheet strip = makeSynthStrip();
    {
        Canvas4<64, 64> canvas;
        canvas.clear(Pixel4(0));
        const int w = drawNumber(canvas, strip, 0, 0, 42u, NumberStyle{});
        ASSERT(w == 8, "drawNumber 42 width = 8");
        ASSERT(cellCentre(canvas, 0, 0) == 5, "cell0 shows frame 4 (fill 5)");
        ASSERT(cellCentre(canvas, 0, 1) == 3, "cell1 shows frame 2 (fill 3)");
    }
    {
        // Right-anchored: x=20 is the right edge, field width 8 -> starts at 12.
        Canvas4<64, 64> canvas;
        canvas.clear(Pixel4(0));
        NumberStyle rs; rs.align = NumberAlign::Right;
        drawNumber(canvas, strip, 20, 0, 42u, rs);
        ASSERT(cellCentre(canvas, 12, 0) == 5, "right-anchored cell0 at x=12");
        ASSERT(cellCentre(canvas, 12, 1) == 3, "right-anchored cell1 at x=16");
    }
    {
        // Blank pad cells advance without drawing (background stays 0).
        Canvas4<64, 64> canvas;
        canvas.clear(Pixel4(0));
        NumberStyle bs; bs.pad = 3; bs.padZeros = false;
        drawNumber(canvas, strip, 0, 0, 5u, bs);
        ASSERT(cellCentre(canvas, 0, 0) == 0, "blank pad cell draws nothing");
        ASSERT(cellCentre(canvas, 0, 2) == 6, "value digit 5 -> frame5 fill 6");
    }
    {
        // drawTimer 1:05 -> frames [0,1,colon,0,5] fills [1,2,11,1,6].
        Canvas4<64, 64> canvas;
        canvas.clear(Pixel4(0));
        const int w = drawTimer(canvas, strip, 0, 0, 65000u, NumberStyle{});
        ASSERT(w == 20, "drawTimer width = 5 cells * 4");
        ASSERT(cellCentre(canvas, 0, 2) == 11, "colon cell shows frame 10 (fill 11)");
        ASSERT(cellCentre(canvas, 0, 4) == 6, "seconds units 5 -> frame5 fill 6");
    }

    // --- RollingCounter: rolls to target, snaps and stops exactly ---
    {
        RollingCounter rc(0.0f);
        rc.set(100.0f);
        float prev = rc.valuef();
        bool monotonic = true;
        for (int i = 0; i < 240; ++i) {  // 8 s at 30 fps — well past settle
            rc.update(1.0f / 30.0f);
            if (rc.valuef() < prev - 0.001f) monotonic = false;
            prev = rc.valuef();
        }
        ASSERT(monotonic, "RollingCounter climbs monotonically to a higher target");
        ASSERT(std::fabs(rc.valuef() - 100.0f) < 0.001f, "RollingCounter snaps onto target");
        ASSERT(rc.value() == 100u, "RollingCounter value() lands on 100");
    }
    {
        RollingCounter rc(50.0f);
        ASSERT(rc.value() == 50u, "RollingCounter initial value");
        rc.snap(7.0f);
        ASSERT(rc.value() == 7u && std::fabs(rc.valuef() - 7.0f) < 1e-6f, "snap jumps instantly");
    }

    // --- Timer: countdown, done(), format, reset, countup ---
    {
        Timer t(3000.0);  // 3 s countdown
        ASSERT(!t.done(), "fresh countdown not done");
        t.update(1.0f);
        ASSERT(t.millis() == 2000u && !t.done(), "countdown 3s - 1s = 2s");
        t.update(5.0f);  // overshoot clamps at 0
        ASSERT(t.millis() == 0u && t.done(), "countdown clamps at 0 and is done");
        t.reset();
        ASSERT(t.millis() == 3000u && !t.done(), "reset restores start");
    }
    {
        Timer t(65000.0);
        char buf[8];
        t.format(buf, sizeof(buf));
        ASSERT(buf[0] == '0' && buf[1] == '1' && buf[2] == ':' &&
                   buf[3] == '0' && buf[4] == '5',
               "format 65000ms = 01:05");
        ASSERT(t.seconds() == 65u, "seconds() = 65");
    }
    {
        Timer t(2000.0, TimerMode::CountUp);
        ASSERT(t.millis() == 0u && !t.done(), "countup starts at 0");
        t.update(3.0f);
        ASSERT(t.millis() == 2000u && t.done(), "countup clamps at duration and is done");
    }
    {
        Timer t(5000.0);
        t.pause();
        t.update(1.0f);
        ASSERT(t.millis() == 5000u, "paused timer does not advance");
        t.resume();
        t.update(1.0f);
        ASSERT(t.millis() == 4000u, "resumed timer advances");
    }

    printf("passes=%d failures=%d\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
