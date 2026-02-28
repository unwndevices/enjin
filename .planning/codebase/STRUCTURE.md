# Codebase Structure

**Analysis Date:** 2026-02-28

## Directory Layout

```
/home/unwn/dev/enjin/
├── include/                 # Public headers (namespace enjin2)
│   └── enjin2/
│       ├── core/            # ECS, scene management
│       ├── components/      # Drawable, sprite, position, Lua script
│       ├── graphics/        # Canvas, rendering, effects
│       ├── scripting/       # Lua bindings, engine bridge
│       ├── input/           # Input state structures
│       ├── animation/       # Animation tracks, keyframes
│       ├── effects/         # Post-processing effects
│       ├── ui/              # UI widgets, layout, theme
│       ├── utils/           # Math, noise, drawing helpers
│       └── abstract/        # Base interfaces (ICanvas)
├── src/                     # Implementation files
│   ├── core/                # object.cpp, scene.cpp, memory.cpp, math.cpp
│   ├── components/          # drawable.cpp, lua_script.cpp, canvas.cpp, image_cache.cpp
│   ├── graphics/            # canvas.cpp, primitives.cpp, palette.cpp, effects.cpp
│   ├── scripting/           # lua_engine.cpp, bindings.cpp, bindings_*.cpp
│   ├── platform/            # Platform-specific code (SDL)
│   ├── input/               # input.cpp
│   ├── animation/           # keyframe.cpp
│   ├── effects/             # postfx.cpp
│   ├── ui/                  # component.cpp, layout.cpp, widget.cpp
│   └── utils/               # drawing_helpers.cpp, noise.cpp, polar.cpp
├── tests/                   # Unit tests and test fixtures
├── examples/                # Example programs
├── build/                   # CMake build directory
├── project/                 # VCV Rack integration (plugin-specific)
├── docs/                    # API documentation source
├── scripts/                 # Utility scripts (API generation, etc.)
├── CMakeLists.txt           # Build configuration
├── README.md                # Project overview
└── .planning/               # GSD project planning (this structure)
    └── codebase/            # Architecture documentation (this file)
```

## Directory Purposes

