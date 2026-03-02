# Architecture

**Analysis Date:** 2026-03-01

## Pattern Overview

**Overall:** Entity Component System (ECS) with Lua scripting bridge

**Key Characteristics:**
- **Object-Component Model**: Each `Object` holds up to 16 statically-allocated `Component` instances
- **Scene-based Lifecycle**: Objects exist within `Scene` instances, managed by `SceneStateMachine`
- **Lua Integration**: Full Lua scripting layer via `LuaScriptSystem` with C++ bindings to engine systems
- **Static Memory**: Embedded-first design—fixed pools, no dynamic allocation in hot paths
- **Phase-driven Development**: Incremental features tracked via `.planning/` phases

## Layers

**Core (Entity Foundation):**
- Purpose: Game object lifecycle and component management
- Location: `include/enjin2/core/`, `src/core/`
- Contains: `Object` (entity container), `Component` (behavior base class), `Scene` (object collection)
- Depends on: Type definitions (`types.hpp`), memory utilities
- Used by: All components, rendering, scripting systems

**Components (Behavior Library):**
- Purpose: Reusable behaviors attached to Objects (rendering, animation, physics state, scripting)
- Location: `include/enjin2/components/`, `src/components/`
- Contains: `C_Position`, `C_Sprite`, `C_Drawable`, `C_Camera`, `C_LuaScript`, `C_Tilemap`, `C_Timer`, `C_StateMachine`, physics components
- Depends on: Core, graphics, scripting (for C_LuaScript)
- Used by: Scenes, host application code

**Graphics (Rendering Backend):**
- Purpose: Low-level pixel manipulation, sprite blitting, text rendering, palette management
- Location: `include/enjin2/graphics/`, `src/graphics/`
- Contains: `Canvas4<W,H>`, `Canvas8<W,H>` (template-based fixed-size buffers), `Sprite`/`SpriteSheet`, text rendering
- Depends on: Type definitions
- Used by: Components (C_Sprite, C_Drawable), rendering pipeline, Lua bindings

**Scripting (Lua Runtime Bridge):**
- Purpose: Execute Lua game code, expose C++ engine as Lua tables/functions
- Location: `include/enjin2/scripting/`, `src/scripting/`
- Contains: `LuaEngine` (Lua state + static memory pool), `LuaBindings` (function registration), bindings_*.cpp modules, `LuaScriptSystem` (combined interface)
- Depends on: Core, graphics, components, LuaJIT
- Used by: C_LuaScript component, host application

**Platform Abstraction:**
- Purpose: Hardware/environment-specific code (SDL for desktop, ESP32 for embedded)
- Location: `src/platform/sdl/`, `include/enjin2/platform/`
- Contains: SDL rendering integration, input polling, platform-specific Lua bindings
- Depends on: Core, graphics, scripting
- Used by: Host application (main loop)

**Input System:**
- Purpose: Button/axis polling with per-frame state tracking
- Location: `include/enjin2/input/`, `src/input/`
- Contains: `InputState` (16 buttons + analog axes), button edge detection (just pressed/released)
- Depends on: Type definitions
- Used by: Lua bindings, C_LuaScript callbacks, host input dispatch

**Utils & Math:**
- Purpose: Helper functions and mathematics (noise, polar coordinates, collision, physics)
- Location: `include/enjin2/utils/`, `include/enjin2/core/math.hpp`, `include/enjin2/core/collision.hpp`, `include/enjin2/core/physics.hpp`
- Contains: Noise generation, polar math, 2D collision detection, stateless physics functions
- Depends on: Type definitions
- Used by: Components, Lua bindings, game code

## Data Flow

**Scene Update Loop:**
```
Host calls: SceneStateMachine::update(dt)
  ↓
  For each Object in current Scene:
    → Object::awake()         [first time only]
    → Object::start()         [before first update]
    → Object::update(dt)      [every frame]
      - Update all enabled components
  ↓
  For each Object in current Scene:
    → Object::lateUpdate(dt)  [after all updates]
      - Sprite animation, camera follow
  ↓
Host calls: SceneStateMachine::render(canvas)
  ↓
  For each Object in current Scene:
    → Collect all C_Drawable components
    → Sort by layer, sort order
    → Apply camera offset (C_Camera screen offset)
    → Render each drawable
```

