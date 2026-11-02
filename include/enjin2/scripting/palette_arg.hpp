#pragma once

// Lua colour arguments (ADR-0019): every binding that takes a palette index
// reads it through these, so 8-14 (no Palette role) raise a Lua error that
// names the bad value instead of drawing. A byte-wide canvas (Canvas8) has no
// palette: its colours are grey levels 0..255 (checkCanvasColour). Header-only, shared by enjin's
// bindings and libtomo's (compositor, stroke, shader, ...).

#include <cstdint>

#include "../graphics/palette.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {
namespace lua {

/// Whether a Lua integer is a palette index: a role 0..7 or transparent 15.
inline bool isPaletteIndexValue(lua_Integer v) {
    return v >= 0 && v <= PALETTE_TRANSPARENT && isPaletteIndex(static_cast<unsigned>(v));
}

/// A palette index already read from Lua (a table field, a list item): raises
/// "<what> N is not a Palette role (0..7) or transparent (15)" unless valid.
inline uint8_t paletteIndexOrError(lua_State* L, lua_Integer v, const char* what = "colour") {
    if (!isPaletteIndexValue(v)) {
        luaL_error(L, "%s %d is not a Palette role (0..7) or transparent (15)", what, static_cast<int>(v));
    }
    return static_cast<uint8_t>(v);
}

/// Argument @p arg as a palette index: an integer that is a role or 15, else a
/// Lua error naming the function, the argument and the bad value.
inline uint8_t checkPaletteIndex(lua_State* L, int arg) {
    const lua_Integer v = luaL_checkinteger(L, arg);
    if (!isPaletteIndexValue(v)) {
        luaL_argerror(L, arg,
                      lua_pushfstring(L, "colour %d is not a Palette role (0..7) or transparent (15)",
                                      static_cast<int>(v)));
    }
    return static_cast<uint8_t>(v);
}

/// checkPaletteIndex, or @p def when the argument is absent or nil.
inline uint8_t optPaletteIndex(lua_State* L, int arg, uint8_t def) {
    return lua_isnoneornil(L, arg) ? def : checkPaletteIndex(L, arg);
}

/// A draw colour for a canvas that may be byte-wide: on an indexed (4-bit) canvas it
/// is a palette index (checkPaletteIndex); on a byte canvas (Canvas8, a grey level)
/// any 0..255 value, as before the palette had roles.
inline uint8_t checkCanvasColour(lua_State* L, int arg, bool indexed) {
    return indexed ? checkPaletteIndex(L, arg) : static_cast<uint8_t>(luaL_checkinteger(L, arg));
}

/// checkCanvasColour, or @p def when the argument is absent or nil.
inline uint8_t optCanvasColour(lua_State* L, int arg, uint8_t def, bool indexed) {
    return lua_isnoneornil(L, arg) ? def : checkCanvasColour(L, arg, indexed);
}

}  // namespace lua
}  // namespace enjin2
