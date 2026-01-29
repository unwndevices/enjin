# Coding Conventions

**Analysis Date:** 2026-01-29

## Naming Patterns

**Files:**
- Headers: `.hpp` for C++ headers (enjin2), `.hpp` for enjin
- Implementation: `.cpp` for both versions
- PascalCase for class files: `Object.hpp`, `Scene.hpp`, `Animation.hpp`
- Lowercase for utility files: `utils.hpp`, `math.hpp`
- Component prefix: `C_` prefix for component files: `C_Animation.hpp`, `C_Position.hpp`

**Functions:**
- PascalCase for public methods: `GetComponent()`, `AddComponent()`, `Update()`, `Draw()`
- PascalCase for protected methods: `Awake()`, `Start()`, `LateUpdate()`
- camelCase for private helpers: `cachePositionIfType()`, `initializeComponentCache()`
- Prefix patterns: `Set...` for setters, `Get...` for getters, `is...` for boolean queries
- Virtual overrides: Use exact same name as base class method

**Variables:**
- camelCase for member variables: `position`, `anchorOffset`, `componentCount`, `queued_for_removal`
- snake_case for function parameters: `deltaTime`, `isActive`, `x`, `y`, `radius`
- Constants: UPPER_CASE with underscores: `MAX_COMPONENTS`, `VCV_RACK`, `BUFFER_SIZE`
- Pointer variables: snake_case with descriptive names: `componentPtr`, `owner`

**Types:**
- PascalCase for class names: `Object`, `Component`, `Scene`, `Animation`
- PascalCase for struct names: `Point`, `Size`, `Rect`, `Pixel4`, `FrameData`
- PascalCase for enum classes: `Anchor`, `BlendMode`, `DrawLayer`, `FacingDirection`
- PascalCase for template parameters: `TPixel`, `T`, `Args`

## Code Style

**Formatting:**
- No explicit formatter configuration detected (no .clang-format, .prettierrc files)
- Indentation: 4 spaces (consistent across all files)
- Brackets: K&R style (opening bracket on same line as statement)
- Line length: No strict limit, but generally under 120 characters
- Spacing: Space after commas, around operators, after keywords (if, for, while)

**Linting:**
- No explicit linter configuration detected
- Code maintains consistency through manual discipline

**File headers:**
- enjin2 uses `#pragma once` for include guards
- enjin uses traditional `#ifndef HEADER_H` / `#define HEADER_H` / `#endif` guards
- Doxygen-style comments for public APIs in enjin2

**Namespaces:**
- `namespace enjin` for original version
- `namespace enjin2` for refactored version
- Closing namespace comment: `} // namespace enjin2`

## Import Organization

**Order:**
1. System headers: `#include <cstdint>`, `#include <algorithm>`
2. Third-party headers: `#include <Adafruit_GFX.h>`, `#include <Arduino.h>`
3. Local headers with relative paths: `#include "../core/types.hpp"`
4. Forward declarations before includes when possible

**Path conventions:**
- Use relative paths: `#include "../core/component.hpp"`
- No include path aliases detected
- Organized by module: `#include "components/position.hpp"`

**Forward declarations:**
- Preferred over includes for header-only dependencies
- Format: `class Object;`, `struct Point;`
- Used in template-heavy code to reduce compile-time dependencies

## Error Handling

**Patterns:**
- Return `nullptr` for component allocation failures: `return nullptr;` when `MAX_COMPONENTS` exceeded
- Return empty vectors/results for non-critical failures
- Use static assertions for compile-time type checking: `static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");`
- Exceptions used sparingly, only for critical errors in enjin (e.g., `C_ImageCache` throws `ImageCacheException`)
- enjin2 avoids exceptions (embedded-friendly design)

**Null pointer checks:**
- Common pattern: `if (curScene) { curScene->ProcessInput(); }`
- Safe access: Use conditional checks before dereferencing

**Bounds checking:**
- Canvas operations: `if (!inBounds(x, y)) return;`
- Array access: Checked before access in most graphics operations

**Commented error handling:**
- Some fallback implementations commented out: `// Or throw an error` (enjin/utils/Bitmask.cpp)

## Logging

**Framework:** None (platform-dependent)

