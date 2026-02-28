# Architecture Research

**Domain:** Embedded/WASM 2D game engine — v1.6 Game-Ready feature integration
**Researched:** 2026-02-28
**Confidence:** HIGH

This document focuses exclusively on how the five v1.6 features (C_Timer, C_StateMachine,
ComponentProxy/self:get(), signal/event bus, persistent objects) integrate with the existing
Component/Object/Scene architecture. All analysis is derived from direct reading of the live
codebase at HEAD (post-v1.5).

---

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Lua Script Layer                             │
│  init(self)  update(self, dt)  draw(self)  on_button_pressed(self,b)│
│  self:get("C_Timer")  engine.events.*  engine.scene.persist(obj)    │
├─────────────────────────────────────────────────────────────────────┤
│                    ScriptProxy / ComponentProxy                      │
│   __index/__newindex dispatch  |  ComponentProxy full userdata      │
├─────────────────────────────────────────────────────────────────────┤
│                    C++ Component Layer                               │
│  C_Timer    C_StateMachine    C_Position    C_Sprite    C_LuaScript │
│     |              |               |            |            |       │
│              Object (MAX_COMPONENTS=16, array of unique_ptr)        │
├─────────────────────────────────────────────────────────────────────┤
│                  Scene / ObjectCollection                            │
│  ObjectCollection (owned MAX_OBJECTS=128 + external MAX_PERSIST=16) │
│  SceneStateMachine (MAX_SCENES=32) + PersistentObjectRegistry       │
├─────────────────────────────────────────────────────────────────────┤
│                     Signal / EventBus Layer                          │
│  Signal<Args...> (existing, MAX_CONNECTIONS=16, std::function)      │
│  EventBus (NEW: named channels, int luaRefs[], const char* keys)    │
└─────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| `C_Timer` | Delayed and repeating Lua callbacks; owns static TimerSlot array | Fires via Lua registry refs; C_Timer::update() in Object update chain |
| `C_StateMachine` | Named state machine; tracks current state; calls enter/exit/update Lua callbacks | Accessed from Lua via ComponentProxy returned by self:get() |
| `ScriptProxy` | Existing full userdata passed as `self` to all Lua callbacks | Now also dispatches `self:get(typeName)` to new lua_proxy_get_component |
| `ComponentProxy` | NEW full userdata returned by `self:get("TypeName")`; wraps Component* | Has per-type metatable (e.g. "C_Timer"); invalidated by Object::~Object() |
| `EventBus` | Named string-keyed channels; static slot arrays; fires Lua callbacks cross-object | Wired into engine.events sub-table in registerEngineTable(); cleared on scene switch |
| `PersistentObjectRegistry` | Static array of Object* surviving scene transitions; SSM-owned | SSM calls removeFrom/transferTo during applyDeferredTransition() |

---

## Recommended Project Structure

```
include/enjin2/
├── components/
│   ├── timer.hpp              NEW  -- C_Timer component + TimerSlot struct
│   ├── state_machine.hpp      NEW  -- C_StateMachine component + StateSlot struct
│   ├── lua_script.hpp         MOD  -- ScriptProxy __index dispatches self:get()
│   └── ...existing...
├── core/
│   ├── event_bus.hpp          NEW  -- EventBus + EventChannel struct
│   ├── persistent_registry.hpp NEW -- PersistentObjectRegistry
│   ├── component_proxy.hpp    NEW  -- ComponentProxy userdata struct
│   ├── object.hpp             MOD  -- add m_componentProxies[], registerComponentProxy()
│   ├── object_collection.hpp  MOD  -- add m_external[] non-owning array
│   ├── scene_state_machine.hpp MOD -- add m_persistent, modify applyDeferredTransition()
│   └── ...existing...
└── scripting/
    ├── bindings.hpp           MOD  -- add engine.timer, engine.events sub-tables; ComponentProxy metatable
    └── ...existing...

src/
├── components/
│   ├── timer.cpp              NEW  -- C_Timer implementation
│   └── state_machine.cpp      NEW  -- C_StateMachine implementation
├── core/
│   ├── event_bus.cpp          NEW  -- EventBus implementation
│   └── persistent_registry.cpp NEW -- PersistentObjectRegistry implementation
└── scripting/
    ├── bindings_timer.cpp     NEW  -- engine.timer.* Lua bindings + C_Timer metatable
    ├── bindings_events.cpp    NEW  -- engine.events.* Lua bindings
    └── bindings_engine.cpp    MOD  -- add engine.timer + engine.events sub-tables;
                                       add engine.scene.persist/unpersist;
                                       add resolveComponent() dispatch;
                                       add lua_proxy_get_component
```

