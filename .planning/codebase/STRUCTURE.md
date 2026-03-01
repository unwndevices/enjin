# Codebase Structure

**Analysis Date:** 2026-03-01

## Directory Layout

```
enjin/
├── include/enjin2/          # Public API headers (organized by subsystem)
│   ├── core/                # Entity model, scenes, lifecycle
│   ├── components/          # Component implementations
│   ├── graphics/            # Canvas, sprites, rendering
│   ├── scripting/           # Lua engine, bindings, bridges
│   ├── input/               # Input state, button polling
│   ├── animation/           # Keyframe animation utilities
│   ├── utils/               # Math, noise, collision, physics helpers
│   ├── ui/                  # UI system (buttons, dialogs, layouts)
│   ├── effects/             # Post-processing effects
│   ├── platform/            # Platform-specific APIs
│   ├── abstract/            # Abstract interfaces (ICanvas)
│   └── bindings/            # Emscripten/platform bindings
├── src/                     # Implementation files (mirror include/ structure)
│   ├── core/                # Object, Scene, SceneStateMachine, memory management
│   ├── components/          # Component .cpp files
│   ├── graphics/            # Canvas, sprite rendering implementation
│   ├── scripting/           # Lua engine, bindings_*.cpp modules
│   ├── input/               # Input implementation
│   ├── animation/           # Animation implementation
│   ├── utils/               # Utils implementation
│   ├── ui/                  # UI implementation
│   ├── effects/             # Effects implementation
│   ├── platform/sdl/        # SDL platform implementation
│   └── bindings/            # Platform-specific bindings
├── tests/                   # Test files (mirror src/ structure)
│   ├── *_test.cpp           # Unit tests using Google Test
│   └── *_sdl_test.cpp       # SDL-specific integration tests
├── examples/                # Example applications
│   ├── basic_drawing/       # Sprite and drawing demo
│   ├── ecs_demo/            # ECS architecture example
│   ├── lua_scripting/       # Lua scripting example
│   └── esp32_idf_example/   # ESP32 integration example
├── .planning/               # GSD phase tracking and codebase docs
│   ├── phases/              # Active phases (numbered directories)
│   ├── milestones/          # Completed milestone phases
│   ├── codebase/            # Generated analysis docs (ARCHITECTURE.md, etc.)
│   └── **/*.md              # Phase documentation
├── tools/                   # Utility scripts and tools
├── scripts/                 # Game scripts (Lua examples: tamagotchi.lua, arkanoid.lua)
├── docs/                    # Doxygen documentation output
├── vendor/                  # Third-party library vendoring
├── luajit/                  # LuaJIT submodule (Lua runtime)
├── CMakeLists.txt          # Build configuration
├── package.json            # Node.js dev dependencies (Doxygen, etc.)
└── README.md               # Project overview
```

## Directory Purposes

**include/enjin2/core/:**
- Purpose: Core engine abstractions
- Contains: `object.hpp` (Object, Component base), `scene.hpp` (Scene, ObjectCollection), `scene_state_machine.hpp` (SceneStateMachine), `types.hpp` (Point, Vec2, Rect, etc.), `math.hpp` (math utilities), `collision.hpp` (collision detection), `physics.hpp` (physics helpers), `memory.hpp` (memory management)
- Key files: `object.hpp` (40 KB), `scene.hpp` (30 KB), `scene_state_machine.hpp` (40 KB)

**include/enjin2/components/:**
- Purpose: Reusable component implementations
- Contains: `drawable.hpp`, `sprite.hpp`, `position.hpp`, `camera.hpp`, `lua_script.hpp`, `timer.hpp`, `state_machine.hpp`, `tilemap.hpp`, `canvas.hpp`, `image_cache.hpp`, etc.
- Key files: `sprite.hpp` (animated rendering), `drawable.hpp` (render base), `lua_script.hpp` (Lua integration), `camera.hpp` (2D camera with shake/follow)

