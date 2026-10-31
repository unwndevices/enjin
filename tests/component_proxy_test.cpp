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
 *   PROXY-05: a dropped proxy, once collected, leaves no back-pointer for the component's destructor
 *   PROXY-06: every get/add of one component gives the same proxy, so all of them go stale with it
 */
#include <enjin2/core/object.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/components/body.hpp>
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
    lss.getBindings().setFeatures(LuaFeatures::all());  // the component proxies
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

// A bare VM with the proxies and an active scene the test can kill at will.
struct SceneVm {
    LuaEngine engine;
    LuaBindings bindings;
    Scene* scene = new Scene(1u);

    SceneVm() : bindings(&engine) {
        engine.initialize();
        bindings.setFeatures(LuaFeatures::all());
        bindings.registerAll();
        bindings.setActiveScene(scene);
    }
    ~SceneVm() { killScene(); }
    void killScene() {
        bindings.setActiveScene(nullptr);
        delete scene;
        scene = nullptr;
    }
    bool run(const char* code) { return engine.executeString(code).success; }
};

// ============================================================
// PROXY-05: a dropped component proxy is collected before its component dies
//
// An applet that drops obj:add()'s result (engine.scene.spawn('x'):add(...))
// lets the GC free the proxy while the component lives on. The component's
// destructor must not then write `valid = false` into the freed userdata
// (a heap-use-after-free under ASan): the proxy's __gc detaches it first.
// ============================================================
static void test_proxy05_collected_proxy_detaches()
{
    printf("--- PROXY-05: a collected proxy detaches from its component ---\n");
    SceneVm vm;
    ASSERT(vm.run("engine.scene.spawn('dropped'):add('C_Body', { radius = 2 })\n"
                  "engine.scene.spawn('kept'):get('C_Position')\n"
                  "collectgarbage(); collectgarbage()\n"),
           "PROXY-05: Lua runs");

    Object* dropped = vm.scene->findByName("dropped");
    Object* kept = vm.scene->findByName("kept");
    ASSERT(dropped && kept, "PROXY-05: both objects are in the scene");
    if (dropped) {
        ASSERT(dropped->getComponent<C_Body>()->getLuaProxy() == nullptr,
               "PROXY-05: the collected C_Body proxy no longer hangs off its component");
    }
    if (kept) {
        ASSERT(kept->getPosition()->getLuaProxy() == nullptr,
               "PROXY-05: the collected C_Position proxy no longer hangs off its component");
    }
    // The components die after their proxies were freed: ASan flags any write.
    vm.killScene();
}

// ============================================================
// PROXY-06: one proxy per component
//
// get() twice used to make two proxies, and only the last went stale when
// the component died: the first kept a dangling pointer. Every get/add of a
// component now returns the one live proxy.
// ============================================================
static void test_proxy06_one_proxy_per_component()
{
    printf("--- PROXY-06: get/add of one component give one proxy ---\n");
    SceneVm vm;
    ASSERT(vm.run("o = engine.scene.spawn('ball')\n"
                  "a = o:add('C_Body', { radius = 2 })\n"
                  "b = o:get('C_Body')\n"
                  "same = rawequal(a, b)\n"
                  "o:destroy()\n"
                  "okA = pcall(function() return a.x end)\n"
                  "okB = pcall(function() return b.x end)\n"),
           "PROXY-06: Lua runs");
    ASSERT(vm.engine.getGlobalBool("same", false), "PROXY-06: add then get return the same proxy");
    ASSERT(!vm.engine.getGlobalBool("okA", true), "PROXY-06: the first proxy went stale with the body");
    ASSERT(!vm.engine.getGlobalBool("okB", true), "PROXY-06: the second proxy went stale with the body");
}

// ============================================================
// PROXY-07: a component that dies and a new one at the same address
//
// The proxy cache is keyed by the component's address. A new component the
// allocator puts at a dead one's address must get a fresh, live proxy, not the
// dead one's stale proxy.
// ============================================================
static void test_proxy07_reused_address_gets_a_fresh_proxy()
{
    printf("--- PROXY-07: a reused component address gets a fresh proxy ---\n");
    SceneVm vm;
    ASSERT(vm.run("old = {}\n"
                  "for i = 1, 8 do\n"
                  "  local o = engine.scene.spawn('b')\n"
                  "  old[i] = o:add('C_Body', { radius = 2 })\n"
                  "  o:destroy()\n"
                  "end\n"
                  "fresh = engine.scene.spawn('b'):add('C_Body', { radius = 3 })\n"
                  "ok, r = pcall(function() return fresh.radius end)\n"),
           "PROXY-07: Lua runs");
    ASSERT(vm.engine.getGlobalBool("ok", false), "PROXY-07: the new body's proxy is live");
    ASSERT(vm.engine.getGlobalNumber("r", -1) == 3, "PROXY-07: and reads the new body");
}

int main() {
    test_proxy01_get_returns_proxy();
    test_proxy02_typed_methods();
    test_proxy03_destructor_invalidation();
    test_proxy04_stale_raises_error();
    test_proxy04b_get_priority_over_properties();
    test_proxy05_collected_proxy_detaches();
    test_proxy06_one_proxy_per_component();
    test_proxy07_reused_address_gets_a_fresh_proxy();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
