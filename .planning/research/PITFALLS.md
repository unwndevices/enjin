# Pitfalls Research

**Domain:** Adding C_Timer, C_StateMachine, ComponentProxy, signal/event bus, and persistent objects to existing zero-alloc 2D game engine with Lua scripting (enjin2 v1.6 Game Ready)
**Researched:** 2026-02-28
**Confidence:** HIGH (direct codebase analysis of all relevant headers, cross-referenced against existing v1.5 design decisions, and embedded constraints documented in PROJECT.md)

---

## Critical Pitfalls

### Pitfall 1: Timer Callback Fires Into a Destroyed Lua State

**What goes wrong:**
`C_Timer` holds a Lua function reference (via `luaL_ref`) that it calls when the timer expires. If the scene transitions while a timer is still ticking, the scene's `ObjectCollection` destroys all objects (including the one holding `C_Timer`). If `C_Timer` is implemented with any external ownership — a static timer pool, a scene-level scheduler, or a signal connection — there is a window where the timer fires after the owning object is dead but before the timer slot is released.

Even with correct C++ object ownership (timer lives inside the component, component destroyed with object), the Lua function reference stored via `luaL_ref` is allocated in a specific `lua_State*`. After a hot reload (F5), the old `lua_State` is closed and a new one opened. Any `luaL_ref` handles from the old state are invalid in the new state. If `C_Timer` does not null its ref on Lua state teardown, calling `lua_rawgeti` with the stale ref in the new state produces undefined behavior or a silent wrong function call.

**Why it happens:**
`luaL_ref` returns an integer key into the Lua registry. The integer looks valid across states but is not — registry keys are state-local. Developers familiar with integer handles from other systems (file descriptors, object IDs) may not realize the integer is state-coupled. Hot reload is especially dangerous because the C++ `C_Timer` object may survive across reloads if timers are registered at the scene level, while the `lua_State` it was registered against is gone.

**How to avoid:**
- `C_Timer` must store both a `lua_State*` and an `int` ref. In `C_Timer::update()`, check `L == expected_state` before calling `lua_rawgeti`. If they differ (e.g., after reload), the timer is stale — cancel it silently.
- `C_LuaScript` already invalidates its `ScriptProxy` in its destructor before `lua_close`. Apply the same pattern: `C_Timer` should expose a `cancelLuaCallback()` method that the script system calls before tearing down the `lua_State`. This requires `C_Timer` to register itself with the `LuaScriptSystem` on callback registration.
- Simpler alternative: `C_Timer` does not store Lua refs directly. Instead it stores a `const char*` Lua function name (a string literal with script-static lifetime), and calls `lua_getglobal(L, name)` at fire time. This avoids the ref lifetime problem entirely, at the cost of global function name requirement. For anonymous callbacks, the ref approach with teardown protocol is necessary.
- In the destructor of `C_Timer`, call `luaL_unref(L, LUA_REGISTRYINDEX, ref)` to release the reference regardless of whether the timer fired.

**Warning signs:**
- `C_Timer` stores `int ref` without storing `lua_State*`
- No `luaL_unref` call in `C_Timer` destructor
- `C_Timer` fires a callback after F5 reload and produces a wrong function call (calls a function from a previous script)
- `C_LuaScript` teardown sequence does not notify live timers

**Phase to address:**
C_Timer implementation phase — Lua callback registration and teardown protocol must be designed before writing any timer tick logic.

---

### Pitfall 2: Timer Accumulator Drift Under Scene Transitions and Pauses

**What goes wrong:**
`C_Timer` accumulates `elapsed += dt` in `update(float dt)`. When a scene transition occurs mid-countdown, the timer's owning object is destroyed along with the scene — the timer fires never (correct behavior). However, if objects persist across scenes (the persistent-objects feature being added in the same milestone), a timer on a persistent object continues accumulating during the new scene's `update()` calls.

The problem: persistent object timers accumulate `dt` from the new scene's frame budget, which may be different from the old scene's (different update rates, different game states). More subtly, if the game ever suspends (SDL window loses focus, `dt` is clamped or set to zero), the timer's accumulated time does not advance. When the game resumes and `dt` is back to normal, the timer fires as if the suspension did not happen. This may be correct (timer pauses with game) or wrong (timer should be wall-clock based). The engine has no protocol for "timer intent."

A second accumulator issue: floating-point accumulation over many frames. A 60-second timer at 60 Hz is 3,600 frames of `elapsed += dt`. Each frame's `dt` is a float computed from SDL frame timing, which has floating-point rounding error. After 3,600 additions, `elapsed` may be 60.003 or 59.998 seconds. If the timer fires on `elapsed >= duration` (correct approach), it fires at most one frame late — acceptable. If it fires on `elapsed == duration` (incorrect), it never fires.

**Why it happens:**
Timer semantics (real-time vs game-time vs frame-count) are implicit in most game engine designs. The developer assumes "game time" but the game loop defines what that means. Floating-point accumulation is a known issue but often forgotten until tested over long durations.

**How to avoid:**
- Always test `elapsed >= duration`, never `elapsed == duration` — use `>=` throughout, without exception.
- Document explicitly in `C_Timer`'s API: "timer advances with game time (dt). If the owner object is paused (disabled), the timer does not advance. There is no wall-clock mode."
- For persistent timers across scenes: ensure the persistent object's `update(float dt)` is called by the new scene's `ObjectCollection` or by a separate "persistent object manager" that is updated by `SceneStateMachine::update()` before the scene update. The dt source must be consistent.
- Define `C_Timer::pause()` and `C_Timer::resume()` methods. When an object is deactivated (`setActive(false)`), the timer should automatically pause via the existing `isActive()` check in `ObjectCollection::update()` — no extra work needed since disabled objects do not receive `update()` calls.

