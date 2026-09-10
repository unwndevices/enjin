/**
 * @file component_registry_test.cpp
 * @brief Parity test for the source-of-truth component registry (ADR-0003 §2).
 *
 * The registry (ENJIN2_COMPONENT_LIST in component_registry.hpp) is the single
 * place that maps a component name to its C++ type; the Lua add/get dispatch is
 * generated from it. These tests assert the invariant the ADR requires:
 *
 *   REG-01: every registry row round-trips at the C++ level — add() attaches a
 *           component and get() then returns that same pointer.
 *   REG-02: the registry is non-empty and every row is well-formed (names +
 *           function pointers present, names unique).
 *   REG-03: the one attach verb works from Lua — self:add(name) then
 *           self:get(name) is non-nil for every registered component.
 *   REG-04: the returned proxy is writable — p.x = N routes to the C++ setter
 *           (C_Position), and the write is visible through the owner object.
 *
 * Adding a component to the registry with no other glue must keep all four
 * green — that is the "no Lua-only helper can exist" guarantee.
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/scripting/component_registry.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <cstdio>
#include <cstring>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                       \
    do {                                        \
        if (!(cond)) {                          \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++;                         \
        } else {                                \
            passes++;                           \
        }                                       \
    } while (0)

// ============================================================
// REG-01: every registry row round-trips add() -> get() in C++
// ============================================================
static void test_reg01_cpp_roundtrip() {
    printf("--- REG-01: registry rows round-trip add()/get() in C++ ---\n");

    std::size_t count = 0;
    const ComponentRegistryEntry* entries = componentRegistry(count);
    ASSERT(entries != nullptr, "REG-01: registry table is non-null");

    for (std::size_t i = 0; i < count; ++i) {
        const ComponentRegistryEntry& e = entries[i];
        Object obj;  // auto-adds C_Position

        // Mirror the attach-or-fetch semantic the `add` verb uses (pushComponentAdd):
        // never duplicate a component that already exists (e.g. the auto-added
        // C_Position), otherwise attach a fresh one.
        Component* existing = e.get(&obj);
        Component* added = existing ? existing : e.add(&obj);
        ASSERT(added != nullptr, e.luaName);  // luaName labels which row failed

        Component* fetched = e.get(&obj);
        ASSERT(fetched != nullptr, e.luaName);
        ASSERT(fetched == added, e.luaName);  // one component of this type on the object
    }
}

// ============================================================
// REG-02: registry is well-formed (non-empty, unique names, ptrs set)
// ============================================================
static void test_reg02_wellformed() {
    printf("--- REG-02: registry table is well-formed ---\n");

    std::size_t count = 0;
    const ComponentRegistryEntry* entries = componentRegistry(count);
    ASSERT(count > 0, "REG-02: registry is non-empty");

    for (std::size_t i = 0; i < count; ++i) {
        ASSERT(entries[i].luaName && entries[i].luaName[0], "REG-02: luaName set");
        ASSERT(entries[i].proxyMeta && entries[i].proxyMeta[0], "REG-02: proxyMeta set");
        ASSERT(entries[i].get != nullptr, "REG-02: get thunk set");
        ASSERT(entries[i].add != nullptr, "REG-02: add thunk set");
        // findComponentEntry must resolve every registered name to this row.
        ASSERT(findComponentEntry(entries[i].luaName) == &entries[i],
               "REG-02: findComponentEntry resolves the name");
        for (std::size_t j = i + 1; j < count; ++j) {
            ASSERT(std::strcmp(entries[i].luaName, entries[j].luaName) != 0,
                   "REG-02: names are unique");
        }
    }

    ASSERT(findComponentEntry("C_DoesNotExist") == nullptr,
           "REG-02: unknown name resolves to nullptr");
    ASSERT(findComponentEntry(nullptr) == nullptr,
           "REG-02: null name resolves to nullptr");
}

// ============================================================
// REG-03: self:add(name) then self:get(name) non-nil for every row (Lua)
// ============================================================
static void test_reg03_lua_add_get() {
    printf("--- REG-03: self:add()/self:get() work for every registered component ---\n");

    // Build the Lua script that loops the registry names supplied from C++.
    std::size_t count = 0;
    const ComponentRegistryEntry* entries = componentRegistry(count);

    std::string names;  // Lua table literal: {"C_Position", "C_Timer", ...}
    names += "{";
    for (std::size_t i = 0; i < count; ++i) {
        names += "'";
        names += entries[i].luaName;
        names += "',";
    }
    names += "}";

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "REG-03: addComponent<C_LuaScript>");

    std::string src =
        "all_added = true\n"
        "all_got = true\n"
        "function init(self)\n"
        "  local names = " + names + "\n"
        "  for _, n in ipairs(names) do\n"
        "    local a = self:add(n)\n"
        "    if a == nil then all_added = false end\n"
        "    local g = self:get(n)\n"
        "    if g == nil then all_got = false end\n"
        "  end\n"
        "end\n"
        "function update(self, dt) end\n";

    bool loaded = script->loadScript(src.c_str());
    ASSERT(loaded, "REG-03: script loaded");
    script->update(0.016f);
    ASSERT(!script->hasErrors(), "REG-03: no Lua errors");
    ASSERT(script->getScriptBool("all_added", false), "REG-03: self:add() non-nil for all rows");
    ASSERT(script->getScriptBool("all_got", false), "REG-03: self:get() non-nil for all rows");
}

// ============================================================
// REG-04: the returned proxy is writable — p.x = N hits the C++ setter
// ============================================================
static void test_reg04_writable_proxy() {
    printf("--- REG-04: added component proxy is writable (p.x = N -> C++ setter) ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "REG-04: addComponent<C_LuaScript>");

    bool loaded = script->loadScript(
        "read_back = -1\n"
        "function init(self)\n"
        "  local p = self:add('C_Position')\n"
        "  p.x = 123\n"
        "  p.y = 45\n"
        "  read_back = p.x\n"
        "end\n"
        "function update(self, dt) end\n");
    ASSERT(loaded, "REG-04: script loaded");
    script->update(0.016f);
    ASSERT(!script->hasErrors(), "REG-04: no Lua errors");

    // Write is visible through Lua (proxy read side)...
    ASSERT(static_cast<int>(script->getScriptNumber("read_back", -1.0)) == 123,
           "REG-04: p.x reads back the written value");
    // ...and through the owning C++ object (the setter really fired).
    C_Position* pos = obj.getPosition();
    ASSERT(pos != nullptr, "REG-04: owner has C_Position");
    ASSERT(pos->getPosition().x == 123 && pos->getPosition().y == 45,
           "REG-04: C_Position setter received the writes");
}

// ============================================================
// REG-05: add(name, params) applies the params table via the proxy setters
// ============================================================
static void test_reg05_add_with_params() {
    printf("--- REG-05: self:add('C_Position', {x=,y=}) configures via setters ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "REG-05: addComponent<C_LuaScript>");

    bool loaded = script->loadScript(
        "function init(self)\n"
        "  self:add('C_Position', { x = 71, y = 82 })\n"
        "end\n"
        "function update(self, dt) end\n");
    ASSERT(loaded, "REG-05: script loaded");
    script->update(0.016f);
    ASSERT(!script->hasErrors(), "REG-05: no Lua errors");

    C_Position* pos = obj.getPosition();
    ASSERT(pos != nullptr, "REG-05: owner has C_Position");
    ASSERT(pos->getPosition().x == 71 && pos->getPosition().y == 82,
           "REG-05: params table routed to the C_Position setter");
}

int main() {
    test_reg01_cpp_roundtrip();
    test_reg02_wellformed();
    test_reg03_lua_add_get();
    test_reg04_writable_proxy();
    test_reg05_add_with_params();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