**Lua Scripting Data Flow:**
```
C_LuaScript component (attached to Object)
  ↓
  On awake: C_LuaScript::initializeScriptSystem()
    - Creates LuaScriptSystem (engine + bindings)
    - Loads script via executeFile() or loadScript()
    - Calls Lua init() function
  ↓
  Each frame: C_LuaScript::update(dt)
    - Fires input callbacks (on_button_pressed, on_button_released)
    - Calls Lua update(self, dt)
      * self = ScriptProxy userdata wrapping this C_LuaScript
      * Lua can call engine.* bindings (scene.find, scene.spawn, graphics functions)
      * Changes to object position, visibility, layer are persisted via ScriptProxy
  ↓
  Each frame: C_LuaScript::draw(canvas)
    - Calls Lua draw(self)
      * Lua receives LuaCanvas (type-erased wrapper over Pixel4 or uint8_t canvas)
      * Can draw primitives, sprites, text via love2d.graphics style API
```

**State Management:**
```
Object
  ├─ awoken flag (first lifecycle call guard)
  ├─ started flag (second lifecycle call guard)
  ├─ active flag (determines if update/render runs)
  ├─ Component[0..15]
  │   ├─ enabled flag
  │   ├─ Lifecycle (awake, start, update, lateUpdate)
  │   └─ Type-specific state
  ├─ Position component (cached pointer for fast lookup)
  ├─ Lua proxy (back-pointer for stale-proxy detection)
  ├─ Name (const char*, caller-owned)
  └─ Tags (up to 8, const char* array, caller-owned)

Scene
  ├─ ObjectCollection (manages 0..N objects)
  ├─ Signals (onCreate, onActivate, onDeactivate, onDestroy)
  └─ Rendering state (camera offset, layer visibility)

SceneStateMachine
  ├─ 32 scenes (pre-allocated array)
  ├─ Current/next scene pointers
  ├─ Transition state (type, duration, progress)
  └─ Deferred transition queue (prevents re-entrancy from update())

LuaBindings
  ├─ Current canvas + input state (set by host before each update)
  ├─ Sprite pool (16 fixed SpriteSheet slots for Lua animations)
  ├─ Asset buffer (64KB for loaded sprite pixel data)
  ├─ Layer canvas pointers (for multi-layer rendering)
  ├─ Font registry (8 GFXfont* slots)
  ├─ Persistent store (16 key-value pairs, JSON serializable)
  ├─ Event bus (scene-scoped pub/sub)
  └─ Game state machine (16 named states with on_enter/on_exit callbacks)
```

## Key Abstractions

**Object:**
- Purpose: Represents a game entity as a container of components
- Examples: `include/enjin2/core/object.hpp`
- Pattern: Static-allocation container with addComponent<T>(), getComponent<T>(), removeComponent<T>()
- Caching: Position component cached for O(1) lookup (frequent rendering bottleneck)

**Component:**
- Purpose: Encapsulates a reusable behavior or data
- Examples: `C_Position`, `C_Sprite`, `C_Drawable`, `C_LuaScript`, `C_Camera`, `C_Timer`
- Pattern: Virtual lifecycle (awake, start, update, lateUpdate) + type-specific methods
- Dependencies: `assertRequires<T>()` method for compile-time checking (fails debug builds, logs once in release)

**Scene:**
- Purpose: Manages a collection of objects with synchronized lifecycle
- Examples: `include/enjin2/core/scene.hpp`
- Pattern: Template methods (onCreate, onActivate, onDeactivate, onUpdate, onRender) override in subclasses
- Signals: Emitted on lifecycle transitions, allows external listeners (e.g., UI updates)

**LuaCanvas:**
- Purpose: Type-erased wrapper over 4-bit or 8-bit canvas for Lua drawing operations
- Examples: `include/enjin2/scripting/bindings.hpp` (class definition)
- Pattern: Holds void* canvasPtr + is4Bit flag, dispatches operations via template specialization in implementation
- Compatibility: Accepts Canvas4<W,H>*, Canvas8<W,H>*, or ICanvas<Pixel4>* abstract interface

**ObjectProxy:**
- Purpose: Lua userdata representing an Object, with stale-pointer safety
- Examples: `include/enjin2/scripting/object_proxy.hpp`
- Pattern: Non-owning pointer + valid flag; Object destructor sets valid=false, Lua __index checks flag first
- Use case: Lua can safely pass objects returned from engine.scene.find() between frames without crashes

**ComponentProxy:**
- Purpose: Lua userdata representing a component (e.g., C_Position), with stale-pointer safety
- Examples: `include/enjin2/scripting/component_proxy.hpp`
- Pattern: Identical to ObjectProxy—non-owning pointer + valid flag
- Lifecycle: Created by self:get() Lua method, invalidated if component is removed or object destroyed

