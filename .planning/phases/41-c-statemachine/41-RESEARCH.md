# Phase 41: C_StateMachine - Research

**Researched:** 2026-02-28
**Domain:** Per-object Lua FSM, luaL_ref callback storage, deferred state transition pattern, ComponentProxy integration
**Confidence:** HIGH

## Summary

Phase 41 adds `C_StateMachine`, a `Component` subclass that gives Lua scripts a named-state FSM with `enter`, `exit`, and `update` callbacks. It is retrieved via `self:get("C_StateMachine")` (the ComponentProxy infrastructure from Phase 39) and exposes three methods: `fsm:addState(name, {enter, exit, update})`, `fsm:setState(name)`, and `fsm:getState()`. Each callback is stored as a `luaL_ref` in the Lua registry — the same pattern that Phase 40 (C_Timer) established.

The most important design constraint is deferred transitions (FSM-04). The existing `SceneStateMachine` in this codebase already implements this exact pattern: `switchTo(id)` sets `hasPendingTransition = true` and stores `pendingSceneId`, then at the end of `update()` the pending transition is applied via `applyDeferredTransition()`. `C_StateMachine` must replicate this pattern verbatim: calling `fsm:setState(name)` from inside any callback (including `update`) must queue the transition rather than applying it immediately, preventing re-entrant FSM corruption.

The architecture is entirely internal; no external libraries are required. Everything C_StateMachine needs is already established: `luaL_ref` / `lua_pcall` via Phase 40, ComponentProxy metatable registration via Phase 39, ScriptProxy registry-key lookup via `lua_script.cpp::callWithProxy`, and the deferred-transition pattern from `scene_state_machine.hpp`. C_StateMachine is assembly of verified patterns.

