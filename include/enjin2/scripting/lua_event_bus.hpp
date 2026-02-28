#pragma once
#include "lua_platform.hpp"
#include <cstring>

namespace enjin2 {

/**
 * @brief Scene-scoped Lua event bus for named publish/subscribe.
 *
 * Zero heap allocation. Fixed capacity for channels and subscribers.
 * All luaL_ref handles are released on clearHandlers() -- called on
 * scene deactivation (EVENT-04) and hot-reload (EVENT-05).
 */
class LuaEventBus {
public:
    static constexpr int MAX_CHANNELS    = 16;  ///< Max distinct event names
    static constexpr int MAX_SUBS_PER_CH = 8;   ///< Max subscribers per event
    static constexpr int MAX_NAME_LEN    = 64;  ///< Max event name length (including null terminator)

    struct Subscriber {
        int  callbackRef{LUA_NOREF}; ///< luaL_ref handle; LUA_NOREF = inactive
        int  id{0};                  ///< Subscription ID returned to Lua (for off())
        bool active{false};
    };

    struct Channel {
        char       name[MAX_NAME_LEN]{};
        Subscriber subs[MAX_SUBS_PER_CH];
        bool       active{false};  ///< true = name slot is occupied
    };

private:
    Channel    m_channels[MAX_CHANNELS];
    lua_State* m_L{nullptr};       ///< Non-owning; valid while Lua state is open
    int        m_nextId{1};        ///< Monotonically increasing subscription ID

public:
    void setLuaState(lua_State* L) { m_L = L; }
    lua_State* getLuaState() const { return m_L; }

    /// Register a callback for an event. Returns subscription ID (>0) or 0 on failure.
    int subscribe(const char* name, int callbackRef);

    /// Fire all active callbacks for the named event.
    /// Snapshots refs before pcall loop to be re-entrancy-safe.
    void emit(const char* name);

    /// Unregister a subscription by ID. Calls luaL_unref on the callback.
    void unsubscribe(int id);

    /// Release all luaL_ref handles and reset all slots.
    /// Called on scene deactivation (EVENT-04) and hot-reload (EVENT-05).
    /// Sets m_L = nullptr as safe sentinel to prevent double-unref.
    void clearHandlers();

    // Test helpers
    int getActiveSubscriberCount() const;

private:
    Channel* findChannel(const char* name);
    Channel* findOrCreateChannel(const char* name);
};

} // namespace enjin2
