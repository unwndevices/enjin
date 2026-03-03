#include "../../include/enjin2/scripting/lua_event_bus.hpp"
#include <cstdio>

namespace enjin2 {

LuaEventBus::Channel* LuaEventBus::findChannel(const char* name) {
    for (int i = 0; i < MAX_CHANNELS; ++i) {
        if (m_channels[i].active && strcmp(m_channels[i].name, name) == 0) {
            return &m_channels[i];
        }
    }
    return nullptr;
}

LuaEventBus::Channel* LuaEventBus::findOrCreateChannel(const char* name) {
    // First, try to find an existing channel with this name
    Channel* existing = findChannel(name);
    if (existing) return existing;

    // Create a new channel in the first inactive slot
    for (int i = 0; i < MAX_CHANNELS; ++i) {
        if (!m_channels[i].active) {
            strncpy(m_channels[i].name, name, MAX_NAME_LEN - 1);
            m_channels[i].name[MAX_NAME_LEN - 1] = '\0';
            m_channels[i].active = true;
            return &m_channels[i];
        }
    }
    return nullptr;  // No free channel slot
}

int LuaEventBus::subscribe(const char* name, int callbackRef) {
    if (!m_L || !name) { return 0; }
    Channel* ch = findOrCreateChannel(name);
    if (!ch) {
        printf("[EventBus] channel capacity exceeded for '%s'\n", name);
        // Release the ref to avoid leak since we can't store it
        luaL_unref(m_L, LUA_REGISTRYINDEX, callbackRef);
        return 0;
    }
    for (int i = 0; i < MAX_SUBS_PER_CH; ++i) {
        if (!ch->subs[i].active) {
            ch->subs[i].callbackRef = callbackRef;
            ch->subs[i].id         = m_nextId++;
            ch->subs[i].active     = true;
            return ch->subs[i].id;
        }
    }
    printf("[EventBus] subscriber capacity exceeded for '%s'\n", name);
    // Release the ref to avoid leak since we can't store it
    luaL_unref(m_L, LUA_REGISTRYINDEX, callbackRef);
    return 0;
}

void LuaEventBus::emit(const char* name) {
    if (!m_L) {
        // m_L is nullptr in the window between clearHandlers() (called on scene
        // deactivation or hot-reload) and setLuaState() (called from
        // executeScript() / loadScriptFile() once the new Lua state is ready).
        // This window is safe: no Lua callbacks can be registered during scene
        // setup, so no subscriber list exists to invoke.
        // C++ code that calls emit() during this window silently drops the event
        // — this is intentional. If a C++ lifecycle hook must emit events during
        // scene setup, it must call setLuaState() first.
        // INVARIANT: clearHandlers() must always be called BEFORE setLuaState()
        // on hot-reload; reversing the order would cause luaL_unref on wrong state.
        return;
    }
    Channel* ch = findChannel(name);
    if (!ch) return;

    // Snapshot active callback refs before any pcall (prevents re-entrant mutation).
    // If a handler calls engine.event.off() or engine.event.on(), it modifies the
    // channel's subscriber array but NOT this local snapshot. New on() calls from
    // inside a handler are not included -- they fire starting from the next emit().
    int refs[MAX_SUBS_PER_CH];
    int count = 0;
    for (int i = 0; i < MAX_SUBS_PER_CH; ++i) {
        if (ch->subs[i].active && ch->subs[i].callbackRef != LUA_NOREF) {
            refs[count++] = ch->subs[i].callbackRef;
        }
    }

    for (int i = 0; i < count; ++i) {
        lua_rawgeti(m_L, LUA_REGISTRYINDEX, refs[i]);
        if (lua_isfunction(m_L, -1)) {
            // No arguments -- callbacks receive zero args (no payload, per REQUIREMENTS.md out-of-scope)
            if (lua_pcall(m_L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(m_L, -1);
                printf("[EventBus] handler error for '%s': %s\n", name,
                       err ? err : "unknown");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);  // pop non-function
        }
    }
}

void LuaEventBus::unsubscribe(int id) {
    if (id == 0) return;  // 0 = invalid ID, no-op
    for (int c = 0; c < MAX_CHANNELS; ++c) {
        if (!m_channels[c].active) continue;
        for (int s = 0; s < MAX_SUBS_PER_CH; ++s) {
            if (m_channels[c].subs[s].active && m_channels[c].subs[s].id == id) {
                if (m_L && m_channels[c].subs[s].callbackRef != LUA_NOREF) {
                    luaL_unref(m_L, LUA_REGISTRYINDEX,
                               m_channels[c].subs[s].callbackRef);
                }
                m_channels[c].subs[s].callbackRef = LUA_NOREF;
                m_channels[c].subs[s].active = false;
                m_channels[c].subs[s].id     = 0;
                return;
            }
        }
    }
}

void LuaEventBus::clearHandlers() {
    if (m_L) {
        for (int c = 0; c < MAX_CHANNELS; ++c) {
            if (!m_channels[c].active) continue;
            for (int s = 0; s < MAX_SUBS_PER_CH; ++s) {
                if (m_channels[c].subs[s].active &&
                    m_channels[c].subs[s].callbackRef != LUA_NOREF) {
                    luaL_unref(m_L, LUA_REGISTRYINDEX,
                               m_channels[c].subs[s].callbackRef);
                    m_channels[c].subs[s].callbackRef = LUA_NOREF;
                }
                m_channels[c].subs[s].active = false;
                m_channels[c].subs[s].id     = 0;
            }
            m_channels[c].active = false;
            m_channels[c].name[0] = '\0';
        }
    }
    m_L = nullptr;  // safe sentinel — prevents double-unref on subsequent clearHandlers() call.
                    // Must be set AFTER all luaL_unref calls above. setLuaState() re-arms it.
}

int LuaEventBus::getActiveSubscriberCount() const {
    int count = 0;
    for (int c = 0; c < MAX_CHANNELS; ++c) {
        if (!m_channels[c].active) continue;
        for (int s = 0; s < MAX_SUBS_PER_CH; ++s) {
            if (m_channels[c].subs[s].active) ++count;
        }
    }
    return count;
}

} // namespace enjin2
