# Architecture

**Analysis Date:** 2026-01-29

## Pattern Overview

**Overall:** Component-Based Game Engine with Scene Graph

**Key Characteristics:**
- Object-Component system for game entities
- Scene-based architecture with state machine for transitions
- Template-based graphics abstraction supporting multiple pixel formats
- Static memory allocation for embedded systems
- Modular library structure (core, graphics, UI, scripting)
- Signal/event system for loose coupling between components

## Layers

**Core Layer:**
- Purpose: Foundation types, memory management, base classes for objects/components
- Location: `enjin2/include/enjin2/core/`, `enjin2/src/core/`
- Contains: Object, Component, Scene, types (Point, Rect, Pixel4), memory management
- Depends on: C++17 standard library, Adafruit_GFX (for font support)
- Used by: All other layers

**Graphics Layer:**
- Purpose: Hardware-independent drawing abstraction
- Location: `enjin2/include/enjin2/graphics/`, `enjin2/src/graphics/`
- Contains: Canvas interfaces (ICanvas, Canvas4, Canvas8), sprites, text rendering, effects
- Depends on: Core layer, Adafruit-GFX-Library
- Used by: UI layer, Components layer, Scene rendering

**Components Layer:**
- Purpose: Reusable game logic pieces that attach to Objects
- Location: `enjin2/include/enjin2/components/`, `enjin2/src/components/`
- Contains: Drawable, Position, Sprite, Canvas, ImageCache, LuaScript
- Depends on: Core layer, Graphics layer
- Used by: Object instances, UI widgets

**UI Layer:**
- Purpose: User interface widgets and interaction systems
- Location: `enjin2/include/enjin2/ui/`, `enjin2/src/ui/`
- Contains: Theme, System, Component, Widget, layout managers
- Depends on: Core layer, Graphics layer, Components layer
- Used by: Scenes for UI rendering

**Scripting Layer:**
- Purpose: Lua embedding for dynamic game logic
- Location: `enjin2/include/enjin2/scripting/`, `enjin2/src/scripting/`
- Contains: LuaEngine, LuaPlatform, bindings to engine APIs
- Depends on: Core layer, LuaJIT (embedded) or system Lua
- Used by: LuaScript component

**Animation Layer:**
- Purpose: Keyframe-based animation system
- Location: `enjin2/include/enjin2/animation/`, `enjin2/src/animation/`
- Contains: Keyframe, animation tracks
- Depends on: Core layer
- Used by: Components for animated properties

**Utils Layer:**
- Purpose: Helper functions and algorithms
- Location: `enjin2/include/enjin2/utils/`, `enjin2/src/utils/`
- Contains: Drawing helpers, noise generation, polar coordinates, easing functions
- Depends on: Core layer
- Used by: Graphics, Components, Effects

**Effects Layer:**
- Purpose: Post-processing and visual effects
- Location: `enjin2/include/enjin2/effects/`, `enjin2/src/effects/`
- Contains: CRT simulation, pixel effects, filters
- Depends on: Graphics layer, Utils layer
- Used by: Components, Scenes

## Data Flow

**Scene Initialization Flow:**

1. Create Scene instance (e.g., `MyScene scene(1)`)
2. Add scene to SceneStateMachine: `machine.addScene<MyScene>(1)`
3. Activate scene: `machine.changeScene(1)`
4. Scene calls `initialize()` → `onCreate()` (user override)
5. Scene activates → calls objects `initialize()` → `awake()` for all components
6. Scene activates → calls objects `start()` → `start()` for all components

**Per-Frame Update Flow:**

1. Application calls `SceneStateMachine::update(deltaTime)`
2. State machine handles transitions (if active)
3. State machine delegates to current scene's `update(deltaTime)`
4. Scene calls `onUpdate(deltaTime)` (user override)
5. Scene iterates through all active objects via ObjectCollection
6. For each active object: `object->update(deltaTime)` → calls each component's `update()`
7. After all updates: scene calls `lateUpdate(deltaTime)` → objects `lateUpdate()` → components `lateUpdate()`

**Rendering Flow:**

1. Application calls `SceneStateMachine::render(canvas)`
2. State machine delegates to current scene's `render(canvas)`
3. Scene calls `onRender(canvas)` (user override for scene-specific rendering)
4. Scene collects all C_Drawable components from all active objects
5. Sort drawables by layer and sort order using `shouldDrawBefore()`
6. For each sorted drawable: call `drawable->draw(canvas)`
7. Each drawable's draw method uses canvas operations (setPixel, fillRect, etc.)

**Component Lifecycle Flow:**

1. `object->addComponent<T>()` creates component instance
2. Component stored in object's fixed-size array (max 16 components)
3. Special components cached: Position, Drawables
4. If object already awoken: component `awake()` called immediately
5. If object already started: component `start()` called immediately
6. During update: component `update(deltaTime)` called if enabled
7. During late update: component `lateUpdate(deltaTime)` called if enabled

**State Machine Transition Flow:**

