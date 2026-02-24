#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/graphics/palette.hpp"
#include <chrono>
#include <iostream>

namespace enjin2 {

// Global pointer to current bindings instance for static callbacks
static LuaBindings* g_currentBindings = nullptr;

//==============================================================================
// LuaCanvas Implementation
//==============================================================================

void LuaCanvas::clear(uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        canvas->clear(Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        canvas->clear(color);
    }
}

void LuaCanvas::setPixel(int16_t x, int16_t y, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        canvas->setPixel(x, y, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        canvas->setPixel(x, y, color);
    }
}

uint8_t LuaCanvas::getPixel(int16_t x, int16_t y) const {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        return canvas->getPixel(x, y).value;
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        return canvas->getPixel(x, y);
    }
}

void LuaCanvas::drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawLine(*canvas, x1, y1, x2, y2, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawLine(*canvas, x1, y1, x2, y2, color);
    }
}

void LuaCanvas::drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    Rect rect(x, y, width, height);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawRect(*canvas, rect, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawRect(*canvas, rect, color);
    }
}

void LuaCanvas::fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    Rect rect(x, y, width, height);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::fillRect(*canvas, rect, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::fillRect(*canvas, rect, color);
    }
}

void LuaCanvas::drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawCircle(*canvas, x, y, radius, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawCircle(*canvas, x, y, radius, color);
    }
}

void LuaCanvas::fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::fillCircle(*canvas, x, y, radius, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::fillCircle(*canvas, x, y, radius, color);
    }
}

void LuaCanvas::drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                           int16_t x3, int16_t y3, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawTriangle(*canvas, x1, y1, x2, y2, x3, y3, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawTriangle(*canvas, x1, y1, x2, y2, x3, y3, color);
    }
}

void LuaCanvas::fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, 
                           int16_t x3, int16_t y3, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::fillTriangle(*canvas, x1, y1, x2, y2, x3, y3, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::fillTriangle(*canvas, x1, y1, x2, y2, x3, y3, color);
    }
}

//==============================================================================
// LuaBindings Implementation
//==============================================================================

LuaBindings::LuaBindings(LuaEngine* luaEngine) 
    : engine(luaEngine), currentCanvas(nullptr), currentColor(15), lineWidth(1) {
    g_currentBindings = this;
}

void LuaBindings::registerAll() {
    if (!engine || !engine->isInitialized()) {
        return;
    }
    
    // Store bindings instance in Lua registry
    lua_State* L = engine->getState();
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
    
    // Canvas functions
    engine->registerFunction("getWidth", lua_getWidth);
    engine->registerFunction("getHeight", lua_getHeight);
    engine->registerFunction("clear", lua_clear);
    
    // Drawing state functions (love2d.graphics style)
    engine->registerFunction("setColor", lua_setColor);
    engine->registerFunction("getColor", lua_getColor);
    engine->registerFunction("setLineWidth", lua_setLineWidth);
    engine->registerFunction("getLineWidth", lua_getLineWidth);
    
    // Drawing primitives
    engine->registerFunction("point", lua_point);
    engine->registerFunction("line", lua_line);
    engine->registerFunction("rectangle", lua_rectangle);
    engine->registerFunction("circle", lua_circle);
    engine->registerFunction("triangle", lua_triangle);
    
    // Pixel access
    engine->registerFunction("setPixel", lua_setPixel);
    engine->registerFunction("getPixel", lua_getPixel);
    
    // Utility functions
    engine->registerFunction("print", lua_print);
    engine->registerFunction("time", lua_time);
    
    // High-performance optimized drawing functions
    engine->registerFunction("fastFillRect", lua_fastFillRect);
    engine->registerFunction("fastDrawLine", lua_fastDrawLine);

    // Palette
    engine->registerFunction("setPaletteColor", lua_setPaletteColor);
    engine->registerFunction("getPaletteColor", lua_getPaletteColor);
    engine->registerFunction("loadPalette", lua_loadPalette);
    engine->registerFunction("getPaletteSize", lua_getPaletteSize);

    // Create love2d.graphics-style table for familiarity
    registerTable("love", {
        {"draw", lua_rectangle},  // Basic draw function
    });
    
    // Create graphics subtable properly
    lua_getglobal(L, "love");
    if (lua_istable(L, -1)) {
        lua_newtable(L);  // Create graphics table
        
        // Add functions to graphics table
        lua_pushcfunction(L, lua_setColor);
        lua_setfield(L, -2, "setColor");
        lua_pushcfunction(L, lua_getColor);
        lua_setfield(L, -2, "getColor");
        lua_pushcfunction(L, lua_setLineWidth);
        lua_setfield(L, -2, "setLineWidth");
        lua_pushcfunction(L, lua_getLineWidth);
        lua_setfield(L, -2, "getLineWidth");
        lua_pushcfunction(L, lua_point);
        lua_setfield(L, -2, "point");
        lua_pushcfunction(L, lua_line);
        lua_setfield(L, -2, "line");
        lua_pushcfunction(L, lua_rectangle);
        lua_setfield(L, -2, "rectangle");
        lua_pushcfunction(L, lua_circle);
        lua_setfield(L, -2, "circle");
        lua_pushcfunction(L, lua_clear);
        lua_setfield(L, -2, "clear");
        
        // Set graphics table as love.graphics
        lua_setfield(L, -2, "graphics");
    }
    lua_pop(L, 1);
}

