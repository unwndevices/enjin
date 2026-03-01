# Phase 40: C_Timer - Research

**Researched:** 2026-02-28
**Domain:** Lua callback timers, luaL_ref lifecycle management, C++ component update loop
**Confidence:** HIGH

## Summary

Phase 40 adds `C_Timer`, a `Component` subclass that lets Lua scripts schedule one-shot and repeating callbacks without busy-polling in `update()`. The timer is retrieved via `self:get("C_Timer")` (from the ComponentProxy infrastructure built in Phase 39), and exposes three Lua methods: `timer:after(seconds, fn)`, `timer:every(seconds, fn)`, and `timer:cancel(id)`. Each callback is stored as a `luaL_ref` in the Lua registry.

The architecture is entirely internal to the project. There are no external libraries to install. The pattern follows the existing proxy/ref lifecycle already used by `ScriptProxy` (stored in the registry via `lua_pushlightuserdata` + `lua_settable`), and the `luaL_ref` pattern is already present in `bindings_engine.cpp` (used to anchor an interned name string in `engine.scene.spawn()`). C_Timer's `update(float dt)` accumulates the elapsed time for each active timer entry and fires the callback via `lua_rawgeti` + `lua_pcall` when the deadline is reached.

The critical design constraint is the pointer to the owning `C_LuaScript`'s `lua_State`. Since `C_Timer` is a sibling component on the same `Object`, it does not have its own `lua_State`. It must hold a pointer to the `lua_State` that created its timers — obtained at timer-registration time from the Lua call itself (`lua_State* L` is always the correct state for the calling script). The `lua_State*` is stored once when the first timer is registered and must remain valid for the lifetime of `C_Timer`. Since all components on an `Object` share the same Lua state (there is only one `C_LuaScript` per object in the current architecture), this is safe.

**Primary recommendation:** Implement C_Timer as a `Component` with a fixed-size array of timer entries (no heap allocation), store Lua callbacks via `luaL_ref` / `luaL_unref`, fire via `lua_rawgeti` + push ScriptProxy + `lua_pcall`, and release all refs in `~C_Timer()` and on `clearTimers()`. Register `"C_Timer_Proxy"` metatable in `LuaBindings::registerComponentProxyMetatable()` and add the `"C_Timer"` branch to `lua_proxy_get_component_impl`.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TIMER-01 | `C_Timer` supports one-shot delayed callback via `timer:after(seconds, callback)` | Fixed timer entry array; entry `repeating = false`; fires once when `elapsed >= deadline` then entry is freed via `luaL_unref` |
| TIMER-02 | `C_Timer` supports repeating callback via `timer:every(seconds, callback)` | Entry `repeating = true`; on fire, reset `elapsed = 0` and re-arm; callback called each interval indefinitely until cancelled |
| TIMER-03 | Timer can be cancelled via `timer:cancel(id)` | Each `timer:after()` / `timer:every()` returns an integer ID; `cancel(id)` calls `luaL_unref`, clears the entry slot |
| TIMER-04 | Timer callbacks receive `self` (ScriptProxy) as first argument | C_Timer stores a registry reference to the ScriptProxy userdata (identical to how `callWithProxy` retrieves it in `lua_script.cpp`); pushed as first arg before `lua_pcall` |
| TIMER-05 | `luaL_ref` handles are cleaned up on component destruction and hot-reload | `C_Timer::~C_Timer()` iterates all active entries and calls `luaL_unref(L, LUA_REGISTRYINDEX, entry.callbackRef)`; `clearTimers()` callable from hot-reload path |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua (LuaJIT) | Bundled in `/luajit/` | `luaL_ref` / `luaL_unref` for callback storage; `lua_rawgeti` / `lua_pcall` for callback invocation | Already the scripting runtime for all components |
| C++ Component base class | Project-internal | `update(float dt)` drives timer tick; `~Component()` invalidates proxy (Phase 39) | Standard component lifecycle |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `luaL_newmetatable` / `luaL_checkudata` | Lua 5.1 API | `"C_Timer_Proxy"` typed userdata metatable | Same as all other component proxy metatables in this codebase |
| Fixed-size array (C++ `std::array` or raw array) | C++11 | Zero-allocation timer entry storage | Project requires zero dynamic allocation; matches `LuaBindings::spritePool[16]` and `LuaBindings::fontRegistry[8]` patterns |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Fixed-size timer array | `std::vector<TimerEntry>` | `std::vector` requires heap allocation — incompatible with zero-alloc project requirement |
| `luaL_ref` per callback | Storing callback as a Lua global | Globals are not instance-scoped; `luaL_ref` keeps the callback alive in the registry per-script-state and is the standard Lua pattern for anchoring functions |
| ScriptProxy ref obtained at registration | ScriptProxy ref obtained at fire time | Fire-time lookup of ScriptProxy from registry (keyed by `C_LuaScript*`) is the correct approach — matches `callWithProxy()` pattern — avoids storing a second ref |

