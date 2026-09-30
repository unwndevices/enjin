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
//   t = engine.hud.timer(ms [, "down"|"up"])   (kTimerModeNames)
//     t:update(dtSeconds)  t:pause()  t:resume()  t:reset()
//     t:done() -> bool  t:millis() -> int  t:seconds() -> int
//     t:format() -> "mm:ss" string  t:paused() -> bool

#include <cstring>
#include <new>

#include "../core/name_index.hpp"
#include "../graphics/numerals.hpp"
#include "lua_api.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {
namespace lua {

/// Metatable names for the value-object userdata.
inline const char* rollingCounterMt() { return "enjin2.RollingCounter"; }
inline const char* timerMt() { return "enjin2.Timer"; }

/// engine.hud.timer's mode names, indexed by TimerMode and published as the
/// TimerMode enum of its descriptor.
inline constexpr const char* kTimerModeNames[] = {"down", "up"};
static_assert(static_cast<int>(TimerMode::CountDown) == 0 &&
                  static_cast<int>(TimerMode::CountUp) == 1,
              "kTimerModeNames is indexed by TimerMode");

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
        const int m = nameIndex(luaL_checkstring(L, 2), kTimerModeNames);
        if (m < 0) return luaL_error(L, "engine.hud.timer: mode must be 'down' or 'up'");
        mode = static_cast<TimerMode>(m);
    }
    void* p = lua_newuserdata(L, sizeof(Timer));
    new (p) Timer(ms, mode);
    luaL_setmetatable(L, timerMt());
    return 1;
}

} // namespace detail

/// Register the RollingCounter / Timer userdata metatables from their
/// descriptors: each metatable's __index is its methods table. Idempotent
/// (safe on a Lua-state reload) — RollingCounter and Timer are trivially
/// destructible, so no __gc is attached.
inline void ensureHudMetatables(lua_State* L) {
    static constexpr LuaApiEntry kRcMethods[] = {
        luaFunction("set", detail::rc_set, "(target:number) -> nil",
                    "Roll toward a new target, keeping the current speed.",
                    "target: the value to roll to"),
        luaFunction("snap", detail::rc_snap, "(value:number) -> nil",
                    "Jump to a value with no roll.",
                    "value: the new value and target"),
        luaFunction("update", detail::rc_update, "(dt:number) -> nil",
                    "Advance the roll by dt seconds; within 1 of the target it snaps onto it.",
                    "dt: seconds"),
        luaFunction("value", detail::rc_value, "() -> int",
                    "The number to display: floored, and never below 0."),
        luaFunction("valuef", detail::rc_valuef, "() -> number",
                    "The unrounded position of the roll."),
        luaFunction("target", detail::rc_target, "() -> number", "The value it is rolling to."),
    };
    static constexpr LuaApiModule kRcMethodsModule = luaApiModule(
        LuaApiScope::Methods, "RollingCounter", "RollingCounter methods, called as rc:name(...).",
        kRcMethods);
    static constexpr LuaApiEntry kRcMeta[] = {
        luaTable("__index", kRcMethodsModule, "The methods."),
    };
    static constexpr LuaApiModule kRcMetaModule = luaApiModule(
        LuaApiScope::Metatable, "RollingCounter", "RollingCounter method lookup.", kRcMeta);

    static constexpr LuaApiEntry kTimerMethods[] = {
        luaFunction("update", detail::tmr_update, "(dt:number) -> nil",
                    "Advance by dt seconds unless paused, stopping at 0 (down) or the start "
                    "value (up).",
                    "dt: seconds"),
        luaFunction("pause", detail::tmr_pause, "() -> nil", "Stop the clock."),
        luaFunction("resume", detail::tmr_resume, "() -> nil", "Start the clock again."),
        luaFunction("reset", detail::tmr_reset, "() -> nil",
                    "Back to the start (the full time down, 0 up), and running."),
        luaFunction("done", detail::tmr_done, "() -> boolean",
                    "Whether it reached 0 (down) or the start value (up)."),
        luaFunction("millis", detail::tmr_millis, "() -> int", "Whole milliseconds on the clock."),
        luaFunction("seconds", detail::tmr_seconds, "() -> int", "Whole seconds on the clock."),
        luaFunction("paused", detail::tmr_paused, "() -> boolean", "Whether the clock is paused."),
        luaFunction("format", detail::tmr_format, "() -> string",
                    "The clock as \"mm:ss\"; past 99 minutes the minutes grow a digit."),
    };
    static constexpr LuaApiModule kTimerMethodsModule = luaApiModule(
        LuaApiScope::Methods, "Timer", "Timer methods, called as t:name(...).", kTimerMethods);
    static constexpr LuaApiEntry kTimerMeta[] = {
        luaTable("__index", kTimerMethodsModule, "The methods."),
    };
    static constexpr LuaApiModule kTimerMetaModule = luaApiModule(
        LuaApiScope::Metatable, "Timer", "Timer method lookup.", kTimerMeta);

    luaL_newmetatable(L, rollingCounterMt());
    luaApiSetFields(L, -1, kRcMetaModule);
    luaL_newmetatable(L, timerMt());
    luaApiSetFields(L, -1, kTimerMetaModule);
    lua_pop(L, 2);
}

} // namespace lua
} // namespace enjin2
