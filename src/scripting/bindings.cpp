#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/graphics/defaultfont.hpp"
#include "../../include/enjin2/graphics/palette.hpp"
#include "../../include/enjin2/graphics/text_renderer.hpp"
#include "../../include/enjin2/core/scene.hpp"
#include "../../include/enjin2/core/scene_state_machine.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/core/object.hpp"

namespace enjin2 {

// Global pointer to current bindings instance for static callbacks
static LuaBindings* g_currentBindings = nullptr;

//==============================================================================
// ScriptProxy Metatable Implementation
//==============================================================================

static constexpr const char* PROXY_METATABLE = "ScriptProxy";

// __index metamethod: called when Lua reads self.property
// Stack layout on entry: [1]=userdata(self), [2]=key_string
static int lua_proxy_index_impl(lua_State* L) {
    if (!lua_isuserdata(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(lua_touserdata(L, 1));
    if (!proxy || !proxy->valid || !proxy->component) {
        lua_pushnil(L);
        return 1;
    }

    const char* key = lua_tostring(L, 2);
    if (!key) {
        lua_pushnil(L);
        return 1;
    }

    enjin2::C_LuaScript* comp = proxy->component;
    enjin2::Object* owner = comp->getOwner();

    if (strcmp(key, "x") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        lua_pushinteger(L, pos ? static_cast<lua_Integer>(pos->getPosition().x) : 0);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        lua_pushinteger(L, pos ? static_cast<lua_Integer>(pos->getPosition().y) : 0);
        return 1;
    } else if (strcmp(key, "visible") == 0) {
        lua_pushboolean(L, comp->isVisible() ? 1 : 0);
        return 1;
    } else if (strcmp(key, "layer") == 0) {
        // 1-indexed in Lua: buffer_index 0 == layer 1
        lua_pushinteger(L, static_cast<lua_Integer>(comp->GetBufferIndex() + 1));
        return 1;
    } else if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, (owner && owner->isActive()) ? 1 : 0);
        return 1;
    } else if (strcmp(key, "name") == 0) {
        // Phase 29 complete: Object::getName() available
        const char* n = owner ? owner->getName() : nullptr;
        if (n) {
            lua_pushstring(L, n);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// __newindex metamethod: called when Lua writes self.property = value
// Stack layout on entry: [1]=userdata(self), [2]=key_string, [3]=value
static int lua_proxy_newindex_impl(lua_State* L) {
    if (!lua_isuserdata(L, 1)) return 0;
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(lua_touserdata(L, 1));
    if (!proxy || !proxy->valid || !proxy->component) return 0;

    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    enjin2::C_LuaScript* comp = proxy->component;
    enjin2::Object* owner = comp->getOwner();

    if (strcmp(key, "x") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        if (pos) {
            int16_t newX = static_cast<int16_t>(luaL_checkinteger(L, 3));
            pos->setPosition(newX, pos->getPosition().y);
        }
    } else if (strcmp(key, "y") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        if (pos) {
            int16_t newY = static_cast<int16_t>(luaL_checkinteger(L, 3));
            pos->setPosition(pos->getPosition().x, newY);
        }
    } else if (strcmp(key, "visible") == 0) {
        comp->SetVisibility(lua_toboolean(L, 3) != 0);
    } else if (strcmp(key, "layer") == 0) {
        // 1-indexed in Lua; convert to 0-indexed C++
        int luaLayer = static_cast<int>(luaL_checkinteger(L, 3));
        if (luaLayer >= 1) {
            comp->SetBufferIndex(static_cast<uint8_t>(luaLayer - 1));
        }
    } else if (strcmp(key, "active") == 0) {
        if (owner) owner->setActive(lua_toboolean(L, 3) != 0);
    }
    // "name" is intentionally read-only — silently ignore writes

    return 0;
}

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

void LuaCanvas::drawText(const char* str, int16_t x, int16_t y,
                         uint8_t color, uint8_t size, const ::GFXfont* font) {
    if (!str) return;
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(Pixel4(color));
        tr.setTextSize(size);
        tr.drawString(*canvas, x, y, str);
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(color);
        tr.setTextSize(size);
        tr.drawString(*canvas, x, y, str);
    }
}

void LuaCanvas::drawTextWrapped(const char* str, int16_t x, int16_t y,
                               uint16_t maxWidth, uint8_t color, uint8_t size,
                               const ::GFXfont* font) {
    if (!str) return;
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(Pixel4(color));
        tr.setTextSize(size);
        tr.drawStringWrapped(*canvas, x, y, maxWidth, str);
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(color);
        tr.setTextSize(size);
        tr.drawStringWrapped(*canvas, x, y, maxWidth, str);
    }
}

uint16_t LuaCanvas::measureTextWidth(const char* str, uint8_t size, const ::GFXfont* font) {
    if (!str) return 0;
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getTextWidth(str);
    } else {
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getTextWidth(str);
    }
}

uint8_t LuaCanvas::measureTextHeight(uint8_t size, const ::GFXfont* font) {
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getCharHeight() * size;
    } else {
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getCharHeight() * size;
    }
}

//==============================================================================
// LuaBindings Implementation
//==============================================================================

LuaBindings::LuaBindings(LuaEngine* luaEngine)
    : engine(luaEngine), currentCanvas(nullptr), currentInput(nullptr), currentColor(15), lineWidth(1) {
    g_currentBindings = this;
}