### Structure Rationale

- **`components/timer.hpp` + `timer.cpp`:** C_Timer is a standard Component subclass. The pattern is identical to C_Position and C_Sprite — C++ component in `components/`, Lua surface in a dedicated `bindings_timer.cpp`.
- **`core/event_bus.hpp`:** EventBus is a core system (not a Component), analogous to `signal.hpp`. Placed in `core/` because it is Lua-independent — C++ objects could use it without Lua if needed.
- **`core/component_proxy.hpp`:** ComponentProxy userdata struct is directly analogous to `scripting/object_proxy.hpp`. Standalone header prevents circular includes.
- **`scripting/bindings_timer.cpp` + `bindings_events.cpp`:** Consistent with the existing 8-file bindings split strategy established in v1.5. Each domain gets its own file.

---

## Architectural Patterns

### Pattern 1: Component-as-Service (C_Timer, C_StateMachine)

**What:** New components live as standard `Component` subclasses on an Object. They are added by C++ host code or from Lua via `engine.scene.spawn()` + add-component Lua functions. The component's `update(float dt)` drives internal logic. Lua accesses the component through a `ComponentProxy` returned by `self:get("C_Timer")`.

**When to use:** Any engine capability that needs to tick per-frame, owns internal state, and must be accessible from Lua on the same object.

**Trade-offs:** Consumes one of the 16 component slots per object. This is correct — do not invent a global timer registry, which would break the per-object ownership model and complicate cleanup.

**C_Timer — zero-alloc data structures:**

```cpp
// include/enjin2/components/timer.hpp
namespace enjin2 {

struct TimerSlot {
    float remaining{0.0f};    // seconds until fire
    float interval{0.0f};     // repeat interval (0 = one-shot)
    int   luaRef{LUA_NOREF};  // Lua registry ref for callback function
    bool  active{false};
    bool  repeat{false};
};

class C_Timer : public Component {
public:
    static constexpr int MAX_TIMERS = 8;  // per-component static limit

    explicit C_Timer(Object* owner);
    ~C_Timer();  // luaL_unref all active refs

    int  after(float delay, int luaRef);   // returns slot id (0..MAX_TIMERS-1)
    int  every(float interval, int luaRef);
    void cancel(int id);
    void cancelAll();

    void update(float dt) override;

    void setLuaState(lua_State* L) { m_L = L; }

private:
    TimerSlot  m_slots[MAX_TIMERS]{};
    lua_State* m_L{nullptr};  // injected by binding after addComponent
};

} // namespace enjin2
```

**Key zero-alloc decisions:**
- `TimerSlot m_slots[MAX_TIMERS]` — fixed array on component stack; no heap.
- Callbacks stored as Lua registry refs (integers from `luaL_ref`) — no C++ heap allocation.
- `m_L` pointer injected after construction; all timer ops null-check `m_L`.

**C_StateMachine — zero-alloc data structures:**

```cpp
// include/enjin2/components/state_machine.hpp
namespace enjin2 {

struct StateSlot {
    const char* name{nullptr};       // Lua-interned string pointer (stable lifetime)
    int         enterRef{LUA_NOREF};
    int         updateRef{LUA_NOREF};
    int         exitRef{LUA_NOREF};
    bool        active{false};
};

class C_StateMachine : public Component {
public:
    static constexpr int MAX_STATES = 8;

    explicit C_StateMachine(Object* owner);
    ~C_StateMachine();  // luaL_unref all refs

    void addState(const char* name, int enterRef, int updateRef, int exitRef);
    bool transition(const char* stateName);
    const char* currentState() const;

    void update(float dt) override;
    void start() override;  // fires enter for initial state if set

    void setLuaState(lua_State* L) { m_L = L; }

private:
    StateSlot  m_states[MAX_STATES]{};
    int        m_stateCount{0};
    int        m_currentIdx{-1};
    lua_State* m_L{nullptr};
};

} // namespace enjin2
```

