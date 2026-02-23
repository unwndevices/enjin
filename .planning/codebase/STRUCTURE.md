# Codebase Structure

**Analysis Date:** 2026-02-23

## Directory Layout

```
enjin2/                         # Project root
├── include/                    # Public headers (header-only library surface)
│   └── enjin2/
│       ├── abstract/           # Hardware-independent interfaces (ICanvas)
│       ├── animation/          # AnimationTrack, Keyframe types
│       ├── compat/             # enjin1 migration shims (deprecated)
│       ├── components/         # Concrete component types (C_Drawable, C_Sprite, ...)
│       ├── core/               # Object, Component, Scene, Signal, types, memory
│       ├── effects/            # PostFx post-processing effects
│       ├── graphics/           # Canvas4, Canvas8, primitives, text renderer, sprite
│       ├── scripting/          # LuaEngine, bindings, LuaInterpreter
│       ├── ui/                 # ECS system/component infrastructure, widget theme
│       └── utils/              # drawing_helpers, noise, polar math
├── src/                        # Implementation (.cpp) files
│   ├── animation/              # keyframe.cpp
│   ├── bindings/               # emscripten_bindings.cpp, enjin2.d.ts, pre.js
│   ├── components/             # canvas.cpp, drawable.cpp, image_cache.cpp, lua_script.cpp
│   ├── core/                   # math.cpp, memory.cpp, object.cpp, scene.cpp, types.cpp
│   ├── effects/                # postfx.cpp
│   ├── graphics/               # canvas.cpp, effects.cpp, primitives.cpp
│   ├── scripting/              # bindings.cpp, lua_engine.cpp, lua_platform.cpp
│   ├── ui/                     # component.cpp, layout.cpp, system.cpp, theme.cpp, widget.cpp
│   └── utils/                  # drawing_helpers.cpp, noise.cpp, polar.cpp
├── tests/                      # Test executables (CMake subdirectory)
├── examples/                   # Standalone demo programs
│   ├── basic_drawing/          # Minimal Canvas8 usage
│   ├── ecs_demo/               # ECS entity/system demo
│   ├── lua_scripting/          # Lua integration example
│   └── esp32_idf_example/      # ESP32 IDF project scaffold
├── luajit/                     # Vendored LuaJIT source (used for WASM builds)
├── docs/                       # Docusaurus + Doxygen documentation site
│   ├── api/                    # Generated Doxygen XML → MDX conversion
│   ├── docs/                   # Doxygen raw output
│   └── src/                    # Docusaurus site source
├── scripts/                    # Build/doc helper scripts
│   └── generate-api-docs.js    # Doxygen XML → Docusaurus MDX generator
├── vendor/                     # stb_image, stb_image_write headers
├── .github/workflows/          # CI workflow definitions
├── .planning/                  # GSD planning documents
│   ├── codebase/               # This and other codebase analysis docs
│   ├── milestones/             # Milestone definitions
│   └── phases/                 # Phase plans
├── CMakeLists.txt              # Primary build definition
├── DESIGN.md                   # High-level design goals and specs
├── README.md                   # Project readme
├── library.json                # PlatformIO library manifest
└── build_wasm.sh               # Emscripten WASM build helper script
```

## Directory Purposes

**`include/enjin2/core/`:**
- Purpose: Foundational building blocks used by all other modules
- Contains: `types.hpp` (Point, Size, Rect, Pixel4, PackedPixel4, Colors namespace), `object.hpp` (Object class), `component.hpp` (Component base class), `scene.hpp` (Scene class), `scene_state_machine.hpp` (SceneStateMachine), `object_collection.hpp` (ObjectCollection), `signal.hpp` (Signal, SignalConnection), `memory.hpp` (StaticPool), `math.hpp`/`math.cpp`
- Key files: `include/enjin2/core/types.hpp`, `include/enjin2/core/object.hpp`, `include/enjin2/core/scene.hpp`

