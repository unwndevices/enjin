# Architecture

**Analysis Date:** 2026-02-28

## Pattern Overview

**Overall:** Entity Component System (ECS) with Scene State Machine and Lua scripting integration

**Key Characteristics:**
- Static allocation throughout (no dynamic memory) for embedded systems
- Component-based architecture where Objects own components at compile-time
- Scene-based organization with explicit lifecycle (onCreate → onActivate → onUpdate → onDeactivate)
- Lua integration as a first-class scripting layer with bidirectional access to engine
- Rendering pipeline based on drawable components with layer-based sorting

## Layers

**Core ECS Layer:**
- Purpose: Entity and component lifecycle management
- Location: `include/enjin2/core/`, `src/core/`
- Contains: Object, Component base classes; object/component lifecycle
- Depends on: types.hpp for basic types
- Used by: All subsystems requiring entities

**Scene Management Layer:**
- Purpose: Organize objects into scenes with lifecycle and state transitions
- Location: `include/enjin2/core/scene.hpp`, `src/core/scene.cpp`
- Contains: Scene base class, SceneStateMachine for transitions
- Depends on: ObjectCollection (fixed-size array manager)
- Used by: Application logic for scene organization

**Graphics Rendering Layer:**
- Purpose: Hardware-agnostic drawing abstraction and rendering pipeline
- Location: `include/enjin2/graphics/`, `src/graphics/`
- Contains: ICanvas<T> interface, Canvas8/Canvas4 implementations, primitives, effects
- Depends on: Core types (Point, Rect, colors)
- Used by: Drawable components, Lua bindings for drawing

**Component System:**
- Purpose: Provide reusable functionality attached to objects
- Location: `include/enjin2/components/`, `src/components/`
- Contains: C_Drawable (base for rendering), C_Sprite (animation), C_Position (transforms), C_LuaScript (scripting)
- Depends on: Core ECS, Graphics
- Used by: Objects to add specific behaviors

**Scripting Layer:**
- Purpose: Provide Lua scripting access to engine features
- Location: `include/enjin2/scripting/`, `src/scripting/`
- Contains: LuaEngine (VM), LuaBindings (API bindings), C_LuaScript (component)
- Depends on: LuaJIT (embedded), all other layers for bindings
- Used by: Game logic scripting, UI scripting

## Data Flow

**Frame Update Loop:**

1. **Input Phase:** Platform layer polls input → InputState struct
2. **Update Phase:**
   - SceneStateMachine::update(dt) calls Scene::update(dt)
   - Scene::update(dt) → ObjectCollection::update(dt) → Object::update(dt) → Component::update(dt)
   - Lua scripts receive update() callbacks via C_LuaScript components
   - LateUpdate phase follows same hierarchy for animations/deferred updates
3. **Rendering Phase:**
   - SceneStateMachine::render(canvas) calls Scene::render(canvas)
   - Scene collects all drawable components from objects
   - Drawables sorted by (buffer_index, sort_order) using shouldDrawBefore()
   - Scene renders sorted drawables in order via C_Drawable::draw(canvas)
   - Lua scripts can draw via C_LuaScript::draw() with love2d.graphics-style API

**State Management:**
- Objects are created via Scene::addObject<T>() → stored in ObjectCollection
- Scene lifecycle: initialize() → activate() → update/render → deactivate() → destroy
- Objects track: awoken, started, active flags to control when callbacks fire
- Components depend on owner Object, accessed via getComponent<T>()

## Key Abstractions

**Object (Entity):**
- Purpose: Container for components with lifecycle callbacks
- Examples: `include/enjin2/core/object.hpp`
- Pattern: Automatically adds C_Position; max 16 components per object (static array)
- Lifecycle: awake() → start() → update()/lateUpdate() → destroyed

**Component:**
- Purpose: Reusable behavior attached to objects
- Examples: `C_Drawable`, `C_Sprite`, `C_LuaScript`, `C_Position`
- Pattern: Virtual callbacks (awake, start, update, lateUpdate, onEnable, onDisable)
- Dependencies: Can require sibling components via assertRequires<T>()

**Scene:**
- Purpose: Organize objects and manage rendering
- Examples: `include/enjin2/core/scene.hpp`
- Pattern: Objects collected in ObjectCollection (max 128); explicit lifecycle phases
- Rendering: Collects C_Drawable components, sorts by layer, renders in order

**SceneStateMachine:**
- Purpose: Manage scene transitions with optional visual effects
- Examples: `include/enjin2/core/scene_state_machine.hpp`
- Pattern: Deferred transitions (switchTo() queued after frame update) prevent re-entrancy
- Transitions: IMMEDIATE, FADE_OUT_IN, SLIDE_* with progress signals

