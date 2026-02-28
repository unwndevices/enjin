# Feature Research

**Domain:** Embedded/WASM 2D game engine — v1.5 Lua Scripting Foundation
**Researched:** 2026-02-26
**Confidence:** HIGH (code-verified against src/ and include/; reference engine patterns from lua-embedding-design.md cross-checked)

---

## Scope

v1.5 adds Lua scripting power and C++ engine foundations to the engine that shipped in v1.4. All
ten features are specified in PROJECT.md Active requirements. The prior research file
(`.planning/research/FEATURES.md`) covered v1.4; this file covers v1.5 exclusively.

The existing baseline (post v1.4):
- `Component` base with `update(uint16_t deltaTime)` — ms integer, not float seconds
- `Object` with `getComponent<T>()`, `getPosition()`, no name field, no tag array
- `ObjectCollection` with `findObject<T>()` — type-only, no name or tag query
- `Scene` with `onCreate/onActivate/onDeactivate/onUpdate(uint16_t)` — no SSM pointer
- `SceneStateMachine::changeScene(id)` — early-exits silently when `targetScene == currentScene`
- `C_LuaScript` calling `init/update/draw` as raw globals — no `self` argument, dt as ms global
- `LuaBindings` with all functions registered as globals — no `engine.*` namespace table
- `InputState` with `held/justPressed/justReleased/axes` — edge detection already works per frame
- `LuaEngine` with static memory pool and `lua_gc()` accessible in C++ but not exposed to Lua
- No `ScriptErrorPolicy` — error silences the script with no defined behavior

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features any Lua-scriptable engine must provide. Missing these = scripts cannot do basic game logic.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| float dt in update(self, dt) | LÖVE2d, Defold, Godot, Unity all pass dt as float seconds. uint16_t ms forces scripts to divide every frame; breaks portability between 30 fps and 60 fps targets. ESP32 variable timing makes this especially important. | MEDIUM | Pervasive signature change: `Component::update(uint16_t)` → `update(float)`, same for `lateUpdate`, `Scene::onUpdate`, `SceneStateMachine::update`. All call sites change. The change is mechanical but wide. |
| engine.* namespaced global table | Raw globals (`isButtonHeld`, `rectangle`, etc.) pollute the script namespace and offer no discoverability. Every reference engine (Solarus `sol.*`, LÖVE `love.*`, Defold `go.*`) uses a namespace. Scripts need `engine.scene`, `engine.input`, `engine.time`, `engine.lua`, `engine.log` at minimum. | MEDIUM | `luaL_newlib` pattern creates a table. Graphics functions stay as love2d-compatible globals per lua-embedding-design.md P1 (minimum viable API surface). New engine state goes under `engine.*`. |
| Self proxy injection (self as first arg) | A script component that cannot read or write properties of its own Object is a dead end — every non-trivial behavior requires C++ support code. Defold's `self` argument is the defining feature of component scripting. Without it, multiple scripts sharing a state share globals and interfere. | HIGH | `ScriptProxy` userdata with `__index`/`__newindex` mapping field names to C++ component reads/writes. `self.x`, `self.y`, `self.visible`, `self.layer` minimum. Passed as first arg to every callback. This is the highest-complexity individual item in the milestone. |
| Scene self-transition (engine.scene.switch from Lua) | Without this, Lua scripts cannot drive scene flow. The entire reason to have scripted game logic is to react to game events and change state. `SceneStateMachine::changeScene()` currently silently returns true when `targetScene == currentScene` — this prevents a script from re-entering its own scene. | MEDIUM | Inject `SceneStateMachine*` into `Scene` at activation time. Expose as `engine.scene.switch(id)`. The self-transition case needs explicit handling: deactivate then activate the same scene, or expose `engine.scene.restart()` as a named operation. |
| Named object registry (name field + find by name) | `engine.scene.find("enemy_01")` is the canonical way scripts locate other objects. PICO-8 works without this (too small), but every engine with object systems (Defold, Godot, Unity) provides name-based lookup. Without it, scripts can only interact with `self`. | MEDIUM | `name` field (`char[32]` or similar fixed-length — no heap) on `Object`. `std::array`-based name-to-pointer map on `ObjectCollection`, or linear scan on small scene sizes. Linear scan over 128 objects is fast enough; O(n) is acceptable for enjin2 scene sizes. |
| Script error policy (ScriptErrorPolicy enum) | An unhandled Lua error that crashes the engine is catastrophic on embedded targets (reboot required). Every production scripting system has a recovery mode. Defold disables the script and logs once. | LOW | `ScriptErrorPolicy` enum: `Disable` (default, log once, stop calling), `Log` (log every frame, keep calling), `Panic` (platform panic handler). Single field on `C_LuaScript`, checked in every callback dispatch. |

