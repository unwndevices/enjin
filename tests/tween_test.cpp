/**
 * @file tween_test.cpp
 * @brief Lua integration tests for engine.tween.* sub-table (Phase 50: TWEEN-01..TWEEN-03)
 *
 * Tests:
 *   TWEEN-01: engine.tween.to(target, props, duration, easing, done_cb) returns integer ID
 *   TWEEN-01: Animates Lua table fields to target values over specified duration
 *   TWEEN-01: Pool overflow (9th start) returns nil (no error)
 *   TWEEN-02: engine.tween.cancel(id) stops tween mid-flight, leaves value at current position
 *   TWEEN-02: engine.tween.cancelAll() stops all active tweens; done_cb NOT fired on cancel
 *   TWEEN-02: clearTweens() via registerAll() prevents stale tween resume after hot-reload
 *   TWEEN-03: All 4 easing modes produce distinct midpoint values
 *   TWEEN-03: linear=0.5, easeIn=0.25, easeOut=0.75, easeInOut=0.5 at t=0.5
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cstring>
#include <cmath>

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

#define ASSERT_NEAR(val, expected, eps, msg) \
    do { \
        if (fabs((val) - (expected)) > (eps)) { \
            fprintf(stderr, "FAIL [line %d]: %s (got %f, expected %f)\n", \
                    __LINE__, (msg), (double)(val), (double)(expected)); \
            failures++; \
        } else { passes++; } \
    } while(0)

// ============================================================
// Minimal fixture: LuaEngine + LuaBindings, no canvas injected
// ============================================================
struct TweenFixture {
    LuaEngine   engine;
    LuaBindings bindings;

    TweenFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }

    /** Tick the tween scheduler for one frame */
    void tick(float dt) {
        bindings.tickTweens(dt);
    }
};

// ============================================================
// Test 1: engine.tween sub-table exists with all expected functions
// ============================================================
static void test_tween_table_exists() {
    printf("--- test_tween_table_exists ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "ok_table     = (type(engine.tween) == 'table') and 1 or 0\n"
        "ok_to        = (type(engine.tween.to) == 'function') and 1 or 0\n"
        "ok_cancel    = (type(engine.tween.cancel) == 'function') and 1 or 0\n"
        "ok_cancelAll = (type(engine.tween.cancelAll) == 'function') and 1 or 0\n"
    );

    ASSERT(r.success, "engine.tween table checks must not error");
    ASSERT(f.getNum("ok_table")     == 1.0, "engine.tween must be a table");
    ASSERT(f.getNum("ok_to")        == 1.0, "engine.tween.to must be a function");
    ASSERT(f.getNum("ok_cancel")    == 1.0, "engine.tween.cancel must be a function");
    ASSERT(f.getNum("ok_cancelAll") == 1.0, "engine.tween.cancelAll must be a function");
}

// ============================================================
// Test 2: engine.tween.to returns a positive integer ID
// ============================================================
static void test_tween_to_returns_id() {
    printf("--- test_tween_to_returns_id ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "local obj = {x = 0}\n"
        "id = engine.tween.to(obj, {x = 100}, 1.0, 'linear')\n"
        "id_is_number = (type(id) == 'number') and 1 or 0\n"
    );

    ASSERT(r.success, "engine.tween.to must not error");
    ASSERT(f.getNum("id_is_number") == 1.0, "engine.tween.to must return a number");
    double id = f.getNum("id");
    ASSERT(id > 0.0, "engine.tween.to must return a positive integer ID");
}

// ============================================================
// Test 3: linear animation — field animates from 0 to 100 over 1.0s
// ============================================================
static void test_tween_linear_animation() {
    printf("--- test_tween_linear_animation ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "obj = {x = 0}\n"
        "engine.tween.to(obj, {x = 100}, 1.0, 'linear')\n"
    );
    ASSERT(r.success, "linear animation: tween.to must not error");

    // Tick 5 times at dt=0.1 (total 0.5s) — midpoint
    for (int i = 0; i < 5; ++i) {
        f.tick(0.1f);
    }

    LuaResult r2 = f.exec("x_mid = obj.x");
    ASSERT(r2.success, "linear animation: reading obj.x at midpoint must not error");
    double xMid = f.getNum("x_mid");
    ASSERT_NEAR(xMid, 50.0, 1.0, "linear animation: x at t=0.5 should be ~50");

    // Tick 5 more times (total 1.0s) — end of tween
    for (int i = 0; i < 5; ++i) {
        f.tick(0.1f);
    }

    LuaResult r3 = f.exec("x_end = obj.x");
    ASSERT(r3.success, "linear animation: reading obj.x at end must not error");
    double xEnd = f.getNum("x_end");
    ASSERT_NEAR(xEnd, 100.0, 0.01, "linear animation: x at t=1.0 should be 100");
}

