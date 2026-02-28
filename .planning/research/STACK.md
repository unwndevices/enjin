# Stack Research

**Domain:** Zero-alloc 2D game engine — timer, state machine, component proxy, event bus, persistent objects (v1.6)
**Researched:** 2026-02-28
**Confidence:** HIGH (derived from direct codebase analysis + established C++ embedded patterns)

---

## Scope

This document covers **only stack additions and API-level decisions for v1.6 Game Ready**. It does not re-research validated v1.5 capabilities (Lua scripting, ScriptProxy, float dt, named objects, scene transitions, etc.).

---

## What Already Exists (Critical Context)

Reading the live codebase reveals these constraints and integration points:

| Existing Element | Implication for v1.6 |
|------------------|----------------------|
| `std::function<void(Args...)>` already in `Signal<>` and `forEach` | Not a new dep; heap-risk from closures already accepted in codebase |
| `unique_ptr<T>` for Component/Object ownership | Heap allocation at construction time is already the pattern; "zero-alloc" means no per-frame heap |
| `luaL_ref` / `lua_rawgeti` used in `engine.scene.spawn()` for string interning | Pattern is established for storing Lua values in registry by integer key |
| `C_LuaScript` owns an entire `LuaScriptSystem` (full Lua VM, 20–40 KB overhead) | New components must NOT own separate Lua states — borrow the existing `lua_State*` |
| `ScriptProxy` / `ObjectProxy` use `valid` bool + non-owning raw pointer | Same pattern must apply to all new component proxy userdata |
| `SceneStateMachine::switchTo()` deferred transition after `update()` returns | `C_StateMachine` must mirror this deferral to prevent re-entrant FSM corruption |
| `Object::name` and tag system use `const char*` (string literal pointers, not copies) | Event channel names that may come from transient Lua strings must be COPIED into `char[N]` buffers |
| `LuaBindings::m_ssm` / `m_activeScene` injected via pointer-to-pointer in Lua registry | New EventBus follows the same injection pattern into `LuaBindings` |
| `LuaStore` uses `char[64]` keys, `char[128]` string values, 16-slot fixed array | Directly reusable as precedent for EventBus channel naming |
| `SpriteState::accumSec` float accumulator pattern | Direct precedent for `C_Timer` slot accumulator |

---

## Recommended Stack

### Core Technologies (No Changes)

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| C++17 | existing | All new components | `if constexpr`, `std::array`, SFINAE — already required |
| Lua 5.1 (LuaJIT 2.1) | existing | Script callbacks for timers, state machines, events | Borrow `lua_State*` from owning `C_LuaScript`; no second VM |
| CMake multi-target | existing | Build system | New components join `enjin2_core`; no new CMake targets |

### New Components (Header-Only Where Possible)

| Component | File | Purpose | Key Constraints |
|-----------|------|---------|----------------|
| `C_Timer` | `include/enjin2/components/timer.hpp` | Delayed/repeating Lua callbacks | Borrowed `lua_State*`; 8-slot static array; `luaL_ref` for callbacks |
| `C_StateMachine` | `include/enjin2/components/state_machine.hpp` | Per-object FSM with enter/exit hooks | Deferred transition pattern; `const char*` state names; 8-slot array |
| `EventBus` | `include/enjin2/core/event_bus.hpp` | Scene-scoped cross-object event dispatch | Scene-owned; `char[32]` channel names (copied); 16 channels × 8 listeners |

### Supporting Libraries (No New External Dependencies)

All new features use only stdlib headers already compiled into the project:

| Header | Already Compiled | Use in v1.6 |
|--------|-----------------|-------------|
| `<array>` | Yes | Timer slots, FSM state slots, event bus channels |
| `<cstring>` | Yes | `strcmp` for state/channel name lookup; `strncpy` for channel name copy |
| `<functional>` | Yes (Signal, forEach) | No new `std::function` slots — `luaL_ref` integer handles preferred for Lua callbacks |
| `<type_traits>` | Yes | `assertRequires<T>` in new components |
| `<cassert>` / `<cstdio>` | Yes | Debug assertions, release logging |

