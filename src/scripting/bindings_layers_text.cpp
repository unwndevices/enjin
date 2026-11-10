#include "bindings_internal.hpp"
#include "../../include/enjin2/scripting/layer_claims.hpp"
#include "../../include/enjin2/graphics/faces/faces.hpp"
#include "../../include/enjin2/core/name_index.hpp"
#include "../../include/enjin2/scripting/effect_lua.hpp"
#include "../../include/enjin2/scripting/palette_arg.hpp"

namespace enjin2 {

// Applet-facing indices are 1-based; the final compositor layer is shell UI.
static inline int checkLayerIdx(lua_State* L, int arg, int layerCount) {
    const lua_Integer layer = luaL_checkinteger(L, arg);
    if (layer < 1 || layer > layerCount)
        luaL_argerror(L, arg, "applet layer must be 1..3");
    return static_cast<int>(layer - 1);
}

//==============================================================================
// Layer System Bindings (LAYER-06)
//==============================================================================

// setLayer(n)  — Switch the active canvas to layer n (Lua 1-indexed)
int LuaBindings::lua_setLayer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0) return 0;

    int cpp_idx = checkLayerIdx(L, 1, b->layerCount);
    lua::checkLayerUnclaimed(L, cpp_idx, "gfx.setLayer");

    b->activeLayer   = static_cast<uint8_t>(cpp_idx);
    b->currentCanvas = b->layerCanvases[cpp_idx];
    return 0;
}

// getLayer()  — Return current active layer as 1-indexed Lua integer
int LuaBindings::lua_getLayer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, b->activeLayer + 1);
    return 1;
}

// clearLayer([n [, color]])  — Clear only the specified layer buffer (1-indexed;
// without n, the current layer; color optional, defaults to 0)
int LuaBindings::lua_clearLayer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0) return 0;

    int cpp_idx = lua_isnoneornil(L, 1)
        ? static_cast<int>(b->activeLayer)
        : checkLayerIdx(L, 1, b->layerCount);
    lua::checkLayerUnclaimed(L, cpp_idx, "gfx.clearLayer");

    LuaCanvas* target = b->layerCanvases[cpp_idx];
    uint8_t color = lua::optCanvasColour(L, 2, ROLE_BG, indexedColours(target));
    if (target) {
        target->clear(color);
    }
    return 0;
}

// getLayerCount()  — Return the number of layers available
int LuaBindings::lua_getLayerCount(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, b->layerCount);
    return 1;
}

// setLayerVisible(n, visible)  — Set compositor visibility for layer n (1-indexed)
int LuaBindings::lua_setLayerVisible(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0 || !b->layerVisible) return 0;

    int cpp_idx = checkLayerIdx(L, 1, b->layerCount);

    b->layerVisible[cpp_idx] = (lua_toboolean(L, 2) != 0);
    return 0;
}

// isLayerVisible(n)  — Return whether layer n (1-indexed) is visible
int LuaBindings::lua_isLayerVisible(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0 || !b->layerVisible) {
        lua_pushboolean(L, 1);  // default: visible
        return 1;
    }

    int cpp_idx = checkLayerIdx(L, 1, b->layerCount);

    lua_pushboolean(L, b->layerVisible[cpp_idx] ? 1 : 0);
    return 1;
}

//==============================================================================
// Text bindings
//==============================================================================

namespace {

/// The active canvas's text state, or nullptr with no canvas.
TextState* activeText(LuaBindings* b, LuaCanvas* canvas) {
    (void)b;
    return canvas ? canvas->textState() : nullptr;
}

}  // namespace