void LuaBindings::setCanvas(LuaCanvas* canvas) {
    currentCanvas = canvas;
}

LuaBindings* LuaBindings::getBindings(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
    LuaBindings* bindings = static_cast<LuaBindings*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return bindings;
}

void LuaBindings::registerTable(const std::string& tableName, 
                               const std::vector<std::pair<std::string, lua_CFunction>>& functions) {
    lua_State* L = engine->getState();
    
    lua_newtable(L);
    for (const auto& func : functions) {
        lua_pushcfunction(L, func.second);
        lua_setfield(L, -2, func.first.c_str());
    }
    lua_setglobal(L, tableName.c_str());
}

//==============================================================================
// Lua Function Implementations
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
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
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
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
    if (lua_gettop(L) >= 2 && lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
        int16_t x = static_cast<int16_t>(lua_tointeger(L, 1));
        int16_t y = static_cast<int16_t>(lua_tointeger(L, 2));
        
        bindings->currentCanvas->setPixel(x, y, bindings->currentColor);
    }
    
    return 0;
}

int LuaBindings::lua_line(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
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
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
    if (lua_gettop(L) >= 4) {
        const char* mode = "fill";
        if (lua_gettop(L) >= 5 && lua_isstring(L, 1)) {
            mode = lua_tostring(L, 1);
        }
        
        int startIdx = (lua_isstring(L, 1)) ? 2 : 1;
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
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
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
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
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
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
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
    if (lua_gettop(L) >= 1) {
        if (lua_isstring(L, 1)) {
            const char* str = lua_tostring(L, 1);
            std::cout << str << std::endl;
        } else if (lua_isnumber(L, 1)) {
            double num = lua_tonumber(L, 1);
            std::cout << num << std::endl;
        } else if (lua_isboolean(L, 1)) {
            bool val = lua_toboolean(L, 1);
            std::cout << (val ? "true" : "false") << std::endl;
        }
    }
    
    return 0;
}

int LuaBindings::lua_time(lua_State* L) {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration);
    
    lua_pushnumber(L, seconds.count());
    return 1;
}

//==============================================================================
// High-Performance Drawing Functions
//==============================================================================

int LuaBindings::lua_fastFillRect(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
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
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        return 0;
    }
    
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

//==============================================================================
// LuaScriptSystem Implementation
//==============================================================================

LuaScriptSystem::LuaScriptSystem() : bindings(&engine), canvas(nullptr) {
}

bool LuaScriptSystem::initialize() {
    if (!engine.initialize()) {
        return false;
    }
    
    bindings.registerAll();
    return true;
}

void LuaScriptSystem::shutdown() {
    engine.shutdown();
}

void LuaScriptSystem::setCanvas(LuaCanvas* canvas) {
    this->canvas = canvas;
    bindings.setCanvas(canvas);
}

LuaResult LuaScriptSystem::executeScript(const std::string& code) {
    return engine.executeString(code);
}

LuaResult LuaScriptSystem::loadScript(const std::string& filename) {
    return engine.executeFile(filename);
}

size_t LuaScriptSystem::getMemoryUsage() const {
    return engine.getMemoryUsage();
}

} // namespace enjin2