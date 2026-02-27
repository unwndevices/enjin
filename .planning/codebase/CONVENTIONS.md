# Coding Conventions

**Analysis Date:** 2026-02-27

## Naming Patterns

**Classes/Types:**
- Component classes use prefix `C_` followed by PascalCase: `C_Position`, `C_Sprite`, `C_LuaScript`, `C_Drawable`, `C_Animation`
- Other classes use PascalCase: `Object`, `Scene`, `Canvas`, `Palette`
- Enums use PascalCase: `Anchor`, `AnimMode`, `ScriptErrorPolicy`

**Members (Private/Protected):**
- Use `m_` prefix (enforced by clang-tidy configuration)
- Follow with camelCase: `m_position`, `m_enabled`, `m_componentCount`
- Example from `Component`:
  ```cpp
  private:
      Object* owner;      // enjin2 omits m_ prefix (legacy decision)
      bool enabled;       // camelCase without m_ prefix
  ```
- New code should follow clang-tidy rule: `m_camelCase`

**Functions:**
- Use camelCase: `getPosition()`, `setPosition()`, `isEnabled()`, `addComponent()`, `lateUpdate()`
- Virtual lifecycle methods: `awake()`, `start()`, `update()`, `lateUpdate()`, `onEnable()`, `onDisable()`

**Variables (Local/Parameters):**
- camelCase: `componentCount`, `renderPos`, `newX`, `targetPosition`
- Template parameters use ALL_CAPS: `T`, `Args`, `PixelType`

**Constants:**
- Static constexpr globals in namespace use UPPER_SNAKE_CASE: `PI`, `TWO_PI`, `MAX_COMPONENTS`, `MAX_TAGS`
- Magic numbers should be constexpr'd to named constants
- Example from `object.hpp`:
  ```cpp
  static constexpr size_t MAX_COMPONENTS = 16;
  static constexpr size_t MAX_TAGS = 8;
  ```

**Files:**
- Headers: snake_case with .hpp extension: `position.hpp`, `lua_script.hpp`, `drawable.hpp`
- Source: snake_case with .cpp extension: `object.cpp`, `bindings.cpp`
- Test files: descriptive_name_test.cpp: `error_policy_test.cpp`, `gc_assert_test.cpp`, `sprite_test.cpp`

## Code Style

**Formatting:**
- No auto-formatter (Clang Format) present; follows clang-tidy CheckOptions
- 4-space indentation (observed in all files)
- Brace style: Allman (opening brace on new line for block statements)

**Linting:**
- Tool: clang-tidy (`.clang-tidy` configuration file at project root)
- Key checks enabled:
  - `bugprone-*`: Bug detection patterns
  - `cppcoreguidelines-*`: C++ Core Guidelines compliance
  - `modernize-*`: C++17 features and idioms
  - `performance-*`: Performance anti-patterns
  - `readability-*`: Code clarity checks
  - `clang-analyzer-*`: Static analysis

- Key checks disabled:
  - `-cppcoreguidelines-avoid-magic-numbers`: Magic numbers allowed
  - `-modernize-use-trailing-return-type`: Trailing return types not enforced
  - `-cppcoreguidelines-avoid-c-arrays`: C arrays permitted (memory-constrained environments)

**Usage:**
```bash
clang-tidy -p build src/core/object.cpp                # Check single file
clang-tidy -p build --fix src/core/object.cpp         # Apply fixes
```

## Import Organization

**Order:**
1. Local project headers (relative paths): `#include "../../include/enjin2/..."`
2. STL/C++ standard library: `#include <array>`, `#include <memory>`, `#include <cmath>`
3. Third-party: `#include <lua.h>` (LuaJIT), `#include <SDL.h>` (SDL3, optional)

**Example from `object.hpp`:**
```cpp
#pragma once
#include "types.hpp"          // Local
#include <array>              // STL
#include <memory>             // STL
#include <functional>         // STL
#include <type_traits>        // STL
```

**Path Aliases:**
- Relative includes from src/ to include/: `#include "../../include/enjin2/..."`
- No CMake build-time path aliases configured; direct relative paths used

## Error Handling

**Strategy:** No exceptions in C++ code (embedded/ESP32 compatibility). Uses return codes and state flags.

**Patterns:**

**Component Dependencies (assertRequires<T>):**
- Components declare required sibling components via `assertRequires<T>()` in `awake()`
- Debug builds: calls `assert(false)` if dependency missing (developer catches immediately)
- Release builds: logs once via `printf()` and disables component safely (no abort on ESP32)
- Example from `component.hpp`:
  ```cpp
  template<typename T>
  void assertRequires() {
      static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
      if (owner->getComponent<T>() == nullptr) {
#ifndef NDEBUG
          assert(false && "assertRequires<T> failed: required component not present on owner Object");
#else
          printf("[enjin2] assertRequires<T> failed: required component not present — disabling component\n");
          setEnabled(false);
#endif
      }
  }
  ```

**Script Error Handling (ScriptErrorPolicy):**
- Lua scripts use policy-based error handling (3 modes):
  - `ScriptErrorPolicy::Disable` (default): Sets `scriptError=true`, disables script on error
  - `ScriptErrorPolicy::Log`: Logs error every frame, script continues running
  - `ScriptErrorPolicy::Panic`: Calls platform panic handler (abort/esp_restart)
- Located in `include/enjin2/components/lua_script.hpp`
- Checked via `bool hasErrors()` and error message via `getErrorMessage()`

