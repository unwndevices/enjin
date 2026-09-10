/**
 * @file numerals.cpp
 * @brief Glyph layout + digit-strip draw for the HUD numerals (ADR-0003 §8, #83).
 *
 * The layout functions here are the single source of truth for which frame each
 * cell selects and how many cells a value occupies; both the C++ `ICanvas` draw
 * below and the Lua `gfx.number` blit consume them, so the two paths can never
 * disagree on placement.
 */
#include "../../include/enjin2/graphics/numerals.hpp"

namespace enjin2 {

size_t buildNumberGlyphs(uint16_t* out, size_t cap, uint32_t value,
                         const NumberStyle& style) {
    if (!out || cap == 0) return 0;

    // Decompose into decimal digits, most-significant first. uint32 is at most
    // 10 digits (4,294,967,295); a value of 0 still renders one '0'.
    uint16_t digits[10];
    int digitCount = 0;
    do {
        digits[digitCount++] = static_cast<uint16_t>(value % 10u);
        value /= 10u;
    } while (value != 0u && digitCount < 10);
    // digits[] currently least-significant first; the field is built ltr below.

    const int fieldDigits =
        (digitCount < static_cast<int>(style.pad)) ? static_cast<int>(style.pad)
                                                   : digitCount;
    const int fillCount = fieldDigits - digitCount; // leading pad cells
    const uint16_t fillGlyph = style.padZeros ? uint16_t{0} : kNumeralBlank;

    // Emit into a scratch field (left-to-right), inserting a thousands separator
    // every three *digit* cells from the right when style.sep is enabled.
    uint16_t field[kNumeralMaxCells];
    size_t n = 0;
    auto push = [&](uint16_t g) {
        if (n < kNumeralMaxCells) field[n++] = g;
    };

    // Leading pad fill first — a thousands separator never sits inside the pad,
    // so it groups the value's own digits ("1,234"), not the zero/space padding.
    for (int i = 0; i < fillCount; ++i) push(fillGlyph);

    // Significant digits, most-significant first, grouped in threes from the right.
    for (int sig = 0; sig < digitCount; ++sig) {       // sig 0 = most significant
        const int digitsRemaining = digitCount - sig;
        if (style.sep >= 0 && sig > 0 && (digitsRemaining % 3) == 0) {
            push(static_cast<uint16_t>(style.sep));
        }
        push(digits[digitCount - 1 - sig]);
    }

    const size_t count = (n < cap) ? n : cap;
    for (size_t i = 0; i < count; ++i) out[i] = field[i];
    return count;
}

size_t buildTimerGlyphs(uint16_t* out, size_t cap, uint32_t ms) {
    if (!out || cap == 0) return 0;

    const uint32_t totalSec = ms / 1000u;
    const uint32_t mm = totalSec / 60u;
    const uint32_t ss = totalSec % 60u;

    // Minutes reuse the shared number layout (zero-padded to 2, growing past 99);
    // then the colon glyph and the two seconds digits.
    uint16_t field[kNumeralMaxCells];
    NumberStyle minutesStyle;
    minutesStyle.pad = 2;
    size_t n = buildNumberGlyphs(field, kNumeralMaxCells, mm, minutesStyle);
    auto push = [&](uint16_t g) {
        if (n < kNumeralMaxCells) field[n++] = g;
    };

    push(kNumeralColonFrame);
    push(static_cast<uint16_t>(ss / 10u));
    push(static_cast<uint16_t>(ss % 10u));

    const size_t count = (n < cap) ? n : cap;
    for (size_t i = 0; i < count; ++i) out[i] = field[i];
    return count;
}

// Shared draw over an ICanvas: place `count` glyphs from `strip` under `align`.
static int drawGlyphRun(ICanvas<Pixel4>& canvas, const SpriteSheet& strip,
                        int16_t x, int16_t y, const uint16_t* frames, size_t count,
                        NumberAlign align, uint8_t spacing) {
    const int width = numeralFieldWidth(strip.cellW, count, spacing);
    int16_t cx = numeralStartX(x, width, align);
    const int16_t step = static_cast<int16_t>(strip.cellW + spacing);
    for (size_t i = 0; i < count; ++i) {
        if (frames[i] != kNumeralBlank) {
            strip.draw(canvas, frames[i], cx, y);
        }
        cx = static_cast<int16_t>(cx + step);
    }
    return width;
}

int drawNumber(ICanvas<Pixel4>& canvas, const SpriteSheet& strip,
               int16_t x, int16_t y, uint32_t value, const NumberStyle& style) {
    if (!strip.data) return 0;
    uint16_t frames[kNumeralMaxCells];
    const size_t count = buildNumberGlyphs(frames, kNumeralMaxCells, value, style);
    return drawGlyphRun(canvas, strip, x, y, frames, count, style.align, style.spacing);
}

int drawTimer(ICanvas<Pixel4>& canvas, const SpriteSheet& strip,
              int16_t x, int16_t y, uint32_t ms, const NumberStyle& style) {
    if (!strip.data) return 0;
    uint16_t frames[kNumeralMaxCells];
    const size_t count = buildTimerGlyphs(frames, kNumeralMaxCells, ms);
    return drawGlyphRun(canvas, strip, x, y, frames, count, style.align, style.spacing);
}

} // namespace enjin2