// ============================================================
// Test 4: All 4 easing modes produce distinct midpoint values
// ============================================================
static void test_tween_easing_modes_distinct() {
    printf("--- test_tween_easing_modes_distinct ---\n");

    TweenFixture f;

    // Start 4 tweens with one per easing mode from 0 to 100 over 1.0s
    LuaResult r = f.exec(
        "linear_obj    = {x = 0}\n"
        "easein_obj    = {x = 0}\n"
        "easeout_obj   = {x = 0}\n"
        "easeinout_obj = {x = 0}\n"
        "engine.tween.to(linear_obj,    {x = 100}, 1.0, 'linear')\n"
        "engine.tween.to(easein_obj,    {x = 100}, 1.0, 'easeIn')\n"
        "engine.tween.to(easeout_obj,   {x = 100}, 1.0, 'easeOut')\n"
        "engine.tween.to(easeinout_obj, {x = 100}, 1.0, 'easeInOut')\n"
    );
    ASSERT(r.success, "easing modes: tween.to calls must not error");

    // Tick to t=0.5 (5 ticks at dt=0.1)
    for (int i = 0; i < 5; ++i) {
        f.tick(0.1f);
    }

    LuaResult r2 = f.exec(
        "linear_x    = linear_obj.x\n"
        "easein_x    = easein_obj.x\n"
        "easeout_x   = easeout_obj.x\n"
        "easeinout_x = easeinout_obj.x\n"
    );
    ASSERT(r2.success, "easing modes: reading x values must not error");

    double linearX    = f.getNum("linear_x");
    double easeInX    = f.getNum("easein_x");
    double easeOutX   = f.getNum("easeout_x");
    double easeInOutX = f.getNum("easeinout_x");

    // Reference values at t=0.5:
    // linear:    0.5     -> 50
    // easeIn:    t*t = 0.25 -> 25
    // easeOut:   1-(1-t)^2 = 0.75 -> 75
    // easeInOut: 0.5*0.5*(3-2*0.5) = 0.5 -> 50
    ASSERT_NEAR(linearX,    50.0, 1.0, "linear at t=0.5 should be ~50");
    ASSERT_NEAR(easeInX,    25.0, 1.0, "easeIn at t=0.5 should be ~25");
    ASSERT_NEAR(easeOutX,   75.0, 1.0, "easeOut at t=0.5 should be ~75");
    ASSERT_NEAR(easeInOutX, 50.0, 1.0, "easeInOut at t=0.5 should be ~50");

    // easeIn and easeOut must be different (distinct curves)
    ASSERT(fabs(easeInX - easeOutX) > 10.0,
           "easeIn and easeOut must produce distinct midpoint values");
}

// ============================================================
// Test 5: Pool overflow — 9th tween.to returns nil
// ============================================================
static void test_tween_pool_overflow() {
    printf("--- test_tween_pool_overflow ---\n");

    TweenFixture f;

    // Start 8 tweens with very long duration (pool size = 8)
    LuaResult r = f.exec(
        "local objs = {}\n"
        "for i = 1, 8 do\n"
        "    objs[i] = {x = 0}\n"
        "    engine.tween.to(objs[i], {x = 100}, 999, 'linear')\n"
        "end\n"
        "-- 9th tween should return nil (pool full)\n"
        "local ninth_obj = {x = 0}\n"
        "ninth = engine.tween.to(ninth_obj, {x = 100}, 999, 'linear')\n"
        "ninth_is_nil = (ninth == nil) and 1 or 0\n"
    );

    ASSERT(r.success, "pool overflow test must not error");
    ASSERT(f.getNum("ninth_is_nil") == 1.0, "9th tween.to must return nil when pool is full");
}

