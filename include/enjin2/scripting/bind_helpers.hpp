#pragma once
#include "lua_platform.hpp"

namespace enjin2 {

/** @brief Entry in a Lua function binding table */
struct LuaFuncDef {
    const char*   name;  ///< Function name exposed to Lua
    lua_CFunction func;  ///< C callback implementing the function
};

template<typename T, int N>
constexpr int luaArrayLen(T (&)[N]) { return N; }
#define ENJIN_ARRAY_LEN(arr) (enjin2::luaArrayLen(arr))

// Register entries into a table at stack index tableIdx (can be negative).
// Normalizes to absolute index so loop pushes don't shift it.
inline void luaBindFunctions(lua_State* L, int tableIdx,
                             const LuaFuncDef* defs, int n) {
    if (tableIdx < 0) tableIdx = lua_gettop(L) + tableIdx + 1;
    for (int i = 0; i < n; ++i) {
        lua_pushcfunction(L, defs[i].func);
        lua_setfield(L, tableIdx, defs[i].name);
    }
}

// Register entries directly as Lua globals.
inline void luaBindGlobals(lua_State* L, const LuaFuncDef* defs, int n) {
    for (int i = 0; i < n; ++i) {
        lua_pushcfunction(L, defs[i].func);
        lua_setglobal(L, defs[i].name);
    }
}

} // namespace enjin2
