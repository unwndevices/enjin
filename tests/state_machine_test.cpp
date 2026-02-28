/**
 * @file state_machine_test.cpp
 * @brief Tests for C_StateMachine component (Phase 41: FSM-01..FSM-05)
 *
 * Tests:
 *   FSM-01: fsm:addState(name, {enter, exit, update}) registers a named state
 *   FSM-02: fsm:setState(name) fires exit(old) then enter(new) — deferred to end of update
 *   FSM-02b: Self-transition (setState to same state) triggers full exit + enter cycle
 *   FSM-03: fsm:getState() returns active state name (empty string when no state)
 *   FSM-04: setState() from inside update/enter callback takes effect on the NEXT frame
 *   FSM-05: Active state's update(self, dt) is called each frame
 *   FSM-05b: clearStates() releases all luaL_ref handles on hot-reload
 *   FSM-05c: Object destruction does not crash (clearStates via C_LuaScript destructor)
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/state_machine.hpp>
#include <enjin2/components/position.hpp>
#include <cstdio>
#include <cstring>
#include <memory>

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

// ============================================================
// FSM-01: fsm:addState registers a named state
// ============================================================
static void test_fsm01_addState() {
    printf("--- FSM-01: fsm:addState registers a named state ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_StateMachine* fsmComp = obj->addComponent<C_StateMachine>();

    ASSERT(script != nullptr, "FSM-01: addComponent<C_LuaScript> should succeed");
    ASSERT(fsmComp != nullptr, "FSM-01: addComponent<C_StateMachine> should succeed");

    // Before any script runs, no state active
    ASSERT(fsmComp->getActiveStateCount() == 0, "FSM-01: no states before script");

    bool loaded = script->loadScript(
        "state_added = false\n"
        "function init(self)\n"
        "    local fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('idle', {\n"
        "            enter  = function(self) end,\n"
        "            update = function(self, dt) end,\n"
        "            exit   = function(self) end,\n"
        "        })\n"
        "        state_added = true\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-01: script loaded");
    ASSERT(!script->hasErrors(), "FSM-01: no script errors after load");

    // First update triggers init(self), then C_StateMachine::update runs
    obj->update(0.0f);
    ASSERT(!script->hasErrors(), "FSM-01: no script errors after first update");

    bool stateAdded = script->getScriptBool("state_added", false);
    ASSERT(stateAdded, "FSM-01: state_added should be true after init");

    ASSERT(fsmComp->getActiveStateCount() == 1, "FSM-01: getActiveStateCount should be 1");

    delete obj;
}

// ============================================================
// FSM-02: fsm:setState triggers enter/exit callbacks
// ============================================================
static void test_fsm02_setState_enter_exit() {
    printf("--- FSM-02: fsm:setState fires enter/exit callbacks ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_StateMachine>();

    bool loaded = script->loadScript(
        "entered_idle = false\n"
        "exited_idle = false\n"
        "entered_running = false\n"
        "switched = false\n"
        "local fsm = nil\n"
        "function init(self)\n"
        "    fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('idle', {\n"
        "            enter  = function(self) entered_idle = true end,\n"
        "            exit   = function(self) exited_idle = true end,\n"
        "            update = function(self, dt)\n"
        "                if not switched then\n"
        "                    switched = true\n"
        "                    fsm:setState('running')\n"
        "                end\n"
        "            end,\n"
        "        })\n"
        "        fsm:addState('running', {\n"
        "            enter = function(self) entered_running = true end,\n"
        "        })\n"
        "        fsm:setState('idle')\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-02: script loaded");

    // Frame 1: init() called (setState('idle') queued), then C_StateMachine::update
    //   applies deferred enter for 'idle'
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "FSM-02: no script errors after frame 1");
    bool enteredIdle1 = script->getScriptBool("entered_idle", false);
    ASSERT(enteredIdle1, "FSM-02: entered_idle should be true after frame 1");
    bool exitedIdle1 = script->getScriptBool("exited_idle", false);
    ASSERT(!exitedIdle1, "FSM-02: exited_idle should be false after frame 1");
    bool enteredRunning1 = script->getScriptBool("entered_running", false);
    ASSERT(!enteredRunning1, "FSM-02: entered_running should be false after frame 1");

    // Frame 2: idle update() fires (sets switched, calls setState('running')),
    //   then deferred transition applies: exit(idle), enter(running)
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "FSM-02: no script errors after frame 2");
    bool exitedIdle2 = script->getScriptBool("exited_idle", false);
    ASSERT(exitedIdle2, "FSM-02: exited_idle should be true after frame 2");
    bool enteredRunning2 = script->getScriptBool("entered_running", false);
    ASSERT(enteredRunning2, "FSM-02: entered_running should be true after frame 2");

    delete obj;
}

// ============================================================
// FSM-02b: Self-transition triggers full exit + enter cycle
// ============================================================
static void test_fsm02b_self_transition() {
    printf("--- FSM-02b: Self-transition triggers full exit + enter ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_StateMachine>();

    bool loaded = script->loadScript(
        "enter_count = 0\n"
        "exit_count = 0\n"
        "local fsm = nil\n"
        "function init(self)\n"
        "    fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('idle', {\n"
        "            enter  = function(self) enter_count = enter_count + 1 end,\n"
        "            exit   = function(self) exit_count = exit_count + 1 end,\n"
        "            update = function(self, dt)\n"
        "                if enter_count == 1 then\n"
        "                    fsm:setState('idle')  -- self-transition\n"
        "                end\n"
        "            end,\n"
        "        })\n"
        "        fsm:setState('idle')\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-02b: script loaded");

    // Frame 1: init sets pending('idle'), C_StateMachine::update applies enter — enter_count=1
    obj->update(0.016f);
    double ec1 = script->getScriptNumber("enter_count", 0.0);
    double xc1 = script->getScriptNumber("exit_count", 0.0);
    ASSERT(static_cast<int>(ec1) == 1, "FSM-02b: enter_count should be 1 after frame 1");
    ASSERT(static_cast<int>(xc1) == 0, "FSM-02b: exit_count should be 0 after frame 1");

    // Frame 2: idle update fires (enter_count==1, so queues setState('idle')),
    //   then deferred transition: exit fires (exit_count=1), enter fires (enter_count=2)
    obj->update(0.016f);
    double ec2 = script->getScriptNumber("enter_count", 0.0);
    double xc2 = script->getScriptNumber("exit_count", 0.0);
    ASSERT(static_cast<int>(ec2) == 2, "FSM-02b: enter_count should be 2 after self-transition");
    ASSERT(static_cast<int>(xc2) == 1, "FSM-02b: exit_count should be 1 after self-transition");

    delete obj;
}

// ============================================================
// FSM-03: fsm:getState() returns current state name
// ============================================================
static void test_fsm03_getState() {
    printf("--- FSM-03: fsm:getState returns current state name ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_StateMachine* fsmComp = obj->addComponent<C_StateMachine>();

    // Before any state: C++ side returns empty string
    ASSERT(strcmp(fsmComp->getState(), "") == 0, "FSM-03: getState() returns empty string before any state");

    bool loaded = script->loadScript(
        "current = ''\n"
        "local fsm = nil\n"
        "function init(self)\n"
        "    fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('idle', {\n"
        "            enter = function(self) current = fsm:getState() end,\n"
        "        })\n"
        "        fsm:setState('idle')\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-03: script loaded");

    // Frame 1: init() queues setState('idle'), C_StateMachine::update applies it
    //   enter() fires, which calls fsm:getState() — should return "idle"
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "FSM-03: no script errors after frame 1");
    std::string current = script->getScriptString("current", "");
    ASSERT(current == "idle", "FSM-03: getState() should return 'idle' inside enter callback");

    // C++ side should also reflect the correct state
    ASSERT(strcmp(fsmComp->getState(), "idle") == 0, "FSM-03: C++ getState() returns 'idle'");

    delete obj;
}

// ============================================================
// FSM-04: setState() from update callback takes effect next frame (deferred)
// ============================================================
static void test_fsm04_deferred_transition() {
    printf("--- FSM-04: setState from update is deferred to next frame ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_StateMachine>();

    bool loaded = script->loadScript(
        "update_ran_idle = false\n"
        "update_ran_running = false\n"
        "state_during_idle_update = ''\n"
        "local fsm = nil\n"
        "function init(self)\n"
        "    fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('idle', {\n"
        "            update = function(self, dt)\n"
        "                update_ran_idle = true\n"
        "                fsm:setState('running')\n"
        "                state_during_idle_update = fsm:getState()\n"
        "            end,\n"
        "        })\n"
        "        fsm:addState('running', {\n"
        "            update = function(self, dt)\n"
        "                update_ran_running = true\n"
        "            end,\n"
        "        })\n"
        "        fsm:setState('idle')\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-04: script loaded");

    // Frame 1: init() queues setState('idle'), C_StateMachine::update applies enter(idle)
    //   (no enter callback defined — just becomes 'idle')
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "FSM-04: no script errors after frame 1");
    bool idleRan1 = script->getScriptBool("update_ran_idle", false);
    ASSERT(!idleRan1, "FSM-04: idle update should NOT have run in frame 1 (still entering)");

    // Frame 2: C_StateMachine::update fires idle's update callback
    //   (calls setState('running'), but state still 'idle' during this callback),
    //   then deferred transition applies (current becomes 'running')
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "FSM-04: no script errors after frame 2");
    bool idleRan2 = script->getScriptBool("update_ran_idle", false);
    ASSERT(idleRan2, "FSM-04: idle update should have run in frame 2");
    std::string stateDuringIdle = script->getScriptString("state_during_idle_update", "");
    ASSERT(stateDuringIdle == "idle", "FSM-04: state should still be 'idle' during idle update callback (deferred)");

    // Frame 3: running's update callback should fire
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "FSM-04: no script errors after frame 3");
    bool runningRan3 = script->getScriptBool("update_ran_running", false);
    ASSERT(runningRan3, "FSM-04: running update should have fired in frame 3");

    delete obj;
}

// ============================================================
// FSM-05: Active state's update(self, dt) is called each frame
// ============================================================
static void test_fsm05_update_each_frame() {
    printf("--- FSM-05: state update(self, dt) called each frame ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_StateMachine>();

    bool loaded = script->loadScript(
        "update_count = 0\n"
        "total_dt = 0.0\n"
        "local fsm = nil\n"
        "function init(self)\n"
        "    fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('active', {\n"
        "            update = function(self, dt)\n"
        "                update_count = update_count + 1\n"
        "                total_dt = total_dt + dt\n"
        "            end,\n"
        "        })\n"
        "        fsm:setState('active')\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-05: script loaded");

    // Frame 1: init queues setState('active'), C_StateMachine::update applies it (enter fires)
    //   No state update in frame 1 — transition just applied
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "FSM-05: no errors after frame 1");
    double count1 = script->getScriptNumber("update_count", 0.0);
    ASSERT(static_cast<int>(count1) == 0, "FSM-05: update_count should be 0 after frame 1 (only enter applied)");

    // Frames 2, 3, 4 with dt=0.1
    obj->update(0.1f);
    obj->update(0.1f);
    obj->update(0.1f);
    ASSERT(!script->hasErrors(), "FSM-05: no errors after frames 2-4");

    double count4 = script->getScriptNumber("update_count", 0.0);
    ASSERT(static_cast<int>(count4) == 3, "FSM-05: update_count should be 3 after 3 active frames");

    double totalDt = script->getScriptNumber("total_dt", 0.0);
    // Allow small floating point tolerance
    ASSERT(totalDt > 0.29 && totalDt < 0.31, "FSM-05: total_dt should be ~0.3 (3 * 0.1)");

    delete obj;
}

// ============================================================
// FSM-05b: clearStates() releases all luaL_ref handles on hot-reload
// ============================================================
static void test_fsm05b_clear_on_reload() {
    printf("--- FSM-05b: clearStates on hot-reload releases refs ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_StateMachine* fsmComp = obj->addComponent<C_StateMachine>();

    ASSERT(fsmComp != nullptr, "FSM-05b: C_StateMachine created");

    bool loaded = script->loadScript(
        "local fsm = nil\n"
        "function init(self)\n"
        "    fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('idle', {\n"
        "            enter  = function(self) end,\n"
        "            exit   = function(self) end,\n"
        "            update = function(self, dt) end,\n"
        "        })\n"
        "        fsm:addState('running', {\n"
        "            enter  = function(self) end,\n"
        "            update = function(self, dt) end,\n"
        "        })\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-05b: initial script loaded");

    // After frame 1, both states should be active
    obj->update(0.016f);
    int activeCount = fsmComp->getActiveStateCount();
    ASSERT(activeCount == 2, "FSM-05b: two states should be active after init");

    // Hot-reload: loadScript() calls executeScript() which calls clearStates()
    bool reloaded = script->loadScript(
        "function init(self)\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(reloaded, "FSM-05b: hot-reload script loaded");

    // After reload, clearStates() should have been called
    int activeAfter = fsmComp->getActiveStateCount();
    ASSERT(activeAfter == 0, "FSM-05b: all states should be cleared after hot-reload");

    // All state entry refs should be LUA_NOREF
    bool allCleared = true;
    for (int i = 0; i < C_StateMachine::MAX_STATES; ++i) {
        const C_StateMachine::StateEntry& e = fsmComp->getStateEntry(i);
        if (e.enterRef != LUA_NOREF || e.exitRef != LUA_NOREF || e.updateRef != LUA_NOREF) {
            allCleared = false;
            break;
        }
    }
    ASSERT(allCleared, "FSM-05b: all StateEntry refs should be LUA_NOREF after clearStates");

    delete obj;
}

// ============================================================
// FSM-05c: Object destruction does not crash (clearStates via destructor)
// ============================================================
static void test_fsm05c_clear_on_destroy() {
    printf("--- FSM-05c: clearStates via C_LuaScript destructor ---\n");

    // This test succeeds if it doesn't crash or trigger ASAN errors.
    // C_LuaScript::~C_LuaScript() must call C_StateMachine::clearStates() before
    // scriptSystem->shutdown() to prevent use-after-free of Lua state.
    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_StateMachine* fsmComp = obj->addComponent<C_StateMachine>();

    ASSERT(script != nullptr, "FSM-05c: script component created");
    ASSERT(fsmComp != nullptr, "FSM-05c: FSM component created");

    bool loaded = script->loadScript(
        "local fsm = nil\n"
        "function init(self)\n"
        "    fsm = self:get('C_StateMachine')\n"
        "    if fsm then\n"
        "        fsm:addState('idle', {\n"
        "            enter  = function(self) end,\n"
        "            exit   = function(self) end,\n"
        "            update = function(self, dt) end,\n"
        "        })\n"
        "        fsm:setState('idle')\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "FSM-05c: script loaded");

    obj->update(0.016f);  // init + enter applied
    int activeCount = fsmComp->getActiveStateCount();
    ASSERT(activeCount >= 1, "FSM-05c: at least one state active before destruction");

    // Delete Object — this destructs C_LuaScript (which calls C_StateMachine::clearStates()
    // before shutdown()) and then C_StateMachine. Must not crash.
    delete obj;

    // If we reach here without crash, the test passes.
    ASSERT(true, "FSM-05c: Object deleted without crash");
}

// ============================================================
// Main
// ============================================================
int main() {
    test_fsm01_addState();
    test_fsm02_setState_enter_exit();
    test_fsm02b_self_transition();
    test_fsm03_getState();
    test_fsm04_deferred_transition();
    test_fsm05_update_each_frame();
    test_fsm05b_clear_on_reload();
    test_fsm05c_clear_on_destroy();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
