# Phase 42: EventBus - Research

**Researched:** 2026-02-28
**Domain:** Lua pub/sub event bus, luaL_ref lifecycle, scene-scoped resource cleanup, engine.* table extension
**Confidence:** HIGH

## Summary

Phase 42 adds a scene-scoped publish/subscribe event bus exposed as `engine.event.on(name, fn)` and `engine.event.emit(name)`. Lua scripts on different objects communicate via named string events without holding direct object references. This is the final feature in the v1.6 Game Ready milestone.

The EventBus is structurally a global-within-a-scene registry: a fixed-capacity array of (event name, subscriber list) pairs. It is not a `Component` — it is a scene-level service, analogous to `SceneStateMachine` and `activeScene`. The instance is owned by a C++ class (`LuaEventBus`) held in `LuaBindings` (or optionally on the `Scene` itself) and exposed via a pointer-to-pointer registry entry `"enjin_event_bus"` in the Lua registry, following the exact same injection pattern as `enjin_ssm`, `enjin_active_scene`, and `enjin_time`.

Each subscriber stores its callback as a `luaL_ref` in the Lua registry and is identified by an integer subscription ID returned to Lua. `engine.event.on()` returns the ID; `engine.event.off(id)` unregisters it before the scene ends (EVENT-03). All handlers are bulk-cleared on scene deactivation (EVENT-04) and on hot-reload (EVENT-05).

There are no external dependencies. The entire implementation uses patterns already established in this codebase: `luaL_ref` / `luaL_unref` for callback anchoring (established by C_Timer in Phase 40), the pointer-to-pointer `"enjin_*"` registry pattern for injection, the `registerEngineTable()` extension point for adding `engine.event`, and the fixed-capacity static array pattern from `LuaBindings::spritePool` and C_Timer's `TimerEntry` array.

