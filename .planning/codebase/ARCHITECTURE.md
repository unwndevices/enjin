# Architecture

**Analysis Date:** 2026-02-27

## Pattern Overview

**Overall:** ECS-like component system with scene management and embedded Lua scripting layer

**Key Characteristics:**
- Static memory allocation throughout (no dynamic allocation on heap)
- Fixed-size arrays for Objects (16 components max), Scenes (32 max), and Drawables (256 max per scene)
- Two-layer lifecycle: Object/Component awake→start→update→lateUpdate, Scene initialize→activate→update
- Deferred transitions prevent re-entrancy during scene updates
- Type-erased LuaCanvas bridge for 4-bit/8-bit canvas variants
- ScriptProxy userdata provides safe Lua access to C_LuaScript components with validity checking

## Layers

**Core Entity Layer (Object/Component):**
- Purpose: Fundamental entity-component system for game objects
- Location: `include/enjin2/core/object.hpp`, `include/enjin2/core/component.hpp`
- Contains: Object base class managing up to 16 components, Component base class with lifecycle hooks
- Depends on: types.hpp for Point, Rect types
- Used by: Scene, C_LuaScript, all drawable/non-drawable components

**Scene Management Layer (Scene/SSM):**
- Purpose: Manages object collections, lifecycle, rendering, and state machine-driven transitions
- Location: `include/enjin2/core/scene.hpp`, `include/enjin2/core/scene_state_machine.hpp`
- Contains: Scene class holding ObjectCollection; SceneStateMachine managing up to 32 scenes
- Depends on: Object, ObjectCollection, Signal for event binding
- Used by: Application layer, Lua bindings (engine.scene.*)

**Component Library Layer:**
- Purpose: Concrete component implementations providing functionality
- Location: `include/enjin2/components/` (drawable.hpp, position.hpp, lua_script.hpp, sprite.hpp, animation.hpp, etc.)
- Contains: C_Drawable (base for visual components), C_Position (positioning), C_LuaScript (script execution), domain-specific (C_Sprite, C_Animation, C_Canvas, etc.)
- Depends on: Component, Canvas, Graphics, Scripting layers
- Used by: User code creating Objects with specific functionality

**Scripting Layer (Lua Integration):**
- Purpose: Embedded Lua execution with love2d.graphics-style API and engine.* global table
- Location: `include/enjin2/scripting/lua_engine.hpp`, `include/enjin2/scripting/bindings.hpp`, `src/scripting/bindings_*.cpp`
- Contains: LuaEngine (Lua state management), LuaBindings (C++ function registration), LuaCanvas (type-erased canvas wrapper), ScriptProxy (userdata for C_LuaScript), EngineTimeState (time injection)
- Depends on: LuaJIT, Canvas, Object, Scene, SceneStateMachine, Input, collision functions
- Used by: C_LuaScript component, standalone Lua applications

**Graphics Layer:**
- Purpose: Pixel drawing, layering, effects, and composition
- Location: `include/enjin2/graphics/canvas.hpp`, `include/enjin2/graphics/primitives.hpp`, `include/enjin2/graphics/layer_compositor.hpp`
- Contains: ICanvas interface (templated), Pixel4/uint8_t specialized implementations, Primitives4/Primitives8 for shapes, LayerCompositor for multi-layer rendering
- Depends on: types.hpp, effects.hpp
- Used by: Lua bindings, drawable components, rendering pipeline

**Input Layer:**
- Purpose: Platform-agnostic input state capture and querying
- Location: `include/enjin2/input/input_state.hpp`, `src/input/input.cpp`
- Contains: InputState (button state, axis values), input_platform_poll() (platform hook)
- Depends on: core types
- Used by: Application update loop, Lua input bindings

**Platform Layer:**
- Purpose: Platform-specific initialization and integration points
- Location: `src/platform/sdl/sdl_main.cpp`
- Contains: SDL initialization, platform hooks for rendering and input
- Depends on: SDL3, Canvas, SceneStateMachine
- Used by: Standalone SDL applications

## Data Flow

**Initialization → Update → Render Cycle:**

1. **Initialization Phase:**
   - Application creates SceneStateMachine
   - Application calls ssm.addScene<MyScene>(sceneId)
   - Application calls ssm.changeScene(sceneId) to activate first scene
   - SceneStateMachine calls Scene::initialize() → Scene::onCreate() → Scene::activate() → Scene::onActivate()
   - Scene::activate() calls ObjectCollection::start()
   - ObjectCollection::start() calls Object::awake() then Object::start() on all objects
   - Each Object::awake() calls all Component::awake() methods
   - Each Object::start() calls all Component::start() methods (order: add order)

