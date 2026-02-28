# Feature Research

**Domain:** Embedded/WASM 2D game engine — v1.6 Game Ready (C_Timer, C_StateMachine, ComponentProxy, signals/event bus, persistent objects)
**Researched:** 2026-02-28
**Confidence:** HIGH (code-verified against src/ and include/; target game requirements derived from first-principles analysis)

---

## Scope

v1.6 adds five capabilities needed to build complete small games (Arkanoid, physics sandbox, tamagotchi)
purely from Lua on SDL3. The existing v1.5 baseline provides:

- `engine.*` global table: scene.switch, scene.find, input polling, time, log, lua.collect/memory
- `ScriptProxy` full userdata: `self` as first callback arg; `__index`/`__newindex` dispatch to C++ components
- `ObjectProxy` with Object-destructor invalidation; named objects + 8-slot tag system
- `SpriteSheet` with frame animation (Once/Loop/PingPong), 16-slot Lua sprite pool
- `LayerCompositor` with 4 Canvas4 buffers, painter's-order composition
- F5 hot-reload; input edge callbacks; `assertRequires<T>()`; `ScriptErrorPolicy`
- `Signal<Args...>` exists in `core/signal.hpp` (C++-only, 16 connections/signal, no Lua bindings)
- `LuaStore` exists in `bindings.hpp` (per-script key-value store, JSON save/load)
- `engine.collision.*` with AABB, circle, point-in-rect, line-line, reflect bindings
- `engine.random.*` with seed/integer/float bindings
- `engine.store.*` with save/load/exists/delete/clear bindings

What is NOT yet available from Lua scripts:
- No time-based callback scheduling (C_Timer)
- No structured state machine for object behavior (C_StateMachine)
- No way for a script to access sibling C++ components (ComponentProxy / self:get())
- No inter-object event/signal communication from Lua (event bus)
- Objects are owned by scenes; no mechanism to survive a scene transition (persistent objects)

The three target games define the feature acceptance bar:

| Game | Primary Mechanics Needed |
|------|--------------------------|
| Arkanoid | Ball/paddle collision (exists via engine.collision), brick death, level clear detect, lives counter, speed ramp — all stateful, scene-flow driven |
| Physics sandbox | Spawn/destroy objects from Lua (engine.scene.spawn/destroy already shipped), manual update loop for verlet positions, no engine physics needed |
| Tamagotchi | Hunger/happiness decay over time, FSM states (idle/hungry/sleeping/happy/dead), background timers, cross-scene persistence |

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features any game-capable scripting layer must provide. Missing these blocks the three target games
from being buildable purely from Lua.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| C_Timer — delayed one-shot callback | Every game needs "do X after N seconds." Arkanoid: restart delay after ball loss. Tamagotchi: hunger tick every 30s. Without a timer, scripts must manually accumulate time in every update() function, which is error-prone and verbose. LÖVE2d users use hump.timer; Defold uses go.animate / timer.delay. | MEDIUM | C++ component: `float elapsed`, `float duration`, `lua_State*`, Lua function ref in registry, `bool repeat`, `bool active`. `update(dt)`: accumulate elapsed, fire callback when elapsed >= duration, reset or deactivate. Fixed pool of N timers on the component (or one timer per C_Timer component — one-component-per-timer is simpler). |
| C_Timer — repeating interval callback | Tamagotchi needs periodic hunger/happiness decay (every 30s). Arkanoid needs speed ramp every N seconds. Repeating timers are as essential as one-shot. | MEDIUM | Same as above — `bool repeat` flag. On fire: if `repeat`, reset `elapsed = 0` and keep active. Lua API: `self:startTimer(duration, callback, repeat)` or engine-level `engine.timer.after(obj, duration, fn)`. |
| C_StateMachine — named states with enter/exit/update | Tamagotchi is literally a textbook FSM (idle, hungry, sleeping, happy, dead). Arkanoid has game-level states (attract, playing, paused, game_over, victory). Without an FSM abstraction, scripts write ad-hoc `if state == "hungry" then ... end` chains that grow unmanageable. | MEDIUM | C++ component storing current/next state names (`char[32]`, no heap). Lua-side: table of state tables `{ enter=fn, update=fn(dt), exit=fn }`. C++ drives lifecycle: call `enter` on state entry, `update(dt)` every frame, `exit` on departure. `transition(newState)` defers to next frame (mirrors SSM deferred-transition pattern). |
| ComponentProxy / self:get("ComponentType") | Lua scripts cannot read position from C_Position, animation state from C_Sprite, or drawable properties from C_Drawable without this. Arkanoid ball script needs its own position to compute collision responses. Tamagotchi pet script needs to toggle C_Sprite frame sets based on state. Without ComponentProxy, all component data must be manually mirrored into ScriptProxy `__index`, which does not scale. | HIGH | String-to-component type registry (static table, compile-time registered). `self:get("Position")` returns a lightweight proxy userdata wrapping the raw `C_Position*`. Proxy fields (x, y) map to component getters/setters. Must invalidate when owner Object is destroyed (same valid flag pattern as ObjectProxy/ScriptProxy). At minimum: Position, Sprite, Drawable. |
| Lua-addressable event bus (engine.on / engine.emit) | Arkanoid: ball needs to tell the score system "brick destroyed, add 10 points." The score object cannot be passed as a closure capture without a reference. Without an event bus, scripts must `engine.scene.find("score")` every time they emit — which requires every object to be named and is fragile. Defold's `msg.post` is the dominant pattern for this. GDQuest's event bus singleton is the Godot pattern. | MEDIUM | Global named-event registry in LuaBindings. `engine.on("event_name", handler_fn)` registers a Lua callback. `engine.emit("event_name", ...)` calls all registered handlers with the remaining args. Fixed-capacity registry (e.g., 32 events × 8 handlers each = 256 Lua function refs). Must clear on scene switch (or support explicit `engine.off` to prevent leaks). |

