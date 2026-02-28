#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/graphics/palette.hpp"
#include <algorithm>

namespace enjin2 {

// Replaces null-check preamble repeated across drawing functions
#define REQUIRE_CANVAS(b, L) \
    LuaBindings* b = LuaBindings::getBindings(L); \
    if (!(b) || !(b)->currentCanvas) return 0

//==============================================================================
// Lua Drawing Functions
//==============================================================================

int LuaBindings::lua_getWidth(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, bindings->currentCanvas->getWidth());
    return 1;
}

int LuaBindings::lua_getHeight(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, bindings->currentCanvas->getHeight());
    return 1;
}

int LuaBindings::lua_clear(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    uint8_t color = bindings->currentColor;
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        color = static_cast<uint8_t>(lua_tointeger(L, 1));
    }

    bindings->currentCanvas->clear(color);
    return 0;
}

int LuaBindings::lua_setColor(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        return 0;
    }

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        bindings->currentColor = static_cast<uint8_t>(lua_tointeger(L, 1));
    }

    return 0;
}

int LuaBindings::lua_getColor(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, bindings->currentColor);
    return 1;
}

int LuaBindings::lua_setLineWidth(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        return 0;
    }

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        bindings->lineWidth = static_cast<uint16_t>(lua_tointeger(L, 1));
    }

    return 0;
}

int LuaBindings::lua_getLineWidth(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings) {
        lua_pushinteger(L, 1);
        return 1;
    }

    lua_pushinteger(L, bindings->lineWidth);
    return 1;
}

int LuaBindings::lua_point(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 2 && lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
        int16_t x = static_cast<int16_t>(lua_tointeger(L, 1));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, 2));

        bindings->currentCanvas->setPixel(x, y, bindings->currentColor);
    }

    return 0;
}

int LuaBindings::lua_line(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 4) {
        int16_t x1 = static_cast<int16_t>(lua_tointeger(L, 1));
        int16_t y1 = static_cast<int16_t>(lua_tointeger(L, 2));
        int16_t x2 = static_cast<int16_t>(lua_tointeger(L, 3));
        int16_t y2 = static_cast<int16_t>(lua_tointeger(L, 4));

        bindings->currentCanvas->drawLine(x1, y1, x2, y2, bindings->currentColor);
    }

    return 0;
}

int LuaBindings::lua_rectangle(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 4) {
        const char* mode = "fill";
        if (lua_gettop(L) >= 5 && lua_type(L, 1) == LUA_TSTRING) {
            mode = lua_tostring(L, 1);
        }

        int startIdx = (lua_type(L, 1) == LUA_TSTRING) ? 2 : 1;
        int16_t x = static_cast<int16_t>(lua_tointeger(L, startIdx));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, startIdx + 1));
        uint16_t width = static_cast<uint16_t>(lua_tointeger(L, startIdx + 2));
        uint16_t height = static_cast<uint16_t>(lua_tointeger(L, startIdx + 3));

        if (strcmp(mode, "fill") == 0) {
            bindings->currentCanvas->fillRect(x, y, width, height, bindings->currentColor);
        } else {
            bindings->currentCanvas->drawRect(x, y, width, height, bindings->currentColor);
        }
    }

    return 0;
}

int LuaBindings::lua_circle(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 3) {
        const char* mode = "fill";
        if (lua_gettop(L) >= 4 && lua_isstring(L, 1)) {
            mode = lua_tostring(L, 1);
        }

        int startIdx = (lua_isstring(L, 1)) ? 2 : 1;
        int16_t x = static_cast<int16_t>(lua_tointeger(L, startIdx));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, startIdx + 1));
        uint16_t radius = static_cast<uint16_t>(lua_tointeger(L, startIdx + 2));

        if (strcmp(mode, "fill") == 0) {
            bindings->currentCanvas->fillCircle(x, y, radius, bindings->currentColor);
        } else {
            bindings->currentCanvas->drawCircle(x, y, radius, bindings->currentColor);
        }
    }

    return 0;
}

int LuaBindings::lua_triangle(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 6) {
        const char* mode = "fill";
        if (lua_gettop(L) >= 7 && lua_isstring(L, 1)) {
            mode = lua_tostring(L, 1);
        }

        int startIdx = (lua_isstring(L, 1)) ? 2 : 1;
        int16_t x1 = static_cast<int16_t>(lua_tointeger(L, startIdx));
        int16_t y1 = static_cast<int16_t>(lua_tointeger(L, startIdx + 1));
        int16_t x2 = static_cast<int16_t>(lua_tointeger(L, startIdx + 2));
        int16_t y2 = static_cast<int16_t>(lua_tointeger(L, startIdx + 3));
        int16_t x3 = static_cast<int16_t>(lua_tointeger(L, startIdx + 4));
        int16_t y3 = static_cast<int16_t>(lua_tointeger(L, startIdx + 5));

        if (strcmp(mode, "fill") == 0) {
            bindings->currentCanvas->fillTriangle(x1, y1, x2, y2, x3, y3, bindings->currentColor);
        } else {
            bindings->currentCanvas->drawTriangle(x1, y1, x2, y2, x3, y3, bindings->currentColor);
        }
    }

    return 0;
}