**`include/enjin2/abstract/`:**
- Purpose: Pure abstract interface definitions shared across enjin1/enjin2
- Contains: `icanvas.hpp` (ICanvas<TPixel> pure-virtual interface), `module_group.hpp` (aggregate include)
- Key files: `include/enjin2/abstract/icanvas.hpp`

**`include/enjin2/graphics/`:**
- Purpose: Pixel-level drawing implementations targeting `ICanvas`
- Contains: `canvas.hpp` (Canvas4<W,H> and Canvas8<W,H> implementations), `primitives.hpp`, `effects.hpp`, `sprite.hpp`, `text_renderer.hpp`, `image_export.hpp`, `gfxfont.h`, `glcdfont.hpp`, `defaultfont.hpp`, `canvas_esp32s3.hpp`, `canvas_extended.hpp`
- Key files: `include/enjin2/graphics/canvas.hpp`

**`include/enjin2/components/`:**
- Purpose: All concrete components attached to Objects
- Contains: `drawable.hpp` (C_Drawable base), `position.hpp` (C_Position), `sprite.hpp` (C_Sprite), `label.hpp` (C_Label), `canvas.hpp` (C_Canvas), `animation.hpp` (C_Animation), `lua_script.hpp` (C_LuaScript), `image_cache.hpp` (C_ImageCache), `probe.hpp` (C_Probe), `planet.hpp`/`satellite.hpp` (orbital simulation), `draw.hpp`, `slider.hpp` (C_Slider), `button_dial.hpp` (C_ButtonDial), `fill_up_gauge.hpp` (C_FillUpGauge), `tickmarks.hpp` (C_Tickmarks), `module_group.hpp`
- Key files: `include/enjin2/components/drawable.hpp`, `include/enjin2/components/position.hpp`

**`include/enjin2/ui/`:**
- Purpose: ECS-style system framework and widget infrastructure
- Contains: `component.hpp` (ComponentBase, Component<T>, Entity, ComponentStorage<T,N>, ComponentQuery), `system.hpp` (SystemBase, System<T>, EntityManager, SystemManager), `theme.hpp`, `components.hpp`/`systems.hpp` (aggregate headers)
- Key files: `include/enjin2/ui/component.hpp`, `include/enjin2/ui/system.hpp`

**`include/enjin2/scripting/`:**
- Purpose: Lua VM wrapper and C++ binding registration
- Contains: `lua_engine.hpp` (LuaEngine), `lua_interpreter.hpp`, `lua_platform.hpp`, `bindings.hpp`, `script_interface.hpp`, `module_group.hpp`
- Key files: `include/enjin2/scripting/lua_engine.hpp`, `include/enjin2/scripting/bindings.hpp`

**`include/enjin2/effects/`:**
- Purpose: Post-render screen effects
- Contains: `postfx.hpp` (PostFx class), `module_group.hpp`
- Key files: `include/enjin2/effects/postfx.hpp`

**`include/enjin2/animation/`:**
- Purpose: Keyframe animation playback
- Contains: `animation_track.hpp` (AnimationTrack<T,KeyframeType>), `keyframe.hpp` (PositionKeyframe, FloatKeyframe, ColorKeyframe, easing), `module_group.hpp`
- Key files: `include/enjin2/animation/animation_track.hpp`

**`include/enjin2/utils/`:**
- Purpose: Mathematical and drawing utilities
- Contains: `drawing_helpers.hpp`, `noise.hpp` (Perlin/simplex noise), `polar.hpp` (polar coordinate math), `module_group.hpp`

**`include/enjin2/compat/`:**
- Purpose: Backward-compatibility shims for migrating from enjin1
- Contains: `scene.hpp` (PascalCase wrappers: OnCreate, OnDestroy, OnActivate, OnDeactivate, Update), `component.hpp`, `types.hpp`
- Key files: `include/enjin2/compat/scene.hpp` — do not use for new code

