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

    /** @brief A single subscriber callback entry */
    struct Subscriber {
        int  callbackRef{LUA_NOREF}; ///< luaL_ref handle; LUA_NOREF = inactive
        int  id{0};                  ///< Subscription ID returned to Lua (for off())
        bool active{false};          ///< Whether this subscriber slot is in use
    };

    /** @brief A named event channel holding its subscriber slots */
    struct Channel {
        char       name[MAX_NAME_LEN]{};       ///< Event name (NUL-terminated)
        Subscriber subs[MAX_SUBS_PER_CH];      ///< Subscriber array for this channel
        bool       active{false};  ///< true = name slot is occupied
    };

private:
    Channel    m_channels[MAX_CHANNELS];
    lua_State* m_L{nullptr};       ///< Non-owning; valid while Lua state is open
    int        m_nextId{1};        ///< Monotonically increasing subscription ID

public:
    /** @brief Set the Lua state for callback dispatch
     *  @param L Lua state pointer */
    void setLuaState(lua_State* L) { m_L = L; }
    /** @brief Get the Lua state
     *  @return Current Lua state pointer */
    lua_State* getLuaState() const { return m_L; }

    /**
     * @brief Register a callback for an event
     * @param name  Event name
     * @param callbackRef  luaL_ref handle for the callback function
     * @return Subscription ID (>0) or 0 on failure
     */
    int subscribe(const char* name, int callbackRef);

    /**
     * @brief Fire all active callbacks for the named event
     *
     * Snapshots refs before pcall loop to be re-entrancy-safe.
     * @param name  Event name
     */
    void emit(const char* name);

    /**
     * @brief Unregister a subscription by ID
     *
     * Calls luaL_unref on the callback.
     * @param id  Subscription ID returned by subscribe()
     */
    void unsubscribe(int id);

    /// Release all luaL_ref handles and reset all slots.
    /// Called on scene deactivation (EVENT-04) and hot-reload (EVENT-05).
    /// Sets m_L = nullptr as safe sentinel to prevent double-unref.
    void clearHandlers();

    /** @brief Get count of active subscribers across all channels (for testing)
     *  @return Number of active subscribers */
    int getActiveSubscriberCount() const;

private:
    Channel* findChannel(const char* name);
    Channel* findOrCreateChannel(const char* name);
};

} // namespace enjin2
