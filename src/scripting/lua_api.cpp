// Lua API descriptors: registration and the per-VM registry (Tomodachi #257).
#include "../../include/enjin2/scripting/lua_api.hpp"

#include <cstring>

namespace enjin2 {

namespace {

// Registry key of the table recording registered modules: an array of
// lightuserdata (registration order) plus a lightuserdata -> true set.
constexpr const char* kApiRegistryKey = "enjin.api";

void recordModule(lua_State* L, const LuaApiModule& module) {
    lua_getfield(L, LUA_REGISTRYINDEX, kApiRegistryKey);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, kApiRegistryKey);
    }
    const void* key = &module;
    lua_pushlightuserdata(L, const_cast<void*>(key));
    lua_rawget(L, -2);
    const bool seen = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    if (!seen) {
        lua_pushlightuserdata(L, const_cast<void*>(key));
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);
        lua_pushlightuserdata(L, const_cast<void*>(key));
        lua_rawseti(L, -2, static_cast<lua_Integer>(lua_rawlen(L, -2)) + 1);
    }
    lua_pop(L, 1);
}

void pushTable(lua_State* L, const LuaApiModule& module);

// Push one entry's value; false for entries that register nothing.
bool pushEntry(lua_State* L, const LuaApiEntry& e) {
    switch (e.kind) {
        case LuaApiKind::Function:
            lua_pushcfunction(L, e.func);
            return true;
        case LuaApiKind::Constant:
            lua_pushinteger(L, e.value);
            return true;
        case LuaApiKind::Table:
            pushTable(L, *e.table);
            return true;
        case LuaApiKind::Lifecycle:
            break;
    }
    return false;
}

void setEntries(lua_State* L, int tableIdx, const LuaApiModule& module) {
    for (size_t i = 0; i < module.count; ++i) {
        const LuaApiEntry& e = module.entries[i];
        if (pushEntry(L, e)) lua_setfield(L, tableIdx, e.name);
    }
}

void pushTable(lua_State* L, const LuaApiModule& module) {
    lua_createtable(L, 0, static_cast<int>(module.count));
    setEntries(L, lua_gettop(L), module);
}

const char* leafName(const char* path) {
    const char* dot = std::strrchr(path, '.');
    return dot ? dot + 1 : path;
}

} // namespace

void luaApiSetFields(lua_State* L, int tableIdx, const LuaApiModule& module) {
    setEntries(L, lua_absindex(L, tableIdx), module);
    recordModule(L, module);
}

void luaApiSetSubtable(lua_State* L, int parentIdx, const LuaApiModule& module) {
    parentIdx = lua_absindex(L, parentIdx);
    pushTable(L, module);
    lua_setfield(L, parentIdx, leafName(module.path));
    recordModule(L, module);
}

void luaApiSetGlobalTable(lua_State* L, const LuaApiModule& module) {
    pushTable(L, module);
    lua_setglobal(L, module.path);
    recordModule(L, module);
}

void luaApiSetGlobals(lua_State* L, const LuaApiModule& module) {
    for (size_t i = 0; i < module.count; ++i) {
        const LuaApiEntry& e = module.entries[i];
        if (pushEntry(L, e)) lua_setglobal(L, e.name);
    }
    recordModule(L, module);
}

std::vector<const LuaApiModule*> luaApiModules(lua_State* L) {
    std::vector<const LuaApiModule*> modules;
    lua_getfield(L, LUA_REGISTRYINDEX, kApiRegistryKey);
    if (lua_istable(L, -1)) {
        const lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L, -1));
        modules.reserve(static_cast<size_t>(n));
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            modules.push_back(static_cast<const LuaApiModule*>(lua_touserdata(L, -1)));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return modules;
}

} // namespace enjin2
