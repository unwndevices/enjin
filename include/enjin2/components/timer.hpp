#pragma once
#include "../core/component.hpp"
#include "../scripting/lua_platform.hpp"

namespace enjin2 {

// Forward declarations
class C_LuaScript;

/**
 * @brief Timer component for scheduling one-shot and repeating Lua callbacks.
 *
 * Retrieved by Lua scripts via self:get("C_Timer"). Exposes three Lua methods:
 *   timer:after(seconds, fn)  — one-shot delayed callback (TIMER-01)
 *   timer:every(seconds, fn)  — repeating callback (TIMER-02)
 *   timer:cancel(id)          — cancel by ID (TIMER-03)
 *
 * Callbacks are stored as luaL_ref handles in the Lua registry. All refs are
 * released in clearTimers() (called by destructor and by C_LuaScript on hot-reload).
 *
 * Zero dynamic allocation: fixed-size array of MAX_TIMERS entries.
 */
class C_Timer : public Component {
public:
    static constexpr int MAX_TIMERS = 8;  ///< Maximum simultaneous timers per component

    /** @brief Single timer slot entry */
    struct TimerEntry {
        int     callbackRef{LUA_NOREF}; ///< luaL_ref handle; LUA_NOREF = inactive
        float   interval{0.0f};         ///< Seconds between firings (or delay for one-shot)
        float   elapsed{0.0f};          ///< Accumulated time since arm
        int     id{0};                  ///< Cancellation ID returned to Lua
        bool    repeating{false};       ///< true = every(), false = after()
        bool    active{false};          ///< Slot in use
    };

    /** @brief Construct a timer component
     *  @param owner The object that owns this component */
    explicit C_Timer(Object* owner);
    /** @brief Destructor — releases all Lua callback refs */
    ~C_Timer() override;

    void update(float dt) override;

    // Called by Lua bindings (timer:after, timer:every, timer:cancel)
    /**
     * @brief Schedule a one-shot callback after a delay
     * @param seconds  Delay in seconds
     * @param callbackRef  luaL_ref handle for the callback
     * @return Timer ID for cancellation, or 0 if no slot available
     */
    int  scheduleAfter(float seconds, int callbackRef);
    /**
     * @brief Schedule a repeating callback at a fixed interval
     * @param seconds  Interval in seconds
     * @param callbackRef  luaL_ref handle for the callback
     * @return Timer ID for cancellation, or 0 if no slot available
     */
    int  scheduleEvery(float seconds, int callbackRef);
    /** @brief Cancel a timer by ID
     *  @param timerId Timer ID returned by scheduleAfter/scheduleEvery */
    void cancel(int timerId);

    /**
     * @brief Release all luaL_ref handles and deactivate all timer entries.
     * Called by: ~C_Timer(), C_LuaScript::~C_LuaScript() (destruction-order safety),
     * and C_LuaScript::executeScript() / loadScriptFile() (hot-reload cleanup, TIMER-05).
     * After this call, m_L is set to nullptr to prevent double-unref.
     */
    void clearTimers();

    /** @brief Set the Lua state for callback dispatch
     *  @param L Lua state pointer */
    void setLuaState(lua_State* L) { m_L = L; }
    /** @brief Get the Lua state
     *  @return Current Lua state pointer */
    lua_State* getLuaState() const { return m_L; }

    /** @brief Get a timer entry by index (for testing)
     *  @param index Array index
     *  @return Reference to the TimerEntry */
    const TimerEntry& getTimerEntry(int index) const { return m_timers[index]; }
    /** @brief Get number of active timers (for testing)
     *  @return Count of active timer slots */
    int getActiveCount() const;

private:
    TimerEntry  m_timers[MAX_TIMERS];
    lua_State*  m_L{nullptr};           ///< Non-owning; valid while C_LuaScript Lua state is open
    int         m_nextId{1};            ///< Monotonically increasing ID counter

    int scheduleInternal(float seconds, int callbackRef, bool repeating);
    void fireCallback(int cbRef);
};

} // namespace enjin2
