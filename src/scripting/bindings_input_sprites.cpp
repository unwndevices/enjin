#include "../../include/enjin2/scripting/bindings.hpp"

namespace enjin2 {

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

// drawSprite(handle, x, y [, flipH] [, flipV] [, rotate90])
// Draws the current frame of the sprite to currentCanvas.
// Transparent pixels (palette index 15) are skipped.
// Optional flags:
//   flipH   (bool, default false) — mirror horizontally
//   flipV   (bool, default false) — mirror vertically
//   rotate90 (bool, default false) — rotate 90° clockwise
int LuaBindings::lua_drawSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;

    int handle = static_cast<int>(luaL_checkinteger(L, 1));
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) return 0;

    int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 3));

    // Optional flip/rotation flags (default false)
    bool flipH    = lua_toboolean(L, 4) != 0;
    bool flipV    = lua_toboolean(L, 5) != 0;
    bool rotate90 = lua_toboolean(L, 6) != 0;

    const auto& s = b->spritePool[handle];
    if (!s.sheet.data || s.frame >= s.sheet.frameCount()) return 0;

    const int16_t cellW = static_cast<int16_t>(s.sheet.cellW);
    const int16_t cellH = static_cast<int16_t>(s.sheet.cellH);

    // Blit frame via LuaCanvas::setPixel (type-erased path — works for 4-bit canvas)
    const uint8_t* frame_data = s.sheet.data
        + static_cast<uint16_t>(s.frame) * s.sheet.cellW * s.sheet.cellH;

    for (int16_t fy = 0; fy < cellH; ++fy) {
        for (int16_t fx = 0; fx < cellW; ++fx) {
            // Source coordinate remapping for flip
            int16_t srcX = flipH ? (cellW - 1 - fx) : fx;
            int16_t srcY = flipV ? (cellH - 1 - fy) : fy;

            uint8_t px = frame_data[srcY * cellW + srcX] & 0x0F;
            if (px != 15) {  // index 15 = transparent
                // Destination coordinate remapping for 90° rotation
                int16_t dstX = rotate90 ? (cellH - 1 - fy) : fx;
                int16_t dstY = rotate90 ? fx : fy;
                b->currentCanvas->setPixel(x + dstX, y + dstY, px);
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

} // namespace enjin2
