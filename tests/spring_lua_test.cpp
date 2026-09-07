/**
 * @file spring_lua_test.cpp
 * @brief Lua round-trip for engine.tween.spring + easeOutBack (wayfinder #17 / spec #30)
 *
 * The spring integrator, sub-stepping, retarget velocity inheritance and settle
 * threshold are pinned as pure numerics in ui_spring_test.cpp. This file is the
 * binding surface: that a spring created from Lua parses its args, ticks its target
 * field toward the goal and settles, that a second spring on the same (obj, key)
 * RETARGETS the live one (one slot, R1) rather than queueing a second, and that the
 * new `easeOutBack` tween easing round-trips through engine.tween.to.
 *
 * Mirrors the tween_test.cpp fixture (LuaEngine + LuaBindings, no canvas), the
 * native Lua round-trip idiom the tween pool is already tested with.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cmath>

using namespace enjin2;

static int passes   = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); failures++; } \
        else { passes++; } \
    } while(0)

#define ASSERT_NEAR(val, expected, eps, msg) \
    do { \
        if (fabs((val) - (expected)) > (eps)) { \
            fprintf(stderr, "FAIL [line %d]: %s (got %f, expected %f)\n", \
                    __LINE__, (msg), (double)(val), (double)(expected)); \
            failures++; \
        } else { passes++; } \
    } while(0)

// Minimal fixture: LuaEngine + LuaBindings, no canvas injected.
struct SpringFixture {
    LuaEngine   engine;
    LuaBindings bindings;

    SpringFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }
    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* name) { return engine.getGlobalNumber(name); }
    void tick(float dt) { bindings.tickSprings(dt); }
};

// The spring binding is present on engine.tween, and easeOutBack round-trips.
static void test_surface_exists() {
    printf("--- test_surface_exists ---\n");
    SpringFixture f;
    LuaResult r = f.exec(
        "ok_spring = (type(engine.tween.spring) == 'function') and 1 or 0\n"
        "local o = {v = 0}\n"
        "ok_back = engine.tween.to(o, {v = 1}, 1.0, 'easeOutBack')\n"
        "ok_back_num = (type(ok_back) == 'number') and 1 or 0\n"
    );
    ASSERT(r.success, "surface: table checks must not error");
    ASSERT(f.getNum("ok_spring") == 1.0, "surface: engine.tween.spring is a function");
    ASSERT(f.getNum("ok_back_num") == 1.0, "surface: easeOutBack accepted by engine.tween.to");
}

// spring(obj, key, target, opts) returns a positive integer ID.
static void test_spring_returns_id() {
    printf("--- test_spring_returns_id ---\n");
    SpringFixture f;
    LuaResult r = f.exec(
        "obj = {y = 0}\n"
        "id = engine.tween.spring(obj, 'y', 40, {duration = 0.34, bounce = 0.55})\n"
        "id_is_number = (type(id) == 'number') and 1 or 0\n"
    );
    ASSERT(r.success, "id: spring must not error");
    ASSERT(f.getNum("id_is_number") == 1.0, "id: spring returns a number");
    ASSERT(f.getNum("id") > 0.0, "id: spring returns a positive integer ID");
}

// A Lua-created spring ticks its field toward the target and settles exactly on it
// (settled springs snap to rest — no sub-pixel jitter left in the field).
static void test_spring_ticks_and_settles() {
    printf("--- test_spring_ticks_and_settles ---\n");
    SpringFixture f;
    LuaResult r = f.exec(
        "obj = {y = 0}\n"
        "engine.tween.spring(obj, 'y', 18, {duration = 0.34, bounce = 0.55})\n"
    );
    ASSERT(r.success, "tick: spring must not error");

    // A couple frames in, the field has moved but not yet arrived.
    f.tick(1.0f / 30.0f);
    f.tick(1.0f / 30.0f);
    f.exec("y_mid = obj.y");
    ASSERT(f.getNum("y_mid") > 1.0, "tick: field moves toward the target");

    // Run well past the 400 ms settle window.
    for (int i = 0; i < 30; ++i) f.tick(1.0f / 30.0f);
    f.exec("y_end = obj.y");
    ASSERT_NEAR(f.getNum("y_end"), 18.0, 0.001, "tick: field settles exactly on the target");
}

// R1 at the binding: a second spring on the same (obj, key) RETARGETS the live one
// — same slot, same ID — rather than allocating a second competing spring.
static void test_retarget_reuses_slot() {
    printf("--- test_retarget_reuses_slot ---\n");
    SpringFixture f;
    LuaResult r = f.exec(
        "obj = {y = 0}\n"
        "id1 = engine.tween.spring(obj, 'y', 40, {})\n"
    );
    ASSERT(r.success, "retarget: first spring must not error");
    // Let it build some velocity, then retarget mid-flight.
    f.tick(1.0f / 30.0f);
    f.tick(1.0f / 30.0f);
    f.exec("id2 = engine.tween.spring(obj, 'y', 5, {})");
    ASSERT(f.getNum("id1") == f.getNum("id2"), "retarget: same (obj,key) reuses the live slot (R1)");

    // The retargeted spring still settles — now on the new target.
    for (int i = 0; i < 30; ++i) f.tick(1.0f / 30.0f);
    f.exec("y_end = obj.y");
    ASSERT_NEAR(f.getNum("y_end"), 5.0, 0.001, "retarget: settles on the new target after retarget");
}

// easeOutBack overshoots past the target before easing back to it exactly.
static void test_easeoutback_overshoots() {
    printf("--- test_easeoutback_overshoots ---\n");
    SpringFixture f;
    // Drive the tween pool (not the spring pool) — this exercises the easing curve.
    LuaResult r = f.exec("obj = {x = 0}\n engine.tween.to(obj, {x = 100}, 1.0, 'easeOutBack')\n");
    ASSERT(r.success, "back: tween.to must not error");

    // ~85% through, easeOutBack sits above the target (overshoot).
    double maxSeen = 0.0;
    for (int i = 0; i < 10; ++i) {
        f.bindings.tickTweens(0.1f);
        f.exec("x_now = obj.x");
        if (f.getNum("x_now") > maxSeen) maxSeen = f.getNum("x_now");
    }
    ASSERT(maxSeen > 100.0, "back: easeOutBack overshoots past the target");
    f.exec("x_end = obj.x");
    ASSERT_NEAR(f.getNum("x_end"), 100.0, 0.01, "back: easeOutBack lands exactly on the target");
}

// Untrusted params are clamped: an out-of-range bounce (ζ = 1 − bounce ≤ 0 would be
// divergent) and a too-short duration (ω·dt past the stability bound) must still
// settle on the target and free the slot rather than blow up to NaN and leak.
static void test_clamps_hostile_input() {
    printf("--- test_clamps_hostile_input ---\n");
    SpringFixture f;
    LuaResult r = f.exec(
        "a = {y = 0}\n b = {y = 0}\n"
        "engine.tween.spring(a, 'y', 20, {bounce = 1.5})\n"   // ζ would be -0.5 unclamped
        "engine.tween.spring(b, 'y', 20, {duration = 0.01})\n" // ω·dt would diverge unclamped
    );
    ASSERT(r.success, "clamp: hostile spring params must not error");
    for (int i = 0; i < 40; ++i) { f.tick(1.0f / 30.0f); }
    f.exec("ya = a.y  yb = b.y");
    ASSERT_NEAR(f.getNum("ya"), 20.0, 0.001, "clamp: bounce>=1 spring still settles (no divergence)");
    ASSERT_NEAR(f.getNum("yb"), 20.0, 0.001, "clamp: tiny-duration spring still settles (no divergence)");
}

int main() {
    test_surface_exists();
    test_spring_returns_id();
    test_spring_ticks_and_settles();
    test_retarget_reuses_slot();
    test_easeoutback_overshoots();
    test_clamps_hostile_input();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
