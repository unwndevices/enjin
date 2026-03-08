#pragma once

/**
 * @file alloc_guard.hpp
 * @brief RAII allocation guard for zero-alloc hot-path verification.
 *
 * Provides AllocGuard — an RAII class that arms a thread-local operator new
 * counter on construction and checks it on destruction. If any allocation fires
 * inside the guarded scope, the binary prints a diagnostic and calls exit(1).
 *
 * Two compilation paths:
 *   - ENJIN2_ALLOC_VERIFICATION defined:   Full guard with extern thread_local
 *                                           counter declarations; exits non-zero
 *                                           on detected allocation.
 *   - ENJIN2_ALLOC_VERIFICATION not defined: No-op stub — zero overhead.
 *
 * The thread_local variables (g_alloc_guard_depth, g_alloc_count) and the
 * global operator new override are defined in benchmarks/bench_alloc.cpp.
 * This header only declares them as extern.
 *
 * Usage:
 *   {
 *     enjin2::AllocGuard guard("canvas4: clear");
 *     canvas.clear(Pixel4(0));
 *   }  // exits non-zero if clear() allocated
 */

#ifdef ENJIN2_ALLOC_VERIFICATION

#include <cstdio>
#include <cstdlib>

// Thread-local allocation counter — defined in bench_alloc.cpp
extern thread_local int  g_alloc_guard_depth;
extern thread_local long g_alloc_count;

namespace enjin2 {

/**
 * @brief RAII allocation guard — enabled path (ENJIN2_ALLOC_VERIFICATION defined).
 *
 * Arms the global operator new counter on construction by incrementing
 * g_alloc_guard_depth. On destruction, decrements depth and checks whether
 * any allocation fired (g_alloc_count delta > 0). If so, prints a diagnostic
 * to stderr and calls exit(1) so the binary exits non-zero.
 *
 * Non-copyable, non-movable.
 */
class AllocGuard {
public:
    explicit AllocGuard(const char* label)
        : m_label(label), m_count_before(g_alloc_count)
    {
        g_alloc_guard_depth++;
    }

    ~AllocGuard() {
        g_alloc_guard_depth--;
        long delta = g_alloc_count - m_count_before;
        if (delta > 0) {
            fprintf(stderr,
                "[ALLOC-FAIL] %s: %ld allocation(s) detected in hot path\n",
                m_label, delta);
            exit(1);
        }
    }

    AllocGuard(const AllocGuard&)            = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
    AllocGuard(AllocGuard&&)                 = delete;
    AllocGuard& operator=(AllocGuard&&)      = delete;

private:
    const char* m_label;
    long        m_count_before;
};

} // namespace enjin2

#else // ENJIN2_ALLOC_VERIFICATION not defined — zero-overhead no-op stub

namespace enjin2 {

/**
 * @brief RAII allocation guard stub — disabled path (ENJIN2_ALLOC_VERIFICATION not defined).
 *
 * Constructor takes a label string and does nothing. Destructor does nothing.
 * Compiles to zero instructions on all platforms where ENJIN2_ALLOC_VERIFICATION
 * is omitted (WASM, ESP32, normal engine builds).
 */
class AllocGuard {
public:
    explicit AllocGuard(const char* /*label*/) {}
    ~AllocGuard() = default;

    AllocGuard(const AllocGuard&)            = delete;
    AllocGuard& operator=(const AllocGuard&) = delete;
    AllocGuard(AllocGuard&&)                 = delete;
    AllocGuard& operator=(AllocGuard&&)      = delete;
};

} // namespace enjin2

#endif // ENJIN2_ALLOC_VERIFICATION