**`src/bindings/`:**
- Purpose: WebAssembly/Emscripten JavaScript interop
- Contains: `emscripten_bindings.cpp` (Embind exports), `enjin2.d.ts` (TypeScript type definitions), `pre.js` (Emscripten pre-JS hook)
- Key files: `src/bindings/emscripten_bindings.cpp`, `src/bindings/enjin2.d.ts`

**`vendor/`:**
- Purpose: Header-only third-party libraries
- Contains: `stb_image.h`, `stb_image_write.h`
- Generated: No
- Committed: Yes

**`luajit/`:**
- Purpose: Vendored LuaJIT source for WASM builds (amalgamated build `luajit/src/ljamalg.c`)
- Generated: No (vendored)
- Committed: Yes
- Note: Desktop builds use system Lua found by CMake `find_package(Lua)`

**`build/` and `build_wasm/`:**
- Purpose: CMake out-of-tree build directories
- Generated: Yes
- Committed: No

## Key File Locations

**Entry Points:**
- `CMakeLists.txt`: All build targets; static library definitions for `enjin2_core`, `enjin2_graphics`, `enjin2_ui`, `enjin2_lua`, interface target `enjin2`
- `examples/basic_drawing/`: Minimal working demo showing canvas + scene setup
- `examples/ecs_demo/`: ECS entity/system demonstration
- `src/bindings/emscripten_bindings.cpp`: WASM entry point

**Configuration:**
- `CMakeLists.txt`: Build options `ENJIN2_BUILD_TESTS`, `ENJIN2_BUILD_EXAMPLES`, `ENJIN2_BUILD_LUA`, `ENJIN2_BUILD_WASM`, `ENJIN2_USE_SIMD`
- `library.json`: PlatformIO manifest for Arduino/ESP32 library manager
- `build_wasm.sh`: Emscripten build invocation script
- `docs/Doxyfile` (inferred): Doxygen config consumed by `cmake --build . --target docs`
- `scripts/generate-api-docs.js`: Post-Doxygen Docusaurus MDX generation

**Core Logic:**
- `include/enjin2/core/object.hpp`: Object (entity) with component management
- `include/enjin2/core/component.hpp`: Component base class with lifecycle methods
- `include/enjin2/core/scene.hpp`: Scene base class
- `include/enjin2/core/scene_state_machine.hpp`: Scene manager
- `include/enjin2/core/signal.hpp`: Observer pattern
- `include/enjin2/graphics/canvas.hpp`: Canvas4 and Canvas8 implementations
- `include/enjin2/components/drawable.hpp`: Renderable component base

**Testing:**
- `tests/CMakeLists.txt`: Test build configuration
- `tests/*.cpp`: Test source files

## Naming Conventions

**Files:**
- `snake_case.hpp` / `snake_case.cpp` for all header and source files
- `module_group.hpp` in each module directory is an aggregate include for that module
- Platform-specific files use a suffix: `canvas_esp32s3.hpp`, `lua_platform.hpp`

**Directories:**
- `snake_case` for all directories under `include/enjin2/` and `src/`
- Module names match between `include/enjin2/<module>/` and `src/<module>/`

**Types and Classes:**
- PascalCase for classes and structs: `Object`, `Component`, `Scene`, `SceneStateMachine`, `Canvas4`, `Canvas8`, `AnimationTrack`, `PostFx`
- Component classes prefixed with `C_`: `C_Drawable`, `C_Position`, `C_Sprite`, `C_Label`, `C_Canvas`, `C_Animation`, `C_LuaScript`
- Enum classes PascalCase: `DrawLayer`, `BlendMode`, `Anchor`, `EffectType`
- Enum values PascalCase: `DrawLayer::Background`, `BlendMode::Add`
- Type aliases `snake_case`: `PositionTrack`, `FloatTrack`, `ColorTrack`, `ComponentID`, `SystemID`