void LuaBindings::registerAll() {
    if (!engine || !engine->isInitialized()) {
        return;
    }

    // Reset all per-reload state so every load starts clean
    resetSpritePool();
    currentColor = 15;
    lineWidth = 1;

    // Store bindings instance in Lua registry
    lua_State* L = engine->getState();
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_bindings");

    // Store injected engine.* pointers in registry for closure retrieval
    lua_pushlightuserdata(L, m_ssm);          // may be nullptr; checked in each closure
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    lua_pushlightuserdata(L, m_activeScene);  // may be nullptr; checked in each closure
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    lua_pushlightuserdata(L, &m_timeState);   // always valid (member of LuaBindings)
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_time");

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
    
    // High-performance optimized drawing functions
    engine->registerFunction("fastFillRect", lua_fastFillRect);
    engine->registerFunction("fastDrawLine", lua_fastDrawLine);

    // Palette
    engine->registerFunction("setPaletteColor", lua_setPaletteColor);
    engine->registerFunction("getPaletteColor", lua_getPaletteColor);
    engine->registerFunction("loadPalette", lua_loadPalette);
    engine->registerFunction("getPaletteSize", lua_getPaletteSize);

    // Input polling (INP-05)
    engine->registerFunction("isButtonHeld",        lua_isButtonHeld);
    engine->registerFunction("isButtonJustPressed",  lua_isButtonJustPressed);
    engine->registerFunction("isButtonJustReleased", lua_isButtonJustReleased);
    engine->registerFunction("getAxis",              lua_getAxis);

    // Sprite pool (SPR-06)
    engine->registerFunction("newSprite",    lua_newSprite);
    engine->registerFunction("drawSprite",   lua_drawSprite);
    engine->registerFunction("updateSprite", lua_updateSprite);
    engine->registerFunction("setFrame",     lua_setFrame);

    // Layer system (LAYER-06)
    engine->registerFunction("setLayer",        lua_setLayer);
    engine->registerFunction("getLayer",        lua_getLayer);
    engine->registerFunction("clearLayer",      lua_clearLayer);
    engine->registerFunction("getLayerCount",   lua_getLayerCount);
    engine->registerFunction("setLayerVisible", lua_setLayerVisible);
    engine->registerFunction("isLayerVisible",  lua_isLayerVisible);

    // Text
    engine->registerFunction("text",            lua_text);
    engine->registerFunction("textWrapped",     lua_textWrapped);
    engine->registerFunction("setTextSize",    lua_setTextSize);
    engine->registerFunction("getTextSize",     lua_getTextSize);
    engine->registerFunction("setFont",        lua_setFont);
    engine->registerFunction("getFont",        lua_getFont);
    engine->registerFunction("getTextWidth",    lua_getTextWidth);
    engine->registerFunction("getTextHeight",  lua_getTextHeight);

    // Pre-register built-in 8pt font so setFont("default8") works
    registerFont("default8", &defaultFont8pt7b);

    // Layer global constants (Lua 1-indexed)
    lua_pushinteger(L, 1); lua_setglobal(L, "LAYER_BG");
    lua_pushinteger(L, 2); lua_setglobal(L, "LAYER_MID");
    lua_pushinteger(L, 3); lua_setglobal(L, "LAYER_FG");
    lua_pushinteger(L, 4); lua_setglobal(L, "LAYER_UI");

    // Create love2d.graphics-style table for familiarity
    lua_newtable(L);
    lua_pushcfunction(L, lua_rectangle);
    lua_setfield(L, -2, "draw");
    lua_setglobal(L, "love");
    
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

    // Register engine.* global table (ENG-06: must be before any script loads)
    registerEngineTable();

    // Register ScriptProxy metatable for C_LuaScript component path
    registerProxyMetatable();

    // Register Vec2/Point/Rect userdata metatables and math utility globals
    registerMathBindings();
}

void LuaBindings::setCanvas(LuaCanvas* canvas) {
    currentCanvas = canvas;
}

void LuaBindings::setInput(InputState* input) {
    currentInput = input;
}

void LuaBindings::setLayers(LuaCanvas** canvases, uint8_t count, bool* visibleArr) {
    layerCount = (count > MAX_LUA_LAYERS) ? static_cast<uint8_t>(MAX_LUA_LAYERS) : count;
    for (uint8_t i = 0; i < layerCount; ++i) {
        layerCanvases[i] = canvases[i];
    }
    layerVisible = visibleArr;
    activeLayer = 0;
    currentCanvas = layerCanvases[0];
}

void LuaBindings::resetSpritePool() {
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        spritePool[i] = SpriteState{};
    }
    currentTextSize = 1;
    currentFont = nullptr;
    strncpy(currentFontName, "default", 31);
    currentFontName[31] = '\0';
}

LuaBindings* LuaBindings::getBindings(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
    LuaBindings* bindings = static_cast<LuaBindings*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return bindings;
}

void LuaBindings::registerProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;
    // luaL_newmetatable returns 1 if new (creates it), 0 if it already exists
    if (luaL_newmetatable(L, PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_proxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_proxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);  // always pop — both new and existing cases leave table on stack
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
// Input Polling Functions (INP-05)
//==============================================================================

int LuaBindings::lua_isButtonHeld(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->held(btn) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_isButtonJustPressed(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justPressed(btn) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_isButtonJustReleased(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justReleased(btn) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_getAxis(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushnumber(L, 0.0); return 1; }
    int axis = static_cast<int>(luaL_checkinteger(L, 1));
    float val = (axis >= 0 && axis < 8) ? b->currentInput->axes[axis] : 0.0f;
    lua_pushnumber(L, static_cast<lua_Number>(val));
    return 1;
}

//==============================================================================
// Sprite Pool Bindings (SPR-06)
//==============================================================================

// newSprite(data_lightuserdata, cell_w, cell_h, cols, rows) -> handle (0..15) or -1
// data: lightuserdata pointing to const uint8_t* pixel array (caller owns lifetime)
// Returns -1 if pool is full.
int LuaBindings::lua_newSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, -1); return 1; }

    // Find first inactive slot
    int handle = -1;
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        if (!b->spritePool[i].active) { handle = i; break; }
    }
    if (handle < 0) { lua_pushinteger(L, -1); return 1; }  // pool full

    // Initialize slot from Lua arguments
    auto& s = b->spritePool[handle];
    s.sheet.data  = static_cast<const uint8_t*>(lua_topointer(L, 1));  // lightuserdata
    s.sheet.cellW = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    s.sheet.cellH = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    s.sheet.cols  = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    s.sheet.rows  = static_cast<uint8_t>(luaL_checkinteger(L, 5));
    s.fps      = 8.0f;
    s.accumSec = 0.0f;
    s.frame    = 0;
    s.mode    = AnimMode::Loop;
    s.forward = true;
    s.done    = false;
    s.active  = true;

    lua_pushinteger(L, handle);
    return 1;
}