**Zero new library dependencies.** No new CMake `find_package` calls. No vendored headers.

---

## Implementation Patterns

### C_Timer

**Problem:** Timers need to fire Lua callbacks after a delay or on repeat interval. They must not own a Lua VM (too heavy — 20–40 KB). They must store callback references without heap allocation.

**Pattern: `luaL_ref` integer handles + float accumulator slots**

```cpp
// include/enjin2/components/timer.hpp
class C_Timer : public Component {
public:
    static constexpr size_t MAX_TIMERS = 8;

    struct Slot {
        float remaining{0.0f};   // seconds until next fire
        float interval{0.0f};    // >0 = repeating; 0 = one-shot
        int   luaRef{LUA_NOREF}; // luaL_ref handle into Lua registry (LUA_NOREF = empty)
        bool  active{false};
    };

    C_Timer(Object* owner, lua_State* L);
    ~C_Timer();  // calls luaL_unref on all active refs

    // Push Lua function to stack first, then call:
    // Returns slot index (0..MAX_TIMERS-1) or -1 if pool full
    int after(float delaySec, int luaFuncRef);
    int every(float intervalSec, int luaFuncRef);
    void cancel(int slotIdx);
    void cancelAll();

    void update(float dt) override;

private:
    lua_State* L_;    // borrowed — same state as parent C_LuaScript; NOT owned
    Slot slots_[MAX_TIMERS]{};

    void fireSlot(Slot& slot);
};
```

**Why:**
- `luaL_ref(L, LUA_REGISTRYINDEX)` atomically removes the top Lua value from the stack and anchors it by integer key — zero allocation, GC-safe, stable across GC cycles
- `float remaining` accumulator mirrors `SpriteState::accumSec` exactly — established pattern
- `lua_State*` borrowed from the C_LuaScript that owns this component — no second Lua VM
- 8 slots × ~16 bytes = 128 bytes per C_Timer instance — negligible on ESP32 SRAM

**Firing a timer:**
```cpp
void C_Timer::fireSlot(Slot& slot) {
    lua_rawgeti(L_, LUA_REGISTRYINDEX, slot.luaRef);  // push callback
    if (lua_isfunction(L_, -1)) {
        lua_pcall(L_, 0, 0, 0);  // call with no args, respect ScriptErrorPolicy upstream
    } else {
        lua_pop(L_, 1);
    }
}
```

**Destructor:**
```cpp
C_Timer::~C_Timer() {
    for (auto& slot : slots_) {
        if (slot.active && slot.luaRef != LUA_NOREF) {
            luaL_unref(L_, LUA_REGISTRYINDEX, slot.luaRef);
        }
    }
}
```

**Lua API (via ComponentProxy):**
```lua
local timer = self:get("C_Timer")
local h = timer:after(1.5, function() engine.log("fire once") end)
local h2 = timer:every(0.25, tick)
timer:cancel(h)
```

---

### C_StateMachine

**Problem:** Per-object FSM with named states, enter/exit/update Lua callbacks. Must not re-enter during transition (same problem as `SceneStateMachine::switchTo()`).

**Pattern: Fixed state slots + deferred transition + `luaL_ref` callbacks**

```cpp
// include/enjin2/components/state_machine.hpp
class C_StateMachine : public Component {
public:
    static constexpr size_t MAX_STATES = 8;

    struct State {
        const char* name{nullptr};     // string literal pointer — caller owns lifetime
        int enterRef{LUA_NOREF};       // luaL_ref for enter(self) callback
        int exitRef{LUA_NOREF};        // luaL_ref for exit(self) callback
        int updateRef{LUA_NOREF};      // luaL_ref for update(self, dt) callback (optional)
        bool active{false};            // slot occupied
    };

    C_StateMachine(Object* owner, lua_State* L);
    ~C_StateMachine();

    // name must be a string literal (permanent pointer) or interned Lua string.
    // enterRef, exitRef, updateRef from luaL_ref(); LUA_NOREF = no callback.
    bool addState(const char* name, int enterRef, int exitRef, int updateRef = LUA_NOREF);

    // Deferred: queued; applied after current update() returns.
    // Last-wins within a single frame (mirrors SceneStateMachine::switchTo()).
    void transitionTo(const char* name);

    const char* getState() const { return current_ ? current_->name : nullptr; }
    bool isInState(const char* name) const;

    void update(float dt) override;

private:
    lua_State* L_;
    State states_[MAX_STATES]{};
    size_t stateCount_{0};
    State* current_{nullptr};
    const char* pending_{nullptr};   // deferred target name; nullptr = no pending transition

    void applyTransition();
    void callRef(int ref, float dt, bool passDt);
};
```

