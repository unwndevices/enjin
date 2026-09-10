#pragma once
#include <cstddef>

namespace enjin2 {

class Object;
class Component;

// ── Source-of-truth component registry (ADR-0003 §2) ────────────────────────
//
// The SINGLE place that knows which components are attachable from Lua. Adding a
// row here makes a component both get()-able and add()-able from Lua with no
// other glue: the string-dispatch in the object/script proxies is *generated*
// from this list, so no Lua-only component helper can exist. The parity test
// iterates componentRegistry() to assert every row round-trips.
//
//   X(Type, ProxyMetatable)
// The Lua type name is the stringized C++ type (#Type == "C_Position"), so each
// row carries no redundant name column that could drift out of sync.
#define ENJIN2_COMPONENT_LIST(X)              \
    X(C_Position,     "C_Position_Proxy")     \
    X(C_Timer,        "C_Timer_Proxy")        \
    X(C_StateMachine, "C_StateMachine_Proxy") \
    X(C_Tilemap,      "C_Tilemap_Proxy")      \
    X(C_Camera,       "C_Camera_Proxy")        \
    X(C_Sprite,       "C_Sprite_Proxy")         \
    X(C_Body,         "C_Body_Proxy")

/// One registry row. `get`/`add` are generated thunks over
/// Object::getComponent<T> / Object::addComponent<T>, so the string→type mapping
/// lives in exactly one place (ENJIN2_COMPONENT_LIST).
struct ComponentRegistryEntry {
    const char* luaName;    ///< Lua type string, e.g. "C_Position".
    const char* proxyMeta;  ///< Per-type ComponentProxy metatable name.
    Component* (*get)(Object*);  ///< Fetch the component (nullptr if absent).
    Component* (*add)(Object*);  ///< Attach the component (nullptr if it can't).
};

/// The registry table (generated from ENJIN2_COMPONENT_LIST). Never null;
/// writes the row count to `countOut`.
const ComponentRegistryEntry* componentRegistry(std::size_t& countOut);

/// Look up a row by Lua type name, or nullptr if the name is not registered.
const ComponentRegistryEntry* findComponentEntry(const char* luaName);

} // namespace enjin2
