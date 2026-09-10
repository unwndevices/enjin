// src/scripting/bindings_internal.hpp
// Private inter-TU declarations for enjin2_lua — NOT a public install header
#pragma once
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/component_proxy.hpp"

namespace enjin2 {

// Metatable name constants — shared by bindings.cpp and bindings_proxy.cpp
static constexpr const char* PROXY_METATABLE           = "ScriptProxy";
static constexpr const char* CPOSITION_PROXY_METATABLE = "C_Position_Proxy";
static constexpr const char* CTIMER_PROXY_METATABLE    = "C_Timer_Proxy";
static constexpr const char* CFSM_PROXY_METATABLE      = "C_StateMachine_Proxy";
static constexpr const char* CTILEMAP_PROXY_METATABLE  = "C_Tilemap_Proxy";
static constexpr const char* CCAMERA_PROXY_METATABLE   = "C_Camera_Proxy";
static constexpr const char* CSPRITE_PROXY_METATABLE   = "C_Sprite_Proxy";
static constexpr const char* OBJECT_PROXY_METATABLE    = "ObjectProxy";

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

} // namespace enjin2