### Differentiators (Competitive Advantage)

Features that go beyond the baseline. Not expected by beginners, but valued by anyone writing real game code.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Tag system on Object (8-slot tag array) | Name lookup finds one object; tags find groups. "find all objects tagged 'enemy'" is a common game query (enemy waves, triggers, collectibles). PICO-8 has no tags; Godot and Unity have them. | MEDIUM | `uint8_t tags[8]` on `Object` (zero-alloc, 8 tags per object). Linear scan over `ObjectCollection` collecting matches into caller-provided array. Expose as `engine.scene.findTagged(tag)` returning an array-like result. Tag values are integers — the script defines meaning. |
| Input event callbacks (on_button_pressed / on_button_released) | Polling is right for held-state logic; edge events are right for actions ("fire once on press"). LÖVE2d has `keypressed`, Defold has `on_input`. Polling-only systems force scripts to track edge state themselves, re-inventing what `InputState::justPressed` already provides in C++. | LOW | Thin wrapper over existing `InputState` edge detection. After `input_advance_frame`, iterate buttons and call the Lua function if it exists. No new C++ state. The function names `on_button_pressed(btn)` and `on_button_released(btn)` mirror Defold's `on_input` pattern. |
| GC control (engine.lua.collect / engine.lua.memory) | ESP32 with 32–254 KB Lua pool will drop frames if the GC fires mid-update. Exposing `lua_gc(L, LUA_GCCOLLECT, 0)` and `lua_gc(L, LUA_GCCOUNT, 0)` lets scripts schedule collection at scene boundaries, not mid-frame. Playdate explicitly documented this problem; Defold recommends object reuse. | LOW | Two Lua C functions wrapping `lua_gc`. No new C++ types. Register under `engine.lua.collect()` and `engine.lua.memory()`. Document: call `collect()` in scene deactivate, not in `update`. |
| Component dependency assertions (requires<T>()) | Calling `owner->getComponent<C_Position>()` in `awake()` and null-checking it is boilerplate that every component author writes. A `requires<T>()` call in `awake()` that asserts at startup makes missing-component bugs visible immediately instead of silently failing at runtime. Unity, Godot, and Bevy all have dependency declaration mechanisms. | LOW | Template method on `Component` base: `template<typename T> void requires()` — calls `owner->getComponent<T>()` and triggers a platform assertion/panic if null. Zero overhead at runtime if the component is present. Embedded-safe: asserting in `awake()` fires before the game loop. |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Global `self` variable (not passed as argument) | "Simpler to just use self like a keyword" | PICO-8 works with one global because there is one script. enjin2 runs multiple `C_LuaScript` instances; a global `self` makes the last-set instance visible to all scripts. Cross-script contamination is silent and hard to debug. | Pass `self` as first argument. This is the Defold convention. It requires `update(self, dt)` not `update(dt)`, which is the trade-off. |
| ComponentProxy / self:get(typename) from Lua | "Scripts should read all sibling components" | The heaviest C++ work in the milestone. Requires a string-to-C++-type registry, safe proxy lifetime management across Lua GC cycles, and careful API design for every component type. PROJECT.md explicitly deferred this to v1.6. | Expose the properties scripts need most (x, y, visible, layer) directly on `self` via `ScriptProxy.__index`. The common case (read position, toggle visibility) does not need `self:get()`. |
| Script-to-script communication (event bus / msg.post) | "Scripts need to talk to each other" | Event bus is a separate subsystem. Synchronous events within a frame are fine; async delivery with routing by URL (Defold-style) is complex and requires careful ordering. PROJECT.md deferred event bus to v1.6. | Use `engine.scene.find("name")` to get a reference to another object, then read/write its properties via the proxy. Direct reference is simpler than message routing at this scale. |
| Shared Lua state across all C_LuaScript instances | "More memory efficient" | Scripts can accidentally clobber each other's globals. The current architecture shares one Lua state (all scripts run in it), but callbacks use per-script function names. Moving to per-instance script isolation is a larger architecture change with no clear benefit at enjin2 scale. | Shared state is the current implementation. Document the naming convention: use unique function names per script file, or wrap script state in a local table. This is the PICO-8 pattern and works at small scale. |
| Persistent objects across scene transitions | "Need player to survive scene change" | Persistent objects require either a separate root-level collection outside scenes or reference-counted ownership — both conflict with the zero-alloc static-array design. PROJECT.md explicitly deferred this to v1.6. | For Tomodachi's use case, re-create the player object in each scene's `onCreate`. State can be stored in compile-time global C++ variables if persistence is needed. |
| Coroutine-based scripted sequences | "Cutscenes, timed events" | `coroutine.wrap` works in Lua 5.x; the issue is the C++ driver loop must yield and resume correctly. This is feasible but adds lifecycle complexity. Not needed for v1.5 scope. | Write sequential logic as state machines using a local `state` variable in the script. This is the PICO-8 idiom and works without coroutines for the immediate use cases. |

