# Codebase Structure

**Analysis Date:** 2026-01-29

## Directory Layout

```
[project-root]/
├── enjin/                      # Original engine (legacy)
│   ├── Animation.cpp/.hpp       # Animation system
│   ├── Components/              # Component library
│   │   ├── C_*.cpp/.hpp        # Component implementations
│   │   ├── Component.hpp       # Component base class
│   │   └── S_*.cpp/.hpp        # System components
│   ├── Object.cpp/.hpp         # Object entity class
│   ├── Scene.cpp/.hpp          # Scene base class
│   ├── SceneStateMachine.cpp/.hpp # Scene state machine
│   ├── Sprite.hpp              # Sprite definitions
│   ├── UI/                     # UI widget library
│   │   └── *.hpp              # Widget implementations
│   ├── utils/                  # Utility functions
│   └── luajit/                 # LuaJIT integration
├── enjin2/                     # New engine (refactored)
│   ├── build/                  # CMake build artifacts (gitignored)
│   ├── build_wasm/             # WebAssembly build files (gitignored)
│   ├── build_wasm.sh           # WASM build script
│   ├── CMakeLists.txt          # Main CMake configuration
│   ├── include/enjin2/         # Public headers
│   │   ├── animation/          # Animation system
│   │   ├── components/         # Component library
│   │   ├── core/               # Core abstractions
│   │   ├── effects/            # Visual effects
│   │   ├── graphics/           # Graphics abstraction
│   │   ├── scripting/          # Lua integration
│   │   ├── ui/                 # UI system
│   │   └── utils/              # Utilities
│   ├── src/                    # Implementation files
│   │   ├── animation/          # Animation implementation
│   │   ├── components/         # Component implementations
│   │   ├── core/               # Core implementations
│   │   ├── graphics/           # Graphics implementations
│   │   ├── scripting/          # Lua bindings
│   │   ├── ui/                 # UI implementations
│   │   ├── bindings/           # WebAssembly bindings
│   │   ├── effects/            # Effects implementations
│   │   └── utils/              # Utils implementations
│   ├── luajit/                 # Embedded LuaJIT source
│   ├── examples/               # Example applications
│   ├── tests/                  # Unit tests
│   └── library.json           # PlatformIO library metadata
└── .planning/                  # Planning and analysis docs
    └── codebase/               # This directory
```

## Directory Purposes