int LuaBindings::lua_setPixel(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    if (lua_gettop(L) >= 3) {
        int16_t x = static_cast<int16_t>(lua_tointeger(L, 1));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, 2));
        uint8_t color = static_cast<uint8_t>(lua_tointeger(L, 3));

        bindings->currentCanvas->setPixel(x, y, color);
    }

    return 0;
}

int LuaBindings::lua_getPixel(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }

    if (lua_gettop(L) >= 2) {
        int16_t x = static_cast<int16_t>(lua_tointeger(L, 1));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, 2));

        uint8_t color = bindings->currentCanvas->getPixel(x, y);
        lua_pushinteger(L, color);
        return 1;
    }

    lua_pushinteger(L, 0);
    return 1;
}

int LuaBindings::lua_print(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            printf("%s", s);
        } else {
            printf("(%s)", lua_typename(L, lua_type(L, i)));
        }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}

//==============================================================================
// High-Performance Drawing Functions
//==============================================================================

int LuaBindings::lua_fastFillRect(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    int x = static_cast<int>(luaL_checknumber(L, 1));
    int y = static_cast<int>(luaL_checknumber(L, 2));
    int w = static_cast<int>(luaL_checknumber(L, 3));
    int h = static_cast<int>(luaL_checknumber(L, 4));
    uint8_t color = static_cast<uint8_t>(luaL_optnumber(L, 5, bindings->currentColor));

    // Fast bulk fill - bypass individual pixel calls
    int x2 = x + w;
    int y2 = y + h;

    // Clamp to canvas bounds
    x = std::max(x, 0);
    y = std::max(y, 0);
    x2 = std::min(x2, static_cast<int>(bindings->currentCanvas->getWidth()));
    y2 = std::min(y2, static_cast<int>(bindings->currentCanvas->getHeight()));

    // Bulk fill
    for (int py = y; py < y2; py++) {
        for (int px = x; px < x2; px++) {
            bindings->currentCanvas->setPixel(px, py, color);
        }
    }

    return 0;
}

int LuaBindings::lua_fastDrawLine(lua_State* L) {
    REQUIRE_CANVAS(bindings, L);

    int x1 = static_cast<int>(luaL_checknumber(L, 1));
    int y1 = static_cast<int>(luaL_checknumber(L, 2));
    int x2 = static_cast<int>(luaL_checknumber(L, 3));
    int y2 = static_cast<int>(luaL_checknumber(L, 4));
    uint8_t color = static_cast<uint8_t>(luaL_optnumber(L, 5, bindings->currentColor));

    // Fast Bresenham line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    int x = x1, y = y1;
    int width = static_cast<int>(bindings->currentCanvas->getWidth());
    int height = static_cast<int>(bindings->currentCanvas->getHeight());

    while (true) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            bindings->currentCanvas->setPixel(x, y, color);
        }
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }

    return 0;
}

//==============================================================================
// Palette Functions
//==============================================================================

int LuaBindings::lua_setPaletteColor(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    if (lua_isstring(L, 2)) {
        // Overload: setPaletteColor(index, '#rrggbb')
        const char* hex = luaL_checkstring(L, 2);
        uint8_t r = 0, g = 0, b = 0;
        enjin2::parseHexColor(hex, r, g, b);
        enjin2::g_palette.setColor(static_cast<uint8_t>(index), r, g, b);
    } else {
        // Overload: setPaletteColor(index, r, g, b)
        int r = luaL_checkinteger(L, 2);
        int g = luaL_checkinteger(L, 3);
        int b = luaL_checkinteger(L, 4);
        enjin2::g_palette.setColor(
            static_cast<uint8_t>(index),
            static_cast<uint8_t>(r),
            static_cast<uint8_t>(g),
            static_cast<uint8_t>(b));
    }
    return 0;
}

int LuaBindings::lua_getPaletteColor(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    RGB c = enjin2::g_palette.getColor(static_cast<uint8_t>(index));
    lua_pushinteger(L, c.r);
    lua_pushinteger(L, c.g);
    lua_pushinteger(L, c.b);
    return 3;
}

int LuaBindings::lua_loadPalette(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    bool ok = enjin2::g_palette.loadPreset(name);
    lua_pushboolean(L, ok);
    return 1;
}

int LuaBindings::lua_getPaletteSize(lua_State* L) {
    lua_pushinteger(L, enjin2::g_palette.getSize());
    return 1;
}

} // namespace enjin2
