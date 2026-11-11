#include "bindings_internal.hpp"
#include "../../include/enjin2/scripting/layer_claims.hpp"
#include "../../include/enjin2/graphics/numerals.hpp"

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

// One draw for gfx.text, gfx.textAligned and native callers (drawText): the
// current face and colour at `scale`, x moved by the align.
static void drawTextOn(LuaCanvas& canvas, const char* str, int16_t x, int16_t y, NumberAlign align,
                       uint8_t color, uint8_t scale, const GFXfont* font) {
    if (align == NumberAlign::Center) {
        uint16_t tw = canvas.measureTextWidth(str, scale, font);
        x = static_cast<int16_t>(x - static_cast<int16_t>(tw / 2));
    } else if (align == NumberAlign::Right) {
        uint16_t tw = canvas.measureTextWidth(str, scale, font);
        x = static_cast<int16_t>(x - static_cast<int16_t>(tw));
    }
    // "left" = no adjustment
    canvas.drawText(str, x, y, color, scale, font);
}

void LuaBindings::drawText(const char* str, int16_t x, int16_t y, NumberAlign align) {
    if (!currentCanvas || !str) return;
    drawTextOn(*currentCanvas, str, x, y, align, currentColor, currentTextSize, currentFont);
}

void LuaBindings::fillRect(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    if (currentCanvas) currentCanvas->fillRect(x, y, w, h, currentColor);
}

bool LuaBindings::selectFont(const char* name) {
    if (!name) return false;
    if (strcmp(name, "default") == 0) {
        currentFont = nullptr;
        strncpy(currentFontName, "default", 31);
        currentFontName[31] = '\0';
        return true;
    }
    for (uint8_t i = 0; i < fontCount; ++i) {
        if (strcmp(fontRegistry[i].name, name) == 0) {
            currentFont = fontRegistry[i].font;
            strncpy(currentFontName, name, 31);
            currentFontName[31] = '\0';
            return true;
        }
    }
    return false;
}

int LuaBindings::lua_text(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    b->checkDrawLayer(L);
    const char* str = luaL_checkstring(L, 1);
    lua_Integer x = luaL_checkinteger(L, 2);
    lua_Integer y = luaL_checkinteger(L, 3);
    // Optional 4th arg: per-call scale override — does NOT mutate currentTextSize
    uint8_t scale = b->currentTextSize;
    if (!lua_isnoneornil(L, 4)) {   // off 1..255 raises (ADR-0030)
        const int s = lua::checkInt(L, 4);
        luaL_argcheck(L, s > 0 && s <= 255, 4, "scale is 1..255");
        scale = static_cast<uint8_t>(s);
    }
    drawTextOn(*b->currentCanvas, str, static_cast<int16_t>(x), static_cast<int16_t>(y),
               NumberAlign::Left, b->currentColor, scale, b->currentFont);
    return 0;
}

int LuaBindings::lua_textCentered(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    b->checkDrawLayer(L);
    const char* str = luaL_checkstring(L, 1);
    int16_t y = static_cast<int16_t>(lua::checkInt(L, 2));
    uint8_t scale = b->currentTextSize;
    if (!lua_isnoneornil(L, 3)) {   // off 1..255 raises (ADR-0030)
        const int s = lua::checkInt(L, 3);
        luaL_argcheck(L, s > 0 && s <= 255, 3, "scale is 1..255");
        scale = static_cast<uint8_t>(s);
    }
    uint16_t tw = b->currentCanvas->measureTextWidth(str, scale, b->currentFont);
    int16_t x = static_cast<int16_t>(((int)b->currentCanvas->getWidth() - (int)tw) / 2);
    b->currentCanvas->drawText(str, x, y, b->currentColor, scale, b->currentFont);
    return 0;
}

int LuaBindings::lua_textAligned(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    b->checkDrawLayer(L);
    const char* str = luaL_checkstring(L, 1);
    int16_t x = static_cast<int16_t>(lua::checkInt(L, 2));
    int16_t y = static_cast<int16_t>(lua::checkInt(L, 3));
    // An unknown align name or a scale off 1..255 is a mistake: it raises (ADR-0030).
    const int align = lua::optName(L, 4, kNumberAlignNames, "align", static_cast<int>(NumberAlign::Left));
    uint8_t scale = b->currentTextSize;
    if (!lua_isnoneornil(L, 5)) {
        const int s = lua::checkInt(L, 5);
        luaL_argcheck(L, s > 0 && s <= 255, 5, "scale is 1..255");
        scale = static_cast<uint8_t>(s);
    }
    drawTextOn(*b->currentCanvas, str, x, y, static_cast<NumberAlign>(align), b->currentColor,
               scale, b->currentFont);
    return 0;
}

int LuaBindings::lua_textWrapped(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    b->checkDrawLayer(L);
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
    const int s = lua::checkInt(L, 1);
    luaL_argcheck(L, s > 0 && s <= 255, 1, "size is 1..255");   // a mistake raises (ADR-0030)
    b->currentTextSize = static_cast<uint8_t>(s);
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
    if (b->selectFont(name)) return 0;
    // A font no host registered is a mistake: it raises (ADR-0030).
    return luaL_argerror(L, 1, lua_pushfstring(L, "unknown font '%s'", name));
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
