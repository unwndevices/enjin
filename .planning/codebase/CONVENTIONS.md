# Coding Conventions

**Analysis Date:** 2026-02-23

## Naming Patterns

**Files:**
- Header files use `.hpp` extension: `canvas.hpp`, `object.hpp`, `signal.hpp`
- C source uses `.h` extension only for external C libraries: `gfxfont.h`
- Implementation files use `.cpp` with the same base name as the header
- File names are `snake_case`: `image_cache.hpp`, `drawing_helpers.hpp`, `lua_engine.hpp`
- `@file` Doxygen tags at the top of complex headers to name the file explicitly

**Classes and Structs:**
- Plain data structs use `PascalCase`: `Point`, `Size`, `Rect`, `Pixel4`, `PackedPixel4`
- Component classes are prefixed with `C_`: `C_Drawable`, `C_Position`, `C_Rectangle`, `C_Satellite`, `C_Planet`
- Abstract interface classes are prefixed with `I`: `ICanvas`
- Template classes use `PascalCase`: `Canvas4`, `Canvas8`, `Signal`, `ComponentStorage`, `StaticPool`
- RAII wrapper classes use `PascalCase`: `SignalConnection`

**Enumerations:**
- `enum class` is used exclusively (scoped enums): `DrawLayer`, `BlendMode`, `Anchor`
- Enum values are `UPPER_SNAKE_CASE`: `DrawLayer::Background`, `Anchor::TOP_LEFT`, `BlendMode::Opacity50`

**Functions and Methods:**
- Public API methods follow two distinct styles depending on context:
  - New ECS/core API uses `camelCase`: `getWidth()`, `setPosition()`, `isEnabled()`, `addComponent()`, `getComponent()`
  - Legacy Enjin1-compatibility methods use `PascalCase`: `SetSortOrder()`, `GetBlendMode()`, `SetDrawLayer()`, `SetVisibility()`
- Boolean query functions follow `isX()` or `hasX()` pattern: `isEnabled()`, `isActive()`, `hasComponent()`, `isVisible()`, `hasConnections()`
- Lifecycle hooks use `onX()` pattern for virtual overrideable events: `onCreate()`, `onActivate()`, `onUpdate()`, `onRender()`
- Object/component lifecycle methods use `camelCase` verbs: `awake()`, `start()`, `update()`, `lateUpdate()`

**Member Variables:**
- Private/protected members use `snake_case` without prefix: `position`, `enabled`, `sort_order`, `blend_mode`, `orbit_radius`
- Boolean state fields are named to read naturally: `enabled`, `active`, `initialized`, `awoken`, `started`, `clockwise`
- Static constexpr members use `UPPER_SNAKE_CASE`: `MAX_COMPONENTS`, `MAX_CONNECTIONS`, `TRAIL_LENGTH`
- Doxygen inline documentation uses `///< comment` style for member variables

**Template Parameters:**
- Pixel type templates named `TPixel` or `PixelType`: `template<typename TPixel>`, `template<uint16_t WIDTH, uint16_t HEIGHT>`
- Generic type parameters use single capital letters or descriptive names: `T`, `T1`, `T2`, `Args`

**Type Aliases:**
- Canvas type aliases are explicit dimension names: `Canvas4_128x128`, `Canvas8_128x64`
- Signal aliases describe their event: `Signals::Clicked`, `Signals::ValueChanged`, `Signals::PositionChanged`

**Namespaces:**
- All project code lives in `namespace enjin2 { ... } // namespace enjin2`
- Sub-namespaces group related constants: `enjin2::Colors`, `enjin2::Signals`, `enjin2::math`

## Code Style

**Formatting:**
- No `.clang-format` or `.editorconfig` detected; formatting is manually maintained
- 4-space indentation throughout
- Opening braces on same line as declaration (`{` not on new line)
- Exception: Constructor member initializer lists split across lines with `: member(val)` on next line
- Closing namespace braces annotated: `} // namespace enjin2`
- Single blank line between method definitions in headers
- Multi-line parameter lists align parameters under opening parenthesis

**Linting:**
- No `.clang-tidy` or static analysis configuration detected
- C++17 standard required (`set(CMAKE_CXX_STANDARD 17)`)

## Include Organization

**Headers use `#pragma once` exclusively** - no `#ifndef` guards.

**Order in implementation files (`.cpp`):**
1. The file's own header via relative path: `#include "../../include/enjin2/core/math.hpp"`
2. Other project headers
3. Standard library headers

