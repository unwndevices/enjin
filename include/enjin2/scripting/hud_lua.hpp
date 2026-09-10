#pragma once

// Lua surface for the HUD numeral value objects: engine.hud.rollingCounter and
// engine.hud.timer, returning typed userdata wrapping enjin2::RollingCounter /
// enjin2::Timer. The draw side (gfx.number / gfx.timer) lives with the other
// immediate-draw bindings because it needs the active canvas; these two are pure
// value objects with no engine state, so — like gfx.remap/mask/effect — they are
// header-only and engine-owned, one representation shared by every host.
//
// Methods (mirroring the C++ API 1:1, the ADR-0003 §8 parity rule):
//   rc = engine.hud.rollingCounter(initial [, durationS])
//     rc:set(target)  rc:snap(value)  rc:update(dtSeconds)
//     rc:value() -> int (floored)   rc:valuef() -> number   rc:target() -> number
//   t = engine.hud.timer(ms [, "down"|"up"])
//     t:update(dtSeconds)  t:pause()  t:resume()  t:reset()
//     t:done() -> bool  t:millis() -> int  t:seconds() -> int
//     t:format() -> "mm:ss" string  t:paused() -> bool

#include <cstring>
#include <new>

#include "../graphics/numerals.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {
namespace lua {

/// Metatable names for the value-object userdata.
inline const char* rollingCounterMt() { return "enjin2.RollingCounter"; }
inline const char* timerMt() { return "enjin2.Timer"; }

namespace detail {

// --- RollingCounter methods ------------------------------------------------

inline RollingCounter* checkRolling(lua_State* L) {
    return static_cast<RollingCounter*>(luaL_checkudata(L, 1, rollingCounterMt()));
}

inline int rc_set(lua_State* L) {
    checkRolling(L)->set(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
inline int rc_snap(lua_State* L) {
    checkRolling(L)->snap(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
inline int rc_update(lua_State* L) {
    checkRolling(L)->update(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
inline int rc_value(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(checkRolling(L)->value()));
    return 1;
}
inline int rc_valuef(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(checkRolling(L)->valuef()));
    return 1;
}
inline int rc_target(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(checkRolling(L)->target()));
    return 1;
}

/// engine.hud.rollingCounter(initial=0 [, durationS])
inline int lua_newRollingCounter(lua_State* L) {
    // Read all args before lua_newuserdata — the new userdata would otherwise
    // land at stack index 2 and be mistaken for the optional durationS.
    const float initial = lua_isnoneornil(L, 1)
                              ? 0.0f
                              : static_cast<float>(luaL_checknumber(L, 1));
    const bool hasDuration = !lua_isnoneornil(L, 2);
    const float durationS = hasDuration ? static_cast<float>(luaL_checknumber(L, 2)) : 0.0f;

    void* p = lua_newuserdata(L, sizeof(RollingCounter));
    if (hasDuration) {
        new (p) RollingCounter(initial, durationS);
    } else {
        new (p) RollingCounter(initial);
    }
    luaL_setmetatable(L, rollingCounterMt());
    return 1;
}

// --- Timer methods ---------------------------------------------------------

inline Timer* checkTimer(lua_State* L) {
    return static_cast<Timer*>(luaL_checkudata(L, 1, timerMt()));
}

inline int tmr_update(lua_State* L) {
    checkTimer(L)->update(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
inline int tmr_pause(lua_State* L)  { checkTimer(L)->pause();  return 0; }
inline int tmr_resume(lua_State* L) { checkTimer(L)->resume(); return 0; }
inline int tmr_reset(lua_State* L)  { checkTimer(L)->reset();  return 0; }
inline int tmr_done(lua_State* L) {
    lua_pushboolean(L, checkTimer(L)->done() ? 1 : 0);
    return 1;
}
inline int tmr_millis(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(checkTimer(L)->millis()));
    return 1;
}
inline int tmr_seconds(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(checkTimer(L)->seconds()));
    return 1;
}
inline int tmr_paused(lua_State* L) {
    lua_pushboolean(L, checkTimer(L)->paused ? 1 : 0);
    return 1;
}
inline int tmr_format(lua_State* L) {
    char buf[8];
    checkTimer(L)->format(buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

/// engine.hud.timer(ms [, "down"|"up"])
inline int lua_newTimer(lua_State* L) {
    const double ms = static_cast<double>(luaL_checknumber(L, 1));
    TimerMode mode = TimerMode::CountDown;
    if (!lua_isnoneornil(L, 2)) {
        const char* m = luaL_checkstring(L, 2);
        if (std::strcmp(m, "up") == 0) {
            mode = TimerMode::CountUp;
        } else if (std::strcmp(m, "down") != 0) {
            return luaL_error(L, "engine.hud.timer: mode must be 'down' or 'up'");
        }
    }
    void* p = lua_newuserdata(L, sizeof(Timer));
    new (p) Timer(ms, mode);
    luaL_setmetatable(L, timerMt());
    return 1;
}

// Build a metatable whose __index is itself, populated from a method table.
inline void buildMethodMt(lua_State* L, const char* name,
                          const luaL_Reg* methods) {
    luaL_newmetatable(L, name);                 // [mt]
    lua_pushvalue(L, -1);                       // [mt, mt]
    lua_setfield(L, -2, "__index");             // mt.__index = mt  -> [mt]
    luaL_setfuncs(L, methods, 0);               // install methods into mt
    lua_pop(L, 1);                              // []
}

} // namespace detail

/// Register the RollingCounter / Timer userdata metatables. Idempotent (safe on
/// a Lua-state reload) — RollingCounter and Timer are trivially destructible, so
/// no __gc is attached.
inline void ensureHudMetatables(lua_State* L) {
    static const luaL_Reg kRcMethods[] = {
        {"set",    detail::rc_set},
        {"snap",   detail::rc_snap},
        {"update", detail::rc_update},
        {"value",  detail::rc_value},
        {"valuef", detail::rc_valuef},
        {"target", detail::rc_target},
        {nullptr, nullptr},
    };
    static const luaL_Reg kTimerMethods[] = {
        {"update",  detail::tmr_update},
        {"pause",   detail::tmr_pause},
        {"resume",  detail::tmr_resume},
        {"reset",   detail::tmr_reset},
        {"done",    detail::tmr_done},
        {"millis",  detail::tmr_millis},
        {"seconds", detail::tmr_seconds},
        {"paused",  detail::tmr_paused},
        {"format",  detail::tmr_format},
        {nullptr, nullptr},
    };
    detail::buildMethodMt(L, rollingCounterMt(), kRcMethods);
    detail::buildMethodMt(L, timerMt(), kTimerMethods);
}

} // namespace lua
} // namespace enjin2
