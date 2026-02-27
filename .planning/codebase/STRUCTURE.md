# Codebase Structure

**Analysis Date:** 2026-02-27

## Directory Layout

```
enjin/
├── include/enjin2/              # Public header API
│   ├── core/                    # Entity/component/scene core
│   ├── components/              # Concrete component implementations
│   ├── graphics/                # Canvas, primitives, effects, composition
│   ├── scripting/               # Lua engine and bindings
│   ├── input/                   # Input state management
│   ├── animation/               # Keyframe and track systems
│   ├── effects/                 # Post-processing effects
│   ├── ui/                      # UI widget system
│   ├── utils/                   # Utility functions (math, drawing helpers)
│   ├── abstract/                # Interface definitions (ICanvas)
│   └── types.hpp                # Core type definitions (Point, Rect, Pixel4, etc.)
│
├── src/                         # Implementation files (mirrored structure)
│   ├── core/                    # Object, Scene, ObjectCollection implementation
│   ├── components/              # Component implementations (lua_script, drawable, canvas, etc.)
│   ├── scripting/               # Lua bindings (bindings.cpp, bindings_*.cpp for subsystems)
│   ├── graphics/                # Canvas, primitives, effects implementation
│   ├── input/                   # Input polling implementation
│   ├── ui/                      # UI system implementation
│   ├── utils/                   # Utility implementations
│   ├── animation/               # Animation system implementation
│   ├── effects/                 # Effects implementation
│   ├── bindings/                # Platform-specific bindings (emscripten_bindings.cpp)
│   └── platform/sdl/            # SDL3 integration (sdl_main.cpp)
│
├── tests/                       # Comprehensive test suite
│   ├── *_test.cpp               # Unit/integration tests (collision_test, engine_table_test, etc.)
│   ├── CMakeLists.txt           # Test build configuration
│   └── pikachu.h                # Test sprite data (binary)
│
├── tools/                       # Development tools
│   ├── aseprite/                # Aseprite export scripts
│   ├── palettes/                # Pre-built palette files
│   ├── aseprite2enjin.py        # Python script to convert Aseprite files
│   └── README_aseprite2enjin.md
│
├── examples/                    # Usage examples
│   ├── lua_scripting/           # Lua scripting example
│   ├── ecs_demo.cpp             # ECS pattern demonstration
│   ├── postfx_demo.cpp          # Post-effect demonstration
│   ├── text_demo_improved.cpp   # Text rendering example
│   └── [20+ other examples]
│
├── docs/                        # Documentation
│   └── [API documentation, guides]
│
├── CMakeLists.txt               # Top-level build configuration
├── DESIGN.md                    # Design document
└── .clang-tidy                  # Linting configuration
```

## Directory Purposes

**include/enjin2/core/:**
- Purpose: Fundamental ECS infrastructure (Object, Component, Scene, ObjectCollection, SceneStateMachine)
- Contains: .hpp files defining entity/component lifecycle, scene management, deferred transitions
- Key files:
  - `object.hpp`: Object base class with component management
  - `component.hpp`: Component base class with awake/start/update/lateUpdate lifecycle
  - `scene.hpp`: Scene container with object collection and signal events
  - `scene_state_machine.hpp`: Multi-scene state machine with immediate/deferred/transition modes
  - `object_collection.hpp`: Fixed-size array management for objects
  - `types.hpp`: Point, Rect, Pixel4, Anchor, BlendMode enums

**include/enjin2/components/:**
- Purpose: Concrete component implementations providing game engine features
- Contains: Drawable base (position, layer, blend), Sprite (spritesheet animation), Animation (keyframe tracks), LuaScript (Lua execution), Canvas (per-object rendering), specialized UI (Slider, Button, Label)
- Key files:
  - `drawable.hpp`: Base class for all renderable components (layer, visibility, anchor)
  - `position.hpp`: 2D positioning with anchor points
  - `lua_script.hpp`: Script execution with Lua bindings access
  - `sprite.hpp`: Spritesheet-based animation
  - `animation.hpp`: Keyframe-based tweening
  - `canvas.hpp`: Per-object render target