---

## Feature Dependencies

```
[float dt (C++ signature change)]
    └──required by──> [update(self, dt) in Lua callbacks]
    └──required by──> [engine.time.delta() returning float]

[Named object registry (name field on Object)]
    └──required by──> [engine.scene.find("name")]

[SceneStateMachine* injected into Scene]
    └──required by──> [engine.scene.switch(id) from Lua]
    └──required by──> [scene self-transition (restart)]

[engine.* global table registration]
    └──required by──> [engine.scene.switch()]
    └──required by──> [engine.input.held() namespace]
    └──required by──> [engine.time.now() / engine.time.delta()]
    └──required by──> [engine.lua.collect() / engine.lua.memory()]
    └──required by──> [engine.log(...)]

[ScriptProxy userdata (self proxy)]
    └──required by──> [self.x / self.y / self.visible / self.layer read+write]
    └──depends on──> [float dt (for update(self, dt) signature)]

[ScriptErrorPolicy on C_LuaScript]
    └──independent of all above, can ship in any order]

[InputState edge detection (already exists)]
    └──enables──> [on_button_pressed / on_button_released callbacks]

[engine.lua.collect / engine.lua.memory]
    └──independent; wraps existing lua_gc C API]

[requires<T>() on Component base]
    └──independent; self-contained template method]
```

### Dependency Notes

- **float dt must ship before self proxy:** The Lua callback signature `update(self, dt)` requires both the proxy (self) and float dt. They should be implemented together in a single phase.
- **engine.* table must ship before engine.scene.switch:** The table registration is the prerequisite for all namespaced sub-tables.
- **Scene self-transition requires SSM injection which requires float dt:** Scene's `onUpdate(uint16_t)` will change to `onUpdate(float)` as part of float dt. Both changes touch the same files; do them together.
- **Error policy is independent:** `ScriptErrorPolicy` touches only `C_LuaScript` and can be implemented in isolation at any phase.
- **Input callbacks are independent:** They wrap the existing `InputState` edge detection that ships in v1.3 and require no other v1.5 features.
- **GC control is independent:** Two `lua_gc` wrappers registered under `engine.lua`. Zero dependencies.
- **requires<T>() is independent:** Template method on `Component` base. Zero dependencies.

---

## MVP Definition

### Launch With (v1.5 — this milestone)

The milestone is all ten features. Each is listed with its reason for being in this milestone
rather than deferred.