**Warning signs:**
- `elapsed == duration` comparison anywhere in timer logic
- No documentation on what "game time" means for persistent timers
- Timer on a persistent object receives `dt` from two different scenes with no continuity guarantee
- Timer duration specified as `0.0f` (fires immediately next frame) with no guard against division by zero or immediate re-fire in repeating mode

**Phase to address:**
C_Timer implementation phase — timer accumulator semantics must be established in the interface design before writing tick logic.

---

### Pitfall 3: C_StateMachine Enter/Exit Callbacks Called During Object Construction

**What goes wrong:**
`C_StateMachine` is configured from Lua: the script calls methods like `sm:addState("idle", enter_fn, exit_fn, update_fn)` and then `sm:setState("idle")`. If `setState("idle")` is called inside the Lua `init(self)` callback, the enter function fires during initialization — before the object's other components have received `start()`. If `enter_fn` calls `self:get("C_Sprite")` (via the ComponentProxy being added in the same milestone) to change sprite frame, but `C_Sprite` does not exist yet or has not been started, the call either panics, returns nil, or uses an uninitialized component.

The ordering: `awake()` → `start()` → `init(self)` (Lua). Actually, looking at the codebase: `ObjectCollection::start()` calls `Object::start()` → `Component::start()`. The Lua `init(self)` callback is called in `C_LuaScript::update()` on the first frame (the `hasScript && !initCalled` guard). This means `init(self)` fires during the first `update()` call, AFTER `awake()` and `start()`. So `C_Sprite` IS started by then.

BUT: if the developer calls `engine.scene.spawn()` from within a state machine callback that fires during `update()`, the new object is added to `ObjectCollection` mid-iteration. `ObjectCollection::update()` iterates 0..objectCount — if `addObject()` increments `objectCount` mid-loop, the new object gets an immediate `update()` in the same frame, but without `awake()` or `start()` if initialization has already run.

Looking at `Object::addComponent`: "Call start if object has already been started" — this is handled for components added after start. But `ObjectCollection::addObject` calls `awake()` and `start()` immediately if `initialized` is true. The issue is the mid-iteration object count change: if a callback spawns an object during `ObjectCollection::update()`, the new object at index `objectCount-1` may or may not be within the current loop range depending on whether the loop uses a snapshot count or reads `objectCount` fresh each iteration.

**Why it happens:**
State machines by nature fire callbacks in response to transitions. Those transitions happen during game logic (update calls). The game loop does not have a "safe callback zone" separate from the update zone. This is the same class of problem as the deferred scene transition — enjin2 already solved it for scene transitions via the deferred queue pattern, but that pattern must be extended to cover spawning from state machine callbacks.

**How to avoid:**
- `ObjectCollection::update()` must snapshot `objectCount` at loop entry: `size_t count = objectCount;` then loop `0..count`. Objects added during update are at indices `>= count` and receive their first update on the next frame. `awake()` and `start()` still fire immediately in `addObject()` when `initialized == true` — this is correct and safe.
- State machine `setState()` during `init(self)` (first frame): this is safe because `awake()` and `start()` have already fired. Document this.
- Prohibit (via documentation) calling `engine.scene.spawn()` from state machine callbacks that fire during the update loop. If spawn-during-state-transition is needed, use the same deferred approach as `engine.scene.switch()` — queue the spawn, execute it after the update loop completes.
- `C_StateMachine` itself should not call enter/exit functions during `addState()` — only during explicit `setState()` or `update()`. Document this.

**Warning signs:**
- `ObjectCollection::update()` loop reads `objectCount` fresh each iteration (not snapshotted)
- Spawning objects from inside state machine enter/exit callbacks without a deferred queue
- `C_StateMachine::addState()` triggers enter callback immediately
- No documentation on safe vs unsafe call sites for `setState()`

**Phase to address:**
C_StateMachine implementation phase AND persistent objects phase — deferred spawn queue must be in place before state machine callbacks are tested with spawn.

---

### Pitfall 4: ComponentProxy Returns Stale C++ Pointer After Component Removal

**What goes wrong:**
`ComponentProxy` (or `self:get("C_Sprite")`) returns a pointer to a sibling component on the same object. The component pointer is valid as long as the component exists. But `Object::removeComponent<T>()` can be called at any time (from C++ or Lua). After removal, any Lua-held ComponentProxy that cached the raw pointer is dangling.

Unlike `ObjectProxy` (which has the `valid` flag set by `Object::~Object()`), a ComponentProxy pointing to a removed-but-not-destroyed component has no automatic invalidation. `removeComponent<T>()` calls `components[j] = std::move(...)` which invokes the destructor of the removed component — at that point, any ComponentProxy holding a raw pointer to it now points to freed memory.

Additionally, `Object::removeComponent<T>()` shifts the remaining components in the array. A ComponentProxy holding a pointer to `components[3]` does not need updating — it holds the raw pointer to the `Component` object, not the array slot. The pointed-to `Component` is destroyed (via `unique_ptr` destructor), not moved. So the raw pointer is dangling, not just pointing to a different component.

**Why it happens:**
ComponentProxy is new. The `valid` flag pattern for ObjectProxy is established, but applying it to a per-component level requires either a validity flag in every component (cost: 1 byte per component) or a generation counter scheme. Developers adding ComponentProxy may model it on ObjectProxy and add the `valid` flag to the proxy struct — but then forget to set it to `false` in the component destructor.