**include/enjin2/scripting/:**
- Purpose: Lua embedding and bindings layer
- Contains: LuaEngine (state management), LuaBindings (C→Lua function registration), LuaCanvas (type erasure), ScriptProxy (userdata for components)
- Key files:
  - `lua_engine.hpp`: Lua state lifecycle, script execution
  - `bindings.hpp`: LuaBindings class, LuaCanvas wrapper, ScriptProxy userdata, EngineTimeState injection
  - `bind_helpers.hpp`: Template utilities for registering function tables (luaBindFunctions, luaBindGlobals)
  - `lua_platform.hpp`: Platform-specific Lua integration (memory pools, panic handling)

**include/enjin2/graphics/:**
- Purpose: Low-level pixel manipulation and composition
- Contains: Canvas interface (templated), Pixel4/8-bit variants, shape primitives, effects, layer composition
- Key files:
  - `canvas.hpp`: ICanvas<T> interface, Canvas4<W,H>, Canvas8<W,H> implementations
  - `primitives.hpp`: Primitives4/Primitives8 static classes for shapes (line, rect, circle, triangle, text)
  - `layer_compositor.hpp`: Multi-layer rendering with visibility/blending
  - `effects.hpp`: Post-processing effects (posterize, dither, etc.)

**include/enjin2/input/:**
- Purpose: Platform-agnostic input state capture
- Contains: InputState struct (buttons, axes), input_platform_poll() hook
- Key files:
  - `input_state.hpp`: Button bitmask, axis storage, state queries

**include/enjin2/animation/, effects/, ui/, utils/, abstract/:**
- `animation/`: Keyframe, AnimationTrack, easing functions
- `effects/`: Post-FX interfaces
- `ui/`: Widget system (Component, System, Theme, Layout)
- `utils/`: Drawing helpers, noise generation, polar coordinates
- `abstract/`: ICanvas interface definition

**src/core/:**
- Purpose: Implement Object/Scene lifecycle, component initialization, object collection updates
- Key files:
  - `object.cpp`: Object::awake/start/update/lateUpdate; component caching (position, drawables)
  - `scene.cpp`: Scene lifecycle and object collection management
  - `math.cpp`: Math utilities (Point, Rect, vector operations)
  - `memory.cpp`: Memory management utilities

**src/scripting/:**
- Purpose: Implement Lua engine and all bindings
- Key files:
  - `lua_engine.cpp`: LuaEngine initialization, script execution, memory management
  - `bindings.cpp`: LuaBindings core, canvas functions, entity functions, engine.* table registration
  - `bindings_draw.cpp`: love2d.graphics-style drawing functions (point, line, rectangle, circle, triangle, text)
  - `bindings_input_sprites.cpp`: Input polling (engine.input.*) and sprite pool functions
  - `bindings_layers_text.cpp`: Layer system and text bindings (font management, text measurement)
  - `bindings_math.cpp`: Math type constructors (Vec2, Point, Rect) and utility functions
  - `bindings_system.cpp`: System/debug functions
  - `bindings_engine.cpp`: engine.scene.*, engine.time.*, engine.log.*, collision functions, GC bindings

**src/components/:**
- Purpose: Implement drawable and script components
- Key files:
  - `drawable.cpp`: C_Drawable base functionality
  - `lua_script.cpp`: Script loading, execution, error handling, ScriptProxy setup
  - `canvas.cpp`: Per-object canvas rendering
  - `image_cache.cpp`: Image caching system
  - `animation.cpp`: Animation playback

**src/graphics/:**
- Purpose: Implement canvas, primitives, effects, composition
- Key files:
  - `canvas.cpp`: Canvas initialization, pixel access
  - `primitives.cpp`: Shape drawing algorithms
  - `effects.cpp`: Post-processing implementations
  - `palette.cpp`: Palette management

**src/platform/sdl/:**
- Purpose: SDL3 integration and main loop
- Key files:
  - `sdl_main.cpp`: SDL3 window creation, event loop, scene/input integration

**src/input/:**
- Purpose: Input state capture and polling
- Key files:
  - `input.cpp`: Button/axis state updates, platform-agnostic polling