// drawSprite(handle, x, y)
// Draws the current frame of the sprite to currentCanvas.
// Transparent pixels (palette index 15) are skipped.
int LuaBindings::lua_drawSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;

    int handle = static_cast<int>(luaL_checkinteger(L, 1));
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) return 0;

    int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 3));

    const auto& s = b->spritePool[handle];
    if (!s.sheet.data || s.frame >= s.sheet.frameCount()) return 0;

    // Blit frame via LuaCanvas::setPixel (type-erased path — works for 4-bit canvas)
    const uint8_t* frame_data = s.sheet.data
        + static_cast<uint16_t>(s.frame) * s.sheet.cellW * s.sheet.cellH;
    for (int16_t fy = 0; fy < static_cast<int16_t>(s.sheet.cellH); ++fy) {
        for (int16_t fx = 0; fx < static_cast<int16_t>(s.sheet.cellW); ++fx) {
            uint8_t px = frame_data[fy * s.sheet.cellW + fx] & 0x0F;
            if (px != 15) {  // index 15 = transparent
                b->currentCanvas->setPixel(x + fx, y + fy, px);
            }
        }
    }
    return 0;
}

// updateSprite(handle, dt_ms)
// Advances animation by dt_ms milliseconds.
// Uses same accumulator logic as C_Sprite::lateUpdate().
int LuaBindings::lua_updateSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;

    int handle = static_cast<int>(luaL_checkinteger(L, 1));
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) return 0;

    auto& s = b->spritePool[handle];
    if (!s.sheet.data || s.fps <= 0.0f || s.done) return 0;

    float dt = static_cast<float>(luaL_checknumber(L, 2));
    s.accumSec += dt;
    const float frameSec = 1.0f / s.fps;

    while (s.accumSec >= frameSec) {
        s.accumSec -= frameSec;  // preserve carry-over

        const uint8_t total = s.sheet.frameCount();
        if (total == 0) break;

        switch (s.mode) {
            case AnimMode::Once:
                if (s.frame < total - 1) {
                    ++s.frame;
                } else {
                    s.done = true;
                }
                break;
            case AnimMode::Loop:
                s.frame = static_cast<uint8_t>((s.frame + 1) % total);
                break;
            case AnimMode::PingPong:
                if (s.forward) {
                    if (s.frame < total - 1) {
                        ++s.frame;
                    } else {
                        s.forward = false;
                        if (total > 1) --s.frame;
                    }
                } else {
                    if (s.frame > 0) {
                        --s.frame;
                    } else {
                        s.forward = true;
                        ++s.frame;
                    }
                }
                break;
        }

        if (s.done) break;
    }
    return 0;
}

// setFrame(handle, frame_index)
// Directly sets the current frame. Clamped to [0, frameCount-1].
// Clears accumulator. Does not affect done/forward state.
int LuaBindings::lua_setFrame(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;

    int handle = static_cast<int>(luaL_checkinteger(L, 1));
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) return 0;

    auto& s = b->spritePool[handle];
    const uint8_t total = s.sheet.frameCount();
    if (total == 0) return 0;

    int requestedFrame = static_cast<int>(luaL_checkinteger(L, 2));
    if (requestedFrame < 0) requestedFrame = 0;
    if (requestedFrame >= total) requestedFrame = total - 1;

    s.frame    = static_cast<uint8_t>(requestedFrame);
    s.accumSec = 0.0f;
    return 0;
}

//==============================================================================
// Layer System Bindings (LAYER-06)
//==============================================================================

// setLayer(n)  — Switch the active canvas to layer n (Lua 1-indexed, silently clamped)
int LuaBindings::lua_setLayer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0) return 0;

    int lua_idx = static_cast<int>(luaL_checkinteger(L, 1));
    int cpp_idx = lua_idx - 1;  // convert to 0-based
    if (cpp_idx < 0) cpp_idx = 0;
    if (cpp_idx >= static_cast<int>(b->layerCount)) cpp_idx = b->layerCount - 1;

    b->activeLayer  = static_cast<uint8_t>(cpp_idx);
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

    int lua_idx = static_cast<int>(luaL_checkinteger(L, 1));
    int cpp_idx = lua_idx - 1;  // convert to 0-based
    if (cpp_idx < 0) cpp_idx = 0;
    if (cpp_idx >= static_cast<int>(b->layerCount)) cpp_idx = b->layerCount - 1;

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

    int lua_idx = static_cast<int>(luaL_checkinteger(L, 1));
    int cpp_idx = lua_idx - 1;  // convert to 0-based
    if (cpp_idx < 0) cpp_idx = 0;
    if (cpp_idx >= static_cast<int>(b->layerCount)) cpp_idx = b->layerCount - 1;

    bool visible = (lua_toboolean(L, 2) != 0);
    b->layerVisible[cpp_idx] = visible;
    return 0;
}

// isLayerVisible(n)  — Return whether layer n (1-indexed) is visible
int LuaBindings::lua_isLayerVisible(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || b->layerCount == 0 || !b->layerVisible) {
        lua_pushboolean(L, 1);  // default: visible
        return 1;
    }

    int lua_idx = static_cast<int>(luaL_checkinteger(L, 1));
    int cpp_idx = lua_idx - 1;  // convert to 0-based
    if (cpp_idx < 0) cpp_idx = 0;
    if (cpp_idx >= static_cast<int>(b->layerCount)) cpp_idx = b->layerCount - 1;

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
    b->currentCanvas->drawText(str, static_cast<int16_t>(x), static_cast<int16_t>(y),
                              b->currentColor, b->currentTextSize, b->currentFont);
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

