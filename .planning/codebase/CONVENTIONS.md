# Coding Conventions

**Analysis Date:** 2026-03-01

## Naming Patterns

**Files:**
- C++ headers: `.hpp` (e.g., `include/enjin2/core/object.hpp`, `include/enjin2/scripting/bindings.hpp`)
- C++ implementation: `.cpp` (e.g., `src/core/object.cpp`, `src/scripting/bindings.cpp`)
- Component classes: `C_<ComponentName>` (e.g., `C_Position`, `C_LuaScript`, `C_Timer`, `C_StateMachine`)
- Test files: `*_test.cpp` (e.g., `physics_lua_test.cpp`, `engine_table_test.cpp`)
- Lua proxy types: `<ComponentName>Proxy` (e.g., `ScriptProxy`, `ComponentProxy`)

**Classes/Structs:**
- PascalCase: `Object`, `Component`, `Scene`, `LuaEngine`, `LuaBindings`
- Struct naming: Follows class convention (e.g., `EngineTimeState`, `ScriptProxy`)
- Component base class: Always named `C_<Name>` for consistency

**Functions and Methods:**
- camelCase: `getPosition()`, `setPosition()`, `executeString()`, `registerAll()`
- Lua binding implementations: `lua_<proxy_type>_<method>_impl()` (e.g., `lua_proxy_index_impl()`, `lua_proxy_newindex_impl()`)
- Private/protected methods: Follow camelCase, no special prefix
- Virtual lifecycle methods: `awake()`, `start()`, `update()`, `lateUpdate()`, `onRender()`

**Variables:**
- Local/parameter: camelCase: `componentCount`, `currentScene`, `offsetX`
- Member variables: Prefix with `m_`, camelCase: `m_luaProxy`, `m_canvasPtr`, `m_currentBindings`
- Static members: PascalCase: `MAX_COMPONENTS`, `memoryUsed` (if not `m_` prefixed)
- Pointer names: Include `Ptr` or no special suffix; type clarity from context (e.g., `component`, `owner`, `canvasPtr`)

**Types:**
- Enums: PascalCase for enum names, UPPER_CASE for values: `enum class Anchor { TOP_LEFT, CENTER, BOTTOM_RIGHT }`
- Namespaces: lowercase: `enjin2`
- Template parameters: PascalCase: `T`, `W`, `H` (where T is type, W/H are template dimensions)

**Constants:**
- Compile-time constants: UPPER_CASE_WITH_UNDERSCORES: `MAX_COMPONENTS`, `MEMORY_LIMIT`
- Metatable names (C strings): PascalCase: `"ScriptProxy"`, `"Pixel4"`, `"Pixel8"`
- Static local strings: PascalCase: `constexpr const char* PROXY_METATABLE = "ScriptProxy"`

## Code Style

**Formatting:**
- Line length: No strict limit enforced, readability-focused
- Indentation: 4 spaces
- Brace style: Allman (opening brace on new line for functions, classes; same-line for control structures)
  ```cpp
  // Class/function
  class Object {
  public:
      void update(float dt) {
          if (enabled) {
              // statements
          }
      }
  };
  ```
- Spacing: Around operators, after control keywords, no space before `(`

**Linting:**
- Tool: `clang-tidy`
- Config: `.clang-tidy` enforces:
  - bugprone-* checks enabled
  - cppcoreguidelines-* with exceptions for varargs, magic numbers, C arrays
  - modernize-* checks (trailing return types disabled)
  - performance-* checks
  - readability-* checks (magic numbers and identifier length disabled)
- Run: `clang-tidy -p build src/core/object.cpp` or with `--fix` for auto-corrections

**Documentation:**
- Doxygen comments for public APIs
- Format: `/// @brief`, `@param`, `@return`, `@tparam`
- File headers: Include file path, brief description, and relevant phase/feature tags
  ```cpp
  /**
   * @file object.hpp
   * @brief Object base class for game entities
   * Manages components using static allocation.
   */
  ```
- Inline comments: Single-line `//` for clarification; multi-line `/* */` for block explanations

## Import Organization

**Order:**
1. Project headers (same directory or parent): `#include "types.hpp"`, `#include "../core/object.hpp"`
2. Project system headers (from include/enjin2): `#include <enjin2/scripting/bindings.hpp>`
3. Standard C++ library: `#include <vector>`, `#include <memory>`, `#include <string>`
4. C standard library: `#include <cstdio>`, `#include <cassert>`
5. Platform-specific (if needed): Conditional includes

**Path Aliases:**
- No explicit aliases in CMakeLists observed; relative paths used with backtracking (`../../include`)
- Public headers always under `include/enjin2/` hierarchy
- Implementation in `src/` mirrors header structure

**Namespace:**
- All code within `namespace enjin2 { ... }`
- Forward declarations at namespace level for circular dependency prevention
- No `using namespace enjin2` in headers; local `using` acceptable in .cpp files

## Error Handling

**Patterns:**
- No exceptions: C++17 code avoids exceptions (embedded/ESP32 compatibility)
- Return codes: `bool`, `LuaResult` struct with `success` flag and error message
- Lua errors: Use `luaL_error()` for Lua-side exceptions; aborts Lua execution (does NOT affect C++)
- Assertions: `assert()` in debug builds for preconditions; `printf()` fallback in release (ESP32 safety)
  ```cpp
  #ifndef NDEBUG
      assert(false && "message");
  #else
      printf("[enjin2] error message\n");
      setEnabled(false);  // Graceful degrade
  #endif
  ```