### Differentiators (Competitive Advantage)

Features that go beyond what is strictly needed, but materially improve the Lua scripting experience.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Persistent objects across scenes | Tamagotchi pet needs to survive a scene switch from "play" scene back to "main" scene without its stats resetting. Without persistence, everything must be re-encoded in LuaStore on every transition. Persistent objects are the "DontDestroyOnLoad" pattern. | HIGH | Requires an object list outside Scene's ObjectCollection — held by SceneStateMachine (or a new PersistentRegistry). Objects in this list update and render like normal objects but are not destroyed on scene change. Complexity: ownership, rendering pipeline injection, name uniqueness. Alternative: just use LuaStore for state and re-create the visual object each scene. If LuaStore is sufficient for the three target games, this can be deferred. |
| C_Timer — repeat count (fire N times then stop) | Arkanoid: "flash brick 3 times on hit, then remove." Tamagotchi: "play animation once, then return to idle." A repeat count (0 = infinite) avoids the need to track counters manually in Lua callbacks. | LOW | Add `int repeatCount` (0 = infinite) and `int firedCount` to C_Timer. On each fire: increment `firedCount`, deactivate if `firedCount >= repeatCount`. |
| C_StateMachine — transition guards (canTransition callback) | Prevents invalid transitions. Tamagotchi should not enter "eating" from "sleeping." A `canTransition(fromState, toState)` callback lets Lua script reject the transition. | LOW | Optional `canTransition` Lua function per state. If defined, call before applying transition; if returns false, ignore the transition request. |
| engine.on with automatic cleanup on scope exit | Event handlers registered in a script init() survive Lua state reloads if not explicitly removed. Auto-clearing all handlers registered by a specific C_LuaScript on that component's destruction prevents stale callbacks silently firing on dead objects. | MEDIUM | Tag each registration with the registering component's ID. On C_LuaScript destruction, remove all registrations with that tag. Requires component identity (owner pointer or ID). |
| engine.emit with return value collection | Some events need responses ("how many lives do I have?"). If emit returns a table of all handler return values, scripts can implement query-style events. | LOW | After calling all handlers, collect non-nil return values into a table pushed to the Lua stack. Caller decides whether to use them. |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Async/deferred timer delivery (queue timers across frames) | "I want setTimeout semantics" | Deferred callbacks fire at an indeterminate point relative to scene updates. If the object was destroyed between scheduling and firing, the callback accesses a dead object. Frame-synchronous delivery (fire during update) is safe because the owning Object is alive. | Fire all timer callbacks synchronously during C_Timer::update(dt). The callback executes within the same frame's update pass, when the owner object is guaranteed alive. |
| Per-object event namespace (emit to specific object) | "I want to send a message to only one object" | Routing by object identity requires either named events scoped to a target, or a direct method call on a proxy. The complexity of a point-to-point routing system is not justified at enjin2 scale (< 128 objects). | Use `engine.scene.find("name")` to get an ObjectProxy, then call a method on it, or store the proxy as a closure variable. Direct is simpler than routed. |
| Coroutine-based state machine (yield/resume states) | "I want async state sequences without an FSM component" | Lua coroutines work, but the C++ driver loop must correctly yield and resume across frames. If the owner Object is destroyed mid-coroutine, the coroutine cannot be safely resumed. Managing coroutine lifetime (associated with Object lifetime) is non-trivial. | Use C_StateMachine with enter/update/exit callbacks. Each state is an explicit data entry, not a suspended coroutine. This matches the embedded-safe zero-ambiguity philosophy of enjin2. |
| Physics engine integration (Box2D bindings) | "Physics sandbox needs real physics" | Box2D requires dynamic allocation and a velocity/position solver that runs independently of the game loop frame order. The zero-alloc constraint and ESP32 target make a full physics engine incompatible with enjin2's design. | Physics sandbox is "verlet integration from Lua" — manually update position by velocity, clamp to bounds, handle user interactions. Engine.collision.* provides AABB and circle tests. No Box2D needed. |
| Global persistent event subscriptions (survive full Lua reset) | "I want event handlers to survive F5 hot-reload" | Lua function refs in the registry are invalidated by lua_close. If the C++ event bus holds Lua function refs across a Lua state reset, those refs become dangling pointers. | On F5 hot-reload (full Lua state destroy/recreate), re-register all handlers from the new script's init(). The script already handles this by design — init() is always called after reload. |
| Hierarchical state machines (nested states) | "I need substates within states" | Hierarchical FSMs (Harel statecharts) require a stack-based state manager, transition inheritance, and region semantics. This is 3-5x more complex than a flat FSM and not needed for Arkanoid/tamagotchi/sandbox. | Use flat C_StateMachine. If nested logic is needed, create a second C_StateMachine component on the same Object (two state machines running in parallel). |
| self:getAll("ComponentType") returning multiple results | "An object might have multiple C_Sprite instances" | Multiple components of the same type per object is a deliberate enjin2 design point (getComponents<T>()), but returning a Lua table of proxies from getAll() requires temporary heap allocation or a fixed-size array on the Lua stack. | Expose self:get(type) returning the first match. For the known multi-drawable use case, access layer properties via self.layer (ScriptProxy __index). Multiple components of the same type on one object are rare in the three target games. |