**Canvas (ICanvas<T>):**
- Purpose: Hardware-independent drawing interface
- Examples: `include/enjin2/graphics/canvas.hpp`; Canvas4/Canvas8 implementations
- Pattern: Template-based for 4-bit (Pixel4) or 8-bit (uint8_t) pixels
- Bounds checking: inBounds() guards all pixel operations

**Drawable Component (C_Drawable):**
- Purpose: Base for all renderable components
- Examples: `include/enjin2/components/drawable.hpp`; C_Sprite, C_LuaScript extend
- Pattern: Pure virtual draw(ICanvas<Pixel4>& canvas); manages layer, blend mode, anchor
- Rendering sort: Layer buffer index primary, then sort order within layer

**Lua Proxy (ObjectProxy):**
- Purpose: Safe Lua reference to C++ Object with validity tracking
- Examples: `include/enjin2/scripting/object_proxy.hpp`
- Pattern: Object sets m_luaProxy pointer; destructor sets valid=false
- Usage: Lua receives ObjectProxy userdata from engine.scene.find(); queries fail if invalid

**LuaBindings:**
- Purpose: Register all Lua global functions and tables
- Examples: `include/enjin2/scripting/bindings.hpp`
- Pattern: Registered during LuaScriptSystem initialization; state persists across script reloads
- Subsystem bindings: engine.scene.*, engine.time.*, engine.input.*, drawing primitives

## Entry Points

**Main Loop (Host Application):**
- Location: Host creates Canvas, Scene, starts loop
- Triggers: application main() or embedded system timer
- Responsibilities: Calls ssm.update(dt) → ssm.render(canvas) each frame

**Scene Initialization:**
- Location: `include/enjin2/core/scene.hpp` Scene::initialize()
- Triggers: SceneStateMachine::changeScene() or explicit Scene::activate()
- Responsibilities: Calls onCreate(), initializes ObjectCollection, fires onCreateSignal

**Object Creation:**
- Location: `Scene::addObject<T>()` template method
- Triggers: Application calls during Scene::onCreate() or runtime
- Responsibilities: Creates Object, auto-adds C_Position, stores in ObjectCollection

**Rendering:**
- Location: `Scene::render<PixelType>(ICanvas<PixelType>& canvas)`
- Triggers: SceneStateMachine::render() each frame
- Responsibilities: Collects drawables, sorts by layer, calls draw() on each

**Lua Script Execution:**
- Location: `C_LuaScript::update/draw()` in component
- Triggers: Object lifecycle (Scene::update → Object::update → Component::update)
- Responsibilities: Calls Lua update(self, dt) and draw(self, canvas) with script access to engine.*

## Error Handling

**Strategy:** Graceful degradation with logging; embedded systems use printf over exceptions

**Patterns:**

- **Component Dependencies:** assertRequires<T>() fails loudly (assert in debug, disables component in release)
  - File: `include/enjin2/core/component.hpp`

- **Object/Component Limits:** Exceed static limits (16 components/object, 128 objects/scene) returns nullptr
  - Files: `include/enjin2/core/object.hpp`, `include/enjin2/core/object_collection.hpp`

- **Lua Errors:** ScriptErrorPolicy controls behavior
  - Disable (default): Script disabled, error logged once
  - Log: Error logged every frame (debug mode)
  - Panic: Platform panic handler invoked
  - File: `include/enjin2/components/lua_script.hpp`

- **Invalid Lua Proxy:** ObjectProxy::valid flag prevents use-after-free
  - Lua receives ObjectProxy from engine.scene.find(); Object destructor sets valid=false
  - Subsequent Lua access raises error instead of crash
  - Files: `include/enjin2/scripting/object_proxy.hpp`, `include/enjin2/core/object.hpp`

- **Sprite Loading:** Returns false on .njn binary format errors; silently skips bad frames
  - Files: `src/scripting/bindings_sprite_load.cpp`

## Cross-Cutting Concerns

**Logging:**
- Approach: printf-based; no C++ exceptions for embedded compatibility
- Used by: Error messages, debug info, asset loading

**Validation:**
- Component dependencies: assertRequires<T>() from Component base
- Canvas bounds: inBounds() called before pixel access
- Lua proxy validity: ObjectProxy::valid checked before dereference

**Input Handling:**
- Platform abstraction in `src/platform/sdl/` and platform-specific code
- InputState struct polled by platform layer, injected into Lua via setInput()
- Input callbacks: C_LuaScript dispatches on_button_pressed/on_button_released per frame

**Memory Management:**
- Static allocation: Fixed-size arrays throughout (no malloc after initialization)
- Ownership: unique_ptr<> for scene management (SceneStateMachine owns scenes)
- Object lifetime: ObjectCollection owns objects; removeObject() calls destructor
- Lua memory: Embedded LuaJIT with fixed 256KB pool; custom allocator with static buffer

---

*Architecture analysis: 2026-02-28*