// gfx.text(str, x, y [, align]) -> lines
int LuaBindings::lua_text(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) { lua_pushinteger(L, 0); return 1; }
    b->checkDrawLayer(L);
    size_t len = 0;
    const char* str = luaL_checklstring(L, 1, &len);
    const lua_Integer x = luaL_checkinteger(L, 2);
    const lua_Integer y = luaL_checkinteger(L, 3);
    TextAlign align = TextAlign::Left;
    if (!lua_isnoneornil(L, 4)) {
        const int a = nameIndex(luaL_checkstring(L, 4), kTextAlignNames);
        if (a < 0) return luaL_argerror(L, 4, "align must be \"left\", \"center\" or \"right\"");
        align = static_cast<TextAlign>(a);
    }
    if (TextState* st = b->currentCanvas->textState()) st->color = b->currentColor;
    const int lines = b->currentCanvas->drawText(std::string_view(str, len), static_cast<int16_t>(x),
                                                 static_cast<int16_t>(y), align);
    lua_pushinteger(L, lines);
    return 1;
}

// gfx.setFont(name)
int LuaBindings::lua_setFont(lua_State* L) {
    LuaBindings* b = getBindings(L);
    const char* name = luaL_checkstring(L, 1);
    const Face* face = faces::find(name);
    if (!face) return luaL_argerror(L, 1, "font must be \"monogram\", \"body\" or \"display\"");
    if (TextState* st = b ? activeText(b, b->currentCanvas) : nullptr) st->face = face;
    return 0;
}

// gfx.setTextWrap(overflow [, width])
int LuaBindings::lua_setTextWrap(lua_State* L) {
    LuaBindings* b = getBindings(L);
    const int mode = nameIndex(luaL_checkstring(L, 1), kTextOverflowNames);
    if (mode < 0) return luaL_argerror(L, 1, "overflow must be \"wrap\", \"clip\" or \"ellipsis\"");
    lua_Integer width = 0;
    if (!lua_isnoneornil(L, 2)) {
        width = luaL_checkinteger(L, 2);
        if (width < 1 || width > INT16_MAX) return luaL_argerror(L, 2, "width must be 1 or more");
    }
    if (TextState* st = b ? activeText(b, b->currentCanvas) : nullptr) {
        st->overflow = static_cast<TextOverflow>(mode);
        st->width = static_cast<int16_t>(width);
    }
    return 0;
}

// gfx.setTextSize(size)
int LuaBindings::lua_setTextSize(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !lua_isnumber(L, 1)) return 0;
    const lua_Integer s = lua_tointeger(L, 1);
    if (TextState* st = activeText(b, b->currentCanvas))
        st->size = (s > 0 && s <= 255) ? static_cast<uint8_t>(s) : 1;
    return 0;
}

// gfx.getTextWidth([str]) -> int
int LuaBindings::lua_getTextWidth(lua_State* L) {
    LuaBindings* b = getBindings(L);
    size_t len = 0;
    const char* str = luaL_optlstring(L, 1, "", &len);
    if (!b || !b->currentCanvas) { lua_pushinteger(L, 0); return 1; }
    lua_pushinteger(L, b->currentCanvas->textWidth(std::string_view(str, len)));
    return 1;
}

// gfx.setTextOutline(on [, color])
int LuaBindings::lua_setTextOutline(lua_State* L) {
    LuaBindings* b = getBindings(L);
    const bool on = lua_toboolean(L, 1) != 0;
    TextState* st = b ? activeText(b, b->currentCanvas) : nullptr;
    const uint8_t color = lua::optCanvasColour(L, 2, ROLE_BG, b ? indexedColours(b->currentCanvas) : true);
    if (st) {
        st->outline = on;
        st->outlineColor = color;
    }
    return 0;
}

// gfx.setTextEffect([effect])
int LuaBindings::lua_setTextEffect(lua_State* L) {
    LuaBindings* b = getBindings(L);
    TextState* st = b ? activeText(b, b->currentCanvas) : nullptr;
    if (lua_isnoneornil(L, 1)) {
        if (st) { st->hasEffect = false; st->effect = Effect{}; }
        return 0;
    }
    const Effect fx = lua::checkEffect(L, 1);
    if (st) { st->effect = fx; st->hasEffect = true; }
    return 0;
}

} // namespace enjin2