- [ ] **Fix onRender Pixel4 bug** — Correctness regression. The `if constexpr` guard in `Scene::render()` blocks the `Pixel4` canvas dispatch. Blocks all rendering tests. Must ship first.
- [ ] **float dt everywhere** — Pervasive signature change. Must ship early because it is a dependency for the callback signature change. Width of change (all update signatures) is why it needs its own phase.
- [ ] **Named objects + tags** — Prerequisite for `engine.scene.find()`. Without name lookup, scripts cannot find other objects, making the engine usable only for single-script demos.
- [ ] **Scene self-transitions** — Prerequisite for `engine.scene.switch()`. Without this, Lua scripts cannot drive scene flow at all.
- [ ] **engine.* global table** — The Lua-side payoff. All prior features are prerequisites for this table to be meaningful.
- [ ] **Self proxy (ScriptProxy)** — The highest-value single feature. Makes scripts 10x more useful by giving them object access.
- [ ] **ScriptErrorPolicy on C_LuaScript** — Robustness. Embedded targets cannot reboot on script error.
- [ ] **Input event callbacks** — Complements existing polling. Defold pattern: `on_button_pressed` for actions, `isButtonHeld` for sustained state.
- [ ] **GC control** — Embedded platform safety valve. `engine.lua.collect()` called at scene transitions prevents mid-frame GC pauses.
- [ ] **Component dependency assertions** — Developer ergonomics. `requires<T>()` catches missing components at startup.

### Add After Validation (v1.x — v1.6 scope, already planned)

- [ ] **ComponentProxy / self:get(typename)** — Deferred to v1.6. Heaviest C++ work. Needs careful design of the type registry and proxy lifetime.
- [ ] **Persistent objects across scenes** — Deferred to v1.6. Requires a root-level collection outside `Scene`. Independent subsystem.
- [ ] **Event bus (engine.emit / engine.on)** — Deferred to v1.6. Separate communication pattern. Synchronous delivery within a frame is the design.
- [ ] **Component signals** — Deferred to v1.6. Separate pattern from event bus.
- [ ] **C_Timer component** — Deferred to v1.6. Standalone new component; not required for v1.5 features.
- [ ] **C_StateMachine component** — Deferred to v1.6. Medium effort standalone.
- [ ] **Integer layer system** — Deferred to v1.6. Independent change.

### Future Consideration (v2+)

- [ ] **Coroutine-based scripted sequences** — Feasible but adds lifecycle complexity. Motivate with a concrete cutscene use case from Tomodachi.
- [ ] **Per-C_LuaScript Lua state isolation** — Memory cost is significant. Only worthwhile if cross-script contamination becomes a real problem in practice.
- [ ] **Script hot-patch (function-level reload)** — Produces dangling references. Explicitly rejected in PROJECT.md. Revisit only if full-state reload proves too slow for iteration on ESP32.

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Fix onRender Pixel4 bug | HIGH (blocks all rendering) | LOW | P1 — do first |
| float dt everywhere | HIGH (enables correct game physics) | MEDIUM (wide change, mechanical) | P1 |
| engine.* global table | HIGH (Lua discoverability) | MEDIUM | P1 |
| Self proxy (ScriptProxy) | HIGH (scripts useless without object access) | HIGH | P1 |
| Named objects + tag system | HIGH (object lookup, group queries) | MEDIUM | P1 |
| Scene self-transitions | HIGH (Lua cannot drive flow without it) | MEDIUM | P1 |
| ScriptErrorPolicy | HIGH (embedded robustness) | LOW | P1 |
| Input event callbacks | MEDIUM (polling already exists; events add comfort) | LOW | P1 |
| GC control | MEDIUM (prevents frame drops on ESP32) | LOW | P1 |
| requires<T>() assertions | MEDIUM (developer ergonomics) | LOW | P1 |

All ten features are P1 for this milestone. None qualify for deferral — the deferred features
are already in v1.6 scope per PROJECT.md.

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Reference Engine Comparison

How each feature maps to patterns from the reference engines in lua-embedding-design.md:

| Feature | PICO-8 | Playdate | LÖVE2d | Defold | Solarus | enjin2 v1.5 approach |
|---------|--------|----------|--------|--------|---------|----------------------|
| float dt | No (30/60fps fixed) | Yes (float) | Yes (float) | Yes (float) | Yes (float) | float seconds — LÖVE convention |
| engine namespace | No (globals only) | `playdate.*` | `love.*` | `go.*` + `msg.*` | `sol.*` | `engine.*` — Solarus/LÖVE pattern |
| self proxy | No (global state) | No (global) | No (tables by convention) | Yes (userdata) | Yes (userdata) | userdata — Defold/Solarus pattern |
| scene switch from script | No scenes | No (manual) | Via library | Via `go.set_parent()` | Via `sol.game:set_map()` | `engine.scene.switch(id)` |
| named object lookup | No | No | Via library | `go.get_id("name")` | Yes | `engine.scene.find("name")` |
| tag system | No | No | Via library | `go.get_id` + groups | No | 8-slot tag array — Unity-inspired |
| error policy | Silent (stops script) | Log + continue | pcall-based | Disable script | Log | ScriptErrorPolicy enum — Defold default |
| input events | Polling only | `playdate.buttonJustPressed` | `keypressed` callback | `on_input(self, action_id, action)` | `on_key_pressed` | `on_button_pressed(btn)` — LÖVE pattern |
| GC control | None (automatic) | `collectgarbage()` via C | `collectgarbage()` exposed | Object reuse recommended | Not documented | `engine.lua.collect()` + `engine.lua.memory()` |
| component deps | No components | No components | No components | Yes (implicit) | No | `requires<T>()` — Unity-inspired |

---

## Existing System Interfaces (Dependency Context)

These are the key existing APIs that v1.5 features must integrate with:

| System | Relevant Interface | v1.5 Change |
|--------|-------------------|-------------|
| `Component` | `update(uint16_t deltaTime)` | Change to `update(float dt)` across all derived classes |
| `Object` | No name, no tags | Add `char name[32]` and `uint8_t tags[8]` |
| `ObjectCollection` | `findObject<T>()` type-only | Add `findByName(const char*)` and `findByTag(uint8_t, Object**, size_t)` |
| `Scene` | `onUpdate(uint16_t)`, no SSM pointer | Add `SceneStateMachine* ssm`, change to `onUpdate(float)` |
| `SceneStateMachine` | `changeScene(id)` returns true when already current | Fix to support self-transition as explicit re-enter |
| `C_LuaScript` | Calls `init/update/draw` as globals, no self, no error policy | Add `ScriptErrorPolicy`, inject `ScriptProxy` as first arg |
| `LuaBindings::registerAll()` | All globals | Add `engine.*` sub-table with `scene`, `input`, `time`, `lua`, `log` sub-tables |
| `InputState` | `held/justPressed/justReleased/axes` — frame-level edge detection | Drive `on_button_pressed/released` callbacks from justPressed/justReleased state |
| `LuaEngine` | `lua_gc()` accessible in C++ | Expose via `engine.lua.collect()` and `engine.lua.memory()` |

---

## Sources

- Direct code inspection: `include/enjin2/core/component.hpp`, `include/enjin2/core/object.hpp`, `include/enjin2/core/object_collection.hpp`, `include/enjin2/core/scene.hpp`, `include/enjin2/core/scene_state_machine.hpp`, `src/components/lua_script.cpp`, `src/scripting/bindings.cpp`
- `project/lua-embedding-design.md` — reference engine survey (PICO-8, Playdate, LÖVE2d, Defold, Solarus) with design principles
- `.planning/PROJECT.md` — authoritative requirements list, out-of-scope decisions, v1.5 Active requirements
- Domain knowledge: Defold component scripting model (self as first arg, on_input callback), LÖVE2d delta-time convention (float seconds), Solarus sol.* table pattern, Unity requires/assertion pattern

---

*Feature research for: enjin2 v1.5 — Lua scripting foundation, float dt, named objects, scene transitions, engine.* table, self proxy, input events, error policy, GC control, component dependency assertions*
*Researched: 2026-02-26*
