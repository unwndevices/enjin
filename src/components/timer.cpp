#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/core/object.hpp"
#include <cstdio>

namespace enjin2 {

C_Timer::C_Timer(Object* owner)
    : Component(owner) {
    // All TimerEntry members are value-initialized by default
}

C_Timer::~C_Timer() {
    clearTimers();
}

int C_Timer::scheduleInternal(float seconds, int callbackRef, bool repeating) {
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (!m_timers[i].active) {
            m_timers[i].callbackRef = callbackRef;
            m_timers[i].interval    = seconds;
            m_timers[i].elapsed     = 0.0f;
            m_timers[i].repeating   = repeating;
            m_timers[i].active      = true;
            m_timers[i].id          = m_nextId++;
            return m_timers[i].id;
        }
    }
    // No free slot — release the ref to avoid leak, return 0 (invalid ID)
    if (m_L) {
        luaL_unref(m_L, LUA_REGISTRYINDEX, callbackRef);
    }
    return 0;
}

int C_Timer::scheduleAfter(float seconds, int callbackRef) {
    return scheduleInternal(seconds, callbackRef, false);
}

int C_Timer::scheduleEvery(float seconds, int callbackRef) {
    return scheduleInternal(seconds, callbackRef, true);
}

void C_Timer::cancel(int timerId) {
    if (timerId == 0) return;  // 0 = invalid ID, no-op
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (m_timers[i].active && m_timers[i].id == timerId) {
            if (m_L && m_timers[i].callbackRef != LUA_NOREF) {
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_timers[i].callbackRef);
            }
            m_timers[i].callbackRef = LUA_NOREF;
            m_timers[i].active = false;
            return;
        }
    }
}

void C_Timer::clearTimers() {
    if (m_L) {
        for (int i = 0; i < MAX_TIMERS; ++i) {
            if (m_timers[i].active && m_timers[i].callbackRef != LUA_NOREF) {
                luaL_unref(m_L, LUA_REGISTRYINDEX, m_timers[i].callbackRef);
            }
            m_timers[i].callbackRef = LUA_NOREF;
            m_timers[i].active = false;
            m_timers[i].elapsed = 0.0f;
        }
    }
    m_L = nullptr;  // Sentinel: prevents double-unref in destructor
}

int C_Timer::getActiveCount() const {
    int count = 0;
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (m_timers[i].active) ++count;
    }
    return count;
}

void C_Timer::fireCallback(int cbRef) {
    if (!m_L) return;

    // Push the Lua function from registry using the provided ref
    // (NOT entry.callbackRef — for one-shot timers, entry.callbackRef is already LUA_NOREF
    //  at this point because update() clears it before calling fireCallback)
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, cbRef);
    if (!lua_isfunction(m_L, -1)) {
        lua_pop(m_L, 1);
        return;
    }

    // Push ScriptProxy as self (TIMER-04)
    // Uses the same registry key pattern as callWithProxy: lightuserdata(C_LuaScript*)
    C_LuaScript* script = owner->getComponent<C_LuaScript>();
    if (script) {
        lua_pushlightuserdata(m_L, script);
        lua_gettable(m_L, LUA_REGISTRYINDEX);  // pushes ScriptProxy userdata
    } else {
        lua_pushnil(m_L);  // Fallback if no C_LuaScript (should not happen in normal use)
    }

    // Call: fn(self) — 1 arg, 0 results
    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        printf("[C_Timer] callback error: %s\n", err ? err : "unknown");
        lua_pop(m_L, 1);
    }
}

void C_Timer::update(float dt) {
    if (!m_L) return;

    // Iterate all slots. Snapshot: new timers added during callbacks will be in
    // inactive slots beyond existing entries or will reuse freed slots — safe because
    // we iterate by index up to MAX_TIMERS and check active flag per slot.
    for (int i = 0; i < MAX_TIMERS; ++i) {
        TimerEntry& e = m_timers[i];
        if (!e.active) continue;

        e.elapsed += dt;
        if (e.elapsed < e.interval) continue;

        // Timer fires
        int cbRef = e.callbackRef;
        bool repeating = e.repeating;

        if (!repeating) {
            // Deactivate BEFORE calling — prevents re-entrant double-fire
            e.active = false;
            e.callbackRef = LUA_NOREF;
        } else {
            // Reset elapsed for next interval — keep active
            e.elapsed = 0.0f;
        }

        // Fire the callback (pass cbRef directly — for one-shot timers, e.callbackRef
        // is already LUA_NOREF at this point, so we must use the saved local copy)
        fireCallback(cbRef);

        // Release ref for one-shot timers AFTER fire (ref was copied before deactivation)
        if (!repeating && m_L) {
            luaL_unref(m_L, LUA_REGISTRYINDEX, cbRef);
        }
    }
}

} // namespace enjin2