**Installation:** No new dependencies. All required infrastructure already present.

## Architecture Patterns

### Recommended Project Structure

New files for Phase 40:
```
include/enjin2/components/timer.hpp    # C_Timer class declaration
src/components/timer.cpp               # C_Timer update(), clearTimers(), destructor
tests/timer_test.cpp                   # TIMER-01..TIMER-05 test suite
```

Modified files:
```
src/scripting/bindings.cpp             # Add C_Timer_Proxy metatable + "C_Timer" branch in lua_proxy_get_component_impl
include/enjin2/scripting/bindings.hpp  # Forward-declare C_Timer (or include timer.hpp)
tests/CMakeLists.txt                   # Register timer_test under ENJIN2_BUILD_LUA guard
```

### Pattern 1: TimerEntry Fixed-Size Array

**What:** A fixed-capacity array of `TimerEntry` structs stored directly in `C_Timer`. Each entry holds the interval, elapsed time, callback ref, repeat flag, active flag, and an ID. The ID is used for cancellation.

**When to use:** Always — this is the required zero-allocation pattern. Capacity of 8 timers per component is sufficient for v1.6 target games; can be increased via a compile-time constant.

**Example:**
```cpp
// include/enjin2/components/timer.hpp
#pragma once
#include "../core/component.hpp"
#include "../scripting/lua_platform.hpp"

namespace enjin2 {

class C_Timer : public Component {
public:
    static constexpr int MAX_TIMERS = 8;  ///< Maximum simultaneous timers per component

private:
    struct TimerEntry {
        int     callbackRef{LUA_NOREF}; ///< luaL_ref handle; LUA_NOREF = inactive
        float   interval{0.0f};         ///< Seconds between firings (or delay for one-shot)
        float   elapsed{0.0f};          ///< Accumulated time since arm
        int     id{0};                  ///< Cancellation ID returned to Lua
        bool    repeating{false};       ///< true = every(), false = after()
        bool    active{false};          ///< Slot in use
    };

    TimerEntry  m_timers[MAX_TIMERS];
    lua_State*  m_L{nullptr};           ///< Non-owning; valid while C_LuaScript Lua state is open
    int         m_nextId{1};            ///< Monotonically increasing ID counter

public:
    explicit C_Timer(Object* owner);
    ~C_Timer();

    void update(float dt) override;

    // Called by Lua bindings
    int  scheduleAfter(float seconds, int callbackRef);   // returns timer ID
    int  scheduleEvery(float seconds, int callbackRef);   // returns timer ID
    void cancel(int id);
    void clearTimers();  // Called by destructor and hot-reload

    void setLuaState(lua_State* L) { m_L = L; }
    lua_State* getLuaState() const { return m_L; }
};

} // namespace enjin2
```

### Pattern 2: luaL_ref Callback Storage

**What:** `timer:after(seconds, fn)` receives the Lua function on the stack; `lua_pushvalue` + `luaL_ref(L, LUA_REGISTRYINDEX)` anchors it. The returned integer ref is stored in `TimerEntry::callbackRef`. On fire, `lua_rawgeti(L, LUA_REGISTRYINDEX, entry.callbackRef)` pushes the function; on cancel or cleanup, `luaL_unref(L, LUA_REGISTRYINDEX, entry.callbackRef)` releases it.

**When to use:** Always — this is the standard Lua pattern for C-side callback storage. Used once already in `bindings_engine.cpp` for name interning, and the conceptual model is used throughout the scripting layer.

**Example:**
```cpp
// In the Lua binding for timer:after(seconds, fn):
static int lua_timer_after(lua_State* L) {
    // Stack: [1]=C_Timer_Proxy userdata, [2]=seconds, [3]=fn
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, "C_Timer_Proxy"));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    float seconds = static_cast<float>(luaL_checknumber(L, 2));
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // Anchor the callback in the Lua registry
    lua_pushvalue(L, 3);                          // push fn copy
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);     // pops it, returns int ref

    auto* timer = static_cast<enjin2::C_Timer*>(proxy->component);
    timer->setLuaState(L);  // record the Lua state (first call sets it)
    int id = timer->scheduleAfter(seconds, ref);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}
```