## Entry Points

**Host Application (Desktop/ESP32):**
- Location: `src/platform/sdl/sdl_main.cpp` (SDL example)
- Triggers: Called from main()
- Responsibilities:
  - Initialize platform (SDL, input device)
  - Create SceneStateMachine and scenes
  - Main loop: poll input → update → render
  - Inject InputState and canvas pointers to LuaBindings before each frame

**Object.awake():**
- Location: `include/enjin2/core/object.hpp` (virtual, overridable)
- Triggers: When object is added to scene via addObject<T>()
- Responsibilities: Verify required components exist via assertRequires<T>(), initialize component graph

**Object.start():**
- Location: `include/enjin2/core/object.hpp` (virtual, overridable)
- Triggers: Before first update, after all objects in scene are awoken
- Responsibilities: Initialize state that depends on other objects being fully set up

**Object.update(float dt):**
- Location: `include/enjin2/core/object.hpp` (virtual, overridable)
- Triggers: Every frame, after scene state machines process transitions
- Responsibilities: Game logic, physics simulation, state changes

**C_LuaScript.update(float dt):**
- Location: `include/enjin2/components/lua_script.hpp`
- Triggers: As part of Object::update() lifecycle
- Responsibilities:
  - Fire input edge callbacks (on_button_pressed/released)
  - Call Lua update(self, dt) function
  - Handle Lua errors per ScriptErrorPolicy

**Scene lifecycle methods (onCreate, onActivate, onUpdate, onRender):**
- Location: `include/enjin2/core/scene.hpp` (virtual, overridable)
- Triggers: Scene creation, activation, each frame update, each render
- Responsibilities: Scene-specific setup, background rendering, overlay rendering

## Error Handling

**Strategy:** Defensive layer-by-layer checks with policy-driven fallbacks

**Patterns:**

**Component Dependencies (assertRequires<T>()):**
```cpp
// In C_Sprite::awake() (example)
void awake() {
    assertRequires<C_Position>();  // Fails if Position not present
}
```
- Debug: assert(false) with descriptive message, stack trace identifies component
- Release: printf() once, disable this component, engine continues
- Rationale: Embedded targets cannot crash; prefer degraded functionality

**Lua Errors (C_LuaScript error policy):**
```cpp
enum class ScriptErrorPolicy {
    Disable = 0,  // On error: disable script, log once
    Log     = 1,  // On error: log every frame (debug)
    Panic   = 2   // On error: invoke platform panic (abort/reset)
};
```
- Default: Disable—script disabled on first error, engine continues
- Development: Log—errors printed every frame for iteration
- Critical: Panic—halt immediately for hardware failure scenarios

**Canvas Access:**
- All canvas operations bounds-check pixel coordinates
- Out-of-bounds writes are silently ignored (safe for partial off-screen draws)
- Input indices validated in bindings before passing to graphics layer

**Scene Transitions (Deferred Execution):**
```cpp
void SceneStateMachine::update(float dt) {
    // ... handle active transition ...
    if (currentScene) currentScene->update(dt);

    // Deferred transition AFTER update returns (prevents re-entrancy)
    if (hasPendingTransition) {
        hasPendingTransition = false;
        applyDeferredTransition(pendingSceneId);
    }
}
```
- Scene can call switchTo() safely from onUpdate()
- Transition queued until after update() returns
- Prevents stack corruption from nested scene changes

## Cross-Cutting Concerns

**Logging:**
- Mechanism: printf() to stdout (stdout redirected to log in production)
- When: Component dependency failures (release builds), Lua errors, engine lifecycle events
- Example: `printf("[enjin2] C_Sprite::awake: C_Position dependency missing\n");`

**Validation:**
- Static: Compile-time checks via `std::is_base_of<Component, T>` in templates
- Runtime: assertRequires<T>() for component graphs, bounds-checking in graphics
- Example: `static_assert(std::is_base_of<Component, T>::value, "...")` in Object::addComponent<T>()

**Authentication/Authorization:**
- Not applicable—embedded gaming context, no access control
- Lua scripts run in same address space as engine (full trust model)

**Resource Lifecycle:**
- Memory: Static pools (component array per Object, sprite pool in LuaBindings, fixed canvas buffers)
- Components: Unique_ptr array in Object, auto-destroyed on Object destruction
- Lua State: Owned by LuaScriptSystem, destroyed when script component destroyed
- Scenes: Unique_ptr array in SceneStateMachine, can be removed by removeScene() before active

---

*Architecture analysis: 2026-03-01*