**include/enjin2/graphics/:**
- Purpose: Rendering backend
- Contains: `canvas.hpp` (4-bit/8-bit fixed-size buffers), `sprite.hpp` (SpriteSheet, blit), `primitives.hpp` (line, rect, circle, triangle drawing), `text_renderer.hpp` (text rendering), `palette.hpp` (color palettes), `layer_compositor.hpp` (multi-layer rendering)
- Key files: `canvas.hpp` (template-based Canvas4<W,H>, Canvas8<W,H>), `sprite.hpp` (SpriteSheet with frame animation)

**include/enjin2/scripting/:**
- Purpose: Lua runtime and C++ bindings
- Contains: `lua_engine.hpp` (Lua state + memory pool), `lua_platform.hpp` (platform-specific Lua globals), `bindings.hpp` (LuaBindings, LuaCanvas, LuaStore), `lua_interpreter.hpp` (alternate Lua impl), `object_proxy.hpp` (Lua userdata for Objects), `component_proxy.hpp` (Lua userdata for components), `bind_helpers.hpp` (macro helpers), `lua_event_bus.hpp` (pub/sub system)
- Key files: `bindings.hpp` (800+ lines, all Lua C-function declarations), `lua_engine.hpp` (engine init/execute)

**src/scripting/:**
- Purpose: Binding implementations (split across multiple files to reduce compilation time)
- Contains: `bindings_engine.cpp` (engine.* table), `bindings_draw.cpp` (graphics functions), `bindings_math.cpp` (math type metatables), `bindings_input_sprites.cpp` (input + sprite pool), `bindings_layers_text.cpp` (layer + text system), `bindings_physics.cpp` (physics helpers), `bindings_system.cpp` (core system), `bindings_store.cpp` (persistent store), `lua_engine.cpp` (implementation), `lua_event_bus.cpp` (event system)
- Design: Each binding group organized by subsystem (engine, draw, math, physics, etc.) to parallelize compilation

**include/enjin2/input/:**
- Purpose: Input state and button polling
- Contains: `input_state.hpp` (16 buttons + analog axes, edge tracking), `input.hpp` (platform-agnostic input interface)
- Key files: `input_state.hpp` (tracks buttons, just_pressed, just_released per frame)

**include/enjin2/utils/:**
- Purpose: Game development utilities
- Contains: `noise.hpp` (Perlin-like noise), `polar.hpp` (polar coordinate helpers), `drawing_helpers.hpp` (drawing macros), `module_group.hpp` (API organization)
- Key files: `noise.hpp` (procedural noise for terrain)

**src/core/:**
- Purpose: Core system implementations
- Contains: `object.cpp`, `scene.cpp`, `memory.cpp`, `types.cpp`, `math.cpp`
- Key files: `scene.cpp` (rendering pipeline with camera offset), `object.cpp` (component management)

**src/components/:**
- Purpose: Component implementations
- Contains: `sprite.cpp`, `drawable.cpp`, `camera.cpp`, `lua_script.cpp`, `timer.cpp`, `state_machine.cpp`, `tilemap.cpp`, `canvas.cpp`, `image_cache.cpp`
- Key files: `lua_script.cpp` (LuaScriptSystem integration, lifecycle management), `camera.cpp` (camera lerp + shake), `sprite.cpp` (animation advancement)

**src/graphics/:**
- Purpose: Graphics backend implementations
- Contains: `canvas.cpp` (pixel operations, blitting), `primitives.cpp` (drawing shapes), `palette.cpp` (palette management), `effects.cpp` (post-processing)
- Key files: `canvas.cpp` (blending modes, coordinate validation)

**src/platform/sdl/:**
- Purpose: SDL desktop platform integration
- Contains: `sdl_main.cpp` (example host application), SDL event loop, input polling, canvas management
- Key files: `sdl_main.cpp` (complete working example)