**Why state names are `const char*` not `char[N]`:**
State names are typically string literals (`"idle"`, `"run"`, `"attack"`) with static storage duration, or Lua-interned strings (registered into the Lua registry via `luaL_ref` for lifetime anchoring). The existing `Object::name` and tag system uses `const char*` for the same reason. Copying into a `char[N]` buffer is unnecessary overhead when the caller controls the string lifetime.

**Deferred transition (mirrors SceneStateMachine):**
```cpp
void C_StateMachine::update(float dt) {
    if (!enabled) return;

    // Run current state's per-frame update
    if (current_ && current_->updateRef != LUA_NOREF) {
        callRef(current_->updateRef, dt, true);
    }

    // Apply any pending transition queued this frame
    if (pending_) {
        applyTransition();
        pending_ = nullptr;
    }
}

void C_StateMachine::applyTransition() {
    // Find target state
    State* target = nullptr;
    for (size_t i = 0; i < stateCount_; ++i) {
        if (states_[i].active && strcmp(states_[i].name, pending_) == 0) {
            target = &states_[i];
            break;
        }
    }
    if (!target) return;

    // Exit current
    if (current_ && current_->exitRef != LUA_NOREF) {
        callRef(current_->exitRef, 0.0f, false);
    }

    // Enter new
    current_ = target;
    if (current_->enterRef != LUA_NOREF) {
        callRef(current_->enterRef, 0.0f, false);
    }
}
```

**Lua API (via ComponentProxy):**
```lua
local fsm = self:get("C_StateMachine")
fsm:addState("idle",
    function(self) engine.log("enter idle") end,
    function(self) engine.log("exit idle") end)
fsm:addState("run",
    function(self) engine.log("enter run") end,
    function(self) engine.log("exit run") end,
    function(self, dt) -- per-frame update while in "run"
        -- movement logic
    end)
fsm:transitionTo("idle")
```

---

### ComponentProxy / self:get()

**Problem:** From a Lua script (`self` = ScriptProxy for C_LuaScript), the author needs `self:get("C_Timer")` to obtain a usable proxy to a sibling component. No std::map, no heap.

**Pattern: Extend ScriptProxy.__index + typed component userdata + fixed strcmp dispatch**

**Part 1 — Lua userdata struct for each component type:**

```cpp
// Reuse the ObjectProxy pattern. One generic struct covers all component types.
struct ComponentRef {
    Component* component;   // non-owning raw pointer
    bool valid;             // set false by component destructor
    const char* typeName;   // "C_Timer", "C_StateMachine" etc. — for metatable dispatch
};
```

`ComponentRef` is allocated as Lua full userdata (GC-managed, no C++ heap). Its metatable is named after the component type (e.g., `"C_Timer"`).

**Part 2 — `get` method on ScriptProxy:**

Extend `ScriptProxy.__index` to recognize `"get"` as a key, returning a closure or method:

```cpp
// In ScriptProxy __index metamethod:
if (strcmp(key, "get") == 0) {
    // Push a C closure that captures the owner Object* via upvalue
    // Closure receives one string arg: the component type name
    lua_pushvalue(L, 1);  // push self (ScriptProxy userdata) as upvalue
    lua_pushcclosure(L, scriptproxy_get, 1);
    return 1;
}

// scriptproxy_get closure:
static int scriptproxy_get(lua_State* L) {
    auto* proxy = static_cast<ScriptProxy*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!proxy || !proxy->valid) { lua_pushnil(L); return 1; }

    const char* typeName = luaL_checkstring(L, 1);
    Component* comp = findComponentByName(proxy->component->getOwner(), typeName);
    if (!comp) { lua_pushnil(L); return 1; }

    auto* ref = static_cast<ComponentRef*>(lua_newuserdata(L, sizeof(ComponentRef)));
    ref->component = comp;
    ref->valid = true;
    ref->typeName = typeName;
    luaL_getmetatable(L, typeName);   // e.g., "C_Timer" metatable
    lua_setmetatable(L, -2);
    return 1;
}
```

**Part 3 — Fixed string dispatch (no heap, no std::map):**

```cpp
// In bindings.cpp — static helper, no allocation
static Component* findComponentByName(Object* obj, const char* name) {
    if (strcmp(name, "C_Timer") == 0)        return obj->getComponent<C_Timer>();
    if (strcmp(name, "C_StateMachine") == 0) return obj->getComponent<C_StateMachine>();
    if (strcmp(name, "C_Position") == 0)     return obj->getComponent<C_Position>();
    // Add new component types here as they ship
    return nullptr;
}
```

**Why `ComponentRef` userdata not lightuserdata:**
Lightuserdata in Lua 5.1 / LuaJIT has NO metatable support. A `C_Timer` proxy needs `timer:after()`, `timer:every()`, `timer:cancel()` — these require a method table via `__index`. Full userdata is mandatory.

**Why `ComponentRef.valid` bool:**
If the owner Object is destroyed during the frame, any stored `ComponentRef` would hold a dangling `Component*`. The `valid` flag is set false in the component destructor (via a destructor hook). Mirrors `ObjectProxy`/`ScriptProxy` precedent exactly.

**Each component type registers its own metatable in `registerAll()`:**

```cpp
// C_Timer metatable
luaL_newmetatable(L, "C_Timer");
  lua_pushcfunction(L, componentref_after);
  lua_setfield(L, -2, "after");
  lua_pushcfunction(L, componentref_every);
  lua_setfield(L, -2, "every");
  lua_pushcfunction(L, componentref_cancel);
  lua_setfield(L, -2, "cancel");
  lua_pushvalue(L, -1);             // __index = self (method table)
  lua_setfield(L, -2, "__index");
lua_pop(L, 1);
```

---

### Event Bus

**Problem:** Lua scripts from different objects need to emit named events and subscribe callbacks. Events are string-keyed. No STL containers, no heap. Cross-object coupling must be safe after scene transitions.

**Pattern: Scene-scoped fixed-slot bus + `char[32]` channel names + `luaL_ref` listeners**

```cpp
// include/enjin2/core/event_bus.hpp
class EventBus {
public:
    static constexpr size_t MAX_CHANNELS = 16;
    static constexpr size_t MAX_LISTENERS = 8;   // per channel

    struct Listener {
        int  luaRef{LUA_NOREF};
        bool active{false};
    };

    struct Channel {
        char     name[32]{};    // COPIED from the event name string (transient-safe)
        Listener listeners[MAX_LISTENERS]{};
        size_t   listenerCount{0};
        bool     active{false};  // slot occupied
    };

    explicit EventBus(lua_State* L);
    ~EventBus();   // luaL_unref all active listeners

    // Subscribe. Returns listener handle (0..MAX_LISTENERS-1) or -1 if full.
    int on(const char* eventName, int luaFuncRef);

    // Unsubscribe by handle returned from on().
    void off(const char* eventName, int handle);

    // Emit event to all subscribers. Optional number payload.
    void emit(const char* eventName);
    void emitNumber(const char* eventName, float value);

    // Called on scene deactivation — releases all refs and clears all channels.
    void clear();

private:
    lua_State* L_;
    Channel channels_[MAX_CHANNELS]{};
    size_t channelCount_{0};

    Channel* findOrCreate(const char* name);
    Channel* find(const char* name) const;
};
```

