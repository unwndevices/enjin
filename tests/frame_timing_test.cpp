/**
 * @file frame_timing_test.cpp
 * @brief Unit tests for FrameTimingInstrumentation singleton, store/load, defaults,
 *        and lock-free static_assert. Tests the ENJIN2_FRAME_TIMING-enabled path.
 */

// Note: ENJIN2_FRAME_TIMING=1 is injected via CMake target_compile_definitions.
// Do NOT define it here — use the CMake approach for consistency.

#include <enjin2/instrumentation/frame_timing.hpp>
#include <cstdio>

using namespace enjin2;

static int passes   = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

// Test 1: Singleton identity — two calls return the same instance.
static void test_singleton_identity() {
    FrameTimingInstrumentation& a = FrameTimingInstrumentation::get();
    FrameTimingInstrumentation& b = FrameTimingInstrumentation::get();
    ASSERT(&a == &b, "singleton identity: &a == &b");
}

// Test 2: Store/load round-trip on luaTime_us.
static void test_store_load_round_trip() {
    FrameTimingInstrumentation& ft = FrameTimingInstrumentation::get();
    ft.luaTime_us.store(12345u, std::memory_order_relaxed);
    uint32_t loaded = ft.luaTime_us.load(std::memory_order_relaxed);
    ASSERT(loaded == 12345u, "store/load round-trip: luaTime_us == 12345");
    // Restore to 0 so other tests don't see stale value
    ft.luaTime_us.store(0u, std::memory_order_relaxed);
}

// Test 3: All four fields default to 0 on fresh singleton.
// (Since this test runs first or after cleanup above, values should be 0.)
static void test_default_zero() {
    FrameTimingInstrumentation& ft = FrameTimingInstrumentation::get();
    ASSERT(ft.updateTime_us.load(std::memory_order_relaxed) == 0u,
           "default zero: updateTime_us == 0");
    ASSERT(ft.renderTime_us.load(std::memory_order_relaxed) == 0u,
           "default zero: renderTime_us == 0");
    ASSERT(ft.luaTime_us.load(std::memory_order_relaxed) == 0u,
           "default zero: luaTime_us == 0");
    ASSERT(ft.compositeTime_us.load(std::memory_order_relaxed) == 0u,
           "default zero: compositeTime_us == 0");
}

// Test 4: Verify round-trip for all four fields independently.
static void test_all_four_fields() {
    FrameTimingInstrumentation& ft = FrameTimingInstrumentation::get();
    ft.updateTime_us.store(100u, std::memory_order_relaxed);
    ft.renderTime_us.store(200u, std::memory_order_relaxed);
    ft.luaTime_us.store(300u, std::memory_order_relaxed);
    ft.compositeTime_us.store(400u, std::memory_order_relaxed);

    ASSERT(ft.updateTime_us.load(std::memory_order_relaxed) == 100u,
           "all four fields: updateTime_us == 100");
    ASSERT(ft.renderTime_us.load(std::memory_order_relaxed) == 200u,
           "all four fields: renderTime_us == 200");
    ASSERT(ft.luaTime_us.load(std::memory_order_relaxed) == 300u,
           "all four fields: luaTime_us == 300");
    ASSERT(ft.compositeTime_us.load(std::memory_order_relaxed) == 400u,
           "all four fields: compositeTime_us == 400");
}

int main() {
    printf("=== frame_timing_test ===\n");
    // Run default_zero first (before any store modifies the singleton)
    test_default_zero();
    test_singleton_identity();
    test_store_load_round_trip();
    test_all_four_fields();
    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