**State name storage:** State names must outlive the component. Intern the string via Lua registry at binding time (`luaL_ref` on the string, retrieve pointer via `lua_rawgeti` + `lua_tostring`). The `const char*` stored in `StateSlot::name` points to the Lua-interned string, which is stable for the Lua state lifetime. This matches the existing pattern for object names in `lua_engine_scene_spawn`.

---

### Pattern 2: ComponentProxy — self:get() Dispatch

**What:** `self:get("TypeName")` returns a `ComponentProxy` full userdata wrapping a `Component*`. The proxy has a per-type metatable (e.g. `"C_Timer"`) whose `__index` dispatches method calls to the specific component. The proxy must be invalidated when its owner Object is destroyed — identical mechanism to `ObjectProxy`.

**New struct (analogous to `object_proxy.hpp`):**

```cpp
// include/enjin2/core/component_proxy.hpp
namespace enjin2 {

struct ComponentProxy {
    Component* component;  // Non-owning. Do NOT dereference if valid == false.
    bool valid;            // Set false by Object::~Object() before components are freed.
};

} // namespace enjin2
```

**Integration into ScriptProxy `__index`:**

The existing `ScriptProxy.__index` handler in `bindings.cpp` dispatches known string keys to `C_LuaScript` properties. Add `"get"` as a key that returns a closure:

```cpp
// In ScriptProxy __index handler (bindings.cpp)
if (strcmp(key, "get") == 0) {
    lua_pushcfunction(L, lua_proxy_get_component);
    return 1;
}
```

**`lua_proxy_get_component` implementation (in `bindings_engine.cpp`):**

```cpp
static int lua_proxy_get_component(lua_State* L) {
    auto* sp = static_cast<ScriptProxy*>(luaL_checkudata(L, 1, "ScriptProxy"));
    if (!sp->valid) { luaL_error(L, "ScriptProxy: owner object destroyed"); }
    const char* typeName = luaL_checkstring(L, 2);
    // Dispatch by name -- stays in binding layer, Object has no Lua knowledge
    Component* comp = resolveComponent(sp->component->getOwner(), typeName);
    if (!comp) { lua_pushnil(L); return 1; }

    auto* proxy = static_cast<ComponentProxy*>(
        lua_newuserdata(L, sizeof(ComponentProxy)));
    proxy->component = comp;
    proxy->valid     = true;
    luaL_getmetatable(L, typeName);  // e.g. "C_Timer", "C_StateMachine"
    lua_setmetatable(L, -2);

    // Register proxy with Object for invalidation on destruction
    sp->component->getOwner()->registerComponentProxy(proxy);
    return 1;
}

// Type-name dispatch table -- lives in binding layer only
static Component* resolveComponent(Object* obj, const char* typeName) {
    if (strcmp(typeName, "C_Timer")        == 0) return obj->getComponent<C_Timer>();
    if (strcmp(typeName, "C_StateMachine") == 0) return obj->getComponent<C_StateMachine>();
    if (strcmp(typeName, "C_Position")     == 0) return obj->getComponent<C_Position>();
    return nullptr;
}
```

**Object modification for proxy invalidation:**

`Object` gains a fixed array of ComponentProxy pointers and a registration method:

```cpp
// In object.hpp (private section)
static constexpr size_t MAX_COMPONENT_PROXIES = MAX_COMPONENTS;  // one per component max
ComponentProxy* m_componentProxies[MAX_COMPONENT_PROXIES]{};
size_t m_componentProxyCount{0};

// In object.hpp (public section)
void registerComponentProxy(ComponentProxy* proxy) {
    if (m_componentProxyCount < MAX_COMPONENT_PROXIES) {
        m_componentProxies[m_componentProxyCount++] = proxy;
    }
}
```

`Object::~Object()` invalidates all registered ComponentProxy instances before the component array is destroyed:

```cpp
// In object.cpp ~Object()
// Invalidate ComponentProxy instances (Phase 2 addition alongside existing ObjectProxy logic)
for (size_t i = 0; i < m_componentProxyCount; ++i) {
    if (m_componentProxies[i]) {
        m_componentProxies[i]->valid = false;
    }
}
```

---

### Pattern 3: C_StateMachine Lua Surface via ComponentProxy Metatable

**Lua surface (from script):**

