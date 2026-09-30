// src/scripting/bindings_internal.hpp
// Private inter-TU declarations for enjin2_lua — NOT a public install header
#pragma once
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/component_proxy.hpp"
#include "../../include/enjin2/scripting/tilemap_lua.hpp"
#include "../../include/enjin2/scripting/lua_api.hpp"
#include "../../include/enjin2/core/name_index.hpp"

namespace enjin2 {

// Metatable name constants — shared by bindings.cpp and bindings_proxy.cpp
static constexpr const char* PROXY_METATABLE           = "ScriptProxy";
static constexpr const char* CPOSITION_PROXY_METATABLE = "C_Position_Proxy";
static constexpr const char* CTIMER_PROXY_METATABLE    = "C_Timer_Proxy";
static constexpr const char* CFSM_PROXY_METATABLE      = "C_StateMachine_Proxy";
static constexpr const char* CTILEMAP_PROXY_METATABLE  = lua::tilemapProxyMt();
static constexpr const char* CCAMERA_PROXY_METATABLE   = "C_Camera_Proxy";
static constexpr const char* CSPRITE_PROXY_METATABLE   = "C_Sprite_Proxy";
static constexpr const char* CBODY_PROXY_METATABLE     = "C_Body_Proxy";
static constexpr const char* COLLIDERSET_PROXY_METATABLE = "ColliderSet_Proxy";
static constexpr const char* OBJECT_PROXY_METATABLE    = "ObjectProxy";

// Draw modes of gfx.rectangle/circle/triangle, indexed by ShapeMode and
// published as the ShapeMode enum. "fill" fills; any other name (canonically
// "line") draws the outline.
enum class ShapeMode : uint8_t { Fill, Line };
inline constexpr const char* kShapeModeNames[] = {"fill", "line"};

inline bool isFillMode(const char* mode) {
    return nameIndex(mode, kShapeModeNames) == static_cast<int>(ShapeMode::Fill);
}

// A proxy type's methods live in a registry table built from its Methods
// descriptors (the Vec2 pattern), which its __index consults before the
// type's properties. `registryKey` names that table ("C_Timer.methods").
inline void setProxyMethods(lua_State* L, const char* registryKey, const LuaApiModule& methods) {
    lua_newtable(L);
    luaApiSetFields(L, -1, methods);
    lua_setfield(L, LUA_REGISTRYINDEX, registryKey);
}

// Push the method named by the key at `keyIdx`; false (nothing pushed) when
// the type has no such method.
inline bool pushProxyMethod(lua_State* L, const char* registryKey, int keyIdx) {
    lua_getfield(L, LUA_REGISTRYINDEX, registryKey);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }
    lua_pushvalue(L, keyIdx);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    if (lua_isnil(L, -1)) { lua_pop(L, 1); return false; }
    return true;
}

class Object;

// Registry-generated component attach/fetch, shared by the ScriptProxy (self)
// and ObjectProxy (spawn/find) `add`/`get` verbs. Both push exactly one Lua
// value (a ComponentProxy userdata, or nil) and return 1. `pushComponentAdd`
// is attach-or-fetch: it returns the existing component when one is present so
// a second add never duplicates a singleton like C_Position.
//
// `paramsIdx` is the absolute stack index of an optional params table (0 = none)
// applied field-by-field through the new proxy's __newindex, so
// add("C_Position", {x=5, y=6}) routes to the C++ setters. Fields a component's
// proxy does not write are silently ignored.
int pushComponentGet(lua_State* L, Object* owner, const char* typeName);
int pushComponentAdd(lua_State* L, Object* owner, const char* typeName, int paramsIdx);

class ColliderSet;

// Wrap a scene's collider set in a ColliderSet userdata (or push nil). Used by
// the engine.scene.colliders() binding in bindings_engine.cpp (ADR-0003 §4).
int pushColliderSetProxy(lua_State* L, ColliderSet* set);

} // namespace enjin2