**Why `char[32]` copied names:**
`const char*` pointers sourced from Lua are valid only for the duration of the C function call. After the call returns, the Lua GC may collect the string. Copying into `char[32]` ensures the channel name survives for the scene's lifetime. This matches `LuaStore::StoreSlot::key[64]` precedent exactly.

**Why scene-scoped (not global):**
A global event bus accumulates stale listener `luaRef` handles across scene transitions. When a scene's Lua state is not shared (each C_LuaScript owns its own `LuaScriptSystem`), cross-scene refs would be invalid. Scene-scoped bus eliminates the entire class of cross-scene dangling ref bugs. `clear()` is called in `Scene::deactivate()` — single clean sweep.

**Integration into LuaBindings:**
`EventBus*` is injected via the pointer-to-pointer registry pattern, matching `m_ssm` and `m_activeScene`:

```cpp
// In LuaBindings:
void setEventBus(EventBus* bus) { m_eventBus = bus; }
EventBus* m_eventBus{nullptr};  // non-owning; owned by Scene
```

In Lua registry (stored during `registerAll()`):
```cpp
auto** ebPP = static_cast<EventBus**>(lua_newuserdata(L, sizeof(EventBus*)));
*ebPP = nullptr;  // set later via setEventBus
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_event_bus");
```

**Lua API (`engine.event.*` sub-table):**
```lua
local h = engine.event.on("player_died", function()
    engine.scene.switch(1)
end)

engine.event.emit("player_died")
engine.event.emit_number("score", 42.0)
engine.event.off("player_died", h)
```

**Memory budget:**
16 channels × (32 + 8×(4+1+3 pad) + 8 + 1 + pad) ≈ 16 × ~100 bytes = ~1.6 KB. Acceptable on ESP32 PSRAM.

---

### Persistent Objects

**Problem:** Some objects (player state, audio manager) must survive scene transitions. Currently `ObjectCollection` in each `Scene` destroys all objects when the scene is deactivated/destroyed.

**Pattern: Persistent ObjectCollection on SceneStateMachine**

`SceneStateMachine` already owns `scenes[]` via `unique_ptr`. Add a second `ObjectCollection` that is NOT inside any scene:

```cpp
// Additions to SceneStateMachine:
class SceneStateMachine {
    // ... existing ...

    ObjectCollection persistentObjects;  // ticked every frame regardless of active scene

public:
    template<typename T, typename... Args>
    T* addPersistentObject(Args&&... args) {
        return persistentObjects.addObject<T>(std::forward<Args>(args)...);
    }

    void removePersistentObject(Object* obj) {
        persistentObjects.removeObject(obj);
    }

    void update(float dt) {
        // Persistent objects update FIRST, always
        persistentObjects.update(dt);
        persistentObjects.lateUpdate(dt);

        // ... existing transition + currentScene->update(dt) logic ...
    }
};
```

**Lua-side: `engine.scene.spawn_persistent(name)`**

Returns an `ObjectProxy` userdata (same type as `engine.scene.spawn()`). The object persists across `engine.scene.switch()` calls. `engine.scene.find()` searches persistent collection FIRST, then active scene — consistent lookup regardless of where the object lives.

**`engine.scene.find()` extension:**
```cpp
int LuaBindings::lua_engine_scene_find(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    // Check persistent objects first
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    auto** ssmPP = static_cast<SceneStateMachine**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (ssmPP && *ssmPP) {
        Object* obj = (*ssmPP)->getPersistentObjects().findByName(name);
        if (obj) { /* wrap in ObjectProxy, return */ }
    }

    // Then check active scene
    // ... existing logic ...
}
```

**Why persistent collection on SSM and not a static global:**
A static global would survive hot-reload (F5), meaning objects from a previous Lua session remain live — dangling proxy issue. SSM is the natural parent because it owns all scenes. Resetting SSM (rare) clears persistent objects correctly.

