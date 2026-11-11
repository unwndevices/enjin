#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/core/object.hpp"
#include <cstdio>
#include <utility>

namespace enjin2 {

C_Timer::C_Timer(Object* owner)
    : Component(owner) {
    // All TimerEntry members are value-initialized by default
}

C_Timer::~C_Timer() {
    clearTimers();
}

int C_Timer::scheduleInternal(float seconds, lua::ScriptRef callback, bool repeating) {
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (!m_timers[i].active) {
            m_timers[i].callback    = std::move(callback);
            m_timers[i].interval    = seconds;
            m_timers[i].elapsed     = 0.0f;
            m_timers[i].repeating   = repeating;
            m_timers[i].active      = true;
            m_timers[i].id          = m_nextId++;
            return m_timers[i].id;
        }
    }
    // No free slot: the callback is released as it goes out of scope.
    return 0;
}

int C_Timer::scheduleAfter(float seconds, lua::ScriptRef callback) {
    return scheduleInternal(seconds, std::move(callback), false);
}

int C_Timer::scheduleEvery(float seconds, lua::ScriptRef callback) {
    return scheduleInternal(seconds, std::move(callback), true);
}

void C_Timer::cancel(int timerId) {
    if (timerId == 0) return;  // 0 = invalid ID, no-op
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (m_timers[i].active && m_timers[i].id == timerId) {
            m_timers[i].callback.reset();
            m_timers[i].active = false;
            return;
        }
    }
}

void C_Timer::clearTimers() {
    for (int i = 0; i < MAX_TIMERS; ++i) {
        m_timers[i].callback.reset();
        m_timers[i].active = false;
        m_timers[i].elapsed = 0.0f;
    }
    m_L = nullptr;  // re-armed by the next timer:after/every
}

int C_Timer::getActiveCount() const {
    int count = 0;
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (m_timers[i].active) ++count;
    }
    return count;
}

void C_Timer::fireCallback(const lua::ScriptRef& callback) {
    if (!m_L) return;

    // The callback is passed in, not read from the entry: a one-shot timer's
    // entry has already given its callback up (update() moves it out first).
    callback.push(m_L);
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
        if (!e.repeating) {
            // Deactivate BEFORE calling (prevents re-entrant double-fire); the
            // callback leaves the entry and is released after it runs.
            e.active = false;
            lua::ScriptRef callback = std::move(e.callback);
            fireCallback(callback);
        } else {
            // Reset elapsed for next interval — keep active
            e.elapsed = 0.0f;
            fireCallback(e.callback);
        }
    }
}

} // namespace enjin2