// ============================================================
// Test 6: cancel by ID — stops tween mid-flight
// ============================================================
static void test_tween_cancel_by_id() {
    printf("--- test_tween_cancel_by_id ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "obj = {x = 0}\n"
        "local tid = engine.tween.to(obj, {x = 100}, 1.0, 'linear')\n"
        "cancel_id = tid\n"
    );
    ASSERT(r.success, "cancel by ID: tween.to must not error");

    // Tick once (dt=0.1) to advance to ~10%
    f.tick(0.1f);

    // Read value at cancel time
    LuaResult r2 = f.exec("x_at_cancel = obj.x");
    ASSERT(r2.success, "cancel by ID: reading x before cancel must not error");
    double xAtCancel = f.getNum("x_at_cancel");
    ASSERT_NEAR(xAtCancel, 10.0, 1.0, "cancel by ID: x after 0.1s should be ~10");

    // Cancel the tween
    LuaResult r3 = f.exec("engine.tween.cancel(cancel_id)");
    ASSERT(r3.success, "cancel by ID: tween.cancel must not error");

    // Tick 9 more times — value should NOT change
    for (int i = 0; i < 9; ++i) {
        f.tick(0.1f);
    }

    LuaResult r4 = f.exec("x_after_cancel = obj.x");
    ASSERT(r4.success, "cancel by ID: reading x after cancel must not error");
    double xAfterCancel = f.getNum("x_after_cancel");
    // Value should remain at approximately xAtCancel (within tiny float epsilon)
    ASSERT_NEAR(xAfterCancel, xAtCancel, 0.5, "cancel by ID: x must not change after cancel");
}

// ============================================================
// Test 7: cancelAll — stops all active tweens
// ============================================================
static void test_tween_cancel_all() {
    printf("--- test_tween_cancel_all ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "a = {x = 0}\n"
        "b = {x = 0}\n"
        "c = {x = 0}\n"
        "engine.tween.to(a, {x = 100}, 1.0, 'linear')\n"
        "engine.tween.to(b, {x = 200}, 1.0, 'easeIn')\n"
        "engine.tween.to(c, {x = 300}, 1.0, 'easeOut')\n"
        "engine.tween.cancelAll()\n"
    );
    ASSERT(r.success, "cancelAll: setup and cancelAll must not error");

    // Tick many times — cancelled tweens must never update
    for (int i = 0; i < 15; ++i) {
        f.tick(0.1f);
    }

    LuaResult r2 = f.exec(
        "a_x = a.x\n"
        "b_x = b.x\n"
        "c_x = c.x\n"
    );
    ASSERT(r2.success, "cancelAll: reading values after cancel must not error");
    ASSERT_NEAR(f.getNum("a_x"), 0.0, 0.5, "cancelAll: a.x must stay at 0 after cancelAll");
    ASSERT_NEAR(f.getNum("b_x"), 0.0, 0.5, "cancelAll: b.x must stay at 0 after cancelAll");
    ASSERT_NEAR(f.getNum("c_x"), 0.0, 0.5, "cancelAll: c.x must stay at 0 after cancelAll");
}

