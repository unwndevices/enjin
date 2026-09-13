/**
 * @file numerals.hpp
 * @brief HUD numerals — digit-strip draw (`number` / `timer`) plus the
 *        `RollingCounter` and `Timer` value objects (ADR-0003 §8, Tomodachi #83).
 *
 * HUD scores and timers are drawn from fixed-width `.njn` v2 **digit strips**,
 * not a proportional text face: monospace glyphs never jitter as a value climbs.
 * A strip is a plain sheet (`META`+`PIXL`, no `CLIP`) whose frame index *is* the
 * glyph — digit `d` → frame `d`, and `:` → frame @ref kNumeralColonFrame (10) on
 * the timer strip. Advance is the sheet's `cellW`; there is no separate monospace
 * field.
 *
 * Four helpers, all filed upstream to enjin and mirrored in Lua:
 *   - @ref drawNumber / `number()` — padded, aligned, optional thousands sep.
 *   - @ref drawTimer  / `timer()`  — `mm:ss` from a millisecond count.
 *   - @ref RollingCounter — a value that eases toward a target with a critically
 *     damped @ref Spring (ADR-0002, `bounce=0`), the score-climb tween.
 *   - @ref Timer — a millisecond countdown/​countup value object, `mm:ss` format,
 *     polled @ref Timer::done (no zero callback).
 *
 * The domain is **unsigned** — HUD scores and clocks are non-negative, so there
 * is no minus glyph. Glyph layout (@ref buildNumberGlyphs / @ref buildTimerGlyphs)
 * is single-sourced here so the C++ `ICanvas` draw and the Lua `gfx.number` blit
 * select identical frames at identical positions.
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cmath>

#include "sprite.hpp"
#include "canvas.hpp"
#include "../ui/spring.hpp"

namespace enjin2 {

/// Frame holding the `:` glyph in the timer strip (0-9 occupy frames 0-9).
inline constexpr uint16_t kNumeralColonFrame = 10;

/// Sentinel glyph meaning "advance one cell without drawing" (a blank space pad).
inline constexpr uint16_t kNumeralBlank = 0xFFFF;

/// Maximum glyph cells any single @ref buildNumberGlyphs / @ref buildTimerGlyphs
/// call can emit (10 uint32 digits + generous pad + thousands separators).
inline constexpr size_t kNumeralMaxCells = 24;

/// Anchor edge for a drawn numeral field relative to the given x.
enum class NumberAlign : uint8_t {
    Left,    ///< x is the field's left edge (default).
    Right,   ///< x is the field's right edge — the field extends left (right-anchored score).
    Center,  ///< x is the field's centre.
};

/**
 * @brief Layout options for @ref drawNumber (`number()`).
 *
 * `pad` is the minimum digit count; a shorter value is filled on the left with
 * `'0'` glyphs (@ref padZeros true) or blank cells (false, "spaces"). `sep` is
 * an optional thousands-separator glyph frame inserted every three digits from
 * the right; `< 0` disables it (a plain 0-9 score strip has no separator glyph).
 */
struct NumberStyle {
    uint8_t     pad      = 0;                 ///< Minimum digit count (leading fill).
    bool        padZeros = true;              ///< Pad with '0' glyphs (true) or blank cells (false).
    NumberAlign align    = NumberAlign::Left; ///< Which edge x anchors.
    int16_t     sep      = -1;                ///< Thousands-separator glyph frame; < 0 = none.
    uint8_t     spacing  = 0;                 ///< Extra pixels between glyph cells.
};

/**
 * @brief Emit the glyph frame indices for @p value under @p style.
 * @param out   Caller buffer of at least @p cap entries.
 * @param cap   Buffer capacity (cells beyond it are dropped).
 * @param value Unsigned value to render.
 * @param style Padding / separator options (alignment is applied at draw time).
 * @return Number of cells written (digits + pad + separators), clamped to @p cap.
 *
 * @ref kNumeralBlank entries are advance-only (blank space pad); every other
 * entry is a glyph frame index for the digit strip.
 */