### Pattern 3: ScriptProxy Retrieval for Callback Invocation (TIMER-04)

**What:** When a timer fires, it must push the `ScriptProxy` userdata as the first argument before calling the Lua function. The ScriptProxy is stored in the Lua registry keyed by `lightuserdata(C_LuaScript*)`. The `C_LuaScript*` is reachable from `C_Timer` via `owner->getComponent<C_LuaScript>()`.

**When to use:** Any time C_Timer fires a callback — this is required by TIMER-04.

**Example:**
```cpp
// In C_Timer::update() when a timer fires:
void C_Timer::firePending(TimerEntry& entry) {
    if (!m_L) return;

    // Push the Lua function from registry
    lua_rawgeti(m_L, LUA_REGISTRYINDEX, entry.callbackRef);
    if (!lua_isfunction(m_L, -1)) {
        lua_pop(m_L, 1);
        return;
    }

    // Retrieve the ScriptProxy for self (same key as callWithProxy uses)
    // C_LuaScript* is reachable via owner->getComponent<C_LuaScript>()
    C_LuaScript* script = owner->getComponent<C_LuaScript>();
    if (script) {
        lua_pushlightuserdata(m_L, script);
        lua_gettable(m_L, LUA_REGISTRYINDEX);  // pushes ScriptProxy userdata
    } else {
        lua_pushnil(m_L);  // fallback if no C_LuaScript present
    }

    // Call: fn(self) — 1 arg, 0 results
    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        printf("[C_Timer] callback error: %s\n", err ? err : "unknown");
        lua_pop(m_L, 1);
    }
}
```

### Pattern 4: C_Timer_Proxy Metatable Registration

**What:** A `"C_Timer_Proxy"` metatable registered in `LuaBindings::registerComponentProxyMetatable()` (the same function Phase 39 creates). Methods: `after`, `every`, `cancel`. The `__index` function dispatches these method names via `strcmp`.

**When to use:** Called during `LuaBindings::registerAll()`.

**Example:**
```cpp
// In src/scripting/bindings.cpp — added to registerComponentProxyMetatable():

static constexpr const char* CTIMER_PROXY_METATABLE = "C_Timer_Proxy";

static int lua_ctimer_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CTIMER_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (strcmp(key, "after") == 0) {
        lua_pushcfunction(L, lua_timer_after);
        return 1;
    } else if (strcmp(key, "every") == 0) {
        lua_pushcfunction(L, lua_timer_every);
        return 1;
    } else if (strcmp(key, "cancel") == 0) {
        lua_pushcfunction(L, lua_timer_cancel);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

// In registerComponentProxyMetatable(), after the C_Position_Proxy block:
if (luaL_newmetatable(L, CTIMER_PROXY_METATABLE)) {
    lua_pushcfunction(L, lua_ctimer_proxy_index_impl);
    lua_setfield(L, -2, "__index");
}
lua_pop(L, 1);
```

### Pattern 5: self:get("C_Timer") Dispatch Entry

**What:** Add a `"C_Timer"` branch to `lua_proxy_get_component_impl` in `bindings.cpp` (the Phase 39 function). One `else if` clause after the existing `"C_Position"` branch.

**Example:**
```cpp
// In lua_proxy_get_component_impl — add after "C_Position" branch:
else if (strcmp(typeName, "C_Timer") == 0) {
    comp = owner->getComponent<enjin2::C_Timer>();
    metaName = "C_Timer_Proxy";
}
```

This requires including `timer.hpp` at the top of `bindings.cpp`.

### Pattern 6: Hot-Reload Cleanup (TIMER-05)

**What:** When a script hot-reloads (`C_LuaScript::loadScript()` or `reloadScript()` is called again), all outstanding `luaL_ref` handles must be released before the new script state is set up. `C_LuaScript` is the correct point to trigger this since it owns the reload path. The mechanism: before calling `executeScript(code)` on reload, check if a `C_Timer` sibling exists on the same owner and call `clearTimers()` on it.

**When to use:** In `C_LuaScript::loadScript()` and `C_LuaScript::loadScriptFile()` before executing the new script code. This is the same place where the old ScriptProxy is invalidated.

