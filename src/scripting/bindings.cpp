#include "bindings_internal.hpp"
#include "../../include/enjin2/scripting/effect_lua.hpp"
#include "../../include/enjin2/scripting/lua_api.hpp"
#include "../../include/enjin2/graphics/numerals.hpp"
#include "../../include/enjin2/graphics/palette.hpp"
#include "../../include/enjin2/graphics/defaultfont.hpp"
#include "../../include/enjin2/graphics/text_renderer.hpp"
#include "../../include/enjin2/graphics/blit.hpp"
#include "../../include/enjin2/graphics/layer_compositor.hpp"
#include <cmath>
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

// Forward declaration for self:add("TypeName"[, params]) — attach verb (ADR-0003 §2)
static int lua_proxy_add_component_impl(lua_State* L);

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
    // self:add("TypeName"[, params]) — the one attach verb (ADR-0003 §2).
    if (strcmp(key, "add") == 0) {
        lua_pushcfunction(L, lua_proxy_add_component_impl);
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
    // Registry-generated dispatch (ADR-0003 §2): the type→proxy mapping lives
    // only in ENJIN2_COMPONENT_LIST, so adding a component there makes it
    // get()-able here with no edit to this function.
    return pushComponentGet(L, owner, typeName);
}

// lua_proxy_add_component_impl: stack [1]=ScriptProxy (self), [2]=type_name,
// [3]=optional params table. Attaches the named component (or returns the
// existing one) and hands back its writable ComponentProxy — the `add` half of
// the one attach verb (ADR-0003 §2). Mirrors ObjectProxy:add.
static int lua_proxy_add_component_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* typeName = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    return pushComponentAdd(L, owner, typeName, 3);  // optional params table at arg 3
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

