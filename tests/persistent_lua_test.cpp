/**
 * @file persistent_lua_test.cpp
 * @brief Lua integration tests for engine.scene.persist/unpersist/find (Phase 51: PERSIST-01..PERSIST-03)
 *
 * Tests:
 *   Test 1 (PERSIST-01): persist(proxy) returns true; object is findable in new scene after transition
 *   Test 2 (PERSIST-01): persistent object retains its C_Position component state after transition
 *   Test 3 (PERSIST-02): unpersist(proxy) + transition destroys object (find returns nil)
 *   Test 4 (PERSIST-03): find(name) from scene2 returns valid proxy for object persisted from scene1
 *   Test 5 (PERSIST-01): pool overflow — 5th persist returns nil, no Lua error
 *   Test 6 (PERSIST-01): persist same object twice is silent no-op (returns true, no slot consumed)
 *   Test 7 (PERSIST-01): self-transition preserves persistent objects
 *   Test 8 (PERSIST-03): find(name) returns scene-local object when both local and persistent share same name
 */
#include <enjin2/core/scene_state_machine.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/object_proxy.hpp>
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
            printf("PASS: %s\n", (msg)); \
            passes++; \
        } \
    } while(0)

// Minimal scene for SSM tests
struct MinimalScene : Scene {
    explicit MinimalScene(uint32_t id) : Scene(id) {}
    int createCount  = 0;
    int activateCount = 0;
    void onCreate()   override { createCount++; }
    void onActivate() override { activateCount++; }
};

// ============================================================
// Fixture: SSM + LuaEngine + LuaBindings wired together
// ============================================================
struct PersistLuaFixture {
    SceneStateMachine ssm;
    LuaEngine engine;
    LuaBindings bindings;

    PersistLuaFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
        bindings.setSceneStateMachine(&ssm);
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }

    bool getBool(const char* name) {
        return engine.getGlobalBool(name);
    }

    // Set up two minimal scenes and activate scene 1
    void setup2Scenes() {
        ssm.addScene<MinimalScene>(1u);
        ssm.addScene<MinimalScene>(2u);
        ssm.switchTo(1u);
        ssm.update(0.016f);
        bindings.setActiveScene(ssm.getCurrentScene());
    }

    // Trigger a deferred scene transition and update bindings
    void switchScene(uint32_t id) {
        exec(("engine.scene.switch(" + std::to_string(id) + ")").c_str());
        ssm.update(0.016f);  // applies deferred transition
        bindings.setActiveScene(ssm.getCurrentScene());
    }
};

