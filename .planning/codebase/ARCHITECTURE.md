# Architecture

**Analysis Date:** 2026-02-23

## Pattern Overview

**Overall:** Component-Based Entity System with Scene State Machine

**Key Characteristics:**
- All memory allocation is static; no heap fragmentation. Limits are compile-time constants (e.g., 16 components per object, 128 objects per scene, 32 scenes per state machine).
- Hardware-abstracted rendering: the `ICanvas<TPixel>` interface decouples all drawing code from hardware. Swapping display backends requires only a new `ICanvas` implementation.
- Signal/observer pattern for decoupled inter-component communication. All signals use static arrays with a maximum of 16 connections per signal.
- Dual rendering pathways: `Canvas4<W,H>` (4-bit packed pixels, 50% memory savings) and `Canvas8<W,H>` (8-bit, Adafruit GFX compatible).
- Optional Lua scripting layer and optional WebAssembly (Emscripten) bindings compiled as separate static libraries.

## Layers

**Abstract Interfaces:**
- Purpose: Hardware-independent contracts
- Location: `include/enjin2/abstract/`
- Contains: `ICanvas<TPixel>` drawing interface; `module_group.hpp` aggregate header
- Depends on: `core/types.hpp`
- Used by: All graphics, scene, and component code

**Core:**
- Purpose: Foundational types, lifecycle primitives, object/component model, scene management
- Location: `include/enjin2/core/`, `src/core/`
- Contains: `Object`, `Component`, `Scene`, `SceneStateMachine`, `ObjectCollection`, `Signal`, `types.hpp` (Point, Size, Rect, Pixel4), `memory.hpp`, `math.cpp`
- Depends on: Nothing (no external dependencies)
- Used by: All other layers

**Graphics:**
- Purpose: Canvas implementations and drawing primitives
- Location: `include/enjin2/graphics/`, `src/graphics/`
- Contains: `Canvas4<W,H>`, `Canvas8<W,H>`, `primitives.hpp`, `effects.hpp`, `text_renderer.hpp`, `sprite.hpp`, `image_export.hpp`, ESP32 canvas variant (`canvas_esp32s3.hpp`)
- Depends on: `core`
- Used by: UI, components, effects, scripting

**Components:**
- Purpose: Concrete drawable and game-logic components attached to Objects
- Location: `include/enjin2/components/`, `src/components/`
- Contains: `C_Drawable` (base), `C_Position`, `C_Sprite`, `C_Label`, `C_Canvas`, `C_Animation`, `C_LuaScript`, `C_ImageCache`, `C_Probe`, `C_Planet`, `C_Satellite`, `C_Draw`, widget components (`C_Slider`, `C_ButtonDial`, `C_FillUpGauge`, `C_Tickmarks`)
- Depends on: `core`, `graphics`
- Used by: User game code; UI layer

**UI:**
- Purpose: ECS-style entity/system framework and UI widget infrastructure
- Location: `include/enjin2/ui/`, `src/ui/`
- Contains: `ComponentBase`/`Component<T>` (ECS data components), `SystemBase`/`System<T>`, `EntityManager`, `SystemManager`, `ComponentStorage<T,N>`, `ComponentQuery`, UI `theme.hpp`, widget layout and rendering
- Depends on: `core`, `graphics`
- Used by: User UI code; scripting layer

**Scripting:**
- Purpose: Lua embedding for game-logic scripting
- Location: `include/enjin2/scripting/`, `src/scripting/`
- Contains: `LuaEngine`, `LuaInterpreter`, `lua_platform.hpp`, `bindings.hpp`, `ScriptInterface`
- Depends on: `core`, `graphics`, `ui`, LuaJIT/system Lua
- Used by: `C_LuaScript` component; WASM bindings

**Effects:**
- Purpose: Post-processing visual effects applied to canvases after scene render
- Location: `include/enjin2/effects/`, `src/effects/`
- Contains: `PostFx` class with CRT scanlines, barrel distortion, noise, blur, glow, dithering, contrast/brightness
- Depends on: `core`, `graphics`
- Used by: Game main loop after scene render