```lua
local sm = self:get("C_StateMachine")

local function on_idle_enter(self) engine.log("entering idle") end
local function on_idle_update(self, dt) end
local function on_idle_exit(self)  engine.log("leaving idle")  end

sm:addState("idle",  on_idle_enter,  on_idle_update,  on_idle_exit)
sm:addState("chase", on_chase_enter, on_chase_update, on_chase_exit)
sm:transition("idle")

-- Per-frame (called automatically by C_StateMachine::update):
-- on_idle_update(self, dt) fires each frame while in "idle"
```

**`"C_StateMachine"` metatable methods:**

```
addState(name, enterFn, updateFn, exitFn)  -> nil
transition(name)                           -> bool (success)
current()                                  -> string (current state name)
```

Callbacks follow the `callWithProxy` convention — each callback receives `self` (the ScriptProxy) as first argument. This requires storing the ScriptProxy ref alongside the state refs, or re-pushing it during each callback dispatch. The cleanest approach: pass `self` as the first argument exactly as `callWithProxy` does already. The `C_StateMachine` binding has access to the ScriptProxy ref stored in the Lua registry (same mechanism as `callWithProxy`).

---

### Pattern 4: EventBus — Named Channels for Cross-Object Communication

**What:** `EventBus` is a static-alloc named channel system. Channels are identified by `const char*` name. Subscribers register Lua function refs. Publishers call `emit(channel, ...)` which fires all subscribers with variadic arguments forwarded from the Lua stack.

**Why the existing `Signal<T>` is wrong for this use case:** `Signal<Args...>` uses `std::function` (heap-allocated closures larger than ~24 bytes) and is strongly-typed at compile time. The EventBus needs string-keyed channels unknown at compile time, and Lua function callbacks stored as integer registry refs with no C++ heap involvement.

**EventBus data structure — zero alloc:**

```cpp
// include/enjin2/core/event_bus.hpp
namespace enjin2 {

struct EventListener {
    int  luaRef{LUA_NOREF};
    bool active{false};
};

struct EventChannel {
    const char* name{nullptr};          // Lua-interned or static string
    bool        channelActive{false};
    static constexpr int MAX_LISTENERS = 8;
    EventListener listeners[MAX_LISTENERS]{};
    int           listenerCount{0};     // total slots used (including inactive gaps)
};

class EventBus {
public:
    static constexpr int MAX_CHANNELS = 16;

    // Returns subscription ID (channel_idx * MAX_LISTENERS + listener_idx), or -1 on failure
    int  subscribe(lua_State* L, const char* channel, int funcRef);
    void unsubscribe(lua_State* L, int subId);
    // nargs: number of args on Lua stack above the channel name to forward to callbacks
    void emit(lua_State* L, const char* channel, int nargs);
    void clear(lua_State* L);  // unref all listener refs, reset all slots

private:
    EventChannel m_channels[MAX_CHANNELS]{};
    int          m_channelCount{0};

    int findChannel(const char* name) const;   // returns index or -1
    int findOrCreateChannel(const char* name); // returns index or -1 if full
};

} // namespace enjin2
```

**Lua surface — `engine.events` sub-table:**

```lua
-- Subscribe to a named channel
local sub_id = engine.events.subscribe("score_changed", function(score)
    engine.log("new score:", score)
end)

-- Emit with arguments (all args forwarded to all subscribers)
engine.events.emit("score_changed", 100)

-- Unsubscribe when done
engine.events.unsubscribe(sub_id)
```

**Variadic emit implementation:** `lua_events_emit` pops the channel string from arg 1, counts remaining args as `nargs = lua_gettop(L) - 1`, then calls `EventBus::emit(L, channel, nargs)`. Inside `emit`, for each active listener: push callback from registry, duplicate the nargs stack values above it, call with `lua_call(L, nargs, 0)`. This requires careful stack management — push callback first, then copy args.

**Scene lifetime:** EventBus is stored on `LuaBindings` as a value member (zero heap). Pointer stored in Lua registry as `"enjin_event_bus"` (same pointer-to-pointer pattern as SSM and activeScene). EventBus::clear() is called from `applyDeferredTransition()` in SceneStateMachine before activating the new scene, preventing stale cross-scene subscriptions.

**Engine wiring in `registerEngineTable()`:**

