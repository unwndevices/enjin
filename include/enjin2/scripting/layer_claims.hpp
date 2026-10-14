/**
 * @file layer_claims.hpp
 * @brief Applet layers claimed by a retained owner (ADR-0016, Tomodachi #331)
 *
 * A Shape group owns its compositor layer: the layer is its cache, so nothing
 * else may draw on it. The claim lives in the Lua registry (one bit per
 * 0-based layer slot), so it dies with the VM, and both Enjin's immediate gfx.*
 * drawing and a host's own drawing bindings can check it with only lua.h.
 * gfx.setLayer / gfx.clearLayer / immediate drawing on a claimed layer raise;
 * per-layer state (backdrop, tint, visibility) stays the host's to set.
 */
#pragma once

#include <cstdint>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {
namespace lua {

inline constexpr const char* kLayerClaimsKey = "enjin2.layerClaims";

/// Bit i set: 0-based layer slot i is claimed.
inline uint32_t layerClaims(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, kLayerClaimsKey);
    const uint32_t bits = static_cast<uint32_t>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return bits;
}

inline bool layerClaimed(lua_State* L, int slot)
{
    return slot >= 0 && slot < 32 && ((layerClaims(L) >> slot) & 1u);
}

inline void setLayerClaim(lua_State* L, int slot, bool claimed)
{
    if (slot < 0 || slot >= 32)
        return;
    uint32_t bits = layerClaims(L);
    bits = claimed ? (bits | (1u << slot)) : (bits & ~(1u << slot));
    lua_pushinteger(L, static_cast<lua_Integer>(bits));
    lua_setfield(L, LUA_REGISTRYINDEX, kLayerClaimsKey);
}

/// Raise if 0-based layer `slot` is claimed: `what` names the call.
inline void checkLayerUnclaimed(lua_State* L, int slot, const char* what)
{
    if (layerClaimed(L, slot))
        luaL_error(L, "%s: layer %d is drawn by a Shape group; draw on another layer", what, slot + 1);
}

} // namespace lua
} // namespace enjin2
