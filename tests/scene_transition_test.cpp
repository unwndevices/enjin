#include <enjin2/core/scene_state_machine.hpp>
#include <cstdio>
using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

struct TestScene : Scene {
    explicit TestScene(uint32_t id) : Scene(id) {}
    int createCount = 0;
    int activateCount = 0;
    int deactivateCount = 0;
    bool selfSwitchOnNextUpdate = false;

    void onCreate()     override { createCount++; }
    void onActivate()   override { activateCount++; }
    void onDeactivate() override { deactivateCount++; }
    void onUpdate(float /*dt*/) override {
        if (selfSwitchOnNextUpdate) {
            selfSwitchOnNextUpdate = false;
            m_ssm->switchTo(sceneId);  // self-transition: uses m_ssm and sceneId
        }
    }

    // Expose m_ssm for test assertions (m_ssm is protected)
    SceneStateMachine* getSSM() const { return m_ssm; }
};

// SCENE-01: SSM back-pointer injection
static void test_scene01_ssm_pointer() {
    SceneStateMachine ssm;
    TestScene* scene = ssm.addScene<TestScene>(1u);
    ssm.changeScene(1u);
    ssm.update(0.016f);
    ASSERT(scene->getSSM() != nullptr, "SCENE-01: m_ssm injected and non-null after activate");
}

// SCENE-02: Deferred cross-scene transition from onUpdate()
static void test_scene02_deferred_cross_transition() {
    SceneStateMachine ssm;
    ssm.addScene<TestScene>(1u);
    ssm.addScene<TestScene>(2u);
    ssm.changeScene(1u);
    ssm.update(0.016f);  // settle

    TestScene* s1 = static_cast<TestScene*>(ssm.getScene(1u));
    TestScene* s2 = static_cast<TestScene*>(ssm.getScene(2u));

    // Request cross-scene transition directly (not from update)
    ssm.switchTo(2u);
    ssm.update(0.016f);

    ASSERT(ssm.getCurrentScene()->getId() == 2u, "SCENE-02: currentScene is scene 2 after deferred transition");
    ASSERT(s1->deactivateCount == 1, "SCENE-02: s1 was deactivated after transition");
    ASSERT(s2->activateCount == 1, "SCENE-02: s2 was activated after transition");
}

// SCENE-03: Self-transition full reset (via onUpdate)
static void test_scene03_self_transition() {
    SceneStateMachine ssm;
    ssm.addScene<TestScene>(1u);
    ssm.changeScene(1u);
    ssm.update(0.016f);  // settle

    TestScene* s = static_cast<TestScene*>(ssm.getScene(1u));

    // Verify baseline
    ASSERT(s->createCount == 1,     "SCENE-03: baseline createCount == 1");
    ASSERT(s->activateCount == 1,   "SCENE-03: baseline activateCount == 1");
    ASSERT(s->deactivateCount == 0, "SCENE-03: baseline deactivateCount == 0");

    // Trigger self-switch from onUpdate
    s->selfSwitchOnNextUpdate = true;
    ssm.update(0.016f);

    ASSERT(s->deactivateCount == 1, "SCENE-03: deactivateCount == 1 after self-transition");
    ASSERT(s->createCount == 2,     "SCENE-03: createCount == 2 after self-transition (onCreate fired again)");
    ASSERT(s->activateCount == 2,   "SCENE-03: activateCount == 2 after self-transition");
    ASSERT(ssm.getCurrentScene() == s, "SCENE-03: getCurrentScene() still returns same scene after self-reset");
}

// SCENE-03 variant: Self-transition via direct switchTo call
static void test_scene03_self_transition_via_direct_switch() {
    SceneStateMachine ssm;
    ssm.addScene<TestScene>(1u);
    ssm.changeScene(1u);
    ssm.update(0.016f);

    TestScene* s = static_cast<TestScene*>(ssm.getScene(1u));

    // Baseline
    ASSERT(s->createCount == 1,     "SCENE-03b: baseline createCount == 1");
    ASSERT(s->activateCount == 1,   "SCENE-03b: baseline activateCount == 1");
    ASSERT(s->deactivateCount == 0, "SCENE-03b: baseline deactivateCount == 0");

    // Direct switchTo(selfId) — NOT from onUpdate
    ssm.switchTo(1u);
    ssm.update(0.016f);

    ASSERT(s->createCount == 2,     "SCENE-03b: createCount == 2 after direct switchTo(selfId)");
    ASSERT(s->activateCount == 2,   "SCENE-03b: activateCount == 2 after direct switchTo(selfId)");
    ASSERT(s->deactivateCount == 1, "SCENE-03b: deactivateCount == 1 after direct switchTo(selfId)");
}

int main() {
    test_scene01_ssm_pointer();
    test_scene02_deferred_cross_transition();
    test_scene03_self_transition();
    test_scene03_self_transition_via_direct_switch();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
