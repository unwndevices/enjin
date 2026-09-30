/**
 * @file bindings_debug.cpp
 * @brief engine.debug.* Lua sub-table — debug draw overlay (Phase 47: DEBUG-01..DEBUG-03)
 *
 * Routes debug draw calls to a dedicated top-layer canvas (m_debugCanvas).
 * All functions early-return with zero cost when m_debugEnabled is false or
 * m_debugCanvas is nullptr.
 */
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/lua_api.hpp"

namespace enjin2 {

// ── Guard macro ──────────────────────────────────────────────────────────────
// Retrieves LuaBindings*, returns 0 if null, disabled, or no debug canvas.
#define REQUIRE_DEBUG_CANVAS(b) \
    LuaBindings* b = LuaBindings::getBindings(L); \
    if (!b || !b->m_debugEnabled || !b->m_debugCanvas) return 0

// ── DEBUG-01: engine.debug.rect(x, y, w, h [, color]) ────────────────────────
int LuaBindings::lua_engine_debug_rect(lua_State* L) {
    REQUIRE_DEBUG_CANVAS(b);
    int16_t  x   = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y   = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w   = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h   = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    uint8_t  col = static_cast<uint8_t>(luaL_optinteger(L, 5, 8));  // default: palette index 8
    b->m_debugCanvas->drawRect(x, y, w, h, col);
    return 0;
}

// ── DEBUG-01: engine.debug.circle(x, y, r [, color]) ─────────────────────────
int LuaBindings::lua_engine_debug_circle(lua_State* L) {
    REQUIRE_DEBUG_CANVAS(b);
    int16_t  x   = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y   = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t r   = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint8_t  col = static_cast<uint8_t>(luaL_optinteger(L, 4, 8));
    b->m_debugCanvas->drawCircle(x, y, r, col);
    return 0;
}

// ── DEBUG-01: engine.debug.line(x1, y1, x2, y2 [, color]) ───────────────────
int LuaBindings::lua_engine_debug_line(lua_State* L) {
    REQUIRE_DEBUG_CANVAS(b);
    int16_t x1  = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t y1  = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t x2  = static_cast<int16_t>(luaL_checkinteger(L, 3));
    int16_t y2  = static_cast<int16_t>(luaL_checkinteger(L, 4));
    uint8_t col = static_cast<uint8_t>(luaL_optinteger(L, 5, 8));
    b->m_debugCanvas->drawLine(x1, y1, x2, y2, col);
    return 0;
}

// ── DEBUG-01: engine.debug.cross(x, y [, size [, color]]) ────────────────────
// Draws a "+" crosshair as two lines. No drawCross primitive exists in LuaCanvas.
int LuaBindings::lua_engine_debug_cross(lua_State* L) {
    REQUIRE_DEBUG_CANVAS(b);
    int16_t  x   = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y   = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t sz  = static_cast<uint16_t>(luaL_optinteger(L, 3, 4));
    uint8_t  col = static_cast<uint8_t>(luaL_optinteger(L, 4, 8));
    b->m_debugCanvas->drawLine(static_cast<int16_t>(x - sz), y,
                               static_cast<int16_t>(x + sz), y, col);  // horizontal
    b->m_debugCanvas->drawLine(x, static_cast<int16_t>(y - sz),
                               x, static_cast<int16_t>(y + sz), col);  // vertical
    return 0;
}

// ── DEBUG-02: engine.debug.text(str, x, y [, color]) ─────────────────────────
// Uses default font (nullptr) and size 1 for predictable debug output.
int LuaBindings::lua_engine_debug_text(lua_State* L) {
    REQUIRE_DEBUG_CANVAS(b);
    const char* str = luaL_checkstring(L, 1);
    int16_t x   = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y   = static_cast<int16_t>(luaL_checkinteger(L, 3));
    uint8_t col = static_cast<uint8_t>(luaL_optinteger(L, 4, 8));
    b->m_debugCanvas->drawText(str, x, y, col, 1, nullptr);  // size=1, default font
    return 0;
}

// ── DEBUG-03: engine.debug.setEnabled(bool) ──────────────────────────────────
int LuaBindings::lua_engine_debug_setEnabled(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;
    b->m_debugEnabled = (lua_toboolean(L, 1) != 0);
    return 0;
}

// ── DEBUG-03: engine.debug.getEnabled() -> bool ──────────────────────────────
int LuaBindings::lua_engine_debug_getEnabled(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, b->m_debugEnabled ? 1 : 0);
    return 1;
}

// ── Sub-table registration (called from registerEngineTable, when switched on) ──
void LuaBindings::registerDebugSubtable(lua_State* L) {
    // Assumes engine table is at top of stack
    static constexpr LuaApiEntry kDebug[] = {
        luaFunction("rect", lua_engine_debug_rect,
                    "(x:int, y:int, w:int, h:int, color:int?=8) -> nil",
                    "Outline a rectangle on the debug overlay.",
                    "x: left edge\n"
                    "y: top edge\n"
                    "w: width\n"
                    "h: height\n"
                    "color: palette index"),
        luaFunction("circle", lua_engine_debug_circle, "(x:int, y:int, r:int, color:int?=8) -> nil",
                    "Outline a circle on the debug overlay.",
                    "x: centre column\n"
                    "y: centre row\n"
                    "r: radius\n"
                    "color: palette index"),
        luaFunction("line", lua_engine_debug_line,
                    "(x1:int, y1:int, x2:int, y2:int, color:int?=8) -> nil",
                    "Draw a line on the debug overlay.",
                    "x1: start column\n"
                    "y1: start row\n"
                    "x2: end column\n"
                    "y2: end row\n"
                    "color: palette index"),
        luaFunction("cross", lua_engine_debug_cross,
                    "(x:int, y:int, size:int?=4, color:int?=8) -> nil",
                    "Draw a + marker on the debug overlay.",
                    "x: centre column\n"
                    "y: centre row\n"
                    "size: arm length in pixels\n"
                    "color: palette index"),
        luaFunction("text", lua_engine_debug_text,
                    "(str:string, x:int, y:int, color:int?=8) -> nil",
                    "Draw text on the debug overlay, in the default font at size 1.",
                    "str: the text\n"
                    "x: left edge\n"
                    "y: vertical position\n"
                    "color: palette index"),
        luaFunction("setEnabled", lua_engine_debug_setEnabled, "(on:boolean) -> nil",
                    "Turn the overlay draws on or off.",
                    "on: whether debug draws run")
            .note("On again after every reload."),
        luaFunction("getEnabled", lua_engine_debug_getEnabled, "() -> boolean",
                    "Whether the overlay draws run."),
    };
    static constexpr LuaApiModule kDebugModule = luaApiModule(
        LuaApiScope::Table, "engine.debug",
        "Overlay drawing on the host's debug layer; a no-op without one or while disabled.",
        kDebug);
    luaApiSetSubtable(L, -1, kDebugModule);   // engine.debug = { ... }
}

} // namespace enjin2