**tests/:**
- Purpose: Comprehensive test coverage for core systems
- Test files (each ~100-1000 lines):
  - `collision_test.cpp`: Collision detection functions (AABB, circle, line, point-in-rect)
  - `engine_table_test.cpp`: engine.* Lua bindings (scene switching, time, input, logging)
  - `error_policy_test.cpp`: C_LuaScript error handling (Disable/Log/Panic modes)
  - `gc_assert_test.cpp`: Component assertRequires<T>() dependency validation
  - `input_event_callback_test.cpp`: Input event callback system
  - `layer_binding_test.cpp`: Layer system and visibility
  - `math_binding_test.cpp`: Vec2/Point/Rect math types in Lua
  - `named_objects_test.cpp`: Object name/tag system (findByName, findAllWithTag)
  - `scene_transition_test.cpp`: Scene lifecycle (onCreate, onActivate, switchTo deferred transitions)
  - `sprite_test.cpp`: Sprite pool animation (frame-by-frame, loop modes)
  - `text_binding_test.cpp`: Text rendering (fonts, wrapping, measurement)
  - `hot_reload_test.cpp`: Script reloading and error recovery
  - `compositor_test.cpp`: Layer composition and rendering order
  - Other tests: palette, shadow mode, image comparison

**tools/:**
- Purpose: Asset pipeline and development utilities
- Contains:
  - `aseprite/enjin-export.lua`: Aseprite export plugin (generates sprite data)
  - `aseprite2enjin.py`: Python script to batch convert Aseprite files to Enjin format
  - `palettes/`: Pre-built 16-color palette files

**examples/:**
- Purpose: Demonstrate engine features
- Contains 20+ example programs including:
  - `lua_scripting/main.cpp`: Full Lua scripting example
  - `ecs_demo.cpp`: Entity-component system usage
  - `postfx_demo.cpp`: Post-effects rendering
  - `text_demo_improved.cpp`: Text rendering and fonts
  - `space_ui_demo.cpp`: UI widget demonstration
  - `canvas_demo.cpp`: Low-level canvas API
  - Other examples: graphics, animation, input, collision, performance profiling

## Key File Locations

**Entry Points:**
- `src/platform/sdl/sdl_main.cpp`: SDL3 window and event loop for desktop apps
- `examples/lua_scripting/main.cpp`: Lua scripting full integration example
- User code: typically subclasses Scene and implements onCreate/onActivate/onUpdate/onRender

**Configuration:**
- `CMakeLists.txt`: Build configuration (SDL3, LuaJIT, testing, examples)
- `.clang-tidy`: C++ linting rules
- No runtime config files (all configuration is compile-time or programmatic)

**Core Logic:**
- `include/enjin2/core/object.hpp`: Object/Component interaction patterns
- `include/enjin2/core/scene_state_machine.hpp`: Scene transitions and deferred switching
- `include/enjin2/scripting/bindings.hpp`: Lua interface definitions
- `src/scripting/bindings_engine.cpp`: engine.* global table implementation

**Testing:**
- `tests/CMakeLists.txt`: Test compilation
- `tests/*_test.cpp`: Individual feature tests (no mock framework; direct integration tests)
- Example: `tests/scene_transition_test.cpp` tests Scene::initialize → Scene::activate → Object::update cycles

## Naming Conventions

**Files:**

| Pattern | Example | Purpose |
|---------|---------|---------|
| `*.hpp` | `object.hpp`, `drawable.hpp` | Public header files in include/enjin2/ |
| `*.cpp` | `object.cpp`, `bindings.cpp` | Implementation in src/ |
| `bindings_*.cpp` | `bindings_draw.cpp`, `bindings_engine.cpp` | Lua binding subsystems |
| `*_test.cpp` | `collision_test.cpp` | Test files in tests/ |
| Module groups | `module_group.hpp` | Convenience headers including all components in a subsystem |

**Directories:**

| Pattern | Example | Purpose |
|---------|---------|---------|
| Subsystem folder | `core/`, `components/`, `scripting/` | Groups related functionality |
| Mirror structure | `include/enjin2/X/` ↔ `src/X/` | Headers and implementations |
| `*_test.cpp` location | `tests/` | All tests in one directory (flat) |