```cpp
// In bindings_engine.cpp registerEngineTable()
static const LuaFuncDef kEventsFuncs[] = {
    {"subscribe",   lua_events_subscribe},
    {"unsubscribe", lua_events_unsubscribe},
    {"emit",        lua_events_emit},
};
lua_newtable(L);
luaBindFunctions(L, -1, kEventsFuncs, ENJIN_ARRAY_LEN(kEventsFuncs));
lua_setfield(L, -2, "events");
```

---

### Pattern 5: Persistent Objects — Surviving Scene Transitions

**What:** Objects marked persistent are transferred from the departing scene to the arriving scene by `SceneStateMachine::applyDeferredTransition()`. SSM owns the persistent objects via a dedicated `unique_ptr` array. Scenes borrow them via a non-owning raw pointer array in ObjectCollection.

**Why SSM ownership:** ObjectCollection uses `unique_ptr<Object>` for owned objects. Persistent objects must NOT be destroyed when the old scene's ObjectCollection is torn down. The cleanest solution: SSM holds the `unique_ptr<Object>` for persistent objects in a dedicated `m_persistentOwned[]` array. The old scene's ObjectCollection holds a raw non-owning pointer that is removed before deactivation. The new scene's ObjectCollection also holds a raw non-owning pointer added before activation.

**PersistentObjectRegistry — zero alloc:**

```cpp
// include/enjin2/core/persistent_registry.hpp
namespace enjin2 {

class PersistentObjectRegistry {
public:
    static constexpr int MAX_PERSISTENT = 16;

    bool contains(Object* obj) const;
    // Called when SSM takes ownership:
    bool add(std::unique_ptr<Object> obj);
    // Called when removing persistence (returns ownership to scene or destroys):
    std::unique_ptr<Object> remove(Object* obj);

    // Transfer into scene's external array (non-owning):
    void injectInto(ObjectCollection& col) const;
    // Remove from scene's external array before scene teardown:
    void withdrawFrom(ObjectCollection& col) const;

    int count() const { return m_count; }

private:
    std::unique_ptr<Object> m_owned[MAX_PERSISTENT]{};
    int                     m_count{0};
};

} // namespace enjin2
```

**ObjectCollection modification — non-owning external array:**

```cpp
// In object_collection.hpp (private section)
static constexpr size_t MAX_EXTERNAL = 16;  // matches PersistentObjectRegistry::MAX_PERSISTENT
Object* m_external[MAX_EXTERNAL]{};          // non-owning raw pointers
size_t  m_externalCount{0};

// New public methods:
bool injectExternal(Object* obj);       // add to m_external[]
bool withdrawExternal(Object* obj);     // remove from m_external[]

// update() and lateUpdate() iterate m_external[] in addition to owned objects[]
```

**SceneStateMachine modification:**

```cpp
// In scene_state_machine.hpp (private section)
PersistentObjectRegistry m_persistent;  // value member, zero heap

// In applyDeferredTransition():
void applyDeferredTransition(uint32_t targetId) {
    // ... existing scene lookup ...
    if (currentScene) {
        m_persistent.withdrawFrom(currentScene->getObjects());  // NEW
        currentScene->deactivate();
    }
    currentScene = targetScene;
    m_persistent.injectInto(currentScene->getObjects());         // NEW
    if (!currentScene->isInitialized()) currentScene->initialize();
    if (!currentScene->isActive()) currentScene->activate();
    // ... signals ...
}
```

**Lua surface:**

```lua
local player = engine.scene.spawn("player")
engine.scene.persist(player)     -- SSM takes ownership; player survives scene switch

-- Later:
engine.scene.unpersist(player)   -- SSM returns ownership to current scene
                                  -- (or destroys if current scene refuses it)
```

`engine.scene.persist(proxy)` is implemented in `lua_engine_scene_persist`:
1. Validate ObjectProxy.
2. Remove Object from current scene's owned collection (scene->removeObject, which returns the unique_ptr).
3. Call `m_persistent.add(std::move(ownedPtr))`.
4. Call `m_persistent.injectInto(currentScene->getObjects())` so the object continues to update this frame.

---

## Data Flow

### C_Timer Fire Sequence

```
SceneStateMachine::update(dt)
    SceneStateMachine::applyPendingTransition() -- deferred, after update
    Scene::update(dt)
        ObjectCollection::update(dt)
            Object::update(dt) for each active object
                C_Timer::update(dt)
                    for each active slot:
                        slot.remaining -= dt
                        if slot.remaining <= 0:
                            lua_rawgeti(L, LUA_REGISTRYINDEX, slot.luaRef)
                            lua_call(L, 0, 0)          -- fire callback
                            if slot.repeat:
                                slot.remaining = slot.interval
                            else:
                                luaL_unref(L, LUA_REGISTRYINDEX, slot.luaRef)
                                slot.active = false
```