// ============================================================
// Test 8: done_cb fires when tween completes normally
// ============================================================
static void test_tween_done_cb_fires() {
    printf("--- test_tween_done_cb_fires ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "cb_fired = 0\n"
        "local obj = {x = 0}\n"
        "engine.tween.to(obj, {x = 100}, 0.5, 'linear', function()\n"
        "    cb_fired = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "done_cb fires: tween.to with callback must not error");
    ASSERT(f.getNum("cb_fired") == 0.0, "done_cb fires: callback must not fire before completion");

    // Tick to completion: 0.5s / 0.1 = 5 ticks + 1 extra for done_cb firing
    for (int i = 0; i < 6; ++i) {
        f.tick(0.1f);
    }

    LuaResult r2 = f.exec("fired_val = cb_fired");
    ASSERT(r2.success, "done_cb fires: reading cb_fired must not error");
    ASSERT(f.getNum("fired_val") == 1.0, "done_cb fires: callback must fire after tween completion");
}

// ============================================================
// Test 9: done_cb NOT fired when tween is cancelled
// ============================================================
static void test_tween_done_cb_not_on_cancel() {
    printf("--- test_tween_done_cb_not_on_cancel ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "cb_fired = 0\n"
        "local obj = {x = 0}\n"
        "local tid = engine.tween.to(obj, {x = 100}, 0.5, 'linear', function()\n"
        "    cb_fired = 1\n"
        "end)\n"
        "engine.tween.cancel(tid)\n"
    );
    ASSERT(r.success, "done_cb not on cancel: cancel must not error");

    // Tick far past the duration
    for (int i = 0; i < 10; ++i) {
        f.tick(0.1f);
    }

    LuaResult r2 = f.exec("fired_val = cb_fired");
    ASSERT(r2.success, "done_cb not on cancel: reading cb_fired must not error");
    ASSERT(f.getNum("fired_val") == 0.0, "done_cb not on cancel: callback must NOT fire after cancel");
}

// ============================================================
// Test 10: hot-reload safety — registerAll() clears stale tweens
// ============================================================
static void test_tween_clear_on_reload() {
    printf("--- test_tween_clear_on_reload ---\n");

    TweenFixture f;

    // Start a tween
    LuaResult r = f.exec(
        "obj = {x = 0}\n"
        "engine.tween.to(obj, {x = 100}, 1.0, 'linear')\n"
    );
    ASSERT(r.success, "clear_on_reload: initial tween.to must not error");

    // Tick once to confirm it's running
    f.tick(0.1f);

    LuaResult r2 = f.exec("x_before_reload = obj.x");
    ASSERT(r2.success, "clear_on_reload: reading x before reload must not error");
    // x should have advanced slightly (~10)
    ASSERT(f.getNum("x_before_reload") > 0.0, "clear_on_reload: tween should be advancing before reload");

    // Simulate hot-reload: registerAll() calls clearTweens() internally
    f.bindings.registerAll();

    // After reload, Lua state is fresh — tick many times and verify no stale update
    for (int i = 0; i < 15; ++i) {
        f.tick(0.1f);
    }

    // The old coroutine is cleared; no crash, no stale update
    // Verify tween count is 0 by attempting to start 8 tweens (all should succeed)
    LuaResult r3 = f.exec(
        "pool_ok = 1\n"
        "for i = 1, 8 do\n"
        "    local o = {x = 0}\n"
        "    local id = engine.tween.to(o, {x = 100}, 999, 'linear')\n"
        "    if id == nil then pool_ok = 0 end\n"
        "end\n"
    );
    ASSERT(r3.success, "clear_on_reload: pool check after reload must not error");
    ASSERT(f.getNum("pool_ok") == 1.0, "clear_on_reload: all 8 pool slots must be free after registerAll()");
}

// ============================================================
// Test 11: multi-property — animates multiple fields simultaneously
// ============================================================
static void test_tween_multi_property() {
    printf("--- test_tween_multi_property ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "obj = {x = 0, y = 0}\n"
        "engine.tween.to(obj, {x = 100, y = 200}, 1.0, 'linear')\n"
    );
    ASSERT(r.success, "multi-property: tween.to must not error");

    // Tick to t=0.5
    for (int i = 0; i < 5; ++i) {
        f.tick(0.1f);
    }

    LuaResult r2 = f.exec(
        "x_mid = obj.x\n"
        "y_mid = obj.y\n"
    );
    ASSERT(r2.success, "multi-property: reading x,y at midpoint must not error");
    ASSERT_NEAR(f.getNum("x_mid"), 50.0, 1.0, "multi-property: x at t=0.5 should be ~50");
    ASSERT_NEAR(f.getNum("y_mid"), 100.0, 1.0, "multi-property: y at t=0.5 should be ~100");
}

// ============================================================
// Test 12: zero duration — completes on first tick; done_cb fires
// ============================================================
static void test_tween_zero_duration() {
    printf("--- test_tween_zero_duration ---\n");

    TweenFixture f;

    LuaResult r = f.exec(
        "cb_fired = 0\n"
        "obj = {x = 0}\n"
        "engine.tween.to(obj, {x = 100}, 0, 'linear', function()\n"
        "    cb_fired = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "zero duration: tween.to must not error");

    // One tick should complete the tween immediately
    f.tick(0.016f);

    LuaResult r2 = f.exec(
        "x_val    = obj.x\n"
        "fired_val = cb_fired\n"
    );
    ASSERT(r2.success, "zero duration: reading values after one tick must not error");
    ASSERT_NEAR(f.getNum("x_val"), 100.0, 0.01, "zero duration: x should equal end value after first tick");
    ASSERT(f.getNum("fired_val") == 1.0, "zero duration: done_cb must fire on first tick");
}

// ============================================================
// main
// ============================================================
int main() {
    test_tween_table_exists();
    test_tween_to_returns_id();
    test_tween_linear_animation();
    test_tween_easing_modes_distinct();
    test_tween_pool_overflow();
    test_tween_cancel_by_id();
    test_tween_cancel_all();
    test_tween_done_cb_fires();
    test_tween_done_cb_not_on_cancel();
    test_tween_clear_on_reload();
    test_tween_multi_property();
    test_tween_zero_duration();

    printf("\n=== Tween Test: %d passed, %d failed ===\n", passes, failures);
    return failures;
}
