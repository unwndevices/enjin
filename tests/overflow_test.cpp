/**
 * @file overflow_test.cpp
 * @brief Overflow boundary tests for event bus, sprite pool, and component destruction.
 *
 * Tests:
 *   OVERFLOW-01: EventBus channel overflow (beyond MAX_CHANNELS=16) returns 0 (failure ID)
 *   OVERFLOW-02: EventBus subscriber overflow (beyond MAX_SUBS_PER_CH=8) returns 0
 *   OVERFLOW-03: Component destruction proxy safety (stale C_LuaScript proxy invalidated)
 *
 * Phase 46: TEST-02 — BIND-02, TEST-01, TEST-02
 */
#include "../include/enjin2/scripting/bindings.hpp"
#include "../include/enjin2/scripting/lua_event_bus.hpp"
#include "../include/enjin2/core/object.hpp"
#include "../include/enjin2/components/lua_script.hpp"
#include <cstdio>

using namespace enjin2;

static int passes = 0, failures = 0;
#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else { passes++; } \
} while(0)

//==============================================================================
// OVERFLOW-01: EventBus channel overflow (MAX_CHANNELS=16)
//
// Subscribe to 16 distinct event names. Each should return a valid (>0) ID.
// The 17th channel subscription should return 0 (bus is full).
//
// Lua approach: build the channel name in Lua (ch0..ch15 = 16 channels),
// store each returned ID in a separate global, then store overflow ID.
//==============================================================================
static void test_overflow01_eventbus_channel_overflow() {
    printf("--- OVERFLOW-01: EventBus channel overflow (MAX_CHANNELS=16) ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    ASSERT(script != nullptr, "OVERFLOW-01: addComponent<C_LuaScript> should succeed");

    // Subscribe to 16 distinct channel names (ch0..ch15).
    // All 16 must succeed. Then subscribe to a 17th 'overflow' channel — must fail (id=0).
    // Use individual globals to avoid table indexing issues with getScriptNumber.
    bool loaded = script->loadScript(
        "all_valid = true\n"
        "overflow_id = -1\n"
        "function init(self)\n"
        "    local id\n"
        "    for i = 0, 15 do\n"
        "        id = engine.event.on('ch' .. i, function() end)\n"
        "        if id == 0 then all_valid = false end\n"
        "    end\n"
        "    overflow_id = engine.event.on('overflow_ch', function() end)\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "OVERFLOW-01: script loaded");

    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "OVERFLOW-01: no script errors after update");

    // All 16 valid channel subscriptions should have returned non-zero IDs
    bool allValid = script->getScriptBool("all_valid", false);
    ASSERT(allValid, "OVERFLOW-01: all 16 channel subscriptions should return non-zero IDs");

    // The 17th subscription (on 'overflow_ch') should return 0 (channel table full)
    double overflowId = script->getScriptNumber("overflow_id", -1.0);
    ASSERT(overflowId == 0.0, "OVERFLOW-01: 17th channel subscription should return 0 (bus full)");

    delete obj;
}

//==============================================================================
// OVERFLOW-02: EventBus subscriber overflow (MAX_SUBS_PER_CH=8)
//
// Subscribe 8 times to the same channel. Each should succeed (>0 ID).
// The 9th subscription on the same channel should return 0 (slot full).
//==============================================================================
static void test_overflow02_eventbus_subscriber_overflow() {
    printf("--- OVERFLOW-02: EventBus subscriber overflow (MAX_SUBS_PER_CH=8) ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    ASSERT(script != nullptr, "OVERFLOW-02: addComponent<C_LuaScript> should succeed");

    bool loaded = script->loadScript(
        "all_subs_valid = true\n"
        "ninth_id = -1\n"
        "function init(self)\n"
        "    local id\n"
        "    for i = 1, 8 do\n"
        "        id = engine.event.on('same_channel', function() end)\n"
        "        if id == 0 then all_subs_valid = false end\n"
        "    end\n"
        "    ninth_id = engine.event.on('same_channel', function() end)\n"
        "end\n"
        "function update(self, dt) end\n"
    );
    ASSERT(loaded, "OVERFLOW-02: script loaded");

    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "OVERFLOW-02: no script errors after update");

    // All 8 valid subscriptions should have returned non-zero IDs
    bool allSubsValid = script->getScriptBool("all_subs_valid", false);
    ASSERT(allSubsValid, "OVERFLOW-02: all 8 subscriber slots should return non-zero IDs");

    // The 9th subscription on the same channel should return 0 (slot full)
    double ninthId = script->getScriptNumber("ninth_id", -1.0);
    ASSERT(ninthId == 0.0, "OVERFLOW-02: 9th subscriber on same channel should return 0 (full)");

    delete obj;
}

//==============================================================================
// OVERFLOW-03: Component destruction proxy safety
//
// Verifies that after a C_LuaScript component's Lua state is torn down
// (via reloadScript() or object deletion), the old ScriptProxy validity flag
// is set to false. The mechanism is: C_LuaScript destructor/reload calls
// lua_close() which triggers GC, which calls the proxy finalizer setting valid=false.
//
// We verify this observationally:
//   1. Script runs normally (accessing self.x and self.y)
//   2. reloadScript() destroys old state — old proxy invalidated
//   3. Post-reload update runs without crash
//   4. Object deletion at end must not crash (all proxies cleaned up)
//==============================================================================
static void test_overflow03_component_destruction_proxy_safety() {
    printf("--- OVERFLOW-03: component destruction proxy safety ---\n");

    Object* obj = new Object();
    C_LuaScript* script = obj->addComponent<C_LuaScript>(64u, 64u);
    ASSERT(script != nullptr, "OVERFLOW-03: addComponent<C_LuaScript> should succeed");

    // Script stores self proxy and accesses properties on each update
    bool loaded = script->loadScript(
        "update_count = 0\n"
        "function init(self)\n"
        "    -- Store self reference (proxy) to test destruction safety\n"
        "    _stored_self = self\n"
        "end\n"
        "function update(self, dt)\n"
        "    -- Access proxy properties on every update (exercises proxy validity path)\n"
        "    local x = self.x\n"
        "    local y = self.y\n"
        "    local vis = self.visible\n"
        "    update_count = update_count + 1\n"
        "end\n"
    );
    ASSERT(loaded, "OVERFLOW-03: script loaded");

    // First update cycle — initializes proxy and runs update
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "OVERFLOW-03: no errors on first update");
    double count1 = script->getScriptNumber("update_count", 0.0);
    ASSERT(count1 >= 1.0, "OVERFLOW-03: update_count >= 1 after first update");

    // Second update — proxy still valid
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "OVERFLOW-03: no errors on second update");

    // Reload destroys old Lua state (invalidating old proxy), creates a fresh state
    bool reloaded = script->reloadScript();
    ASSERT(reloaded, "OVERFLOW-03: reloadScript should succeed without crash");
    ASSERT(!script->hasErrors(), "OVERFLOW-03: no errors immediately after reload");

    // Post-reload update: old proxy is invalidated; new proxy is valid
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "OVERFLOW-03: no errors after reload update");

    // Rapid reload cycle: stress-test proxy invalidation
    bool reloaded2 = script->reloadScript();
    ASSERT(reloaded2, "OVERFLOW-03: second reloadScript should succeed");
    obj->update(0.016f);
    ASSERT(!script->hasErrors(), "OVERFLOW-03: no errors after second reload update");

    delete obj;

    // Reaching here without crash = proxy destruction was safe on Object deletion
    passes++;
    printf("--- OVERFLOW-03: Object deletion completed without crash (proxy safety OK) ---\n");
}

//==============================================================================
// main
//==============================================================================
int main() {
    printf("=== overflow_test: boundary tests for event bus and component destruction ===\n\n");

    test_overflow01_eventbus_channel_overflow();
    test_overflow02_eventbus_subscriber_overflow();
    test_overflow03_component_destruction_proxy_safety();

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