2. **Main Update Loop (per frame):**
   - Application calls ssm.update(dt)
   - SceneStateMachine updates active scene: currentScene→update(dt)
   - Scene::update(dt) calls onUpdate(dt), then ObjectCollection::update(dt), then ObjectCollection::lateUpdate(dt)
   - ObjectCollection::update(dt) calls Object::update(dt) on active objects
   - Object::update(dt) calls Component::update(dt) on enabled components
   - ObjectCollection::lateUpdate(dt) calls Object::lateUpdate(dt) for position correction/sorting
   - **Deferred Transitions:** If switchTo() was called during scene update, it queues pendingSceneId and fires applyDeferredTransition() after update completes (prevents re-entrancy)

3. **Rendering Phase:**
   - Application calls ssm.render(canvas)
   - SceneStateMachine delegates to Scene::render(canvas) if no transition active
   - Scene::render(canvas) calls:
     - onRender(canvas) (scene-specific background/overlay)
     - Collects all drawable components from all objects
     - Sorts by (buffer_index, sort_order) using C_Drawable::shouldDrawBefore()
     - Calls draw(canvas) on each drawable in sorted order
   - C_Drawable subclasses implement draw() to render using canvas primitives

**Component Lifecycle Calls:**
```
Object Creation:
  Object::constructor → auto-adds C_Position → addComponent<T>()
      ↓ (if already awoken)
    T::awake()
      ↓ (if already started)
    T::start()

Component Access Pattern:
  object→getComponent<T>() → dynamic_cast in loop (O(n) but n≤16)
  object→getPosition() → cached pointer (O(1))
  object→getDrawables() → cached array (O(1) access)
```

**State Management:**
- Objects track: awoken, started, active flags
- Components track: enabled flag
- Scenes track: initialized, active flags
- SceneStateMachine tracks: currentScene, nextScene, transitionState, transitionProgress

## Key Abstractions

**Object (Composite Container):**
- Purpose: Represents a game entity holding multiple components
- Examples: `include/enjin2/core/object.hpp` (base), user subclasses
- Pattern: Composite pattern with fixed array (std::array<std::unique_ptr<Component>, MAX_COMPONENTS>)
- Component caching: position cached as C_Position* (O(1)); drawables cached as array (O(1) iteration)

**Component (Mixin/Role):**
- Purpose: Provides a single capability to an Object
- Examples: `include/enjin2/components/position.hpp`, `drawable.hpp`, `lua_script.hpp`, `sprite.hpp`, `animation.hpp`
- Pattern: Base class with virtual lifecycle hooks (awake, start, update, lateUpdate); enabled flag for runtime control
- assertRequires<T>() method: declares component dependencies; asserts in debug, disables component in release

**Scene (Level/State Container):**
- Purpose: Manages a collection of Objects and provides scene-specific logic
- Examples: `include/enjin2/core/scene.hpp` (base), user subclasses override onCreate/onActivate/onUpdate/onRender
- Pattern: Container with ObjectCollection; signal emitters for lifecycle events
- Name/Tag System: Objects have optional const char* name and up to 8 tags (zero-allocation); findByName() and findAllWithTag() for queries

**SceneStateMachine (State Controller):**
- Purpose: Manages multiple scenes with transitions and deferred switching
- Location: `include/enjin2/core/scene_state_machine.hpp`
- Pattern: State machine with implicit current/next scene; changeScene() for immediate or transitioned switches; switchTo() for deferred (re-entrant-safe)
- Transition Types: IMMEDIATE, FADE_OUT_IN, SLIDE_LEFT/RIGHT/UP/DOWN with progress callbacks

**LuaCanvas (Type Erasure Bridge):**
- Purpose: Wraps 4-bit (Pixel4) and 8-bit (uint8_t) canvas variants for Lua bindings
- Location: `include/enjin2/scripting/bindings.hpp`
- Pattern: Stores void* + bool is4Bit flag; methods dispatch via if(is4Bit) type checking
- Used by: Lua drawing functions, C_LuaScript component

**ScriptProxy (Userdata Safety):**
- Purpose: Provides Lua access to C_LuaScript component with dangling-pointer protection
- Location: `include/enjin2/scripting/bindings.hpp` (struct ScriptProxy)
- Pattern: {C_LuaScript* component, bool valid} — valid set to false by C_LuaScript destructor before lua_close()
- Access Pattern: Lua metamethods check `if (!proxy.valid) { error("invalid") }` before dereferencing

