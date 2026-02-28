#include "../../include/enjin2/components/state_machine.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/core/object.hpp"
#include <cstdio>
#include <cstring>

namespace enjin2 {

C_StateMachine::C_StateMachine(Object* owner)
    : Component(owner) {
    // All StateEntry members are value-initialized by default
}

C_StateMachine::~C_StateMachine() {
    clearStates();
}

bool C_StateMachine::addState(const char* name, int enterRef, int exitRef, int updateRef) {
    if (!name) return false;

    // Reject names that would be truncated (Pitfall 7 from research)
    if (strlen(name) >= static_cast<size_t>(MAX_STATE_NAME)) return false;

    // Check for duplicate state name — overwrite if found
    for (int i = 0; i < MAX_STATES; ++i) {
        if (m_states[i].active && strcmp(m_states[i].name, name) == 0) {
            // Release old refs before overwriting
            if (m_L) {
                if (m_states[i].enterRef  != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, m_states[i].enterRef);
                if (m_states[i].exitRef   != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, m_states[i].exitRef);
                if (m_states[i].updateRef != LUA_NOREF) luaL_unref(m_L, LUA_REGISTRYINDEX, m_states[i].updateRef);
            }
            m_states[i].enterRef  = enterRef;
            m_states[i].exitRef   = exitRef;
            m_states[i].updateRef = updateRef;
            return true;
        }
    }

    // Find first free slot
    for (int i = 0; i < MAX_STATES; ++i) {
        if (!m_states[i].active) {
            strncpy(m_states[i].name, name, MAX_STATE_NAME - 1);
            m_states[i].name[MAX_STATE_NAME - 1] = '\0';
            m_states[i].enterRef  = enterRef;
            m_states[i].exitRef   = exitRef;
            m_states[i].updateRef = updateRef;
            m_states[i].active    = true;
            return true;
        }
    }

    // No free slot
    return false;
}

void C_StateMachine::setState(const char* name) {
    if (!name) return;
    if (strlen(name) >= static_cast<size_t>(MAX_STATE_NAME)) {
        printf("[C_StateMachine] setState: name too long '%s'\n", name);
        return;
    }
    strncpy(m_pendingState, name, MAX_STATE_NAME - 1);
    m_pendingState[MAX_STATE_NAME - 1] = '\0';
    m_hasPending = true;
}

const char* C_StateMachine::getState() const {
    return m_currentState;  // Returns "" (empty string) when no state is active
}

C_StateMachine::StateEntry* C_StateMachine::findState(const char* name) {
    if (!name || name[0] == '\0') return nullptr;
    for (int i = 0; i < MAX_STATES; ++i) {
        if (m_states[i].active && strcmp(m_states[i].name, name) == 0) {
            return &m_states[i];
        }
    }
    return nullptr;
}

void C_StateMachine::fireCallback(int ref, float dt, bool passDt) {
    if (!m_L || ref == LUA_NOREF) return;

    lua_rawgeti(m_L, LUA_REGISTRYINDEX, ref);
    if (!lua_isfunction(m_L, -1)) {
        lua_pop(m_L, 1);
        return;
    }

    // Push ScriptProxy as self (same registry-key pattern as callWithProxy and C_Timer)
    C_LuaScript* script = owner->getComponent<C_LuaScript>();
    if (script) {
        lua_pushlightuserdata(m_L, script);
        lua_gettable(m_L, LUA_REGISTRYINDEX);  // pushes ScriptProxy userdata
    } else {
        lua_pushnil(m_L);  // Fallback if no C_LuaScript (should not happen in normal use)
    }

    int nargs = 1;
    if (passDt) {
        lua_pushnumber(m_L, static_cast<lua_Number>(dt));
        nargs = 2;
    }

    if (lua_pcall(m_L, nargs, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        printf("[C_StateMachine] callback error: %s\n", err ? err : "unknown");
        lua_pop(m_L, 1);
    }
}

void C_StateMachine::applyPendingTransition() {
    // Fire exit on current state (if any)
    StateEntry* current = findState(m_currentState);
    if (current && current->exitRef != LUA_NOREF) {
        fireCallback(current->exitRef, 0.0f, false);  // exit(self)
    }

    // Update current state name
    strncpy(m_currentState, m_pendingState, MAX_STATE_NAME - 1);
    m_currentState[MAX_STATE_NAME - 1] = '\0';
    m_pendingState[0] = '\0';

    // Fire enter on new state (if any)
    StateEntry* next = findState(m_currentState);
    if (next && next->enterRef != LUA_NOREF) {
        fireCallback(next->enterRef, 0.0f, false);  // enter(self)
    }
}

void C_StateMachine::update(float dt) {
    if (!m_L) return;

    // Step 1: Fire active state's update callback (FSM-05)
    StateEntry* current = findState(m_currentState);
    if (current && current->updateRef != LUA_NOREF) {
        fireCallback(current->updateRef, dt, true);  // update(self, dt)
    }

    // Step 2: Apply pending transition AFTER update callback returns (FSM-04)
    // CRITICAL: m_hasPending cleared BEFORE applyPendingTransition() so that any
    // setState() called from exit()/enter() callbacks queues for the NEXT frame.
    // This matches SceneStateMachine::update() ordering exactly.
    if (m_hasPending) {
        m_hasPending = false;
        applyPendingTransition();
    }
}

void C_StateMachine::clearStates() {
    if (m_L) {
        for (int i = 0; i < MAX_STATES; ++i) {
            StateEntry& e = m_states[i];
            if (!e.active) continue;
            if (e.enterRef  != LUA_NOREF) { luaL_unref(m_L, LUA_REGISTRYINDEX, e.enterRef);  e.enterRef  = LUA_NOREF; }
            if (e.exitRef   != LUA_NOREF) { luaL_unref(m_L, LUA_REGISTRYINDEX, e.exitRef);   e.exitRef   = LUA_NOREF; }
            if (e.updateRef != LUA_NOREF) { luaL_unref(m_L, LUA_REGISTRYINDEX, e.updateRef); e.updateRef = LUA_NOREF; }
            e.active = false;
            e.name[0] = '\0';
        }
    }
    m_currentState[0] = '\0';
    m_pendingState[0] = '\0';
    m_hasPending = false;
    m_L = nullptr;  // Sentinel: prevents double-unref in destructor
}

int C_StateMachine::getActiveStateCount() const {
    int count = 0;
    for (int i = 0; i < MAX_STATES; ++i) {
        if (m_states[i].active) ++count;
    }
    return count;
}

} // namespace enjin2