**tests/:**
- Purpose: Unit and integration tests
- Contains: `*_test.cpp` files using Google Test framework
- Pattern: One test file per header (e.g., `sprite_test.cpp` for `include/enjin2/components/sprite.hpp`)
- Examples: `object_test.cpp`, `scene_test.cpp`, `lua_engine_test.cpp`, `sprite_sdl_test.cpp`

**examples/:**
- Purpose: Runnable example applications
- Contains: `basic_drawing/` (render demo), `ecs_demo/` (component system), `lua_scripting/` (Lua integration), `esp32_idf_example/` (ESP32 IDF build)
- Design: Each example is self-contained with its own main.cpp and demonstrates specific features

**.planning/:**
- Purpose: GSD phase tracking and codebase documentation
- Contains:
  - `phases/NN-phase-name/` directories for in-progress work
  - `milestones/` containing completed phases from prior versions
  - `codebase/` directory with auto-generated docs (ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, TESTING.md, CONCERNS.md, STACK.md, INTEGRATIONS.md)
  - Individual phase markdown files documenting implementation plans and status
- Pattern: Each phase lives in numbered directory (e.g., `45-optimized-2d-physics-engine/`) with phase.md file

## Key File Locations

**Entry Points:**
- `src/platform/sdl/sdl_main.cpp`: Complete working SDL desktop example (main loop, scene setup, input dispatch)
- `examples/basic_drawing/main.cpp`: Simpler drawing demo
- `examples/lua_scripting/main.cpp`: Lua scripting example with scene + C_LuaScript usage

**Configuration:**
- `CMakeLists.txt`: Build configuration, target definitions, platform selection
- `include/enjin2/scripting/lua_platform.hpp`: Platform-specific Lua globals and memory limits
- `.clang-tidy`: Linting configuration for code quality checks

**Core Logic:**
- `include/enjin2/core/object.hpp`: Object container with component storage, lifecycle
- `include/enjin2/core/scene.hpp`: Scene collection, rendering pipeline, camera integration
- `include/enjin2/core/scene_state_machine.hpp`: Scene transitions, deferred execution
- `include/enjin2/scripting/bindings.hpp`: Lua function declarations (800+ lines), canvas wrapper, persistent store
- `src/scripting/bindings_engine.cpp`: engine.* global table setup (scene, input, time, collision, physics, state, event, camera)

**Testing:**
- `tests/object_test.cpp`: Object lifecycle, component management tests
- `tests/scene_test.cpp`: Scene lifecycle, rendering order tests
- `tests/lua_engine_test.cpp`: Lua execution, error handling tests
- `tests/sprite_sdl_test.cpp`: SDL integration tests for rendering

## Naming Conventions

**Files:**
- Headers: `include/enjin2/subsystem/name.hpp` (lowercase with underscores)
- Implementation: `src/subsystem/name.cpp` (matches header name)
- Tests: `tests/name_test.cpp` (matches tested component)
- Examples: `examples/feature_name/main.cpp` (feature-descriptive)

**Directories:**
- Subsystems: Lowercase, plural for collections (components, utils, graphics, scripting)
- Phases: Numbered prefix with kebab-case name (e.g., `45-optimized-2d-physics-engine`)
- Vendor: `vendor/libname/` for third-party code (e.g., `vendor/lua/`)

**C++ Identifiers:**
- Classes: PascalCase (Object, Scene, LuaEngine, C_Sprite)
- Component classes: Prefix C_ (C_Position, C_Drawable, C_Camera)
- Functions: camelCase (getPosition, setActive, isVisible)
- Enums: PascalCase (Anchor, BlendMode, TransitionType)
- Variables: camelCase (componentCount, isActive, position)
- Namespaces: enjin2 (lowercase, single namespace)
- Macros: UPPER_SNAKE_CASE (MAX_COMPONENTS, ENJIN_ARRAY_LEN)