**Return Codes:**
- Template functions return `nullptr` on failure: `addComponent<T>()`, `getComponent<T>()`
- Boolean returns for success/failure: `loadScript()`, `removeComponent<T>()`
- Example from `object.hpp`:
  ```cpp
  template<typename T, typename... Args>
  T* addComponent(Args&&... args) {
      if (componentCount >= MAX_COMPONENTS) return nullptr;  // Failure: nullptr
      // ... create and return component pointer
      return componentPtr;
  }
  ```

## Logging

**Framework:** printf (C stdio)

**Patterns:**
- Error messages: `printf("[enjin2] <message>\n")`
- Logging location: test output via fprintf(stderr, ...) and printf(stdout, ...)
- No structured logging library used (embedded compatibility)
- Example from `error_policy_test.cpp`:
  ```cpp
  #define ASSERT(cond, msg) \
      do { \
          if (!(cond)) { \
              fprintf(stderr, "FAIL: %s\n", msg); \
              failures++; \
          } else { \
              passes++; \
          } \
      } while(0)
  ```

## Comments

**When to Comment:**
- Complex algorithms or non-obvious logic
- Public API contracts (parameters, return values)
- Performance-critical sections
- Workarounds for known issues

**JSDoc/Doxygen:**
- Use `/**` block comments for public headers (consumed by Doxygen)
- Format: `@brief`, `@param`, `@return`, `@file`, `@tparam` (template parameters)
- All public classes and methods documented
- Example from `component.hpp`:
  ```cpp
  /**
   * @brief Assert that a sibling component of type T exists on the same Object.
   *
   * In debug builds (NDEBUG not defined): calls assert(false) with a message naming the
   * requirement. The call stack identifies the failing component. No abort on missing
   * dep when the dep IS present (no-op).
   *
   * In release builds (NDEBUG defined): logs once via printf and disables this component.
   * No process abort — safe for ESP32.
   *
   * Call from awake() to declare component dependencies loudly.
   *
   * @tparam T Required component type (must derive from Component)
   */
  ```

## Function Design

**Size:** Keep functions under 50 lines; complex logic extracted to helpers

**Parameters:**
- Const references for non-owning input: `const Point& pos`
- Pointers for optional/mutable: `Object* owner`
- Forward parameters in templates: `Args&&... args` with `std::forward<Args>(args...)`

**Return Values:**
- Prefer returning values over out-parameters (C++17 RVO)
- Use nullptr sentinel for "not found" in pointer returns
- Use bool for simple success/failure
- Example from `position.hpp`:
  ```cpp
  Point calculateRenderPosition(const Size& size) const {  // Returns calculated value
      Point renderPos = position + anchorOffset;
      // ... transform based on anchor ...
      return renderPos;
  }

  float distanceTo(const Point& other) const {  // Return distance value
      int16_t dx = other.x - position.x;
      int16_t dy = other.y - position.y;
      return std::sqrt(static_cast<float>(dx * dx + dy * dy));
  }
  ```

## Module Design

**Exports:**
- Public headers in `include/enjin2/` (organized by subsystem)
- All public APIs use namespace `enjin2`
- Private implementation headers in source directories (not exposed)

**Barrel Files:**
- Not used (direct includes of needed headers)

## C++17 Features Used

**Advanced Type Traits:**
- `std::is_base_of<Base, T>()`: Check component/object inheritance
- `std::is_same<T1, T2>()` / `std::is_same_v<T1, T2>`: SFINAE specialization
- Example from `object.hpp`:
  ```cpp
  template<typename T>
  typename std::enable_if<std::is_same<T, C_Position>::value>::type
  cachePositionIfType(T* componentPtr) {
      position = componentPtr;
  }
  ```

**constexpr if:**
- Used in template definitions for compile-time branches
- Example from `scene.hpp`:
  ```cpp
  if constexpr (std::is_same_v<PixelType, uint8_t>) {
      // 8-bit specific code
  }
  ```

**std::array:**
- Static allocation for fixed-size collections: `std::array<Component*, MAX>`
- Used instead of dynamic vectors for embedded platforms
- Accessed via `.data()`, `.fill()`, `.size()`

**std::unique_ptr:**
- Smart pointer ownership for components and objects
- Movement semantics: `std::move()` in container operations
- Example from `object.hpp`:
  ```cpp
  std::unique_ptr<T> component(new T(this, std::forward<Args>(args)...));
  components[componentCount++] = std::move(component);
  ```

**std::forward & Perfect Forwarding:**
- Template constructors accept variadic arguments: `addComponent<T>(Args&&... args)`
- Forward to component constructor: `new T(this, std::forward<Args>(args)...)`

**static constexpr:**
- Compile-time constants: `static constexpr size_t MAX_COMPONENTS = 16;`
- Constexpr functions for math: `constexpr T clamp(T value, T min_val, T max_val)`
- Example from `math.hpp`:
  ```cpp
  namespace math {
      constexpr float PI = 3.14159265358979323846f;

      template<typename T>
      constexpr T clamp(T value, T min_val, T max_val) {
          return (value < min_val) ? min_val : (value > max_val) ? max_val : value;
      }
  }
  ```

**Deleted/Defaulted Special Members:**
- `virtual ~Component() = default;` for virtual destructors
- No copy constructors (all move-only for unique_ptr semantics)

---

*Convention analysis: 2026-02-27*
