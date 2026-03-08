#pragma once

/**
 * @file frame_timing.hpp
 * @brief Per-frame phase timing instrumentation for the enjin2 engine.
 *
 * Provides FrameTimingInstrumentation — a singleton struct with four lock-free
 * atomic uint32_t fields that record microsecond durations for each game loop
 * phase (update, render, Lua, composite).
 *
 * Two compilation paths:
 *   - ENJIN2_FRAME_TIMING defined:   atomic<uint32_t> fields, static_assert lock-free
 *   - ENJIN2_FRAME_TIMING not defined: plain uint32_t fields, zero overhead stub
 *
 * WASM and ESP32 builds omit the define and compile the zero-overhead stub.
 * The SDL3 runner defines ENJIN2_FRAME_TIMING=1 unconditionally.
 */

#ifdef ENJIN2_FRAME_TIMING
#include <atomic>
#endif
#include <cstdint>

namespace enjin2 {

#ifdef ENJIN2_FRAME_TIMING

// Lock-free contract: std::atomic<uint32_t> must be always lock-free.
// This is guaranteed on all x86, x86-64, ARM32, and ARM64 platforms.
static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "FrameTimingInstrumentation requires lock-free std::atomic<uint32_t>");

/**
 * @brief Singleton instrumentation struct — enabled path (ENJIN2_FRAME_TIMING defined).
 *
 * All four fields are std::atomic<uint32_t> to allow safe reads from a render thread
 * or debug overlay while the game loop writes from the main thread.
 * All stores and loads use std::memory_order_relaxed (no cross-field ordering needed).
 */
struct FrameTimingInstrumentation {
    std::atomic<uint32_t> updateTime_us{0};     ///< Lua update() + tick helpers (microseconds)
    std::atomic<uint32_t> renderTime_us{0};     ///< SDL texture upload + RenderPresent (microseconds)
    std::atomic<uint32_t> luaTime_us{0};        ///< Entire Lua section including update + draw (microseconds)
    std::atomic<uint32_t> compositeTime_us{0};  ///< Layer composite + RGB expand (microseconds)

    /// Meyer's singleton — returns the same instance on every call.
    static FrameTimingInstrumentation& get() {
        static FrameTimingInstrumentation instance;
        return instance;
    }

    FrameTimingInstrumentation(const FrameTimingInstrumentation&)            = delete;
    FrameTimingInstrumentation& operator=(const FrameTimingInstrumentation&) = delete;
    FrameTimingInstrumentation(FrameTimingInstrumentation&&)                 = delete;
    FrameTimingInstrumentation& operator=(FrameTimingInstrumentation&&)      = delete;

private:
    FrameTimingInstrumentation() = default;
};

#else // ENJIN2_FRAME_TIMING not defined — zero-overhead disabled stub

/**
 * @brief Singleton instrumentation stub — disabled path (ENJIN2_FRAME_TIMING not defined).
 *
 * Plain uint32_t fields, no atomic includes. Writes compile to nothing on
 * platforms where ENJIN2_FRAME_TIMING is omitted (WASM, ESP32).
 */
struct FrameTimingInstrumentation {
    uint32_t updateTime_us{0};
    uint32_t renderTime_us{0};
    uint32_t luaTime_us{0};
    uint32_t compositeTime_us{0};

    static FrameTimingInstrumentation& get() {
        static FrameTimingInstrumentation instance;
        return instance;
    }

    FrameTimingInstrumentation(const FrameTimingInstrumentation&)            = delete;
    FrameTimingInstrumentation& operator=(const FrameTimingInstrumentation&) = delete;
    FrameTimingInstrumentation(FrameTimingInstrumentation&&)                 = delete;
    FrameTimingInstrumentation& operator=(FrameTimingInstrumentation&&)      = delete;

private:
    FrameTimingInstrumentation() = default;
};

#endif // ENJIN2_FRAME_TIMING

} // namespace enjin2
