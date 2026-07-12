#include "bindings_internal.hpp"
#include "../../include/enjin2/graphics/defaultfont.hpp"
#include "../../include/enjin2/graphics/text_renderer.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/state_machine.hpp"
#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/components/camera.hpp"
#include "../../include/enjin2/core/object.hpp"

namespace enjin2 {

// Global pointer to current bindings instance for static callbacks
static LuaBindings* g_currentBindings = nullptr;

//==============================================================================
// ScriptProxy Metatable Implementation
//==============================================================================

// Forward declarations for tag method implementations (defined after __newindex)
static int lua_proxy_addTag_impl(lua_State* L);
static int lua_proxy_hasTag_impl(lua_State* L);
static int lua_proxy_clearTags_impl(lua_State* L);

// Forward declaration for self:get("TypeName") — ComponentProxy dispatch (Phase 39)
static int lua_proxy_get_component_impl(lua_State* L);

// __index metamethod: called when Lua reads self.property
// Stack layout on entry: [1]=userdata(self), [2]=key_string
static int lua_proxy_index_impl(lua_State* L) {
    if (!lua_isuserdata(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(lua_touserdata(L, 1));
    if (!proxy) {
        lua_pushnil(L);
        return 1;
    }
    if (!proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;  // unreachable — luaL_error longjmps
    }

    const char* key = lua_tostring(L, 2);
    if (!key) {
        lua_pushnil(L);
        return 1;
    }

    // PROXY-01: self:get("TypeName") — checked FIRST before any property (PROXY-04b collision prevention)
    if (strcmp(key, "get") == 0) {
        lua_pushcfunction(L, lua_proxy_get_component_impl);
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
    } else if (strcmp(key, "addTag") == 0) {
        lua_pushcfunction(L, lua_proxy_addTag_impl);
        return 1;
    } else if (strcmp(key, "hasTag") == 0) {
        lua_pushcfunction(L, lua_proxy_hasTag_impl);
        return 1;
    } else if (strcmp(key, "clearTags") == 0) {
        lua_pushcfunction(L, lua_proxy_clearTags_impl);
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
    if (!proxy) return 0;
    if (!proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;  // unreachable — luaL_error longjmps
    }

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

// lua_proxy_addTag_impl: stack [1]=proxy userdata (self), [2]=tag_string
// Called as self:addTag("enemy") from Lua
static int lua_proxy_addTag_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* tag = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    if (owner) owner->addTag(tag);
    return 0;
}

// lua_proxy_hasTag_impl: stack [1]=proxy, [2]=tag_string -> returns boolean
static int lua_proxy_hasTag_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* tag = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    bool result = owner ? owner->hasTag(tag) : false;
    lua_pushboolean(L, result ? 1 : 0);
    return 1;
}

// lua_proxy_clearTags_impl: stack [1]=proxy -- clears all tags on the owner Object
static int lua_proxy_clearTags_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    enjin2::Object* owner = proxy->component->getOwner();
    if (owner) owner->clearTags();
    return 0;
}

// lua_proxy_get_component_impl: stack [1]=ScriptProxy userdata (self), [2]=type_name_string
// Called as self:get("C_Position") from Lua — returns ComponentProxy userdata or nil
static int lua_proxy_get_component_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }

    const char* typeName = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    if (!owner) { lua_pushnil(L); return 1; }

    // Type dispatch — each proxied component type has an entry here.
    // Phase 40 adds: C_Timer. Phase 41 adds: C_StateMachine.
    enjin2::Component* comp = nullptr;
    const char* metaName = nullptr;

    if (strcmp(typeName, "C_Position") == 0) {
        comp = owner->getComponent<enjin2::C_Position>();
        metaName = "C_Position_Proxy";
    } else if (strcmp(typeName, "C_Timer") == 0) {
        comp = owner->getComponent<enjin2::C_Timer>();
        metaName = "C_Timer_Proxy";
    } else if (strcmp(typeName, "C_StateMachine") == 0) {
        comp = owner->getComponent<enjin2::C_StateMachine>();
        metaName = "C_StateMachine_Proxy";
    } else if (strcmp(typeName, "C_Tilemap") == 0) {
        comp = owner->getComponent<enjin2::C_Tilemap>();
        metaName = "C_Tilemap_Proxy";
    } else if (strcmp(typeName, "C_Camera") == 0) {
        comp = owner->getComponent<enjin2::C_Camera>();
        metaName = "C_Camera_Proxy";
    }