**C++ Identifiers:**

| Pattern | Example | Purpose |
|---------|---------|---------|
| Class prefix `C_` | `C_Position`, `C_Drawable`, `C_LuaScript` | Component classes |
| Enum class | `Anchor`, `BlendMode`, `TransitionType` | Type-safe enums |
| Function prefix | `lua_` (static) | Lua C-API wrapper functions in LuaBindings |
| Template helpers | `luaBindFunctions`, `luaArrayLen` | Generic utilities in bind_helpers.hpp |
| Class name (no prefix) | `Object`, `Scene`, `Component` | Non-component core classes |
| Interface prefix `I` | `ICanvas` | Abstract base interfaces |
| Method names | `camelCase` | Member function naming (getComponent, addObject, setPosition) |
| Constants | `MAX_COMPONENTS = 16` | Static constexpr limits |

## Where to Add New Code

**New Component Type:**
- Definition: `include/enjin2/components/mycomponent.hpp`
  - Inherit from `Component` or `C_Drawable` (if renderable)
  - Implement awake(), start(), update() as needed
  - Store state in member variables
- Implementation: `src/components/mycomponent.cpp`
- Test: `tests/mycomponent_test.cpp` (following existing test patterns)
- Example: `examples/mycomponent_example.cpp`

**New Scene:**
- Definition: User code (no required location)
  - Inherit from `Scene`
  - Override onCreate() to create initial objects
  - Override onUpdate(dt) for scene logic
  - Override onRender(canvas) for backgrounds/overlays
- Register with SSM: `ssm.addScene<MyScene>(sceneId)` then `ssm.changeScene(sceneId)`

**New Drawable Component:**
- Definition: `include/enjin2/components/mydrawable.hpp` (inherit C_Drawable)
- Implementation: `src/components/mydrawable.cpp`
- Implement draw(ICanvas<Pixel4>& canvas) method
- Access position via `position` member (cached by Object)
- Use Primitives4 or Primitives8 for drawing

**Lua Binding (New Function):**
- C++ handler: Add static int lua_myfunction(lua_State* L) in LuaBindings
- Implementation: `src/scripting/bindings_mysubsystem.cpp` or extend existing file
- Registration: Call lua_pushcfunction(L, lua_myfunction); lua_setglobal(L, "myfunction") in LuaBindings::registerAll()
- Test: `tests/mybinding_test.cpp` with lua_State integration

**Utilities/Helpers:**
- Math helpers: `include/enjin2/utils/mymath.hpp` and `src/utils/mymath.cpp`
- Drawing helpers: `include/enjin2/utils/drawing_helpers.hpp`
- Type definitions: Extend `include/enjin2/core/types.hpp`

## Special Directories

**include/enjin2/ (Public API):**
- Purpose: All public-facing headers for users of the library
- Structure: Exactly mirrors `src/` layout
- Generated: No (all hand-written)
- Committed: Yes (source files, not generated)

**src/ (Implementation):**
- Purpose: All .cpp implementation files
- Structure: Mirrors include/enjin2/
- Generated: No
- Committed: Yes

**tests/:**
- Purpose: Comprehensive test suite (no mocking, direct integration)
- Structure: Flat (all tests in one directory regardless of subsystem)
- Generated: No (CMake generates build targets)
- Committed: Yes

**tools/:**
- Purpose: Development utilities and asset pipeline
- Contents: Aseprite export plugins, Python converters
- Generated: No
- Committed: Yes (tools and utilities, not generated assets)

**examples/:**
- Purpose: Working examples demonstrating engine features
- Structure: Organized by feature (lua_scripting/, ecs_demo/, postfx_demo/)
- Generated: No (source code)
- Committed: Yes (source code and some test data)

**build/, build_*/ (Build Artifacts):**
- Purpose: CMake build outputs (object files, executables, libraries)
- Generated: Yes (by CMake)
- Committed: No (.gitignore excludes build/)

---

*Structure analysis: 2026-02-27*