**include/enjin2/:**
- Purpose: Public API headers (all application code #includes from here)
- Contains: Header-only and declaration files
- Key files: enjin2.hpp (main entry point header likely - gather all)

**include/enjin2/core/:**
- Purpose: Core ECS framework - entities, components, scenes
- Key files:
  - `object.hpp`: Object base class, component storage, lifecycle
  - `component.hpp`: Component base class with virtual callbacks
  - `object_collection.hpp`: Static array (max 128) managing objects in scene
  - `scene.hpp`: Scene with object management and rendering coordination
  - `scene_state_machine.hpp`: Multi-scene management with transitions
  - `types.hpp`: Point, Rect, Pixel4, color types
  - `math.hpp`: Math utilities (Vec2, trig, collision)
  - `memory.hpp`: Memory utilities
  - `signal.hpp`: Event signal system
  - `collision.hpp`: Collision detection functions

**include/enjin2/components/:**
- Purpose: Reusable component implementations
- Key files:
  - `drawable.hpp`: C_Drawable base for renderable components (layer, blend, anchor)
  - `position.hpp`: C_Position for transforms (auto-added to all objects)
  - `sprite.hpp`: C_Sprite with SpriteSheet animation (Once/Loop/PingPong modes)
  - `lua_script.hpp`: C_LuaScript for script-driven UI/logic
  - `canvas.hpp`: C_Canvas for custom drawing
  - `image_cache.hpp`: Sprite asset management
  - `label.hpp`, `button_dial.hpp`, `slider.hpp`: UI widgets
  - `animation.hpp`: Animation state machine
  - `planet.hpp`, `satellite.hpp`: Domain-specific components

**include/enjin2/graphics/:**
- Purpose: Rendering pipeline and drawing primitives
- Key files:
  - `canvas.hpp`: ICanvas<T> abstract interface and Canvas4/Canvas8 implementations
  - `sprite.hpp`: SpriteSheet struct, sprite blitting with transparency (palette index 15)
  - `sprite_asset.hpp`: Binary .njn sprite format loader
  - `primitives.hpp`: Line, rect, circle, triangle drawing
  - `palette.hpp`: Color palette management
  - `effects.hpp`: Visual effects (fade, scanlines, etc.)
  - `layer_compositor.hpp`: Multi-layer rendering coordination
  - `text_renderer.hpp`: Text drawing with font support
  - `gfxfont.h`: Adafruit GFX font format
  - `defaultfont.hpp`: Built-in 5x7 font

**include/enjin2/scripting/:**
- Purpose: Lua integration and script bindings
- Key files:
  - `lua_engine.hpp`: LuaJIT wrapper with custom allocator (256KB static pool)
  - `lua_platform.hpp`: Platform-specific Lua setup
  - `bindings.hpp`: LuaBindings class with all Lua global functions
  - `bind_helpers.hpp`: Utility macros for binding registration
  - `object_proxy.hpp`: ObjectProxy userdata for safe Object references from Lua
  - `script_interface.hpp`: Script-side interface definitions

**include/enjin2/input/:**
- Purpose: Input state management
- Key files: `input_state.hpp` - Button state, axis values

**include/enjin2/animation/:**
- Purpose: Animation system
- Key files: `animation_track.hpp`, `keyframe.hpp`

**include/enjin2/ui/:**
- Purpose: UI framework and widgets
- Key files: `component.hpp`, `system.hpp`, `theme.hpp`, `layout.hpp`

**include/enjin2/utils/:**
- Purpose: Utility functions and helpers
- Key files: `drawing_helpers.hpp`, `noise.hpp`, `polar.hpp`, `math.hpp`

**src/core/:**
- Key implementations:
  - `object.cpp`: Object constructor (auto-adds C_Position), lifecycle methods
  - `scene.cpp`: Scene template specializations for rendering
  - `memory.cpp`, `math.cpp`, `types.cpp`: Utility implementations

**src/components/:**
- Key implementations:
  - `drawable.cpp`: C_Drawable anchor/offset/visibility setup
  - `lua_script.cpp`: C_LuaScript with update/draw lifecycle

**src/graphics/:**
- Key implementations:
  - `canvas.cpp`, `primitives.cpp`: Pixel operations, shape drawing
  - `palette.cpp`, `effects.cpp`: Color and effects

**src/scripting/:**
- Purpose: Lua bindings organized by subsystem
- Key files:
  - `lua_engine.cpp`: LuaJIT initialization, memory allocator
  - `bindings.cpp`: Main binding registry
  - `bindings_engine.cpp`: engine.* table (scene, time, input, collision, store)
  - `bindings_draw.cpp`: love2d.graphics-style drawing (line, rect, circle, triangle)
  - `bindings_layers_text.cpp`: Layer and text rendering bindings
  - `bindings_input_sprites.cpp`: Input polling and sprite pool
  - `bindings_sprite_load.cpp`: .njn binary sprite asset loading
  - `bindings_math.cpp`: Math utilities and types (Vec2, Point, Rect)
  - `bindings_system.cpp`: System functions (print, etc.)
  - `bindings_store.cpp`: Persistent key-value store bindings
  - `lua_platform.cpp`: Platform-specific initialization

**src/platform/sdl/:**
- Purpose: SDL platform implementation for desktop/testing
- Key files: `sdl_main.cpp`

**tests/:**
- Purpose: Unit tests and integration tests
- Structure: Test fixtures for components, bindings, scenes

**examples/:**
- Purpose: Demonstration programs
- Contains: Sample applications using enjin2

## Key File Locations

**Entry Points:**

- `include/enjin2.hpp` or similar: Main public API header (gather all major types)
- `src/platform/sdl/sdl_main.cpp`: Desktop SDL application entry point
- Host application main(): Typically creates Canvas, Scene, manages game loop

**Configuration:**

- `CMakeLists.txt`: Build configuration with options for Lua, WASM, tests, examples
- `.clang-tidy`: Static analysis configuration
- `library.json`: VCV Rack plugin metadata

**Core Logic:**

- `include/enjin2/core/object.hpp`: Entity management (max 16 components/object)
- `include/enjin2/core/component.hpp`: Component base pattern
- `include/enjin2/core/scene.hpp`: Scene/ObjectCollection coordination
- `include/enjin2/core/scene_state_machine.hpp`: Scene transitions and deferred switching
- `include/enjin2/scripting/bindings.hpp`: Lua API (engine.*, drawing, sprites, physics)

**Rendering:**

- `include/enjin2/graphics/canvas.hpp`: Canvas interface and implementations
- `include/enjin2/components/drawable.hpp`: Rendering base with layer/anchor system
- `include/enjin2/graphics/sprite.hpp`: Sprite sheet with frame animation
- `include/enjin2/graphics/sprite_asset.hpp`: .njn binary format (indexed colors, frame metadata)

**Scripting:**

- `include/enjin2/components/lua_script.hpp`: Script component with lifecycle callbacks
- `src/scripting/bindings_engine.cpp`: engine.scene.spawn/destroy/find/switch
- `src/scripting/bindings_sprite_load.cpp`: engine.sprite.load for .njn assets
- `src/scripting/bindings_draw.cpp`: Drawing primitives from Lua

**Testing:**

- `tests/`: Test cases (likely for components, bindings, math)

## Naming Conventions

**Files:**

- Headers: `.hpp` (C++ headers in include/enjin2/)
- Implementation: `.cpp` (C++ sources in src/)
- All organization files use `UPPERCASE.md`

**Directories:**

- Namespace aligned: `include/enjin2/core/` → `src/core/`
- Subsystems grouped by domain: graphics, scripting, input, animation, ui, effects

**Types and Classes:**

- Components: `C_ComponentName` (e.g., C_Position, C_Drawable, C_LuaScript, C_Sprite)
- Lua types: `Lua*` (e.g., LuaEngine, LuaBindings, LuaCanvas, LuaStore)
- Canvas types: `Canvas<BitDepth>_<Width>x<Height>` (e.g., Canvas4_128x128, Canvas8_128x64)
- Structs: PascalCase (Point, Rect, SpriteSheet, ObjectProxy, InputState)
- Enums: PascalCase or EnumType (AnimMode::Loop, BlendMode::Normal, Anchor::CENTER)

**Functions:**

- Member functions: camelCase (getComponent, addObject, setPixel)
- Lua bindings: `lua_*` C-style names (lua_setColor, lua_drawCircle, lua_engine_scene_spawn)
- Helpers: camelCase (updateTransition, renderObjects, drawables)

## Where to Add New Code

**New Feature (Game Logic):**
- Primary code: Implement as Object subclass or Component subclass
  - Object: `include/enjin2/core/object.hpp` (extend Object, override onCreate/onActivate/onUpdate)
  - Component: `include/enjin2/components/custom.hpp` + `src/components/custom.cpp`
- Tests: `tests/` (unit tests for component behavior)

**New Component/Module:**
- Implementation:
  - Header: `include/enjin2/components/name.hpp` or `include/enjin2/subsystem/name.hpp`
  - Source: `src/components/name.cpp` or `src/subsystem/name.cpp`
- Registration: Add to CMakeLists.txt target_sources()

**New Lua Binding:**
- Implementation: Create `src/scripting/bindings_feature.cpp` with lua_* functions
- Registration: Call registerAll() in bindings.cpp → add to table setup
- Pattern: Follow existing bindings_engine.cpp structure (LuaFuncDef array + luaBindFunctions)

**Utilities:**
- Shared helpers: `include/enjin2/utils/name.hpp` + `src/utils/name.cpp`
- Math functions: Add to `include/enjin2/core/math.hpp`

**Platform-Specific Code:**
- Location: `src/platform/<platform>/` (e.g., src/platform/sdl/, src/platform/esp32/)
- Use preprocessor guards: `#ifdef PLATFORM_X` for compile-time selection

## Special Directories

**build/:**
- Purpose: CMake build artifacts
- Generated: Automatically created by `mkdir build && cd build && cmake ..`
- Committed: No (in .gitignore)

**luajit/:**
- Purpose: Embedded LuaJIT source (bundled for static linking)
- Generated: No
- Committed: Yes (vendor dependency)

**project/:**
- Purpose: VCV Rack plugin integration files
- Generated: No
- Committed: Yes (plugin-specific)

**docs/:**
- Purpose: API documentation source (Doxygen + Docusaurus)
- Generated: Doxygen creates docs/xml on build with docs target
- Committed: Source .md files yes, generated xml/html no

**.planning/codebase/:**
- Purpose: GSD codebase analysis documents (ARCHITECTURE.md, STRUCTURE.md, etc.)
- Generated: By /gsd:map-codebase command
- Committed: Yes (guides future implementations)

---

*Structure analysis: 2026-02-28*
