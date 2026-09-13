#pragma once

/**
 * @file clock.hpp
 * @brief Monotonic microsecond clock seam for instrumentation (ADR-0004, #98).
 *
 * Layered asset loading and rendering are instrumented with a pluggable time
 * source so the measurement harness can use the real monotonic clock while
 * unit tests inject a deterministic fake. A `nullptr` clock disables timing:
 * the metric accumulators stay at zero and the timed code becomes a no-op
 * branch, so shipping builds that never set a clock pay almost nothing.
 */

#include <chrono>
#include <cstdint>

namespace enjin2 {

/// Monotonic microsecond source. Returns an arbitrary-epoch count; only
/// differences are meaningful. `nullptr` means "do not measure".
using MicrosFn = uint64_t (*)();

/// Default time source over `std::chrono::steady_clock`.
inline uint64_t steadyMicros() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

/**
 * @brief Sums and maximises durations from a pluggable clock.
 *
 * `clock` defaults to `nullptr` (disabled), so a store/runtime that is never
 * given a clock pays only two predicted-not-taken null checks per measured
 * operation — no clock calls. Install `&steadyMicros` (or a test fake) to
 * collect metrics. `start()`/`stop()` bracket one measured operation.
 */
struct TimingAccumulator {
    MicrosFn clock = nullptr;  ///< Time source, or nullptr when disabled.
    uint64_t total = 0;        ///< Summed measured duration, microseconds.
    uint64_t max = 0;          ///< Longest measured duration, microseconds.

    /// Capture the start of an operation (0 when disabled).
    uint64_t start() const { return clock != nullptr ? clock() : 0; }

    /// Fold the operation's duration in (no-op when disabled).
    void stop(uint64_t startMicros) {
        if (clock == nullptr) return;
        const uint64_t elapsed = clock() - startMicros;
        total += elapsed;
        if (elapsed > max) max = elapsed;
    }

    /// Zero the accumulators (leaves the time source installed).
    void reset() {
        total = 0;
        max = 0;
    }
};

} // namespace enjin2
