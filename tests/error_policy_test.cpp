/**
 * @file error_policy_test.cpp
 * @brief Unit tests for ScriptErrorPolicy on C_LuaScript (ERR-01 through ERR-05)
 *
 * Tests are ordered to match requirement IDs:
 *   ERR-01: default policy is Disable
 *   ERR-02: Disable policy sets scriptError=true after error, stops subsequent calls
 *   ERR-03: Log policy does NOT set scriptError; script keeps running
 *   ERR-04: Panic policy field is correctly stored (live abort not invoked — kills process)
 *   ERR-05: reloadScript() clears scriptError and re-enables disabled script
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <cstdio>

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

// Lua code for a script that errors on every update() call
static const char* k_buggyScript =
    "function update(self, dt)\n"
    "    error('boom')\n"
    "end\n";

// Lua code for a well-behaved script (used to reload after error)
static const char* k_goodScript =
    "function update(self, dt)\n"
    "    -- no error\n"
    "end\n";

// ============================================================
// ERR-01: default policy field is ScriptErrorPolicy::Disable
// ============================================================
static void test_err01_default_policy_is_disable()
{
    printf("--- ERR-01: default policy is Disable ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "ERR-01: addComponent<C_LuaScript> should succeed");

    ASSERT(script->getErrorPolicy() == ScriptErrorPolicy::Disable,
           "ERR-01: default errorPolicy should be Disable");
}

// ============================================================
// ERR-02: Disable policy stops script after one error
// ============================================================
static void test_err02_disable_policy_stops_after_error()
{
    printf("--- ERR-02: Disable policy stops script after error ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "ERR-02: addComponent should succeed");

    // Disable is the default — explicit set for clarity
    script->setErrorPolicy(ScriptErrorPolicy::Disable);

    // Load a script that errors on update
    bool loaded = script->loadScript(k_buggyScript);
    ASSERT(loaded, "ERR-02: loadScript should succeed (no syntax errors)");

    // Before any update, no error
    ASSERT(!script->hasErrors(), "ERR-02: hasErrors() should be false before any update");

    // First update triggers the Lua error
    script->update(0.016f);
    ASSERT(script->hasErrors(), "ERR-02: hasErrors() should be true after error with Disable policy");

    // Second update — script should be disabled (update guard skips the call)
    // hasErrors stays true
    script->update(0.016f);
    ASSERT(script->hasErrors(), "ERR-02: hasErrors() should remain true on subsequent updates");
}

// ============================================================
// ERR-03: Log policy does NOT disable script after error
// ============================================================
static void test_err03_log_policy_continues_after_error()
{
    printf("--- ERR-03: Log policy keeps script running ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "ERR-03: addComponent should succeed");

    script->setErrorPolicy(ScriptErrorPolicy::Log);

    bool loaded = script->loadScript(k_buggyScript);
    ASSERT(loaded, "ERR-03: loadScript should succeed");

    // First update — error occurs but Log policy does NOT set scriptError
    script->update(0.016f);
    ASSERT(!script->hasErrors(),
           "ERR-03: hasErrors() should be false after error with Log policy");

    // Second update — script still runs (not disabled)
    script->update(0.016f);
    ASSERT(!script->hasErrors(),
           "ERR-03: hasErrors() still false after second update with Log policy");
}

// ============================================================
// ERR-04: Panic policy field is correctly stored
// NOTE: Live invocation of Panic is NOT tested — std::abort() kills the process.
// The code path is verified by policy field value and source inspection.
// ============================================================
static void test_err04_panic_policy_field_set()
{
    printf("--- ERR-04: Panic policy field correctly stored ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "ERR-04: addComponent should succeed");

    script->setErrorPolicy(ScriptErrorPolicy::Panic);
    ASSERT(script->getErrorPolicy() == ScriptErrorPolicy::Panic,
           "ERR-04: getErrorPolicy() should return Panic after setErrorPolicy(Panic)");

    // Verify round-trip back to Disable
    script->setErrorPolicy(ScriptErrorPolicy::Disable);
    ASSERT(script->getErrorPolicy() == ScriptErrorPolicy::Disable,
           "ERR-04: getErrorPolicy() should return Disable after round-trip");
}

// ============================================================
// ERR-05: reloadScript() clears errorMessage and re-enables disabled script
// ============================================================
static void test_err05_reload_clears_error_state()
{
    printf("--- ERR-05: reloadScript() clears error state ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "ERR-05: addComponent should succeed");

    // Use Disable policy — will set scriptError=true on first error
    script->setErrorPolicy(ScriptErrorPolicy::Disable);

    bool loaded = script->loadScript(k_buggyScript);
    ASSERT(loaded, "ERR-05: loadScript should succeed");

    // Trigger error
    script->update(0.016f);
    ASSERT(script->hasErrors(), "ERR-05: hasErrors() should be true after error");

    // Reload with a good script — should clear error state
    bool reloaded = script->loadScript(k_goodScript);
    ASSERT(reloaded, "ERR-05: loadScript (good script) should succeed");
    ASSERT(!script->hasErrors(),
           "ERR-05: hasErrors() should be false after reload with good script");

    // Verify the script executes again (no longer disabled)
    script->update(0.016f);
    ASSERT(!script->hasErrors(),
           "ERR-05: hasErrors() should remain false after update of reloaded script");
}

// ============================================================
// ERR-SIBLING: Disable policy on one component does NOT block sibling components
// Addresses CONCERNS.md: "ScriptErrorPolicy State Machine — Two-Level Error Handling"
// Verifies: error in component A (Disable) does not block component B's update()
// ============================================================
static void test_err_sibling_not_blocked()
{
    printf("--- ERR-SIBLING: Disable policy does not block sibling components ---\n");

    // Object A: has a buggy script with Disable policy
    Object objA;
    C_LuaScript* scriptA = objA.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(scriptA != nullptr, "ERR-SIBLING: scriptA addComponent should succeed");
    scriptA->setErrorPolicy(ScriptErrorPolicy::Disable);
    bool loadedA = scriptA->loadScript(k_buggyScript);  // errors on every update
    ASSERT(loadedA, "ERR-SIBLING: scriptA loaded");

    // Object B: has a well-behaved script
    Object objB;
    C_LuaScript* scriptB = objB.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(scriptB != nullptr, "ERR-SIBLING: scriptB addComponent should succeed");
    bool loadedB = scriptB->loadScript(
        "update_count = 0\n"
        "function update(self, dt)\n"
        "    update_count = update_count + 1\n"
        "end\n"
    );
    ASSERT(loadedB, "ERR-SIBLING: scriptB loaded");

    // Frame 1: A errors and is disabled; B runs normally
    scriptA->update(0.016f);
    scriptB->update(0.016f);

    ASSERT(scriptA->hasErrors(), "ERR-SIBLING: scriptA should have error after frame 1");
    ASSERT(!scriptB->hasErrors(), "ERR-SIBLING: scriptB should NOT have error after frame 1");

    double countAfter1 = scriptB->getScriptNumber("update_count");
    ASSERT(countAfter1 == 1.0, "ERR-SIBLING: scriptB update_count should be 1 after frame 1");

    // Frame 2: A is disabled (skip), B still runs
    scriptA->update(0.016f);
    scriptB->update(0.016f);

    ASSERT(scriptA->hasErrors(), "ERR-SIBLING: scriptA still disabled on frame 2");
    ASSERT(!scriptB->hasErrors(), "ERR-SIBLING: scriptB still no errors on frame 2");

    double countAfter2 = scriptB->getScriptNumber("update_count");
    ASSERT(countAfter2 == 2.0, "ERR-SIBLING: scriptB update_count should be 2 after frame 2");
}

// ============================================================
// main
// ============================================================
int main()
{
    printf("error_policy_test\n");
    printf("=================\n");

    test_err01_default_policy_is_disable();
    test_err02_disable_policy_stops_after_error();
    test_err03_log_policy_continues_after_error();
    test_err04_panic_policy_field_set();
    test_err05_reload_clears_error_state();
    test_err_sibling_not_blocked();

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