//==============================================================================
// engine.* Global Table (ENG-01..ENG-06)
//==============================================================================

void LuaBindings::registerEngineTable() {
    lua_State* L = engine->getState();

    // Stack balance check: track depth before and after
    // Every lua_newtable must be balanced by lua_setfield or lua_setglobal

    lua_newtable(L);                               // [engine_table]

    // --- engine.scene sub-table (ENG-01: switch, ENG-02: find) ---
    lua_newtable(L);                               // [engine_table] [scene_table]
    lua_pushcfunction(L, lua_engine_scene_switch);
    lua_setfield(L, -2, "switch");                 // scene_table.switch = fn
    lua_pushcfunction(L, lua_engine_scene_find);
    lua_setfield(L, -2, "find");                   // scene_table.find   = fn
    lua_setfield(L, -2, "scene");                  // engine_table.scene = scene_table

    // --- engine.input sub-table (ENG-03) ---
    lua_newtable(L);                               // [engine_table] [input_table]
    lua_pushcfunction(L, lua_engine_input_held);
    lua_setfield(L, -2, "held");
    lua_pushcfunction(L, lua_engine_input_just_pressed);
    lua_setfield(L, -2, "just_pressed");
    lua_pushcfunction(L, lua_engine_input_just_released);
    lua_setfield(L, -2, "just_released");
    lua_pushcfunction(L, lua_engine_input_axis);
    lua_setfield(L, -2, "axis");
    lua_setfield(L, -2, "input");                  // engine_table.input = input_table

    // --- engine.time sub-table (ENG-04) ---
    lua_newtable(L);                               // [engine_table] [time_table]
    lua_pushcfunction(L, lua_engine_time_delta);
    lua_setfield(L, -2, "delta");
    lua_pushcfunction(L, lua_engine_time_now);
    lua_setfield(L, -2, "now");
    lua_pushcfunction(L, lua_engine_time_frame);
    lua_setfield(L, -2, "frame");
    lua_setfield(L, -2, "time");                   // engine_table.time = time_table

    // --- engine.collision sub-table ---
    lua_newtable(L);                               // [engine_table] [collision_table]
    lua_pushcfunction(L, lua_engine_collision_aabb);
    lua_setfield(L, -2, "aabb");
    lua_pushcfunction(L, lua_engine_collision_circleCircle);
    lua_setfield(L, -2, "circleCircle");
    lua_pushcfunction(L, lua_engine_collision_pointInRect);
    lua_setfield(L, -2, "pointInRect");
    lua_pushcfunction(L, lua_engine_collision_pointInCircle);
    lua_setfield(L, -2, "pointInCircle");
    lua_pushcfunction(L, lua_engine_collision_lineLine);
    lua_setfield(L, -2, "lineLine");
    lua_pushcfunction(L, lua_engine_collision_lineCircle);
    lua_setfield(L, -2, "lineCircle");
    lua_setfield(L, -2, "collision");              // engine_table.collision = collision_table

    // --- engine.lua sub-table (ENG-06 compat; Phase 35 adds collect/memory) ---
    lua_newtable(L);                               // [engine_table] [lua_table]
    lua_setfield(L, -2, "lua");                    // engine_table.lua = empty table

    // --- engine.log top-level function (ENG-05) ---
    lua_pushcfunction(L, lua_engine_log);
    lua_setfield(L, -2, "log");                    // engine_table.log = fn

    lua_setglobal(L, "engine");                    // pops engine_table; stack is now balanced
}

// --- engine.scene.switch(id) — ENG-01 ---
// Calls SceneStateMachine::switchTo(uint32_t). Silent no-op when SSM is nullptr
// (SDL standalone mode has no SceneStateMachine).
int LuaBindings::lua_engine_scene_switch(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    auto* ssm = static_cast<SceneStateMachine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!ssm) return 0;  // no SSM installed — silent no-op
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    ssm->switchTo(id);
    return 0;
}

// --- engine.scene.find(name) — ENG-02 ---
// Returns lightuserdata (Object*) when found, nil when not found.
// Phase 32 upgrades lightuserdata to full ScriptProxy with metatable.
// Silent nil-return when activeScene is nullptr.
int LuaBindings::lua_engine_scene_find(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto* scene = static_cast<Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!scene) { lua_pushnil(L); return 1; }
    const char* name = luaL_checkstring(L, 1);
    Object* obj = scene->findByName(name);
    if (!obj) {
        lua_pushnil(L);
    } else {
        lua_pushlightuserdata(L, obj);  // Phase 32 upgrades to full proxy
    }
    return 1;
}

// --- engine.input.held(btn) — ENG-03 ---
int LuaBindings::lua_engine_input_held(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->held(btn) ? 1 : 0);
    return 1;
}

// --- engine.input.just_pressed(btn) — ENG-03 ---
int LuaBindings::lua_engine_input_just_pressed(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justPressed(btn) ? 1 : 0);
    return 1;
}

// --- engine.input.just_released(btn) — ENG-03 ---
int LuaBindings::lua_engine_input_just_released(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->justReleased(btn) ? 1 : 0);
    return 1;
}

// --- engine.input.axis(n) — ENG-03 ---
int LuaBindings::lua_engine_input_axis(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushnumber(L, 0.0); return 1; }
    int axis = static_cast<int>(luaL_checkinteger(L, 1));
    float val = (axis >= 0 && axis < 8) ? b->currentInput->axes[axis] : 0.0f;
    lua_pushnumber(L, static_cast<lua_Number>(val));
    return 1;
}

// --- engine.time.delta() — ENG-04 ---
int LuaBindings::lua_engine_time_delta(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushnumber(L, ts ? static_cast<lua_Number>(ts->dt) : 0.0);
    return 1;
}