1. `changeScene(sceneId, transitionType, duration)` called
2. State machine validates scene exists and not already transitioning
3. Signal `onSceneChangeStartSignal` emitted
4. Based on transition type:
   - IMMEDIATE: Complete transition immediately
   - FADE_OUT_IN: Set state to FADING_OUT, start timer
   - SLIDE_*: Set state to SLIDING, initialize next scene
5. During transition updates:
   - Update timer and progress (0.0 to 1.0)
   - Emit `onTransitionProgressSignal`
   - For FADE: at 50%, switch scenes, change to FADING_IN
6. At progress 1.0: call `completeTransition()`
7. Deactivate old scene, activate new scene
8. Emit `onSceneChangeCompleteSignal`

**State Management:**
- Objects: Component-based, state distributed across components
- Scenes: Managed by SceneStateMachine with unique IDs
- Global: No global state in enjin2 namespace (unlike original enjin)

## Key Abstractions

**Object:**
- Purpose: Game entity base class that composes functionality from components
- Examples: `enjin2/include/enjin2/core/object.hpp`, `enjin/Object.hpp`
- Pattern: Object-Component pattern with static component arrays, cached common components

**Component:**
- Purpose: Modular piece of functionality attached to Objects
- Examples: `enjin2/include/enjin2/core/component.hpp`, `enjin/Components/Component.hpp`
- Pattern: Base class with lifecycle methods (awake, start, update, lateUpdate), owner pointer

**Scene:**
- Purpose: Container for a game state with its own collection of Objects
- Examples: `enjin2/include/enjin2/core/scene.hpp`, `enjin/Scene.hpp`
- Pattern: Lifecycle management (onCreate, onActivate, onUpdate, onDeactivate, onDestroy, onRender)

**ICanvas:**
- Purpose: Hardware-agnostic drawing interface
- Examples: `enjin2/include/enjin2/graphics/canvas.hpp` (ICanvas, Canvas4, Canvas8)
- Pattern: Template-based with pixel type specialization, supports 4-bit packed and 8-bit storage

**Signal:**
- Purpose: Type-safe event/notification system
- Examples: `enjin2/include/enjin2/core/signal.hpp`, `enjin/Signal.hpp`
- Pattern: Observer pattern with RAII connection management (SignalConnection)

**SceneStateMachine:**
- Purpose: Manage multiple scenes and transitions between them
- Examples: `enjin2/include/enjin2/core/scene_state_machine.hpp`, `enjin/SceneStateMachine.hpp`
- Pattern: State machine with transition effects, signal-based events

**C_Drawable:**
- Purpose: Base class for components that can render themselves
- Examples: `enjin2/include/enjin2/components/drawable.hpp`, `enjin/Components/C_Drawable.hpp`
- Pattern: Rendering interface with layer/sort ordering, blend modes, anchoring

## Entry Points

**Library Entry Points:**
- `enjin2/include/enjin2/core/object.hpp` - Object class for creating game entities
- `enjin2/include/enjin2/core/scene.hpp` - Scene class for game states
- `enjin2/include/enjin2/core/scene_state_machine.hpp` - SceneStateMachine for managing scenes
- `enjin2/include/enjin2/graphics/canvas.hpp` - Canvas types for rendering
- `enjin2/include/enjin2/scripting/lua_engine.hpp` - LuaEngine for scripting

**Example Entry Points:**
- `enjin2/examples/basic_drawing/` - Simple drawing examples
- `enjin2/examples/lua_scripting/` - Lua integration examples
- `enjin2/examples/ecs_demo/` - Entity-Component-System examples
- `enjin2/examples/esp32_idf_example/` - ESP32 hardware examples

**Triggers:**
- Main application loop calls `SceneStateMachine::update()` and `SceneStateMachine::render()`
- User creates Objects and Components, adds them to Scenes
- User registers custom Scene types with SceneStateMachine
- Lua scripts loaded via LuaEngine and attached to LuaScript components

**Responsibilities:**
- Core layer: Foundation abstractions, no platform-specific code
- Graphics layer: Canvas implementations, drawing primitives
- Components layer: Reusable game logic (position, rendering, scripting)
- Scenes: Object lifecycle management, rendering orchestration
- State machine: Scene transitions, effect rendering

## Error Handling

**Strategy:** Minimal error handling for embedded environments

**Patterns:**
- Static assertions for template type constraints (e.g., `static_assert(std::is_base_of<Component, T>::value)`)
- Null pointer checks in critical paths (e.g., `if (!gfx_font) return;`)
- Return nullptr for failures (e.g., `addComponent()` returns nullptr if full)
- No exceptions (embedded constraint)
- Lua execution returns LuaResult struct with success/error flag
- Assertions disabled in release builds

## Cross-Cutting Concerns

**Logging:** No logging framework (embedded constraint), uses printf for debugging in examples
**Validation:** Template static assertions, bounds checking in canvas operations
**Authentication:** Not applicable (game engine, no network auth)
**Memory Management:** Static allocation with fixed-size arrays, no dynamic allocation after initialization
**Platform Abstraction:** Adafruit_GFX compatibility layer, ESP32-specific canvas implementations
**Threading:** Single-threaded (embedded constraint), no concurrent access patterns
---

*Architecture analysis: 2026-01-29*