**Example:**
```cpp
// In C_LuaScript::executeScript (and loadScriptFile) — before creating new proxy:
// TIMER-05: release any pending timer callbacks from the previous script load
if (owner) {
    C_Timer* timer = owner->getComponent<C_Timer>();
    if (timer) {
        timer->clearTimers();
    }
}
```

**Alternative approach:** `C_Timer::update()` can check if the `C_LuaScript`'s `hasScript` changed (via polling), but this is coupling. The explicit `clearTimers()` call from the reload path is cleaner and matches the "invalidate old proxy" pattern already in `executeScript`.

### Anti-Patterns to Avoid

- **Using std::vector or std::list for timer storage:** Heap allocation is forbidden by the project's zero-alloc constraint. Use a fixed-size array with an `active` flag per slot.
- **Storing the Lua function as a Lua global:** Globals are script-global, not per-timer. Two timers registered for different callbacks would stomp each other. Use `luaL_ref` per-callback.
- **Not releasing luaL_ref on cancel:** Every `cancel()` and `clearTimers()` path MUST call `luaL_unref(L, LUA_REGISTRYINDEX, ref)` for each active ref. Forgetting this leaks a reference that prevents Lua GC from collecting the function closure — the exact bug TIMER-05 tests for.
- **Calling `luaL_unref` after `lua_close`:** The destructor `~C_Timer()` must check if `m_L != nullptr` AND the Lua state is still open before calling `luaL_unref`. Since `C_Timer::~C_Timer()` runs before `C_LuaScript::~C_LuaScript()` only if the components are destroyed in the right order — but in `Object`, components are destroyed in construction order. The order dependency: if `C_LuaScript` is added before `C_Timer`, `C_Timer` destructs first (correct). If added after, `C_LuaScript` destructs first, closing the Lua state before `C_Timer::~C_Timer()` can call `luaL_unref` (use-after-free). **Resolution:** `C_Timer` should call `clearTimers()` from its destructor only if `m_L != nullptr`; and `C_LuaScript` should call `timer->clearTimers()` on `~C_LuaScript()` before closing the Lua state. This ensures the refs are released while the state is still alive regardless of destruction order.
- **Not including `timer.hpp` in `bindings.cpp`:** The `"C_Timer"` branch needs `owner->getComponent<C_Timer>()`, which requires the full `C_Timer` declaration.
- **Pushing ScriptProxy by re-executing lookup in a different way than `callWithProxy`:** Use the exact same registry key pattern: `lua_pushlightuserdata(L, script)` + `lua_gettable(L, LUA_REGISTRYINDEX)`. This is the established contract from `callWithProxy` in `lua_script.cpp`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Callback reference anchoring | Custom callback struct with C++ function pointer | `luaL_ref` / `luaL_unref` | `luaL_ref` is the standard Lua C-API mechanism; handles GC rooting, arbitrary Lua closures |
| Timer ID generation | UUID or hash | Monotonically incrementing integer `m_nextId` | Sufficient for uniqueness within one `C_Timer` instance; zero overhead |
| ScriptProxy lookup at callback time | Second `luaL_ref` on the ScriptProxy userdata | Registry lookup via `lua_pushlightuserdata(L, script)` + `lua_gettable` | ScriptProxy is already in the registry at the established key; second ref would be redundant and require extra cleanup |

**Key insight:** Every mechanism needed for C_Timer is already established in the codebase. `luaL_ref` is used in `bindings_engine.cpp`. The ScriptProxy registry key pattern is used in `callWithProxy`. The ComponentProxy metatable pattern is established in Phase 39. C_Timer is assembly of existing patterns.

## Common Pitfalls

### Pitfall 1: Destruction Order — lua_State Outlives C_Timer
**What goes wrong:** If `C_LuaScript` destructs before `C_Timer` (because `C_LuaScript` was added to the Object first and components destruct in reverse construction order via `std::unique_ptr` array teardown), `C_Timer::~C_Timer()` calls `luaL_unref` on a closed Lua state.
**Why it happens:** `Object::components` is a `std::array<std::unique_ptr<Component>, 16>`. Destruction order is array index order (0 → 15). `C_LuaScript` is typically added first, before `C_Timer`. When the Object destructs, `components[0]` (C_LuaScript) is destroyed first, closing its Lua state. Then `components[1]` (C_Timer) destructs and tries to call `luaL_unref` on a dangling `lua_State*`.
**How to avoid:** Two-pronged approach:
  1. `C_LuaScript::~C_LuaScript()` calls `owner->getComponent<C_Timer>()->clearTimers()` BEFORE shutting down its Lua state.
  2. `C_Timer::clearTimers()` sets `m_L = nullptr` after releasing all refs. `C_Timer::~C_Timer()` checks `m_L != nullptr` before any Lua calls.
