# Requirements: enjin2 v1.5

**Defined:** 2026-02-26
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1.5 Requirements

Requirements for Lua Scripting Foundation milestone. Each maps to roadmap phases.

### Rendering

- [ ] **RENDER-01**: Scene-derived `onRender(ICanvas<Pixel4>&)` override is called during `Scene::render()` when using Pixel4 canvas

### Delta Time

- [ ] **DT-01**: `Object::update()`, `Component::update()`, `Scene::update()`, and `SceneStateMachine` pass `float dt` in seconds (not `uint16_t` milliseconds)
- [ ] **DT-02**: All concrete Component subclasses compile and run with the new `float dt` signature
- [x] **DT-03**: `-Woverride` enabled on all platform builds to catch silent override detachment

### Named Objects

- [x] **OBJ-01**: User can assign a name to an Object at construction or via setter
- [x] **OBJ-02**: User can find an Object by name via `ObjectCollection::findByName()` with O(n) linear scan
- [x] **OBJ-03**: User can add up to 8 tags (string literal pointers) to an Object with zero allocation
- [x] **OBJ-04**: User can find all Objects with a given tag via `ObjectCollection::findAllWithTag()`

### Scene Transitions

- [x] **SCENE-01**: Scene holds a non-owning `SceneStateMachine*` pointer injected at activation time
- [x] **SCENE-02**: A derived scene can request a transition to another scene from its own `onUpdate()`
- [x] **SCENE-03**: Self-transitions (scene switching to itself) reset and reinitialize correctly via deferred execution

### Engine Table

- [x] **ENG-01**: Lua scripts access `engine.scene.switch(id)` to request scene transitions
- [x] **ENG-02**: Lua scripts access `engine.scene.find(name)` to locate named objects (returns proxy or nil)
- [x] **ENG-03**: Lua scripts access `engine.input.held(btn)`, `engine.input.just_pressed(btn)`, `engine.input.just_released(btn)`, `engine.input.axis(n)` for polling
- [x] **ENG-04**: Lua scripts access `engine.time.delta()`, `engine.time.now()`, `engine.time.frame()` for timing
- [x] **ENG-05**: Lua scripts access `engine.log(...)` for platform-safe logging
- [x] **ENG-06**: `engine.*` table is registered before any script loads (module-level access works)

### Self Proxy

- [x] **PROXY-01**: Every Lua callback receives `self` as the first argument: `init(self)`, `update(self, dt)`, `draw(self)`
- [x] **PROXY-02**: Scripts can read/write `self.x`, `self.y`, `self.visible`, `self.layer` mapped to C++ component properties
- [x] **PROXY-03**: ScriptProxy uses a validity mechanism (generation token or flag) to prevent dangling pointer access after Object destruction
- [x] **PROXY-04**: All existing Lua scripts migrated to new `(self, ...)` callback signature atomically

### Error Policy

- [x] **ERR-01**: `C_LuaScript` has a `ScriptErrorPolicy` field with values Disable, Log, Panic
- [x] **ERR-02**: Default Disable policy: on error, script is disabled, logs once, engine continues
- [x] **ERR-03**: Log policy: on error, logs every frame, script keeps running (debug mode)
- [x] **ERR-04**: Panic policy: on error, calls platform panic handler
- [x] **ERR-05**: F5 hot-reload clears error state and re-enables disabled scripts

### Input Events

- [ ] **INPUT-01**: Lua scripts can define `on_button_pressed(btn)` callback, fired on button press edge
- [ ] **INPUT-02**: Lua scripts can define `on_button_released(btn)` callback, fired on button release edge
- [ ] **INPUT-03**: Input event callbacks fire after input polling, before `update()` each frame

### GC Control

- [ ] **GC-01**: Lua scripts access `engine.lua.collect()` for explicit GC step
- [ ] **GC-02**: Lua scripts access `engine.lua.memory()` to query current memory usage in bytes

