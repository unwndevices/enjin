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
static constexpr const char* OBJECT_PROXY_METATABLE    = "ObjectProxy";

} // namespace enjin2