**Re-entrancy note:** Timer callbacks that call `engine.scene.switch()` are safe — the deferred transition mechanism in `SceneStateMachine::update()` executes AFTER `currentScene->update()` returns. No additional protection needed.

### C_StateMachine Transition Sequence

```
Lua: sm:transition("chase")
    ComponentProxy.__index("transition") -> lua_statemachine_transition(L)
        C_StateMachine::transition("chase")
            1. Find slot index for "chase"
            2. If m_currentIdx >= 0 and exitRef != LUA_NOREF:
                   push ScriptProxy (from registry)
                   lua_rawgeti(L, LUA_REGISTRYINDEX, exitRef)
                   swap self to top, lua_call(L, 1, 0)
            3. m_currentIdx = slot index for "chase"
            4. If enterRef != LUA_NOREF:
                   push ScriptProxy
                   lua_rawgeti(L, LUA_REGISTRYINDEX, enterRef)
                   swap self to top, lua_call(L, 1, 0)

Per-frame (C_StateMachine::update(dt)):
    If m_currentIdx >= 0 and updateRef != LUA_NOREF:
        push ScriptProxy
        lua_rawgeti(L, LUA_REGISTRYINDEX, updateRef)
        swap self to top
        lua_pushnumber(L, dt)
        lua_call(L, 2, 0)       -- update(self, dt)
```

### ComponentProxy Lookup and Use

```
Lua: local timer = self:get("C_Timer")
    ScriptProxy.__index("get") -> returns lua_proxy_get_component closure
    self:get("C_Timer") -> lua_proxy_get_component(L)
        1. Validate ScriptProxy.valid
        2. resolveComponent(owner, "C_Timer") -> dynamic_cast<C_Timer*>
        3. Allocate ComponentProxy userdata
        4. Attach "C_Timer" metatable
        5. owner->registerComponentProxy(proxy)
        Returns: ComponentProxy userdata

Lua: timer:after(1.0, callback)
    ComponentProxy.__index("after") -> lua_ctimer_after(L)
        1. luaL_checkudata(L, 1, "C_Timer") -> ComponentProxy*
        2. if !proxy->valid: luaL_error(...)
        3. float delay = luaL_checknumber(L, 2)
        4. lua_pushvalue(L, 3); int ref = luaL_ref(L, LUA_REGISTRYINDEX)
        5. C_Timer* t = static_cast<C_Timer*>(proxy->component)
        6. int id = t->after(delay, ref)
        7. lua_pushinteger(L, id)
        Returns: slot id (for cancel)
```

### Event Bus Signal Flow

```
Lua: engine.events.emit("score_changed", 100)
    lua_events_emit(L)
        channel = luaL_checkstring(L, 1)    -- "score_changed"
        nargs   = lua_gettop(L) - 1         -- 1 (just the 100)
        EventBus::emit(L, "score_changed", nargs=1)
            ch = findChannel("score_changed")
            for each active listener in ch.listeners[]:
                lua_rawgeti(L, LUA_REGISTRYINDEX, listener.luaRef)
                -- duplicate nargs values from bottom of arg list
                for i in 1..nargs: lua_pushvalue(L, 2+i-1)   -- push copies of args
                lua_call(L, nargs, 0)
            -- all subscriber callbacks fired synchronously
```

### Persistent Object Transfer

```
SSM::applyDeferredTransition(targetId)
    1. m_persistent.withdrawFrom(currentScene->getObjects())
       -- ObjectCollection removes obj from m_external[] (does NOT free)
    2. currentScene->deactivate()
       -- currentScene's owned ObjectCollection destroyed
       -- persistent objects were withdrawn from external[] -- safe, SSM still owns unique_ptr
    3. currentScene = targetScene
    4. m_persistent.injectInto(currentScene->getObjects())
       -- ObjectCollection adds obj to m_external[]; update/lateUpdate will include it
    5. if !currentScene->isInitialized(): currentScene->initialize()
    6. if !currentScene->isActive(): currentScene->activate()
       -- start() called on persistent objects too (they may need re-start)
```

---