size_t buildNumberGlyphs(uint16_t* out, size_t cap, uint32_t value, const NumberStyle& style);

/**
 * @brief Emit the `mm:ss` glyphs for a millisecond count.
 * @param out Caller buffer of at least @p cap entries.
 * @param cap Buffer capacity.
 * @param ms  Milliseconds; formatted as minutes:seconds, seconds zero-padded to 2,
 *            minutes to at least 2 digits (more if the clock exceeds 99 minutes).
 * @return Number of cells written (minute digits + colon + two second digits).
 *
 * The colon is emitted as frame @ref kNumeralColonFrame, so this needs the timer
 * strip (11 frames), not the score strip.
 */
size_t buildTimerGlyphs(uint16_t* out, size_t cap, uint32_t ms);

/// Total pixel width of @p count cells of width @p cellW spaced by @p spacing.
inline int numeralFieldWidth(uint8_t cellW, size_t count, uint8_t spacing) {
    if (count == 0) return 0;
    return static_cast<int>(count) * static_cast<int>(cellW)
         + static_cast<int>(count - 1) * static_cast<int>(spacing);
}

/// Left edge for a field of @p width pixels anchored at @p x under @p align.
inline int16_t numeralStartX(int16_t x, int width, NumberAlign align) {
    switch (align) {
        case NumberAlign::Right:  return static_cast<int16_t>(x - width);
        case NumberAlign::Center: return static_cast<int16_t>(x - width / 2);
        case NumberAlign::Left:
        default:                  return x;
    }
}

/**
 * @brief Draw @p value from @p strip at (@p x, @p y) and return the field width.
 * @return Field width in pixels (also what @ref numeralFieldWidth would give),
 *         suitable for chaining a caret. Zero for an empty/invalid strip.
 */
int drawNumber(ICanvas<Pixel4>& canvas, const SpriteSheet& strip,
               int16_t x, int16_t y, uint32_t value, const NumberStyle& style = {});

/**
 * @brief Draw @p ms as `mm:ss` from a timer @p strip; returns the field width.
 * @note @p style.pad / @p style.sep are ignored (the format is fixed); only
 *       @p style.align and @p style.spacing apply.
 */
int drawTimer(ICanvas<Pixel4>& canvas, const SpriteSheet& strip,
              int16_t x, int16_t y, uint32_t ms, const NumberStyle& style = {});

// ---------------------------------------------------------------------------
// RollingCounter — a score value that eases toward its target (value object)
// ---------------------------------------------------------------------------

/**
 * @brief A displayed count that rolls toward a target with a critically damped
 *        spring (ADR-0002, `bounce=0`).
 *
 * Set a new @ref set target and the displayed value climbs (or drops) smoothly,
 * inheriting in-flight velocity on retarget (the spring's R1). The display is
 * `floor(x)`; the spring is **snapped and killed once it is within 1 unit** of
 * the target so the shown integer lands exactly on the target and stops (no
 * jitter, no physical odometer roll). The domain is unsigned — a negative
 * position reads as 0.
 */
struct RollingCounter {
    Spring spring; ///< The scalar integrator driving the roll.

    /// Construct at @p initial; @p durationS is the perceptual settle time. Both
    /// constructors route through here so the damping is critically damped
    /// (`bounce = 0`) regardless of the @ref springpreset::Smooth shape.
    explicit RollingCounter(float initial = 0.0f,
                            float durationS = springpreset::Smooth.durationS) {
        spring.setPerceptual(durationS, 0.0f); // bounce = 0 → critically damped
        spring.x = spring.target = initial;
        spring.v = 0.0f;
    }

    /// Retarget the roll toward @p target, keeping in-flight velocity (R1).
    void set(float target) { spring.retarget(target); }

    /// Jump straight to @p value with no roll (kills velocity).
    void snap(float value) {
        spring.x = spring.target = value;
        spring.v = 0.0f;
    }

