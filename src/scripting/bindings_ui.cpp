/**
 * @file bindings_ui.cpp
 * @brief engine.ui.* Lua sub-table — immediate-mode UI component draw functions (Phase 52: UI-01..UI-04)
 *
 * Provides four stateless draw functions that bypass C++ Label/FillUpGauge components,
 * drawing directly to the active layer canvas (currentCanvas) via LuaCanvas primitives.
 * All functions early-return silently when currentCanvas is nullptr (null-canvas safety).
 */
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/bind_helpers.hpp"

namespace enjin2 {

// ── Guard macro ──────────────────────────────────────────────────────────────
// Retrieves LuaBindings*, returns 0 if null or no active canvas.
#define REQUIRE_CANVAS(b, L) \
    LuaBindings* b = LuaBindings::getBindings(L); \
    if (!(b) || !(b)->currentCanvas) return 0

// ── UI-01: engine.ui.progressBar(x, y, w, h, value, fg, bg) ──────────────────
// Draws a filled progress bar. value is clamped to [0, 1].
// bg fill covers full width; fg fill covers proportional width.
int LuaBindings::lua_engine_ui_progressBar(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x     = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y     = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w     = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h     = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    float    value = static_cast<float>(luaL_checknumber(L, 5));  // float — do NOT use checkinteger
    uint8_t  fg    = static_cast<uint8_t>(luaL_checkinteger(L, 6));
    uint8_t  bg    = static_cast<uint8_t>(luaL_checkinteger(L, 7));

    // Clamp value to [0, 1]
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    // Draw background
    b->currentCanvas->fillRect(x, y, w, h, bg);

    // Compute and clamp fill width
    uint16_t fillW = static_cast<uint16_t>(static_cast<float>(w) * value);
    if (fillW > w) fillW = w;

    // Draw filled portion (only if non-zero width)
    if (fillW > 0) {
        b->currentCanvas->fillRect(x, y, fillW, h, fg);
    }
    return 0;
}

// ── UI-02: engine.ui.statBar(x, y, w, h, current, max, fg, bg) ───────────────
// Draws a stat bar proportional to current/max.
// Division by zero is guarded: if max <= 0, fill is 0.
int LuaBindings::lua_engine_ui_statBar(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x       = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y       = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w       = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h       = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    float    current = static_cast<float>(luaL_checknumber(L, 5));  // float — do NOT use checkinteger
    float    max     = static_cast<float>(luaL_checknumber(L, 6));  // float — do NOT use checkinteger
    uint8_t  fg      = static_cast<uint8_t>(luaL_checkinteger(L, 7));
    uint8_t  bg      = static_cast<uint8_t>(luaL_checkinteger(L, 8));

    // Compute fraction — guard against division by zero
    float fraction = (max > 0.0f) ? (current / max) : 0.0f;

    // Clamp fraction to [0, 1]
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    // Draw background
    b->currentCanvas->fillRect(x, y, w, h, bg);

    // Compute and clamp fill width
    uint16_t fillW = static_cast<uint16_t>(static_cast<float>(w) * fraction);
    if (fillW > w) fillW = w;

    // Draw filled portion (only if non-zero width)
    if (fillW > 0) {
        b->currentCanvas->fillRect(x, y, fillW, h, fg);
    }
    return 0;
}

// ── UI-03: engine.ui.panel(x, y, w, h, bg, border) ───────────────────────────
// Draws a filled rectangle (bg) with a border outline on top.
int LuaBindings::lua_engine_ui_panel(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x      = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y      = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w      = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h      = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    uint8_t  bg     = static_cast<uint8_t>(luaL_checkinteger(L, 5));
    uint8_t  border = static_cast<uint8_t>(luaL_checkinteger(L, 6));

    b->currentCanvas->fillRect(x, y, w, h, bg);      // background fill
    b->currentCanvas->drawRect(x, y, w, h, border);  // border outline over fill
    return 0;
}

// ── UI-04: engine.ui.label(x, y, text, fg) ───────────────────────────────────
// Draws text at the specified position with the given foreground color.
int LuaBindings::lua_engine_ui_label(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t     x   = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t     y   = static_cast<int16_t>(luaL_checkinteger(L, 2));
    const char* str = luaL_checkstring(L, 3);
    uint8_t     fg  = static_cast<uint8_t>(luaL_checkinteger(L, 4));

    b->currentCanvas->drawText(str, x, y, fg, 1, nullptr);  // size=1, default font
    return 0;
}

// ── Sub-table registration (called from registerEngineTable) ──────────────────
void LuaBindings::registerUISubtable(lua_State* L) {
    // Assumes engine table is at top of stack
    static const LuaFuncDef kUIFuncs[] = {
        {"progressBar", lua_engine_ui_progressBar},
        {"statBar",     lua_engine_ui_statBar},
        {"panel",       lua_engine_ui_panel},
        {"label",       lua_engine_ui_label},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kUIFuncs, ENJIN_ARRAY_LEN(kUIFuncs));
    lua_setfield(L, -2, "ui");  // engine.ui = { ... }
}

} // namespace enjin2