**Persistent object limit:**
`ObjectCollection::MAX_OBJECTS = 128` is inherited. In practice, persistent objects are few (player, audio manager, game config). Consider a separate `MAX_PERSISTENT = 16` compile-time constant to limit SSM SRAM footprint if needed on ESP32.

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `std::map` / `std::unordered_map` | Heap allocation per insert; no guaranteed embedded allocator | Fixed-size struct arrays with `strcmp` lookup |
| `std::vector` | Dynamic heap; not zero-alloc | `std::array<T, N>` with a count member |
| `std::string` for event/state names | Heap allocation, copy overhead | `const char*` string literals; `char[N]` for owned copies |
| Separate `lua_State` per timer or FSM | 20–40 KB per Lua VM; fatal on ESP32 | Borrow `lua_State*` from owning `C_LuaScript` |
| `std::function` for new Lua callback slots | Small-buffer optimization not guaranteed; may heap-allocate for captures | `luaL_ref()` integer handles — zero allocation, GC-safe |
| Global singleton EventBus | Cross-scene listener refs become dangling when scene Lua state is reset | Scene-scoped `EventBus`, cleared in `Scene::deactivate()` |
| `std::chrono` | Not available on ESP32 bare-metal; unnecessary | `float dt` accumulation — already the established pattern |
| `dynamic_cast` in `self:get()` hot path | O(N components) scan on every call | Cache the `Component*` in `ComponentRef` userdata after first `get()` call |
| lightuserdata for ComponentProxy | No metatable support in Lua 5.1 — cannot attach `timer:after()` method table | Full userdata with named metatable per component type |
| `luaL_newlib` | Lua 5.2+ API; not in LuaJIT 2.1 / Lua 5.1 | `lua_newtable` + `lua_pushcfunction` + `lua_setfield` (existing pattern) |
| Storing Lua function pointer as `lua_CFunction` | Cannot store Lua closures from scripts | `luaL_ref(L, LUA_REGISTRYINDEX)` returns stable integer key |

---

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| `luaL_ref` integer handles for callbacks | Upvalue closures | `luaL_ref` centralizes in registry; cheaper per slot; simpler cleanup |
| `strcmp` dispatch for `self:get()` | Type-erased function pointer table registered at startup | Same O(N) at N < 10; `strcmp` table is simpler to maintain and extend |
| Scene-scoped `EventBus` | `Signal<>` on Component for inter-object events | `Signal<>` is C++-typed, not string-keyed; Lua scripts need string-named events |
| Persistent collection on SSM | Fake "persistent scene" that never deactivates | Conceptual overhead; fake scene still participates in scene management |
| `ComponentRef` full userdata | Return raw `Component*` as lightuserdata | lightuserdata has no metatable — cannot attach method table for `timer:after()` |
| `C_StateMachine::pending_` deferred | Immediate transition in `transitionTo()` | Re-entrant FSM corruption during `update()` — same problem `SceneStateMachine::switchTo()` solves |
| Scene-owned EventBus | LuaBindings-owned EventBus | LuaBindings does not have a destructor hook on scene change; Scene does |

---

## Stack Patterns by Variant

**If adding more component types accessible via `self:get()` later:**
- Add one `strcmp` branch to `findComponentByName()`
- Register the component's metatable in `registerAll()`
- No architectural change

**If event payloads beyond a single float are needed:**
- Add `emitString(name, value)` and `emitInt(name, value)` overloads on EventBus
- Do NOT push a Lua table as payload from C — table construction from C is allocation-heavy

**If ESP32 SRAM is critically tight:**
- `C_Timer::MAX_TIMERS`: 8 → 4 (saves ~64 bytes per C_Timer instance)
- `C_StateMachine::MAX_STATES`: 8 → 4 (saves ~100 bytes per instance)
- `EventBus::MAX_CHANNELS`: 16 → 8
- `EventBus::MAX_LISTENERS`: 8 → 4
- All are `constexpr` — one-line change each

**If a script needs a callback-free timer (polling style):**
```lua
-- No C_Timer needed — use engine.time.now() directly:
local start = engine.time.now()
-- in update:
if engine.time.now() - start > 1.5 then ... end
```
`C_Timer` is only needed when the Lua function reference pattern is desired.