### Component Assertions

- [ ] **DEP-01**: Component base class provides `assertRequires<T>()` protected template method
- [ ] **DEP-02**: In debug builds, missing dependency triggers an assertion with clear error message naming both components
- [ ] **DEP-03**: In release builds, missing dependency logs once and disables the component (no abort on ESP32)

## v2 Requirements

Deferred to future milestone. Tracked but not in current roadmap.

### Object Query

- **QUERY-01**: Scripts can access sibling components via `self:get(typename)` returning a ComponentProxy
- **QUERY-02**: ComponentProxy maps field reads/writes to C++ component accessors

### Communication

- **COMM-01**: Scripts can emit events via `engine.emit(name, data)`
- **COMM-02**: Scripts can subscribe to events via `engine.on(name, callback)`

### Persistent Objects

- **PERSIST-01**: Objects in a persistent layer survive scene transitions
- **PERSIST-02**: Persistent objects accessible from any scene via `engine.persistent.find(name)`

### Components

- **COMP-01**: C_Timer component with after/every/cancel API (times in float seconds)
- **COMP-02**: C_StateMachine component with addState/addTransition/setState API
- **COMP-03**: Component signals (Signal<> on components for event-driven communication)

### Rendering

- **RLAYER-01**: Render layer changed from enum to int16_t for unlimited Z-ordering granularity

## Out of Scope

| Feature | Reason |
|---------|--------|
| Script isolation (separate Lua states) | Memory overhead too high for embedded; shared state is correct for enjin2 scale |
| Coroutines | Add when game code demands async patterns (cutscenes, sequences) |
| on_axis_changed() callback | Defer until analog input hardware is tested |
| engine.scene.add_object() | Requires object prefab system not yet designed |
| Script loading from binary | BinScript needs investigation; not blocking scripting foundation |
| sol2 / LuaBridge binding libraries | Bypass the static pool allocator; raw C API is correct |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| RENDER-01 | Phase 27 | Pending |
| DT-01 | Phase 28 | Pending |
| DT-02 | Phase 28 | Pending |
| DT-03 | Phase 28 | Complete |
| OBJ-01 | Phase 29 | Complete |
| OBJ-02 | Phase 29 | Complete |
| OBJ-03 | Phase 29 | Complete |
| OBJ-04 | Phase 29 | Complete |
| SCENE-01 | Phase 30 | Complete |
| SCENE-02 | Phase 30 | Complete |
| SCENE-03 | Phase 30 | Complete |
| ENG-01 | Phase 31 | Complete |
| ENG-02 | Phase 31 | Complete |
| ENG-03 | Phase 31 | Complete |
| ENG-04 | Phase 31 | Complete |
| ENG-05 | Phase 31 | Complete |
| ENG-06 | Phase 31 | Complete |
| PROXY-01 | Phase 32 | Complete |
| PROXY-02 | Phase 32 | Complete |
| PROXY-03 | Phase 32 | Complete |
| PROXY-04 | Phase 32 | Complete |
| ERR-01 | Phase 33 | Complete |
| ERR-02 | Phase 33 | Complete |
| ERR-03 | Phase 33 | Complete |
| ERR-04 | Phase 33 | Complete |
| ERR-05 | Phase 33 | Complete |
| INPUT-01 | Phase 34 | Pending |
| INPUT-02 | Phase 34 | Pending |
| INPUT-03 | Phase 34 | Pending |
| GC-01 | Phase 35 | Pending |
| GC-02 | Phase 35 | Pending |
| DEP-01 | Phase 35 | Pending |
| DEP-02 | Phase 35 | Pending |
| DEP-03 | Phase 35 | Pending |

**Coverage:**
- v1.5 requirements: 34 total
- Mapped to phases: 34
- Unmapped: 0

---
*Requirements defined: 2026-02-26*
*Last updated: 2026-02-26 after roadmap creation (Phases 27-35)*
