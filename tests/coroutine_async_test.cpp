/**
 * @file coroutine_async_test.cpp
 * @brief Lua integration tests for engine.async.* sub-table (Phase 49: ASYNC-01..ASYNC-03)
 *
 * Tests:
 *   ASYNC-01: engine.async.start(fn) returns integer ID; coroutine runs on first tick
 *   ASYNC-01: Pool overflow (9th start) returns nil (no error)
 *   ASYNC-02: engine.async.wait(seconds) pauses coroutine for correct number of frames
 *   ASYNC-02: engine.async.cancel(id) prevents future execution
 *   ASYNC-02: engine.async.cancelAll() clears all active coroutines
 *   ASYNC-03: clearCoroutines() on registerAll() prevents stale coroutine resume
 *   API:      engine.async is a table with start/wait/cancel/cancelAll as functions
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cstring>

using namespace enjin2;

static int passes   = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// Minimal fixture: LuaEngine + LuaBindings, no canvas injected
// ============================================================
struct AsyncFixture {
    LuaEngine   engine;
    LuaBindings bindings;

    AsyncFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }

    /** Tick the coroutine scheduler for one frame */
    void tick(float dt) {
        bindings.tickCoroutines(dt);
    }
};

// ============================================================
// Test 1: engine.async sub-table exists with all expected functions
// ============================================================
static void test_async_table_exists() {
    printf("--- test_async_table_exists ---\n");

    AsyncFixture f;

    LuaResult r = f.exec(
        "ok_table     = (type(engine.async) == 'table') and 1 or 0\n"
        "ok_start     = (type(engine.async.start) == 'function') and 1 or 0\n"
        "ok_wait      = (type(engine.async.wait) == 'function') and 1 or 0\n"
        "ok_cancel    = (type(engine.async.cancel) == 'function') and 1 or 0\n"
        "ok_cancelAll = (type(engine.async.cancelAll) == 'function') and 1 or 0\n"
    );

    ASSERT(r.success, "engine.async table checks must not error");
    ASSERT(f.getNum("ok_table")     == 1.0, "engine.async must be a table");
    ASSERT(f.getNum("ok_start")     == 1.0, "engine.async.start must be a function");
    ASSERT(f.getNum("ok_wait")      == 1.0, "engine.async.wait must be a function");
    ASSERT(f.getNum("ok_cancel")    == 1.0, "engine.async.cancel must be a function");
    ASSERT(f.getNum("ok_cancelAll") == 1.0, "engine.async.cancelAll must be a function");
}

// ============================================================
// Test 2: engine.async.start returns a numeric ID
// ============================================================
static void test_async_start_returns_id() {
    printf("--- test_async_start_returns_id ---\n");

    AsyncFixture f;

    LuaResult r = f.exec(
        "id = engine.async.start(function() end)\n"
    );

    ASSERT(r.success, "engine.async.start must not error");
    double id = f.getNum("id");
    ASSERT(id > 0.0, "engine.async.start must return a positive integer ID");
}

// ============================================================
// Test 3: coroutine runs — sets global flag after one tick
// ============================================================
static void test_async_coroutine_runs() {
    printf("--- test_async_coroutine_runs ---\n");

    AsyncFixture f;

    LuaResult r = f.exec(
        "ran = 0\n"
        "engine.async.start(function()\n"
        "    ran = 1\n"
        "end)\n"
    );

    ASSERT(r.success, "engine.async.start must not error");
    ASSERT(f.getNum("ran") == 0.0, "coroutine should not run before tick");

    f.tick(0.016f);

    ASSERT(f.getNum("ran") == 1.0, "coroutine should run after one tick");
}

// ============================================================
// Test 4: pool overflow — 9th start returns nil
// ============================================================
static void test_async_pool_overflow() {
    printf("--- test_async_pool_overflow ---\n");

    AsyncFixture f;

    // Start 8 coroutines that wait forever (pool size = 8)
    LuaResult r = f.exec(
        "for i = 1, 8 do\n"
        "    engine.async.start(function()\n"
        "        engine.async.wait(999)\n"
        "    end)\n"
        "end\n"
        "-- 9th start should return nil (pool full)\n"
        "ninth = engine.async.start(function() end)\n"
        "ninth_is_nil = (ninth == nil) and 1 or 0\n"
    );

    ASSERT(r.success, "pool overflow test must not error");
    // Tick to actually submit coroutines into waiting state
    f.tick(0.016f);
    ASSERT(f.getNum("ninth_is_nil") == 1.0, "9th start() must return nil when pool is full");
}