---

## Feature Dependencies

```
[C_Timer component]
    └──requires──> [C_LuaScript on same Object] (Lua callback ref lives in same Lua state)
    └──requires──> [float dt in Component::update()] (already shipped in v1.5)
    └──requires──> [ScriptProxy userdata] (self passed to timer callback, already v1.5)

[C_StateMachine component]
    └──requires──> [float dt in Component::update()] (already v1.5)
    └──requires──> [ScriptProxy userdata] (self in state callbacks, already v1.5)
    └──optional──> [C_Timer] (timer-driven transitions are a common pattern)

[ComponentProxy / self:get()]
    └──requires──> [ScriptProxy userdata] (self:get() is a method on ScriptProxy, v1.5)
    └──requires──> [ObjectProxy validity pattern] (proxy must invalidate when C++ object dies, v1.5)
    └──requires──> [Component::getComponent<T>()] (already v1.5, used internally)
    └──enhances──> [C_StateMachine] (state callbacks can read sibling Sprite to change frames)
    └──enhances──> [C_Timer] (timer callbacks can read/write sibling component state)

[engine.on / engine.emit (event bus)]
    └──requires──> [LuaBindings::registerEngineTable()] (add engine.on/emit sub-registrations, v1.5)
    └──independent of──> [C_Timer, C_StateMachine, ComponentProxy]
    └──enhances──> [C_Timer] (timer fires, emits event, other objects react)
    └──conflicts with──> [persistent Lua function refs across scene switch] (refs must clear on switch)

[Persistent objects across scenes]
    └──requires──> [SceneStateMachine] (must hold a parallel object list, v1.5)
    └──requires──> [engine.scene.spawn/destroy] (already shipped in v1.5)
    └──conflicts with──> [ObjectProxy single-proxy-per-object constraint] (documented debt in PROJECT.md)
    └──alternative──> [LuaStore for state + re-create per scene] (zero new C++ work)
```