    if (!comp) { lua_pushnil(L); return 1; }

    // Allocate ComponentProxy userdata with per-type metatable
    auto* cproxy = static_cast<enjin2::ComponentProxy*>(
        lua_newuserdata(L, sizeof(enjin2::ComponentProxy)));
    cproxy->component = comp;
    cproxy->valid = true;
    luaL_getmetatable(L, metaName);
    lua_setmetatable(L, -2);

    // Register proxy with component for destructor invalidation
    // Note: overwrites any previous proxy (single-proxy-per-component constraint — accepted v1.6 limitation)
    comp->setLuaProxy(cproxy);

    return 1;
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
    const GFXfont* gfxFont = font;
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
    const GFXfont* gfxFont = font;
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
    const GFXfont* gfxFont = font;
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
    const GFXfont* gfxFont = font;
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

    // Reset game state machine
    strncpy(m_currentGameState, "none", sizeof(m_currentGameState) - 1);
    m_currentGameState[sizeof(m_currentGameState) - 1] = '\0';
    // Unref any previously registered callbacks (guard against double-registerAll without lua_close)
    for (int i = 0; i < m_stateCount; ++i) {
        // Note: we don't have L yet at this point; L is retrieved below.
        // We'll defer the unref to after L is available.
        (void)i;
    }
    m_stateCount = 0;
    for (int i = 0; i < MAX_GAME_STATES; ++i) {
        m_stateOnEnterRefs[i] = LUA_NOREF;
        m_stateOnExitRefs[i]  = LUA_NOREF;
        m_stateNames[i][0]    = '\0';
    }

    // Store bindings instance in Lua registry
    lua_State* L = engine->getState();
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_bindings");

    // Store injected engine.* pointers in registry for closure retrieval
    lua_pushlightuserdata(L, &m_ssm);          // store address-of-member (pointer-to-pointer)
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    lua_pushlightuserdata(L, &m_activeScene);  // store address-of-member (pointer-to-pointer)
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    lua_pushlightuserdata(L, &m_timeState);   // always valid (member of LuaBindings)
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_time");

    // Re-enable debug draw on every hot-reload
    m_debugEnabled = true;

    // EVENT-05: clear event bus handlers from previous load (hot-reload cleanup)
    m_eventBus.clearHandlers();
    m_eventBus.setLuaState(L);
    // ASYNC-03: clear coroutine pool on every hot-reload (clean slate)
    clearCoroutines();
    clearTweens();     // TWEEN-02: clean slate on every hot-reload
    m_followTargetProxy = nullptr;  // DEBT-01: clear follow target on hot reload
    m_deadZoneW = 0.0f;             // Phase 57 QOL-03: clear dead zone on hot reload
    m_deadZoneH = 0.0f;

    // Store event bus pointer in registry for engine.event.* closures
    lua_pushlightuserdata(L, &m_eventBus);
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_event_bus");

    // === gfx.* namespace table ===
    lua_newtable(L);