**Methods:**
- `camelCase` for all methods: `addComponent()`, `getComponent()`, `awake()`, `start()`, `update()`, `lateUpdate()`, `onEnable()`, `onDisable()`
- Lifecycle hooks prefixed `on`: `onCreate()`, `onActivate()`, `onUpdate()`, `onRender()`
- Getters prefixed `get`: `getOwner()`, `getWidth()`, `getDrawableCount()`
- Boolean getters prefixed `is`/`has`: `isEnabled()`, `hasComponent<T>()`, `isActive()`

**Constants:**
- `SCREAMING_SNAKE_CASE` for `constexpr` capacity constants: `MAX_COMPONENTS`, `MAX_OBJECTS`, `MAX_SCENES`, `MAX_CONNECTIONS`

**Namespaces:**
- `enjin2` for all production code
- `enjin` for compat shims only (`include/enjin2/compat/`)
- `enjin2::Colors` for color constants
- `enjin2::Signals` for common signal type aliases

## Where to Add New Code

**New Component:**
- Header: `include/enjin2/components/my_component.hpp` — extend `C_Drawable` if renderable, else extend `Component` directly
- Implementation: `src/components/my_component.cpp`
- Register in aggregate: add `#include` to `include/enjin2/components/module_group.hpp`
- Add to `CMakeLists.txt` under `enjin2_ui` target sources if it has a `.cpp`

**New Scene:**
- Create a class that extends `enjin2::Scene` (header-only or in user code; not in `include/enjin2/`)
- Override `onCreate()` to add objects and components
- Register with `SceneStateMachine::addScene<MyScene>(id)`

**New Graphics Primitive or Canvas Operation:**
- Add to `include/enjin2/graphics/primitives.hpp` and `src/graphics/primitives.cpp`
- For canvas-level operations: add methods to `Canvas8<W,H>` and/or `Canvas4<W,H>` in `include/enjin2/graphics/canvas.hpp`

**New PostFx Effect:**
- Add `EffectType` enum value in `include/enjin2/effects/postfx.hpp`
- Add static `applyXxx(ICanvas<uint8_t>&, const PostFxParams&)` method declaration in same file
- Implement in `src/effects/postfx.cpp`
- Add case to `PostFx::applyEffectChain()`

**New Utility:**
- Add header to `include/enjin2/utils/my_util.hpp`
- Add implementation to `src/utils/my_util.cpp`
- Add to `CMakeLists.txt` under `enjin2_ui` (or `enjin2_core` if dependency-free)
- Include in `include/enjin2/utils/module_group.hpp`

**New Lua Binding:**
- Register functions in `src/scripting/bindings.cpp` via `lua_engine.registerFunction()`
- Declare any new script interface abstractions in `include/enjin2/scripting/script_interface.hpp`

**New Example:**
- Create a subdirectory under `examples/my_example/`
- Add `CMakeLists.txt` that links against `enjin2` interface target
- Reference from `examples/CMakeLists.txt` via `add_subdirectory`

**New Test:**
- Add `.cpp` file under `tests/`
- Register in `tests/CMakeLists.txt`

## Special Directories

**`.planning/`:**
- Purpose: GSD planning documents (milestones, phases, codebase analysis)
- Generated: No (human/AI authored)
- Committed: Yes

**`build/` / `build_wasm/`:**
- Purpose: CMake out-of-tree build outputs
- Generated: Yes
- Committed: No (in `.gitignore`)

**`docs/node_modules/` / `node_modules/`:**
- Purpose: Docusaurus and documentation tooling NPM packages
- Generated: Yes
- Committed: No (in `.gitignore`)

**`docs/xml/` / `docs/docs/`:**
- Purpose: Doxygen output (XML, LaTeX)
- Generated: Yes (by `cmake --build . --target docs`)
- Committed: No

**`docs/api/`:**
- Purpose: Generated Docusaurus MDX API pages
- Generated: Yes (by `scripts/generate-api-docs.js`)
- Committed: Yes (serves as documentation source for Docusaurus site)

---

*Structure analysis: 2026-02-23*