**Primary recommendation:** Implement `LuaEventBus` as a standalone C++ struct/class with fixed-capacity arrays (zero heap allocation), store it as a member of `LuaBindings`, inject its address via `"enjin_event_bus"` in the Lua registry during `registerAll()`, add `engine.event` sub-table in `registerEngineTable()`, and clear all refs from two call sites: `LuaBindings::registerAll()` (hot-reload path) and via a `clearHandlers()` call from the Scene deactivation hook.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| EVENT-01 | Lua scripts can register event handlers via `engine.event.on(name, callback)` | `LuaEventBus::subscribe(name, callbackRef)` stores `luaL_ref` in a fixed subscriber array; returns integer subscription ID |
| EVENT-02 | Lua scripts can emit events via `engine.event.emit(name)` | `LuaEventBus::emit(name)` iterates subscribers for the matching name, calls `lua_rawgeti` + `lua_pcall` for each active callback |
| EVENT-03 | Handlers can be manually unregistered | `engine.event.off(id)` calls `luaL_unref` + marks slot inactive; ID returned by `engine.event.on()` |
| EVENT-04 | Event bus is scene-scoped — all handlers cleared on scene deactivation | `LuaEventBus::clearHandlers()` called from scene deactivation hook; `onDeactivateSignal` wired from `LuaBindings::setActiveScene()` or equivalent |
| EVENT-05 | Handler `luaL_ref` handles cleaned up properly (no leaks across hot-reload) | `LuaBindings::registerAll()` calls `m_eventBus.clearHandlers()` before re-registering (mirrors timer's clearTimers() on reload) |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua (LuaJIT) | Bundled in `/luajit/` | `luaL_ref` / `luaL_unref` for callback anchoring; `lua_rawgeti` + `lua_pcall` for dispatch | Already the scripting runtime; `luaL_ref` pattern established by C_Timer |
| C++ fixed-size arrays | C++11 (`std::array` or raw array) | Zero-allocation subscriber storage | Project requires no heap allocation; matches `spritePool[16]`, `TimerEntry[8]`, `fontRegistry[8]` |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Lua registry string-key storage | Lua 5.1 | `lua_setfield(L, LUA_REGISTRYINDEX, "enjin_event_bus")` | Same pointer-to-pointer injection as `enjin_ssm`, `enjin_active_scene`, `enjin_time` |
| `Scene::onDeactivateSignal` (existing) | Internal | Trigger `clearHandlers()` on scene exit | Scene already exposes `onDeactivateSignal` — EventBus wires into it |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `LuaEventBus` member of `LuaBindings` | `LuaEventBus` member of `Scene` | Scene-owned gives tighter scoping but requires Scene to include Lua headers; `LuaBindings` already holds all Lua-side state (ssm, activeScene, timeState, store, spritePool); keeping EventBus there is consistent |
| `Scene::onDeactivateSignal` hook | Scene override / SSM hook | Signal approach is zero-coupling: `LuaBindings::setActiveScene()` can `connect()` a lambda that calls `m_eventBus.clearHandlers()`; no Scene subclassing needed |
| Fixed-capacity subscriber per event | Flat global subscriber list iterated by name | Per-event capacity is more efficient for emit (no name scan per element); flat list is simpler but slower for emit with many registered events |

**Installation:** No new dependencies. All required infrastructure already present.

## Architecture Patterns

### Recommended Project Structure

New files for Phase 42:
```
include/enjin2/scripting/lua_event_bus.hpp   # LuaEventBus class declaration
tests/eventbus_test.cpp                       # EVENT-01..EVENT-05 test suite
```

Modified files:
```
include/enjin2/scripting/bindings.hpp         # Add m_eventBus member; add setActiveScene() hook; declare registerEventTable()
src/scripting/bindings.cpp                    # Call m_eventBus.clearHandlers() in registerAll(); wire deactivation hook in setActiveScene()
src/scripting/bindings_engine.cpp             # Add engine.event sub-table in registerEngineTable()
tests/CMakeLists.txt                          # Register eventbus_test under ENJIN2_BUILD_LUA guard
```

### Pattern 1: LuaEventBus Fixed-Capacity Struct

**What:** A standalone header (`lua_event_bus.hpp`) containing `LuaEventBus` — a zero-heap-allocation struct with fixed-capacity arrays for named event channels and their subscribers. Placed in a standalone header to keep `bindings.hpp` clean of implementation detail.

**When to use:** Always — this is the only allocation strategy that satisfies the zero-alloc constraint.

**Example:**
```cpp
// include/enjin2/scripting/lua_event_bus.hpp
#pragma once
#include "lua_platform.hpp"
#include <cstring>

namespace enjin2 {

/**
 * @brief Scene-scoped Lua event bus for named publish/subscribe.
 *
 * Zero heap allocation. Fixed capacity for channels and subscribers.
 * All luaL_ref handles are released on clearHandlers() — called on
 * scene deactivation (EVENT-04) and hot-reload (EVENT-05).
 */
class LuaEventBus {
public:
    static constexpr int MAX_CHANNELS    = 16;  ///< Max distinct event names
    static constexpr int MAX_SUBS_PER_CH = 8;   ///< Max subscribers per event
    static constexpr int MAX_NAME_LEN    = 64;  ///< Max event name length

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

    /// Register a callback for an event. Returns subscription ID (>0) or 0 on failure.
    int subscribe(const char* name, int callbackRef);

    /// Fire all active callbacks for the named event.
    void emit(const char* name);

    /// Unregister a subscription by ID.
    void unsubscribe(int id);

    /// Release all luaL_ref handles and reset all slots.
    /// Called on scene deactivation and hot-reload.
    void clearHandlers();

private:
    Channel* findChannel(const char* name);
    Channel* findOrCreateChannel(const char* name);
};

} // namespace enjin2
```

### Pattern 2: Pointer-to-Pointer Registry Injection (matches existing engine.* pattern exactly)

**What:** `LuaBindings::registerAll()` stores `&m_eventBus` in the Lua registry under the string key `"enjin_event_bus"`. The `engine.event.on` / `engine.event.emit` / `engine.event.off` C functions retrieve this pointer via `lua_getfield(L, LUA_REGISTRYINDEX, "enjin_event_bus")`. This is byte-for-byte the same pattern as `enjin_ssm`, `enjin_active_scene`, and `enjin_time`.

**When to use:** Always — this is the established injection pattern for all engine.* services in this codebase.

**Example:**
```cpp
// In LuaBindings::registerAll() — addition to bindings.cpp:

// Release any event handlers from a previous load (EVENT-05: hot-reload cleanup)
m_eventBus.clearHandlers();
m_eventBus.setLuaState(L);

// Store event bus pointer in registry for engine.event.* closures
lua_pushlightuserdata(L, &m_eventBus);
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_event_bus");
```

And in each `engine.event.*` binding:
```cpp
static int lua_engine_event_on(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_event_bus");
    auto* bus = static_cast<LuaEventBus*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!bus) { lua_pushinteger(L, 0); return 1; }

    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    int id = bus->subscribe(name, ref);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}
```

### Pattern 3: engine.event Sub-Table in registerEngineTable()

**What:** Add an `engine.event` sub-table to `registerEngineTable()` in `bindings_engine.cpp`, following the exact same pattern as the existing `engine.scene`, `engine.input`, `engine.time`, `engine.lua`, `engine.random`, `engine.store`, and `engine.sprite` sub-tables. The `engine` global is built with `lua_newtable`, sub-tables are populated with `lua_newtable` + `luaBindFunctions`, then assigned with `lua_setfield`.

**When to use:** Always — this is the single place where all `engine.*` sub-tables are registered.

**Example:**
```cpp
// In LuaBindings::registerEngineTable() — bindings_engine.cpp addition:

static const LuaFuncDef kEventFuncs[] = {
    {"on",   lua_engine_event_on},
    {"off",  lua_engine_event_off},
    {"emit", lua_engine_event_emit},
};
lua_newtable(L);
luaBindFunctions(L, -1, kEventFuncs, ENJIN_ARRAY_LEN(kEventFuncs));
lua_setfield(L, -2, "event");

// Declare static functions in bindings.hpp (or bindings_engine.cpp private section):
static int lua_engine_event_on(lua_State* L);
static int lua_engine_event_off(lua_State* L);
static int lua_engine_event_emit(lua_State* L);
```

### Pattern 4: Scene Deactivation Hook for EVENT-04

**What:** `LuaBindings::setActiveScene(Scene*)` is called by the host to wire the active scene pointer. This is the correct place to also wire the `onDeactivateSignal` so `m_eventBus.clearHandlers()` is called when the scene deactivates. The signal connection lambda captures `this` (the `LuaBindings` instance).

**Critical detail:** `Scene::onDeactivateSignal` is private. The connection must be made via `scene->connectOnDeactivate(...)`. The `SignalConnection` returned must be stored to keep the connection alive. One option: store the connection as a member of `LuaBindings`. However, `SignalConnection<Scene*>` holds a `std::function`, which uses heap allocation unless the callable is trivially small.

**Alternative approach (heap-free):** Rather than using `Signal`, call `m_eventBus.clearHandlers()` from `C_LuaScript`'s update path by detecting scene deactivation, OR call it from `LuaBindings::setActiveScene(nullptr)` — when `setActiveScene` is called with `nullptr` it means the scene is being deactivated. This avoids `std::function` heap allocation entirely.

**Recommended approach:** Override the `setActiveScene()` call flow: add a `void notifySceneDeactivating()` method to `LuaBindings` that calls `m_eventBus.clearHandlers()`, and call it from the SDL runner / SSM before `setActiveScene(nullptr)`. This is the zero-heap-allocation path.

**Simpler alternative:** Call `m_eventBus.clearHandlers()` at the start of `registerAll()` (already planned for EVENT-05 / hot-reload). This also covers EVENT-04 if the scene transition always goes through a reload: but it does NOT cover the case where the scene deactivates WITHOUT a hot-reload (i.e., normal `engine.scene.switch()`). Therefore a dedicated deactivation notification is required.

**Concrete recommendation:** Add a `void onSceneDeactivated()` method to `LuaBindings`. Call it from `SceneStateMachine::applyDeferredTransition()` on the old scene (or from `Scene::deactivate()` via a hook). The lowest-coupling option: add a `LuaBindings*` back-pointer to `Scene` that is called in `deactivate()` — but this pollutes `Scene` with a Lua dependency. Better: call `LuaBindings::onSceneDeactivated()` from the SDL runner's scene transition path and from the SSM's `applyDeferredTransition` where `currentScene->deactivate()` is called.

**Simplest correct approach:** Inject the event bus clear into `LuaBindings::setActiveScene(scene)`. When this is called with a new scene, it means the old scene has deactivated. Add the `clearHandlers()` call there — it's the moment scene context switches, which is exactly when EVENT-04 requires cleanup.

```cpp
// In LuaBindings::setActiveScene() — addition:
void LuaBindings::setActiveScene(Scene* scene) {
    if (scene != m_activeScene) {
        // Scene is changing — clear event bus for the outgoing scene
        m_eventBus.clearHandlers();
    }
    m_activeScene = scene;
}
```

**Warning:** `setActiveScene()` is currently a simple one-liner inline in `bindings.hpp`. Moving it to a `.cpp` file is required for this change.

### Pattern 5: Hot-Reload Cleanup (EVENT-05)

**What:** `LuaBindings::registerAll()` is called on every hot-reload (F5). Since `m_eventBus.clearHandlers()` is called at the top of `registerAll()` (before re-registering the engine table), all `luaL_ref` handles from the old script load are released before the new Lua state begins. This is identical to how C_Timer's `clearTimers()` is called from the reload path.

**Example:**
```cpp
// In LuaBindings::registerAll() — existing structure:
void LuaBindings::registerAll() {
    if (!engine || !engine->isInitialized()) { return; }

    resetSpritePool();  // existing
    currentColor = 15;  // existing
    lineWidth = 1;      // existing

    // EVENT-05: clear event bus handlers from previous load
    m_eventBus.clearHandlers();
    m_eventBus.setLuaState(engine->getState());

    // Store pointers in registry (existing pattern + new event bus):
    lua_State* L = engine->getState();
    // ... existing registry stores ...
    lua_pushlightuserdata(L, &m_eventBus);
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_event_bus");

    // ... rest of registerAll() unchanged ...
}
```

### Pattern 6: LuaEventBus::emit() Implementation

**What:** Iterating the subscriber list and calling each active callback via `lua_rawgeti` + `lua_pcall`. The event bus does NOT pass any payload (out of scope per REQUIREMENTS.md). No arguments beyond the event name itself are passed to the callback — the callback signature is `function()` (no self argument, because EventBus is not component-scoped).

**Key design decision:** Should `emit()` pass `self` (ScriptProxy) to the handler? Per the requirements and the out-of-scope note ("Event bus data payload — Adds complexity; basic on/emit sufficient for v1.6 target games"), no payload is passed. The callback receives zero arguments. Scripts that need context capture it via closure.

**Re-entrancy concern:** If a handler registered via `engine.event.on()` calls `engine.event.emit()` inside its callback, `emit()` is called re-entrantly while the outer `emit()` is still iterating its subscriber list. Safe approach: snapshot the active subscriber list before iterating (copy refs to a local stack array), then call all callbacks after the snapshot. This prevents re-entrant modification of the channel's subscriber array.

**Example:**
```cpp
// src/scripting/lua_event_bus.cpp
void LuaEventBus::emit(const char* name) {
    if (!m_L) return;
    Channel* ch = findChannel(name);
    if (!ch) return;

    // Snapshot active callback refs before any pcall (prevents re-entrant mutation)
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
```

### Anti-Patterns to Avoid

- **Storing the LuaEventBus on Scene directly:** Scene is in `include/enjin2/core/scene.hpp` with no Lua dependency. Adding a `LuaEventBus` member would require `scene.hpp` to include `lua_event_bus.hpp`, which includes `lua_platform.hpp` (Lua headers). This would make the core `Scene` class Lua-dependent, breaking the core/scripting separation. The EventBus belongs in `LuaBindings`, not `Scene`.
- **Using `std::map` or `std::unordered_map` for event channels:** These heap-allocate. Use the fixed-array approach with linear search. With `MAX_CHANNELS = 16` and `MAX_SUBS_PER_CH = 8`, the worst-case scan is O(16) — negligible for game frame rates.
- **Not calling `clearHandlers()` before re-registering the event table:** If `registerAll()` rebuilds the `engine.event` Lua table without releasing existing `luaL_ref` handles, the old handlers are orphaned in the Lua registry and can never be GC'd. This is the EVENT-05 leak scenario.
- **Not snapshotting refs before `emit()` iteration:** Re-entrant `emit()` calls (a handler calls `engine.event.emit()` or `engine.event.off()`) can corrupt the subscriber array while it is being iterated. Always copy refs to a local stack before calling any `lua_pcall`.
- **Passing `self` (ScriptProxy) as a callback argument:** The EventBus is not component-scoped. Handlers should capture context via Lua closures, not via an injected `self` parameter. Passing `self` would require each handler to be aware of which script registered it, complicating the emission path significantly.
- **Forgetting to call `setLuaState(L)` after `clearHandlers()`:** `clearHandlers()` sets `m_L = nullptr` (sentinel pattern identical to C_Timer). If `registerAll()` calls `clearHandlers()` and then fails to call `setLuaState(L)`, subsequent `subscribe()` calls will fail silently (the bus has no Lua state).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Callback reference anchoring | C++ function pointer table | `luaL_ref` / `luaL_unref` | Standard Lua C-API mechanism; handles GC rooting, arbitrary closures; established in C_Timer |
| Event name → subscriber dispatch | `std::map<string, vector>` | Fixed-capacity array with linear scan by name | `std::map` and `std::vector` heap-allocate; linear scan over 16 channels is O(16) — fast enough |
| Scene deactivation detection | Custom lifecycle observer framework | Piggyback on `LuaBindings::setActiveScene(scene)` call | Already the established injection point; zero new infrastructure required |

**Key insight:** The entire EventBus implementation is the `luaL_ref` pattern (from C_Timer) applied to a simple named-channel struct, injected via the pointer-to-pointer registry (from the existing `engine.*` infrastructure), and exposed via `registerEngineTable()` (existing). There is no new pattern invented in this phase.

## Common Pitfalls

### Pitfall 1: Scene Deactivation Coverage Gap (EVENT-04)
**What goes wrong:** `clearHandlers()` is only called in `registerAll()` (hot-reload path). A normal scene switch via `engine.scene.switch()` deactivates the scene without going through `registerAll()`. Handlers from the old scene remain registered and fire when `engine.event.emit()` is called from the new scene.
**Why it happens:** `registerAll()` is called on initial script load and on F5 hot-reload. Normal scene transitions do NOT call `registerAll()` — they call `setActiveScene(newScene)` directly.
**How to avoid:** Call `m_eventBus.clearHandlers()` when the scene changes. The cleanest hook is inside `LuaBindings::setActiveScene(scene)` — detect that the scene pointer is changing and clear before updating `m_activeScene`. This requires `setActiveScene()` to become a non-trivial method (defined in a `.cpp` file, not inline).
**Warning signs:** After a `engine.scene.switch()`, event handlers from the old scene fire in the new scene's context.

### Pitfall 2: Re-Entrancy in emit() (handler calls emit() or off())
**What goes wrong:** A handler registered via `engine.event.on("foo", fn)` calls `engine.event.emit("foo")` inside `fn`. The outer `emit()` is iterating the subscriber list while the inner `emit()` also iterates it — no corruption per se, but handler `off()` calls from inside a callback could set `active = false` on a slot the outer loop has already read or is about to read, causing double-invocation or skipped invocation.
**Why it happens:** `lua_pcall` executes Lua code synchronously in the same call stack. The Lua function can call any Lua API, including `engine.event.emit()` and `engine.event.off()`.
**How to avoid:** Snapshot the callback refs to a local stack array before the pcall loop. The snapshot is taken before any `lua_pcall`, so re-entrant `off()` calls change the channel's subscriber array but not the already-snapshotted local array. New `on()` calls from inside a handler are not included in the snapshot — they fire starting from the next `emit()`.
**Warning signs:** `engine.event.off(id)` inside a handler causes subsequent handlers in the same `emit()` to be skipped or double-invoked.

### Pitfall 3: setLuaState() Not Called After clearHandlers()
**What goes wrong:** `clearHandlers()` sets `m_L = nullptr` as a safe sentinel (same as C_Timer's `clearTimers()`). If `registerAll()` calls `clearHandlers()` but then fails to call `setLuaState(L)`, all subsequent `subscribe()` calls silently return 0 (fail) and all `emit()` calls are no-ops.
**Why it happens:** The `clearHandlers()` + `setLuaState()` pair must always be called together in `registerAll()`. Missing the second call is an easy oversight.
**How to avoid:** In `registerAll()`, always call `m_eventBus.setLuaState(engine->getState())` immediately after `m_eventBus.clearHandlers()`. Better: combine them into a single `reset(lua_State* L)` method that clears and sets the state atomically.
**Warning signs:** `engine.event.on()` returns 0 (invalid ID); all `emit()` calls are silent.

### Pitfall 4: lua_State Lifetime — clearHandlers() Called After Lua State Is Closed
**What goes wrong:** If `LuaBindings` is destroyed before the Lua state is closed, `clearHandlers()` in the destructor (if added) would call `luaL_unref` on a closed `lua_State*`.
**Why it happens:** This is the same destruction-order pitfall as C_Timer (see Phase 40 Research, Pitfall 1). `LuaBindings` is owned by `LuaScriptSystem`, which is owned by `C_LuaScript`. `C_LuaScript::~C_LuaScript()` calls `scriptSystem->shutdown()` (which calls `lua_close`) before `LuaBindings` is destroyed. The order within `LuaScriptSystem` destructor determines safety.
**How to avoid:** Call `m_eventBus.clearHandlers()` from `C_LuaScript::~C_LuaScript()` before `scriptSystem->shutdown()` — same pattern as clearing timers before closing the Lua state. Alternatively, `LuaScriptSystem::shutdown()` can call `getBindings().clearEventBus()` before `lua_close`. The key invariant: `luaL_unref` must be called while `m_L` is still valid.
**Warning signs:** Valgrind/ASAN reports use-after-free in `luaL_unref` during C_LuaScript destruction.

### Pitfall 5: MAX_CHANNELS or MAX_SUBS_PER_CH Too Small
**What goes wrong:** A game registers more than `MAX_CHANNELS` distinct event names, or more than `MAX_SUBS_PER_CH` handlers for one event. `subscribe()` returns 0 (failure). The Lua script receives ID 0, which is the "invalid" sentinel. Subsequent `off(0)` is a no-op. The handler never fires.
**Why it happens:** The fixed capacity is a trade-off against zero-alloc. For v1.6 target games (Arkanoid, physics sandbox, tamagotchi), 16 channels and 8 subscribers per channel should be more than sufficient. However, a game author registering many events could silently overflow.
**How to avoid:** Log a warning (via `printf`) when `subscribe()` fails due to capacity. Return 0 and let Lua scripts check the returned ID (`if id == 0 then engine.log("event bus full") end`). Document the capacities. Make them `static constexpr` so they're easy to increase.
**Warning signs:** `engine.event.on()` returns 0; subsequent `emit()` does not invoke the callback.

### Pitfall 6: Event Name Comparison — Exact String Match Required
**What goes wrong:** Lua scripts use `engine.event.on("brick_hit", ...)` and `engine.event.emit("brick_Hit", ...)` (capital H). The event does not fire because the name comparison is case-sensitive `strcmp`.
**Why it happens:** `strcmp` is case-sensitive. Lua string comparison is also case-sensitive, so this is consistent — but it's a common source of script authoring bugs.
**How to avoid:** Use `strcmp` consistently (case-sensitive). Document that event names are case-sensitive in comments. No workaround needed — this is correct behavior. Games should use consistent naming conventions (lowercase with underscores).
**Warning signs:** `emit("brick_hit")` does not trigger handlers registered with `on("Brick_Hit", ...)`.

## Code Examples

Verified patterns from the existing codebase that EventBus directly reuses:

### Pointer-to-Pointer Registry Store/Retrieve (from bindings.cpp / bindings_engine.cpp)
```cpp
// Source: src/scripting/bindings.cpp — registerAll()
lua_pushlightuserdata(L, &m_ssm);          // store address-of-member
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_ssm");

// Source: src/scripting/bindings_engine.cpp — lua_engine_scene_switch()
lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
auto** ssmPP = static_cast<SceneStateMachine**>(lua_touserdata(L, -1));
lua_pop(L, 1);
if (ssmPP == nullptr || *ssmPP == nullptr) { return 0; }
// EventBus uses same pattern with "enjin_event_bus"
```

### engine.* Sub-Table Registration (from bindings_engine.cpp — registerEngineTable())
```cpp
// Source: src/scripting/bindings_engine.cpp — registerEngineTable()
static const LuaFuncDef kRandomFuncs[] = {
    {"seed",    lua_engine_random_seed},
    {"integer", lua_engine_random_integer},
    {"float",   lua_engine_random_float},
};
lua_newtable(L);
luaBindFunctions(L, -1, kRandomFuncs, ENJIN_ARRAY_LEN(kRandomFuncs));
lua_setfield(L, -2, "random");
// engine.event follows this pattern exactly
```

### luaL_ref / luaL_unref Callback Anchoring (from bindings_engine.cpp)
```cpp
// Source: src/scripting/bindings_engine.cpp — lua_engine_scene_spawn()
lua_pushvalue(L, 1);
int ref = luaL_ref(L, LUA_REGISTRYINDEX);  // anchor string
// EventBus uses identical pattern to anchor function callbacks
// Unlike spawn (which intentionally leaks), EventBus MUST unref:
// luaL_unref(L, LUA_REGISTRYINDEX, ref);
```

### Expected Lua Usage (from phase requirements)
```lua
-- EVENT-01: register a handler for "brick_hit"
local id = engine.event.on("brick_hit", function()
    engine.log("brick was hit!")
    bricks_remaining = bricks_remaining - 1
end)

-- EVENT-02: emit the event from another script
engine.event.emit("brick_hit")

-- EVENT-03: unregister before scene ends
engine.event.off(id)

-- Closure-based context capture (no self argument needed)
local score = 0
engine.event.on("score_add", function()
    score = score + 10
end)
```

### LuaEventBus::subscribe() Implementation (design)
```cpp
// src/scripting/lua_event_bus.cpp
int LuaEventBus::subscribe(const char* name, int callbackRef) {
    if (!m_L || !name) { return 0; }
    Channel* ch = findOrCreateChannel(name);
    if (!ch) {
        printf("[EventBus] channel capacity exceeded for '%s'\n", name);
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
    return 0;
}
```

### LuaEventBus::clearHandlers() Implementation (design)
```cpp
// src/scripting/lua_event_bus.cpp
void LuaEventBus::clearHandlers() {
    if (!m_L) return;
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
    m_L = nullptr;  // safe sentinel — prevents double-unref in destructor
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Direct object references between scripts | Named event bus (no direct references) | Phase 42 | Scripts decouple from each other; objects can be destroyed without breaking event wires |
| No scene-scoped event lifecycle | EVENT-04: all handlers cleared on scene deactivation | Phase 42 | No cross-scene stale callbacks; each scene starts with a clean event registry |
| `luaL_ref` cleanup only in component destructors | EventBus adds bulk `clearHandlers()` at scene boundary | Phase 42 | Establishes the pattern for any future Lua-side shared-state with scene-scoped lifetime |

**Deprecated/outdated:**
- Polling-based inter-object communication (checking another object's state in `update()`): EventBus replaces this for decoupled notification patterns.

## Open Questions

1. **Where exactly should `clearHandlers()` be called for EVENT-04 (scene deactivation)?**
   - What we know: `LuaBindings::setActiveScene(scene)` is called by the host when the active scene changes. The signal-based approach (`connectOnDeactivate`) requires `std::function` which may heap-allocate. The `setActiveScene()` approach is heap-free but requires turning the inline into a non-trivial method.
   - What's unclear: Whether the `setActiveScene()` call reliably precedes the old scene's deactivation in all code paths (SDL runner + SSM deferred transition).
   - Recommendation: Call `m_eventBus.clearHandlers()` inside `LuaBindings::setActiveScene(Scene* scene)` when `scene != m_activeScene`. Verify in the SDL runner and SSM that `setActiveScene(newScene)` is called on every scene switch. This is the zero-heap-allocation path.

2. **Should `engine.event.emit(name, ...)` accept variadic arguments for future payload support?**
   - What we know: REQUIREMENTS.md explicitly marks "Event bus data payload" as Out of Scope for v1.6: "Adds complexity; basic on/emit sufficient for v1.6 target games."
   - Recommendation: Implement `engine.event.emit(name)` with exactly one argument (the name string). Do NOT implement payload passing. Scripts capture context via closures. Document that payload is deferred to v1.7.

3. **Should `LuaEventBus` be a separate `.hpp`/`.cpp` pair or folded into `bindings.hpp`/`bindings.cpp`?**
   - What we know: All event bus logic is accessed only from `LuaBindings`. The struct is moderately sized (~50 lines of implementation).
   - Recommendation: Use a separate `lua_event_bus.hpp` + `src/scripting/lua_event_bus.cpp`. This mirrors the pattern: `object_proxy.hpp` is standalone, `component_proxy.hpp` is standalone. Keeping EventBus separate makes `bindings.hpp` cleaner and makes `lua_event_bus.hpp` independently testable.

4. **Should `engine.event.on()` return a subscription ID as an integer, or a Lua table with an `off()` method?**
   - What we know: The requirements say "handlers can be manually unregistered" (EVENT-03). The success criteria say "unregistered by subscription ID." An integer ID is the simplest approach and matches `C_Timer`'s return-ID pattern.
   - Recommendation: Return an integer ID. Call `engine.event.off(id)` to unregister. This is consistent with the C_Timer cancel(id) pattern from Phase 40.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Hand-rolled (ASSERT macro + pass/failure counters) — matches all existing tests |
| Config file | none — vanilla ctest |
| Quick run command | `ctest -R eventbus_test --output-on-failure` (from build dir) |
| Full suite command | `ctest --output-on-failure` (from build dir) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| EVENT-01 | `engine.event.on("foo", fn)` returns non-zero ID; fn is invoked when "foo" is emitted | unit (C++ + Lua) | `ctest -R eventbus_test` | No — Wave 0 |
| EVENT-02 | `engine.event.emit("foo")` invokes all registered handlers for "foo" | unit (C++ + Lua) | `ctest -R eventbus_test` | No — Wave 0 |
| EVENT-03 | `engine.event.off(id)` prevents callback from firing on next emit | unit (C++ + Lua) | `ctest -R eventbus_test` | No — Wave 0 |
| EVENT-04 | Scene switch clears all handlers; emit after switch does not fire old handlers | unit (C++ + Lua) | `ctest -R eventbus_test` | No — Wave 0 |
| EVENT-05 | All `luaL_ref` handles released after hot-reload; no memory growth | unit (C++) | `ctest -R eventbus_test` | No — Wave 0 |

**Testing approach for EVENT-01 / EVENT-02:** Create a `C_LuaScript`, load a script that calls `engine.event.on("foo", function() fired = true end)` in `init()`. Then load a second script (or use the same script) that calls `engine.event.emit("foo")`. Check `script->getScriptBool("fired")` is true after the emit.

**Testing approach for EVENT-04:** Create a `C_LuaScript`, load a script with an `engine.event.on(...)` handler. Call `LuaBindings::setActiveScene(nullptr)` (or `setActiveScene(differentScene)`) to simulate scene deactivation. Then emit the event and verify the handler does NOT fire (Lua global `fired` remains false).

**Testing approach for EVENT-05:** Load a script with `engine.event.on(...)` handlers. Check `engine.lua.memory()` baseline. Hot-reload the script via `loadScript()`. Check memory returns to baseline (within GC tolerance). C++-level: verify all `callbackRef` slots are `LUA_NOREF` after reload via access to `m_eventBus` internals.

**Cross-object test for EVENT-01/02 (integration):** Create two `C_LuaScript` components on different `Object`s in the same `Scene`. Object A's script registers `engine.event.on("score", fn)`. Object B's script calls `engine.event.emit("score")` in its `update()`. After one frame, Object A's handler should have fired. This tests the actual inter-object communication use case.

### Sampling Rate
- **Per task commit:** `ctest -R eventbus_test --output-on-failure`
- **Per wave merge:** `ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `include/enjin2/scripting/lua_event_bus.hpp` — `LuaEventBus` class declaration
- [ ] `src/scripting/lua_event_bus.cpp` — `LuaEventBus` method implementations
- [ ] `tests/eventbus_test.cpp` — covers EVENT-01 through EVENT-05
- [ ] `tests/CMakeLists.txt` — `eventbus_test` entry under `ENJIN2_BUILD_LUA` guard
- [ ] `include/enjin2/scripting/bindings.hpp` — add `m_eventBus` member; add `clearEventBus()` or make `setActiveScene()` non-trivial; include `lua_event_bus.hpp`
- [ ] `src/scripting/bindings.cpp` — `clearHandlers()` + `setLuaState()` call in `registerAll()`; `clearHandlers()` call in `setActiveScene()` when scene changes
- [ ] `src/scripting/bindings_engine.cpp` — `engine.event` sub-table in `registerEngineTable()`; `lua_engine_event_on/off/emit` implementations
- [ ] CMake: register `lua_event_bus.cpp` in the build (either in root `CMakeLists.txt` or `src/scripting/CMakeLists.txt`)

*(No framework install needed — hand-rolled test framework already in place)*

## Sources

### Primary (HIGH confidence)
- Codebase: `src/scripting/bindings.cpp` — `registerAll()` structure; pointer-to-pointer registry pattern for `enjin_ssm`, `enjin_active_scene`, `enjin_time`; `LuaBindings::setActiveScene()` injection point
- Codebase: `src/scripting/bindings_engine.cpp` — `registerEngineTable()` sub-table pattern; `lua_engine_scene_switch()` pointer-to-pointer retrieval; `lua_engine_scene_spawn()` (only existing `luaL_ref` usage pattern)
- Codebase: `include/enjin2/scripting/bindings.hpp` — `LuaBindings` class layout; all injected pointer members; `m_timeState`, `m_ssm`, `m_activeScene`, `m_store` — EventBus follows same style
- Codebase: `.planning/phases/40-c-timer/40-RESEARCH.md` — `luaL_ref` lifecycle, clearTimers() pattern, destruction order pitfall — EventBus replicates these patterns for scene-scoped lifetime
- Codebase: `include/enjin2/core/scene.hpp` — `onDeactivateSignal`, `deactivate()`, `connectOnDeactivate()` — scene lifecycle hooks available for EventBus deactivation
- Codebase: `include/enjin2/core/scene_state_machine.hpp` — `applyDeferredTransition()`, `switchTo()` — confirms that scene transitions go through `deactivate()` on the old scene
- Codebase: `include/enjin2/scripting/bind_helpers.hpp` — `luaBindFunctions`, `LuaFuncDef`, `ENJIN_ARRAY_LEN` — used for engine.event sub-table registration
- Codebase: `.planning/REQUIREMENTS.md` — EVENT-01..EVENT-05 definitions; "Event bus data payload" explicitly Out of Scope

### Secondary (MEDIUM confidence)
- Codebase: `.planning/STATE.md` — pointer-to-pointer registry decision documented: "engine.* pointers via pointer-to-pointer registry — Post-registerAll injection survives registry snap; no re-registerAll needed"
- Codebase: `.planning/ROADMAP.md` — Phase 42 success criteria; "Phase 38 (engine.* table infrastructure for adding engine.event sub-table)" as the declared dependency

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new technology; EventBus is a composition of `luaL_ref` (verified in C_Timer), pointer-to-pointer injection (verified in multiple existing bindings), and fixed-array storage (verified throughout codebase)
- Architecture: HIGH — all patterns are established in the codebase; EventBus is the simplest of the v1.6 phases (no Component involvement, no proxy lifecycle)
- Pitfalls: HIGH — destruction order pitfall verified by reading C_LuaScript destructor order; re-entrancy pitfall is a well-known Lua C-API issue; EVENT-04 gap pitfall identified by tracing `setActiveScene()` callsites

**Research date:** 2026-02-28
**Valid until:** 2026-03-28 (stable C++ codebase, 30-day window appropriate)