**Animation:**
- Purpose: Keyframe-based value interpolation
- Location: `include/enjin2/animation/`, `src/animation/`
- Contains: `AnimationTrack<T,KeyframeType>`, `Keyframe` types (PositionKeyframe, FloatKeyframe, ColorKeyframe), easing functions
- Depends on: `core`
- Used by: `C_Animation` component

**Utils:**
- Purpose: Standalone mathematical helpers
- Location: `include/enjin2/utils/`, `src/utils/`
- Contains: `drawing_helpers.hpp`, `noise.hpp`, `polar.hpp`
- Depends on: `core`
- Used by: Components, effects, user code

**Compat:**
- Purpose: Migration shim for enjin1 PascalCase API
- Location: `include/enjin2/compat/`
- Contains: `scene.hpp` (OnCreate/OnDestroy/OnActivate wrappers), `component.hpp`, `types.hpp`
- Depends on: `core`
- Used by: Legacy code only; not for new development

## Data Flow

**Frame Update:**

1. Host calls `SceneStateMachine::update(deltaTime)`
2. `SceneStateMachine` updates transition state, then delegates to `currentScene->update(deltaTime)`
3. `Scene::update()` calls `onUpdate(deltaTime)` (user override), then `ObjectCollection::update(deltaTime)` and `ObjectCollection::lateUpdate(deltaTime)`
4. `ObjectCollection` iterates all active `Object*`, calling `object->update(deltaTime)` then `lateUpdate(deltaTime)`
5. Each `Object` iterates its component array, calling each `Component::update(deltaTime)` then `Component::lateUpdate(deltaTime)`

**Frame Render:**

1. Host calls `SceneStateMachine::render(canvas)`
2. State machine applies transition rendering (fade/slide overlays) or passes through to `currentScene->render(canvas)`
3. `Scene::renderObjects(canvas)` collects all `C_Drawable*` from active objects, sorts by `DrawLayer` then `sort_order`
4. Each `C_Drawable::draw(ICanvas<uint8_t>&)` is called in sorted order
5. Optionally host applies `PostFx` effects to canvas after scene render

**Signal/Event Flow:**

- Components emit `Signal<Args...>` when state changes (e.g., value changed, animation complete)
- Observers connect via `signal.connect(callback)` returning a connection ID
- `SignalConnection<Args...>` RAII wrapper auto-disconnects on destruction
- Up to 16 listeners per signal; static array storage

**State Management:**
- Scene transitions managed by `SceneStateMachine` with `TransitionType` (IMMEDIATE, FADE_OUT_IN, SLIDE_LEFT/RIGHT/UP/DOWN)
- Component state is plain member data; no shared mutable global state
- Lua scripts modify component state via registered C bindings

## Key Abstractions

**ICanvas<TPixel>:**
- Purpose: Hardware-independent drawing target
- Examples: `include/enjin2/abstract/icanvas.hpp`, `include/enjin2/graphics/canvas.hpp`
- Pattern: Abstract base with pure virtual `setPixel`, `getPixel`, `clear`, `fill`, `drawLine`, `drawRect`, `fillRect`, `drawCircle`, `fillCircle`, `drawBitmap`, `drawText`. Concrete `Canvas4<W,H>` and `Canvas8<W,H>` are template specializations with static pixel buffers.

**Object + Component:**
- Purpose: Entity with attached behaviour modules
- Examples: `include/enjin2/core/object.hpp`, `include/enjin2/core/component.hpp`
- Pattern: `Object` holds `std::array<std::unique_ptr<Component>, 16>`. `addComponent<T>()`, `getComponent<T>()`, `removeComponent<T>()` are template methods using `dynamic_cast`. Position and drawable components are cached for O(1) access.