    // Canvas
    lua_pushcfunction(L, lua_getWidth);       lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, lua_getHeight);      lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, lua_clear);          lua_setfield(L, -2, "clear");

    // Drawing state
    lua_pushcfunction(L, lua_setColor);       lua_setfield(L, -2, "setColor");
    lua_pushcfunction(L, lua_getColor);       lua_setfield(L, -2, "getColor");
    lua_pushcfunction(L, lua_setLineWidth);   lua_setfield(L, -2, "setLineWidth");
    lua_pushcfunction(L, lua_getLineWidth);   lua_setfield(L, -2, "getLineWidth");

    // Primitives
    lua_pushcfunction(L, lua_point);          lua_setfield(L, -2, "point");
    lua_pushcfunction(L, lua_line);           lua_setfield(L, -2, "line");
    lua_pushcfunction(L, lua_rectangle);      lua_setfield(L, -2, "rectangle");
    lua_pushcfunction(L, lua_circle);         lua_setfield(L, -2, "circle");
    lua_pushcfunction(L, lua_triangle);       lua_setfield(L, -2, "triangle");

    // Pixel access
    lua_pushcfunction(L, lua_setPixel);       lua_setfield(L, -2, "setPixel");
    lua_pushcfunction(L, lua_getPixel);       lua_setfield(L, -2, "getPixel");

    // Fast drawing
    lua_pushcfunction(L, lua_fastFillRect);   lua_setfield(L, -2, "fastFillRect");
    lua_pushcfunction(L, lua_fastDrawLine);   lua_setfield(L, -2, "fastDrawLine");

    // Palette
    lua_pushcfunction(L, lua_setPaletteColor); lua_setfield(L, -2, "setPaletteColor");
    lua_pushcfunction(L, lua_getPaletteColor); lua_setfield(L, -2, "getPaletteColor");
    lua_pushcfunction(L, lua_loadPalette);     lua_setfield(L, -2, "loadPalette");
    lua_pushcfunction(L, lua_getPaletteSize);  lua_setfield(L, -2, "getPaletteSize");

    // Sprites
    lua_pushcfunction(L, lua_newSprite);      lua_setfield(L, -2, "newSprite");
    lua_pushcfunction(L, lua_freeSprite);     lua_setfield(L, -2, "freeSprite");
    lua_pushcfunction(L, lua_drawSprite);     lua_setfield(L, -2, "drawSprite");
    lua_pushcfunction(L, lua_updateSprite);   lua_setfield(L, -2, "updateSprite");
    lua_pushcfunction(L, lua_setFrame);       lua_setfield(L, -2, "setFrame");

    // Layers
    lua_pushcfunction(L, lua_setLayer);       lua_setfield(L, -2, "setLayer");
    lua_pushcfunction(L, lua_getLayer);       lua_setfield(L, -2, "getLayer");
    lua_pushcfunction(L, lua_clearLayer);     lua_setfield(L, -2, "clearLayer");
    lua_pushcfunction(L, lua_getLayerCount);  lua_setfield(L, -2, "getLayerCount");
    lua_pushcfunction(L, lua_setLayerVisible); lua_setfield(L, -2, "setLayerVisible");
    lua_pushcfunction(L, lua_isLayerVisible); lua_setfield(L, -2, "isLayerVisible");

    // Text
    lua_pushcfunction(L, lua_text);           lua_setfield(L, -2, "text");
    lua_pushcfunction(L, lua_textWrapped);    lua_setfield(L, -2, "textWrapped");
    lua_pushcfunction(L, lua_textCentered);   lua_setfield(L, -2, "textCentered");
    lua_pushcfunction(L, lua_textAligned);    lua_setfield(L, -2, "textAligned");
    lua_pushcfunction(L, lua_setTextSize);    lua_setfield(L, -2, "setTextSize");
    lua_pushcfunction(L, lua_getTextSize);    lua_setfield(L, -2, "getTextSize");
    lua_pushcfunction(L, lua_setFont);        lua_setfield(L, -2, "setFont");
    lua_pushcfunction(L, lua_getFont);        lua_setfield(L, -2, "getFont");
    lua_pushcfunction(L, lua_getTextWidth);   lua_setfield(L, -2, "getTextWidth");
    lua_pushcfunction(L, lua_getTextHeight);  lua_setfield(L, -2, "getTextHeight");

    // Layer constants nested under gfx (Lua 1-indexed)
    lua_pushinteger(L, 1); lua_setfield(L, -2, "LAYER_BG");
    lua_pushinteger(L, 2); lua_setfield(L, -2, "LAYER_MID");
    lua_pushinteger(L, 3); lua_setfield(L, -2, "LAYER_FG");
    lua_pushinteger(L, 4); lua_setfield(L, -2, "LAYER_UI");
    lua_pushinteger(L, 5); lua_setfield(L, -2, "LAYER_DEBUG");

    // COLOR table nested under gfx
    lua_newtable(L);
    lua_pushinteger(L, 0);  lua_setfield(L, -2, "BLACK");
    lua_pushinteger(L, 1);  lua_setfield(L, -2, "DARK_BLUE");
    lua_pushinteger(L, 2);  lua_setfield(L, -2, "DARK_RED");
    lua_pushinteger(L, 3);  lua_setfield(L, -2, "DARK_GREEN");
    lua_pushinteger(L, 4);  lua_setfield(L, -2, "BROWN");
    lua_pushinteger(L, 5);  lua_setfield(L, -2, "DARK_GRAY");
    lua_pushinteger(L, 6);  lua_setfield(L, -2, "GRAY");
    lua_pushinteger(L, 7);  lua_setfield(L, -2, "WHITE");
    lua_pushinteger(L, 8);  lua_setfield(L, -2, "RED");
    lua_pushinteger(L, 9);  lua_setfield(L, -2, "ORANGE");
    lua_pushinteger(L, 10); lua_setfield(L, -2, "YELLOW");
    lua_pushinteger(L, 11); lua_setfield(L, -2, "GREEN");
    lua_pushinteger(L, 12); lua_setfield(L, -2, "BLUE");
    lua_pushinteger(L, 13); lua_setfield(L, -2, "INDIGO");
    lua_pushinteger(L, 14); lua_setfield(L, -2, "PINK");
    lua_pushinteger(L, 15); lua_setfield(L, -2, "TRANSPARENT");
    lua_setfield(L, -2, "COLOR");  // gfx.COLOR

    lua_setglobal(L, "gfx");

    // === print() stays as bare global ===
    engine->registerFunction("print", lua_print);

    // Pre-register built-in 8pt font so setFont("default8") works
    registerFont("default8", &defaultFont8pt7b);

    // === BTN stays as bare global ===
    // Indices match InputState button order: 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT, 4=A(Z), 5=B(X), 6=START
    lua_newtable(L);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "UP");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "DOWN");
    lua_pushinteger(L, 2); lua_setfield(L, -2, "LEFT");
    lua_pushinteger(L, 3); lua_setfield(L, -2, "RIGHT");
    lua_pushinteger(L, 4); lua_setfield(L, -2, "A");
    lua_pushinteger(L, 5); lua_setfield(L, -2, "B");
    lua_pushinteger(L, 6); lua_setfield(L, -2, "START");
    lua_setglobal(L, "BTN");

    // Register engine.* global table (ENG-06: must be before any script loads)
    registerEngineTable();

    // Register ScriptProxy metatable for C_LuaScript component path
    registerProxyMetatable();

    // Register ObjectProxy metatable for engine.scene.find() return value (Phase 37)
    registerObjectProxyMetatable();

    // Register ComponentProxy metatables for self:get() return values (Phase 39)
    registerComponentProxyMetatable();

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
        loadedAssets_[i] = SpriteAsset{};
    }
    assetBufferUsed_ = 0;
    currentTextSize = 1;
    currentFont = nullptr;
    strncpy(currentFontName, "default", 31);
    currentFontName[31] = '\0';
    m_rngState = 0x12345678;  // reset RNG on reload
}

LuaBindings* LuaBindings::getBindings(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
    LuaBindings* bindings = static_cast<LuaBindings*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return bindings;
}

const enjin2::SpriteSheet* LuaBindings::getSpriteSheet(int handle) const {
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !spritePool[handle].active)
        return nullptr;
    return &spritePool[handle].sheet;
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

void LuaBindings::setActiveScene(Scene* scene) {
    if (scene != m_activeScene) {
        // EVENT-04: Scene is changing -- clear event bus for the outgoing scene.
        // This ensures no stale handlers carry over to the new scene.
        m_eventBus.clearHandlers();
        // CAM-08: Clear cached camera pointer on scene change (Phase 44).
        m_activeCamera = nullptr;
        // ASYNC-03: clear coroutines on scene transition (prevent stale refs)
        clearCoroutines();
        clearTweens();     // TWEEN-02: clean slate on scene transition
        m_followTargetProxy = nullptr;  // DEBT-01: clear follow target on scene change
        m_deadZoneW = 0.0f;             // Phase 57 QOL-03: clear dead zone on scene change
        m_deadZoneH = 0.0f;
    }
    m_activeScene = scene;
}

} // namespace enjin2