// ============================================================
// Test 5: engine.async.wait delays resume correctly
// ============================================================
static void test_async_wait_delays_resume() {
    printf("--- test_async_wait_delays_resume ---\n");

    AsyncFixture f;

    // Start coroutine that waits 0.5 seconds then sets resumed = 1
    LuaResult r = f.exec(
        "resumed = 0\n"
        "engine.async.start(function()\n"
        "    engine.async.wait(0.5)\n"
        "    resumed = 1\n"
        "end)\n"
    );

    ASSERT(r.success, "async.wait test must not error");

    // Tick 4 times at dt=0.1 (total 0.4s) — should NOT resume yet
    f.tick(0.1f);
    ASSERT(f.getNum("resumed") == 0.0, "should not resume after tick 1 (0.1s)");

    f.tick(0.1f);
    ASSERT(f.getNum("resumed") == 0.0, "should not resume after tick 2 (0.2s)");

    f.tick(0.1f);
    ASSERT(f.getNum("resumed") == 0.0, "should not resume after tick 3 (0.3s)");

    f.tick(0.1f);
    ASSERT(f.getNum("resumed") == 0.0, "should not resume after tick 4 (0.4s)");

    // 5th tick: total >= 0.5s — should resume
    f.tick(0.1f);
    ASSERT(f.getNum("resumed") == 1.0, "should resume after tick 5 (>= 0.5s total)");
}

// ============================================================
// Test 6: engine.async.cancel(id) prevents future execution
// ============================================================
static void test_async_cancel_by_id() {
    printf("--- test_async_cancel_by_id ---\n");

    AsyncFixture f;

    LuaResult r = f.exec(
        "cancelled_ran = 0\n"
        "local cid = engine.async.start(function()\n"
        "    engine.async.wait(0.2)\n"
        "    cancelled_ran = 1\n"
        "end)\n"
        "engine.async.cancel(cid)\n"
    );

    ASSERT(r.success, "async.cancel test must not error");

    // Tick multiple times beyond wait duration
    f.tick(0.1f);
    f.tick(0.1f);
    f.tick(0.1f);
    f.tick(0.1f);

    ASSERT(f.getNum("cancelled_ran") == 0.0, "cancelled coroutine must never run");
}

// ============================================================
// Test 7: engine.async.cancelAll() clears all active coroutines
// ============================================================
static void test_async_cancel_all() {
    printf("--- test_async_cancel_all ---\n");

    AsyncFixture f;

    LuaResult r = f.exec(
        "flag1 = 0\n"
        "flag2 = 0\n"
        "flag3 = 0\n"
        "engine.async.start(function() engine.async.wait(0.1) flag1 = 1 end)\n"
        "engine.async.start(function() engine.async.wait(0.1) flag2 = 1 end)\n"
        "engine.async.start(function() engine.async.wait(0.1) flag3 = 1 end)\n"
        "engine.async.cancelAll()\n"
    );

    ASSERT(r.success, "async.cancelAll test must not error");

    // Tick many times — cancelled coroutines must never run
    for (int i = 0; i < 10; ++i) {
        f.tick(0.05f);
    }

    ASSERT(f.getNum("flag1") == 0.0, "cancelAll: flag1 must never be set");
    ASSERT(f.getNum("flag2") == 0.0, "cancelAll: flag2 must never be set");
    ASSERT(f.getNum("flag3") == 0.0, "cancelAll: flag3 must never be set");
}

// ============================================================
// Test 8: clearCoroutines on registerAll (hot-reload) clears coroutines
// ============================================================
static void test_async_clear_on_reload() {
    printf("--- test_async_clear_on_reload ---\n");

    AsyncFixture f;

    // Start a coroutine that waits then sets a flag
    LuaResult r = f.exec(
        "reload_flag = 0\n"
        "engine.async.start(function()\n"
        "    engine.async.wait(0.2)\n"
        "    reload_flag = 1\n"
        "end)\n"
    );

    ASSERT(r.success, "clear_on_reload: initial script must not error");

    // Tick once to start the coroutine's wait timer
    f.tick(0.05f);
    ASSERT(f.getNum("reload_flag") == 0.0, "clear_on_reload: flag not set before reload");

    // Simulate hot-reload: registerAll() calls clearCoroutines() internally
    f.bindings.registerAll();

    // Re-set the flag global to 0 after reload (registerAll creates a fresh Lua environment)
    // After registerAll(), Lua state is reset — reload_flag is back to nil/0
    // Tick many more times — old coroutine should not resume (it was cleared)
    for (int i = 0; i < 10; ++i) {
        f.tick(0.05f);
    }

    // reload_flag may be nil/0 since we reloaded — check it's not 1
    double flagVal = f.getNum("reload_flag");
    ASSERT(flagVal != 1.0, "clear_on_reload: coroutine must not resume after reload");
}

// ============================================================
// main
// ============================================================
int main() {
    test_async_table_exists();
    test_async_start_returns_id();
    test_async_coroutine_runs();
    test_async_pool_overflow();
    test_async_wait_delays_resume();
    test_async_cancel_by_id();
    test_async_cancel_all();
    test_async_clear_on_reload();

    printf("\n=== Coroutine Async Test: %d passed, %d failed ===\n", passes, failures);
    return failures;
}