## Scaling Considerations

This engine targets embedded devices (ESP32, WASM). Scale is measured in objects and components per scene, not in concurrent users.

| Concern | At 32 objects/scene | At 128 objects/scene |
|---------|--------------------|-----------------------|
| C_Timer slot scan | 8 slots x 32 = 256 checks/frame — negligible | 8 x 128 = 1024 — sub-microsecond on ESP32 |
| EventBus emit scan | 16 channels x 8 listeners = 128 checks per emit — fine | Same (bus is global/scene-scoped, not per-object) |
| ComponentProxy resolve | strcmp chain over ~5 known types — O(1) in practice | Same |
| Persistent objects | Max 16 — no issue | Max 16 — no issue |
| C_StateMachine lookup | strcmp over max 8 states per component — O(1) | Same |
| Object::~Object() proxy sweep | MAX_COMPONENTS (16) + MAX_COMPONENT_PROXIES (16) iterations | Same |

These ceilings are architectural limits, not performance bottlenecks for target game scale (Arkanoid, tamagotchi, physics sandbox at ESP32 frame budgets).

---

## Anti-Patterns

### Anti-Pattern 1: Global Timer Registry

**What people do:** Create a `TimerManager` singleton outside the Component system.

**Why it's wrong:** Breaks per-object ownership. When an Object is destroyed, its timers should stop automatically. A global registry requires explicit cancellation at destroy time, adding coordination complexity that violates zero-alloc discipline.

**Do this instead:** C_Timer as a Component on the Object. When the Object is destroyed, its components array (`std::unique_ptr<Component>`) destroys C_Timer, whose destructor calls `luaL_unref` on all active Lua callback refs. Cleanup is automatic and zero-cost.

---

### Anti-Pattern 2: std::function for EventBus Listeners

**What people do:** Use the existing `Signal<Args...>` (which uses `std::function` internally) as the EventBus listener store.

**Why it's wrong:** `std::function` heap-allocates for callables larger than the small-buffer optimization (~24 bytes). Lua registry refs are `int` — no heap needed. The existing `Signal` is correct for strongly-typed C++ callbacks (Scene lifecycle signals), but wrong for string-keyed Lua EventBus where callbacks are just integer refs.

**Do this instead:** Store `int luaRefs[]` arrays in `EventChannel`. Fire with `lua_rawgeti` + `lua_call`. Zero `std::function`, zero heap in the hot path.

---

### Anti-Pattern 3: Adding C_Timer Methods Directly to ScriptProxy Metatable

**What people do:** Add `self:timerAfter(...)`, `self:stateTransition(...)` etc. directly to the `ScriptProxy` metatable.

**Why it's wrong:** Pollutes the ScriptProxy namespace indefinitely as more component types are added. Every new component type expands `ScriptProxy` with more methods, creating name collision risk.

**Do this instead:** `self:get("C_Timer")` returns a separate `ComponentProxy` userdata with its own `"C_Timer"` metatable. Each component type has its own clean method namespace. `ScriptProxy` only gains a single `get` method.

---

### Anti-Pattern 4: Forgetting ComponentProxy Invalidation on Object Destruction

**What people do:** Only invalidate `ObjectProxy` (already done in v1.5) but not `ComponentProxy` instances pointing to components on the same destroyed Object.

**Why it's wrong:** After `Object::~Object()`, any `ComponentProxy` held in a Lua variable will have a dangling `Component*`. Dereferencing it is undefined behavior. This is the same class of bug that ObjectProxy proxy invalidation was designed to prevent.

**Do this instead:** `Object::~Object()` iterates `m_componentProxies[]` and sets `valid = false` on each registered `ComponentProxy` before the component array (`components[]`) is destroyed. `lua_proxy_get_component` registers the new proxy immediately after construction.

---

### Anti-Pattern 5: Shared Ownership for Persistent Objects

**What people do:** Change `ObjectCollection` to `std::shared_ptr<Object>` to allow shared ownership between scenes and the persistent registry.

**Why it's wrong:** `std::shared_ptr` requires a heap-allocated control block — violates zero-alloc constraint. On ESP32, every heap allocation is a risk.

**Do this instead:** SSM holds `std::unique_ptr<Object>` for persistent objects in `m_persistentOwned[]`. Scenes receive a raw non-owning `Object*` in `ObjectCollection::m_external[]`. The ownership boundary is clear: SSM owns persistent objects exclusively; scenes only borrow them.