### Dependency Notes

- **ComponentProxy must come after ScriptProxy:** `self:get()` is a metamethod on the ScriptProxy userdata. The type registry and proxy invalidation pattern are extensions of the v1.5 proxy work, not independent systems.
- **Event bus clear-on-scene-switch is not optional:** Stale Lua function refs from the previous scene firing after a switch is a use-after-free (Lua function ref in a closed Lua state, or ref to a dead Object). Must be scoped to scene lifetime or cleared by scene deactivation.
- **C_Timer and C_StateMachine are mutually independent:** Either can be built first. However, combining them enables "timer-driven state transitions" (enter HUNGRY state after 30s of IDLE), which is the primary tamagotchi mechanic. Build C_Timer first because it has lower complexity.
- **Persistent objects can be approximated without new C++ code:** LuaStore (already in v1.5) serializes state to JSON on scene exit and loads it on scene enter. For the three target games, this is likely sufficient and avoids the architectural complexity of a cross-scene object registry.

---

## Game-Specific Feature Requirements

### Arkanoid

| Mechanic | Feature Required | Already Available |
|----------|-----------------|-------------------|
| Ball/wall collision + reflect | engine.collision.reflect() | YES (v1.5) |
| Brick/ball collision (AABB) | engine.collision.aabb() | YES (v1.5) |
| Brick destroy + score add | engine.emit("brick_hit", points) → score handler | NO — needs event bus |
| Speed ramp (ball gets faster) | Manual velocity scaling in script | YES (pure Lua math) |
| Restart delay after ball loss | engine.timer.after(2.0, fn) | NO — needs C_Timer |
| Game states (attract/play/paused/game_over/victory) | C_StateMachine on a GameManager object | NO — needs C_StateMachine |
| Lives counter persistence | LuaStore or local variable (single scene) | YES (LuaStore) |
| Level clear detection | Count remaining bricks in update | YES (pure Lua logic) |

### Physics Sandbox

| Mechanic | Feature Required | Already Available |
|----------|-----------------|-------------------|
| Spawn objects at click | engine.scene.spawn() | YES (v1.5) |
| Destroy objects | engine.scene.destroy() | YES (v1.5) |
| Verlet position integration | Manual in Lua update(self, dt) | YES (pure Lua math) |
| AABB boundary clamping | engine.collision.aabb() for floor/walls | YES (v1.5) |
| Circle-circle collision | engine.collision.circleCircle() | YES (v1.5) |
| Object selection by click | engine.scene.find() + point-in-rect | YES (v1.5) |
| Object position read/write | self.x / self.y via ScriptProxy | YES (v1.5) |
| Sibling component access (Sprite frame toggle) | self:get("Sprite") | NO — needs ComponentProxy |
| Physics sandbox needs no timers, FSM, or events | — | — |

Physics sandbox is the simplest of the three games for v1.6. The existing v1.5 baseline is nearly
sufficient. ComponentProxy is the only new feature that adds meaningful value.

### Tamagotchi

| Mechanic | Feature Required | Already Available |
|----------|-----------------|-------------------|
| Pet states (idle/hungry/sleeping/happy/dead) | C_StateMachine on Pet object | NO — needs C_StateMachine |
| Hunger decay every 30s | C_Timer repeating with hunger-- callback | NO — needs C_Timer |
| Happiness decay every 45s | C_Timer repeating | NO — needs C_Timer |
| Sleep state entered at 8PM (game time) | C_Timer one-shot or state condition | NO — needs C_Timer |
| Feed action → happiness++ | engine.emit("feed") → pet handler | NO — needs event bus (or direct call) |
| Change sprite on state change | self:get("Sprite"):setFrame(n) | NO — needs ComponentProxy |
| Pet stats survive scene switch (main→menu→main) | LuaStore save/load on scene switch | YES (LuaStore in v1.5) |
| Death detection (hunger == 0) | State machine transition condition | YES (pure Lua in state update) |

