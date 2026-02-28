/**
 * @file component_proxy_test.cpp
 * @brief Tests for ComponentProxy infrastructure (Phase 39: PROXY-01..PROXY-04)
 *
 * Tests:
 *   PROXY-01: self:get("C_Position") returns non-nil proxy userdata in init()
 *   PROXY-02: Returned proxy exposes getX() and getY() returning correct integer values
 *   PROXY-03: Component destructor sets ComponentProxy.valid = false (C++ level)
 *   PROXY-04: Stale ComponentProxy access raises luaL_error containing "component has been destroyed"
 *   PROXY-04b: "get" key is checked before all other ScriptProxy properties (collision prevention)
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/scripting/component_proxy.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
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
// PROXY-01: self:get("C_Position") returns non-nil proxy in init()
// ============================================================
static void test_proxy01_get_returns_proxy()
{
    printf("--- PROXY-01: self:get('C_Position') returns non-nil proxy ---\n");

    Object obj;
    C_Position* pos = obj.getPosition();
    ASSERT(pos != nullptr, "PROXY-01: Object should have C_Position");
    pos->setPosition(42, 99);

    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "PROXY-01: addComponent<C_LuaScript> should succeed");

    bool loaded = script->loadScript(
        "got_proxy = false\n"
        "function init(self)\n"
        "    local pos = self:get('C_Position')\n"
        "    if pos ~= nil then\n"
        "        got_proxy = true\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "PROXY-01: script loaded");

    script->update(0.016f);
    ASSERT(!script->hasErrors(), "PROXY-01: no Lua errors");

    bool gotProxy = script->getScriptBool("got_proxy", false);
    ASSERT(gotProxy, "PROXY-01: self:get('C_Position') should return non-nil proxy");
}

// ============================================================
// PROXY-02: Proxy exposes getX() and getY() returning correct values
// ============================================================
static void test_proxy02_typed_methods()
{
    printf("--- PROXY-02: pos:getX() and pos:getY() return correct values ---\n");

    Object obj;
    C_Position* pos = obj.getPosition();
    ASSERT(pos != nullptr, "PROXY-02: Object should have C_Position");
    pos->setPosition(42, 99);

    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "PROXY-02: addComponent<C_LuaScript> should succeed");

    bool loaded = script->loadScript(
        "got_x = -1\n"
        "got_y = -1\n"
        "function init(self)\n"
        "    local p = self:get('C_Position')\n"
        "    if p ~= nil then\n"
        "        got_x = p:getX()\n"
        "        got_y = p:getY()\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "PROXY-02: script loaded");

    script->update(0.016f);
    ASSERT(!script->hasErrors(), "PROXY-02: no Lua errors");

    double gotX = script->getScriptNumber("got_x", -1.0);
    double gotY = script->getScriptNumber("got_y", -1.0);

    ASSERT(static_cast<int>(gotX) == 42, "PROXY-02: pos:getX() should return 42");
    ASSERT(static_cast<int>(gotY) == 99, "PROXY-02: pos:getY() should return 99");
}

// ============================================================
// PROXY-03: Component destructor invalidates outstanding proxy (C++ level)
//
// Create a ComponentProxy manually, bind it to a C_Position,
// delete the owning Object. Verify proxy.valid == false.
// ============================================================
static void test_proxy03_destructor_invalidation()
{
    printf("--- PROXY-03: Component destructor sets proxy.valid = false ---\n");

    ComponentProxy proxy;
    proxy.component = nullptr;
    proxy.valid = true;

    {
        Object* obj = new Object();
        C_Position* pos = obj->getPosition();
        ASSERT(pos != nullptr, "PROXY-03: Object should have C_Position");

        proxy.component = pos;
        proxy.valid = true;
        pos->setLuaProxy(&proxy);

        // Delete the Object — destroys C_Position -> Component::~Component() runs
        // -> sets m_luaProxy->valid = false (writes to our local proxy struct).
        delete obj;
    }

    ASSERT(proxy.valid == false,
           "PROXY-03: Component destructor must set proxy.valid = false");
}

// ============================================================
// PROXY-04: Stale ComponentProxy access raises luaL_error
//
// This test uses a standalone LuaScriptSystem (not C_LuaScript) to decouple
// the Lua state lifetime from the component's owner Object lifetime.
//
// Approach:
//   1. Create an Object with only C_Position (no C_LuaScript).
//   2. Create a standalone LuaScriptSystem, register all bindings
//      (including C_Position_Proxy metatable).
//   3. Directly push a ComponentProxy userdata to Lua via the C API,
//      set its metatable, register it with C_Position via setLuaProxy().
//   4. Delete the Object -> C_Position destroyed -> cproxy->valid = false.
//   5. Run Lua pcall on the stale proxy, verify the error message.
// ============================================================
static void test_proxy04_stale_raises_error()
{
    printf("--- PROXY-04: stale ComponentProxy raises 'component has been destroyed' error ---\n");

    // Step 1: Create the standalone Lua scripting system
    LuaScriptSystem lss;
    bool initOk = lss.initialize();
    ASSERT(initOk, "PROXY-04: LuaScriptSystem initialize");
    if (!initOk) return;

    // Register all bindings — this registers C_Position_Proxy metatable
    lss.getBindings().registerAll();

    lua_State* L = lss.getEngine().getState();
    ASSERT(L != nullptr, "PROXY-04: Lua state valid");
    if (!L) return;

    // Step 2: Create Object with C_Position on the heap
    Object* posObj = new Object();
    C_Position* pos = posObj->getPosition();
    ASSERT(pos != nullptr, "PROXY-04: posObj should have C_Position");
    pos->setPosition(10, 20);

    // Step 3: Allocate ComponentProxy userdata in Lua, set metatable, push as global
    auto* cproxy = static_cast<ComponentProxy*>(
        lua_newuserdata(L, sizeof(ComponentProxy)));
    cproxy->component = pos;
    cproxy->valid = true;
    luaL_getmetatable(L, "C_Position_Proxy");
    lua_setmetatable(L, -2);
    // Register this proxy with the C_Position so destruction invalidates it
    pos->setLuaProxy(cproxy);
    // Store as Lua global "cached_pos"
    lua_setglobal(L, "cached_pos");

    // Verify the proxy is valid before destruction
    ASSERT(cproxy->valid, "PROXY-04: proxy.valid should be true before destruction");

    // Step 4: Delete posObj — this destroys C_Position — which sets cproxy->valid = false
    delete posObj;
    // pos is now dangling. cproxy (in Lua heap) should have valid == false.
    ASSERT(!cproxy->valid, "PROXY-04: proxy.valid should be false after component destruction");

    // Step 5: Run Lua code that pcall-accesses the stale proxy
    // cached_pos is still in the Lua global, its userdata bytes now have valid=false
    const char* luaCode =
        "stale_error = ''\n"
        "local ok, err = pcall(function()\n"
        "    local x = cached_pos:getX()\n"
        "end)\n"
        "if not ok then\n"
        "    stale_error = tostring(err)\n"
        "end\n";

    int rc = luaL_dostring(L, luaCode);
    if (rc != 0) {
        // If luaL_dostring itself fails (shouldn't happen since pcall catches errors)
        const char* err = lua_tostring(L, -1);
        fprintf(stderr, "PROXY-04: luaL_dostring failed: %s\n", err ? err : "(null)");
        lua_pop(L, 1);
    }

    // Retrieve the error string from Lua
    lua_getglobal(L, "stale_error");
    const char* errMsg = lua_tostring(L, -1);
    lua_pop(L, 1);

    ASSERT(errMsg != nullptr && errMsg[0] != '\0',
           "PROXY-04: stale_error should be non-empty after stale access");

    if (errMsg) {
        bool hasExpected = (strstr(errMsg, "component has been destroyed") != nullptr);
        ASSERT(hasExpected,
               "PROXY-04: error message should contain 'component has been destroyed'");
        if (!hasExpected) {
            fprintf(stderr, "PROXY-04: actual error: %s\n", errMsg);
        }
    }
}

// ============================================================
// PROXY-04b: "get" key is checked first in ScriptProxy.__index
//
// Verify self.get dispatches to lua_proxy_get_component_impl (returns a function)
// before any other property check (x, y, visible, active, name, addTag, hasTag).
// ============================================================
static void test_proxy04b_get_priority_over_properties()
{
    printf("--- PROXY-04b: 'get' key dispatches to component lookup before other properties ---\n");

    Object obj;
    C_Position* pos = obj.getPosition();
    ASSERT(pos != nullptr, "PROXY-04b: Object should have C_Position");
    pos->setPosition(77, 88);

    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "PROXY-04b: addComponent<C_LuaScript> should succeed");

    bool loaded = script->loadScript(
        "got_via_get = false\n"
        "get_is_function = false\n"
        "function init(self)\n"
        "    -- self.get must return a function (lua_proxy_get_component_impl)\n"
        "    -- checked FIRST before 'x', 'y', 'visible', etc.\n"
        "    local fn = self.get\n"
        "    get_is_function = (type(fn) == 'function')\n"
        "    if get_is_function then\n"
        "        local pos_proxy = fn(self, 'C_Position')\n"
        "        if pos_proxy ~= nil then\n"
        "            got_via_get = true\n"
        "        end\n"
        "    end\n"
        "end\n"
        "function update(self, dt)\n"
        "end\n"
    );
    ASSERT(loaded, "PROXY-04b: script loaded");

    script->update(0.016f);
    ASSERT(!script->hasErrors(), "PROXY-04b: no Lua errors");

    bool getIsFunction = script->getScriptBool("get_is_function", false);
    ASSERT(getIsFunction, "PROXY-04b: self.get should be a function (not a property)");

    bool gotViaGet = script->getScriptBool("got_via_get", false);
    ASSERT(gotViaGet, "PROXY-04b: calling self.get(self, 'C_Position') should return non-nil proxy");
}

int main() {
    test_proxy01_get_returns_proxy();
    test_proxy02_typed_methods();
    test_proxy03_destructor_invalidation();
    test_proxy04_stale_raises_error();
    test_proxy04b_get_priority_over_properties();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
