#include "../../include/enjin2/scripting/lua_event_bus.hpp"
#include <cstdio>
#include <utility>

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

int LuaEventBus::subscribe(const char* name, lua::ScriptRef callback) {
    if (!m_L || !name) { return 0; }
    Channel* ch = findOrCreateChannel(name);
    if (!ch) {
        // The callback is released as it goes out of scope.
        printf("[EventBus] channel capacity exceeded for '%s'\n", name);
        return 0;
    }
    for (int i = 0; i < MAX_SUBS_PER_CH; ++i) {
        if (!ch->subs[i].active) {
            ch->subs[i].callback   = std::move(callback);
            ch->subs[i].id         = m_nextId++;
            ch->subs[i].active     = true;
            return ch->subs[i].id;
        }
    }
    printf("[EventBus] subscriber capacity exceeded for '%s'\n", name);
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
        return;
    }
    Channel* ch = findChannel(name);
    if (!ch) return;

    // Snapshot the active subscription ids before any pcall (prevents
    // re-entrant mutation). If a handler calls engine.event.off() or
    // engine.event.on(), it modifies the channel's subscriber array but NOT this
    // snapshot: new on() calls from inside a handler fire starting from the
    // next emit(), and a handler off()'d by an earlier one no longer fires.
    int ids[MAX_SUBS_PER_CH];
    int count = 0;
    for (int i = 0; i < MAX_SUBS_PER_CH; ++i) {
        if (ch->subs[i].active && ch->subs[i].callback.valid()) {
            ids[count++] = ch->subs[i].id;
        }
    }

    for (int i = 0; i < count; ++i) {
        if (!m_L || !ch->active) return;   // a handler cleared the bus
        const Subscriber* sub = nullptr;
        for (int k = 0; k < MAX_SUBS_PER_CH; ++k) {
            if (ch->subs[k].active && ch->subs[k].id == ids[i]) { sub = &ch->subs[k]; break; }
        }
        if (!sub) continue;                // off()'d by an earlier handler
        sub->callback.push(m_L);
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
                m_channels[c].subs[s].callback.reset();
                m_channels[c].subs[s].active = false;
                m_channels[c].subs[s].id     = 0;
                return;
            }
        }
    }
}

void LuaEventBus::clearHandlers() {
    for (int c = 0; c < MAX_CHANNELS; ++c) {
        for (int s = 0; s < MAX_SUBS_PER_CH; ++s) {
            m_channels[c].subs[s].callback.reset();
            m_channels[c].subs[s].active = false;
            m_channels[c].subs[s].id     = 0;
        }
        m_channels[c].active = false;
        m_channels[c].name[0] = '\0';
    }
    m_L = nullptr;  // no emit until setLuaState() re-arms the bus
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
