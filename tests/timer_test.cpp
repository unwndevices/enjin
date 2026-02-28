/**
 * @file timer_test.cpp
 * @brief Tests for C_Timer component (Phase 40: TIMER-01..TIMER-05)
 *
 * Tests:
 *   TIMER-01: timer:after(seconds, fn) fires fn(self) exactly once after seconds
 *   TIMER-02: timer:every(seconds, fn) fires fn(self) repeatedly at interval
 *   TIMER-03: timer:cancel(id) prevents the cancelled timer from firing
 *   TIMER-03b: cancel is ID-specific — does not cancel other timers
 *   TIMER-04: Timer callbacks receive ScriptProxy as first argument (self ~= nil)
 *   TIMER-05a: clearTimers() releases all luaL_ref handles on hot-reload
 *   TIMER-05b: clearTimers() via C_LuaScript destructor — no crash / ASAN clean
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/timer.hpp>
#include <enjin2/components/position.hpp>
#include <cstdio>
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
// TIMER-01: timer:after(1.0, fn) fires exactly once after >= 1.0s
// ============================================================
static void test_timer01_after_fires_once() {
    printf("--- TIMER-01: timer:after fires exactly once ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_Timer* timerComp = obj->addComponent<C_Timer>();

    ASSERT(script != nullptr, "TIMER-01: addComponent<C_LuaScript> should succeed");
    ASSERT(timerComp != nullptr, "TIMER-01: addComponent<C_Timer> should succeed");

    bool loaded = script->loadScript(
        "fire_count = 0\n"
        "fired = false\n"
        "function init(self)\n"
        "    local t = self:get('C_Timer')\n"
        "    if t then\n"
        "        t:after(1.0, function(self)\n"
        "            fired = true\n"
        "            fire_count = fire_count + 1\n"
        "        end)\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "TIMER-01: script loaded");
    ASSERT(!script->hasErrors(), "TIMER-01: no script errors after load");

    // After 0.5s — should NOT have fired yet
    obj->update(0.5f);
    ASSERT(!script->hasErrors(), "TIMER-01: no script errors after update");
    bool firedEarly = script->getScriptBool("fired", false);
    ASSERT(!firedEarly, "TIMER-01: timer should not fire before 1.0s");

    // After another 0.5s (total 1.0s) — should fire now
    obj->update(0.5f);
    bool firedNow = script->getScriptBool("fired", false);
    ASSERT(firedNow, "TIMER-01: timer should fire at 1.0s");

    double count = script->getScriptNumber("fire_count", 0.0);
    ASSERT(static_cast<int>(count) == 1, "TIMER-01: after() should fire exactly once");

    // One more update — should NOT fire again
    obj->update(0.5f);
    double count2 = script->getScriptNumber("fire_count", 0.0);
    ASSERT(static_cast<int>(count2) == 1, "TIMER-01: after() must not fire a second time");

    delete obj;
}

// ============================================================
// TIMER-02: timer:every(0.5, fn) fires repeatedly every 0.5s
// ============================================================
static void test_timer02_every_repeats() {
    printf("--- TIMER-02: timer:every fires repeatedly ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_Timer>();

    bool loaded = script->loadScript(
        "count = 0\n"
        "function init(self)\n"
        "    local t = self:get('C_Timer')\n"
        "    if t then\n"
        "        t:every(0.5, function(self)\n"
        "            count = count + 1\n"
        "        end)\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "TIMER-02: script loaded");

    // First tick: 0.5s -> fires once
    obj->update(0.5f);
    double c1 = script->getScriptNumber("count", 0.0);
    ASSERT(static_cast<int>(c1) == 1, "TIMER-02: count should be 1 after 0.5s");

    // Second tick: another 0.5s -> fires again
    obj->update(0.5f);
    double c2 = script->getScriptNumber("count", 0.0);
    ASSERT(static_cast<int>(c2) == 2, "TIMER-02: count should be 2 after 1.0s");

    // Third tick: another 0.5s -> fires again
    obj->update(0.5f);
    double c3 = script->getScriptNumber("count", 0.0);
    ASSERT(static_cast<int>(c3) == 3, "TIMER-02: count should be 3 after 1.5s");

    delete obj;
}

// ============================================================
// TIMER-03: timer:cancel(id) prevents the callback from firing
// ============================================================
static void test_timer03_cancel_prevents_fire() {
    printf("--- TIMER-03: timer:cancel prevents firing ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_Timer>();

    bool loaded = script->loadScript(
        "cancelled_fired = false\n"
        "function init(self)\n"
        "    local t = self:get('C_Timer')\n"
        "    if t then\n"
        "        local id = t:after(1.0, function(self)\n"
        "            cancelled_fired = true\n"
        "        end)\n"
        "        t:cancel(id)\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "TIMER-03: script loaded");

    // Update past the deadline — should NOT fire
    obj->update(2.0f);
    bool fired = script->getScriptBool("cancelled_fired", false);
    ASSERT(!fired, "TIMER-03: cancelled timer must not fire");

    delete obj;
}

// ============================================================
// TIMER-03b: cancel is ID-specific — only cancels the target timer
// ============================================================
static void test_timer03b_cancel_specificity() {
    printf("--- TIMER-03b: cancel is ID-specific ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_Timer>();

    bool loaded = script->loadScript(
        "first_fired = false\n"
        "second_fired = false\n"
        "function init(self)\n"
        "    local t = self:get('C_Timer')\n"
        "    if t then\n"
        "        local id1 = t:after(1.0, function(self)\n"
        "            first_fired = true\n"
        "        end)\n"
        "        t:after(1.0, function(self)\n"
        "            second_fired = true\n"
        "        end)\n"
        "        t:cancel(id1)\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "TIMER-03b: script loaded");

    obj->update(2.0f);
    bool first = script->getScriptBool("first_fired", false);
    bool second = script->getScriptBool("second_fired", false);
    ASSERT(!first, "TIMER-03b: first timer (cancelled) must not fire");
    ASSERT(second, "TIMER-03b: second timer (not cancelled) must fire");

    delete obj;
}

// ============================================================
// TIMER-04: Timer callbacks receive ScriptProxy as first argument
// ============================================================
static void test_timer04_callback_receives_self() {
    printf("--- TIMER-04: timer callback receives ScriptProxy as self ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    obj->addComponent<C_Timer>();

    bool loaded = script->loadScript(
        "got_self = false\n"
        "got_visible = false\n"
        "function init(self)\n"
        "    local t = self:get('C_Timer')\n"
        "    if t then\n"
        "        t:after(0.1, function(s)\n"
        "            got_self = (s ~= nil)\n"
        "            got_visible = (s ~= nil and s.visible ~= nil)\n"
        "        end)\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "TIMER-04: script loaded");

    obj->update(0.2f);
    bool gotSelf = script->getScriptBool("got_self", false);
    bool gotVisible = script->getScriptBool("got_visible", false);
    ASSERT(gotSelf, "TIMER-04: callback must receive non-nil self");
    ASSERT(gotVisible, "TIMER-04: self.visible must be accessible in callback");

    delete obj;
}

// ============================================================
// TIMER-05a: clearTimers() releases all luaL_ref handles on hot-reload
// ============================================================
static void test_timer05a_clear_on_reload() {
    printf("--- TIMER-05a: clearTimers on hot-reload ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_Timer* timerComp = obj->addComponent<C_Timer>();

    ASSERT(timerComp != nullptr, "TIMER-05a: C_Timer component created");

    bool loaded = script->loadScript(
        "function init(self)\n"
        "    local t = self:get('C_Timer')\n"
        "    if t then\n"
        "        t:after(10.0, function(self) end)\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "TIMER-05a: initial script loaded");

    // After init(), timer should be active
    int activeCount = timerComp->getActiveCount();
    ASSERT(activeCount == 1, "TIMER-05a: one timer should be active after init()");

    // Verify the timer slot has a valid ref
    bool hasRef = (timerComp->getTimerEntry(0).callbackRef != LUA_NOREF);
    ASSERT(hasRef, "TIMER-05a: timer entry should have a valid Lua ref");

    // Hot-reload: loadScript() calls executeScript() which calls clearTimers()
    bool reloaded = script->loadScript(
        "function init(self)\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(reloaded, "TIMER-05a: hot-reload script loaded");

    // After reload, clearTimers() should have been called
    int activeAfter = timerComp->getActiveCount();
    ASSERT(activeAfter == 0, "TIMER-05a: all timers should be cleared after hot-reload");

    // All timer entry refs should be LUA_NOREF
    bool allCleared = true;
    for (int i = 0; i < C_Timer::MAX_TIMERS; ++i) {
        if (timerComp->getTimerEntry(i).callbackRef != LUA_NOREF) {
            allCleared = false;
            break;
        }
    }
    ASSERT(allCleared, "TIMER-05a: all timer entries should have LUA_NOREF after clearTimers()");

    delete obj;
}

// ============================================================
// TIMER-05b: clearTimers() via C_LuaScript destructor — no crash
// ============================================================
static void test_timer05b_clear_on_destroy() {
    printf("--- TIMER-05b: clearTimers via C_LuaScript destructor ---\n");

    // This test succeeds if it doesn't crash or trigger ASAN errors.
    // C_LuaScript::~C_LuaScript() must call C_Timer::clearTimers() before
    // scriptSystem->shutdown() to prevent use-after-free of Lua state.
    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    C_Timer* timerComp = obj->addComponent<C_Timer>();

    ASSERT(script != nullptr, "TIMER-05b: script component created");
    ASSERT(timerComp != nullptr, "TIMER-05b: timer component created");

    bool loaded = script->loadScript(
        "function init(self)\n"
        "    local t = self:get('C_Timer')\n"
        "    if t then\n"
        "        t:after(10.0, function(self) end)\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "TIMER-05b: script loaded");

    int activeCount = timerComp->getActiveCount();
    ASSERT(activeCount == 1, "TIMER-05b: one timer active before destruction");

    // Delete Object — this destructs C_LuaScript (which calls C_Timer::clearTimers()
    // before shutdown()) and then C_Timer. Must not crash.
    delete obj;

    // If we reach here without crash, the test passes.
    ASSERT(true, "TIMER-05b: Object deleted without crash");
}

int main() {
    test_timer01_after_fires_once();
    test_timer02_every_repeats();
    test_timer03_cancel_prevents_fire();
    test_timer03b_cancel_specificity();
    test_timer04_callback_receives_self();
    test_timer05a_clear_on_reload();
    test_timer05b_clear_on_destroy();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