**Scene:**
- Purpose: Container for a gameplay state
- Examples: `include/enjin2/core/scene.hpp`
- Pattern: Subclass and override `onCreate()`, `onActivate()`, `onDeactivate()`, `onDestroy()`, `onUpdate(deltaTime)`, `onRender(canvas)`. Scene owns an `ObjectCollection`.

**SceneStateMachine:**
- Purpose: Scene lifecycle and transition orchestrator
- Examples: `include/enjin2/core/scene_state_machine.hpp`
- Pattern: Holds up to 32 scenes. `addScene<T>(id)` factory. `changeScene(id, TransitionType)` triggers deactivate/activate with optional animated transition.

**Signal<Args...>:**
- Purpose: Type-safe observer/event mechanism
- Examples: `include/enjin2/core/signal.hpp`
- Pattern: Template variadic class. `connect(callback)` returns int connection ID. `emit(args...)` calls all connected callbacks. `SignalConnection<Args...>` RAII auto-disconnect.

**C_Drawable:**
- Purpose: Base for all renderable components
- Examples: `include/enjin2/components/drawable.hpp`
- Pattern: Extends `Component`. Pure virtual `draw(ICanvas<uint8_t>&)`. Has `DrawLayer`, `BlendMode`, `sort_order`, `Anchor`, `is_visible`. `shouldDrawBefore()` drives sort order in `Scene::renderObjects`.

**AnimationTrack<T, KeyframeType>:**
- Purpose: Keyframe interpolation for any value type
- Examples: `include/enjin2/animation/animation_track.hpp`
- Pattern: Template with max 16 keyframes. Supports NONE/LOOP/PING_PONG. Emits `onStartSignal`, `onCompleteSignal`, `onUpdateSignal`. Type aliases: `PositionTrack`, `FloatTrack`, `ColorTrack`.

## Entry Points

**Desktop/Linux Main Loop:**
- Location: `examples/` (e.g., `examples/basic_drawing/`, `examples/ecs_demo/`)
- Triggers: User application binary
- Responsibilities: Create a `Canvas8<W,H>`, instantiate `SceneStateMachine`, add scenes, call `update()` and `render()` each frame

**WebAssembly:**
- Location: `src/bindings/emscripten_bindings.cpp`, TypeScript types at `src/bindings/enjin2.d.ts`
- Triggers: Browser JS calls `Enjin2Module` exports
- Responsibilities: Expose engine lifecycle and canvas buffer to JavaScript

**ESP32 IDF:**
- Location: `examples/esp32_idf_example/`
- Triggers: FreeRTOS task or `app_main()`
- Responsibilities: Map ICanvas to DMA display driver, run update/render loop

**CMake Build:**
- Location: `CMakeLists.txt`
- Static libraries produced: `enjin2_core`, `enjin2_graphics`, `enjin2_ui`, `enjin2_lua` (optional)
- Interface target `enjin2` links all of the above

## Error Handling

**Strategy:** Silent failure with null/false returns. No exceptions. No panics (except Lua panic handler).

**Patterns:**
- `addComponent<T>()` returns `nullptr` if the 16-component limit is reached
- `addObject<T>()` returns `nullptr` if the 128-object limit is reached
- `addScene<T>()` returns `nullptr` if the 32-scene limit is reached or ID is duplicate
- `Signal::connect()` returns `-1` if max 16 connections are already used
- `LuaResult` struct carries `bool success` and `std::string error`

## Cross-Cutting Concerns

**Logging:** `std::cout`/`std::cerr` used in a few source files; no unified logging framework.
**Validation:** Compile-time via `static_assert` (e.g., `T must derive from Component`); bounds checks via index comparisons.
**Authentication:** Not applicable (embedded game engine).
**Platform Isolation:** `VCV_RACK` preprocessor define used to exclude `<Arduino.h>` on non-Arduino builds. `EMSCRIPTEN` and `ESP32` defines gate platform-specific code paths.

---

*Architecture analysis: 2026-02-23*