**Order in headers (`.hpp`):**
1. `#pragma once`
2. Other project headers (relative with `../` prefix): `#include "../core/component.hpp"`
3. Standard library headers: `#include <cstdint>`, `#include <array>`, `#include <functional>`
4. Third-party headers (e.g., `#include "gfxfont.h"`)

**Path Aliases:** None. All paths are relative. Implementation files use `../../include/enjin2/...` to reach public headers. Tests use `<enjin2/...>` angle bracket style (via `target_include_directories`).

**Platform Guards:**
```cpp
#ifndef VCV_RACK
#include <Arduino.h>
#endif
```

## Error Handling

**Strategy:** Return-value based with null pointer checks. No exceptions are used (embedded/bare-metal target compatibility).

**Patterns:**
- Functions that can fail return `nullptr` or `false`: `addComponent()` returns `nullptr` if `componentCount >= MAX_COMPONENTS`
- Guard clauses at function top check preconditions early and return: `if (!active) return;`
- Bounds checked before array access: `if (index >= drawableCount) return nullptr;`
- Null pointer checks before use: `if (!data1 || !data2) { fprintf(stderr, ...); return -1.0f; }`
- Template constraints use `static_assert`: `static_assert(std::is_base_of<Component, T>::value, "T must derive from Component")`
- Signal connections return -1 on failure: `return -1; // No space available`
- Canvas operations silently ignore out-of-bounds pixels via `inBounds()` check before write

## Logging

**Framework:** Standard C I/O (`fprintf`, `printf`) for test executables; `std::cout` for test harness output. No logging framework in the library itself.

**Patterns:**
- Errors go to `stderr` via `fprintf(stderr, ...)`: used in test utilities only
- Progress/status output uses `std::cout` with `std::endl`
- Library code does not log; it signals errors through return values

## Comments

**When to Comment:**
- Every public class, function, and member variable receives a Doxygen comment
- Implementation-side comments explain non-obvious logic (algorithms, magic numbers)
- `// Note:` inline comments explain platform-specific or compatibility decisions
- Section separators like `// ========================================` used for grouping in large files

**Doxygen Style:**
Every public symbol is documented with `/** @brief ... */` block comments:
```cpp
/**
 * @brief Add a component to this object
 * @tparam T Component type (must derive from Component)
 * @param args Constructor arguments
 * @return Pointer to the created component or nullptr if failed
 */
template<typename T, typename... Args>
T* addComponent(Args&&... args);
```
Single-line members use `/// @brief ...` or `///< inline` style:
```cpp
bool enabled;       ///< Whether the component is enabled
/// @brief Set the sort order for drawing priority
/// @param order Sort order value
void SetSortOrder(int order) { sort_order = order; }
```
File-level documentation uses `@file` tag in the header:
```cpp
/**
 * @file drawable.hpp
 * @brief Base drawable component for rendering
 */
```

## Function Design

**Size:** Methods kept focused; complex logic split across protected virtual hooks (`onCreate`, `onUpdate`, `onRender`).

**Parameters:**
- Primitive types passed by value: `void update(uint16_t deltaTime)`
- Objects/structs passed by const reference: `void fill(const Rect& rect, TPixel color)`
- Output via return value, not output parameters, except Adafruit_GFX compatibility methods

**Return Values:**
- Pointer return `T*` where nullptr means "not found/failed"
- Boolean return for success/failure: `bool removeComponent()`
- Value return for accessors: `uint16_t getWidth() const`

**Inline Methods:** Simple getters/setters defined inline in headers. Complex logic goes in `.cpp`.

## Module Design

**Header-Only Template Classes:** All template classes (`Canvas4`, `Canvas8`, `Signal`, `ComponentStorage`, `Object::addComponent`) are fully defined in headers.

**Exports:**
- No barrel include files - include specific headers
- Exception: aggregate headers like `include/enjin2/ui/components.hpp` and `include/enjin2/ui/systems.hpp` bundle related types

**`#pragma once`:** Used universally instead of include guards.

**Forward Declarations:**
Used to break circular dependencies between headers:
```cpp
// Forward declaration
class Object;
class C_Position;
class C_Drawable;
```

## C++17 Feature Usage

- `if constexpr` for compile-time branching on pixel types: `if constexpr (std::is_same_v<PixelType, uint8_t>)`
- `std::is_same_v<>` type traits
- Structured bindings (`std::pair`) in iterator patterns
- `static_assert` with SFINAE for template constraints
- `constexpr` used extensively for constants and simple constructors: `constexpr Pixel4(uint8_t v) : value(v & 0x0F) {}`

---

*Convention analysis: 2026-02-23*