void LuaCanvas::strokeBorder(int16_t x, int16_t y, uint16_t width, uint16_t height,
                             const BorderStyle& style) {
    Rect rect(x, y, width, height);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        enjin2::strokeBorder(*canvas, rect, style);
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        enjin2::strokeBorder(*canvas, rect, style);
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

void LuaCanvas::fillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        enjin2::fillEllipse(*canvas, cx, cy, rx, ry, Pixel4(color));
    } else {
        // 8-bit path (unused by the 4-bit UI stack): a plain scanline fill via
        // setPixel, since blit.hpp's fillEllipse is Pixel4-native.
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        if (rx <= 0 || ry <= 0) return;
        const float rxf = static_cast<float>(rx);
        const float ryf = static_cast<float>(ry);
        for (int16_t dy = -ry; dy <= ry; ++dy) {
            const float ny = static_cast<float>(dy) / ryf;
            const float inside = 1.0f - ny * ny;
            if (inside < 0.0f) continue;
            const int16_t hx = static_cast<int16_t>(rxf * std::sqrt(inside));
            for (int16_t x = static_cast<int16_t>(cx - hx); x <= static_cast<int16_t>(cx + hx); ++x)
                canvas->setPixel(x, static_cast<int16_t>(cy + dy), color);
        }
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
    // Style slots (#19/#37): a fresh applet starts on the ROM theme defaults —
    // clearing the mask discards every override without touching the values.
    m_styleSetMask = 0;

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
    static constexpr LuaApiEnum kShapeMode = luaApiEnum("ShapeMode", kShapeModeNames);
    static constexpr LuaApiEnum kAlign = luaApiEnum("Align", kNumberAlignNames);
    static constexpr LuaApiEnum kPalettePreset = luaApiEnum("PalettePreset", kPalettePresetNames);

    // Palette indices are authored directly; only transparency is named.
    static constexpr LuaApiEntry kGfxColor[] = {
        luaConstant("TRANSPARENT", 15, "Palette index 15: nothing is drawn, the layers below show."),
    };
    static constexpr LuaApiModule kGfxColorModule = luaApiModule(
        LuaApiScope::Table, "gfx.COLOR", "Named palette indices.", kGfxColor);

    static constexpr LuaApiEntry kGfx[] = {
        // Canvas
        luaFunction("getWidth", lua_getWidth, "() -> int",
                    "Width of the active layer in pixels; 0 with no canvas."),
        luaFunction("getHeight", lua_getHeight, "() -> int",
                    "Height of the active layer in pixels; 0 with no canvas."),
        luaFunction("clear", lua_clear, "(color:int?=current) -> nil",
                    "Fill the active layer with one colour.",
                    "color: palette index; defaults to the current draw colour"),

        // Drawing state
        luaFunction("setColor", lua_setColor, "(color:int) -> nil",
                    "Set the draw colour the shapes and text use.",
                    "color: palette index; a non-number is ignored")
            .note("Starts at 15 (transparent) after every reload."),
        luaFunction("getColor", lua_getColor, "() -> int", "The current draw colour."),
        luaFunction("setLineWidth", lua_setLineWidth, "(width:int) -> nil",
                    "Set the stroke width of gfx.line and of the \"line\" shape modes.",
                    "width: pixels, clamped to 1..255")
            .note("Starts at 1 after every reload."),
        luaFunction("getLineWidth", lua_getLineWidth, "() -> int", "The current stroke width."),

        // Primitives
        luaFunction("point", lua_point, "(x:number, y:number) -> nil",
                    "Plot one pixel in the draw colour; coordinates are rounded.",
                    "x: column\n"
                    "y: row"),
        luaFunction("line", lua_line, "(x1:number, y1:number, x2:number, y2:number) -> nil",
                    "Draw a line in the draw colour, gfx.getLineWidth() pixels wide.",
                    "x1: start column\n"
                    "y1: start row\n"
                    "x2: end column\n"
                    "y2: end row"),
        luaFunction("rectangle", lua_rectangle,
                    "(x:number, y:number, w:int, h:int) -> nil\n"
                    "(mode:ShapeMode, x:number, y:number, w:int, h:int) -> nil",
                    "Draw a rectangle in the draw colour: filled, or outlined in \"line\" mode.",
                    "x: left edge (rounded)\n"
                    "y: top edge (rounded)\n"
                    "w: width; a fractional number draws nothing\n"
                    "h: height; a fractional number draws nothing\n"
                    "mode: \"fill\", or any other name for an outline that grows inward")
            .withEnum(kShapeMode),
        luaFunction("circle", lua_circle,
                    "(x:number, y:number, r:int) -> nil\n"
                    "(mode:ShapeMode, x:number, y:number, r:int) -> nil",
                    "Draw a circle in the draw colour: filled, or outlined in \"line\" mode.",
                    "x: centre column (rounded)\n"
                    "y: centre row (rounded)\n"
                    "r: radius; a fractional number counts as 0\n"
                    "mode: \"fill\", or any other name for a ring that grows inward")
            .withEnum(kShapeMode),
        luaFunction("fillEllipse", lua_fillEllipse,
                    "(cx:number, cy:number, rx:int, ry:int) -> nil",
                    "Draw a filled ellipse in the draw colour.",
                    "cx: centre column (rounded)\n"
                    "cy: centre row (rounded)\n"
                    "rx: horizontal radius; 0 or a fractional number draws nothing\n"
                    "ry: vertical radius; 0 or a fractional number draws nothing"),
        luaFunction("triangle", lua_triangle,
                    "(x1:number, y1:number, x2:number, y2:number, x3:number, y3:number) -> nil\n"
                    "(mode:ShapeMode, x1:number, y1:number, x2:number, y2:number, x3:number, "
                    "y3:number) -> nil",
                    "Draw a triangle in the draw colour: filled, or outlined in \"line\" mode.",
                    "x1: first corner column\n"
                    "y1: first corner row\n"
                    "x2: second corner column\n"
                    "y2: second corner row\n"
                    "x3: third corner column\n"
                    "y3: third corner row\n"
                    "mode: \"fill\", or any other name for the outline")
            .withEnum(kShapeMode),

        // Pixel access
        luaFunction("setPixel", lua_setPixel, "(x:int, y:int, color:int) -> nil",
                    "Set one pixel of the active layer to the given colour.",
                    "x: column\n"
                    "y: row\n"
                    "color: palette index (not the draw colour)"),
        luaFunction("getPixel", lua_getPixel, "(x:int, y:int) -> int",
                    "The palette index of one pixel of the active layer, not of the composited frame.",
                    "x: column\n"
                    "y: row"),

        // Fast drawing
        luaFunction("fastFillRect", lua_fastFillRect,
                    "(x:number, y:number, w:number, h:number, color:int?=current) -> nil",
                    "Fill a rectangle clipped to the layer; values are truncated, not rounded.",
                    "x: left edge\n"
                    "y: top edge\n"
                    "w: width\n"
                    "h: height\n"
                    "color: palette index; defaults to the draw colour"),
        luaFunction("fastDrawLine", lua_fastDrawLine,
                    "(x1:number, y1:number, x2:number, y2:number, color:int?=current) -> nil",
                    "Draw a 1 px line clipped to the layer; values are truncated, not rounded.",
                    "x1: start column\n"
                    "y1: start row\n"
                    "x2: end column\n"
                    "y2: end row\n"
                    "color: palette index; defaults to the draw colour"),

        // Palette
        luaFunction("setPaletteColor", lua_setPaletteColor,
                    "(index:int, hex:string) -> nil\n"
                    "(index:int, r:int, g:int, b:int) -> nil",
                    "Change the colour of one palette entry.",
                    "index: palette index; 15 is ignored and larger values wrap\n"
                    "hex: \"#rrggbb\" (the # is optional); an invalid string sets black\n"
                    "r: red 0..255\n"
                    "g: green 0..255\n"
                    "b: blue 0..255")
            .note("The palette is global to the process, not to one applet."),
        luaFunction("getPaletteColor", lua_getPaletteColor, "(index:int) -> r:int, g:int, b:int",
                    "The colour of one palette entry; index 15 gives 0, 0, 0.",
                    "index: palette index; larger values wrap"),
        luaFunction("loadPalette", lua_loadPalette, "(name:PalettePreset) -> boolean",
                    "Replace the palette with a built-in preset; false for an unknown name.",
                    "name: the preset; \"gameboy\" has 4 colours, so indices wrap mod 4")
            .withEnum(kPalettePreset)
            .note("The palette is global to the process, not to one applet."),
        luaFunction("getPaletteSize", lua_getPaletteSize, "() -> int",
                    "How many colours the current palette has."),

        // Sprites
        luaFunction("newSprite", lua_newSprite,
                    "(data:userdata, cellW:int, cellH:int, cols:int, rows:int) -> handle:int",
                    "Make a sprite sheet from host-owned pixels; -1 when all 16 slots are busy.",
                    "data: light userdata the C host passes; Lua values cannot supply pixels\n"
                    "cellW: frame width\n"
                    "cellH: frame height\n"
                    "cols: frames per row\n"
                    "rows: rows of frames")
            .note("The sprite starts on frame 0, looping at 8 fps."),
        luaFunction("freeSprite", lua_freeSprite, "(handle:int) -> nil",
                    "Free a sprite slot; unknown handles are ignored.",
                    "handle: the sprite")
            .note("Pixel memory comes back only when this was the newest loaded sprite; "
                  "otherwise at the next reload."),
        luaFunction("drawSprite", lua_drawSprite,
                    "(handle:int, x:int, y:int, flipH:boolean?=false, flipV:boolean?=false, "
                    "rotate90:boolean?=false, fx:Effect?) -> nil",
                    "Draw a sprite's current frame with its top-left at (x, y); index 15 is skipped.",
                    "handle: the sprite; unknown handles draw nothing\n"
                    "x: left edge; a fractional number raises\n"
                    "y: top edge; a fractional number raises\n"
                    "flipH: mirror left to right\n"
                    "flipV: mirror top to bottom\n"
                    "rotate90: turn 90 degrees clockwise\n"
                    "fx: an index shader from gfx.effect, run at each destination pixel"),
        luaFunction("updateSprite", lua_updateSprite, "(handle:int, dt:number) -> nil",
                    "Advance a sprite's animation by dt seconds.",
                    "handle: the sprite\n"
                    "dt: elapsed seconds"),
        luaFunction("setFrame", lua_setFrame, "(handle:int, frame:int) -> nil",
                    "Show one frame and restart its timer.",
                    "handle: the sprite\n"
                    "frame: 0-based frame, clamped to the sheet"),

        // HUD numerals (#83): digit-strip number / timer draws.
        luaFunction("number", lua_number, "(x:int, y:int, value:int, opts:table) -> width:int",
                    "Draw a number from a digit-strip sprite (frame d is digit d); returns its width.",
                    "x: anchor column, see opts.align\n"
                    "y: top edge\n"
                    "value: the number; negative values draw as 0\n"
                    "opts: {strip = sprite handle (required), pad = 0, padZeros = true, "
                    "align = Align (\"left\"), sep = separator frame or -1, spacing = 0}")
            .withEnum(kAlign),
        luaFunction("timer", lua_timer, "(x:int, y:int, ms:int, opts:table) -> width:int",
                    "Draw milliseconds as mm:ss from an 11-frame strip (digits, then ':'); returns its width.",
                    "x: anchor column, see opts.align\n"
                    "y: top edge\n"
                    "ms: the time; negative values draw as 0\n"
                    "opts: {strip = sprite handle (required), align = Align (\"left\"), "
                    "spacing = 0}")
            .withEnum(kAlign),

        // Layers
        luaFunction("setLayer", lua_setLayer, "(layer:int) -> nil",
                    "Draw on another layer from now on.",
                    "layer: 1..gfx.getLayerCount(); out of range raises"),
        luaFunction("getLayer", lua_getLayer, "() -> int", "The active layer (1-based)."),
        luaFunction("clearLayer", lua_clearLayer, "(layer:int?, color:int?=0) -> nil",
                    "Fill one layer with a colour.",
                    "layer: 1..gfx.getLayerCount(); nil means the active layer\n"
                    "color: palette index; 0 by default, unlike gfx.clear"),
        luaFunction("getLayerCount", lua_getLayerCount, "() -> int",
                    "How many layers scripts can draw on."),
        luaFunction("setLayerVisible", lua_setLayerVisible, "(layer:int, visible:boolean) -> nil",
                    "Show or hide a layer in the composited frame.",
                    "layer: 1..gfx.getLayerCount(); out of range raises\n"
                    "visible: whether the layer is drawn"),
        luaFunction("isLayerVisible", lua_isLayerVisible, "(layer:int) -> boolean",
                    "Whether a layer is shown; true when the host has no layers.",
                    "layer: 1..gfx.getLayerCount(); out of range raises"),

        // Text
        luaFunction("text", lua_text, "(str:string, x:int, y:int, scale:int?=current) -> nil",
                    "Draw text at (x, y) in the current font, size and draw colour.",
                    "str: the text\n"
                    "x: left edge; a fractional number raises\n"
                    "y: vertical position; a fractional number raises\n"
                    "scale: size for this call only, 1..255"),
        luaFunction("textWrapped", lua_textWrapped,
                    "(str:string, x:int, y:int, maxWidth:int) -> nil",
                    "Draw text wrapped to a width, at the current size.",
                    "str: the text\n"
                    "x: left edge\n"
                    "y: vertical position of the first line\n"
                    "maxWidth: wrap width in pixels"),
        luaFunction("textCentered", lua_textCentered,
                    "(str:string, y:int, scale:int?=current) -> nil",
                    "Draw text centred across the layer's width.",
                    "str: the text\n"
                    "y: vertical position\n"
                    "scale: size for this call only, 1..255"),
        luaFunction("textAligned", lua_textAligned,
                    "(str:string, x:int, y:int, align:Align?=left, scale:int?=current) -> nil",
                    "Draw text with x as its left edge, centre or right edge.",
                    "str: the text\n"
                    "x: anchor column\n"
                    "y: vertical position\n"
                    "align: which edge x anchors; an unknown name anchors left\n"
                    "scale: size for this call only, 1..255")
            .withEnum(kAlign),
        luaFunction("setTextSize", lua_setTextSize, "(size:int) -> nil",
                    "Set the text scale factor.",
                    "size: 1..255; other numbers reset it to 1")
            .note("Starts at 1 after every reload."),
        luaFunction("getTextSize", lua_getTextSize, "() -> int", "The current text scale factor."),
        luaFunction("setFont", lua_setFont, "(name:string) -> nil",
                    "Switch the text font; an unknown name keeps the current one.",
                    "name: \"default\", \"default8\", or a font the host registered")
            .note("Tomodachi hosts also register \"body\" and \"display\"."),
        luaFunction("getFont", lua_getFont, "() -> string", "The current font's name."),
        luaFunction("getTextWidth", lua_getTextWidth, "(str:string?=\"\") -> int",
                    "Width of str in pixels at the current font and size; 0 with no canvas.",
                    "str: the text"),
        luaFunction("getTextHeight", lua_getTextHeight, "() -> int",
                    "Line height in pixels at the current font and size."),

        // Layer constants (Lua 1-indexed)
        luaConstant("LAYER_BG", 1, "The back layer."),
        luaConstant("LAYER_MID", 2, "The middle layer."),
        luaConstant("LAYER_FG", 3, "The front layer."),

        luaTable("COLOR", kGfxColorModule, "Named palette indices; only transparency is named."),
    };
    static constexpr LuaApiModule kGfxModule = luaApiModule(
        LuaApiScope::Table, "gfx",
        "Drawing onto the active layer: shapes, pixels, sprites, text and the palette.", kGfx);
    luaApiSetGlobalTable(L, kGfxModule);

    // Index-shader constructors: gfx.remap / gfx.mask / gfx.effect (#36).
    // Augments the gfx table just set above; the apply site gfx.drawSprite(..,
    // fx) consumes the Effect userdata these return.
    enjin2::lua::registerEffectApi(L);

    // === print() stays as bare global ===
    static constexpr LuaApiEntry kPrint[] = {
        luaFunction("print", lua_print, "(...:any) -> nil",
                    "Write values to the log, tab-separated, ending with a newline.",
                    "...: strings and numbers print as text; other values print as their type")
            .note("Replaces Lua's print: print(true) writes \"(boolean)\"."),
    };
    static constexpr LuaApiModule kPrintModule =
        luaApiModule(LuaApiScope::Globals, "", "The print global.", kPrint);
    luaApiSetGlobals(L, kPrintModule);

    // Pre-register built-in 8pt font so setFont("default8") works
    registerFont("default8", &defaultFont8pt7b);

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
    layerCount = (count > ENJIN_LAYER_COUNT - 1) ? ENJIN_LAYER_COUNT - 1 : count;
    for (uint8_t i = 0; i < layerCount; ++i) {
        layerCanvases[i] = canvases[i];
    }
    layerVisible = visibleArr;
    activeLayer = 0;
    currentCanvas = layerCount ? layerCanvases[0] : nullptr;
}

void LuaBindings::resetSpritePool() {
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        spritePool[i] = SpriteState{};
        loadedAssets_[i] = SpriteAsset{};
        loadedClips_[i].clear();
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

const std::vector<enjin2::NjnClip>* LuaBindings::getLoadedClips(int handle) const {
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !spritePool[handle].active)
        return nullptr;
    return &loadedClips_[handle];
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