---

## Integration Points

### New vs Modified Files (Explicit)

| File | Status | What Changes |
|------|--------|--------------|
| `include/enjin2/components/timer.hpp` | NEW | C_Timer + TimerSlot struct |
| `include/enjin2/components/state_machine.hpp` | NEW | C_StateMachine + StateSlot struct |
| `include/enjin2/core/event_bus.hpp` | NEW | EventBus + EventChannel + EventListener structs |
| `include/enjin2/core/component_proxy.hpp` | NEW | ComponentProxy userdata struct (analogous to object_proxy.hpp) |
| `include/enjin2/core/persistent_registry.hpp` | NEW | PersistentObjectRegistry |
| `include/enjin2/core/object.hpp` | MODIFIED | Add `m_componentProxies[]`, `m_componentProxyCount`, `registerComponentProxy()`; `~Object()` invalidates them |
| `include/enjin2/core/object_collection.hpp` | MODIFIED | Add `m_external[]`, `m_externalCount`, `injectExternal()`, `withdrawExternal()`; update `update()`/`lateUpdate()` to iterate external |
| `include/enjin2/core/scene_state_machine.hpp` | MODIFIED | Add `PersistentObjectRegistry m_persistent`; modify `applyDeferredTransition()` for withdraw/inject |
| `include/enjin2/scripting/bindings.hpp` | MODIFIED | Add `EventBus m_eventBus` member; declare `engine.timer`, `engine.events` sub-table registration; add ComponentProxy metatable registration |
| `src/components/timer.cpp` | NEW | C_Timer implementation |
| `src/components/state_machine.cpp` | NEW | C_StateMachine implementation |
| `src/core/event_bus.cpp` | NEW | EventBus implementation |
| `src/core/persistent_registry.cpp` | NEW | PersistentObjectRegistry implementation |
| `src/core/object.cpp` | MODIFIED | ComponentProxy invalidation in ~Object() |
| `src/scripting/bindings_engine.cpp` | MODIFIED | Add `engine.timer` + `engine.events` + `engine.scene.persist/unpersist` to `registerEngineTable()`; add `resolveComponent()` dispatch; add `lua_proxy_get_component` |
| `src/scripting/bindings_timer.cpp` | NEW | `engine.timer.*` (if any global timer API needed) + C_Timer ComponentProxy metatable methods |
| `src/scripting/bindings_events.cpp` | NEW | `engine.events.*` Lua bindings; EventBus wiring |
| `CMakeLists.txt` | MODIFIED | Add new .cpp files to `enjin2_lua` + `enjin2_core` `target_sources` blocks |

### Build Order (Dependency-Ordered)

| Phase | Feature | Prerequisite | Rationale |
|-------|---------|-------------|-----------|
| 1 | ComponentProxy / self:get() | None (only existing Object/ScriptProxy) | All other Lua-facing component features depend on ComponentProxy. Build the access mechanism before building the accessed components. |
| 2 | C_Timer | ComponentProxy (Phase 1) | Most commonly needed game primitive. No dependency on C_StateMachine or EventBus. |
| 3 | C_StateMachine | ComponentProxy (Phase 1) | Independent of C_Timer. Can be built in parallel with Phase 2, but Phase 1 must be complete. |
| 4 | EventBus / Signals | None (only LuaBindings) | Cross-object, not component-based. Independent of Phases 1-3. Place here so first three phases can be validated before adding cross-object communication complexity. |
| 5 | Persistent Objects | ObjectCollection (Phase 0 existing), SceneStateMachine | Most invasive structural change. Touches ObjectCollection, SceneStateMachine, and introduces non-owning external arrays. All earlier features should be green before making this change to isolate any regressions. |

---

## Sources

- Direct reading of `/home/unwn/dev/enjin/include/enjin2/core/component.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/core/object.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/core/scene.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/core/scene_state_machine.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/core/object_collection.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/core/signal.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/scripting/bind_helpers.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/scripting/object_proxy.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/include/enjin2/components/lua_script.hpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` — HIGH confidence
- Direct reading of `/home/unwn/dev/enjin/.planning/PROJECT.md` — HIGH confidence (active v1.6 requirements)

---
*Architecture research for: enjin2 v1.6 Game Ready*
*Researched: 2026-02-28*
