#include <enjin2/core/scene_state_machine.hpp>
#include <enjin2/core/object_collection.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/scripting/object_proxy.hpp>
#include <cstdio>
#include <cstring>
using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

// Minimal scene for SSM tests — tracks lifecycle counts
struct MinimalScene : Scene {
    explicit MinimalScene(uint32_t id) : Scene(id) {}
    int createCount = 0;
    int activateCount = 0;
    int deactivateCount = 0;
    void onCreate()     override { createCount++; }
    void onActivate()   override { activateCount++; }
    void onDeactivate() override { deactivateCount++; }
};

// -------------------------------------------------------------------------
// PERSIST-01: extractObject returns unique_ptr without destroying object
// -------------------------------------------------------------------------
static void test_persist01_extract_without_destroy() {
    printf("--- PERSIST-01: extractObject without destroy ---\n");

    ObjectCollection col;
    Object* raw = col.addObject<Object>();
    raw->setName("hero");

    // Attach a proxy so we can verify it is NOT invalidated after extraction
    ObjectProxy proxy{raw, true};
    raw->setLuaProxy(&proxy);

    std::unique_ptr<Object> extracted = col.extractObject(raw);

    ASSERT(extracted != nullptr,        "PERSIST-01: extractObject returns non-null unique_ptr");
    ASSERT(extracted.get() == raw,      "PERSIST-01: extracted ptr is the same object");
    ASSERT(col.size() == 0,             "PERSIST-01: collection size is 0 after extraction");
    ASSERT(proxy.valid == true,         "PERSIST-01: proxy still valid (object not destroyed)");
    ASSERT(col.findByName("hero") == nullptr, "PERSIST-01: findByName returns nullptr after extraction");
}

// -------------------------------------------------------------------------
// PERSIST-02: PersistentObjectRegistry::add — 4 objects succeed, 5th returns false
// -------------------------------------------------------------------------
static void test_persist02_registry_overflow() {
    printf("--- PERSIST-02: registry overflow at MAX_PERSISTENT=4 ---\n");

    SceneStateMachine::PersistentObjectRegistry reg;

    bool ok1 = reg.add(std::unique_ptr<Object>(new Object()));
    bool ok2 = reg.add(std::unique_ptr<Object>(new Object()));
    bool ok3 = reg.add(std::unique_ptr<Object>(new Object()));
    bool ok4 = reg.add(std::unique_ptr<Object>(new Object()));
    bool ok5 = reg.add(std::unique_ptr<Object>(new Object()));  // should fail

    ASSERT(ok1, "PERSIST-02: first add succeeds");
    ASSERT(ok2, "PERSIST-02: second add succeeds");
    ASSERT(ok3, "PERSIST-02: third add succeeds");
    ASSERT(ok4, "PERSIST-02: fourth add succeeds");
    ASSERT(!ok5, "PERSIST-02: fifth add returns false (registry full)");
}

// -------------------------------------------------------------------------
// PERSIST-03: PersistentObjectRegistry::findByName returns correct object
// -------------------------------------------------------------------------
static void test_persist03_find_by_name() {
    printf("--- PERSIST-03: findByName returns correct object ---\n");

    SceneStateMachine::PersistentObjectRegistry reg;

    auto obj = std::unique_ptr<Object>(new Object());
    obj->setName("player");
    Object* rawPtr = obj.get();
    reg.add(std::move(obj));

    Object* found = reg.findByName("player");
    ASSERT(found == rawPtr, "PERSIST-03: findByName returns correct object");
    ASSERT(reg.findByName("enemy") == nullptr, "PERSIST-03: findByName returns nullptr for unknown name");
    ASSERT(reg.findByName(nullptr) == nullptr, "PERSIST-03: findByName nullptr-safe");
}

// -------------------------------------------------------------------------
// PERSIST-04: findByName skips pendingRemoval objects
// -------------------------------------------------------------------------
static void test_persist04_find_skips_pending_removal() {
    printf("--- PERSIST-04: findByName skips pendingRemoval objects ---\n");

    SceneStateMachine::PersistentObjectRegistry reg;

    auto obj = std::unique_ptr<Object>(new Object());
    obj->setName("ghost");
    Object* rawPtr = obj.get();
    reg.add(std::move(obj));

    reg.markForRemoval(rawPtr);
    Object* found = reg.findByName("ghost");

    ASSERT(found == nullptr, "PERSIST-04: findByName returns nullptr for pendingRemoval object");
}