// --- engine.time.now() — ENG-04 ---
int LuaBindings::lua_engine_time_now(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushnumber(L, ts ? static_cast<lua_Number>(ts->totalTime) : 0.0);
    return 1;
}

// --- engine.time.frame() — ENG-04 ---
int LuaBindings::lua_engine_time_frame(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushinteger(L, ts ? static_cast<lua_Integer>(ts->frameCount) : 0);
    return 1;
}

// --- engine.log(...) — ENG-05 ---
// Uses printf (not std::cout) — compatible with ESP32, Emscripten, and desktop.
// Handles all Lua types: strings/numbers coerce via lua_tostring;
// booleans/tables/nil fall back to lua_typename to avoid nullptr deref in printf.
int LuaBindings::lua_engine_log(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            printf("%s", s);
        } else {
            // lua_tostring returns nullptr for boolean, table, function, nil
            printf("(%s)", lua_typename(L, lua_type(L, i)));
        }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}

//==============================================================================
// engine.collision.* bindings
//==============================================================================

int LuaBindings::lua_engine_collision_aabb(lua_State* L) {
    float x1, y1, w1, h1, x2, y2, w2, h2;
    auto* r1 = static_cast<Rect*>(luaL_testudata(L, 1, "Rect"));
    auto* r2 = static_cast<Rect*>(luaL_testudata(L, 2, "Rect"));
    if (r1 && r2) {
        x1 = static_cast<float>(r1->x); y1 = static_cast<float>(r1->y);
        w1 = static_cast<float>(r1->width); h1 = static_cast<float>(r1->height);
        x2 = static_cast<float>(r2->x); y2 = static_cast<float>(r2->y);
        w2 = static_cast<float>(r2->width); h2 = static_cast<float>(r2->height);
    } else {
        x1 = static_cast<float>(luaL_checknumber(L, 1));
        y1 = static_cast<float>(luaL_checknumber(L, 2));
        w1 = static_cast<float>(luaL_checknumber(L, 3));
        h1 = static_cast<float>(luaL_checknumber(L, 4));
        x2 = static_cast<float>(luaL_checknumber(L, 5));
        y2 = static_cast<float>(luaL_checknumber(L, 6));
        w2 = static_cast<float>(luaL_checknumber(L, 7));
        h2 = static_cast<float>(luaL_checknumber(L, 8));
    }
    lua_pushboolean(L, enjin2::collision::aabb(x1, y1, w1, h1, x2, y2, w2, h2) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_circleCircle(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float r1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    lua_pushboolean(L, enjin2::collision::circleCircle(x1, y1, r1, x2, y2, r2) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_pointInRect(lua_State* L) {
    float px, py, rx, ry, rw, rh;
    auto* p = static_cast<Point*>(luaL_testudata(L, 1, "Point"));
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    auto* r = static_cast<Rect*>(luaL_testudata(L, 2, "Rect"));
    if ((p || v) && r) {
        px = p ? static_cast<float>(p->x) : v->x;
        py = p ? static_cast<float>(p->y) : v->y;
        rx = static_cast<float>(r->x); ry = static_cast<float>(r->y);
        rw = static_cast<float>(r->width); rh = static_cast<float>(r->height);
    } else {
        px = static_cast<float>(luaL_checknumber(L, 1));
        py = static_cast<float>(luaL_checknumber(L, 2));
        rx = static_cast<float>(luaL_checknumber(L, 3));
        ry = static_cast<float>(luaL_checknumber(L, 4));
        rw = static_cast<float>(luaL_checknumber(L, 5));
        rh = static_cast<float>(luaL_checknumber(L, 6));
    }
    lua_pushboolean(L, enjin2::collision::pointInRect(px, py, rx, ry, rw, rh) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_pointInCircle(lua_State* L) {
    float px, py, cx, cy, r;
    auto* p = static_cast<Point*>(luaL_testudata(L, 1, "Point"));
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    if (p || v) {
        px = p ? static_cast<float>(p->x) : v->x;
        py = p ? static_cast<float>(p->y) : v->y;
        cx = static_cast<float>(luaL_checknumber(L, 2));
        cy = static_cast<float>(luaL_checknumber(L, 3));
        r  = static_cast<float>(luaL_checknumber(L, 4));
    } else {
        px = static_cast<float>(luaL_checknumber(L, 1));
        py = static_cast<float>(luaL_checknumber(L, 2));
        cx = static_cast<float>(luaL_checknumber(L, 3));
        cy = static_cast<float>(luaL_checknumber(L, 4));
        r  = static_cast<float>(luaL_checknumber(L, 5));
    }
    lua_pushboolean(L, enjin2::collision::pointInCircle(px, py, cx, cy, r) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_lineLine(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float x3 = static_cast<float>(luaL_checknumber(L, 5));
    float y3 = static_cast<float>(luaL_checknumber(L, 6));
    float x4 = static_cast<float>(luaL_checknumber(L, 7));
    float y4 = static_cast<float>(luaL_checknumber(L, 8));
    float ix, iy;
    bool hit = enjin2::collision::lineLine(x1, y1, x2, y2, x3, y3, x4, y4, &ix, &iy);
    lua_pushboolean(L, hit ? 1 : 0);
    if (hit) {
        lua_pushnumber(L, static_cast<lua_Number>(ix));
        lua_pushnumber(L, static_cast<lua_Number>(iy));
        return 3;
    }
    return 1;
}

int LuaBindings::lua_engine_collision_lineCircle(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float cx = static_cast<float>(luaL_checknumber(L, 5));
    float cy = static_cast<float>(luaL_checknumber(L, 6));
    float r  = static_cast<float>(luaL_checknumber(L, 7));
    lua_pushboolean(L, enjin2::collision::lineCircle(x1, y1, x2, y2, cx, cy, r) ? 1 : 0);
    return 1;
}

//==============================================================================
// Math Bindings — Vec2 / Point / Rect userdata + utility globals
//==============================================================================

static constexpr const char* VEC2_METATABLE  = "Vec2";
static constexpr const char* POINT_METATABLE = "Point";
static constexpr const char* RECT_METATABLE  = "Rect";

// ── Helper: push a new Vec2 userdata with metatable ─────────────────────────

static enjin2::Vec2* pushVec2(lua_State* L, float x, float y) {
    auto* v = static_cast<enjin2::Vec2*>(lua_newuserdata(L, sizeof(enjin2::Vec2)));
    v->x = x;
    v->y = y;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);
    return v;
}

static enjin2::Vec2* checkVec2(lua_State* L, int idx) {
    return static_cast<enjin2::Vec2*>(luaL_checkudata(L, idx, VEC2_METATABLE));
}

static enjin2::Point* pushPoint(lua_State* L, int16_t x, int16_t y) {
    auto* p = static_cast<enjin2::Point*>(lua_newuserdata(L, sizeof(enjin2::Point)));
    p->x = x;
    p->y = y;
    luaL_getmetatable(L, POINT_METATABLE);
    lua_setmetatable(L, -2);
    return p;
}

static enjin2::Point* checkPoint(lua_State* L, int idx) {
    return static_cast<enjin2::Point*>(luaL_checkudata(L, idx, POINT_METATABLE));
}

static enjin2::Rect* pushRect(lua_State* L, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    auto* r = static_cast<enjin2::Rect*>(lua_newuserdata(L, sizeof(enjin2::Rect)));
    r->x = x; r->y = y; r->width = w; r->height = h;
    luaL_getmetatable(L, RECT_METATABLE);
    lua_setmetatable(L, -2);
    return r;
}

static enjin2::Rect* checkRect(lua_State* L, int idx) {
    return static_cast<enjin2::Rect*>(luaL_checkudata(L, idx, RECT_METATABLE));
}

// ── Vec2 constructor ────────────────────────────────────────────────────────

int LuaBindings::lua_Vec2_new(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    pushVec2(L, x, y);
    return 1;
}

// ── Vec2 metamethods ────────────────────────────────────────────────────────

int LuaBindings::lua_Vec2_add(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    pushVec2(L, a->x + b->x, a->y + b->y);
    return 1;
}

int LuaBindings::lua_Vec2_sub(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    pushVec2(L, a->x - b->x, a->y - b->y);
    return 1;
}

int LuaBindings::lua_Vec2_mul(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        float s = static_cast<float>(lua_tonumber(L, 1));
        auto* v = checkVec2(L, 2);
        pushVec2(L, v->x * s, v->y * s);
    } else {
        auto* v = checkVec2(L, 1);
        float s = static_cast<float>(luaL_checknumber(L, 2));
        pushVec2(L, v->x * s, v->y * s);
    }
    return 1;
}

int LuaBindings::lua_Vec2_div(lua_State* L) {
    auto* v = checkVec2(L, 1);
    float s = static_cast<float>(luaL_checknumber(L, 2));
    pushVec2(L, v->x / s, v->y / s);
    return 1;
}

int LuaBindings::lua_Vec2_unm(lua_State* L) {
    auto* v = checkVec2(L, 1);
    pushVec2(L, -v->x, -v->y);
    return 1;
}

int LuaBindings::lua_Vec2_eq(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushboolean(L, (a->x == b->x && a->y == b->y) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Vec2_tostring(lua_State* L) {
    auto* v = checkVec2(L, 1);
    char buf[64];
    snprintf(buf, sizeof(buf), "Vec2(%.4g, %.4g)", v->x, v->y);
    lua_pushstring(L, buf);
    return 1;
}

// ── Vec2 methods (called via __index dispatch) ──────────────────────────────

int LuaBindings::lua_Vec2_length(lua_State* L) {
    auto* v = checkVec2(L, 1);
    lua_pushnumber(L, v->length());
    return 1;
}

int LuaBindings::lua_Vec2_lengthSquared(lua_State* L) {
    auto* v = checkVec2(L, 1);
    lua_pushnumber(L, v->lengthSquared());
    return 1;
}

int LuaBindings::lua_Vec2_normalized(lua_State* L) {
    auto* v = checkVec2(L, 1);
    enjin2::Vec2 n = v->normalized();
    pushVec2(L, n.x, n.y);
    return 1;
}

int LuaBindings::lua_Vec2_dot(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushnumber(L, a->dot(*b));
    return 1;
}

int LuaBindings::lua_Vec2_cross(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushnumber(L, a->cross(*b));
    return 1;
}

int LuaBindings::lua_Vec2_distance(lua_State* L) {
    auto* a = checkVec2(L, 1);
    auto* b = checkVec2(L, 2);
    lua_pushnumber(L, enjin2::Vec2::distance(*a, *b));
    return 1;
}

int LuaBindings::lua_Vec2_angle(lua_State* L) {
    auto* v = checkVec2(L, 1);
    lua_pushnumber(L, v->angle());
    return 1;
}

int LuaBindings::lua_Vec2_rotate(lua_State* L) {
    auto* v = checkVec2(L, 1);
    float rad = static_cast<float>(luaL_checknumber(L, 2));
    enjin2::Vec2 r = v->rotated(rad);
    pushVec2(L, r.x, r.y);
    return 1;
}

// ── Vec2 __index / __newindex ───────────────────────────────────────────────

int LuaBindings::lua_Vec2_index(lua_State* L) {
    auto* v = checkVec2(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x' && key[1] == '\0') { lua_pushnumber(L, v->x); return 1; }
    if (key[0] == 'y' && key[1] == '\0') { lua_pushnumber(L, v->y); return 1; }

    // Look up in methods table stored in registry
    lua_getfield(L, LUA_REGISTRYINDEX, "Vec2_methods");
    lua_pushstring(L, key);
    lua_rawget(L, -2);
    return 1;
}

int LuaBindings::lua_Vec2_newindex(lua_State* L) {
    auto* v = checkVec2(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (key[0] == 'x' && key[1] == '\0') { v->x = static_cast<float>(luaL_checknumber(L, 3)); return 0; }
    if (key[0] == 'y' && key[1] == '\0') { v->y = static_cast<float>(luaL_checknumber(L, 3)); return 0; }

    return luaL_error(L, "Vec2 has no writable field '%s'", key);
}

// ── Point constructor ───────────────────────────────────────────────────────

int LuaBindings::lua_Point_new(lua_State* L) {
    int16_t x = static_cast<int16_t>(luaL_optinteger(L, 1, 0));
    int16_t y = static_cast<int16_t>(luaL_optinteger(L, 2, 0));
    pushPoint(L, x, y);
    return 1;
}

int LuaBindings::lua_Point_add(lua_State* L) {
    auto* a = checkPoint(L, 1);
    auto* b = checkPoint(L, 2);
    pushPoint(L, a->x + b->x, a->y + b->y);
    return 1;
}

int LuaBindings::lua_Point_sub(lua_State* L) {
    auto* a = checkPoint(L, 1);
    auto* b = checkPoint(L, 2);
    pushPoint(L, a->x - b->x, a->y - b->y);
    return 1;
}

int LuaBindings::lua_Point_eq(lua_State* L) {
    auto* a = checkPoint(L, 1);
    auto* b = checkPoint(L, 2);
    lua_pushboolean(L, (a->x == b->x && a->y == b->y) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Point_tostring(lua_State* L) {
    auto* p = checkPoint(L, 1);
    char buf[48];
    snprintf(buf, sizeof(buf), "Point(%d, %d)", static_cast<int>(p->x), static_cast<int>(p->y));
    lua_pushstring(L, buf);
    return 1;
}

int LuaBindings::lua_Point_index(lua_State* L) {
    auto* p = checkPoint(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (key[0] == 'x' && key[1] == '\0') { lua_pushinteger(L, p->x); return 1; }
    if (key[0] == 'y' && key[1] == '\0') { lua_pushinteger(L, p->y); return 1; }
    lua_pushnil(L);
    return 1;
}

int LuaBindings::lua_Point_newindex(lua_State* L) {
    auto* p = checkPoint(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (key[0] == 'x' && key[1] == '\0') { p->x = static_cast<int16_t>(luaL_checkinteger(L, 3)); return 0; }
    if (key[0] == 'y' && key[1] == '\0') { p->y = static_cast<int16_t>(luaL_checkinteger(L, 3)); return 0; }
    return luaL_error(L, "Point has no writable field '%s'", key);
}

// ── Rect constructor ────────────────────────────────────────────────────────

int LuaBindings::lua_Rect_new(lua_State* L) {
    int16_t  x = static_cast<int16_t>(luaL_optinteger(L, 1, 0));
    int16_t  y = static_cast<int16_t>(luaL_optinteger(L, 2, 0));
    uint16_t w = static_cast<uint16_t>(luaL_optinteger(L, 3, 0));
    uint16_t h = static_cast<uint16_t>(luaL_optinteger(L, 4, 0));
    pushRect(L, x, y, w, h);
    return 1;
}

int LuaBindings::lua_Rect_eq(lua_State* L) {
    auto* a = checkRect(L, 1);
    auto* b = checkRect(L, 2);
    lua_pushboolean(L, (a->x == b->x && a->y == b->y &&
                        a->width == b->width && a->height == b->height) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Rect_tostring(lua_State* L) {
    auto* r = checkRect(L, 1);
    char buf[80];
    snprintf(buf, sizeof(buf), "Rect(%d, %d, %u, %u)",
             static_cast<int>(r->x), static_cast<int>(r->y),
             static_cast<unsigned>(r->width), static_cast<unsigned>(r->height));
    lua_pushstring(L, buf);
    return 1;
}

int LuaBindings::lua_Rect_contains(lua_State* L) {
    auto* r = checkRect(L, 1);
    int16_t px = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t py = static_cast<int16_t>(luaL_checkinteger(L, 3));
    lua_pushboolean(L, r->contains(px, py) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Rect_intersects(lua_State* L) {
    auto* a = checkRect(L, 1);
    auto* b = checkRect(L, 2);
    lua_pushboolean(L, a->intersects(*b) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_Rect_index(lua_State* L) {
    auto* r = checkRect(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "x") == 0)      { lua_pushinteger(L, r->x);      return 1; }
    if (strcmp(key, "y") == 0)      { lua_pushinteger(L, r->y);      return 1; }
    if (strcmp(key, "width") == 0)  { lua_pushinteger(L, r->width);  return 1; }
    if (strcmp(key, "height") == 0) { lua_pushinteger(L, r->height); return 1; }

    lua_getfield(L, LUA_REGISTRYINDEX, "Rect_methods");
    lua_pushstring(L, key);
    lua_rawget(L, -2);
    return 1;
}

int LuaBindings::lua_Rect_newindex(lua_State* L) {
    auto* r = checkRect(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "x") == 0)      { r->x      = static_cast<int16_t>(luaL_checkinteger(L, 3));  return 0; }
    if (strcmp(key, "y") == 0)      { r->y      = static_cast<int16_t>(luaL_checkinteger(L, 3));  return 0; }
    if (strcmp(key, "width") == 0)  { r->width  = static_cast<uint16_t>(luaL_checkinteger(L, 3)); return 0; }
    if (strcmp(key, "height") == 0) { r->height = static_cast<uint16_t>(luaL_checkinteger(L, 3)); return 0; }

    return luaL_error(L, "Rect has no writable field '%s'", key);
}

// ── Math utility globals ────────────────────────────────────────────────────

int LuaBindings::lua_math_clamp(lua_State* L) {
    float val = static_cast<float>(luaL_checknumber(L, 1));
    float lo  = static_cast<float>(luaL_checknumber(L, 2));
    float hi  = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, enjin2::math::clamp(val, lo, hi));
    return 1;
}

int LuaBindings::lua_math_lerp(lua_State* L) {
    float a = static_cast<float>(luaL_checknumber(L, 1));
    float b = static_cast<float>(luaL_checknumber(L, 2));
    float t = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, enjin2::math::lerp(a, b, t));
    return 1;
}

int LuaBindings::lua_math_remap(lua_State* L) {
    float val    = static_cast<float>(luaL_checknumber(L, 1));
    float in_lo  = static_cast<float>(luaL_checknumber(L, 2));
    float in_hi  = static_cast<float>(luaL_checknumber(L, 3));
    float out_lo = static_cast<float>(luaL_checknumber(L, 4));
    float out_hi = static_cast<float>(luaL_checknumber(L, 5));
    lua_pushnumber(L, enjin2::math::map(val, in_lo, in_hi, out_lo, out_hi));
    return 1;
}

int LuaBindings::lua_math_sign(lua_State* L) {
    float val = static_cast<float>(luaL_checknumber(L, 1));
    lua_pushnumber(L, enjin2::math::sign(val));
    return 1;
}

int LuaBindings::lua_math_smoothstep(lua_State* L) {
    float e0 = static_cast<float>(luaL_checknumber(L, 1));
    float e1 = static_cast<float>(luaL_checknumber(L, 2));
    float x  = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, enjin2::math::smoothstep(e0, e1, x));
    return 1;
}

int LuaBindings::lua_math_distance(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float dx = x2 - x1;
    float dy = y2 - y1;
    lua_pushnumber(L, std::sqrt(dx * dx + dy * dy));
    return 1;
}

// ── registerMathBindings ────────────────────────────────────────────────────

void LuaBindings::registerMathBindings() {
    lua_State* L = engine->getState();
    if (!L) return;

    // ── Vec2 metatable ──────────────────────────────────────────────────
    if (luaL_newmetatable(L, VEC2_METATABLE)) {
        lua_pushcfunction(L, lua_Vec2_add);      lua_setfield(L, -2, "__add");
        lua_pushcfunction(L, lua_Vec2_sub);      lua_setfield(L, -2, "__sub");
        lua_pushcfunction(L, lua_Vec2_mul);      lua_setfield(L, -2, "__mul");
        lua_pushcfunction(L, lua_Vec2_div);      lua_setfield(L, -2, "__div");
        lua_pushcfunction(L, lua_Vec2_unm);      lua_setfield(L, -2, "__unm");
        lua_pushcfunction(L, lua_Vec2_eq);       lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, lua_Vec2_tostring); lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, lua_Vec2_index);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_Vec2_newindex); lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);

    // Vec2 methods table in registry for __index dispatch
    {
        struct { const char* name; lua_CFunction func; } methods[] = {
            {"length",        lua_Vec2_length},
            {"lengthSquared", lua_Vec2_lengthSquared},
            {"normalized",    lua_Vec2_normalized},
            {"dot",           lua_Vec2_dot},
            {"cross",         lua_Vec2_cross},
            {"distance",      lua_Vec2_distance},
            {"angle",         lua_Vec2_angle},
            {"rotate",        lua_Vec2_rotate},
        };
        lua_newtable(L);
        for (auto& m : methods) {
            lua_pushcfunction(L, m.func);
            lua_setfield(L, -2, m.name);
        }
        lua_setfield(L, LUA_REGISTRYINDEX, "Vec2_methods");
    }

    // ── Point metatable ─────────────────────────────────────────────────
    if (luaL_newmetatable(L, POINT_METATABLE)) {
        lua_pushcfunction(L, lua_Point_add);      lua_setfield(L, -2, "__add");
        lua_pushcfunction(L, lua_Point_sub);      lua_setfield(L, -2, "__sub");
        lua_pushcfunction(L, lua_Point_eq);       lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, lua_Point_tostring); lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, lua_Point_index);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_Point_newindex); lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);

    // ── Rect metatable ──────────────────────────────────────────────────
    if (luaL_newmetatable(L, RECT_METATABLE)) {
        lua_pushcfunction(L, lua_Rect_eq);       lua_setfield(L, -2, "__eq");
        lua_pushcfunction(L, lua_Rect_tostring); lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, lua_Rect_index);    lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_Rect_newindex); lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);

    // Rect methods table in registry for __index dispatch
    {
        struct { const char* name; lua_CFunction func; } methods[] = {
            {"contains",   lua_Rect_contains},
            {"intersects", lua_Rect_intersects},
        };
        lua_newtable(L);
        for (auto& m : methods) {
            lua_pushcfunction(L, m.func);
            lua_setfield(L, -2, m.name);
        }
        lua_setfield(L, LUA_REGISTRYINDEX, "Rect_methods");
    }

    // ── Global constructors ─────────────────────────────────────────────
    lua_pushcfunction(L, lua_Vec2_new);  lua_setglobal(L, "Vec2");
    lua_pushcfunction(L, lua_Point_new); lua_setglobal(L, "Point");
    lua_pushcfunction(L, lua_Rect_new);  lua_setglobal(L, "Rect");

    // ── Math utility globals ────────────────────────────────────────────
    lua_pushcfunction(L, lua_math_clamp);      lua_setglobal(L, "clamp");
    lua_pushcfunction(L, lua_math_lerp);       lua_setglobal(L, "lerp");
    lua_pushcfunction(L, lua_math_remap);      lua_setglobal(L, "remap");
    lua_pushcfunction(L, lua_math_sign);       lua_setglobal(L, "sign");
    lua_pushcfunction(L, lua_math_smoothstep); lua_setglobal(L, "smoothstep");
    lua_pushcfunction(L, lua_math_distance);   lua_setglobal(L, "distance");
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