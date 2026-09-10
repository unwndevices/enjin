/**
 * @file bindings_numerals.cpp
 * @brief Lua surface for the HUD numerals (ADR-0003 §8, Tomodachi #83).
 *
 * gfx.number / gfx.timer are immediate-draw functions (they need the active
 * canvas), so they live here beside the other gfx blits and reuse the shared
 * glyph layout in numerals.cpp — the same frames the C++ drawNumber selects.
 * engine.hud.rollingCounter / engine.hud.timer construct the value-object
 * userdata defined header-only in hud_lua.hpp.
 */
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"
#include "../../include/enjin2/scripting/hud_lua.hpp"
#include "../../include/enjin2/graphics/numerals.hpp"

#include <cstring>

namespace enjin2 {

namespace {

int optInt(lua_State* L, int t, const char* k, int def) {
    lua_getfield(L, t, k);
    const int v = lua_isnumber(L, -1) ? static_cast<int>(lua_tointeger(L, -1)) : def;
    lua_pop(L, 1);
    return v;
}

bool optBool(lua_State* L, int t, const char* k, bool def) {
    lua_getfield(L, t, k);
    const bool v = lua_isnil(L, -1) ? def : (lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
    return v;
}

NumberAlign optAlign(lua_State* L, int t) {
    lua_getfield(L, t, "align");
    NumberAlign a = NumberAlign::Left;
    if (lua_isstring(L, -1)) {
        const char* s = lua_tostring(L, -1);
        if (std::strcmp(s, "right") == 0) {
            a = NumberAlign::Right;
        } else if (std::strcmp(s, "center") == 0) {
            a = NumberAlign::Center;
        }
    }
    lua_pop(L, 1);
    return a;
}

// Blit a run of glyph frames through the type-erased LuaCanvas, mirroring the
// no-flip drawSprite path. Returns the field pixel width.
int blitGlyphRun(LuaCanvas* canvas, const SpriteSheet& sheet,
                 int16_t x, int16_t y, const uint16_t* frames, size_t count,
                 NumberAlign align, uint8_t spacing) {
    const int width = numeralFieldWidth(sheet.cellW, count, spacing);
    if (!canvas || !sheet.data) return width;

    const int16_t cellW = static_cast<int16_t>(sheet.cellW);
    const int16_t cellH = static_cast<int16_t>(sheet.cellH);
    const int16_t step = static_cast<int16_t>(sheet.cellW + spacing);
    int16_t cx = numeralStartX(x, width, align);

    for (size_t i = 0; i < count; ++i) {
        const uint16_t frame = frames[i];
        if (frame != kNumeralBlank && frame < sheet.frameCount()) {
            const uint8_t* fd = sheet.data +
                static_cast<uint32_t>(frame) * sheet.cellW * sheet.cellH;
            for (int16_t fy = 0; fy < cellH; ++fy) {
                for (int16_t fx = 0; fx < cellW; ++fx) {
                    const uint8_t px = fd[fy * cellW + fx] & 0x0F;
                    if (px != 15) {  // index 15 = transparent
                        canvas->setPixel(static_cast<int16_t>(cx + fx),
                                         static_cast<int16_t>(y + fy), px);
                    }
                }
            }
        }
        cx = static_cast<int16_t>(cx + step);
    }
    return width;
}

} // namespace

// gfx.number(x, y, value, {strip, pad, align, sep, spacing, padZeros}) -> width
int LuaBindings::lua_number(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, 0); return 1; }

    const int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 1));
    const int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 2));
    const lua_Integer raw = luaL_checkinteger(L, 3);
    const uint32_t value = raw < 0 ? 0u : static_cast<uint32_t>(raw);
    luaL_checktype(L, 4, LUA_TTABLE);

    // Resolve opts.strip → live sheet (member scope has private-pool access).
    lua_getfield(L, 4, "strip");
    if (!lua_isnumber(L, -1)) {
        lua_pop(L, 1);
        return luaL_error(L, "gfx.number: opts.strip must be a sprite handle");
    }
    const int handle = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) {
        return luaL_error(L, "gfx.number: invalid or unloaded strip handle %d", handle);
    }
    const SpriteSheet& strip = b->spritePool[handle].sheet;

    NumberStyle style;
    style.pad      = static_cast<uint8_t>(optInt(L, 4, "pad", 0));
    style.padZeros = optBool(L, 4, "padZeros", true);
    style.align    = optAlign(L, 4);
    style.sep      = static_cast<int16_t>(optInt(L, 4, "sep", -1));
    style.spacing  = static_cast<uint8_t>(optInt(L, 4, "spacing", 0));

    uint16_t frames[kNumeralMaxCells];
    const size_t count = buildNumberGlyphs(frames, kNumeralMaxCells, value, style);
    const int width = blitGlyphRun(b->currentCanvas, strip, x, y, frames, count,
                                   style.align, style.spacing);

    lua_pushinteger(L, width);
    return 1;
}

// gfx.timer(x, y, ms, {strip, align, spacing}) -> width
int LuaBindings::lua_timer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, 0); return 1; }

    const int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 1));
    const int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 2));
    const lua_Integer rawMs = luaL_checkinteger(L, 3);
    const uint32_t ms = rawMs < 0 ? 0u : static_cast<uint32_t>(rawMs);
    luaL_checktype(L, 4, LUA_TTABLE);

    lua_getfield(L, 4, "strip");
    if (!lua_isnumber(L, -1)) {
        lua_pop(L, 1);
        return luaL_error(L, "gfx.timer: opts.strip must be a sprite handle");
    }
    const int handle = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) {
        return luaL_error(L, "gfx.timer: invalid or unloaded strip handle %d", handle);
    }
    const SpriteSheet& strip = b->spritePool[handle].sheet;

    const NumberAlign align = optAlign(L, 4);
    const uint8_t spacing = static_cast<uint8_t>(optInt(L, 4, "spacing", 0));

    uint16_t frames[kNumeralMaxCells];
    const size_t count = buildTimerGlyphs(frames, kNumeralMaxCells, ms);
    const int width = blitGlyphRun(b->currentCanvas, strip, x, y, frames, count,
                                   align, spacing);

    lua_pushinteger(L, width);
    return 1;
}

// engine.hud.* — HUD numeral value objects (RollingCounter, Timer).
void LuaBindings::registerHudSubtable(lua_State* L) {
    enjin2::lua::ensureHudMetatables(L);
    lua_newtable(L);
    static const LuaFuncDef kHudFuncs[] = {
        {"rollingCounter", enjin2::lua::detail::lua_newRollingCounter},
        {"timer",          enjin2::lua::detail::lua_newTimer},
    };
    luaBindFunctions(L, -1, kHudFuncs, ENJIN_ARRAY_LEN(kHudFuncs));
    lua_setfield(L, -2, "hud");
}

} // namespace enjin2