// -------------------------------------------------------------------------
// PERSIST-05: flushPendingRemovals destroys marked objects (proxy->valid = false)
// -------------------------------------------------------------------------
static void test_persist05_flush_destroys_marked() {
    printf("--- PERSIST-05: flushPendingRemovals invalidates proxy ---\n");

    SceneStateMachine::PersistentObjectRegistry reg;

    auto obj = std::unique_ptr<Object>(new Object());
    ObjectProxy proxy{obj.get(), true};
    obj->setLuaProxy(&proxy);
    Object* rawPtr = obj.get();
    reg.add(std::move(obj));

    reg.markForRemoval(rawPtr);

    ASSERT(proxy.valid == true, "PERSIST-05: proxy valid before flush");
    reg.flushPendingRemovals();
    ASSERT(proxy.valid == false, "PERSIST-05: proxy invalid after flush (object destroyed)");
}

// -------------------------------------------------------------------------
// PERSIST-06: persistObject extracts from scene and adds to registry
// -------------------------------------------------------------------------
static void test_persist06_persist_object_extracts_from_scene() {
    printf("--- PERSIST-06: persistObject extracts from current scene ---\n");

    SceneStateMachine ssm;
    ssm.addScene<MinimalScene>(1u);
    ssm.changeScene(1u);
    ssm.update(0.016f);  // settle

    Scene* scene = ssm.getCurrentScene();
    Object* hero = scene->addObject<Object>();
    hero->setName("hero");

    ASSERT(scene->findByName("hero") != nullptr, "PERSIST-06: hero found in scene before persist");

    bool persisted = ssm.persistObject(hero);

    ASSERT(persisted, "PERSIST-06: persistObject returns true");
    // Hero should now be an external — still findable via scene->findByName
    ASSERT(scene->findByName("hero") != nullptr, "PERSIST-06: hero still findable after persist (injected as external)");
    // SSM findPersistentByName should find it
    ASSERT(ssm.findPersistentByName("hero") == hero, "PERSIST-06: findPersistentByName returns hero");
}

// -------------------------------------------------------------------------
// PERSIST-07: Cross-scene transition — persistent object appears in new scene's findByName
// -------------------------------------------------------------------------
static void test_persist07_cross_scene_persistence() {
    printf("--- PERSIST-07: cross-scene transition preserves persistent object ---\n");

    SceneStateMachine ssm;
    ssm.addScene<MinimalScene>(1u);
    ssm.addScene<MinimalScene>(2u);
    ssm.changeScene(1u);
    ssm.update(0.016f);  // settle in scene 1

    // Add an object to scene 1 and persist it
    Scene* scene1 = ssm.getCurrentScene();
    Object* obj = scene1->addObject<Object>();
    obj->setName("persistent_hero");

    bool persisted = ssm.persistObject(obj);
    ASSERT(persisted, "PERSIST-07: persistObject succeeds");

    // Transition to scene 2
    ssm.switchTo(2u);
    ssm.update(0.016f);

    ASSERT(ssm.getCurrentScene()->getId() == 2u, "PERSIST-07: now in scene 2");
    Object* found = ssm.getCurrentScene()->findByName("persistent_hero");
    ASSERT(found == obj, "PERSIST-07: persistent_hero found in scene 2 after cross-scene transition");
    ASSERT(ssm.findPersistentByName("persistent_hero") == obj, "PERSIST-07: findPersistentByName works from scene 2");
}

// -------------------------------------------------------------------------
// PERSIST-08: Self-transition — persistent object survives reset cycle
// -------------------------------------------------------------------------
static void test_persist08_self_transition_persistence() {
    printf("--- PERSIST-08: self-transition preserves persistent object ---\n");

    SceneStateMachine ssm;
    ssm.addScene<MinimalScene>(1u);
    ssm.changeScene(1u);
    ssm.update(0.016f);

    Scene* scene = ssm.getCurrentScene();
    Object* obj = scene->addObject<Object>();
    obj->setName("survivor");

    ssm.persistObject(obj);

    // Self-transition
    ssm.switchTo(1u);
    ssm.update(0.016f);

    // Scene should have reset (create/activate called again)
    MinimalScene* ms = static_cast<MinimalScene*>(ssm.getCurrentScene());
    ASSERT(ms->createCount == 2, "PERSIST-08: scene was re-created (self-transition)");

    // Persistent object should still be findable
    Object* found = ssm.getCurrentScene()->findByName("survivor");
    ASSERT(found == obj, "PERSIST-08: persistent object found after self-transition");
    ASSERT(ssm.findPersistentByName("survivor") == obj, "PERSIST-08: findPersistentByName works after self-transition");
}