**Primary recommendation:** Implement C_StateMachine as a `Component` with a fixed-size array of `StateEntry` structs (named state storage with three `luaL_ref` handles each), a `m_currentState` string, a `m_pendingState` string, and a `m_hasPending` flag. The `update(dt)` method calls the active state's `update(self, dt)` callback first, then applies any pending transition at the end. State callbacks fire in order: exit(self) for the leaving state, then enter(self) for the entering state.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FSM-01 | `fsm:addState(name, {enter, exit, update})` defines a named state with three optional callbacks | Fixed StateEntry array with three `luaL_ref` handles per slot; callback refs set to `LUA_NOREF` when the table key is absent or nil |
| FSM-02 | `fsm:setState(name)` transitions between states, invoking exit on the current state and enter on the new one | Transition is deferred — `m_pendingState` set, `m_hasPending = true`; applied at end of `update()` after active state's update callback runs; exit/enter fire during `applyPendingTransition()` |
| FSM-03 | `fsm:getState()` returns the name of the currently active state | `m_currentState` is a fixed-size char buffer (`char m_currentState[32]`) storing the current state name; `getState()` returns a `const char*`; pushed as Lua string |
| FSM-04 | State transitions are deferred (applied after current frame's update) — same as SceneStateMachine | `SceneStateMachine::switchTo()` is the direct model: set `hasPendingTransition = true`, store target ID, apply after `currentScene->update()` returns in `update()` loop |
| FSM-05 | State `update(self, dt)` callback called each frame while state is active | In `C_StateMachine::update(dt)`: look up `m_currentState` slot, fire `update` ref if non-`LUA_NOREF` via `lua_rawgeti` + push ScriptProxy + push dt + `lua_pcall(3 args, 0 results)` |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua (LuaJIT) | Bundled in `/luajit/` | `luaL_ref` / `luaL_unref` for callback storage; `lua_rawgeti` / `lua_pcall` for callback invocation | Already the scripting runtime; pattern established by Phase 40 |
| C++ Component base class | Project-internal | `update(float dt)` drives FSM tick; `~Component()` invalidates proxy (Phase 39) | Standard component lifecycle |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `luaL_newmetatable` / `luaL_checkudata` | Lua 5.1 API | `"C_StateMachine_Proxy"` typed userdata metatable | Same pattern as `C_Timer_Proxy` from Phase 40 |
| Fixed-size arrays | C++11 `std::array` or raw array | Zero-allocation state entry storage; fixed-size state name buffers | Project requires zero dynamic allocation |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Fixed-size state array with char name buffers | `std::map<std::string, StateEntry>` | `std::map` / `std::string` require heap allocation — incompatible with zero-alloc constraint |
| Deferred transition (FSM-04) | Immediate transition inside `setState()` | Immediate transitions allow re-entrant FSM corruption: calling `setState()` inside an `enter()` or `update()` callback would invalidate the state being processed. SceneStateMachine's deferred model is the correct pattern. |
| Fixed-size state name buffer (`char[32]`) | `std::string` state names | `std::string` is heap-allocated; raw char buffers match existing zero-alloc patterns (`Object::name`, `C_LuaScript::errorMessage[256]`) |

**Installation:** No new dependencies. All required infrastructure already present.

## Architecture Patterns

### Recommended Project Structure

New files for Phase 41:
```
include/enjin2/components/state_machine.hpp    # C_StateMachine class declaration
src/components/state_machine.cpp               # C_StateMachine update(), applyPendingTransition(), destructor
tests/state_machine_test.cpp                   # FSM-01..FSM-05 test suite
```

Modified files:
```
src/scripting/bindings.cpp                     # Add C_StateMachine_Proxy metatable + "C_StateMachine" branch in lua_proxy_get_component_impl
include/enjin2/scripting/bindings.hpp          # Include or forward-declare C_StateMachine
tests/CMakeLists.txt                           # Register state_machine_test under ENJIN2_BUILD_LUA guard
src/components/CMakeLists.txt (or root CMake)  # Register state_machine.cpp in build
```

### Pattern 1: StateEntry Fixed-Size Array

**What:** A fixed-capacity array of `StateEntry` structs stored directly in `C_StateMachine`. Each entry holds the state name (fixed char buffer), three callback refs (enter, exit, update), and an active flag. The number of states per FSM is compile-time bounded.

**When to use:** Always — zero-allocation is a hard project constraint.

**Example:**
```cpp
// include/enjin2/components/state_machine.hpp
#pragma once
#include "../core/component.hpp"
#include "../scripting/lua_platform.hpp"

namespace enjin2 {

class C_StateMachine : public Component {
public:
    static constexpr int MAX_STATES      = 8;   ///< Maximum named states per FSM
    static constexpr int MAX_STATE_NAME  = 32;  ///< Maximum state name length (incl. NUL)

private:
    struct StateEntry {
        char name[MAX_STATE_NAME]{};        ///< State name (NUL-terminated fixed buffer)
        int  enterRef{LUA_NOREF};           ///< luaL_ref for enter(self) callback
        int  exitRef{LUA_NOREF};            ///< luaL_ref for exit(self) callback
        int  updateRef{LUA_NOREF};          ///< luaL_ref for update(self, dt) callback
        bool active{false};                 ///< Slot in use
    };

    StateEntry  m_states[MAX_STATES];
    lua_State*  m_L{nullptr};               ///< Non-owning; valid while C_LuaScript Lua state is open
    char        m_currentState[MAX_STATE_NAME]{};  ///< Name of active state (empty = no state)
    char        m_pendingState[MAX_STATE_NAME]{};  ///< Name of queued transition target
    bool        m_hasPending{false};        ///< True when a deferred setState() is pending

public:
    explicit C_StateMachine(Object* owner);
    ~C_StateMachine();

    void update(float dt) override;

    // Called by Lua bindings
    bool addState(const char* name, int enterRef, int exitRef, int updateRef);
    void setState(const char* name);       // Deferred — applies next update()
    const char* getState() const;          // Returns m_currentState (empty string = no state)
    void clearStates();                    // Called by destructor and hot-reload

    void setLuaState(lua_State* L) { m_L = L; }
    lua_State* getLuaState() const { return m_L; }

private:
    StateEntry* findState(const char* name);
    void applyPendingTransition();         // Called at end of update() when m_hasPending
    void fireCallback(int ref, float dt, bool passDt);  // Pushes ScriptProxy, optionally dt, calls pcall
};

} // namespace enjin2
```

### Pattern 2: luaL_ref Callback Storage Per State (FSM-01)

**What:** `fsm:addState(name, callbacks)` receives the state name as a string and a Lua table with optional `enter`, `exit`, `update` keys. For each key: if present and a function, `lua_pushvalue` + `luaL_ref(L, LUA_REGISTRYINDEX)` anchors it. If absent or nil, the ref stays `LUA_NOREF`.

**When to use:** Always — this is the established Lua C-API pattern for storing functions. Phase 40 uses the exact same mechanism.

**Example:**
```cpp
// In the Lua binding for fsm:addState(name, callbacks_table):
static int lua_fsm_addState(lua_State* L) {
    // Stack: [1]=C_StateMachine_Proxy, [2]=name_string, [3]=table {enter=fn, exit=fn, update=fn}
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, "C_StateMachine_Proxy"));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);

    auto* fsm = static_cast<enjin2::C_StateMachine*>(proxy->component);
    fsm->setLuaState(L);

    // Extract optional callbacks from table
    int enterRef = LUA_NOREF, exitRef = LUA_NOREF, updateRef = LUA_NOREF;

    lua_getfield(L, 3, "enter");
    if (lua_isfunction(L, -1)) enterRef = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    lua_getfield(L, 3, "exit");
    if (lua_isfunction(L, -1)) exitRef = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    lua_getfield(L, 3, "update");
    if (lua_isfunction(L, -1)) updateRef = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    if (!fsm->addState(name, enterRef, exitRef, updateRef)) {
        // Cleanup refs if addState fails (name too long or array full)
        if (enterRef != LUA_NOREF)  luaL_unref(L, LUA_REGISTRYINDEX, enterRef);
        if (exitRef  != LUA_NOREF)  luaL_unref(L, LUA_REGISTRYINDEX, exitRef);
        if (updateRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, updateRef);
        luaL_error(L, "C_StateMachine: too many states or name too long");
    }
    return 0;
}
```

### Pattern 3: Deferred Transition (FSM-04) — SceneStateMachine Model

**What:** `fsm:setState(name)` only queues the transition. It sets `m_hasPending = true` and copies the target name to `m_pendingState`. The actual transition (exit old state, enter new state) happens at the **end** of `C_StateMachine::update()`, after the active state's `update` callback has returned. This prevents re-entrant corruption.

**Why:** This is the exact same pattern used by `SceneStateMachine::switchTo()` + `update()` + `applyDeferredTransition()` in `scene_state_machine.hpp`. The deferred pattern means:
- A callback calling `setState()` will not immediately invalidate the currently-executing callback's state.
- "Last-wins" semantics: multiple `setState()` calls in one frame are safe — only the last one takes effect.
- `setState()` from inside `onDeactivate()` (equivalent: exit callback) safely queues for next frame.

**Example:**
```cpp
// C_StateMachine::update() — the FSM-04 deferred pattern
void C_StateMachine::update(float dt) {
    if (!m_L) return;

    // Step 1: Fire active state's update callback (FSM-05)
    StateEntry* current = findState(m_currentState);
    if (current && current->updateRef != LUA_NOREF) {
        fireCallback(current->updateRef, dt, true);  // update(self, dt)
    }

    // Step 2: Apply pending transition AFTER update callback returns (FSM-04)
    // hasPending cleared BEFORE applyPendingTransition() so that any setState()
    // called from exit()/enter() callbacks queues for the NEXT frame.
    if (m_hasPending) {
        m_hasPending = false;
        applyPendingTransition();
    }
}

// C_StateMachine::applyPendingTransition()
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
```

**Critical ordering note:** `m_hasPending = false` is cleared **before** `applyPendingTransition()` is called — identical to how `SceneStateMachine::update()` clears `hasPendingTransition = false` before calling `applyDeferredTransition(targetId)`. This ensures that a `setState()` call from inside an `exit()` or `enter()` callback queues for next frame, not the current one.

### Pattern 4: ScriptProxy Retrieval in Callbacks (FSM-02, FSM-05)

**What:** Every callback invocation must push the `ScriptProxy` userdata as the first argument. The ScriptProxy is in the Lua registry at key `lightuserdata(C_LuaScript*)`. This is the same lookup used by `C_Timer` and `callWithProxy` in `lua_script.cpp`.

**Example:**
```cpp
// C_StateMachine::fireCallback()
void C_StateMachine::fireCallback(int ref, float dt, bool passDt) {
    if (!m_L || ref == LUA_NOREF) return;

    lua_rawgeti(m_L, LUA_REGISTRYINDEX, ref);
    if (!lua_isfunction(m_L, -1)) {
        lua_pop(m_L, 1);
        return;
    }

    // Push ScriptProxy as self (same registry-key pattern as callWithProxy)
    C_LuaScript* script = owner->getComponent<C_LuaScript>();
    if (script) {
        lua_pushlightuserdata(m_L, script);
        lua_gettable(m_L, LUA_REGISTRYINDEX);
    } else {
        lua_pushnil(m_L);
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
```

### Pattern 5: C_StateMachine_Proxy Metatable Registration

**What:** A `"C_StateMachine_Proxy"` metatable registered alongside `"C_Timer_Proxy"` in `LuaBindings::registerComponentProxyMetatable()` (or wherever Phase 40 places it). Methods: `addState`, `setState`, `getState`.

**Example:**
```cpp
// Added to bindings.cpp (same function that registers C_Timer_Proxy):

static constexpr const char* CFSM_PROXY_METATABLE = "C_StateMachine_Proxy";

static int lua_cfsm_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CFSM_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (strcmp(key, "addState") == 0) {
        lua_pushcfunction(L, lua_fsm_addState);  return 1;
    } else if (strcmp(key, "setState") == 0) {
        lua_pushcfunction(L, lua_fsm_setState);  return 1;
    } else if (strcmp(key, "getState") == 0) {
        lua_pushcfunction(L, lua_fsm_getState);  return 1;
    }
    lua_pushnil(L);
    return 1;
}

// In registerComponentProxyMetatable(), after C_Timer_Proxy block:
if (luaL_newmetatable(L, CFSM_PROXY_METATABLE)) {
    lua_pushcfunction(L, lua_cfsm_proxy_index_impl);
    lua_setfield(L, -2, "__index");
}
lua_pop(L, 1);
```

### Pattern 6: "C_StateMachine" Dispatch Entry in lua_proxy_get_component_impl

**What:** One additional `else if` branch in the `lua_proxy_get_component_impl` function that Phase 39 creates (and Phase 40 extends).

**Example:**
```cpp
// In lua_proxy_get_component_impl — add after "C_Timer" branch:
else if (strcmp(typeName, "C_StateMachine") == 0) {
    comp = owner->getComponent<enjin2::C_StateMachine>();
    metaName = "C_StateMachine_Proxy";
}
```

### Pattern 7: Hot-Reload and Destruction Cleanup

**What:** `clearStates()` releases all `luaL_ref` handles and resets the FSM to empty. Called from both `~C_StateMachine()` and from `C_LuaScript::executeScript()` / `loadScriptFile()` (the same two sites where C_Timer calls `clearTimers()`). Same two-pronged destruction-order safety pattern as Phase 40.

**Example:**
```cpp
// src/components/state_machine.cpp
void C_StateMachine::clearStates() {
    if (!m_L) return;
    for (int i = 0; i < MAX_STATES; ++i) {
        StateEntry& e = m_states[i];
        if (!e.active) continue;
        if (e.enterRef  != LUA_NOREF) { luaL_unref(m_L, LUA_REGISTRYINDEX, e.enterRef);  e.enterRef  = LUA_NOREF; }
        if (e.exitRef   != LUA_NOREF) { luaL_unref(m_L, LUA_REGISTRYINDEX, e.exitRef);   e.exitRef   = LUA_NOREF; }
        if (e.updateRef != LUA_NOREF) { luaL_unref(m_L, LUA_REGISTRYINDEX, e.updateRef); e.updateRef = LUA_NOREF; }
        e.active = false;
        e.name[0] = '\0';
    }
    m_currentState[0] = '\0';
    m_pendingState[0] = '\0';
    m_hasPending = false;
    m_L = nullptr;  // sentinel — prevents double-unref in destructor
}
```

### Anti-Patterns to Avoid

- **Applying setState() immediately inside a callback:** Re-entrant FSM corruption. If `enter()` calls `setState("idle")`, and that immediately fires `exit("idle")` + `enter(next)`, the outer `enter()` is still on the Lua call stack. Use the deferred pattern — SceneStateMachine is the model.
- **Storing state names as `std::string`:** Heap allocation, incompatible with zero-alloc constraint. Fixed `char[32]` buffers match `C_LuaScript::errorMessage[256]` and `Object::name` patterns.
- **Not releasing luaL_refs on clearStates():** Each state has three refs. All three must be `luaL_unref`'d. Missing even one causes a Lua GC leak that persists across hot-reloads.
- **Using `std::map` or `std::unordered_map` for state storage:** Both require heap allocation. Fixed array with linear search by name is correct for MAX_STATES = 8.
- **Calling exit() before setting m_hasPending = false:** If exit() calls setState(), that call must queue for next frame. Clear the pending flag first, then fire callbacks — same ordering discipline as SceneStateMachine.
- **Forgetting that `getState()` should return empty string (not nil/crash) when no state is active:** `m_currentState[0] = '\0'` on construction; `lua_pushstring(L, "")` when empty rather than `lua_pushnil(L)` — preserves type consistency for callers.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Callback reference anchoring | Custom function pointer struct | `luaL_ref` / `luaL_unref` | Standard Lua pattern; already established by Phase 40 |
| State name storage | `std::string` or `std::map` | Fixed `char[MAX_STATE_NAME]` array | Zero-alloc constraint; O(n) linear search is fine for n=8 |
| Deferred transition mechanism | Complex queueing system | Single `m_pendingState` + `m_hasPending` bool | SceneStateMachine proves last-wins single-pending is sufficient; no game needs multiple queued FSM transitions per frame |
| ScriptProxy lookup at callback time | Store a second luaL_ref to the proxy | `lua_pushlightuserdata(L, script)` + `lua_gettable(LUA_REGISTRYINDEX)` | ScriptProxy is already in the registry at the established key; a second ref would be redundant and requires extra cleanup |

**Key insight:** C_StateMachine is the composition of three already-proven patterns: (1) luaL_ref callback storage from Phase 40, (2) ComponentProxy metatable dispatch from Phase 39, and (3) deferred-transition from SceneStateMachine. There is nothing novel to invent.

## Common Pitfalls

### Pitfall 1: Immediate Transition Causes Re-Entrant FSM Corruption
**What goes wrong:** If `setState()` immediately calls `exit()` + `enter()` while a callback is on the Lua call stack, the old state's stack frame is corrupted or a second state's `enter()` fires while the first `enter()` is still executing.
**Why it happens:** `lua_pcall` is synchronous. The callback runs inline. Any `setState()` called from within the callback would immediately tear down the current state while it's still executing.
**How to avoid:** Deferred transition: `setState()` only sets `m_pendingState` and `m_hasPending = true`. The actual transition fires at the end of `update()` after all callbacks return. This is the SceneStateMachine pattern.
**Warning signs:** FSM-04 test fails; Lua errors mention "stack corruption" or double-entry.

### Pitfall 2: Destruction Order — lua_State Outlives C_StateMachine
**What goes wrong:** If `C_LuaScript` destructs before `C_StateMachine` (added to Object first, destructs first via `std::array` index order), `C_StateMachine::~C_StateMachine()` calls `luaL_unref` on a closed Lua state.
**Why it happens:** Identical to Phase 40 Pitfall 1: `Object::components` is a `std::array<std::unique_ptr<Component>, 16>`. Slots are destroyed in index order 0→15. `C_LuaScript` is typically added first.
**How to avoid:** Two-pronged approach (identical to C_Timer):
  1. `C_LuaScript::~C_LuaScript()` calls `owner->getComponent<C_StateMachine>()->clearStates()` BEFORE shutting down the Lua state. (Same location already established for `C_Timer::clearTimers()`.)
  2. `C_StateMachine::clearStates()` sets `m_L = nullptr` after releasing all refs. `~C_StateMachine()` checks `m_L != nullptr` before any Lua calls.
**Warning signs:** ASAN/Valgrind reports use-after-free in `luaL_unref` during Object destruction.

### Pitfall 3: setState() During enter() Fires Immediately
**What goes wrong:** A state's `enter` callback calls `fsm:setState("other")`. If the transition is immediate, `exit("current")` fires while `enter("current")` is still on the Lua call stack, and then `enter("other")` fires, resulting in an illogical state: two `enter()` calls without an intervening `exit()`.
**Why it happens:** Same root as Pitfall 1 — incorrect immediate-transition implementation.
**How to avoid:** Deferred pattern (see Pattern 3). `setState()` from inside `enter()` queues for next frame. The current `enter()` completes normally.
**Warning signs:** State history appears to skip states; game behaves as if two `enter()` callbacks fired in one frame.

### Pitfall 4: Not Releasing All Three Refs Per State
**What goes wrong:** Only `updateRef` is released on `clearStates()`, leaking `enterRef` and `exitRef`. After hot-reload, Lua memory grows because the old callback closures are never GC'd.
**Why it happens:** Three refs per state is easy to miss — developers familiar with simpler systems may only track one callback per "timer-like" entry.
**How to avoid:** `clearStates()` must check all three refs (`enterRef`, `exitRef`, `updateRef`) for each active slot and call `luaL_unref` on each that is not `LUA_NOREF`.
**Warning signs:** `engine.lua.memory()` grows across hot-reloads even when no new states are added.

### Pitfall 5: getState() Returns nil When No State Is Active
**What goes wrong:** Lua code does `if fsm:getState() == "idle" then ...` and it crashes because `getState()` returns `nil` before any state is set.
**Why it happens:** Implementation pushes `lua_pushnil(L)` when `m_currentState[0] == '\0'`.
**How to avoid:** Push `lua_pushstring(L, "")` (empty string) when no state is active. Lua code can check `fsm:getState() ~= ""` or `fsm:getState() == "idle"` consistently.
**Warning signs:** Lua error "attempt to compare nil with string".

### Pitfall 6: forgetting to clear FSM on hot-reload
**What goes wrong:** After F5 hot-reload, old state callbacks from the previous script load are still anchored in the Lua registry. When `update()` fires them, they run stale Lua functions that no longer match the reloaded script's state.
**Why it happens:** `C_LuaScript::executeScript()` creates a fresh ScriptProxy but does not notify `C_StateMachine` to clear its state definitions.
**How to avoid:** Add `owner->getComponent<C_StateMachine>()->clearStates()` to `C_LuaScript::executeScript()` and `loadScriptFile()` alongside the existing `clearTimers()` call (Phase 40 establishes this site).
**Warning signs:** After hot-reload, FSM update callbacks fire but refer to functions that were only defined in the old script version.

### Pitfall 7: State Name Length Exceeds MAX_STATE_NAME
**What goes wrong:** `strncpy(m_currentState, name, MAX_STATE_NAME - 1)` silently truncates long state names, causing `findState("very_long_state_name_that_exceeds_limit")` to return nullptr even though the state was nominally added.
**Why it happens:** Fixed-size buffers require truncation. The name used in `addState` and `setState` must match exactly, and truncation breaks that invariant.
**How to avoid:** In `addState()`, check `strlen(name) >= MAX_STATE_NAME` and return false (triggering a Lua error). Document that state names must be < 32 characters. This is not a realistic constraint for game code.
**Warning signs:** `findState()` returns nullptr for a state that was successfully added — logged as "state not found" error.

## Code Examples

Verified patterns from existing codebase:

### Deferred Transition Pattern (SceneStateMachine — direct model for FSM-04)
```cpp
// Source: include/enjin2/core/scene_state_machine.hpp

// Queuing (called from update() or a callback):
void switchTo(uint32_t sceneId) {
    // ... validate sceneId ...
    pendingSceneId = sceneId;
    hasPendingTransition = true;
}

// Application (called at end of update() after currentScene->update() returns):
void update(float dt) {
    // ... update transition animation ...
    if (currentScene) { currentScene->update(dt); }

    // CRITICAL: hasPendingTransition cleared BEFORE applyDeferredTransition()
    // so that setState() called from exit/enter callbacks queues for NEXT frame
    if (hasPendingTransition) {
        hasPendingTransition = false;        // ← cleared first
        uint32_t targetId = pendingSceneId;
        pendingSceneId = 0;
        applyDeferredTransition(targetId);   // ← fires exit/enter callbacks
    }
}
```

### luaL_ref Callback Storage (established in Phase 40, replicated here)
```cpp
// Pattern: anchor a Lua function in the registry
lua_pushvalue(L, func_stack_index);       // push function copy
int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops it, returns int ref (>= 0 or LUA_NOREF)

// Invoke later:
lua_rawgeti(m_L, LUA_REGISTRYINDEX, ref);
// push args...
lua_pcall(m_L, nargs, 0, 0);

// Release:
luaL_unref(m_L, LUA_REGISTRYINDEX, ref);
ref = LUA_NOREF;
```

### ScriptProxy Registry Lookup (from lua_script.cpp — canonical key for C_StateMachine callbacks)
```cpp
// Source: src/components/lua_script.cpp — callWithProxy()
lua_pushlightuserdata(L, this);      // key: C_LuaScript* pointer (lightuserdata)
lua_gettable(L, LUA_REGISTRYINDEX); // pushes ScriptProxy userdata
// stack top is now ScriptProxy userdata — pass as first arg to pcall
```

### Expected Lua Usage (from requirements)
```lua
-- FSM-01: define states
fsm:addState("idle", {
    enter  = function(self) engine.log("entering idle") end,
    update = function(self, dt) end,
    exit   = function(self) engine.log("leaving idle") end,
})
fsm:addState("running", {
    enter  = function(self) engine.log("entering running") end,
    update = function(self, dt)
        self.x = self.x + 100 * dt
        if self.x > 200 then
            fsm:setState("idle")  -- FSM-02: deferred — fires next frame
        end
    end,
})

-- FSM-02: initial transition
fsm:setState("idle")   -- deferred: enter("idle") fires next update()

-- FSM-03: query current state
local state = fsm:getState()  -- returns "idle" (or "" if none)
```

### C_StateMachine test harness approach
```cpp
// Simulate update to drive FSM forward and verify deferred behavior:
// 1. Load script that calls fsm:addState() and fsm:setState("idle") in init()
// 2. Call script->update(0.0f) once — enter("idle") fires (deferred applied)
// 3. Call script->update(0.0f) again — update("idle") fires
// 4. Verify FSM-03: getScriptString checks Lua global "current_state" set in enter callback
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual if/elseif state machine in Lua update() | C_StateMachine component with named states | Phase 41 | Cleaner Lua code; FSM logic separated from object logic |
| No deferred transitions | Deferred setState() — SceneStateMachine model | Phase 41 | Prevents re-entrant FSM corruption; allows setState() from any callback |
| luaL_ref never used for multi-callback objects in this codebase | Three luaL_refs per state (enter/exit/update) | Phase 41 | Extends Phase 40 pattern from single-ref to multi-ref per "item" |

## Open Questions

1. **What should `fsm:setState()` do if the target state name is not registered?**
   - What we know: `findState(name)` returns nullptr for unregistered names. `applyPendingTransition()` would call exit on the old state and then attempt to enter a state that doesn't exist — silently resulting in an empty `m_currentState` with no enter callback.
   - What's unclear: Whether this should silently fail (log + ignore) or raise a Lua error.
   - Recommendation: Log a warning via `printf("[C_StateMachine] setState: unknown state '%s'\n", name)` and proceed — silently clearing the current state is recoverable. Raising a Lua error inside `applyPendingTransition()` (which runs from `update()`) would disable the script permanently via `ScriptErrorPolicy::Disable`. A warning is safer.

2. **Should `fsm:setState()` to the currently active state trigger a full reset (exit + enter)?**
   - What we know: `SceneStateMachine::switchTo(currentId)` DOES trigger a full reset (deactivate + resetInitialized + activate). This is documented as intentional: "self-transitions trigger a full reset cycle."
   - What's unclear: Whether the same semantics are wanted for C_StateMachine.
   - Recommendation: Yes — `setState("current")` should trigger `exit("current")` + `enter("current")`. This is consistent with SceneStateMachine semantics and is useful for resetting a state. Document it.

3. **Should the initial call to `fsm:setState(name)` in `init()` take effect immediately or be deferred?**
   - What we know: `fsm:setState(name)` is always deferred by the design — it only queues the transition. If called from `init()` (which is called from `C_LuaScript::executeScript()` → `callWithProxy(INIT_FUNCTION)`), the transition is pending but `C_StateMachine::update()` has not run yet. The enter callback fires on the first `update()` call.
   - What's unclear: Whether game scripts expect the FSM to be in the initial state immediately after `init()` returns.
   - Recommendation: This is fine — `init()` is called before the first `update()`. The first `update()` will fire the enter callback for the initial state. Document this in the header: "first setState() in init() takes effect on the first update() call." Tests must simulate at least one update() call after init to verify FSM-02.

4. **Where to add `clearStates()` calls in C_LuaScript?**
   - What we know: Phase 40 establishes that `C_LuaScript::~C_LuaScript()` and `C_LuaScript::executeScript()` / `loadScriptFile()` are the two cleanup sites.
   - Recommendation: Identical to `clearTimers()` placement. Add `owner->getComponent<C_StateMachine>()` check + `clearStates()` call in the same locations. If Phase 40 is not yet implemented, both sites need both `clearTimers()` and `clearStates()` additions.

5. **Should MAX_STATES be 8?**
   - What we know: v1.6 target games are small (Arkanoid, physics sandbox, tamagotchi). An Arkanoid ball might have states: idle, launching, playing, dead — 4 states. 8 is generous.
   - Recommendation: Default to 8. `static constexpr int MAX_STATES = 8`. Easy to increase; no planner decision required.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Hand-rolled (ASSERT macro + pass/failure counters) — matches all existing tests |
| Config file | none — vanilla ctest |
| Quick run command | `ctest -R state_machine_test --output-on-failure` (from build dir) |
| Full suite command | `ctest --output-on-failure` (from build dir) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FSM-01 | `fsm:addState("idle", {enter=fn, exit=fn, update=fn})` stores state without error | unit (C++ + Lua) | `ctest -R state_machine_test` | No — Wave 0 |
| FSM-02 | `fsm:setState("idle")` fires enter("idle") after next update(); `fsm:setState("running")` fires exit("idle") then enter("running") | unit (Lua) | `ctest -R state_machine_test` | No — Wave 0 |
| FSM-03 | `fsm:getState()` returns current state name string | unit (Lua) | `ctest -R state_machine_test` | No — Wave 0 |
| FSM-04 | `setState()` called from inside update callback takes effect next frame (not current) | unit (Lua) | `ctest -R state_machine_test` | No — Wave 0 |
| FSM-05 | Active state's update(self, dt) is called each frame | unit (C++ + Lua) | `ctest -R state_machine_test` | No — Wave 0 |

**Testing approach for FSM-02 (enter/exit sequence):** Create Object + C_LuaScript + C_StateMachine. Load script that defines `"idle"` and `"running"` states; each callback sets a Lua global (`entered_idle`, `exited_idle`, etc.) to `true`. Call `init()` (which calls `setState("idle")`), then `update(0.016f)` twice. After first update: `entered_idle == true`. After `setState("running")` + another update: `exited_idle == true` and `entered_running == true`. Verify with `script->getScriptBool("entered_idle")`.

**Testing approach for FSM-04 (deferred transition):** Script's `update` callback for "idle" calls `fsm:setState("running")`. Verify that after `update(dt)`, the state has NOT yet changed to "running" from within the same update (i.e., no double-entry), and that `update("idle")` ran to completion, and then on the next frame check reveals "running" is active.

**Testing approach for FSM-05 (update called each frame):** Register "idle" with an `update` callback that increments a Lua global counter. Call `update(0.016f)` three times. Counter should be 3 (not 0 or 1).

**Memory leak testing for all luaL_ref cleanup:** After loading script with states, check `engine.lua.memory()` baseline. Hot-reload (`loadScript()` again). Check memory after GC. Should return to near-baseline. Alternatively: C++-level — after `clearStates()`, verify all `m_states[i].enterRef == LUA_NOREF` etc.

### Sampling Rate
- **Per task commit:** `ctest -R state_machine_test --output-on-failure`
- **Per wave merge:** `ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `include/enjin2/components/state_machine.hpp` — C_StateMachine class declaration
- [ ] `src/components/state_machine.cpp` — C_StateMachine update(), applyPendingTransition(), clearStates(), destructor
- [ ] `tests/state_machine_test.cpp` — covers FSM-01, FSM-02, FSM-03, FSM-04, FSM-05
- [ ] `tests/CMakeLists.txt` — `state_machine_test` entry under `ENJIN2_BUILD_LUA` guard
- [ ] `src/scripting/bindings.cpp` — `C_StateMachine_Proxy` metatable + `"C_StateMachine"` branch in `lua_proxy_get_component_impl`
- [ ] `src/components/CMakeLists.txt` (or root CMake) — register `state_machine.cpp` in the build

*(Existing C_LuaScript modifications for clearStates() are in-scope but modify existing files, not gaps.)*

## Sources

### Primary (HIGH confidence)
- Codebase: `include/enjin2/core/scene_state_machine.hpp` — `switchTo()` deferred pattern (lines 200-208), `update()` deferred application (lines 215-235), `applyDeferredTransition()` (lines 369-397). Direct model for FSM-04.
- Codebase: `src/components/lua_script.cpp` — `callWithProxy()` (ScriptProxy registry key pattern), `executeScript()` (proxy invalidation on reload, cleanup insertion point), `~C_LuaScript()` (destruction-order concern, cleanup insertion point)
- Codebase: `src/scripting/bindings.cpp` — `lua_proxy_index_impl` (ScriptProxy `__index` pattern), `registerObjectProxyMetatable()` (metatable registration pattern)
- Codebase: `include/enjin2/core/component.hpp` — Component base class; currently `virtual ~Component() = default;` (Phase 39 changes this)
- Codebase: `include/enjin2/core/object.hpp` — `MAX_COMPONENTS = 16`, `components` array destruction order
- Codebase: `.planning/phases/40-c-timer/40-RESEARCH.md` — established luaL_ref callback pattern, destruction-order pitfall, hot-reload cleanup pattern
- Codebase: `.planning/phases/39-componentproxy/39-RESEARCH.md` — ComponentProxy infrastructure, `lua_proxy_get_component_impl` interface, metatable registration pattern
- Codebase: `.planning/STATE.md` — single-proxy-per-component constraint, ObjectCollection::update() snapshot concern

### Secondary (MEDIUM confidence)
- `.planning/ROADMAP.md` Phase 41 description — success criteria defining Lua API surface
- `.planning/REQUIREMENTS.md` FSM-01..FSM-05 — requirement definitions

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all technology already in use; no external dependencies; patterns verified across multiple files in the codebase
- Architecture: HIGH — deferred transition pattern verified directly in `scene_state_machine.hpp`; luaL_ref pattern verified in Phase 40 research and `bindings_engine.cpp`; ComponentProxy metatable pattern verified in Phase 39 research and `bindings.cpp`
- Pitfalls: HIGH — re-entrancy pitfall verified by reading `scene_state_machine.hpp` deferred design rationale; destruction-order pitfall verified by reading `object.hpp` component array and `lua_script.cpp` destructor; all pitfalls grounded in actual codebase structure

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (stable C++ codebase, 30-day window appropriate)
