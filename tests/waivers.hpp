#ifndef ENJIN2_TESTS_WAIVERS_HPP
#define ENJIN2_TESTS_WAIVERS_HPP

// Visual Parity Bench — waiver table (unwn #159, design § 8; M3 unwn #166).
//
// A waiver is the *output* of an adjudication, never a substitute for one:
// it declares that a specific, ticketed decision ruled a bounded difference
// between the BASE and HEAD implementations of one substitution pair to be
// intended. Waiving is deliberately harder than fixing:
//
//   * Predicate — pair id + MANDATORY parameter sub-range, strictly narrower
//     than the whole pair. Everything outside the sub-range stays byte-exact.
//     A whole-pair "waiver" is retirement wearing a different name; use
//     WaiverStatus::Retired for that, with its own ticket.
//   * Authorization — the adjudication ticket. No ticket, no waiver.
//   * Expected-diff bound, enforced every run — a waiver never turns
//     comparison off; it swaps memcmp == 0 for the stated bound, so "it got
//     worse" stays detectable inside the waived sub-range.
//   * Implementation pin — FNV-1a-64 hash of the implementing header on both
//     sides. Either side's file changes -> the waiver is STALE -> the
//     sub-range reverts to byte-exact and fails as "stale waiver", forcing
//     re-adjudication. There is deliberately no calendar expiry: nothing
//     fires on a date without CI; the pin fires on actual risk.
//   * Author + date — provenance.
//
// Every run ends with a census (count, per-waiver age + ticket, per-pair
// waived fraction). One hard gate: a pair > 50% waived FAILS the run — a
// pair mostly waived mostly tests nothing, and the bench refuses to pretend
// otherwise. Fix or retire.
//
// Retirement (HEAD ratified as the new design) keeps the entry here with
// status Retired + the ticket answering the exit question "what now guards
// the ratified HEAD behavior?". The sweep skips a retired pair; the census
// prints it every run, so shrinking coverage is permanently on the record.
//
// The table started empty (unwn #161's restores removed every known
// systematic divergence ahead of the bench, and #146/#147 landed as restores
// of BASE behavior, seeding nothing). The first sweep's adjudication (M5,
// unwn #168) fixed 13 of its 17 signatures in the graphics headers and
// recorded the remaining four here: two Active glcd-only waivers and two
// retirements.
//
// Pin hashes: run the bench with --hash to print the current per-header
// hashes to copy into a new entry.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace parity
{

    // One named sweep parameter of a case, e.g. {"r", 33} or {"font", 2}.
    // The bench prints each case's parameter list in failure rows; a waiver's
    // `covers` predicate sees exactly that list.
    struct Param
    {
        const char *name;
        int32_t value;
    };

    inline int32_t paramValue(const Param *params, size_t count, const char *name,
                              int32_t fallback = INT32_MIN)
    {
        for (size_t i = 0; i < count; ++i)
            if (strcmp(params[i].name, name) == 0)
                return params[i].value;
        return fallback;
    }

    enum class WaiverStatus : uint8_t
    {
        Active, // bounded, pinned, ticketed sub-range waiver
        Retired // pair retired whole: HEAD ratified as design; sweep skips it
    };

    struct Waiver
    {
        const char *pair_id;  // bench pair id, e.g. "blit.gray8Opacity"
        WaiverStatus status;
        const char *subrange; // human-readable predicate, e.g. "divisor >= 2"
        // Machine predicate over the case's parameter list. Mandatory for
        // Active entries (a null predicate covers nothing — the entry is inert
        // by construction, never accidentally whole-pair). Ignored for Retired.
        bool (*covers)(const Param *params, size_t count);
        // Enforced expected-diff bound: max differing plane bytes per covered
        // case (metric pairs: max summed |component delta|). Checked every run.
        uint32_t max_diff;
        // Implementation pins: header file name (relative to the enjin
        // graphics include dir) + FNV-1a-64 of its bytes, for both sides.
        const char *base_pin_file;
        uint64_t base_pin_hash;
        const char *head_pin_file;
        uint64_t head_pin_hash;
        const char *ticket; // adjudication ticket URL/id — no ticket, no waiver
        const char *author;
        const char *date;   // YYYY-MM-DD, provenance only (pins do the firing)
    };

    // Covers the built-in 5x7 (glcd) font sub-range of a text pair: font
    // fixture index 3 is the nullptr-font case on both sides.
    inline bool coversGlcdFont(const Param *params, size_t count)
    {
        return paramValue(params, count, "font") == 3;
    }

    // THE TABLE. Bump the array size when adding an entry — the explicit count
    // keeps the list visible *as a list* and every addition a one-hunk diff.
    // Evaluation is first-match-wins, so keep predicates non-overlapping per
    // pair: a stale earlier entry would otherwise shadow a fresh later one.
    //
    // M5 adjudication (unwn #168), first sweep run:
    //
    //   * text.drawChar / text.print, font == glcd (Active) — with no GFX
    //     font, BASE's Canvas8 draws nothing (a bare `!gfx_font` guard);
    //     HEAD's TextRenderer renders the built-in 5x7. The 5x7 fallback is
    //     load-bearing engine API (the Lua text bindings and ui widget layer
    //     document and test "nullptr = built-in 5x7"), and no Eisei call
    //     site can reach it: Canvas8 defaults to defaultFont8pt7b and every
    //     Eisei text path sets a font. Ruled an intended difference on the
    //     unreachable sub-range; the GFX-font sub-ranges stay byte-exact.
    //
    //   * text.println (Retired) — BASE's println hardcodes an unscaled 8 px
    //     line advance; every shipped font's yAdvance is larger, and BASE
    //     Eisei has zero Canvas8::println call sites (verified at 941a9ab6),
    //     so no shipped frame ever used it. HEAD's '\n' advance (yAdvance x
    //     size) is ratified. Exit question: text.print's newline fixtures
    //     ("\n", "A\nB", "AB\nCD\nEF") guard the ratified advance, byte-exact
    //     against BASE's own write('\n') path.
    //
    //   * blit.canvasOpacity (Retired) — the transparency plane moved (#158
    //     structural finding): BASE keyed transparency on an out-of-band
    //     8-bit matte (widgets composited with matte=16, one past the 4-bit
    //     range, so drawn black stayed opaque); a Pixel4 source has no
    //     out-of-band value, and HEAD's blitCanvasOpacity keys on Pixel4(0).
    //     Unfixable at 4 bpp — the sentinel cannot exist — and a sub-range
    //     waiver would exceed the 50% census gate. Ratified as the migrated
    //     design. Exit question: blit_semantics_test pins the ratified
    //     contract (transparent skip, 8-bit fade, clipping). Consequence to
    //     verify at C2 (unwn #170): content drawn at true black inside an
    //     Opacity50/25-blended widget canvas now reads transparent.
    inline constexpr std::array<Waiver, 4> kWaivers{{
        {"text.drawChar", WaiverStatus::Active,
         "font == 3 (glcd / no GFX font)", &coversGlcdFont, 80,
         "canvas.hpp", 0xf18aba15dd7867ccull,
         "text_renderer.hpp", 0x3a0e008f0006aea7ull,
         "unwndevices/unwn#168", "claude+ciro", "2026-08-02"},
        {"text.print", WaiverStatus::Active,
         "font == 3 (glcd / no GFX font)", &coversGlcdFont, 1812,
         "canvas.hpp", 0xf18aba15dd7867ccull,
         "text_renderer.hpp", 0x3a0e008f0006aea7ull,
         "unwndevices/unwn#168", "claude+ciro", "2026-08-02"},
        {"text.println", WaiverStatus::Retired,
         "pair retired: HEAD '\\n' yAdvance ratified; no BASE call sites", nullptr, 0,
         "canvas.hpp", 0xf18aba15dd7867ccull,
         "text_renderer.hpp", 0x3a0e008f0006aea7ull,
         "unwndevices/unwn#168", "claude+ciro", "2026-08-02"},
        {"blit.canvasOpacity", WaiverStatus::Retired,
         "pair retired: 4-bit source cannot carry BASE's out-of-band matte", nullptr, 0,
         "canvas.hpp", 0xf18aba15dd7867ccull,
         "blit.hpp", 0xd94a04540dfb6c79ull,
         "unwndevices/unwn#168", "claude+ciro", "2026-08-02"},
    }};

} // namespace parity

#endif // ENJIN2_TESTS_WAIVERS_HPP