// -------------------------------------------------------------------------
// PERSIST-09: unpersistObject + transition: object is destroyed on next transition
// -------------------------------------------------------------------------
static void test_persist09_unpersist_destroys_on_transition() {
    printf("--- PERSIST-09: unpersistObject destroys object on next transition ---\n");

    SceneStateMachine ssm;
    ssm.addScene<MinimalScene>(1u);
    ssm.addScene<MinimalScene>(2u);
    ssm.changeScene(1u);
    ssm.update(0.016f);

    Scene* scene1 = ssm.getCurrentScene();
    Object* obj = scene1->addObject<Object>();
    obj->setName("temporary");

    // Attach proxy to detect destruction
    ObjectProxy proxy{obj, true};
    obj->setLuaProxy(&proxy);

    ssm.persistObject(obj);
    ASSERT(proxy.valid == true, "PERSIST-09: proxy valid after persistObject");

    // Mark for removal
    ssm.unpersistObject(obj);

    // Object is still alive until next transition
    ASSERT(proxy.valid == true, "PERSIST-09: proxy still valid before transition");

    // Trigger cross-scene transition
    ssm.switchTo(2u);
    ssm.update(0.016f);

    // flushPendingRemovals should have fired during applyDeferredTransition
    ASSERT(proxy.valid == false, "PERSIST-09: proxy invalid after transition (object destroyed)");
    ASSERT(ssm.findPersistentByName("temporary") == nullptr, "PERSIST-09: findPersistentByName returns nullptr after removal");
}

// -------------------------------------------------------------------------
// PERSIST-10: contains() prevents double-add (second persist returns true, no slot consumed)
// -------------------------------------------------------------------------
static void test_persist10_double_persist_is_noop() {
    printf("--- PERSIST-10: double-persist is silent no-op via contains() ---\n");

    SceneStateMachine ssm;
    ssm.addScene<MinimalScene>(1u);
    ssm.changeScene(1u);
    ssm.update(0.016f);

    Scene* scene = ssm.getCurrentScene();
    Object* obj1 = scene->addObject<Object>(); obj1->setName("obj1");
    Object* obj2 = scene->addObject<Object>(); obj2->setName("obj2");
    Object* obj3 = scene->addObject<Object>(); obj3->setName("obj3");
    Object* obj4 = scene->addObject<Object>(); obj4->setName("obj4");
    Object* obj5 = scene->addObject<Object>(); obj5->setName("obj5");

    // Persist obj1 once — uses slot 0
    bool first = ssm.persistObject(obj1);
    ASSERT(first == true, "PERSIST-10: first persistObject(obj1) returns true");

    // Second call on same object should return true (no-op, slot 0 NOT duplicated)
    bool second = ssm.persistObject(obj1);
    ASSERT(second == true, "PERSIST-10: second persistObject(obj1) returns true (already persistent, no extra slot)");

    // Fill slots 1, 2, 3 with objects 2, 3, 4
    bool p2 = ssm.persistObject(obj2);
    bool p3 = ssm.persistObject(obj3);
    bool p4 = ssm.persistObject(obj4);
    ASSERT(p2, "PERSIST-10: persistObject(obj2) fills slot 1");
    ASSERT(p3, "PERSIST-10: persistObject(obj3) fills slot 2");
    ASSERT(p4, "PERSIST-10: persistObject(obj4) fills slot 3");

    // Registry is now full (4/4). obj5 is a 5th unique object — must fail.
    bool p5 = ssm.persistObject(obj5);
    ASSERT(p5 == false, "PERSIST-10: 5th unique persistObject returns false (registry full at 4)");
}

int main() {
    test_persist01_extract_without_destroy();
    test_persist02_registry_overflow();
    test_persist03_find_by_name();
    test_persist04_find_skips_pending_removal();
    test_persist05_flush_destroys_marked();
    test_persist06_persist_object_extracts_from_scene();
    test_persist07_cross_scene_persistence();
    test_persist08_self_transition_persistence();
    test_persist09_unpersist_destroys_on_transition();
    test_persist10_double_persist_is_noop();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