The existing `Component` base class has no destructor hook into any proxy system. ObjectProxy works because `Object::~Object()` sets `m_luaProxy->valid = false` — but `Object` has a single proxy pointer (`m_luaProxy`). A component can be proxied from multiple scripts simultaneously, so the single-pointer approach does not generalize cleanly.

**How to avoid:**
- Apply the same `valid` flag pattern: add `ComponentProxy* m_luaProxy` to the `Component` base class (mirroring `Object::m_luaProxy`). In `Component::~Component()`, if `m_luaProxy != nullptr`, set `m_luaProxy->valid = false`. On stale access in Lua, raise `luaL_error` with a clear message.
- Accept the single-proxy-per-component constraint (same as single-proxy-per-object for `ObjectProxy`). Document that holding multiple Lua proxies to the same component is not supported.
- The `ComponentProxy` Lua userdata struct should mirror `ObjectProxy`:
  ```cpp
  struct ComponentProxy {
      Component* component;
      bool valid;
  };
  ```
  And `Component` should add: `ComponentProxy* m_luaProxy = nullptr;`
- `Object::removeComponent<T>()` must null the removed component's `m_luaProxy->valid = false` before destruction. Since the `unique_ptr` destructor handles this only if `Component::~Component()` does it, ensure the virtual destructor chain is correct.

**Warning signs:**
- `ComponentProxy` struct exists but no corresponding `valid` flag field
- `Component` base class has no `m_luaProxy` field
- `Component::~Component()` does not set proxy validity to false
- `Object::removeComponent<T>()` does not include proxy invalidation before destruction
- Lua script holds ComponentProxy across a frame where the component was removed

**Phase to address:**
ComponentProxy phase — design the validity protocol before implementing any `__index`/`__newindex` dispatch.

---

### Pitfall 5: Signal Callbacks Holding Stale Object Pointers Across Scene Transitions

**What goes wrong:**
The existing `Signal<Args...>` class in `signal.hpp` stores `std::function<void(Args...)>` callbacks. These callbacks may be closures that capture `Object*` pointers or `Component*` pointers. When the emitting or receiving object is destroyed (scene transition, object removal), the callback slot in the `Signal` still exists — it is only cleared by `disconnect()` (via `SignalConnection` RAII) or `disconnectAll()`.

If `SignalConnection` is stored in the component as a member (RAII teardown), it will disconnect automatically in the component destructor. But if a Lua script registers a signal handler via `engine.event.on("enemy_died", callback_fn)`, the registration is held in a Lua-side event bus (not a C++ `SignalConnection`). When the registering object is destroyed, the Lua function reference in the event bus becomes a reference to a function in a potentially-dead Lua context. When the event fires, the callback executes with a dangling or expired `self`.

The existing `Signal<Scene*>` signals in `scene.hpp` (`onCreateSignal`, `onActivateSignal`) have MAX_CONNECTIONS=16. If a game registers 17 listeners for a signal, the 17th `connect()` call returns -1 (no space). The caller may not check this return value. The signal silently drops the 17th handler. The game logic that depends on that handler never runs.

**Why it happens:**
The `std::function` in `Signal` is opaque — it captures pointers invisibly. `SignalConnection` RAII works correctly for C++ code, but Lua-registered callbacks have no RAII lifetime. The Lua function reference is held by the event bus, not by the registering object. Lua garbage collection does not help because the bus holds a strong reference (via `luaL_ref`).

**How to avoid:**
- For C++ signal connections from components: always store `SignalConnection` as a member variable of the subscribing component. The RAII destructor disconnects automatically on component destruction. Never call `signal.connect()` and discard the returned `SignalConnection`.
- For Lua-side event bus: implement the bus with Object-scoped registration. `engine.event.on("event", handler)` internally associates the registration with the current `C_LuaScript` component (via the `self` ScriptProxy in scope). When the component is destroyed, automatically `luaL_unref` all its registered handlers. This requires the event bus to hold a list of `(event_name, luaL_ref, owning_component*)` tuples.
- The event bus must use fixed-size static storage: `static constexpr int MAX_LISTENERS = 32; EventRegistration listeners[MAX_LISTENERS];` — zero heap allocation.
- For the `Signal::connect()` return value: log and assert if `connect()` returns -1 (overflow). Do not silently drop callbacks. Increase MAX_CONNECTIONS or require explicit disconnect before registering.

**Warning signs:**
- `signal.connect(lambda)` call not assigned to a `SignalConnection` member variable
- Lua event bus that stores `luaL_ref` without an owning component reference
- No `disconnectAll()` call in scene deactivation for scene-level signals
- `Signal::connect()` return value unchecked — potential silent overflow at MAX_CONNECTIONS=16
- Event bus implemented with `std::vector` (heap allocation)

**Phase to address:**
Signal/event bus phase — design the Lua registration lifetime before implementing `engine.event.on()`; the bus must know which component owns each registration.

---

### Pitfall 6: Persistent Objects Receive Update from Wrong Scene's dt

**What goes wrong:**
Persistent objects survive scene transitions. They must be updated every frame regardless of which scene is active. The current architecture: `SceneStateMachine::update()` calls `currentScene->update(dt)`, which calls `ObjectCollection::update(dt)` for that scene's objects. Persistent objects are by definition NOT in any scene's `ObjectCollection`.

If persistent objects are stored in a separate "persistent collection" owned by `SceneStateMachine`, they must be updated by `SceneStateMachine::update()` before or after the scene update. The order matters: if a persistent object's timer fires and emits a signal, and that signal's handler lives in a scene object, the scene object must already exist and be updated (or not yet updated, depending on order) when the signal fires. This creates a subtle frame-ordering dependency.

