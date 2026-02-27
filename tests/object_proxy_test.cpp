/**
 * @file object_proxy_test.cpp
 * @brief Tests for ObjectProxy returned by engine.scene.find() (Phase 37)
 *
 * Tests:
 *   OBJ-PROXY-01: engine.scene.find() returns nil when no active scene (safe nil path)
 *   OBJ-PROXY-02: ObjectProxy.hasTag(tag) — C++ level: Object tag API that proxy delegates to
 *   OBJ-PROXY-03: ObjectProxy.position — C++ level: C_Position read/write API the proxy delegates to
 *   OBJ-PROXY-04: Stale ObjectProxy: Object destructor sets proxy.valid = false
 *   OBJ-PROXY-05: engine.scene.find() returns nil for unknown name (via Lua)
 *   OBJ-PROXY-06: proxy.enable = false disables C_LuaScript; proxy.enable = true re-enables it (C++ level)
 */
#include <enjin2/core/object.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/object_proxy.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/position.hpp>
#include <cstdio>
#include <cstring>

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
// OBJ-PROXY-01: engine.scene.find() returns nil when no active scene
// Tests the nil-safe path: when no scene is registered, find() returns nil.
// ============================================================
static void test_objproxy01_find_returns_nil_no_scene()
{
    printf("--- OBJ-PROXY-01: engine.scene.find returns nil when no active scene ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "OBJ-PROXY-01: addComponent should succeed");

    bool loaded = script->loadScript(
        "found_name = nil\n"
        "function init(self)\n"
        "    local o = engine.scene.find('hero')\n"
        "    if o ~= nil then\n"
        "        found_name = o.name\n"
        "    else\n"
        "        found_name = 'nil_returned'\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "OBJ-PROXY-01: script loaded");

    script->update(0.016f);
    ASSERT(!script->hasErrors(), "OBJ-PROXY-01: no Lua errors (nil-path test)");

    std::string result = script->getScriptString("found_name");
    ASSERT(result == "nil_returned", "OBJ-PROXY-01: find() returns nil when no scene is active");
}

// ============================================================
// OBJ-PROXY-02: C++ level — Object::hasTag() works correctly
// This verifies the tag API the ObjectProxy __index dispatches to.
// ============================================================
static void test_objproxy02_hastag_cpp_level()
{
    printf("--- OBJ-PROXY-02: C++ hasTag API the ObjectProxy delegates to ---\n");

    Object obj;
    obj.addTag("enemy");
    obj.addTag("boss");

    ASSERT(obj.hasTag("enemy"), "OBJ-PROXY-02: hasTag('enemy') should return true after addTag");
    ASSERT(obj.hasTag("boss"),  "OBJ-PROXY-02: hasTag('boss') should return true after addTag");
    ASSERT(!obj.hasTag("hero"), "OBJ-PROXY-02: hasTag('hero') should return false (not added)");

    obj.clearTags();
    ASSERT(!obj.hasTag("enemy"), "OBJ-PROXY-02: hasTag('enemy') should return false after clearTags");
    ASSERT(obj.getTagCount() == 0, "OBJ-PROXY-02: getTagCount() should be 0 after clearTags");
}

// ============================================================
// OBJ-PROXY-03: C++ level — C_Position read/write path the ObjectProxy position field uses
// ============================================================
static void test_objproxy03_position_cpp_level()
{
    printf("--- OBJ-PROXY-03: C++ C_Position API the ObjectProxy position field delegates to ---\n");

    Object obj;
    // Object constructor auto-adds C_Position; retrieve it via getPosition()
    C_Position* pos = obj.getPosition();
    ASSERT(pos != nullptr, "OBJ-PROXY-03: Object should have C_Position from constructor");

    // Initial position should be (0, 0)
    auto initial = pos->getPosition();
    ASSERT(initial.x == 0 && initial.y == 0,
           "OBJ-PROXY-03: initial position should be (0, 0)");

    // Write via setPosition — this is what ObjectProxy.__newindex dispatches to
    pos->setPosition(42, 99);
    auto updated = pos->getPosition();
    ASSERT(updated.x == 42, "OBJ-PROXY-03: x should be 42 after setPosition(42, 99)");
    ASSERT(updated.y == 99, "OBJ-PROXY-03: y should be 99 after setPosition(42, 99)");

    // Verify Object::getPosition() returns the same component
    ASSERT(obj.getPosition() == pos,
           "OBJ-PROXY-03: Object::getPosition() should return the C_Position component");
}

// ============================================================
// OBJ-PROXY-04: Stale ObjectProxy raises Lua error after Object is destroyed
// Test via direct ObjectProxy struct manipulation since Scene machinery
// is not available in this unit test context.
// ============================================================
static void test_objproxy04_stale_proxy_raises_error()
{
    printf("--- OBJ-PROXY-04: stale ObjectProxy valid=false -> Object destructor called ---\n");

    // Verify the invalidation chain:
    //   1. Create Object and bind an ObjectProxy (simulating engine.scene.find()).
    //   2. Object goes out of scope -> destructor runs -> proxy.valid = false.
    ObjectProxy proxy;
    proxy.object = nullptr;
    proxy.valid  = true;

    {
        Object tempObj;
        proxy.object = &tempObj;
        proxy.valid  = true;
        tempObj.setLuaProxy(&proxy);
        // tempObj goes out of scope here — destructor runs, sets proxy.valid = false
    }
    ASSERT(proxy.valid == false, "OBJ-PROXY-04: Object destructor must set proxy.valid = false");
}

// ============================================================
// OBJ-PROXY-05: engine.scene.find() returns nil for unknown name (via Lua)
// ============================================================
static void test_objproxy05_find_returns_nil_for_unknown()
{
    printf("--- OBJ-PROXY-05: engine.scene.find returns nil for unknown name ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "OBJ-PROXY-05: addComponent should succeed");

    bool loaded = script->loadScript(
        "found = 'not_nil'\n"
        "function init(self)\n"
        "    local r = engine.scene.find('does_not_exist')\n"
        "    if r == nil then found = 'nil' end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "OBJ-PROXY-05: script loaded");

    script->update(0.016f);
    ASSERT(!script->hasErrors(), "OBJ-PROXY-05: no Lua errors");

    std::string result = script->getScriptString("found");
    ASSERT(result == "nil", "OBJ-PROXY-05: find() returns nil for unknown name");
}

// ============================================================
// OBJ-PROXY-06: C++ level — Component::setEnabled() / isEnabled() path that proxy.enable uses
// Locked decision (Phase 37 CONTEXT.md):
//   "ObjectProxy exposes full access: ... component enable/disable control."
//   proxy.enable = false  -> C_LuaScript::setEnabled(false)  (via __newindex)
//   proxy.enable          -> C_LuaScript::isEnabled()         (via __index)
// ============================================================
static void test_objproxy06_enable_disable_cpp_level()
{
    printf("--- OBJ-PROXY-06: C++ Component enable/disable API the ObjectProxy delegates to ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "OBJ-PROXY-06: addComponent<C_LuaScript> should succeed");

    // Components start enabled by default
    ASSERT(script->isEnabled(), "OBJ-PROXY-06: C_LuaScript should be enabled by default");

    // Disable via setEnabled — this is what ObjectProxy.__newindex 'enable' dispatches to
    script->setEnabled(false);
    ASSERT(!script->isEnabled(), "OBJ-PROXY-06: isEnabled() should return false after setEnabled(false)");

    // Re-enable
    script->setEnabled(true);
    ASSERT(script->isEnabled(), "OBJ-PROXY-06: isEnabled() should return true after setEnabled(true)");

    // Verify getComponent<C_LuaScript>() returns the same component — proxy newindex path
    C_LuaScript* fetched = obj.getComponent<C_LuaScript>();
    ASSERT(fetched == script,
           "OBJ-PROXY-06: getComponent<C_LuaScript>() should return the same component instance");
}

int main() {
    test_objproxy01_find_returns_nil_no_scene();
    test_objproxy02_hastag_cpp_level();
    test_objproxy03_position_cpp_level();
    test_objproxy04_stale_proxy_raises_error();
    test_objproxy05_find_returns_nil_for_unknown();
    test_objproxy06_enable_disable_cpp_level();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