- Null-guard pattern: Check pointers before dereference, return default/false on nullptr
  ```cpp
  if (!proxy->valid || !proxy->component) {
      luaL_error(L, "object has been destroyed");
      return 0;
  }
  ```

**Error Messages:**
- Descriptive, single-sentence format
- Include context: component type, operation, expected value if applicable
- Lua errors via `luaL_error(L, "message")` — do NOT return error strings to Lua normally

## Logging

**Framework:** `printf()` / `std::cout`

**Patterns:**
- Test output: Use `printf()` for test framework compatibility
- Engine logging: `engine.log()` Lua function (variadic, type-agnostic)
- Debug output: `printf()` for temporary diagnostics; removed in production
- Performance: Logging in hot paths should be conditional or disabled

**When to Log:**
- Initialization/shutdown events
- Lua script loading/compilation
- Error conditions (not all errors — only unexpected ones)
- Test framework assertions (PASS/FAIL output)

## Comments

**When to Comment:**
- WHY, not WHAT: Comment the reason for unusual code, not what it does
- Complex algorithms: Explain approach and any non-obvious invariants
- TODO/FIXME: Mark with `// TODO: description` if legitimate; `// FIXME: description` for bugs
- Phase/requirement mapping: Reference phase numbers or requirement IDs for Lua bindings
  ```cpp
  // Phase 31 ENG-01: engine.scene.switch() reaches SceneStateMachine::switchTo()
  ```

**JSDoc/Doxygen:**
- Public APIs in headers: Always document
- Private methods: Optional but recommended for complex logic
- Parameters: Use `@param` for all non-obvious parameters
- Return values: Use `@return` for non-void returns
- Exceptions/errors: Document expected failure modes

## Function Design

**Size:**
- Prefer < 50 lines (loose guideline)
- Decompose large functions; static helpers acceptable for Lua binding implementations

**Parameters:**
- Const references for non-POD types: `const Object&`, `const std::string&`
- POD types by value: `float dt`, `int count`, `bool enabled`
- Pointers for optional/nullable: `Component* comp`, `nullptr` when not provided
- No out-parameters; use return values or simple structs (e.g., `LuaResult`)

**Return Values:**
- Single return type per function
- Use `std::pair` or small struct for multiple values (e.g., `Vec2` for x/y)
- `bool` for success/failure; `LuaResult` for detailed error messages
- `std::optional<T>` not used (C++17 support, but avoided for Embedded)

**Overloading:**
- Permitted: same name, different parameter types/counts
- Examples: `setPosition(int16_t x, int16_t y)` and `setPosition(const Point& pos)`
- Lua bindings: Overload via argument count checking in single `lua_*_impl()` function

## Module Design

**Exports:**
- Public headers in `include/enjin2/<category>/`
- Each header includes necessary types but minimizes dependencies
- Implementation details in .cpp files; never exposed in headers

**Barrel Files:**
- `include/enjin2/core/` contains individual headers: `object.hpp`, `component.hpp`, `scene.hpp`
- No single `core.hpp` file that re-exports all headers
- Examples include full headers explicitly

**Header Guards:**
- `#pragma once` (modern, non-standard but widely supported)
- Fallback to ifdef guards only if pragma once fails (not observed in this codebase)

**Circular Dependencies:**
- Forward declarations at top of .hpp to break circles
- Full includes in .cpp only
- Example: `class SceneStateMachine;` forward declaration in bindings.hpp, full include in bindings.cpp

## Lua Binding Conventions

**Proxy Objects:**
- Each Lua-visible C++ object wrapped in a `*Proxy` userdata struct
- Proxy holds non-owning pointer and validity flag: `bool valid` (invalidated on C++ destruction)
- Metamethods: `__index` (read), `__newindex` (write), `__tostring` (debug output)
- Null-guard all proxy access; crash with `luaL_error()` if stale

**Binding Function Naming:**
- Static function per metamethod: `lua_proxy_index_impl()`, `lua_proxy_newindex_impl()`
- Component-specific: `lua_cposition_proxy_index_impl()`, `lua_ctimer_proxy_index_impl()`
- Global Lua functions: `lua_<noun>_<verb>()` (e.g., `lua_timer_after()`, `lua_fsm_addState()`)

**Registration Pattern:**
- `LuaBindings::registerAll()` called once at engine init
- Each module (time, scene, input, collision, etc.) registers its functions and metatables
- Metatables registered via `luaL_newmetatable(L, "MetatableName")`; functions via `lua_setfield()`

**Stack Handling:**
- All lua_*_impl functions manipulate Lua stack directly
- Return value: number of values pushed to stack (typically 1, sometimes 0-2)
- Stack layout documented in comments: `// Stack: [1]=userdata, [2]=key_string`
- Always balance stack before returning

**Error Handling in Bindings:**
- Use `luaL_checkinteger()`, `luaL_checkstring()` for argument validation
- Use `lua_tostring()`, `lua_tonumber()` for optional arguments
- `luaL_error()` for validation failures; aborts Lua execution

**Testing Lua Bindings:**
- Use `LuaEngine` fixture with `executeString()` to load Lua code
- Assert results via `engine.getGlobalNumber()`, `engine.getGlobalString()`
- Pattern: execute Lua, set global variable, read from C++

---

*Convention analysis: 2026-03-01*