A second issue: during a scene transition (between `deactivate()` and `activate()`), there is a brief window where `currentScene` is null (in `completeTransition()`, `currentScene = nextScene` is set, but before `initialize()` and `activate()` are called). If a persistent object fires a signal during this window, any handler that tries to call `engine.scene.find()` will find nothing — the new scene is not yet initialized.

**Why it happens:**
Persistent objects are not a standard ECS feature. Most engines handle them with `DontDestroyOnLoad()` (Unity) semantics, which implicitly places objects in a "persistent scene" that is always active alongside the current scene. Without this model, the developer must define where persistent objects live in the update graph.

**How to avoid:**
- Store persistent objects in a `ObjectCollection m_persistent` member of `SceneStateMachine`. Update them in `SceneStateMachine::update()` AFTER the scene update: persistent objects react to changes made this frame by scene objects.
- During scene transition, flag persistent objects as "transition-safe" by checking `hasPendingTransition` before firing signals that depend on scene objects. This is a documentation requirement, not an enforcement mechanism.
- Provide `engine.scene.persist(name)` to mark a scene object for persistence at transition time. The SSM moves the `unique_ptr` from the scene's `ObjectCollection` to `m_persistent` during `applyDeferredTransition()`. This avoids implementing a separate "persistent scene" concept.
- If `engine.scene.find()` is called from a persistent object's Lua callback during a transition, it must return nil (no active scene) without crashing. The existing null guard on `m_activeScene` in `lua_engine_scene_find` handles this — verify it works for the null-currentScene window.

**Warning signs:**
- Persistent objects stored inside a scene's `ObjectCollection` (they will be destroyed on scene change)
- `SceneStateMachine` does not call `update()` on a persistent collection separate from the current scene
- No null guard for `currentScene` during scene transition in `SceneStateMachine::update()`
- Persistent object signals fire during the transition window before the new scene is initialized
- `engine.scene.persist()` API not implemented — developer must manually track which objects survive

**Phase to address:**
Persistent objects phase — architecture must be decided (separate collection vs "persistent scene" vs move-on-transition) before any implementation begins.

---

### Pitfall 7: ComponentProxy self:get() Conflicts with ScriptProxy __index Dispatch

**What goes wrong:**
The existing `ScriptProxy.__index` handler dispatches property reads to C++ component fields: `self.x` reads from `C_Position`, `self.visible` reads from `C_Drawable`, `self.layer` reads from `C_Drawable`. The new `ComponentProxy` design adds `self:get("C_Timer")` to retrieve a sibling component.

Name collision risk: if a component adds a field called `"get"` to the `ScriptProxy.__index` dispatch table (or if the engine author names a property `"get"` in a future component), it shadows the `self:get()` method. The Lua `__index` handler is a single function — it must check for `"get"` first (method lookup) vs component field name lookup (dispatched to C++ components). The ordering of these checks in `__index` determines which wins.

