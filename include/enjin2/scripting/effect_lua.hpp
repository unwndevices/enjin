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

#include "../core/name_index.hpp"
#include "../graphics/effect.hpp"
#include "lua_api.hpp"

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

// The kind / preset names each constructor dispatches on, indexed by the enum
// beside it and published as that enum in the descriptors below.
enum class RemapKind : uint8_t { Identity, Lighten, Darken, Solid, Recolor, OnRamp, Compose };
inline constexpr const char* kRemapKindNames[] = {
    "identity", "lighten", "darken", "solid", "recolor", "onRamp", "compose",
};
enum class MaskKind : uint8_t { Full, Stripe, Hlines, Bayer };
inline constexpr const char* kMaskKindNames[] = {"full", "stripe", "hlines", "bayer"};
enum class EffectPreset : uint8_t { Holo, Dim, Fade, Scanline, Ghost };
inline constexpr const char* kEffectPresetNames[] = {"holo", "dim", "fade", "scanline", "ghost"};

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
    const int k = nameIndex(kind, kRemapKindNames);
    if (k < 0) return luaL_error(L, "gfx.remap: unknown kind '%s'", kind);
    switch (static_cast<RemapKind>(k)) {
        case RemapKind::Identity: pushRemap(L, Remap::identity()); break;
        case RemapKind::Lighten:  pushRemap(L, Remap::lighten()); break;
        case RemapKind::Darken:   pushRemap(L, Remap::darken()); break;
        case RemapKind::Solid:    pushRemap(L, Remap::solid(argU8(L, 2))); break;
        case RemapKind::Recolor:  pushRemap(L, Remap::recolor(argU8(L, 2), argU8(L, 3))); break;
        case RemapKind::OnRamp:   pushRemap(L, Remap::onRamp(argU8(L, 2), checkRemap(L, 3))); break;
        case RemapKind::Compose:
            pushRemap(L, Remap::compose(checkRemap(L, 2), checkRemap(L, 3)));
            break;
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
    const int k = nameIndex(kind, kMaskKindNames);
    if (k < 0) return luaL_error(L, "gfx.mask: unknown kind '%s'", kind);
    switch (static_cast<MaskKind>(k)) {
        case MaskKind::Full:
            pushMask(L, Mask::full());
            break;
        case MaskKind::Stripe:
            pushMask(L, Mask::stripe(argU8(L, 2), lua_isnoneornil(L, 3) ? 1 : argU8(L, 3)));
            break;
        case MaskKind::Hlines:
            pushMask(L, Mask::hlines(argU8(L, 2), lua_isnoneornil(L, 3) ? 1 : argU8(L, 3)));
            break;
        case MaskKind::Bayer:
            pushMask(L, Mask::bayer(argU8(L, 2)));
            break;
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
        const int p = nameIndex(preset, kEffectPresetNames);
        if (p < 0) return luaL_error(L, "gfx.effect: unknown preset '%s'", preset);
        switch (static_cast<EffectPreset>(p)) {
            case EffectPreset::Holo:     pushEffect(L, Effect::holo(argI16(L, 2, 0))); break;
            case EffectPreset::Dim:      pushEffect(L, Effect::dim()); break;
            case EffectPreset::Fade:     pushEffect(L, Effect::fade(argU8(L, 2))); break;
            case EffectPreset::Scanline: pushEffect(L, Effect::scanline()); break;
            case EffectPreset::Ghost:
                pushEffect(L, Effect::ghost(argI16(L, 2, 1), argI16(L, 3, 1)));
                break;
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
    static constexpr LuaApiEnum kRemapKind = luaApiEnum("RemapKind", detail::kRemapKindNames);
    static constexpr LuaApiEnum kMaskKind = luaApiEnum("MaskKind", detail::kMaskKindNames);
    static constexpr LuaApiEnum kEffectPreset =
        luaApiEnum("EffectPreset", detail::kEffectPresetNames);
    static constexpr LuaApiEntry kEffectApi[] = {
        luaFunction("remap", detail::lua_remap,
                    "(lut:table) -> Remap\n"
                    "(kind:RemapKind) -> Remap\n"
                    "(kind:RemapKind, index:int) -> Remap\n"
                    "(kind:RemapKind, from:int, to:int) -> Remap\n"
                    "(kind:RemapKind, rampId:int, inner:Remap) -> Remap\n"
                    "(kind:RemapKind, first:Remap, second:Remap) -> Remap",
                    "Build a colour remap: a 16-entry table or a named kind.",
                    "lut: lut[i + 1] is the new index for colour i; a missing entry maps to 0\n"
                    "kind: identity, lighten or darken alone; solid, recolor, onRamp and "
                    "compose take the arguments below; unknown kinds raise\n"
                    "index: with \"solid\", the colour every pixel becomes\n"
                    "from: with \"recolor\", the colour to replace\n"
                    "to: with \"recolor\", its replacement\n"
                    "rampId: with \"onRamp\", the ramp the inner remap is confined to\n"
                    "inner: with \"onRamp\", the remap to apply on that ramp\n"
                    "first: with \"compose\", the remap applied first\n"
                    "second: with \"compose\", the remap applied to its result")
            .withEnum(kRemapKind),
        luaFunction("mask", detail::lua_mask,
                    "(kind:MaskKind) -> Mask\n"
                    "(kind:MaskKind, period:int, on:int?=1) -> Mask\n"
                    "(kind:MaskKind, period:int, thickness:int?=1) -> Mask\n"
                    "(kind:MaskKind, level:int) -> Mask",
                    "Build a pixel mask selecting where an effect applies.",
                    "kind: full alone; stripe (diagonal), hlines and bayer take the arguments "
                    "below; unknown kinds raise\n"
                    "period: with \"stripe\" or \"hlines\", the repeat in pixels, 1..32\n"
                    "on: with \"stripe\", pixels on per period, 0..period\n"
                    "thickness: with \"hlines\", line thickness, 0..period\n"
                    "level: with \"bayer\", ordered-dither density, 0..16")
            .withEnum(kMaskKind),
        luaFunction("effect", detail::lua_effect,
                    "(mask:Mask, remap:Remap, phaseX:int?=0, phaseY:int?=0) -> Effect\n"
                    "(preset:EffectPreset, phase:int?=0) -> Effect\n"
                    "(preset:EffectPreset, level:int) -> Effect\n"
                    "(preset:EffectPreset, dx:int?=1, dy:int?=1) -> Effect",
                    "Build an index shader for gfx.drawSprite: a mask and remap, or a preset.",
                    "mask: where the remap applies\n"
                    "remap: the colour change\n"
                    "phaseX: added to x before the mask is sampled\n"
                    "phaseY: added to y before the mask is sampled\n"
                    "preset: holo (with phase), dim, fade (with level), scanline or ghost "
                    "(with dx, dy); unknown presets raise\n"
                    "phase: with \"holo\", the stripe offset\n"
                    "level: with \"fade\", 0..16\n"
                    "dx: with \"ghost\", horizontal offset\n"
                    "dy: with \"ghost\", vertical offset")
            .withEnum(kEffectPreset),
    };
    static constexpr LuaApiModule kEffectModule = luaApiModule(
        LuaApiScope::Table, "gfx", "Index-shader constructors (remap, mask, effect).", kEffectApi);

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
    luaApiSetFields(L, -1, kEffectModule);
    lua_setglobal(L, "gfx");
}

}  // namespace lua
}  // namespace enjin2