**EngineTimeState (Injection Container):**
- Purpose: Passes frame timing to Lua bindings without global variables
- Location: `include/enjin2/scripting/bindings.hpp` (struct EngineTimeState)
- Pattern: {dt, totalTime, frameCount} set by host via LuaBindings::setTimeState() before update

## Entry Points

**Scene Lifecycle (User-Overridden Virtuals):**
- Location: `include/enjin2/core/scene.hpp` (protected virtual methods)
- `onCreate()`: Called during Scene::initialize(); create initial objects here
- `onActivate()`: Called when scene becomes active (resume animations, restart processes)
- `onDeactivate()`: Called when scene loses focus (pause, cleanup)
- `onUpdate(float dt)`: Per-frame scene logic before object updates
- `onRender(canvas)`: Scene-specific rendering (backgrounds, UI overlays)

**Object/Component Lifecycle:**
- Location: `include/enjin2/core/object.hpp`, `include/enjin2/core/component.hpp`
- Object::awake(): Ensures required components exist (call assertRequires in components)
- Object::start(): Initialize inter-object dependencies
- Object::update(float dt): Per-frame logic
- Object::lateUpdate(float dt): Position correction after all updates

**Lua Script Entry Points:**
- Location: `include/enjin2/components/lua_script.hpp` (C_LuaScript)
- Lua functions called by C_LuaScript:
  - `init()`: Called once on script load (setup)
  - `update(self, dt)`: Called every frame (self = ScriptProxy userdata)
  - `draw(self)`: Called during rendering (self = ScriptProxy userdata)
- Lua globals provided by LuaBindings:
  - `engine.scene.switch(sceneId)`: Deferred scene transition
  - `engine.scene.find(name)`: Find object by name in active scene
  - `engine.input.held/justPressed/justReleased/axis()`: Input polling
  - `engine.time.delta/now/frame()`: Frame timing
  - `engine.log(msg)`: Console logging
  - Drawing: `love.graphics.* style (setColor, rectangle, circle, text, etc.)`

**Input Event Callbacks:**
- Location: `include/enjin2/components/` (components can override Component lifecycle to respond to input)
- Pattern: Application calls input_platform_poll() to populate InputState; components query via LuaBindings or direct InputState access

## Error Handling

**Strategy:** Fail-safe with component disabling and logging

**Patterns:**

- **Component Dependency Failure (assertRequires):** Debug build asserts and stack trace; release build logs once and disables component via setEnabled(false)
  - Location: `include/enjin2/core/component.hpp` (assertRequires<T>() template method)

- **Lua Script Errors:** Controlled by ScriptErrorPolicy enum
  - `Disable`: Log once, disable component, continue (default, safe for embedded)
  - `Log`: Log every frame, script keeps running (debug mode)
  - `Panic`: Call platform panic handler (esp_restart on ESP32)
  - Location: `include/enjin2/components/lua_script.hpp` (enum ScriptErrorPolicy)

- **Canvas/Memory Overflow:** Static allocation guarantees no malloc failures; exceeded limits return nullptr (addComponent, addObject, addScene)
  - Example: Object::addComponent() returns nullptr if componentCount >= 16
  - Location: `include/enjin2/core/object.hpp` (template method addComponent)

- **Scene Not Found:** changeScene() returns false; switchTo() silently ignores unknown sceneId (embedded release constraint)
  - Location: `include/enjin2/core/scene_state_machine.hpp`

## Cross-Cutting Concerns

**Logging:** console via printf (C_LuaScript error messages); no file logging by default
- Accessible via Lua: `engine.log(msg)`
- Location: `src/scripting/bindings_engine.cpp` (lua_engine_log)

**Validation:**
- Component presence checked via getComponent<T>() (O(n) linear search) or hasComponent<T>()
- Component dependencies enforced via assertRequires<T>() in awake()
- Input validation in bindings: bounds checking on layer indices, button codes, etc.

**Authentication/Authorization:** Not applicable (embedded game engine)

**Named Identity System:**
- Objects can have optional name (const char*) and up to 8 tags (const char* array)
- Scene::findByName(name) returns first object with matching name (case-sensitive)
- Scene::findAllWithTag(tag, results[], maxResults) returns matching objects into caller array
- Zero-allocation design: caller owns lifetime of name/tag strings (stored as const char* only)
- Location: `include/enjin2/core/object.hpp` (setName, getName, addTag, hasTag, clearTags)

---

*Architecture analysis: 2026-02-27*