**Patterns:**
- `printf()` for general logging in examples and tests
- `std::cout` for C++-style output (enjin2 examples)
- `Serial.println()` for Arduino/embedded platforms (enjin)
- Commented-out debug logging: `// log_d("Frame index %u is out of range...", frame, frames.size());`

**When to log:**
- Test verification: `printf("✓ C_Draw component working\n");`
- Error conditions: `printf("❌ ImageCache error: %s\n", e.what());`
- Performance metrics: `printf("✓ Current frame rate: %.2f FPS\n", fps);`
- Integration test results: Checkmark (✓) and cross (✗) symbols for pass/fail

**Logging levels:**
- No formal logging levels
- Use printf to indicate test success/failure visually
- Performance timing measured with `std::chrono`

## Comments

**When to Comment:**
- Complex algorithms: Bresenham's line drawing, midpoint circle algorithm
- Memory optimization: `// PERFORMANCE OPTIMIZATIONS` sections
- Platform-specific code: `// ESP32 platform` conditional sections
- Temporary workarounds: Marked with `// TODO` comments
- Behavior that may not be obvious: `// Called when scene destroyed`

**Doxygen/TSDoc:**
- enjin2 uses Doxygen-style comments for public APIs
- Format: `/** @brief Description */`
- Multi-line: `/**\n * @brief Description\n * @param param Description\n */`
- Document parameters: `@param owner The object that owns this component`
- Document return values: `@return Pointer to component or nullptr if not found`
- Not used extensively in enjin (original version)

**Inline comments:**
- Explain non-obvious logic: `// Cache position component using template specialization helper`
- Optimization notes: `// Batch set using memset for uniform color`
- Section markers: `// === Adafruit_GFX Compatibility Methods ===`

**TODO/FIXME:**
- Format: `// TODO: A bit cumbersome. Is there another way to do this?`
- Location: enjin/Animation.cpp, enjin/Components/C_Probe.cpp
- Used for known limitations or planned improvements

## Function Design

**Size:**
- Prefer small, focused functions (typically under 30 lines)
- Complex operations split into helper methods
- Template functions may be longer due to type constraints

**Parameters:**
- Use `const T&` for non-primitive parameters passed by reference: `const Point& pos`
- Use `T&&` for perfect forwarding: `Args&&... args`
- Default parameters for common cases: `uint8_t precision = 2`
- Pointer parameters for optional outputs: `uint8_t* text`

**Return Values:**
- Return by value for small types: `Point getPosition() const`
- Return by reference for accessors: `const Point& getPosition() const { return position; }`
- Return pointer for optional/nullable: `T* getComponent()` returns `nullptr` if not found
- Return bool for success/failure: `bool removeComponent()`

**Virtual functions:**
- Use `virtual` keyword for base class methods
- Use `override` keyword in derived classes
- Default implementations in base classes often empty: `virtual void Update(uint16_t deltaTime) {}`

**Const correctness:**
- Mark getters as `const`: `int getComponentCount() const`
- Use `const` references to avoid copies: `const Point& getPosition() const`

## Module Design

**Exports:**
- Each module (.hpp/.cpp pair) exports one primary class
- Header-only templates: Defined entirely in `.hpp` files
- Explicit namespace declarations: `namespace enjin2 { class ... };`

**Barrel files:**
- `enjin2_include.hpp` not detected (would be useful for common includes)
- Components organized by directory, but no central header
- Examples import headers individually as needed

**Directory structure:**
- `include/enjin2/core/` - Core classes (Object, Component, Scene)
- `include/enjin2/graphics/` - Graphics system (Canvas)
- `include/enjin2/components/` - Component classes
- `include/enjin2/utils/` - Utility functions and types
- `src/` mirrors `include/` structure for implementations

**Header/implementation separation:**
- Headers: `include/enjin2/` directory
- Implementations: `src/` directory
- Template implementations often in headers (inline or defined in class body)

**Component pattern:**
- All components inherit from `Component` base class
- Components attached to `Object` instances
- Named with `C_` prefix: `C_Position`, `C_Drawable`, `C_Label`
- Template methods for component management: `addComponent<T>()`, `getComponent<T>()`

---

*Convention analysis: 2026-01-29*
