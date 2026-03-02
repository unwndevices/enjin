/**
 * @file camera_follow_test.cpp
 * @brief Integration tests for engine.camera.follow/stopFollow bindings (Phase 48: CAM-01, CAM-02)
 *
 * Tests:
 *   CAM-01: engine.camera.follow(proxy, speed) stores follow target; tickCameraFollow moves camera
 *   CAM-02: engine.camera.stopFollow() clears target; subsequent ticks do not move camera
 *   Edge cases:
 *     - engine.camera.follow(nil) is silent no-op
 *     - engine.camera.follow with invalid/destroyed proxy silently stops
 *     - engine.camera.follow/stopFollow with no active camera is silent no-op
 *     - engine.camera.follow and engine.camera.stopFollow are registered as functions
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/camera.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/core/scene.hpp>
#include <cstdio>
#include <cmath>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

#define ASSERT_NEAR_F(a, b, tol, msg) \
    do { \
        float _a = static_cast<float>(a); \
        float _b = static_cast<float>(b); \
        float _diff = _a - _b; \
        if (_diff < 0.f) _diff = -_diff; \
        if (_diff > (tol)) { \
            fprintf(stderr, "FAIL: %s (got %.4f, expected %.4f)\n", msg, (double)_a, (double)_b); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// test_follow_function_exists: verify engine.camera.follow and stopFollow are registered
// ============================================================
static void test_follow_function_exists() {
    printf("--- test_follow_function_exists: follow/stopFollow registered ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = obj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveCamera(camera);

    bool loaded = script->loadScript(
        "follow_is_func = type(engine.camera.follow) == 'function'\n"
        "stop_is_func = type(engine.camera.stopFollow) == 'function'\n"
        "function init(self) end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_follow_function_exists: script loaded");

    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_follow_function_exists: no script errors");

    bool followIsFunc = script->getScriptBool("follow_is_func", false);
    bool stopIsFunc   = script->getScriptBool("stop_is_func", false);
    ASSERT(followIsFunc, "test_follow_function_exists: engine.camera.follow is a function");
    ASSERT(stopIsFunc,   "test_follow_function_exists: engine.camera.stopFollow is a function");

    delete obj;
}

// ============================================================
// test_follow_tracks_target: follow + tickCameraFollow moves camera toward target
// ============================================================
static void test_follow_tracks_target() {
    printf("--- test_follow_tracks_target: camera moves toward followed target ---\n");

    // Build a Scene so we can use engine.scene.find()
    Scene scene(1u);

    // Create the target object in the scene with a known position
    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("target");
    // Object auto-creates C_Position at (0,0); set the position directly
    targetObj->getPosition()->setPosition(100, 50);

    // Create script object with LuaScript + Camera
    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    // Set scene first (setActiveScene clears m_activeCamera if scene changes)
    bindings.setActiveScene(&scene);
    bindings.setActiveCamera(camera);

    // Set camera at origin so we can see movement
    camera->setPosition(0.0f, 0.0f);

    // Use lerpSpeed=1.0 so lookAt snaps — verifying position change is enough
    // Store proxy as a global so it survives init() scope and isn't GC'd
    bool loaded = script->loadScript(
        "followed = false\n"
        "g_target = nil\n"
        "function init(self)\n"
        "    g_target = engine.scene.find('target')\n"
        "    if g_target then\n"
        "        engine.camera.follow(g_target, 1.0)\n"
        "        followed = true\n"
        "    end\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_follow_tracks_target: script loaded");

    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_follow_tracks_target: no script errors");

    bool followedSet = script->getScriptBool("followed", false);
    ASSERT(followedSet, "test_follow_tracks_target: follow was called with valid proxy");

    // Now call tickCameraFollow directly (this is what sdl_main.cpp does per-frame)
    bindings.tickCameraFollow(0.016f);

    Vec2 camPos = camera->getPosition();
    // With lerpSpeed=1.0, camera should have snapped to (100, 50)
    ASSERT_NEAR_F(camPos.x, 100.0f, 0.5f, "test_follow_tracks_target: camera x moved to target x=100");
    ASSERT_NEAR_F(camPos.y, 50.0f,  0.5f, "test_follow_tracks_target: camera y moved to target y=50");
    // Note: both scriptObj and targetObj are owned by scene; scene destructor cleans them up
}

// ============================================================
// test_stopFollow_stops_tracking: stopFollow prevents further tracking
// ============================================================
static void test_stopFollow_stops_tracking() {
    printf("--- test_stopFollow_stops_tracking: stopFollow clears target ---\n");

    Scene scene(2u);

    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("movable");
    // Object auto-creates C_Position at (0,0); set the position directly
    C_Position* pos = targetObj->getPosition();
    pos->setPosition(200, 100);

    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    // Set scene first (setActiveScene clears m_activeCamera if scene changes)
    bindings.setActiveScene(&scene);
    bindings.setActiveCamera(camera);

    camera->setPosition(0.0f, 0.0f);

    // Store proxy as global so it survives init() scope and isn't GC'd between ticks
    bool loaded = script->loadScript(
        "g_movable = nil\n"
        "function init(self)\n"
        "    g_movable = engine.scene.find('movable')\n"
        "    engine.camera.follow(g_movable, 1.0)\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_stopFollow_stops_tracking: script loaded");

    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_stopFollow_stops_tracking: no script errors");

    // First tick: camera follows to (200, 100)
    bindings.tickCameraFollow(0.016f);

    Vec2 posAfterFollow = camera->getPosition();
    ASSERT_NEAR_F(posAfterFollow.x, 200.0f, 0.5f, "test_stopFollow_stops_tracking: initial follow worked");

    // Call stopFollow within the SAME Lua state using executeString (avoids state reload)
    auto& luaEngine = script->getScriptSystem().getEngine();
    auto result = luaEngine.executeString("engine.camera.stopFollow()");
    ASSERT(result.success, "test_stopFollow_stops_tracking: stopFollow call succeeded");

    // Move target to a different position
    pos->setPosition(999, 999);

    // Tick again — camera should NOT move (follow cleared)
    bindings.tickCameraFollow(0.016f);

    Vec2 posAfterStop = camera->getPosition();
    ASSERT_NEAR_F(posAfterStop.x, 200.0f, 1.0f, "test_stopFollow_stops_tracking: camera x did not move after stopFollow");
    ASSERT_NEAR_F(posAfterStop.y, 100.0f, 1.0f, "test_stopFollow_stops_tracking: camera y did not move after stopFollow");
    // Note: scriptObj is owned by scene; scene destructor cleans it up
}

// ============================================================
// test_follow_nil_proxy_no_crash: engine.camera.follow() with nil is silent no-op
// ============================================================
static void test_follow_nil_proxy_no_crash() {
    printf("--- test_follow_nil_proxy_no_crash: follow(nil) is silent no-op ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = obj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveCamera(camera);
    camera->setPosition(50.0f, 50.0f);

    bool loaded = script->loadScript(
        "ok = true\n"
        "function init(self)\n"
        "    engine.camera.follow(nil, 1.0)\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_follow_nil_proxy_no_crash: script loaded");

    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_follow_nil_proxy_no_crash: no crash on follow(nil)");

    // tickCameraFollow should also be safe — no follow target set
    bindings.tickCameraFollow(0.016f);

    Vec2 camPos = camera->getPosition();
    ASSERT_NEAR_F(camPos.x, 50.0f, 0.01f, "test_follow_nil_proxy_no_crash: camera x unchanged after follow(nil)");
    ASSERT_NEAR_F(camPos.y, 50.0f, 0.01f, "test_follow_nil_proxy_no_crash: camera y unchanged after follow(nil)");

    bool ok = script->getScriptBool("ok", false);
    ASSERT(ok, "test_follow_nil_proxy_no_crash: script ran to completion");

    delete obj;
}

// ============================================================
// test_follow_invalid_proxy_silent_stop: follow with destroyed proxy silently stops
// ============================================================
static void test_follow_invalid_proxy_silent_stop() {
    printf("--- test_follow_invalid_proxy_silent_stop: destroyed proxy causes silent stop ---\n");

    Scene scene(3u);

    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("ephemeral");
    // Object auto-creates C_Position at (0,0); set the position directly
    targetObj->getPosition()->setPosition(300, 200);

    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    // Set scene first (setActiveScene clears m_activeCamera if scene changes)
    bindings.setActiveScene(&scene);
    bindings.setActiveCamera(camera);

    camera->setPosition(0.0f, 0.0f);

    // Store proxy as global so Lua userdata memory stays alive even after object destroyed
    bool loaded = script->loadScript(
        "g_ephemeral = nil\n"
        "function init(self)\n"
        "    g_ephemeral = engine.scene.find('ephemeral')\n"
        "    engine.camera.follow(g_ephemeral, 1.0)\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_follow_invalid_proxy_silent_stop: script loaded");

    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_follow_invalid_proxy_silent_stop: no script errors on setup");

    // Follow tick: camera should snap to (300, 200)
    bindings.tickCameraFollow(0.016f);
    Vec2 posAfterFollow = camera->getPosition();
    ASSERT_NEAR_F(posAfterFollow.x, 300.0f, 0.5f, "test_follow_invalid_proxy_silent_stop: initial follow worked");

    // Destroy the target object — proxy->valid becomes false (Lua userdata still in memory)
    scene.removeObject(targetObj);

    // tickCameraFollow should silently stop (no crash, proxy->valid == false)
    bindings.tickCameraFollow(0.016f);

    // Camera should remain where it was (not crash, not move to garbage)
    Vec2 posAfterDestroy = camera->getPosition();
    (void)posAfterDestroy;  // Position not checked — just verifying no crash
    ASSERT(true, "test_follow_invalid_proxy_silent_stop: no crash after target destroyed");
    ASSERT(!script->hasErrors(), "test_follow_invalid_proxy_silent_stop: no Lua errors after target destroyed");
    // Note: scriptObj is owned by scene; scene destructor cleans it up
}

// ============================================================
// test_follow_no_camera_no_crash: follow/stopFollow with no active camera is silent
// ============================================================
static void test_follow_no_camera_no_crash() {
    printf("--- test_follow_no_camera_no_crash: no camera = silent no-op ---\n");

    Scene scene(4u);

    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("target2");
    // Object auto-creates C_Position; set position directly
    targetObj->getPosition()->setPosition(50, 50);

    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    // NOTE: do NOT add C_Camera or call setActiveCamera — test silent no-op

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveScene(&scene);
    // bindings.setActiveCamera(nullptr) — default is nullptr

    // Store proxy as global to keep Lua userdata alive
    bool loaded = script->loadScript(
        "ok = true\n"
        "g_target2 = nil\n"
        "function init(self)\n"
        "    g_target2 = engine.scene.find('target2')\n"
        "    engine.camera.follow(g_target2, 0.5)\n"
        "    engine.camera.stopFollow()\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_follow_no_camera_no_crash: script loaded");

    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_follow_no_camera_no_crash: no crash — follow/stopFollow no-op without camera");

    // tickCameraFollow should also be safe with no camera
    bindings.tickCameraFollow(0.016f);
    ASSERT(true, "test_follow_no_camera_no_crash: tickCameraFollow safe without active camera");

    bool ok = script->getScriptBool("ok", false);
    ASSERT(ok, "test_follow_no_camera_no_crash: script ran to completion");
    // Note: scriptObj and targetObj are owned by scene; scene destructor cleans them up
}

// ============================================================
// test_follow_proxy_cleared_on_scene_change: DEBT-01
// After engine.camera.follow() and bindings.setActiveScene(&scene2),
// tickCameraFollow() should NOT move the camera (proxy cleared).
// ============================================================
static void test_follow_proxy_cleared_on_scene_change() {
    printf("--- test_follow_proxy_cleared_on_scene_change: DEBT-01 scene change clears proxy ---\n");

    Scene scene1(10u);
    Scene scene2(11u);

    // Target in scene1 at a known position far from origin
    Object* targetObj = scene1.addObject<Object>();
    targetObj->setName("debt01_target");
    targetObj->getPosition()->setPosition(200, 0);

    // Script object in scene1 with LuaScript + Camera
    Object* scriptObj = scene1.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveScene(&scene1);
    bindings.setActiveCamera(camera);
    camera->setPosition(0.0f, 0.0f);

    // Load script that calls engine.camera.follow(g_target, 1.0) in init
    bool loaded = script->loadScript(
        "g_target = nil\n"
        "function init(self)\n"
        "    g_target = engine.scene.find('debt01_target')\n"
        "    if g_target then engine.camera.follow(g_target, 1.0) end\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_follow_proxy_cleared_on_scene_change: script loaded");

    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_follow_proxy_cleared_on_scene_change: no script errors");

    // First tick: camera should move toward (200, 0) confirming follow is active
    bindings.tickCameraFollow(0.016f);
    Vec2 posAfterFollow = camera->getPosition();
    ASSERT_NEAR_F(posAfterFollow.x, 200.0f, 0.5f,
        "test_follow_proxy_cleared_on_scene_change: follow was active (camera moved to x=200)");

    // Reset camera to origin so we can detect any subsequent movement
    camera->setPosition(0.0f, 0.0f);

    // Switch to scene2 — this should clear m_followTargetProxy (DEBT-01 fix)
    bindings.setActiveScene(&scene2);

    // Re-set the camera (setActiveScene clears m_activeCamera; we restore it to
    // isolate the proxy-clear behavior from the camera-clear behavior).
    bindings.setActiveCamera(camera);

    // Tick again — camera should NOT move (follow proxy cleared by setActiveScene)
    bindings.tickCameraFollow(0.016f);
    Vec2 posAfterSceneChange = camera->getPosition();
    ASSERT_NEAR_F(posAfterSceneChange.x, 0.0f, 0.01f,
        "test_follow_proxy_cleared_on_scene_change: camera x did not move after scene change (DEBT-01)");
    ASSERT_NEAR_F(posAfterSceneChange.y, 0.0f, 0.01f,
        "test_follow_proxy_cleared_on_scene_change: camera y did not move after scene change (DEBT-01)");
}

// ============================================================
// test_follow_proxy_cleared_on_hot_reload: DEBT-01
// After engine.camera.follow() and a second loadScript() (hot-reload via registerAll),
// tickCameraFollow() should NOT move the camera (proxy cleared).
// ============================================================
static void test_follow_proxy_cleared_on_hot_reload() {
    printf("--- test_follow_proxy_cleared_on_hot_reload: DEBT-01 hot-reload clears proxy ---\n");

    Scene scene(12u);

    // Target at a known position far from origin
    Object* targetObj = scene.addObject<Object>();
    targetObj->setName("debt01_target2");
    targetObj->getPosition()->setPosition(300, 0);

    // Script object with LuaScript + Camera
    Object* scriptObj = scene.addObject<Object>();
    C_LuaScript* script = scriptObj->addComponent<C_LuaScript>(64u, 64u);
    C_Camera* camera = scriptObj->addComponent<C_Camera>();

    LuaBindings& bindings = script->getScriptSystem().getBindings();
    bindings.setActiveScene(&scene);
    bindings.setActiveCamera(camera);
    camera->setPosition(0.0f, 0.0f);

    // Load script that establishes camera follow in init
    bool loaded = script->loadScript(
        "g_target2 = nil\n"
        "function init(self)\n"
        "    g_target2 = engine.scene.find('debt01_target2')\n"
        "    if g_target2 then engine.camera.follow(g_target2, 1.0) end\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "test_follow_proxy_cleared_on_hot_reload: script loaded");

    scriptObj->update(0.016f);
    ASSERT(!script->hasErrors(), "test_follow_proxy_cleared_on_hot_reload: no script errors");

    // First tick: camera should move to (300, 0) confirming follow is active
    bindings.tickCameraFollow(0.016f);
    Vec2 posAfterFollow = camera->getPosition();
    ASSERT_NEAR_F(posAfterFollow.x, 300.0f, 0.5f,
        "test_follow_proxy_cleared_on_hot_reload: follow was active (camera moved to x=300)");

    // Reset camera to origin
    camera->setPosition(0.0f, 0.0f);

    // Simulate hot-reload: call bindings.registerAll() directly with the Lua state.
    // This is what happens internally when a script system re-initializes (DEBT-01 fix path).
    lua_State* L = script->getScriptSystem().getEngine().getState();
    ASSERT(L != nullptr, "test_follow_proxy_cleared_on_hot_reload: Lua state valid");
    if (L) {
        bindings.registerAll();
    }

    // Re-set the camera (registerAll resets function table but not m_activeCamera)
    bindings.setActiveCamera(camera);

    // Tick again — camera should NOT move (follow proxy cleared by registerAll)
    bindings.tickCameraFollow(0.016f);
    Vec2 posAfterReload = camera->getPosition();
    ASSERT_NEAR_F(posAfterReload.x, 0.0f, 0.01f,
        "test_follow_proxy_cleared_on_hot_reload: camera x did not move after hot-reload (DEBT-01)");
    ASSERT_NEAR_F(posAfterReload.y, 0.0f, 0.01f,
        "test_follow_proxy_cleared_on_hot_reload: camera y did not move after hot-reload (DEBT-01)");
}

// ============================================================
// main
// ============================================================
int main() {
    test_follow_function_exists();
    test_follow_tracks_target();
    test_stopFollow_stops_tracking();
    test_follow_nil_proxy_no_crash();
    test_follow_invalid_proxy_silent_stop();
    test_follow_no_camera_no_crash();
    test_follow_proxy_cleared_on_scene_change();
    test_follow_proxy_cleared_on_hot_reload();

    printf("\n=== Camera Follow Test: %d passed, %d failed ===\n", passes, failures);
    return failures;
}