    /// Advance the roll by @p dtS **seconds**; snaps + stops within 1 of target.
    void update(float dtS) {
        spring.advance(dtS);
        if (std::fabs(spring.target - spring.x) < 1.0f) spring.snap();
    }

    /// The target the counter is rolling toward.
    float target() const { return spring.target; }

    /// The raw (unfloored) displayed position.
    float valuef() const { return spring.x; }

    /// The displayed integer (`floor`, clamped to the unsigned domain).
    uint32_t value() const {
        const float v = spring.x < 0.0f ? 0.0f : spring.x;
        return static_cast<uint32_t>(std::floor(v));
    }
};

// ---------------------------------------------------------------------------
// Timer — a millisecond countdown / countup value object
// ---------------------------------------------------------------------------

/// Direction a @ref Timer runs.
enum class TimerMode : uint8_t {
    CountDown, ///< Runs from the start value down to 0 (@ref Timer::done at 0).
    CountUp,   ///< Runs from 0 up to the start value (@ref Timer::done at the target).
};

/**
 * @brief A fps-independent stopwatch in milliseconds, drawn as `mm:ss`.
 *
 * Time is kept in `double` milliseconds and advanced by real `dt` seconds, so
 * the clock is independent of frame rate and of the fixed-dt physics substeps.
 * Completion is **polled** (@ref done) rather than signalled by a callback,
 * matching the engine's "events polled, no registry" rule.
 */
struct Timer {
    double    ms         = 0.0;                 ///< Current time in milliseconds.
    double    durationMs = 0.0;                 ///< Start value (countdown top / countup target).
    TimerMode mode       = TimerMode::CountDown;///< Direction.
    bool      paused     = false;               ///< When true, @ref update is a no-op.

    Timer() = default;

    /// Construct a timer of @p startMs milliseconds running in @p m.
    explicit Timer(double startMs, TimerMode m = TimerMode::CountDown)
        : ms(m == TimerMode::CountDown ? startMs : 0.0)
        , durationMs(startMs)
        , mode(m) {}

    /// Reset to the start value and unpause.
    void reset() {
        ms = (mode == TimerMode::CountDown) ? durationMs : 0.0;
        paused = false;
    }

    void pause()  { paused = true; }
    void resume() { paused = false; }

    /// Advance by @p dtS **seconds**; clamps at the 0 / duration bound.
    void update(float dtS) {
        if (paused) return;
        const double d = static_cast<double>(dtS) * 1000.0;
        if (mode == TimerMode::CountDown) {
            ms -= d;
            if (ms < 0.0) ms = 0.0;
        } else {
            ms += d;
            if (ms > durationMs) ms = durationMs;
        }
    }

    /// True once the timer has reached its bound (0 for down, duration for up).
    bool done() const {
        return (mode == TimerMode::CountDown) ? (ms <= 0.0) : (ms >= durationMs);
    }

    /// Current time floored to whole milliseconds (unsigned).
    uint32_t millis() const {
        return ms < 0.0 ? 0u : static_cast<uint32_t>(ms);
    }

    /// Current whole seconds remaining/elapsed.
    uint32_t seconds() const { return millis() / 1000u; }

    /**
     * @brief Write the `mm:ss` string into @p buf.
     * @param buf Destination (≥ 6 bytes for `99:59`; 8+ recommended).
     * @param cap Buffer capacity.
     *
     * Seconds are zero-padded to two digits, minutes to at least two; a clock
     * past 99 minutes grows the minute field rather than wrapping.
     */
    void format(char* buf, size_t cap) const {
        const uint32_t totalSec = millis() / 1000u;
        const uint32_t mm = totalSec / 60u;
        const uint32_t ss = totalSec % 60u;
        std::snprintf(buf, cap, "%02u:%02u",
                      static_cast<unsigned>(mm), static_cast<unsigned>(ss));
    }
};

} // namespace enjin2