**Warning signs:** Valgrind/ASAN reports use-after-free in `luaL_unref` during Object destruction.

### Pitfall 2: Timer Fires During Its Own Callback Modifying the Timer Array
**What goes wrong:** A timer callback calls `timer:after()` or `timer:cancel()`. If `C_Timer::update()` iterates the array while it is being mutated by a callback, entries added during iteration may be skipped or entries cancelled mid-loop may be double-freed.
**Why it happens:** `lua_pcall` is called while the update loop is executing. The callback can call Lua methods that reach `scheduleAfter`, `scheduleEvery`, or `cancel`, which directly modify `m_timers`.
**How to avoid:** Two safe strategies:
  1. **Copy-and-iterate:** Snapshot the count before the loop (`int count = activeBefore`), iterate only up to that count, and let new entries added during the loop be processed in the next `update()` call. Cancellations during iteration: mark the entry inactive immediately but don't compact the array mid-loop — compaction happens at the end of `update()` after all firing is done.
  2. **Deferred fire list:** Collect all entries that need to fire into a local stack array, then fire them after the loop. Adds complexity but is slightly cleaner.
  Strategy 1 is recommended as simpler and matching the project style.
**Warning signs:** Timer callbacks added inside another timer callback never fire, or `cancel()` inside a callback crashes.

### Pitfall 3: Not Passing ScriptProxy as First Argument
**What goes wrong:** The callback receives `nil` as its first argument instead of `self`.
**Why it happens:** The timer fires the Lua function without pushing the ScriptProxy. The Lua script does `function myTimer(self) self.x = 10 end` and crashes with "attempt to index a nil value (local 'self')".
**How to avoid:** Always push the ScriptProxy userdata from the registry before `lua_pcall`. The key is `lua_pushlightuserdata(L, owner->getComponent<C_LuaScript>())` followed by `lua_gettable(L, LUA_REGISTRYINDEX)`. If `C_LuaScript` is nil (no script component on the owner), push `lua_pushnil` as fallback, but this should never happen in normal use.
**Warning signs:** Test TIMER-04 fails with "attempt to index nil".

### Pitfall 4: ID Counter Overflow or Reuse
**What goes wrong:** `m_nextId` wraps to 0 or a negative value after many timers are created. A timer with ID 0 or a previously-used ID cannot be uniquely cancelled.
**Why it happens:** `m_nextId` is an `int` incremented monotonically; after 2^31 registrations (practically impossible in a game session) it wraps.
**How to avoid:** For v1.6, this is not a real concern. ID 0 can be reserved as "invalid" (`cancel(0)` is a no-op). Document the limitation. In practice, a game session will never create 2 billion timers.
**Warning signs:** N/A in practice for v1.6.

### Pitfall 5: Timer Callbacks Called After C_Timer Is Destroyed (Proxy Stale)
**What goes wrong:** A `ComponentProxy` to a `C_Timer` is stored in a Lua variable. The Object is destroyed. Later, the Lua script calls `timer:after(1, fn)` on the stale proxy. The `C_Timer*` pointer is dangling.
**Why it happens:** The Lua script holds a proxy past the lifetime of its `C_Timer`. This is the same stale-proxy issue as PROXY-04 in Phase 39.
**How to avoid:** Phase 39's `Component::~Component()` invalidation (setting `m_luaProxy->valid = false`) means `lua_ctimer_proxy_index_impl` will call `luaL_error(L, "component has been destroyed")` when any method is accessed on the stale proxy. No additional work needed — the ComponentProxy pattern handles this.
**Warning signs:** No crash (because the proxy check catches it), but the error message is raised as a Lua error.