A second collision: if the script defines a global variable named `get` (or imports a library that does), and then calls `self.get("C_Timer")`, the `__index` lookup finds the script's global `get` before the proxy's `get`. This is standard Lua `__index` semantics — table-level values (from the proxy userdata's metatable) are checked after `rawget` on the userdata itself. But because the proxy is a full userdata with no table part, `rawget(self, "get")` returns nil, so `__index` is always called. The method resolution is then entirely in the `__index` handler.

The real collision: `self.name`, `self.active`, `self.x`, `self.y`, `self.visible`, `self.layer` are all dispatched by the existing `ScriptProxy.__index`. If `self:get("C_Position")` returns a ComponentProxy, and that ComponentProxy also dispatches `.x` and `.y` via its own `__index`, there are now two paths to the same data. Scripts may use either path, and they must return consistent values.

**Why it happens:**
`ScriptProxy` accumulates property dispatches organically as new components are added. Each new component contributes new property names. Without a documented namespace — "ScriptProxy handles object-level properties; ComponentProxy handles component-level methods" — the boundary is unclear and will be violated.

**How to avoid:**
- Define the dispatch hierarchy explicitly before implementation: `ScriptProxy.__index` handles ONLY object-level properties (`name`, `active`, `x`, `y`, `visible`, `layer`) and the `get(name)` method. It does NOT dispatch component-specific methods.
- `self:get("C_Timer")` returns a `ComponentProxy` userdata with its own metatable. Component-specific operations go through the ComponentProxy, not through ScriptProxy.
- Reserve `"get"` as a method name on `ScriptProxy` and document it as a permanent reservation. Future component properties must not use `"get"` as a property name.
- Check for `"get"` as the first case in `ScriptProxy.__index`, before any component property dispatch. This ensures the method always resolves correctly regardless of what component properties exist.
- ComponentProxy's `__index` dispatches only to that specific component's properties. It does NOT re-dispatch to the parent object. `componentProxy.x` raises an error if `C_Timer` has no `x` property.

**Warning signs:**
- `ScriptProxy.__index` dispatches `"get"` to a C++ component property (name collision)
- ComponentProxy properties overlap with ScriptProxy properties (same name, different data source)
- No explicit check for `"get"` as first case in `ScriptProxy.__index`
- Script accessing `self.x` and `componentProxy.x` getting different values without explanation

**Phase to address:**
ComponentProxy phase — define the ScriptProxy vs ComponentProxy namespace contract before implementing any `__index` handler.

---

### Pitfall 8: Static Timer/State Machine Arrays Blow the Stack on ESP32

**What goes wrong:**
`C_Timer` and `C_StateMachine` are components stored inside `Object::components` (a `std::array<std::unique_ptr<Component>, 16>`). Each `unique_ptr` stores a heap-allocated component. The components themselves are on the heap (allocated via `new T(...)` in `addComponent`).

However, the STATIC ARRAY design for things like "all timers in the scene" creates a different problem. If `C_Timer` is implemented with an internal `struct Slot { float elapsed; float duration; int luaRef; bool active; bool repeating; } slots[MAX_TIMERS];` and `MAX_TIMERS = 8`, each `C_Timer` component is 8 * ~20 bytes = 160 bytes on the heap. This is fine.

The problem is if a developer "improves" by making a scene-level timer scheduler: `struct TimerScheduler { TimerSlot slots[MAX_SCENE_TIMERS]; } g_scheduler;` with `MAX_SCENE_TIMERS = 128`. At 20 bytes per slot, that's 2,560 bytes. If placed globally (static storage), it is fine. If placed on the stack (local variable in `main()` or a scene method), it reduces the available stack by 2,560 bytes. ESP32 has 8 KB of stack by default (configurable, but limited). A large static local array can cause a stack overflow that manifests as a corrupt call stack, not a clear "stack overflow" error.

More relevantly for this project: `ObjectCollection` has `std::array<std::unique_ptr<Object>, 128>`. Each `unique_ptr` is 8 bytes. Array total: 1,024 bytes of the `ObjectCollection` struct itself — but the `unique_ptr`s point to heap objects. The `ObjectCollection` is a value member of `Scene`. `Scene` is a value member of `SceneStateMachine::scenes[32]`. Total static storage for scenes: 32 * sizeof(Scene) + 128 objects per scene * heap. sizeof(Scene) includes `ObjectCollection` (1,024 bytes for the pointer array) + signals (each Signal<Scene*> has 16 `std::function` slots — each `std::function` is typically 32–64 bytes). Scene size can exceed 5 KB. 32 scenes * 5 KB = 160 KB — this is the `SceneStateMachine` struct size, which lives on the heap if allocated via `new` or on global storage if declared statically.

**Why it happens:**
Zero-alloc constraint means "no `malloc` / `new`" — static arrays are the tool. Static arrays have size, and the size must fit in available memory. ESP32 PSRAM (if present) can extend available RAM, but the project notes this as a concern. Adding new static arrays to components without accounting for their contribution to the total memory budget is the classic embedded memory bloat pattern.

**How to avoid:**
- Define `MAX_TIMERS_PER_COMPONENT = 4` (not 8 or 16) for `C_Timer`. Four simultaneous timers per object is sufficient for game logic.
- Define `MAX_STATES = 8` for `C_StateMachine`. Document why.
- Define `MAX_SIGNAL_LISTENERS = 8` for the event bus (per event channel). The scene-level `Signal` already uses 16; an event bus channel is lower-traffic.
- Track cumulative static memory: document a memory budget table in the implementation plan. Add each new component's sizeof contribution. Verify the total fits within the ESP32's available RAM (520 KB SRAM for ESP32-S3; 320 KB for base ESP32, plus PSRAM if present).
- All new static arrays go in component member variables (on the heap, since components are heap-allocated), not in scene or SSM level structures.
- Use `sizeof(C_Timer)`, `sizeof(C_StateMachine)` in a compile-time `static_assert` to ensure they fit the expected budget. Example: `static_assert(sizeof(C_Timer) <= 256, "C_Timer too large for embedded targets");`

**Warning signs:**
- `MAX_TIMERS` defined as 16 or higher without a memory budget calculation
- Large static arrays as local variables in methods (stack allocation)
- No `static_assert` on component sizes
- `std::function` used in component member arrays (each `std::function` is 32–64 bytes; use plain `int` luaRef + `const char*` name instead)

**Phase to address:**
ALL phases — memory budget must be established at the start of the v1.6 milestone and verified for each new component type.

---

### Pitfall 9: Persistent Store Survives Hot Reload But Lua Script Expects Fresh State

**What goes wrong:**
The existing `LuaStore` (in `bindings.hpp`) is a per-`LuaBindings` instance. On F5 hot reload, `performReload()` calls `shutdown()` + `initialize()`. The `LuaBindings` object is recreated or its `resetSpritePool()` / `registerAll()` are called on the existing instance. Looking at the SDL runner structure: `LuaBindings` is inside `LuaScriptSystem` inside `C_LuaScript`. On F5, `performReload()` calls `scriptSystem->shutdown()` then creates a new `LuaScriptSystem`. The `LuaStore` inside the old system is destroyed; the new system starts with a fresh `LuaStore` — unless the store was persisted to disk (via `saveToFile`) and reloaded on startup (via `setStorePath` → `loadFromFile`).

This creates an ambiguity: in-memory store data is lost on reload; file-persisted store data survives reload. During development (with hot reload), the script author may not realize the store is persisted to disk. After fixing a bug in the save format, they expect a fresh store but get corrupted data from the previous run. There is no `engine.store.reset()` to clear disk state from Lua.

A second issue: the persistent objects feature adds objects that survive scene transitions. If a persistent object carries state that should reset on game restart (score = 0, health = 3), but the `LuaStore` persists that state to disk, the "persistent" store outlives the "session restart." The developer must explicitly call `engine.store.clear()` at game start to reset session state — but "game start" is ambiguous (first ever launch vs engine restart vs scene reload).

**Why it happens:**
Persistence semantics ("what survives what") are often underspecified. Three survival scopes exist: scene transitions, game restarts, and device restarts. The `LuaStore` currently survives device restarts (if file path is set). Object persistence targets scene transitions only. Without explicit scope documentation, developers conflate them.

**How to avoid:**
- Document the three scopes in the API: `engine.store.*` = file-persistent (survives device restart); persistent objects = scene-persistent (survives scene transitions, not restarts); Lua global variables = frame-local (lost on reload).
- Add `engine.store.reset()` as a Lua API that clears the in-memory store AND deletes the disk file. This allows scripts to implement "new game" semantics from Lua.
- During development, log a warning when `setStorePath` points to a file that already exists and is loaded: "Loaded persistent store from [path] — call engine.store.reset() to clear". This surfaces the persistence to the developer.
- Separate "session state" from "persistent preferences": define a convention where store keys prefixed with `session_` are cleared at game start. This is a documentation/convention recommendation, not an enforcement mechanism.

**Warning signs:**
- Script authors confused about why score carries over after F5 reload (store loaded from disk on `setStorePath`)
- No `engine.store.reset()` API
- `LuaStore` loaded from file in `setStorePath` without logging that existing data was found
- Persistent object state and `LuaStore` state used for the same concept with different survival semantics

**Phase to address:**
Persistent objects phase — define persistence scopes before exposing any persistence API to Lua scripts.

---

### Pitfall 10: Multiple ObjectProxy to the Same Object — Last Caller Wins, Earlier Proxies Go Unnotified

**What goes wrong:**
`Object::setLuaProxy(proxy)` overwrites `m_luaProxy`. If `engine.scene.find("enemy")` is called twice in the same frame or across two different scripts, the second call creates a new `ObjectProxy` userdata, calls `setLuaProxy()` on the same object, and the first proxy's registration is silently overwritten. When the object is destroyed, only the second proxy gets notified (`valid = false`). The first proxy retains `valid = true` and a dangling `Object*`.

This is documented in the existing codebase: "Only one ObjectProxy should be active per Object at a time. If engine.scene.find() is called multiple times for the same Object, the last call overwrites Object::m_luaProxy — the previous proxy is NOT invalidated."

In v1.6, the ComponentProxy feature introduces the same problem at the component level. If two scripts call `self:get("C_Timer")` on the same object, two ComponentProxy userdata objects exist for the same `C_Timer*`. Only the last one is registered via `m_luaProxy`. When `C_Timer` is destroyed, the first ComponentProxy holds a stale `valid = true` with a dangling pointer.

Additionally: the new `engine.scene.spawn()` / `engine.scene.destroy()` APIs (visible in `bindings.hpp`: `lua_engine_scene_spawn`, `lua_engine_scene_destroy`) can create/destroy objects dynamically. Destroy invalidates the registered proxy but not stale proxies from previous `find()` calls.

**Why it happens:**
Single-proxy constraint is an acknowledged existing limitation. Adding ComponentProxy extends the same limitation to every component. Each new proxy type needs the same single-registration discipline, but the base class (`Component`) does not currently enforce it.

**How to avoid:**
- In the v1.6 milestone, enforce the documented constraint at runtime: if `Object::setLuaProxy()` is called when `m_luaProxy != nullptr` (an existing proxy is registered), log a warning: "Multiple ObjectProxy created for object [name] — only the latest proxy will be notified on destruction." This makes the limitation visible during development.
- Apply the same logging to `Component::setLuaProxy()` when added.
- For `ComponentProxy`: documents state "cache `self:get()` result in a local variable in `init()`, do not call `self:get()` every frame." This avoids multiple proxy creation.
- Long-term fix (beyond v1.6): replace single `m_luaProxy` with a fixed-size array of proxy pointers (`ObjectProxy* m_luaProxies[4]`). All registered proxies are notified on destruction. This eliminates the stale proxy problem at the cost of 3 extra pointer-sized fields per object.

**Warning signs:**
- `engine.scene.find()` called every frame inside `update()` without caching
- `self:get("C_Timer")` called every frame without caching the result
- No runtime warning when `setLuaProxy()` overwrites an existing registration
- `engine.scene.destroy()` invalidates the registered proxy but not previously-issued stale proxies

**Phase to address:**
ComponentProxy phase — enforce the single-proxy-per-component constraint with a development-mode warning from the start; document the cache-in-init pattern in the API.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| `C_Timer` stores `int` luaRef without `lua_State*` | Simpler struct | Fires stale callback after hot reload; dangling ref after lua_close | Never — always store L alongside the ref |
| Timer `elapsed == duration` comparison | Reads like math | Never fires due to float equality; silent game logic failure | Never — always use `elapsed >= duration` |
| Event bus with no owning-component tracking | Simple list of handlers | Handlers never removed; fire after owning object destroyed | Never — event registration must track owning component |
| `ComponentProxy` without valid flag | Faster to implement | Dangling pointer crash when component removed mid-session | Never — mirrors ObjectProxy, must have valid flag |
| Scene-level timer scheduler (global pool) | Avoids per-object overhead | Stack/global memory pressure; timers not naturally scoped to owner | Only if per-object timer count insufficient and memory budget confirmed |
| `std::function` in timer slots | Clean API | 32–64 bytes per slot; 8 slots per C_Timer = 256–512 bytes per component | Never — use luaRef (int) + optional const char* name |
| Persistent objects in scene ObjectCollection | Simpler implementation | Destroyed on scene transition (defeats the purpose) | Never |
| `LuaStore` loaded silently without logging | Less noise | Developer confused by state carrying over after reload | Never in debug builds — always log when existing store data is loaded |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| C_Timer + hot reload | luaRef valid from old lua_State; fires into wrong state after F5 | C_Timer::cancelLuaCallback() called by LuaScriptSystem before lua_close; ref freed via luaL_unref |
| C_StateMachine + engine.scene.spawn() | Spawning in state enter callback causes mid-iteration object add | Queue spawn via deferred mechanism; execute after ObjectCollection::update() loop completes |
| ComponentProxy + ScriptProxy.__index | "get" property name conflicts with self:get() method | Check "get" first in __index before any component property dispatch; reserve name permanently |
| ComponentProxy + removeComponent | Raw component pointer dangling after removal | Component::~Component() sets m_luaProxy->valid = false; mirrors Object::~Object() pattern |
| Signal + Lua event bus | std::function captures Object* from dead scene | Bus stores (event, luaRef, owning_component*) tuples; unref all tuples on component destruction |
| Persistent objects + scene transitions | Object in scene ObjectCollection destroyed on switchTo() | Persistent objects in separate SSM-level collection; moved out of scene before deactivate() |
| Persistent objects + engine.scene.find() | find() searches only current scene; persistent object not found | Extend find() to search persistent collection if scene search returns nullptr |
| LuaStore + hot reload | Store loaded from disk on setStorePath; developer expects fresh state | Log warning when existing store data loaded; provide engine.store.reset() API |
| C_StateMachine enter/exit + awake/start ordering | enter fires in init(self) before sibling components started | Components are started before init(self) runs (first update() frame); document this ordering |
| Multiple ObjectProxy + engine.scene.destroy() | Earlier stale proxies not notified on destroy | Log development warning when setLuaProxy() overwrites existing registration; document cache-in-init |
| C_Timer repeating + scene transition | Repeating timer on persistent object fires in new scene context | Timer's luaRef L must match current active lua_State; check at fire time |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| `self:get("C_Timer")` called every frame | O(n) dynamic_cast scan per call * 60 Hz per script | Cache result in Lua local during init(): `local timer = self:get("C_Timer")` | Any target — dynamic_cast is not free even on desktop |
| `engine.scene.find()` in timer callback | O(n) name scan per timer fire | Cache find result in Lua upvalue at registration time | ESP32 with 128 objects at high timer frequency |
| `Signal::emit()` with 16 std::function slots | 16 indirect function calls per emit even when most are empty | Keep MAX_CONNECTIONS low per-signal; use per-channel event bus not monolithic bus | Any target — std::function indirect call overhead |
| Repeating timer at very high frequency (< 1 frame period) | Timer fires multiple times per frame via catch-up loop | Clamp timer minimum duration to 1 frame; document frequency limits | Any target — catch-up loop can starve frame budget |
| LuaStore save on every write | Disk I/O on every engine.store.save() call | Save only on scene transitions or explicit engine.store.save() call; never save in update() | SDL3 desktop with slow disks; ESP32 NVS has write cycle limits |
| C_StateMachine update() calling Lua every frame | Lua pcall overhead per object per frame | Only call Lua state update function if state has an update handler; check for nil function before pcall | High object counts (> 32 state machines active simultaneously) on ESP32 |

---

## "Looks Done But Isn't" Checklist

- [ ] **C_Timer:** `luaL_unref(L, LUA_REGISTRYINDEX, ref)` called in destructor — verify ref is released on component destroy
- [ ] **C_Timer:** Callback stores `lua_State*` alongside `int ref` — check that timer cancels gracefully after hot reload (old L != new L)
- [ ] **C_Timer:** Accumulator uses `>=` not `==` for fire condition — verify `elapsed >= duration` in tick logic
- [ ] **C_Timer:** Repeating mode resets `elapsed -= duration` (not `elapsed = 0`) to avoid drift — verify subtraction-based reset
- [ ] **C_StateMachine:** `ObjectCollection::update()` snapshots `objectCount` at loop entry — grep for `for(size_t i = 0; i < objectCount` (unsafe) vs `size_t count = objectCount; for(size_t i = 0; i < count` (safe)
- [ ] **C_StateMachine:** Enter/exit callbacks documented as "safe to call engine.scene APIs; must not call engine.scene.spawn() directly — use deferred queue"
- [ ] **ComponentProxy:** `Component` base class has `ComponentProxy* m_luaProxy` field — check component.hpp
- [ ] **ComponentProxy:** `Component::~Component()` sets `m_luaProxy->valid = false` — verify virtual destructor chain
- [ ] **ComponentProxy:** `self:get("C_Timer")` documented as "cache result in init(), do not call per frame" — check API documentation
- [ ] **ComponentProxy:** `ScriptProxy.__index` checks `"get"` as first case before component property dispatch — verify ordering in bindings.cpp
- [ ] **Signal/event bus:** `engine.event.on()` registration stores owning component pointer alongside `luaRef` — verify EventRegistration struct includes component ref
- [ ] **Signal/event bus:** All event registrations for a component are unref'd in component destructor — verify cleanup path
- [ ] **Signal/event bus:** `Signal::connect()` return value checked for -1 (overflow) — verify in all C++ connection sites
- [ ] **Persistent objects:** Stored in SSM-level `ObjectCollection`, not in any scene's collection — verify persistence target
- [ ] **Persistent objects:** `engine.scene.find()` searches persistent collection if scene search misses — verify extended search path
- [ ] **Persistent objects:** SSM `m_persistent.update(dt)` called in `SceneStateMachine::update()` after scene update — verify ordering
- [ ] **LuaStore:** `engine.store.reset()` API exists and clears both in-memory and on-disk data — verify API surface
- [ ] **Multiple proxies:** Development warning logged when `Object::setLuaProxy()` overwrites non-null `m_luaProxy` — verify warning present

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Timer fires into dead lua_State after reload | MEDIUM | Add `lua_State*` to timer struct; add L comparison guard in fire() path; add cancelLuaCallback() teardown hook to LuaScriptSystem |
| ComponentProxy dangling after removeComponent | HIGH | Add ComponentProxy* m_luaProxy to Component base; add valid=false in Component::~Component(); migrate all ComponentProxy creation to set component->m_luaProxy |
| Signal callbacks with stale Object* from dead scene | MEDIUM | Add owning component tracking to event bus; add cleanup loop in component destructor; audit all lambda captures in signal connections |
| Persistent objects destroyed on scene transition | MEDIUM | Move persistent collection to SSM level; implement engine.scene.persist() API; migrate existing persistent object creation code |
| ObjectCollection::update() mid-iteration add | LOW | Snapshot objectCount before loop; single-line fix in update(); verify with spawn-during-state-machine-callback test |
| LuaStore state confusion after reload | LOW | Add load-existing-data log warning; add engine.store.reset() API; document three persistence scopes |
| Multiple proxy stale notification | MEDIUM (long-term) | Replace m_luaProxy single pointer with m_luaProxies[4] array; notify all on destruction; remove MAX_OBJECTS constraint on proxies |
| Static array memory overflow on ESP32 | HIGH | Reduce MAX_TIMERS / MAX_STATES to fit budget; add static_assert size checks; measure sizeof each new component type before shipping |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Timer callback fires into dead Lua state | C_Timer phase — design Lua ref teardown protocol first | F5 reload with active timer: no wrong-callback fire in new state |
| Timer accumulator drift | C_Timer phase — use >= comparison, subtraction-based reset | 100-frame timer fires exactly once; 10-frame repeating timer fires 10 times in 100 frames |
| StateMachine enter/exit during construction | C_StateMachine phase — snapshot objectCount in update() loop | Spawn from state enter callback: object initialized correctly next frame |
| ComponentProxy dangling pointer | ComponentProxy phase — add m_luaProxy to Component base class | Remove component while proxy held: Lua access raises error, not C++ crash |
| Signal callbacks with stale Object* | Signal/event bus phase — design bus with owning-component tracking | Object destroyed while event registered: event fires zero times, no crash |
| Persistent objects wrong update source | Persistent objects phase — define SSM-level collection before implementation | Persistent timer on scene-transitioned object: continues ticking correctly |
| ComponentProxy vs ScriptProxy name collision | ComponentProxy phase — define dispatch hierarchy, reserve "get" | `self:get("C_Timer")` works; no component property named "get" exists |
| Static array memory overflow | All phases — establish memory budget before first component | sizeof(C_Timer) + sizeof(C_StateMachine) fits within per-object budget; static_assert passes |
| LuaStore persistence confusion | Persistent objects phase — document three scopes; add reset() API | F5 reload with store path set: load warning appears; engine.store.reset() clears disk file |
| Multiple proxy stale notification | ComponentProxy phase — add development warning on proxy overwrite | engine.scene.find() called twice: warning logged; documented cache-in-init pattern |

---

## Sources

- Codebase analysis: `include/enjin2/core/object.hpp` — `Object::setLuaProxy()`, single `m_luaProxy` field, `Object::removeComponent<T>()` shift logic (2026-02-28)
- Codebase analysis: `include/enjin2/core/component.hpp` — `Component::~Component() = default` (no proxy teardown); `Component::enabled`; `assertRequires<T>()` (2026-02-28)
- Codebase analysis: `include/enjin2/scripting/object_proxy.hpp` — `ObjectProxy { Object* object; bool valid; }` pattern (2026-02-28)
- Codebase analysis: `include/enjin2/scripting/bindings.hpp` — `ScriptProxy { C_LuaScript* component; bool valid; }`, `LuaStore` fixed-size store, `lua_engine_scene_spawn`/`lua_engine_scene_destroy` present (2026-02-28)
- Codebase analysis: `include/enjin2/core/signal.hpp` — `MAX_CONNECTIONS=16`, `std::function<void(Args...)>` slots, `SignalConnection` RAII (2026-02-28)
- Codebase analysis: `include/enjin2/core/scene_state_machine.hpp` — `applyDeferredTransition()`, `switchTo()` deferred queue, `hasPendingTransition` flag (2026-02-28)
- Codebase analysis: `include/enjin2/core/object_collection.hpp` — `update()` loop over `objectCount` (not snapshotted), `addObject()` calls `awake()+start()` when `initialized==true` (2026-02-28)
- Codebase analysis: `include/enjin2/core/scene.hpp` — `Scene` holds `ObjectCollection`; `SceneStateMachine* m_ssm`; no persistent collection (2026-02-28)
- Project context: `PROJECT.md` — Single-proxy-per-object documented constraint; deferred scene transition established; zero-alloc constraint; ScriptErrorPolicy behavior; v1.6 target features list (2026-02-28)
- Lua reference manual 5.x: `luaL_ref` / `luaL_unref` semantics; registry keys are state-local; lua_State lifetime (authoritative)
- Embedded constraints: ESP32-S3 520 KB SRAM; stack default 8 KB; no FPU on base ESP32; `std::function` typical size 32–64 bytes (libstdc++ implementation detail)

---
*Pitfalls research for: Adding C_Timer, C_StateMachine, ComponentProxy, signal/event bus, and persistent objects to existing zero-alloc 2D game engine with Lua scripting (enjin2 v1.6 Game Ready)*
*Researched: 2026-02-28*
