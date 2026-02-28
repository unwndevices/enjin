#pragma once
#include "../core/component.hpp"
#include "../scripting/lua_platform.hpp"
#include <cstring>

namespace enjin2 {

// Forward declarations
class C_LuaScript;

/**
 * @brief Per-object state machine component with Lua enter/update/exit callbacks.
 *
 * Retrieved by Lua scripts via self:get("C_StateMachine"). Exposes three Lua methods:
 *   fsm:addState(name, {enter, exit, update})  — register a named state (FSM-01)
 *   fsm:setState(name)                          — deferred transition (FSM-02, FSM-04)
 *   fsm:getState()                              — query current state (FSM-03)
 *
 * State transitions are DEFERRED: fsm:setState(name) only queues the transition.
 * The actual exit/enter callbacks fire at the END of update(), after the active
 * state's update callback returns. This prevents re-entrant FSM corruption and
 * matches the SceneStateMachine pattern exactly.
 *
 * Each state stores three luaL_ref handles (enter, exit, update). All refs are
 * released in clearStates() (called by destructor and by C_LuaScript on hot-reload).
 *
 * Zero dynamic allocation: fixed-size array of MAX_STATES entries with char[MAX_STATE_NAME] buffers.
 */
class C_StateMachine : public Component {
public:
    static constexpr int MAX_STATES     = 8;   ///< Maximum named states per FSM
    static constexpr int MAX_STATE_NAME = 32;  ///< Maximum state name length (incl. NUL)

    /** @brief Single named state slot */
    struct StateEntry {
        char name[MAX_STATE_NAME]{};        ///< State name (NUL-terminated fixed buffer)
        int  enterRef{LUA_NOREF};           ///< luaL_ref for enter(self) callback
        int  exitRef{LUA_NOREF};            ///< luaL_ref for exit(self) callback
        int  updateRef{LUA_NOREF};          ///< luaL_ref for update(self, dt) callback
        bool active{false};                 ///< Slot in use
    };

    /** @brief Construct a state machine component
     *  @param owner The object that owns this component */
    explicit C_StateMachine(Object* owner);
    /** @brief Destructor — releases all Lua callback refs */
    ~C_StateMachine() override;

    void update(float dt) override;

    // Called by Lua bindings (fsm:addState, fsm:setState, fsm:getState)
    /**
     * @brief Register a named state with enter/exit/update callbacks
     * @param name  State name (NUL-terminated)
     * @param enterRef  luaL_ref for enter(self) callback
     * @param exitRef   luaL_ref for exit(self) callback
     * @param updateRef luaL_ref for update(self, dt) callback
     * @return true if state was added, false if slots are full
     */
    bool addState(const char* name, int enterRef, int exitRef, int updateRef);
    /** @brief Queue a deferred state transition (applies at end of next update())
     *  @param name  Target state name */
    void setState(const char* name);
    /** @brief Query the current state name
     *  @return Current state name (empty string if no state is active) */
    const char* getState() const;

    /**
     * @brief Release all luaL_ref handles and reset FSM to empty state.
     * Called by: ~C_StateMachine(), C_LuaScript::~C_LuaScript() (destruction-order safety),
     * and C_LuaScript::executeScript() / loadScriptFile() (hot-reload cleanup).
     * After this call, m_L is set to nullptr to prevent double-unref.
     */
    void clearStates();

    /** @brief Set the Lua state for callback dispatch
     *  @param L Lua state pointer */
    void setLuaState(lua_State* L) { m_L = L; }
    /** @brief Get the Lua state
     *  @return Current Lua state pointer */
    lua_State* getLuaState() const { return m_L; }

    /** @brief Get a state entry by index (for testing)
     *  @param index Array index
     *  @return Reference to the StateEntry */
    const StateEntry& getStateEntry(int index) const { return m_states[index]; }
    /** @brief Get number of active states (for testing)
     *  @return Count of occupied state slots */
    int getActiveStateCount() const;

private:
    StateEntry  m_states[MAX_STATES];
    lua_State*  m_L{nullptr};                      ///< Non-owning; valid while C_LuaScript Lua state is open
    char        m_currentState[MAX_STATE_NAME]{};   ///< Name of active state (empty = no state)
    char        m_pendingState[MAX_STATE_NAME]{};   ///< Name of queued transition target
    bool        m_hasPending{false};                ///< True when a deferred setState() is pending

    StateEntry* findState(const char* name);
    void applyPendingTransition();          ///< Called at end of update() when m_hasPending
    void fireCallback(int ref, float dt, bool passDt);  ///< Pushes ScriptProxy, optionally dt, calls pcall
};

} // namespace enjin2
