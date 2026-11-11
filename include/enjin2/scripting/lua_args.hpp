#pragma once
//
// enjin2::lua typed argument and option-field getters (Tomodachi #481): the
// one way a binding reads an argument as a C++ type, in place of
// `static_cast<int>(luaL_checkinteger(L, i))` and its kin. Each folds the
// check and the cast; the failure is luaL_check*'s own (a Lua error naming the
// argument). Deliberately not a templated marshaller: the argument logic stays
// visible at each call site.

#include "../core/name_index.hpp"

#include <cstddef>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2::lua {

// --- Arguments ---------------------------------------------------------------

inline int checkInt(lua_State* L, int idx) {
    return static_cast<int>(luaL_checkinteger(L, idx));
}
inline int optInt(lua_State* L, int idx, int def) {
    return static_cast<int>(luaL_optinteger(L, idx, def));
}
inline float checkFloat(lua_State* L, int idx) {
    return static_cast<float>(luaL_checknumber(L, idx));
}
inline float optFloat(lua_State* L, int idx, float def) {
    return static_cast<float>(luaL_optnumber(L, idx, def));
}
inline const char* checkStr(lua_State* L, int idx) {
    return luaL_checkstring(L, idx);
}
inline const char* optStr(lua_State* L, int idx, const char* def) {
    return luaL_optstring(L, idx, def);
}
inline bool checkBool(lua_State* L, int idx) {
    luaL_checktype(L, idx, LUA_TBOOLEAN);
    return lua_toboolean(L, idx) != 0;
}

// --- Names (a closed set: an enum's names, ADR-0030) ---------------------------
//
// The index of argument `idx` in `names` (the array its descriptor publishes,
// core/name_index.hpp). An unknown name is a programming mistake: it raises,
// naming the argument and the name ("unknown mode 'fwd'").

template <size_t N>
int checkName(lua_State* L, int idx, const char* const (&names)[N], const char* what) {
    const char* s = luaL_checkstring(L, idx);
    const int i = nameIndex(s, names);
    if (i < 0) luaL_argerror(L, idx, lua_pushfstring(L, "unknown %s '%s'", what, s));
    return i;
}
// checkName, or `def` when the argument is absent or nil.
template <size_t N>
int optName(lua_State* L, int idx, const char* const (&names)[N], const char* what, int def) {
    return lua_isnoneornil(L, idx) ? def : checkName(L, idx, names, what);
}

// --- Option-table fields -----------------------------------------------------
//
// An integer field of the table at stack index `table`, for options tables
// such as `{ x = 1, y = 2 }`. A field that is present must be an integer
// (luaL_checkinteger raises otherwise). The stack is left balanced.

// table[key]; `def` when the field is nil, or when table is 0 (no table given).
inline int optFieldInt(lua_State* L, int table, const char* key, int def) {
    if (table == 0) return def;
    lua_getfield(L, table, key);
    const int v = lua_isnil(L, -1) ? def : static_cast<int>(luaL_checkinteger(L, -1));
    lua_pop(L, 1);
    return v;
}
// table[key] as a name of `names` (checkName's rule); `def` when the field is
// nil, or when table is 0.
template <size_t N>
int optFieldName(lua_State* L, int table, const char* key, const char* const (&names)[N],
                 const char* what, int def) {
    if (table == 0) return def;
    lua_getfield(L, table, key);
    int v = def;
    if (!lua_isnil(L, -1)) {
        const char* s = lua_tostring(L, -1);
        v = s ? nameIndex(s, names) : -1;
        if (v < 0) luaL_error(L, "bad field '%s' (unknown %s '%s')", key, what, s ? s : luaL_typename(L, -1));
    }
    lua_pop(L, 1);
    return v;
}
// table[key], which must be there.
inline int checkFieldInt(lua_State* L, int table, const char* key) {
    lua_getfield(L, table, key);
    const int v = static_cast<int>(luaL_checkinteger(L, -1));
    lua_pop(L, 1);
    return v;
}

// A world failure (ADR-0030): push nil, err and return their count, as
// `return lua::returnErr(L, "pool full");`. err is a short lower-case message.
// A programming mistake raises instead (luaL_error / luaL_argerror).
inline int returnErr(lua_State* L, const char* err) {
    lua_pushnil(L);
    lua_pushstring(L, err);
    return 2;
}

} // namespace enjin2::lua
