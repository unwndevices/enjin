#pragma once

// Lua representation of a tilemap: the scene-free map handle that
// engine.tilemap.load returns (Tomodachi #256), plus the lookup every tilemap
// consumer shares — the map's own methods, colliders:addSolidRects, and a
// host's gfx.setTilemap restore source.
//
// The handle is a full userdata that IS a C_Tilemap with no owner Object: Lua
// owns it, no scene holds it, and __gc runs its destructor. A scene component
// (obj:add("C_Tilemap")) is still reachable as a ComponentProxy; toTilemap
// accepts both.
//
// Header-only and engine-owned (like effect_lua.hpp) so a host that does not
// link enjin's LuaBindings can still resolve a map argument.

#include <new>

#include "../components/tilemap.hpp"
#include "component_proxy.hpp"
#include "lua_api.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {
namespace lua {

/// Metatable of the scene-free map handle.
inline const char* tilemapMt() { return "enjin2.Tilemap"; }
/// Metatable of a scene C_Tilemap's ComponentProxy (the one definition;
/// bindings_internal.hpp's CTILEMAP_PROXY_METATABLE is this).
constexpr const char* tilemapProxyMt() { return "C_Tilemap_Proxy"; }

inline int tilemapGc(lua_State* L) {
    static_cast<C_Tilemap*>(lua_touserdata(L, 1))->~C_Tilemap();
    return 0;
}

/// Create the handle metatable (with __gc) if this state lacks it. The
/// bindings add the method table (__index) on top.
inline void registerTilemapMetatable(lua_State* L) {
    static constexpr LuaApiEntry kGc[] = {
        luaFunction("__gc", tilemapGc, "(map:Tilemap) -> nil",
                    "Free the map's cells when Lua collects the handle.",
                    "map: the map handle"),
    };
    static constexpr LuaApiModule kGcModule =
        luaApiModule(LuaApiScope::Metatable, "Tilemap", "Map lifetime.", kGc);
    if (luaL_newmetatable(L, tilemapMt())) {
        luaApiSetFields(L, -1, kGcModule);
    }
    lua_pop(L, 1);
}

/// Push a new, empty map handle and return the C_Tilemap it owns.
inline C_Tilemap* pushTilemap(lua_State* L) {
    registerTilemapMetatable(L);
    void* p = lua_newuserdata(L, sizeof(C_Tilemap));
    auto* tm = new (p) C_Tilemap(nullptr);
    luaL_setmetatable(L, tilemapMt());
    return tm;
}

/// The tilemap at `idx` — a map handle or a live scene C_Tilemap — or nullptr
/// for any other value, including a proxy whose component was destroyed.
inline C_Tilemap* toTilemap(lua_State* L, int idx) {
    if (void* p = luaL_testudata(L, idx, tilemapMt())) {
        return static_cast<C_Tilemap*>(p);
    }
    auto* proxy = static_cast<ComponentProxy*>(luaL_testudata(L, idx, tilemapProxyMt()));
    if (proxy && proxy->valid && proxy->component) {
        return static_cast<C_Tilemap*>(proxy->component);
    }
    return nullptr;
}

/// Like toTilemap, but raises a Lua error instead of returning nullptr.
inline C_Tilemap* checkTilemap(lua_State* L, int idx) {
    if (C_Tilemap* tm = toTilemap(L, idx)) return tm;
    if (luaL_testudata(L, idx, tilemapProxyMt())) {
        luaL_error(L, "component has been destroyed");
    }
    luaL_typeerror(L, idx, "tilemap");
    return nullptr;  // unreachable — both raise
}

} // namespace lua
} // namespace enjin2
