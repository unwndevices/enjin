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
// The table STARTS EMPTY: unwn #161's restores removed every known
// systematic divergence ahead of the bench, and #146/#147 landed as restores
// of BASE behavior, seeding nothing.
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

    // THE TABLE. Bump the array size when adding an entry — the explicit count
    // keeps the list visible *as a list* and every addition a one-hunk diff.
    // Evaluation is first-match-wins, so keep predicates non-overlapping per
    // pair: a stale earlier entry would otherwise shadow a fresh later one.
    inline constexpr std::array<Waiver, 0> kWaivers{};

} // namespace parity

#endif // ENJIN2_TESTS_WAIVERS_HPP