---

## C++ Feature Requirements

All features are available in C++17 (existing requirement). No language standard bump needed.

| Feature | Used For | Already Required |
|---------|----------|-----------------|
| `if constexpr` | Pixel type branching in Scene render | Yes |
| `std::array<T, N>` | Fixed slot arrays throughout | Yes |
| `std::is_base_of` | assertRequires<T>, addComponent<T> | Yes |
| `constexpr` members | MAX_TIMERS, MAX_STATES, etc. | Yes |
| `dynamic_cast<T*>` | getComponent<T>() in Object | Yes |
| Template member functions | addComponent, getComponent, assertRequires | Yes |

---

## Integration Points

| New Feature | Integrates With | Integration Method |
|-------------|----------------|-------------------|
| `C_Timer` | `C_LuaScript` | `C_LuaScript` passes `lua_State*` at construction; `C_Timer` added via `addComponent<C_Timer>(luaState)` |
| `C_StateMachine` | `C_LuaScript` | Same `lua_State*` borrow pattern as C_Timer |
| ComponentProxy | `ScriptProxy.__index` | Extend `__index` handler; new metatable per component type registered in `registerAll()` |
| `EventBus` | `Scene`, `LuaBindings` | `Scene` owns instance; `Scene::deactivate()` calls `bus.clear()`; `LuaBindings` receives pointer via `setEventBus()` and stores pointer-to-pointer in Lua registry |
| Persistent objects | `SceneStateMachine` | Add `persistentObjects` member; extend `update()` to tick it; extend `engine.scene.find()` and `engine.scene.spawn_persistent()` bindings |

---

## Memory Budget Estimate (ESP32)

| Addition | Size | Count | Total |
|----------|------|-------|-------|
| `C_Timer` (8 slots) | ~128 bytes | 1–4 per scene | 128–512 bytes |
| `C_StateMachine` (8 states) | ~200 bytes | 1–4 per scene | 200–800 bytes |
| `EventBus` (16 chan × 8 listeners) | ~1.6 KB | 1 per active scene | 1.6 KB |
| SSM persistent collection | ~1 KB (16 unique_ptr slots) | 1 global | 1 KB |
| `ComponentRef` userdata | 8–12 bytes each | GC-managed in Lua heap | counted in Lua budget |
| `luaL_ref` entries in registry | ~32 bytes each (LuaJIT) | per timer/listener | counted in Lua budget |

Total C++ addition: ~3–4 KB (scene-scoped) + ~1 KB (global persistent collection). Within ESP32 PSRAM budget.

---

## Sources

- Direct codebase analysis — `include/enjin2/core/signal.hpp` — `std::function` already present, 16-slot array pattern — HIGH confidence
- Direct codebase analysis — `include/enjin2/core/object.hpp` — `unique_ptr<Component>` ownership, `getComponent<T>()` via dynamic_cast — HIGH confidence
- Direct codebase analysis — `include/enjin2/scripting/bindings.hpp` — `LuaStore` char[64]/char[128] key patterns, `m_ssm` pointer injection — HIGH confidence
- Direct codebase analysis — `src/scripting/bindings_engine.cpp` — `luaL_ref` for string interning in `engine.scene.spawn()`, pointer-to-pointer registry pattern — HIGH confidence
- Direct codebase analysis — `include/enjin2/core/scene_state_machine.hpp` — `switchTo()` deferred transition, `pendingSceneId` pattern — HIGH confidence
- Direct codebase analysis — `include/enjin2/core/object_collection.hpp` — 128-slot `unique_ptr<Object>` array, `addObject<T>()`, `findByName()` — HIGH confidence
- Lua 5.1 reference manual — `luaL_ref`, `lua_rawgeti`, `luaL_unref`, full vs light userdata metatable rules — HIGH confidence (standard API)

---

*Stack research for: enjin2 v1.6 Game Ready — C_Timer, C_StateMachine, ComponentProxy/self:get(), event bus, persistent objects*
*Researched: 2026-02-28*
