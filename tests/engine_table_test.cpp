/**
 * @file engine_table_test.cpp
 * @brief Tests for engine.* global Lua table (Phase 31 ENG-01..ENG-06)
 *
 * Verifies:
 * - Module-level access to all engine.* sub-tables (ENG-06)
 * - Behavioral correctness with no host data injected (null-guard paths)
 * - engine.time.* reads live EngineTimeState after setTimeState()
 * - engine.log does not crash on any argument type
 * - ENG-01 live wiring: engine.scene.switch() reaches SceneStateMachine::switchTo()
 * - ENG-02 live wiring: engine.scene.find() returns non-nil ObjectProxy for named Object
 * - PROXY-01 live wiring: loadScriptFile() init(self) receives valid ScriptProxy
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/core/scene_state_machine.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <cstdio>
#include <cstring>

using namespace enjin2;

static int passes = 0;
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
// Minimal fixture: LuaEngine + LuaBindings, no canvas, no input
// ============================================================
struct EngineTableFixture {
    LuaEngine engine;
    LuaBindings bindings;

    EngineTableFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    // Returns the numeric global as double. Returns -1.0 if not found.
    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }
};

// ============================================================
// test_engine_global_not_nil
// ENG-06: accessing engine and all sub-tables at module level must not crash
// ============================================================
static void test_engine_global_not_nil() {
    printf("--- engine global not nil (module-level access) ---\n");

    EngineTableFixture f;

    // Module-level access: these execute at parse/load time, not inside any function
    LuaResult r = f.exec(
        "local s = engine.scene\n"
        "local i = engine.input\n"
        "local t = engine.time\n"
        "local l = engine.lua\n"
        "local g = engine.log\n"
        "local c = engine.collision\n"
        "ok_scene  = (s ~= nil) and 1 or 0\n"
        "ok_input  = (i ~= nil) and 1 or 0\n"
        "ok_time   = (t ~= nil) and 1 or 0\n"
        "ok_lua    = (l ~= nil) and 1 or 0\n"
        "ok_log    = (g ~= nil) and 1 or 0\n"
        "ok_collision = (c ~= nil) and 1 or 0\n"
    );
    ASSERT(r.success, "module-level engine.* access should not error");
    ASSERT(f.getNum("ok_scene") == 1.0, "engine.scene should be non-nil");
    ASSERT(f.getNum("ok_input") == 1.0, "engine.input should be non-nil");
    ASSERT(f.getNum("ok_time")  == 1.0, "engine.time should be non-nil");
    ASSERT(f.getNum("ok_lua")   == 1.0, "engine.lua should be non-nil");
    ASSERT(f.getNum("ok_log")   == 1.0, "engine.log should be non-nil");
    ASSERT(f.getNum("ok_collision") == 1.0, "engine.collision should be non-nil");
}

// ============================================================
// test_engine_type_checks
// All sub-tables must be 'table'; log must be 'function'
// ============================================================
static void test_engine_type_checks() {
    printf("--- engine type checks ---\n");

    EngineTableFixture f;
    LuaResult r = f.exec(
        "t_engine = (type(engine)       == 'table')   and 1 or 0\n"
        "t_scene  = (type(engine.scene) == 'table')   and 1 or 0\n"
        "t_input  = (type(engine.input) == 'table')   and 1 or 0\n"
        "t_time   = (type(engine.time)  == 'table')   and 1 or 0\n"
        "t_lua    = (type(engine.lua)   == 'table')   and 1 or 0\n"
        "t_collision = (type(engine.collision) == 'table') and 1 or 0\n"
        "t_log    = (type(engine.log)   == 'function') and 1 or 0\n"
    );
    ASSERT(r.success, "type checks should not error");
    ASSERT(f.getNum("t_engine") == 1.0, "type(engine) should be 'table'");
    ASSERT(f.getNum("t_scene")  == 1.0, "type(engine.scene) should be 'table'");
    ASSERT(f.getNum("t_input")  == 1.0, "type(engine.input) should be 'table'");
    ASSERT(f.getNum("t_time")   == 1.0, "type(engine.time) should be 'table'");
    ASSERT(f.getNum("t_lua")    == 1.0, "type(engine.lua) should be 'table'");
    ASSERT(f.getNum("t_collision") == 1.0, "type(engine.collision) should be 'table'");
    ASSERT(f.getNum("t_log")    == 1.0, "type(engine.log) should be 'function'");
}

// ============================================================
// test_engine_input_null_guards
// ENG-03: input functions return false/0.0 when no input is set
// ============================================================
static void test_engine_input_null_guards() {
    printf("--- engine.input null guards ---\n");

    EngineTableFixture f;
    // No bindings.setInput() call — currentInput is nullptr
    LuaResult r = f.exec(
        "h  = engine.input.held(0)          and 1 or 0\n"
        "jp = engine.input.just_pressed(0)  and 1 or 0\n"
        "jr = engine.input.just_released(0) and 1 or 0\n"
        "ax = engine.input.axis(0)\n"
    );
    ASSERT(r.success, "engine.input.* should not error when input is nil");
    ASSERT(f.getNum("h")  == 0.0, "held() should return false when no input");
    ASSERT(f.getNum("jp") == 0.0, "just_pressed() should return false when no input");
    ASSERT(f.getNum("jr") == 0.0, "just_released() should return false when no input");
    ASSERT(f.getNum("ax") == 0.0, "axis() should return 0.0 when no input");
}

// ============================================================
// test_engine_time_defaults
// ENG-04: time functions return 0 when setTimeState not called
// ============================================================
static void test_engine_time_defaults() {
    printf("--- engine.time defaults ---\n");

    EngineTableFixture f;
    LuaResult r = f.exec(
        "d = engine.time.delta()\n"
        "n = engine.time.now()\n"
        "fr = engine.time.frame()\n"
    );
    ASSERT(r.success, "engine.time.* should not error");
    ASSERT(f.getNum("d")  == 0.0, "delta() should return 0.0 before setTimeState");
    ASSERT(f.getNum("n")  == 0.0, "now() should return 0.0 before setTimeState");
    ASSERT(f.getNum("fr") == 0.0, "frame() should return 0 before setTimeState");
}

// ============================================================
// test_engine_time_after_setTimeState
// ENG-04: time functions return live values after setTimeState
// ============================================================
static void test_engine_time_after_setTimeState() {
    printf("--- engine.time after setTimeState ---\n");

    EngineTableFixture f;
    f.bindings.setTimeState(0.016f, 1.5f, 100u);

    LuaResult r = f.exec(
        "d  = engine.time.delta()\n"
        "n  = engine.time.now()\n"
        "fr = engine.time.frame()\n"
    );
    ASSERT(r.success, "engine.time.* should not error after setTimeState");
    // Float tolerance: 0.016 may round slightly
    double d = f.getNum("d");
    ASSERT(d > 0.015 && d < 0.017, "delta() should return ~0.016 after setTimeState");
    double n = f.getNum("n");
    ASSERT(n > 1.499 && n < 1.501, "now() should return ~1.5 after setTimeState");
    ASSERT(f.getNum("fr") == 100.0, "frame() should return 100 after setTimeState");
}

// ============================================================
// test_engine_log_no_crash
// ENG-05: engine.log does not crash on string, number, bool, nil args
// ============================================================
static void test_engine_log_no_crash() {
    printf("--- engine.log no crash ---\n");

    EngineTableFixture f;
    LuaResult r1 = f.exec("engine.log('hello world')");
    ASSERT(r1.success, "engine.log(string) should not error");

    LuaResult r2 = f.exec("engine.log(42)");
    ASSERT(r2.success, "engine.log(number) should not error");

    LuaResult r3 = f.exec("engine.log(true)");
    ASSERT(r3.success, "engine.log(boolean) should not error");

    LuaResult r4 = f.exec("engine.log(nil)");
    ASSERT(r4.success, "engine.log(nil) should not error");

    LuaResult r5 = f.exec("engine.log('a', 'b', 'c')");
    ASSERT(r5.success, "engine.log(multiple args) should not error");
}

// ============================================================
// test_engine_collision_basic
// engine.collision.* functions return correct results
// ============================================================
static void test_engine_collision_basic() {
    printf("--- engine.collision basic ---\n");

    EngineTableFixture f;

    // aabb: overlapping rects
    LuaResult r1 = f.exec(
        "aabb_hit = engine.collision.aabb(0,0,10,10, 5,5,10,10) and 1 or 0\n"
    );
    ASSERT(r1.success, "engine.collision.aabb should not error");
    ASSERT(f.getNum("aabb_hit") == 1.0, "aabb overlapping rects should return true");

    // aabb: non-overlapping
    LuaResult r2 = f.exec(
        "aabb_miss = engine.collision.aabb(0,0,10,10, 20,20,10,10) and 1 or 0\n"
    );
    ASSERT(r2.success, "engine.collision.aabb non-overlap should not error");
    ASSERT(f.getNum("aabb_miss") == 0.0, "aabb non-overlapping rects should return false");

    // pointInRect
    LuaResult r3 = f.exec(
        "pr_hit = engine.collision.pointInRect(5,5, 0,0,10,10) and 1 or 0\n"
        "pr_miss = engine.collision.pointInRect(15,15, 0,0,10,10) and 1 or 0\n"
    );
    ASSERT(r3.success, "engine.collision.pointInRect should not error");
    ASSERT(f.getNum("pr_hit") == 1.0, "pointInRect inside should return true");
    ASSERT(f.getNum("pr_miss") == 0.0, "pointInRect outside should return false");

    // circleCircle
    LuaResult r4 = f.exec(
        "cc_hit = engine.collision.circleCircle(0,0,5, 8,0,5) and 1 or 0\n"
        "cc_miss = engine.collision.circleCircle(0,0,5, 20,0,5) and 1 or 0\n"
    );
    ASSERT(r4.success, "engine.collision.circleCircle should not error");
    ASSERT(f.getNum("cc_hit") == 1.0, "circleCircle overlapping should return true");
    ASSERT(f.getNum("cc_miss") == 0.0, "circleCircle non-overlapping should return false");

    // lineLine with intersection
    LuaResult r5 = f.exec(
        "local hit, ix, iy = engine.collision.lineLine(0,0,10,10, 0,10,10,0)\n"
        "ll_hit = hit and 1 or 0\n"
        "ll_ix = ix or 0\n"
        "ll_iy = iy or 0\n"
    );
    ASSERT(r5.success, "engine.collision.lineLine should not error");
    ASSERT(f.getNum("ll_hit") == 1.0, "lineLine crossing should return true");
    double ix = f.getNum("ll_ix");
    double iy = f.getNum("ll_iy");
    ASSERT(ix > 4.9 && ix < 5.1, "lineLine intersection x should be ~5");
    ASSERT(iy > 4.9 && iy < 5.1, "lineLine intersection y should be ~5");
}

// ============================================================
// test_engine_scene_null_guards
// ENG-01, ENG-02: scene functions are no-ops / return nil with no SSM
// ============================================================
static void test_engine_scene_null_guards() {
    printf("--- engine.scene null guards ---\n");

    EngineTableFixture f;
    // No bindings.setSceneStateMachine() or setActiveScene() calls

    LuaResult r1 = f.exec("engine.scene.switch(1)");
    ASSERT(r1.success, "engine.scene.switch() should not crash without SSM");

    LuaResult r2 = f.exec("ok = (engine.scene.find('x') == nil) and 1 or 0");
    ASSERT(r2.success, "engine.scene.find() nil check should not error");
    ASSERT(f.getNum("ok") == 1.0, "engine.scene.find() should return nil when no active scene");
}

// ============================================================
// Minimal concrete scene for live-wiring tests
// ============================================================
struct MinimalScene : Scene {
    explicit MinimalScene(uint32_t id) : Scene(id) {}
};

// ============================================================
// test_engine_scene_live_switch
// ENG-01: engine.scene.switch() reaches SceneStateMachine::switchTo()
// when SSM is injected after registerAll() (pointer-to-pointer fix)
// ============================================================
static void test_engine_scene_live_switch() {
    printf("--- engine.scene.switch live wiring (ENG-01) ---\n");

    EngineTableFixture f;

    // Use a real SceneStateMachine with a registered scene.
    // switchTo() only queues if the scene ID exists in the SSM.
    SceneStateMachine mockSSM;
    mockSSM.addScene<MinimalScene>(2u);

    // Inject AFTER registerAll() — the pointer-to-pointer fix makes this live.
    f.bindings.setSceneStateMachine(&mockSSM);

    // The SSM has no current scene; switchTo(2) is valid (scene 2 registered).
    // switchTo() sets m_hasPendingTransition = true internally.
    // We verify by calling update() — if hasPendingTransition was set, applyDeferredTransition
    // runs and sets currentScene to scene 2.
    LuaResult r = f.exec("engine.scene.switch(2)");
    ASSERT(r.success, "engine.scene.switch(2) should not error");

    // Apply the deferred transition to verify switchTo() was actually called.
    mockSSM.update(0.0f);
    ASSERT(mockSSM.getCurrentScene() != nullptr, "ENG-01: switchTo() set pending transition, SSM applied it");
    ASSERT(mockSSM.getCurrentScene()->getId() == 2u, "ENG-01: current scene is now scene 2");
}

// ============================================================
// test_engine_scene_live_find
// ENG-02: engine.scene.find() returns non-nil ObjectProxy for named Object
// when active scene is injected after registerAll() (pointer-to-pointer fix)
// ============================================================
static void test_engine_scene_live_find() {
    printf("--- engine.scene.find live wiring (ENG-02) ---\n");

    EngineTableFixture f;

    // Create a Scene and add a named Object to it.
    // Scene requires a uint32_t id; Scene::addObject<T>() adds to ObjectCollection.
    MinimalScene scene(1u);
    Object* obj = scene.addObject<Object>();
    obj->setName("hero");

    // Inject AFTER registerAll() — the pointer-to-pointer fix makes this live.
    f.bindings.setActiveScene(&scene);

    LuaResult r = f.exec("found = (engine.scene.find('hero') ~= nil) and 1 or 0");
    ASSERT(r.success, "engine.scene.find('hero') should not error");
    ASSERT(f.getNum("found") == 1.0, "engine.scene.find('hero') should return non-nil proxy");

    LuaResult r2 = f.exec("missing = (engine.scene.find('nobody') == nil) and 1 or 0");
    ASSERT(r2.success, "engine.scene.find('nobody') should not error");
    ASSERT(f.getNum("missing") == 1.0, "engine.scene.find('nobody') should return nil");
}

// ============================================================
// test_proxy01_load_script_file_init_self
// PROXY-01: loadScriptFile() init(self) receives a valid ScriptProxy
// ============================================================
static void test_proxy01_load_script_file_init_self() {
    printf("--- PROXY-01: loadScriptFile init(self) receives proxy ---\n");

    // Write a minimal Lua script to /tmp that checks self is non-nil in init().
    const char* tmpPath = "/tmp/proxy01_test.lua";
    FILE* f = fopen(tmpPath, "w");
    ASSERT(f != nullptr, "PROXY-01: should be able to write /tmp/proxy01_test.lua");
    if (!f) return;

    fprintf(f, "function init(self)\n");
    fprintf(f, "    init_self_valid = (self ~= nil) and 1 or 0\n");
    fprintf(f, "end\n");
    fclose(f);

    // Create a C_LuaScript via Object (same pattern as error_policy_test.cpp)
    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "PROXY-01: addComponent<C_LuaScript> should succeed");
    if (!script) return;

    bool loaded = script->loadScriptFile(tmpPath);
    ASSERT(loaded, "PROXY-01: loadScriptFile should succeed");
    if (!loaded) return;

    // init() was called inside loadScriptFile(); verify the global it set
    double val = script->getScriptNumber("init_self_valid", -1.0);
    ASSERT(val == 1.0, "PROXY-01: init(self) received a valid ScriptProxy (not nil)");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("=== engine_table_test ===\n");

    test_engine_global_not_nil();
    test_engine_type_checks();
    test_engine_input_null_guards();
    test_engine_time_defaults();
    test_engine_time_after_setTimeState();
    test_engine_log_no_crash();
    test_engine_collision_basic();
    test_engine_scene_null_guards();
    test_engine_scene_live_switch();
    test_engine_scene_live_find();
    test_proxy01_load_script_file_init_self();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
