# Architecture

**Analysis Date:** 2025-02-14

## Pattern Overview

**Overall:** Hybrid Object-Component and Entity-Component-System (ECS) Architecture.

**Key Characteristics:**
- **Scene-Object-Component:** Primary high-level structure for game logic and rendering. `Scene` manages `Object` collection; `Object` contains `Component`s.
- **ECS (Pure):** Low-level high-performance architecture found in `ui/` and `components/`, featuring `Entity` handles, `EntityManager`, and `System` bases.
- **Signals & Slots:** Decoupled event system used for lifecycle hooks and inter-component communication.
- **Platform Agnostic:** Abstracted graphics (`ICanvas`) and scripting interfaces to support multiple runtimes (Native, ESP32, WebAssembly).

## Layers

**Core Layer:**
- Purpose: Provides fundamental building blocks, memory management, and lifecycle patterns.
- Location: `src/core/` and `include/enjin2/core/`
- Contains: `Object`, `Scene`, `Signal`, `Memory`, `Math`, `SceneStateMachine`.
- Depends on: Standard Library.
- Used by: All other layers.

**Graphics Layer:**
- Purpose: Handles rendering primitives and canvas abstractions.
- Location: `src/graphics/` and `include/enjin2/graphics/`
- Contains: `ICanvas`, `Primitives`, `Canvas4` (4-bit), `Canvas8` (8-bit).
- Depends on: `Core`.
- Used by: `UI`, `Components`, `Effects`.

**Component Layer:**
- Purpose: Provides reusable behaviors that can be attached to objects.
- Location: `src/components/` and `include/enjin2/components/`
- Contains: `C_Drawable`, `C_Position`, `C_LuaScript`, `C_Sprite`.
- Depends on: `Core`, `Graphics`.
- Used by: `Scene`.

**UI Layer:**
- Purpose: High-level UI framework with widgets and layout management.
- Location: `src/ui/` and `include/enjin2/ui/`
- Contains: `Widget`, `Layout`, `Theme`, `UI::System`.
- Depends on: `Core`, `Graphics`, `Components`.
- Used by: Application code.

**Scripting Layer:**
- Purpose: Integrates Lua for high-level logic and rapid prototyping.
- Location: `src/scripting/` and `include/enjin2/scripting/`
- Contains: `LuaEngine`, `LuaPlatform`, `Bindings`.
- Depends on: `Core`, `Graphics`, `luajit`.
- Used by: `C_LuaScript`.

## Data Flow

**Update Loop:**
1. `SceneStateMachine::update()` calls current `Scene::update()`.
2. `Scene::update()` calls `onUpdate()` (virtual) and then `ObjectCollection::update()`.
3. `ObjectCollection` iterates through all active `Object`s.
4. Each `Object` calls `update()` on its enabled `Component`s.
5. `LateUpdate` follows the same flow for post-processing logic.

**Render Flow:**
1. `Scene::render(canvas)` is invoked.
2. `Scene` collects all `C_Drawable` components from active objects.
3. Drawables are sorted by `layer` and `sortOrder`.
4. Sorted drawables are rendered sequentially to the `ICanvas`.

**State Management:**
- `SceneStateMachine` manages transition between `Scene` instances (e.g., Fade, Slide).
- `Scene` manages its own internal `ObjectCollection`.

## Key Abstractions

**ICanvas:**
- Purpose: Generic interface for drawing operations, supporting different bit depths and hardware targets.
- Examples: `include/enjin2/graphics/canvas.hpp`, `include/enjin2/graphics/canvas_esp32s3.hpp`
- Pattern: Interface / Adapter.

**Component:**
- Purpose: Base class for all object behaviors.
- Examples: `include/enjin2/core/component.hpp`
- Pattern: Decorator / Strategy.

**Signal:**
- Purpose: Thread-safe (where applicable) and decoupled event distribution.
- Examples: `include/enjin2/core/signal.hpp`
- Pattern: Observer.

## Entry Points

**Scene Initialization:**
- Location: `include/enjin2/core/scene.hpp`
- Triggers: `SceneStateMachine::changeScene()`
- Responsibilities: `onCreate()`, `onActivate()`, and initializing the object collection.

**Engine Main Loop (Implementation Specific):**
- Location: Found in examples like `examples/comprehensive_demo.cpp` or `src/bindings/emscripten_bindings.cpp`.
- Triggers: Hardware timer or browser `requestAnimationFrame`.
- Responsibilities: Orchestrating `update()` and `render()` calls.

## Error Handling

**Strategy:** Exception-lite, mostly using return codes, handles, and internal assertions.

**Patterns:**
- **Handle Validation:** `EntityManager::isValid(Entity)` checks generation counters.
- **Null Safety:** Extensive use of `nullptr` checks in component retrieval and object management.

## Cross-Cutting Concerns

**Logging:** Standard output and platform-specific debug hooks (e.g., `ESP_LOG` on ESP32).
**Validation:** `dynamic_cast` for component type safety (runtime) and `static_assert` for template constraints.
**Authentication:** Not applicable (Client-side engine).

---

*Architecture analysis: 2025-02-14*