Tamagotchi requires all five v1.6 features (C_Timer, C_StateMachine, ComponentProxy for sprite
changes, event bus for user actions, LuaStore already serves persistence). It is the most
feature-complete validation target.

---

## MVP Definition

### Launch With (v1.6 — this milestone)

Listed in implementation order (lower complexity and higher dependency priority first):

- [ ] **C_Timer (one-shot + repeating)** — Lowest complexity new component. Unlocks Arkanoid
      restart delay and tamagotchi need decay. Entry point to the milestone; proves the pattern
      for future components.
- [ ] **engine.on / engine.emit (event bus)** — Medium complexity, independent of C_Timer.
      Unlocks brick-to-score communication in Arkanoid. Can be implemented in LuaBindings as
      a named-event registry with fixed capacity. No new C++ types needed beyond what exists.
- [ ] **C_StateMachine (flat FSM with enter/update/exit)** — Medium complexity. Unlocks
      Arkanoid game-state management and tamagotchi pet states. Depends on C_Timer pattern
      being established first (shared Lua function ref management pattern).
- [ ] **ComponentProxy / self:get("Type")** — Highest complexity. Unlocks sprite-frame
      switching from Lua state callbacks and is the keystone of script-to-component access.
      Deferred to after C_Timer/FSM because it requires a type-registry design decision.
- [ ] **Persistent objects (via LuaStore pattern)** — Re-assess after verifying LuaStore
      covers all three target games. If LuaStore is sufficient, defer the full cross-scene
      object registry to v1.7. If not sufficient, implement the lightweight "SSM-owned object
      list" pattern.

### Add After Validation (v1.x — v1.7 candidates)

- [ ] **C_Timer repeat count (fire N times)** — Useful for animation sequences; not strictly
      needed for the three target games. Add when a concrete use case appears.
- [ ] **C_StateMachine transition guards** — Adds safety; only needed if invalid transitions
      are observed in practice during game development.
- [ ] **engine.on automatic cleanup on component destroy** — Important for correctness at
      scale; lower priority while scene count is small (one scene at a time).
- [ ] **Full cross-scene persistent object registry** — Only if LuaStore + re-create pattern
      proves insufficient for a fourth target game.

### Future Consideration (v2+)

- [ ] **Coroutine-based scripted sequences** — Motivate with a concrete cutscene use case.
      Currently the C_StateMachine + C_Timer combination covers the same patterns without
      coroutine lifetime complexity.
- [ ] **Hierarchical state machines** — No evidence of need from the three target games.
- [ ] **Point-to-point event routing (msg.post style)** — Global broadcast is sufficient
      at enjin2 scale. Named routing adds complexity without clear benefit below ~1000 objects.
- [ ] **Tweening / go.animate equivalent** — Useful for UI animations; requires a curve
      representation and property accessor by name. Combine with C_Timer when motivating
      use case exists.

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| C_Timer (one-shot + repeating) | HIGH (all three games) | MEDIUM | P1 |
| engine.on / engine.emit | HIGH (Arkanoid, tamagotchi decoupling) | MEDIUM | P1 |
| C_StateMachine | HIGH (tamagotchi, Arkanoid game flow) | MEDIUM | P1 |
| ComponentProxy / self:get() | HIGH (sprite switching, position reads) | HIGH | P1 |
| Persistent objects (LuaStore path) | MEDIUM (LuaStore may be sufficient) | LOW | P1 (verify first) |
| Persistent objects (cross-scene registry) | MEDIUM | HIGH | P2 (only if LuaStore insufficient) |
| C_Timer repeat count | LOW (workaround: counter in callback) | LOW | P2 |
| FSM transition guards | LOW (defensive programming) | LOW | P2 |
| engine.on auto-cleanup on component destroy | MEDIUM (correctness) | MEDIUM | P2 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Existing System Integration Points

Each new feature integrates with the following already-shipped systems:

| New Feature | Integrates With | Integration Point |
|-------------|----------------|-------------------|
| C_Timer | `Component::update(float dt)` | Accumulate elapsed in update(), fire Lua callback via callWithProxy pattern |
| C_Timer | `ScriptProxy` (v1.5) | Timer callback receives self as first arg, follows existing callWithProxy convention |
| C_Timer | `LuaEngine` Lua registry | Store Lua function ref via `luaL_ref`; release with `luaL_unref` on stop |
| C_StateMachine | `Component::update(float dt)` | State update(dt) called from C_StateMachine::update() |
| C_StateMachine | `ScriptProxy` (v1.5) | enter/update/exit callbacks follow callWithProxy pattern |
| C_StateMachine | `SceneStateMachine` deferred-transition pattern | State transitions should be deferred (last-wins within a frame) to prevent re-entrancy |
| ComponentProxy | `ScriptProxy.__index` (v1.5) | `self:get("Type")` is a new method dispatched through ScriptProxy metatable |
| ComponentProxy | `Object::getComponent<T>()` (v1.5) | C++ type lookup uses existing dynamic_cast path |
| ComponentProxy | `ObjectProxy` validity pattern (v1.5) | ComponentProxy needs a `valid` flag cleared by Object destructor |
| Event bus | `LuaBindings::registerEngineTable()` (v1.5) | Add `engine.on` and `engine.emit` during registerEngineTable() |
| Event bus | Scene lifecycle (v1.5) | Clear all registered handlers on scene deactivation to prevent stale refs |
| Event bus | `LuaEngine` Lua registry | Store handler function refs with `luaL_ref`; bulk-release on scene switch |
| Persistent objects | `SceneStateMachine` (v1.5) | If full registry: SSM owns a separate object list updated/rendered alongside current scene |
| Persistent objects | `LuaStore` (v1.5) | If LuaStore path: serialize state on scene deactivation, restore on scene activation |
| Persistent objects | `engine.scene.spawn/destroy` (v1.5) | Persistent object creation via existing spawn binding; persistence flag marks it exempt from scene cleanup |

---

## Sources

- Direct code inspection: `include/enjin2/core/component.hpp`, `include/enjin2/core/object.hpp`, `include/enjin2/core/scene.hpp`, `include/enjin2/core/scene_state_machine.hpp`, `include/enjin2/core/signal.hpp`, `include/enjin2/scripting/bindings.hpp`, `include/enjin2/scripting/object_proxy.hpp`, `src/scripting/bindings_engine.cpp`
- `.planning/PROJECT.md` — authoritative requirements list, v1.6 Active requirements, out-of-scope decisions
- `.planning/codebase/ARCHITECTURE.md` — frame update loop, component lifecycle, data flow
- Arkanoid physics patterns: [GameDev.net Arkanoid physics thread](https://www.gamedev.net/forums/topic/372965-arkanoid-physics/), [Smiling Cat physics for block breaker](https://www.smilingcatentertainment.com/physics-for-a-block-breaker-game/), [love2d arkanoid tutorial](https://github.com/noooway/love2d_arkanoid_tutorial)
- Tamagotchi FSM: [Tamagotchi and FSM analysis](https://liamharveysae.wordpress.com/2015/07/16/week-7-tamagotchis-and-the-finite-state-machine/), [Foundations of Python Programming: Tamagotchi](https://runestone.academy/ns/books/published/fopp/Classes/Tamagotchi.html)
- Event bus patterns: [Nomad Game Engine event system](https://medium.com/@savas/nomad-game-engine-part-7-the-event-system-45a809ccb68f), [Game Programming Patterns: Event Queue](https://gameprogrammingpatterns.com/event-queue.html), [GDQuest event bus singleton](https://www.gdquest.com/tutorial/godot/design-patterns/event-bus-singleton/), [LÖVE signals module](https://love2d.org/forums/viewtopic.php?t=80224)
- Persistent objects: [Unity DontDestroyOnLoad guide](https://uhiyama-lab.com/en/notes/unity/unity-dontdestroyonload-guide/), [Persistent Scene pattern](https://rwth-acis.github.io/i5-Toolkit-for-Unity/1.5.0/manual/Utilities/Persistent-Scene.html)
- Timer design: [Unreal Engine gameplay timers](https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/ProgrammingWithCPP/UnrealArchitecture/Timers), [Delta-time accumulator pattern](https://medium.com/@lemapp09/beginning-game-development-implementing-timers-and-delays-with-coroutines-5a93d16d173e)

---

*Feature research for: enjin2 v1.6 — C_Timer, C_StateMachine, ComponentProxy, event bus, persistent objects*
*Researched: 2026-02-28*