### Pitfall 6: Forgetting to clear timers on hot-reload (TIMER-05)
**What goes wrong:** After F5 hot-reload, the previous script's timer callbacks are still firing in `C_Timer::update()`. The old Lua functions are still anchored in the registry (preventing GC), and when they fire they execute in the context of the new script's Lua state — potentially calling functions that no longer exist or have different behaviour.
**Why it happens:** `C_LuaScript::loadScript()` creates a new ScriptProxy and re-executes the script, but does not notify `C_Timer` to clear its entries.
**How to avoid:** Add a `clearTimers()` call inside `C_LuaScript::executeScript()` and `C_LuaScript::loadScriptFile()`, in the same block where the old ScriptProxy is invalidated.
**Warning signs:** After hot-reload, timer callbacks fire unexpectedly; `luaL_ref` leak detected via `engine.lua.memory()` growing across reloads.

## Code Examples

Verified patterns from existing codebase:

### luaL_ref Usage (from bindings_engine.cpp — existing pattern)
```cpp
// Source: src/scripting/bindings_engine.cpp — lua_engine_scene_spawn()
lua_pushvalue(L, 1);                                   // push the string
int ref = luaL_ref(L, LUA_REGISTRYINDEX);              // anchor it
lua_rawgeti(L, LUA_REGISTRYINDEX, ref);                // retrieve later
const char* interned = lua_tostring(L, -1);
lua_pop(L, 1);
// Note: ref is never freed in spawn() (intentional — leaked for string lifetime)
// C_Timer MUST unref: luaL_unref(L, LUA_REGISTRYINDEX, ref)
```

### ScriptProxy Registry Lookup (from lua_script.cpp callWithProxy — canonical key)
```cpp
// Source: src/components/lua_script.cpp — callWithProxy()
lua_pushlightuserdata(L, this);           // key: C_LuaScript* pointer (lightuserdata)
lua_gettable(L, LUA_REGISTRYINDEX);       // pushes ScriptProxy userdata
// stack top is now ScriptProxy userdata — pass as first arg to pcall
```

### ComponentProxy Full Userdata Allocation (from bindings_engine.cpp — Phase 39 pattern)
```cpp
// Source: src/scripting/bindings_engine.cpp — lua_engine_scene_find()
auto* proxy = static_cast<enjin2::ObjectProxy*>(
    lua_newuserdata(L, sizeof(enjin2::ObjectProxy)));
proxy->object = obj;
proxy->valid  = true;
luaL_getmetatable(L, "ObjectProxy");
lua_setmetatable(L, -2);
obj->setLuaProxy(proxy);
// C_Timer_Proxy uses ComponentProxy* with same pattern
```

### C_Timer::update() Tick Loop (design — no existing code yet)
```cpp
// Recommended implementation for src/components/timer.cpp
void C_Timer::update(float dt) {
    if (!m_L) return;

    // Snapshot count to avoid re-processing entries added during callbacks
    const int count = MAX_TIMERS;
    for (int i = 0; i < count; ++i) {
        TimerEntry& e = m_timers[i];
        if (!e.active) continue;

        e.elapsed += dt;
        if (e.elapsed < e.interval) continue;

        // Timer fires — retrieve and call the callback
        int cbRef = e.callbackRef;
        bool repeating = e.repeating;
        float interval = e.interval;

        if (!repeating) {
            // Deactivate BEFORE calling — prevents re-entrant double-fire
            e.active = false;
            e.callbackRef = LUA_NOREF;
        } else {
            // Reset elapsed for next interval — keep active
            e.elapsed = 0.0f;
        }

        // Fire: push fn, push ScriptProxy (self), pcall(1 arg, 0 results)
        lua_rawgeti(m_L, LUA_REGISTRYINDEX, cbRef);
        if (lua_isfunction(m_L, -1)) {
            // Push ScriptProxy as self (TIMER-04)
            C_LuaScript* script = owner->getComponent<C_LuaScript>();
            if (script) {
                lua_pushlightuserdata(m_L, script);
                lua_gettable(m_L, LUA_REGISTRYINDEX);
            } else {
                lua_pushnil(m_L);
            }
            if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(m_L, -1);
                printf("[C_Timer] callback error (slot %d): %s\n", i,
                       err ? err : "unknown");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);  // pop non-function
        }

        // Release ref for one-shot timers (ref was stored before deactivation)
        if (!repeating) {
            luaL_unref(m_L, LUA_REGISTRYINDEX, cbRef);
        }
    }
}
```