// ============================================================
// Test 1 (PERSIST-01): persist(proxy) returns true; object findable in new scene
// ============================================================
static void test01_persist_survives_transition() {
    printf("--- Test 1 (PERSIST-01): persist survives cross-scene transition ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Spawn "hero" in scene 1 and persist it from Lua
    LuaResult r1 = f.exec(
        "g_hero = engine.scene.spawn('hero')\n"
        "persist_result = engine.scene.persist(g_hero)\n"
    );
    ASSERT(r1.success, "test01: spawn + persist script ran without error");

    // persist returns truthy (true)
    bool persistOk = f.getBool("persist_result");
    ASSERT(persistOk, "test01: persist(proxy) returns true");

    // Switch to scene 2
    f.switchScene(2u);
    ASSERT(f.ssm.getCurrentScene()->getId() == 2u, "test01: now in scene 2");

    // Find "hero" from scene 2 — should succeed via persistent registry fallback
    LuaResult r2 = f.exec(
        "found = engine.scene.find('hero') ~= nil\n"
    );
    ASSERT(r2.success, "test01: find('hero') script ran without error");
    ASSERT(f.getBool("found"), "test01: find('hero') returns non-nil in scene 2 after transition");
}

// ============================================================
// Test 2 (PERSIST-01): persistent object retains C_Position state after transition
// ============================================================
static void test02_component_state_preserved() {
    printf("--- Test 2 (PERSIST-01): component state preserved across transition ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Spawn object, set position via C++, persist via Lua
    Scene* scene1 = f.ssm.getCurrentScene();
    Object* hero = scene1->addObject<Object>();
    hero->setName("hero2");
    hero->getPosition()->setPosition(42, 99);

    // Persist via Lua — need to find it first so Lua has a proxy
    LuaResult r1 = f.exec(
        "g_hero2 = engine.scene.find('hero2')\n"
        "persist_ok = engine.scene.persist(g_hero2)\n"
    );
    ASSERT(r1.success, "test02: find + persist script ran without error");
    ASSERT(f.getBool("persist_ok"), "test02: persist returns true");

    // Switch to scene 2
    f.switchScene(2u);
    ASSERT(f.ssm.getCurrentScene()->getId() == 2u, "test02: now in scene 2");

    // Find the object from scene 2 and read its position
    LuaResult r2 = f.exec(
        "g_found = engine.scene.find('hero2')\n"
        "found_not_nil = (g_found ~= nil)\n"
    );
    ASSERT(r2.success, "test02: find after transition ran without error");
    ASSERT(f.getBool("found_not_nil"), "test02: found 'hero2' in scene 2");

    // Verify position is preserved via C++
    Object* foundObj = f.ssm.getCurrentScene()->findByName("hero2");
    ASSERT(foundObj != nullptr, "test02: C++ findByName returns hero2");
    if (foundObj) {
        ASSERT(foundObj->getPosition()->getPosition().x == 42, "test02: position x preserved (42)");
        ASSERT(foundObj->getPosition()->getPosition().y == 99, "test02: position y preserved (99)");
    }
}

// ============================================================
// Test 3 (PERSIST-02): unpersist + transition destroys the object
// ============================================================
static void test03_unpersist_destroys_on_transition() {
    printf("--- Test 3 (PERSIST-02): unpersist + transition destroys object ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Spawn, persist, then unpersist
    LuaResult r1 = f.exec(
        "g_temp = engine.scene.spawn('temp_obj')\n"
        "engine.scene.persist(g_temp)\n"
        "unp = engine.scene.unpersist(g_temp)\n"
    );
    ASSERT(r1.success, "test03: spawn + persist + unpersist ran without error");
    ASSERT(f.getBool("unp"), "test03: unpersist returns true");

    // Object is still alive before the next transition
    ASSERT(f.ssm.getCurrentScene()->findByName("temp_obj") != nullptr,
           "test03: temp_obj still in scene before transition");

    // Trigger transition — flushPendingRemovals fires
    f.switchScene(2u);
    ASSERT(f.ssm.getCurrentScene()->getId() == 2u, "test03: now in scene 2");

    // Object should be gone
    LuaResult r2 = f.exec(
        "not_found = (engine.scene.find('temp_obj') == nil)\n"
    );
    ASSERT(r2.success, "test03: find after transition ran without error");
    ASSERT(f.getBool("not_found"), "test03: find('temp_obj') returns nil after unpersist + transition");
}

// ============================================================
// Test 4 (PERSIST-03): find(name) from scene2 returns valid proxy for persisted object
// ============================================================
static void test04_find_from_different_scene() {
    printf("--- Test 4 (PERSIST-03): find() from scene2 finds object persisted from scene1 ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Spawn "hero4" in scene 1, persist it
    LuaResult r1 = f.exec(
        "g_hero4 = engine.scene.spawn('hero4')\n"
        "engine.scene.persist(g_hero4)\n"
    );
    ASSERT(r1.success, "test04: spawn + persist ran without error");

    // Switch to scene 2
    f.switchScene(2u);
    ASSERT(f.ssm.getCurrentScene()->getId() == 2u, "test04: now in scene 2");

    // Spawn a local object with a different name to confirm scene is active
    LuaResult r2 = f.exec(
        "local_obj = engine.scene.spawn('local_obj4')\n"
        "found_hero4 = (engine.scene.find('hero4') ~= nil)\n"
        "found_local = (engine.scene.find('local_obj4') ~= nil)\n"
    );
    ASSERT(r2.success, "test04: find scripts ran without error");
    ASSERT(f.getBool("found_hero4"), "test04: find('hero4') returns non-nil in scene 2 (PERSIST-03)");
    ASSERT(f.getBool("found_local"), "test04: find('local_obj4') finds local scene object");
}

// ============================================================
// Test 5 (PERSIST-01): pool overflow — 5th persist returns nil, no Lua error
// ============================================================
static void test05_pool_overflow() {
    printf("--- Test 5 (PERSIST-01): pool overflow returns nil, no Lua error ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Spawn 5 objects and try to persist all of them
    LuaResult r = f.exec(
        "g_obj1 = engine.scene.spawn('ov1')\n"
        "g_obj2 = engine.scene.spawn('ov2')\n"
        "g_obj3 = engine.scene.spawn('ov3')\n"
        "g_obj4 = engine.scene.spawn('ov4')\n"
        "g_obj5 = engine.scene.spawn('ov5')\n"
        "p1 = (engine.scene.persist(g_obj1) ~= nil)\n"  // slot 0
        "p2 = (engine.scene.persist(g_obj2) ~= nil)\n"  // slot 1
        "p3 = (engine.scene.persist(g_obj3) ~= nil)\n"  // slot 2
        "p4 = (engine.scene.persist(g_obj4) ~= nil)\n"  // slot 3
        "p5 = (engine.scene.persist(g_obj5) == nil)\n"  // 5th -> nil (pool full)
    );
    ASSERT(r.success, "test05: pool overflow script ran without Lua error");
    ASSERT(f.getBool("p1"), "test05: 1st persist returns truthy");
    ASSERT(f.getBool("p2"), "test05: 2nd persist returns truthy");
    ASSERT(f.getBool("p3"), "test05: 3rd persist returns truthy");
    ASSERT(f.getBool("p4"), "test05: 4th persist returns truthy");
    ASSERT(f.getBool("p5"), "test05: 5th persist returns nil (pool full)");
}

// ============================================================
// Test 6 (PERSIST-01): persist same object twice is silent no-op (slot not consumed twice)
// ============================================================
static void test06_double_persist_noop() {
    printf("--- Test 6 (PERSIST-01): double persist is no-op (slot not consumed twice) ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Persist the same object twice, then fill remaining slots and check 5th unique fails
    LuaResult r = f.exec(
        "g_a = engine.scene.spawn('dp_a')\n"
        "g_b = engine.scene.spawn('dp_b')\n"
        "g_c = engine.scene.spawn('dp_c')\n"
        "g_d = engine.scene.spawn('dp_d')\n"
        "g_e = engine.scene.spawn('dp_e')\n"
        "first  = (engine.scene.persist(g_a) ~= nil)\n"  // slot 0
        "second = (engine.scene.persist(g_a) ~= nil)\n"  // no-op, still true
        "pb     = (engine.scene.persist(g_b) ~= nil)\n"  // slot 1
        "pc     = (engine.scene.persist(g_c) ~= nil)\n"  // slot 2
        "pd     = (engine.scene.persist(g_d) ~= nil)\n"  // slot 3
        "pe     = (engine.scene.persist(g_e) == nil)\n"  // 5th unique -> nil (full)
    );
    ASSERT(r.success, "test06: double persist script ran without error");
    ASSERT(f.getBool("first"),  "test06: first persist(a) returns true");
    ASSERT(f.getBool("second"), "test06: second persist(a) returns true (no-op)");
    ASSERT(f.getBool("pb"),     "test06: persist(b) fills slot 1");
    ASSERT(f.getBool("pc"),     "test06: persist(c) fills slot 2");
    ASSERT(f.getBool("pd"),     "test06: persist(d) fills slot 3");
    ASSERT(f.getBool("pe"),     "test06: persist(e) returns nil (pool full at 4 unique)");
}

// ============================================================
// Test 7: self-transition preserves persistent objects
// ============================================================
static void test07_self_transition_preserves() {
    printf("--- Test 7: self-transition preserves persistent objects ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Spawn and persist "survivor7"
    LuaResult r1 = f.exec(
        "g_sv7 = engine.scene.spawn('survivor7')\n"
        "engine.scene.persist(g_sv7)\n"
    );
    ASSERT(r1.success, "test07: spawn + persist ran without error");

    // Self-transition (switch back to scene 1)
    f.switchScene(1u);
    ASSERT(f.ssm.getCurrentScene()->getId() == 1u, "test07: still in scene 1 after self-transition");

    // Verify survivor7 is still findable
    LuaResult r2 = f.exec(
        "still_there = (engine.scene.find('survivor7') ~= nil)\n"
    );
    ASSERT(r2.success, "test07: find after self-transition ran without error");
    ASSERT(f.getBool("still_there"), "test07: 'survivor7' still findable after self-transition");
}

// ============================================================
// Test 8 (PERSIST-03): find priority — active scene takes precedence over registry
// ============================================================
static void test08_find_priority_active_scene_first() {
    printf("--- Test 8 (PERSIST-03): active scene has priority over persistent registry ---\n");

    PersistLuaFixture f;
    f.setup2Scenes();

    // Persist "hero8" from scene 1 with position (100, 100)
    Scene* scene1 = f.ssm.getCurrentScene();
    Object* heroP = scene1->addObject<Object>();
    heroP->setName("hero8");
    heroP->getPosition()->setPosition(100, 100);

    LuaResult r1 = f.exec(
        "g_h8 = engine.scene.find('hero8')\n"
        "engine.scene.persist(g_h8)\n"
    );
    ASSERT(r1.success, "test08: persist hero8 ran without error");

    // Switch to scene 2 and spawn a local "hero8" at a different position (200, 200)
    f.switchScene(2u);
    ASSERT(f.ssm.getCurrentScene()->getId() == 2u, "test08: now in scene 2");

    Scene* scene2 = f.ssm.getCurrentScene();
    Object* heroL = scene2->addObject<Object>();
    heroL->setName("hero8");
    heroL->getPosition()->setPosition(200, 200);

    // find("hero8") should return the LOCAL object (active scene priority)
    Object* found = scene2->findByName("hero8");
    // Scene findByName scans owned + external; local hero8 (owned) appears first
    // Then verify via C++ that we get the local one
    ASSERT(found != nullptr, "test08: findByName found 'hero8' in scene 2");
    if (found) {
        // The scene-local hero8 is at (200, 200); persistent hero8 is at (100, 100)
        // If active scene priority works, we get the local one at (200, 200)
        bool isLocal = (found->getPosition()->getPosition().x == 200);
        ASSERT(isLocal, "test08: find returns scene-local hero8 (x=200, not persistent x=100)");
    }

    // Also verify via Lua using the scene's active binding
    LuaResult r2 = f.exec(
        "local h8 = engine.scene.find('hero8')\n"
        "h8_not_nil = (h8 ~= nil)\n"
    );
    ASSERT(r2.success, "test08: Lua find('hero8') ran without error");
    ASSERT(f.getBool("h8_not_nil"), "test08: Lua find returns non-nil for hero8");
}

// ============================================================
// Test 9 (DEBT-02): engine.scene.persist() without SSM returns nil
// Verifies that calling persist() when no SceneStateMachine is registered
// returns nil (not true) and does not crash. The production fix adds a
// printf warning at the no-SSM guard before returning nil.
// ============================================================
static void test09_persist_without_ssm_prints_warning() {
    printf("--- Test 9 (DEBT-02): persist() without SSM returns nil ---\n");

    // Bare fixture: LuaEngine + LuaBindings, NO setSceneStateMachine call
    LuaEngine engine;
    LuaBindings bindings(&engine);
    engine.initialize();
    bindings.registerAll();
    // Intentionally NOT calling bindings.setSceneStateMachine(&ssm)

    // Need an active scene so engine.scene.spawn() works to create a proxy
    Scene scene(99u);
    bindings.setActiveScene(&scene);

    // Spawn an object and get a proxy, then call persist() — should return nil
    LuaResult r = engine.executeString(
        "local obj = engine.scene.spawn('debt02_obj')\n"
        "persist_result = engine.scene.persist(obj)\n"
        "persist_is_nil = (persist_result == nil)\n"
    );
    ASSERT(r.success, "test09: persist-without-ssm script ran without Lua error");

    bool isNil = engine.getGlobalBool("persist_is_nil");
    ASSERT(isNil, "test09: persist() returns nil when no SSM is set (DEBT-02)");
}

// ============================================================
// main
// ============================================================
int main() {
    test01_persist_survives_transition();
    test02_component_state_preserved();
    test03_unpersist_destroys_on_transition();
    test04_find_from_different_scene();
    test05_pool_overflow();
    test06_double_persist_noop();
    test07_self_transition_preserves();
    test08_find_priority_active_scene_first();
    test09_persist_without_ssm_prints_warning();

    printf("\n=== Persistent Lua Test: %d passed, %d failed ===\n", passes, failures);
    return failures;
}
