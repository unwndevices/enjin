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
#include <cstring>

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

// ── UI-03: engine.ui.panel(x, y, w, h, bg|slot [, border]) ───────────────────
// Draws a filled rectangle with a border outline on top. The 5th arg dispatches
// by lua_type (#19): a string is a style slot (fill/border resolved from the
// theme), a number is the legacy explicit (bg, border) pair. Existing applets
// keep passing numbers; the launcher passes slot names.
// Check a string arg naming a style slot; raises a Lua error (never returns) if
// it is not one of the closed slot names. Shared by panel and setStyle so the
// name→slot decode + error message live in one place.
static StyleSlot checkStyleSlotArg(lua_State* L, int idx) {
    const char* name = luaL_checkstring(L, idx);
    StyleSlot slot = StyleSlot::Panel;
    if (!styleSlotFromName(name, slot)) {
        luaL_error(L, "engine.ui: unknown style slot '%s'", name);  // no return
    }
    return slot;
}

// Overlay one style field from a Lua table key onto `out`, only if the key is
// present and numeric. One template covers Pixel4 / uint8_t / int8_t fields.
template <typename T>
static void overlayKey(lua_State* L, int tableIdx, const char* key, T& out) {
    lua_getfield(L, tableIdx, key);
    if (lua_isnumber(L, -1)) out = static_cast<T>(lua_tointeger(L, -1));
    lua_pop(L, 1);
}

int LuaBindings::lua_engine_ui_panel(lua_State* L) {
    REQUIRE_CANVAS(b, L);
    int16_t  x = static_cast<int16_t>(luaL_checkinteger(L, 1));
    int16_t  y = static_cast<int16_t>(luaL_checkinteger(L, 2));
    uint16_t w = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t h = static_cast<uint16_t>(luaL_checkinteger(L, 4));

    uint8_t bg;
    BorderStyle bstyle;
    if (lua_type(L, 5) == LUA_TSTRING) {
        // Slot: resolve a full BorderStyle (colour, thickness, radius, kind,
        // shadow) from the style slot (#18/#39). Derived tones stay derived.
        const Style& st = b->resolveStyle(checkStyleSlotArg(L, 5));
        bg               = st.fill.value;
        bstyle.color     = st.border;
        bstyle.thickness = st.borderWidth ? st.borderWidth : uint8_t{1};
        bstyle.radius    = st.radius;
        bstyle.kind      = (st.borderKind <= static_cast<uint8_t>(BorderKind::DropShadow))
                               ? static_cast<BorderKind>(st.borderKind)
                               : BorderKind::Solid;
        bstyle.shadowDx  = st.shadowDx;
        bstyle.shadowDy  = st.shadowDy;
    } else {
        // Legacy explicit (bg, border): a plain 1px solid outline.
        bg               = static_cast<uint8_t>(luaL_checkinteger(L, 5));
        bstyle.color     = Pixel4(static_cast<uint8_t>(luaL_checkinteger(L, 6)));
        bstyle.thickness = 1;
        bstyle.radius    = 0;
        bstyle.kind      = BorderKind::Solid;
    }

    b->currentCanvas->fillRect(x, y, w, h, bg);          // background fill
    b->currentCanvas->strokeBorder(x, y, w, h, bstyle);  // computed span-walker border
    return 0;
}

// ── UI-05: engine.ui.setStyle(slotName, { fill=…, border=…, radius=…, … }) ────
// Overrides one style slot. Resolution starts from the theme default (so absent
// keys inherit it), overlays only the keys present in the table, stores the
// result and marks the slot's mask bit. Colours are ramp role indices (#12);
// derived tones (light/dark/shadow) are never stored (#19).
int LuaBindings::lua_engine_ui_setStyle(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    StyleSlot slot = checkStyleSlotArg(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const int i = static_cast<int>(slot);

    // Start from the theme default (absent keys inherit it), overlay present keys.
    Style st = b->m_themeBase[i];
    overlayKey(L, 2, "fill",   st.fill);
    overlayKey(L, 2, "text",   st.text);
    overlayKey(L, 2, "border", st.border);
    overlayKey(L, 2, "borderWidth", st.borderWidth);
    overlayKey(L, 2, "radius",      st.radius);
    overlayKey(L, 2, "padding",     st.padding);
    overlayKey(L, 2, "borderKind",  st.borderKind);
    overlayKey(L, 2, "shadowDx",    st.shadowDx);
    overlayKey(L, 2, "shadowDy",    st.shadowDy);

    b->m_styleValues[i] = st;
    b->m_styleSetMask |= static_cast<uint16_t>(1u << i);
    return 0;
}

// ── UI-06: engine.ui.setTheme(name) ──────────────────────────────────────────
// Swaps the active ROM theme base and clears every applet override (a new theme
// is a fresh base). v1 ships one theme, "default".
int LuaBindings::lua_engine_ui_setTheme(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    const char* name = luaL_checkstring(L, 1);
    if (strcmp(name, "default") == 0) {
        b->m_themeBase = kDefaultStyles;
    } else {
        return luaL_error(L, "engine.ui.setTheme: unknown theme '%s'", name);
    }
    b->m_styleSetMask = 0;  // a new theme is a fresh base — drop overrides
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
        {"setStyle",    lua_engine_ui_setStyle},
        {"setTheme",    lua_engine_ui_setTheme},
    };
    lua_newtable(L);
    luaBindFunctions(L, -1, kUIFuncs, ENJIN_ARRAY_LEN(kUIFuncs));
    lua_setfield(L, -2, "ui");  // engine.ui = { ... }
}

} // namespace enjin2
