/**
 * @file qol_test.cpp
 * @brief Integration tests for Phase 57 QoL features (QOL-01, QOL-02, QOL-03)
 *
 * Tests:
 *   QOL-01: engine.tween.await(id) suspends coroutine until tween completes, resumes exactly once
 *   QOL-01: engine.tween.await(invalid_id) resumes immediately without yielding
 *   QOL-01: Two coroutines both awaiting same tween both resume on completion
 *   QOL-02: engine.async.wait_frames(n) resumes after exactly n frames
 *   QOL-02: engine.async.wait_frames(0) returns immediately without yielding
 *   QOL-02: engine.async.wait_frames(-1) returns immediately without yielding
 *   QOL-03: engine.camera.setDeadZone(w, h) freezes camera when target is inside dead zone
 *   QOL-03: camera resumes follow when target exits dead zone
 *   QOL-03: engine.camera.setDeadZone(0, 0) does not freeze camera (dead zone disabled)
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/camera.hpp>
#include <enjin2/components/position.hpp>
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

#define ASSERT_NEAR_F(a, b, tol, msg) \
    do { \
        float _a = static_cast<float>(a); \
        float _b = static_cast<float>(b); \
        float _diff = (_a > _b) ? (_a - _b) : (_b - _a); \
        if (_diff > (tol)) { \
            fprintf(stderr, "FAIL [line %d]: %s (got %.4f, expected %.4f)\n", __LINE__, msg, (double)_a, (double)_b); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// QoLFixture: standalone LuaEngine + LuaBindings (for coroutine/tween tests)
// ============================================================
struct QoLFixture {
    LuaEngine   engine;
    LuaBindings bindings;

    QoLFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    // Get global integer/number — Lua globals must use 0/1 not false/true
    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }

    // Tick both in correct order: coroutines first, then tweens
    void tickBoth(float dt) {
        bindings.tickCoroutines(dt);
        bindings.tickTweens(dt);
    }

    void tickCoroutines(float dt) {
        bindings.tickCoroutines(dt);
    }

    void tickTweens(float dt) {
        bindings.tickTweens(dt);
    }
};

// ============================================================
// QOL-01 Test 1: tween.await suspends coroutine until tween completes
// ============================================================
static void test_tween_await_suspends_and_resumes() {
    printf("--- test_tween_await_suspends_and_resumes ---\n");

    QoLFixture f;

    // Create a tween on a Lua table from 0 to 100 over 0.1s, then await it in a coroutine.
    // Use integers (0/1) not booleans — getGlobalNumber returns 0.0 for non-numeric types.
    LuaResult r = f.exec(
        "g_resumed = 0\n"
        "local t = {x = 0}\n"
        "local id = engine.tween.to(t, {x = 100}, 0.1)\n"
        "engine.async.start(function()\n"
        "    engine.tween.await(id)\n"
        "    g_resumed = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "test_tween_await_suspends_and_resumes: setup must not error");
    ASSERT(f.getNum("g_resumed") == 0.0, "g_resumed should be 0 before any ticks");

    // Tick with dt=0.05 (coroutines first, then tweens at t=0.5, not complete yet)
    f.tickBoth(0.05f);
    ASSERT(f.getNum("g_resumed") == 0.0, "g_resumed should be 0 after partial tween tick");

    // Tick with dt=0.1 (tween completes at t>=1.0 — coroutine resumes in tickTweens)
    f.tickBoth(0.1f);
    ASSERT(f.getNum("g_resumed") == 1.0, "g_resumed should be 1 after tween completion");
}

// ============================================================
// QOL-01 Test 2: tween.await on expired/invalid ID resumes immediately
// ============================================================
static void test_tween_await_invalid_id_resumes_immediately() {
    printf("--- test_tween_await_invalid_id_resumes_immediately ---\n");

    QoLFixture f;

    // Await a tween that does not exist (ID 9999) — should resume without yielding
    LuaResult r = f.exec(
        "g_resumed = 0\n"
        "engine.async.start(function()\n"
        "    engine.tween.await(9999)\n"
        "    g_resumed = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "test_tween_await_invalid_id_resumes_immediately: setup must not error");

    // Single tick — coroutine runs, await returns immediately (tween not found), g_resumed=1
    f.tickBoth(0.016f);
    ASSERT(f.getNum("g_resumed") == 1.0, "g_resumed should be 1 after single tick with invalid ID");
}

// ============================================================
// QOL-01 Test 3: two coroutines both awaiting same tween — both resume
// ============================================================
static void test_tween_await_resumes_exactly_once() {
    printf("--- test_tween_await_resumes_exactly_once ---\n");

    QoLFixture f;

    // Create one tween, two coroutines both await it
    LuaResult r = f.exec(
        "g_a = 0\n"
        "g_b = 0\n"
        "local t = {x = 0}\n"
        "local id = engine.tween.to(t, {x = 100}, 0.1)\n"
        "engine.async.start(function()\n"
        "    engine.tween.await(id)\n"
        "    g_a = 1\n"
        "end)\n"
        "engine.async.start(function()\n"
        "    engine.tween.await(id)\n"
        "    g_b = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "test_tween_await_resumes_exactly_once: setup must not error");

    // Tick enough for tween to complete (0.15s > 0.1s duration)
    f.tickBoth(0.05f);
    f.tickBoth(0.1f);

    ASSERT(f.getNum("g_a") == 1.0, "g_a should be 1: first coroutine resumed");
    ASSERT(f.getNum("g_b") == 1.0, "g_b should be 1: second coroutine resumed");
}

// ============================================================
// QOL-02 Test 4: wait_frames(n) resumes after exactly n frames
// ============================================================
static void test_wait_frames_yields_exactly_n() {
    printf("--- test_wait_frames_yields_exactly_n ---\n");

    QoLFixture f;

    LuaResult r = f.exec(
        "g_resumed = 0\n"
        "engine.async.start(function()\n"
        "    engine.async.wait_frames(3)\n"
        "    g_resumed = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "test_wait_frames_yields_exactly_n: setup must not error");

    // Tick 1: coroutine starts, calls wait_frames(3) → sets waitFrames=2, yields
    f.tickCoroutines(0.016f);
    ASSERT(f.getNum("g_resumed") == 0.0, "should not resume after tick 1");

    // Tick 2: waitFrames 2→1, skip
    f.tickCoroutines(0.016f);
    ASSERT(f.getNum("g_resumed") == 0.0, "should not resume after tick 2");

    // Tick 3: waitFrames 1→0, resume — g_resumed = 1
    f.tickCoroutines(0.016f);
    ASSERT(f.getNum("g_resumed") == 1.0, "should resume after tick 3");
}

// ============================================================
// QOL-02 Test 5: wait_frames(0) returns immediately without yielding
// ============================================================
static void test_wait_frames_zero_resumes_immediately() {
    printf("--- test_wait_frames_zero_resumes_immediately ---\n");

    QoLFixture f;

    LuaResult r = f.exec(
        "g_resumed = 0\n"
        "engine.async.start(function()\n"
        "    engine.async.wait_frames(0)\n"
        "    g_resumed = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "test_wait_frames_zero_resumes_immediately: setup must not error");

    f.tickCoroutines(0.016f);
    ASSERT(f.getNum("g_resumed") == 1.0, "wait_frames(0) should resume in same tick");
}

// ============================================================
// QOL-02 Test 6: wait_frames(-1) returns immediately without yielding
// ============================================================
static void test_wait_frames_negative_resumes_immediately() {
    printf("--- test_wait_frames_negative_resumes_immediately ---\n");

    QoLFixture f;

    LuaResult r = f.exec(
        "g_resumed = 0\n"
        "engine.async.start(function()\n"
        "    engine.async.wait_frames(-5)\n"
        "    g_resumed = 1\n"
        "end)\n"
    );
    ASSERT(r.success, "test_wait_frames_negative_resumes_immediately: setup must not error");

    f.tickCoroutines(0.016f);
    ASSERT(f.getNum("g_resumed") == 1.0, "wait_frames(-5) should resume in same tick");
}

// ============================================================
// QOL-03 Test 7: dead zone freezes camera when target is inside
// ============================================================
static void test_dead_zone_freezes_camera() {
    printf("--- test_dead_zone_freezes_camera ---\n");

    // Use Scene + LuaScript pattern (matching camera_follow_test.cpp)
    Scene scene(10u);

    // Create target object at (5, 5) — inside 20x20 dead zone centered at origin
    // dx=5, dy=5, half-zone=10,10: 5<=10 AND 5<=10 => inside => freeze
    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("dz_target");
    targetObj->getPosition()->setPosition(5, 5);

    // Create script object with LuaScript + Camera
    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveScene(&scene);
    bindings.setActiveCamera(camera);

    // Set camera at origin — lerpSpeed=1.0 snaps immediately
    camera->setPosition(0.0f, 0.0f);

    bool loaded = script->loadScript(
        "g_target = nil\n"
        "function init(self)\n"
        "    g_target = engine.scene.find('dz_target')\n"
        "    if g_target then\n"
        "        engine.camera.follow(g_target, 1.0)\n"
        "        engine.camera.setDeadZone(20, 20)\n"
        "    end\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_dead_zone_freezes_camera: script loaded");
    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_dead_zone_freezes_camera: no script errors");

    // Record position before tick — camera at origin
    Vec2 before = camera->getPosition();

    // tickCameraFollow: target at (5,5), camera at (0,0), dead zone 20x20
    // dx=5, dy=5, half-zone=10: inside => return early (freeze)
    bindings.tickCameraFollow(0.1f);

    Vec2 after = camera->getPosition();
    ASSERT_NEAR_F(after.x, before.x, 0.001f, "test_dead_zone_freezes_camera: camera x should not move");
    ASSERT_NEAR_F(after.y, before.y, 0.001f, "test_dead_zone_freezes_camera: camera y should not move");
}

// ============================================================
// QOL-03 Test 8: camera resumes follow when target exits dead zone
// ============================================================
static void test_dead_zone_resumes_on_exit() {
    printf("--- test_dead_zone_resumes_on_exit ---\n");

    Scene scene(11u);

    // Create target object far outside the dead zone: (50, 50)
    // Camera at (0,0), dead zone 20x20 -> half=10: dx=50 > 10 => outside => follow
    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("dz_target2");
    targetObj->getPosition()->setPosition(50, 50);

    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveScene(&scene);
    bindings.setActiveCamera(camera);

    // Snap camera to origin (lerpSpeed=1.0 in lookAt snaps)
    camera->setPosition(0.0f, 0.0f);

    // Use lerpSpeed=1.0 so lookAt snaps camera to target immediately
    bool loaded = script->loadScript(
        "g_target = nil\n"
        "function init(self)\n"
        "    g_target = engine.scene.find('dz_target2')\n"
        "    if g_target then\n"
        "        engine.camera.follow(g_target, 1.0)\n"
        "        engine.camera.setDeadZone(20, 20)\n"
        "    end\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_dead_zone_resumes_on_exit: script loaded");
    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_dead_zone_resumes_on_exit: no script errors");

    Vec2 before = camera->getPosition();

    // tickCameraFollow — target at (50,50): dx=50 > 10 => outside dead zone
    // lookAt(50, 50, 1.0) fires => camera snaps to (50, 50)
    bindings.tickCameraFollow(0.1f);

    Vec2 after = camera->getPosition();
    float moved = fabsf(after.x - before.x) + fabsf(after.y - before.y);
    ASSERT(moved > 0.001f, "test_dead_zone_resumes_on_exit: camera should move when target is outside dead zone");
}

// ============================================================
// QOL-03 Test 9: setDeadZone(0, 0) disables dead zone — camera follows
// ============================================================
static void test_dead_zone_zero_disables() {
    printf("--- test_dead_zone_zero_disables ---\n");

    Scene scene(12u);

    // Target at (5, 5) — would be inside a 20x20 dead zone, but dead zone is disabled (0,0)
    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("dz_target3");
    targetObj->getPosition()->setPosition(5, 5);

    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveScene(&scene);
    bindings.setActiveCamera(camera);

    // Camera at (0,0)
    camera->setPosition(0.0f, 0.0f);

    // Use lerpSpeed=1.0 so camera snaps; dead zone disabled (0,0)
    bool loaded = script->loadScript(
        "g_target = nil\n"
        "function init(self)\n"
        "    g_target = engine.scene.find('dz_target3')\n"
        "    if g_target then\n"
        "        engine.camera.follow(g_target, 1.0)\n"
        "        engine.camera.setDeadZone(0, 0)  -- disabled\n"
        "    end\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_dead_zone_zero_disables: script loaded");
    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_dead_zone_zero_disables: no script errors");

    Vec2 before = camera->getPosition();

    // With dead zone disabled (0,0), camera should follow normally
    // lookAt(5, 5, 1.0) fires => camera snaps to (5, 5)
    bindings.tickCameraFollow(0.1f);

    Vec2 after = camera->getPosition();
    float moved = fabsf(after.x - before.x) + fabsf(after.y - before.y);
    ASSERT(moved > 0.001f, "test_dead_zone_zero_disables: camera should follow (dead zone disabled)");
}

// ============================================================
// main
// ============================================================
int main() {
    test_tween_await_suspends_and_resumes();
    test_tween_await_invalid_id_resumes_immediately();
    test_tween_await_resumes_exactly_once();
    test_wait_frames_yields_exactly_n();
    test_wait_frames_zero_resumes_immediately();
    test_wait_frames_negative_resumes_immediately();
    test_dead_zone_freezes_camera();
    test_dead_zone_resumes_on_exit();
    test_dead_zone_zero_disables();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