**enjin/**:
- Purpose: Original engine implementation (legacy)
- Contains: Original Object-Component system, UI widgets, Animation system
- Key files: `Object.hpp`, `Scene.hpp`, `SceneStateMachine.hpp`, `Sprite.hpp`
- Notes: Uses std::shared_ptr for components, Adafruit_GFX dependency

**enjin2/include/enjin2/**:
- Purpose: Public API headers for new engine
- Contains: All header files organized by module
- Key files: `core/object.hpp`, `core/scene.hpp`, `core/component.hpp`, `graphics/canvas.hpp`
- Notes: Template-based, static allocation, no shared_ptr usage

**enjin2/src/**:
- Purpose: Implementation of all engine modules
- Contains: .cpp files corresponding to headers
- Key files: Mirrors include structure, plus bindings/ and effects/ directories
- Notes: Separation of interface (include) and implementation (src)

**enjin2/luajit/**:
- Purpose: Embedded LuaJIT runtime source
- Contains: LuaJIT amalgamated source files
- Notes: Used for WebAssembly and embedded builds

**enjin2/examples/**:
- Purpose: Demonstrations and test programs
- Contains: Example applications for various features
- Key examples: `basic_drawing`, `lua_scripting`, `ecs_demo`, `esp32_idf_example`
- Notes: Build as separate executables from CMake

**enjin2/tests/**:
- Purpose: Unit tests for engine modules
- Contains: Test files organized by module
- Notes: Built when ENJIN2_BUILD_TESTS=ON

## Key File Locations

**Entry Points:**
- `enjin2/include/enjin2/core/object.hpp` - Object class (game entity base)
- `enjin2/include/enjin2/core/scene.hpp` - Scene class (game state)
- `enjin2/include/enjin2/core/scene_state_machine.hpp` - Scene management
- `enjin2/include/enjin2/graphics/canvas.hpp` - Canvas rendering interface
- `enjin2/include/enjin2/scripting/lua_engine.hpp` - Lua scripting engine

**Configuration:**
- `enjin2/CMakeLists.txt` - Main build configuration
- `enjin2/library.json` - PlatformIO library metadata
- `enjin2/build_wasm.sh` - WebAssembly build script

**Core Logic:**
- `enjin2/include/enjin2/core/` - Core abstractions (object, component, scene, types)
- `enjin2/src/core/` - Core implementations
- `enjin2/include/enjin2/components/` - Component library (drawable, position, sprite, etc.)
- `enjin2/src/components/` - Component implementations

**Graphics:**
- `enjin2/include/enjin2/graphics/` - Graphics abstractions (canvas, sprite, text, effects)
- `enjin2/src/graphics/` - Graphics implementations

**Testing:**
- `enjin2/tests/` - Unit test directory
- `enjin2/examples/` - Integration examples and demos

## Naming Conventions

**Files:**
- Headers: `*.hpp` - All header files use .hpp extension
- Sources: `*.cpp` - All implementation files use .cpp extension
- Core headers: lowercase with underscores (e.g., `object.hpp`, `scene.hpp`)
- Component headers: `C_` prefix (e.g., `drawable.hpp`, `position.hpp`)
- System headers: `S_` prefix (e.g., `S_Drawable.hpp` in enjin/)

**Directories:**
- Lowercase with underscores (e.g., `graphics/`, `scripting/`, `animation/`)
- Namespace directory: `include/enjin2/` mirrors namespace structure

**Classes:**
- Core classes: PascalCase (e.g., `Object`, `Component`, `Scene`)
- Component classes: `C_` prefix in enjin/ (e.g., `C_Drawable`, `C_Position`)
- Template classes: PascalCase with template parameter (e.g., `ICanvas<PixelType>`)
- Type aliases: PascalCase with suffix (e.g., `Canvas4_128x64`, `Canvas8_128x128`)

**Functions:**
- camelCase for public methods (e.g., `addComponent()`, `getComponent()`)
- camelCase for private methods (e.g., `cachePositionIfType()`)
- PascalCase for lifecycle overrides (e.g., `Awake()`, `Start()` in enjin/)
- camelCase for lifecycle overrides in enjin2 (e.g., `awake()`, `start()`)

**Variables:**
- snake_case for member variables (e.g., `component_count`, `is_visible`)
- snake_case for local variables (e.g., `scene_id`, `delta_time`)
- CamelCase for template parameters (e.g., `TPixel`, `Args`)

**Constants:**
- ALL_CAPS with underscores (e.g., `MAX_COMPONENTS`, `MAX_DRAWABLES`)
- Namespaced constants: `Colors::BLACK`, `Colors::WHITE`

## Where to Add New Code

**New Feature:**
- Primary code: `enjin2/include/enjin2/[module]/` (header)
- Tests: `enjin2/tests/` (create new test file)
- Examples: `enjin2/examples/` (create example demonstrating feature)

**New Component:**
- Implementation: `enjin2/include/enjin2/components/[component_name].hpp`
- Source: `enjin2/src/components/[component_name].cpp` (if needed)
- Inherits from: `enjin2::Component` or specific base (e.g., `enjin2::C_Drawable`)

**New Scene Type:**
- Implementation: Create in application code or example
- Inherits from: `enjin2::Scene`
- Register with: `SceneStateMachine::addScene<T>(sceneId, ...)`

**New Graphics Operation:**
- Interface: Add to `enjin2/include/enjin2/graphics/canvas.hpp` (ICanvas template)
- Implementation: Add to Canvas4 and/or Canvas8 specializations
- Source: `enjin2/src/graphics/canvas.cpp` (if non-template)

**New UI Widget:**
- Header: `enjin2/include/enjin2/ui/[widget_name].hpp`
- Source: `enjin2/src/ui/[widget_name].cpp`
- Inherits from: `enjin2::ui::Component` or `enjin2::ui::Widget`

**Utilities:**
- Shared helpers: `enjin2/include/enjin2/utils/[helper_name].hpp`
- Implementation: `enjin2/src/utils/[helper_name].cpp`

**Effect:**
- Header: `enjin2/include/enjin2/effects/[effect_name].hpp`
- Source: `enjin2/src/effects/[effect_name].cpp`
- Usage: Apply to canvas in rendering

## Special Directories

**enjin2/luajit/**:
- Purpose: Embedded LuaJIT runtime
- Generated: No (source included)
- Committed: Yes (for embedded WASM builds)
- Notes: Amalgamated source for single-file embedding

**enjin2/build/**:
- Purpose: CMake build artifacts
- Generated: Yes
- Committed: No (in .gitignore)

**enjin2/build_wasm/**:
- Purpose: WebAssembly build configuration
- Generated: Yes
- Committed: Partially (scripts committed, outputs ignored)

**enjin/Components/**:
- Purpose: Component implementations for original enjin
- Contains: Component-specific headers and implementations
- Notes: Uses C_ prefix for components (e.g., `C_Position`, `C_Drawable`)

**enjin/UI/**:
- Purpose: UI widget implementations for original enjin
- Contains: Widget headers
- Notes: More extensive widget library than enjin2/ui/

**enjin/utils/**:
- Purpose: Utility functions for original enjin
- Contains: Helper classes and functions
- Notes: Similar functionality to enjin2/utils/

---

*Structure analysis: 2026-01-29*