**Lua Identifiers:**
- Globals: engine, canvas, love (compatibility)
- Functions: snake_case (set_layer, get_color, draw_rectangle)
- Tables: snake_case (engine.scene, engine.input, engine.time, engine.physics)

## Where to Add New Code

**New Feature (e.g., particle system):**
1. Header: `include/enjin2/components/particle_system.hpp` (public API)
2. Implementation: `src/components/particle_system.cpp` (internals)
3. Tests: `tests/particle_system_test.cpp` (unit + integration)
4. Bindings (if Lua-exposed): Add to `src/scripting/bindings_system.cpp` or create `src/scripting/bindings_particles.cpp`
5. Example: Add to `examples/particle_demo/` (optional, for major features)
6. Phase tracking: Create `.planning/phases/NN-particle-system/` with phase.md

**New Component Type:**
1. Declare class inheriting from `Component` in `include/enjin2/components/mycomponent.hpp`
2. Implement lifecycle methods (awake, start, update, lateUpdate) in `src/components/mycomponent.cpp`
3. Add tests in `tests/mycomponent_test.cpp` (test component lifecycle, dependencies)
4. Lua binding (if needed): Create bindings_*.cpp or add to existing file, register via LuaBindings::registerAll()
5. Usage pattern: `auto comp = obj->addComponent<C_MyComponent>(constructor_args);`

**Utility Functions:**
- Math/collision: Add to `include/enjin2/core/math.hpp` or `include/enjin2/core/collision.hpp` (inline, header-only)
- General helpers: Add to `include/enjin2/utils/drawing_helpers.hpp` (macro-heavy, for performance)
- Physics: Add to `include/enjin2/core/physics.hpp` (inline helper functions, stateless)

**Lua Bindings:**
1. Declare static function in `LuaBindings` class: `static int lua_function_name(lua_State* L);`
2. Implement in appropriate `src/scripting/bindings_*.cpp` file
3. Register in `LuaBindings::registerAll()` via `lua_setglobal()` or table insertion
4. Pattern: Check arg count, validate types, call C++ logic, push return values, handle errors via LuaResult

**New Scene Type:**
1. Subclass `Scene` in your application code (or `examples/your_game/scene.hpp`)
2. Override `onCreate()`, `onActivate()`, `onUpdate(dt)`, `onRender(canvas)` as needed
3. Create objects via `addObject<T>(constructor_args)`
4. Register with SceneStateMachine via `addScene<YourScene>(id, args...)`
5. Transition via `sceneSSM->switchTo(id)` or `engine.scene.switch("id")` from Lua

## Special Directories

**luajit/:**
- Purpose: LuaJIT runtime source code
- Generated: No (vendored submodule)
- Committed: Yes (git submodule)
- Usage: CMakeLists.txt links against libluajit

**build/:**
- Purpose: CMake build artifacts
- Generated: Yes (CMake output, executables, tests)
- Committed: No (.gitignore excludes)
- Contents: Intermediate objects, linked binaries, test executables

**.planning/:**
- Purpose: GSD phase tracking and codebase analysis
- Generated: Some (ARCHITECTURE.md, STRUCTURE.md, etc. written by `/gsd:map-codebase`)
- Committed: Yes (phases/*.md manually written, codebase/*.md auto-generated)
- Lifecycle: Old phases archived to milestones/ after completion, active phases in phases/

**vendor/:**
- Purpose: Vendored third-party code
- Generated: No (manually added)
- Committed: Yes (transitive deps bundled in repo)
- Maintenance: Update when security patches needed

**examples/:**
- Purpose: Self-contained working applications
- Generated: No (manually written)
- Committed: Yes (build artifacts .gitignored)
- Design: Each example has its own CMakeLists.txt entry or is built alongside main library

**docs/:**
- Purpose: Doxygen-generated HTML documentation
- Generated: Yes (`doxygen Doxyfile`)
- Committed: No (build artifact, regenerated from headers)
- Usage: Run `doxygen` from repo root to update

---

*Structure analysis: 2026-03-01*
