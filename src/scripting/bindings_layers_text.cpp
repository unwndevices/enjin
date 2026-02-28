#include "../../include/enjin2/scripting/bindings.hpp"

namespace enjin2 {

// File-local helper: convert 1-indexed Lua layer index to clamped 0-indexed C++
static inline int clampLayerIdx(int lua_idx, int layerCount) {
    int cpp = lua_idx - 1;
    if (cpp < 0) cpp = 0;
    if (cpp >= layerCount) cpp = layerCount - 1;
    return cpp;
}

//==============================================================================
// Layer System Bindings (LAYER-06)
//==============================================================================

// setLayer(n)  — Switch the active canvas to layer n (Lua 1-indexed, silently clamped)
int LuaBindings::lua_setLayer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0) return 0;

    int cpp_idx = clampLayerIdx(
        static_cast<int>(luaL_checkinteger(L, 1)),
        static_cast<int>(b->layerCount));

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

// clearLayer(n, color)  — Clear only the specified layer buffer (1-indexed, color optional, defaults to 0)
int LuaBindings::lua_clearLayer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0) return 0;

    int cpp_idx = clampLayerIdx(
        static_cast<int>(luaL_checkinteger(L, 1)),
        static_cast<int>(b->layerCount));

    uint8_t color = static_cast<uint8_t>(luaL_optinteger(L, 2, 0));

    LuaCanvas* target = b->layerCanvases[cpp_idx];
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

    int cpp_idx = clampLayerIdx(
        static_cast<int>(luaL_checkinteger(L, 1)),
        static_cast<int>(b->layerCount));

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

    int cpp_idx = clampLayerIdx(
        static_cast<int>(luaL_checkinteger(L, 1)),
        static_cast<int>(b->layerCount));

    lua_pushboolean(L, b->layerVisible[cpp_idx] ? 1 : 0);
    return 1;
}

//==============================================================================
// Text bindings
//==============================================================================

int LuaBindings::lua_text(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    const char* str = luaL_checkstring(L, 1);
    lua_Integer x = luaL_checkinteger(L, 2);
    lua_Integer y = luaL_checkinteger(L, 3);
    // Optional 4th arg: per-call scale override — does NOT mutate currentTextSize
    uint8_t scale = b->currentTextSize;
    if (lua_gettop(L) >= 4 && lua_isnumber(L, 4)) {
        int s = static_cast<int>(lua_tointeger(L, 4));
        if (s > 0 && s <= 255) scale = static_cast<uint8_t>(s);
    }
    b->currentCanvas->drawText(str, static_cast<int16_t>(x), static_cast<int16_t>(y),
                              b->currentColor, scale, b->currentFont);
    return 0;
}

int LuaBindings::lua_textCentered(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    const char* str = luaL_checkstring(L, 1);
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint8_t scale = b->currentTextSize;
    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) {
        int s = static_cast<int>(lua_tointeger(L, 3));
        if (s > 0 && s <= 255) scale = static_cast<uint8_t>(s);
    }
    uint16_t tw = b->currentCanvas->measureTextWidth(str, scale, b->currentFont);
    int16_t x = static_cast<int16_t>(((int)b->currentCanvas->getWidth() - (int)tw) / 2);
    b->currentCanvas->drawText(str, x, y, b->currentColor, scale, b->currentFont);
    return 0;
}

int LuaBindings::lua_textAligned(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    const char* str = luaL_checkstring(L, 1);
    int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 3));
    const char* align = luaL_optstring(L, 4, "left");
    uint8_t scale = b->currentTextSize;
    if (lua_gettop(L) >= 5 && lua_isnumber(L, 5)) {
        int s = static_cast<int>(lua_tointeger(L, 5));
        if (s > 0 && s <= 255) scale = static_cast<uint8_t>(s);
    }
    if (strcmp(align, "center") == 0) {
        uint16_t tw = b->currentCanvas->measureTextWidth(str, scale, b->currentFont);
        x = static_cast<int16_t>(x - static_cast<int16_t>(tw / 2));
    } else if (strcmp(align, "right") == 0) {
        uint16_t tw = b->currentCanvas->measureTextWidth(str, scale, b->currentFont);
        x = static_cast<int16_t>(x - static_cast<int16_t>(tw));
    }
    // "left" = no adjustment (default)
    b->currentCanvas->drawText(str, x, y, b->currentColor, scale, b->currentFont);
    return 0;
}

int LuaBindings::lua_textWrapped(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    const char* str = luaL_checkstring(L, 1);
    lua_Integer x = luaL_checkinteger(L, 2);
    lua_Integer y = luaL_checkinteger(L, 3);
    lua_Integer maxWidth = luaL_checkinteger(L, 4);
    b->currentCanvas->drawTextWrapped(str, static_cast<int16_t>(x), static_cast<int16_t>(y),
                                      static_cast<uint16_t>(maxWidth), b->currentColor,
                                      b->currentTextSize, b->currentFont);
    return 0;
}

int LuaBindings::lua_setTextSize(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        int s = static_cast<int>(lua_tointeger(L, 1));
        b->currentTextSize = (s > 0 && s <= 255) ? static_cast<uint8_t>(s) : 1;
    }
    return 0;
}

int LuaBindings::lua_getTextSize(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, 1); return 1; }
    lua_pushinteger(L, b->currentTextSize);
    return 1;
}

int LuaBindings::lua_setFont(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    const char* name = luaL_checkstring(L, 1);
    if (strcmp(name, "default") == 0) {
        b->currentFont = nullptr;
        strncpy(b->currentFontName, "default", 31);
        b->currentFontName[31] = '\0';
        return 0;
    }
    for (uint8_t i = 0; i < b->fontCount; ++i) {
        if (strcmp(b->fontRegistry[i].name, name) == 0) {
            b->currentFont = b->fontRegistry[i].font;
            strncpy(b->currentFontName, name, 31);
            b->currentFontName[31] = '\0';
            return 0;
        }
    }
    // Unknown font name: keep current
    return 0;
}

int LuaBindings::lua_getFont(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushstring(L, "default"); return 1; }
    lua_pushstring(L, b->currentFontName);
    return 1;
}

int LuaBindings::lua_getTextWidth(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }
    const char* str = luaL_optstring(L, 1, "");
    uint16_t w = b->currentCanvas->measureTextWidth(str, b->currentTextSize, b->currentFont);
    lua_pushinteger(L, w);
    return 1;
}

int LuaBindings::lua_getTextHeight(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) {
        lua_pushinteger(L, 8);  // default 5x7 height
        return 1;
    }
    uint8_t h = b->currentCanvas->measureTextHeight(b->currentTextSize, b->currentFont);
    lua_pushinteger(L, h);
    return 1;
}

void LuaBindings::registerFont(const char* name, const ::GFXfont* font) {
    if (fontCount >= MAX_FONTS) return;
    for (uint8_t i = 0; i < fontCount; ++i) {
        if (strcmp(fontRegistry[i].name, name) == 0) {
            fontRegistry[i].font = font;
            return;
        }
    }
    strncpy(fontRegistry[fontCount].name, name, 31);
    fontRegistry[fontCount].name[31] = '\0';
    fontRegistry[fontCount].font = font;
    ++fontCount;
}

} // namespace enjin2