### C_Timer::clearTimers() (design)
```cpp
// src/components/timer.cpp
void C_Timer::clearTimers() {
    if (!m_L) return;
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (m_timers[i].active && m_timers[i].callbackRef != LUA_NOREF) {
            luaL_unref(m_L, LUA_REGISTRYINDEX, m_timers[i].callbackRef);
            m_timers[i].callbackRef = LUA_NOREF;
        }
        m_timers[i].active = false;
        m_timers[i].elapsed = 0.0f;
    }
    m_L = nullptr;  // safe sentinel — prevents double-unref in destructor
}
```

### Expected Lua Usage (from requirements)
```lua
-- TIMER-01: one-shot after 2 seconds
local tid = timer:after(2.0, function(self)
    engine.log("2 seconds elapsed!")
end)

-- TIMER-02: repeat every 0.5 seconds
local rid = timer:every(0.5, function(self)
    engine.log("tick!")
end)

-- TIMER-03: cancel before firing
timer:cancel(tid)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Busy-polling in update() — `if elapsed > threshold then ... end` | C_Timer component ticks in its own update() | Phase 40 | Cleaner script code; timer logic separated from game logic |
| No ComponentProxy for timer | C_Timer accessible via `self:get("C_Timer")` | Phase 39 (infrastructure) + Phase 40 (C_Timer entry) | Scripts can access timer without direct reference |
| luaL_ref never used for callbacks in this codebase | luaL_ref per callback in C_Timer | Phase 40 | Establishes pattern for Phase 42 EventBus |

**Note on existing `luaL_ref` in codebase:** `bindings_engine.cpp` uses `luaL_ref` once to anchor an interned string in `lua_engine_scene_spawn()`, but crucially does NOT call `luaL_unref` (the string is intentionally leaked). C_Timer is the first case in this codebase where `luaL_ref` is used for Lua functions that MUST be released. The cleanup discipline is new for this codebase.

## Open Questions

1. **What capacity should MAX_TIMERS be?**
   - What we know: v1.6 target games (Arkanoid, physics sandbox, tamagotchi) are small. 8 timers per object is generous. The constant is per-`C_Timer` instance (per-object), not global.
   - What's unclear: Whether any game design needs more than 8 simultaneous timers on a single object.
   - Recommendation: Default to 8. Make it a `static constexpr int MAX_TIMERS = 8` in the class so it's easy to change. No planner decision required.

2. **Should C_Timer check `Component::isEnabled()` before ticking?**
   - What we know: `Component::update()` is only called when `isEnabled()` is true (per `Object::update()` which checks `components[i]->isEnabled()`). So if `C_Timer` is disabled, `update()` won't be called and timers won't tick.
   - What's unclear: Whether there's a use case for pausing all timers by disabling the component.
   - Recommendation: No additional `isEnabled()` check needed inside `update()` — the standard `Object::update()` loop already handles this. Document it.

3. **Should `clearTimers()` be called by `C_LuaScript::~C_LuaScript()` or `C_LuaScript::executeScript()`?**
   - What we know: Both the destructor and hot-reload paths need timer cleanup. The destructor is needed to handle the destruction-order pitfall (Pitfall 1). Hot-reload cleanup is needed for TIMER-05.
   - Recommendation: Call `clearTimers()` from BOTH:
     - `C_LuaScript::~C_LuaScript()` — call before `scriptSystem->shutdown()` to release refs while state is still open.
     - `C_LuaScript::executeScript()` (and `loadScriptFile()`) — call before executing new script code, same block as old proxy invalidation.
   - This is a definite architectural decision the planner should include as a PLAN task.

4. **Should the C_Timer_Proxy metatable be registered in bindings.cpp (alongside C_Position_Proxy) or in a separate bindings_timer.cpp?**
   - What we know: The project already splits `LuaBindings` implementation across multiple `.cpp` files (`bindings.cpp`, `bindings_engine.cpp`, `bindings_draw.cpp`, etc.).
   - Recommendation: Since C_Timer proxy registration is small (one metatable, three static functions), add it to `bindings.cpp` alongside `C_Position_Proxy`. A separate file (`bindings_timer.cpp`) is not warranted until there are multiple component proxy types needing their own files.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Hand-rolled (ASSERT macro + pass/failure counters) — matches all existing tests |
| Config file | none — vanilla ctest |
| Quick run command | `ctest -R timer_test --output-on-failure` (from build dir) |
| Full suite command | `ctest --output-on-failure` (from build dir) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| TIMER-01 | `timer:after(2.0, fn)` fires once after 2 seconds of simulated dt | unit (C++ + Lua) | `ctest -R timer_test` | No — Wave 0 |
| TIMER-02 | `timer:every(0.5, fn)` fires repeatedly at 0.5s intervals | unit (C++ + Lua) | `ctest -R timer_test` | No — Wave 0 |
| TIMER-03 | `timer:cancel(id)` prevents callback from firing | unit (C++ + Lua) | `ctest -R timer_test` | No — Wave 0 |
| TIMER-04 | Callback receives `self` (ScriptProxy) as first argument | unit (Lua) | `ctest -R timer_test` | No — Wave 0 |
| TIMER-05 | All `luaL_ref` handles are released on destruction and hot-reload | unit (C++) | `ctest -R timer_test` | No — Wave 0 |

**Testing approach for TIMER-01 / TIMER-02:** Create an Object, add `C_LuaScript` and `C_Timer`. Load a script that calls `timer:after(1.0, fn)` in `init()`. Simulate time by calling `script->update(0.5f)` twice (totaling 1 second). The test fixture must provide a mechanism to observe whether the callback fired. Approach: the callback sets a Lua global (`fired = true`); after the updates, check `script->getScriptBool("fired")`.

**Testing approach for TIMER-05:** After loading a script with timers, check `engine.lua.memory()` baseline, hot-reload (call `loadScript()` again), then check memory again. Memory should return to baseline (within GC tolerance). Alternatively: C++-level check — inspect that `m_timers[i].callbackRef == LUA_NOREF` for all slots after `clearTimers()`.

### Wave 0 Gaps
- [ ] `include/enjin2/components/timer.hpp` — C_Timer class declaration
- [ ] `src/components/timer.cpp` — C_Timer update(), clearTimers(), destructor
- [ ] `tests/timer_test.cpp` — covers TIMER-01, TIMER-02, TIMER-03, TIMER-04, TIMER-05
- [ ] `tests/CMakeLists.txt` — `timer_test` entry under `ENJIN2_BUILD_LUA` guard
- [ ] `src/scripting/bindings.cpp` — `C_Timer_Proxy` metatable + `"C_Timer"` branch in `lua_proxy_get_component_impl`
- [ ] `include/enjin2/scripting/bindings.hpp` — forward declare `C_Timer` or include `timer.hpp`
- [ ] `src/components/CMakeLists.txt` (or root CMake) — register `timer.cpp` in the build

## Sources

### Primary (HIGH confidence)
- Codebase: `src/components/lua_script.cpp` — `callWithProxy()` (ScriptProxy registry key pattern, `lua_pcall` with proxy as first arg), `~C_LuaScript()` (proxy invalidation before lua_close), `executeScript()` (proxy invalidation on reload)
- Codebase: `src/scripting/bindings_engine.cpp` — `lua_engine_scene_spawn()` (only existing `luaL_ref` usage in codebase), `lua_engine_scene_find()` (ComponentProxy allocation pattern)
- Codebase: `include/enjin2/core/object.hpp` — `Object::components` array and destruction order
- Codebase: `include/enjin2/core/component.hpp` — Component lifecycle methods, `m_luaProxy` (Phase 39 addition)
- Codebase: `include/enjin2/core/object_collection.hpp` — `update()` loop (no snapshot count — STATE.md OPEN item about unsafe loop; C_Timer callbacks that add/remove objects must be careful)
- Codebase: `.planning/STATE.md` — destruction order concern, single-proxy-per-component constraint, ObjectCollection::update() loop snapshot concern
- Codebase: `.planning/phases/39-componentproxy/39-RESEARCH.md` — ComponentProxy infrastructure Phase 40 must hook into
- Codebase: `.planning/phases/39-componentproxy/39-01-PLAN.md` — exact `lua_proxy_get_component_impl` interface that Phase 40 adds `"C_Timer"` entry to

### Secondary (MEDIUM confidence)
- `.planning/ROADMAP.md` Phase 40 description — success criteria for Lua API surface
- `.planning/REQUIREMENTS.md` TIMER-01..TIMER-05 — requirement definitions

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all technology already in use; no external dependencies; patterns verified in codebase
- Architecture: HIGH — all patterns (luaL_ref, ScriptProxy registry key, ComponentProxy metatable) are established in existing code; C_Timer is assembly, not invention
- Pitfalls: HIGH — destruction order pitfall is verified by reading Object's component array teardown order; re-entrancy pitfall is a well-known Lua C-API issue; all pitfalls verified against actual codebase structure

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (stable C++ codebase, 30-day window appropriate)
