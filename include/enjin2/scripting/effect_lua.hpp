#pragma once

// Lua surface for the index-shader data model: gfx.remap / gfx.mask /
// gfx.effect constructors that return typed userdata wrapping the engine
// structs enjin2::Remap / enjin2::Mask / enjin2::Effect. The apply sites
// (gfx.shade on a compositor layer, the shaded sprite blit) consume that
// userdata via checkEffect / checkRemap.
//
// This is header-only and engine-owned so every host — the WASM sim's enjin
// bindings and libtomo's compositor surface alike — shares one representation
// and one decode, instead of re-decoding tables at each call site.

#include <cstdint>
#include <cstring>

#include "../graphics/effect.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {
namespace lua {

/// Metatable names — bare type tags for luaL_checkudata (no methods attached).
inline const char* remapMt() { return "enjin2.Remap"; }
inline const char* maskMt() { return "enjin2.Mask"; }
inline const char* effectMt() { return "enjin2.Effect"; }

// --- push / check helpers -------------------------------------------------

inline void pushRemap(lua_State* L, const Remap& r) {
    void* p = lua_newuserdata(L, sizeof(Remap));
    *static_cast<Remap*>(p) = r;
    luaL_setmetatable(L, remapMt());
}
inline Remap checkRemap(lua_State* L, int idx) {
    return *static_cast<Remap*>(luaL_checkudata(L, idx, remapMt()));
}
inline void pushMask(lua_State* L, const Mask& m) {
    void* p = lua_newuserdata(L, sizeof(Mask));
    *static_cast<Mask*>(p) = m;
    luaL_setmetatable(L, maskMt());
}
inline Mask checkMask(lua_State* L, int idx) {
    return *static_cast<Mask*>(luaL_checkudata(L, idx, maskMt()));
}
inline void pushEffect(lua_State* L, const Effect& e) {
    void* p = lua_newuserdata(L, sizeof(Effect));
    *static_cast<Effect*>(p) = e;
    luaL_setmetatable(L, effectMt());
}
inline Effect checkEffect(lua_State* L, int idx) {
    return *static_cast<Effect*>(luaL_checkudata(L, idx, effectMt()));
}

namespace detail {

inline uint8_t argU8(lua_State* L, int idx) {
    return static_cast<uint8_t>(luaL_checkinteger(L, idx));
}
inline int16_t argI16(lua_State* L, int idx, int16_t def) {
    return lua_isnoneornil(L, idx) ? def
                                   : static_cast<int16_t>(luaL_checkinteger(L, idx));
}

/// gfx.remap(...) — build a Remap.
///   remap(table16)                  raw 16-entry lut (0-based Lua 1..16)
///   remap("identity"|"lighten"|"darken")
///   remap("solid", index)
///   remap("recolor", from, to)
///   remap("onRamp", rampId, innerRemap)
///   remap("compose", first, second)
inline int lua_remap(lua_State* L) {
    if (lua_istable(L, 1)) {
        Remap r;
        for (int i = 0; i < 16; ++i) {
            lua_rawgeti(L, 1, i + 1);  // Lua arrays are 1-based
            r.lut[i] = static_cast<uint8_t>(lua_tointeger(L, -1) & 0x0F);
            lua_pop(L, 1);
        }
        pushRemap(L, r);
        return 1;
    }
    const char* kind = luaL_checkstring(L, 1);
    if (std::strcmp(kind, "identity") == 0) {
        pushRemap(L, Remap::identity());
    } else if (std::strcmp(kind, "lighten") == 0) {
        pushRemap(L, Remap::lighten());
    } else if (std::strcmp(kind, "darken") == 0) {
        pushRemap(L, Remap::darken());
    } else if (std::strcmp(kind, "solid") == 0) {
        pushRemap(L, Remap::solid(argU8(L, 2)));
    } else if (std::strcmp(kind, "recolor") == 0) {
        pushRemap(L, Remap::recolor(argU8(L, 2), argU8(L, 3)));
    } else if (std::strcmp(kind, "onRamp") == 0) {
        pushRemap(L, Remap::onRamp(argU8(L, 2), checkRemap(L, 3)));
    } else if (std::strcmp(kind, "compose") == 0) {
        pushRemap(L, Remap::compose(checkRemap(L, 2), checkRemap(L, 3)));
    } else {
        return luaL_error(L, "gfx.remap: unknown kind '%s'", kind);
    }
    return 1;
}

/// gfx.mask(...) — build a Mask.
///   mask("full")
///   mask("stripe", period [, on])
///   mask("hlines", period [, thickness])
///   mask("bayer", level)
inline int lua_mask(lua_State* L) {
    const char* kind = luaL_checkstring(L, 1);
    if (std::strcmp(kind, "full") == 0) {
        pushMask(L, Mask::full());
    } else if (std::strcmp(kind, "stripe") == 0) {
        pushMask(L, Mask::stripe(argU8(L, 2),
                                 lua_isnoneornil(L, 3) ? 1 : argU8(L, 3)));
    } else if (std::strcmp(kind, "hlines") == 0) {
        pushMask(L, Mask::hlines(argU8(L, 2),
                                 lua_isnoneornil(L, 3) ? 1 : argU8(L, 3)));
    } else if (std::strcmp(kind, "bayer") == 0) {
        pushMask(L, Mask::bayer(argU8(L, 2)));
    } else {
        return luaL_error(L, "gfx.mask: unknown kind '%s'", kind);
    }
    return 1;
}

/// gfx.effect(...) — build an Effect.
///   effect(mask, remap [, phaseX [, phaseY]])
///   effect("holo" [, phase]) | effect("dim") | effect("fade", level)
///   effect("scanline") | effect("ghost" [, dx [, dy]])
inline int lua_effect(lua_State* L) {
    if (lua_isstring(L, 1) && !lua_isnumber(L, 1)) {
        const char* preset = luaL_checkstring(L, 1);
        if (std::strcmp(preset, "holo") == 0) {
            pushEffect(L, Effect::holo(argI16(L, 2, 0)));
        } else if (std::strcmp(preset, "dim") == 0) {
            pushEffect(L, Effect::dim());
        } else if (std::strcmp(preset, "fade") == 0) {
            pushEffect(L, Effect::fade(argU8(L, 2)));
        } else if (std::strcmp(preset, "scanline") == 0) {
            pushEffect(L, Effect::scanline());
        } else if (std::strcmp(preset, "ghost") == 0) {
            pushEffect(L, Effect::ghost(argI16(L, 2, 1), argI16(L, 3, 1)));
        } else {
            return luaL_error(L, "gfx.effect: unknown preset '%s'", preset);
        }
        return 1;
    }
    const Mask m = checkMask(L, 1);
    const Remap r = checkRemap(L, 2);
    pushEffect(L, Effect(m, r, argI16(L, 3, 0), argI16(L, 4, 0)));
    return 1;
}

}  // namespace detail

/// @brief Register `gfx.remap` / `gfx.mask` / `gfx.effect` on the global `gfx`
///        table (creating it if absent) and ensure the userdata metatables.
///
/// Idempotent: safe to call again after a Lua-state reload. Only the pure
/// constructors live here; the apply sites (`gfx.shade`, shaded sprite blit)
/// are registered by their owning host.
inline void registerEffectApi(lua_State* L) {
    luaL_newmetatable(L, remapMt());
    lua_pop(L, 1);
    luaL_newmetatable(L, maskMt());
    lua_pop(L, 1);
    luaL_newmetatable(L, effectMt());
    lua_pop(L, 1);

    lua_getglobal(L, "gfx");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    lua_pushcfunction(L, detail::lua_remap);
    lua_setfield(L, -2, "remap");
    lua_pushcfunction(L, detail::lua_mask);
    lua_setfield(L, -2, "mask");
    lua_pushcfunction(L, detail::lua_effect);
    lua_setfield(L, -2, "effect");
    lua_setglobal(L, "gfx");
}

}  // namespace lua
}  // namespace enjin2
