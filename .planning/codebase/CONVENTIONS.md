# Coding Conventions

**Analysis Date:** 2026-02-28

## Language & Standard

**Primary Language:** C++ (C++17)
- `-std=c++17` enforced in CMakeLists.txt
- Compiled as LANGUAGES CXX C in `CMakeLists.txt`
- No exceptions, no RTTI, no dynamic allocation in hot paths

**File Extensions:**
- `.hpp` for headers
- `.cpp` for implementations
- `.lua` for Lua scripts (in `scripts/` directory)

## Naming Patterns

**Classes:**
- PascalCase (e.g., `Object`, `C_Position`, `LuaBindings`, `Canvas8`)
- Component classes prefixed with `C_` (e.g., `C_LuaScript`, `C_Drawable`, `C_Sprite`)
- Abstract base classes use `I` prefix for interfaces (e.g., `ICanvas<T>`)

**Functions:**
- Free functions: `camelCase` (e.g., `pushVec2`, `checkVec2`, `luaBindFunctions`)
- Member functions: `camelCase` (e.g., `getPosition()`, `setPosition()`, `isEnabled()`)
- Lua C API bindings follow pattern: `lua_<context>_<action>` (e.g., `lua_proxy_index_impl`, `lua_engine_scene_switch`, `lua_Vec2_add`)
- Class method pointers stored as public members use camelCase: `registerAll()`, `executeString()`

**Member Variables:**
- Private/protected members: `m_camelCase` prefix required by clang-tidy (e.g., `m_luaProxy`, `m_canvas`)
- Public struct fields: `camelCase` without prefix (e.g., in `Point`: `x`, `y`; in `Vec2`: `x`, `y`)
- Component flags often use lowercase: `enabled`, `active`, `valid`

**Constants:**
- `constexpr` types use `camelCase` (e.g., `EPS` for epsilon in tests, but stored as `static const double EPS`)
- Lua metatable names use ALL_CAPS: `"Vec2_METATABLE"`, `"Point_METATABLE"`, `"PROXY_METATABLE"`
- Global pointers: lowercase with underscores: `g_currentBindings`, `g_currentScene`

**Type Aliases:**
- `using` syntax in modern C++17 style (e.g., `using Canvas4 = Canvas<Pixel4, 160, 128>`)

**Enumerations:**
- PascalCase enum class names: `class Anchor { TOP_LEFT, CENTER, ... }`
- `class ScriptErrorPolicy { Disable, Log, Panic }`
- Enum values: `PascalCase` or `UPPERCASE_WITH_UNDERSCORES` depending on context

## Code Style

**Formatting:**
- Uses clang-format with file `.clang-format` configuration
- Indentation: 4 spaces (inferred from code samples)
- Line length: no hard limit enforced in config, but generally kept reasonable

**Linting:**
- **Tool:** clang-tidy
- **Config:** `.clang-tidy` file at project root
- **Run command:** `clang-tidy -p build src/core/*.cpp`
- **Key checks enabled:**
  - `bugprone-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `readability-*`, `clang-analyzer-*`
  - **Disabled:** magic numbers, varargs, C arrays (allowed for fixed-size arrays), pointer arithmetic checks
  - **WarningsAsErrors:** enabled (empty string means all warnings)
- **Target:** `include/enjin2/*` headers only (HeaderFilterRegex)

**Compiler Flags (from CMakeLists.txt):**
- `-MMD -MP` for dependency tracking
- `-std=c++17` enforced
- No special optimization flags enforced (platform-specific)

## Import Organization

**C++ Includes Order:**
1. Standard library headers (`<cstdint>`, `<cstring>`, `<cmath>`, `<memory>`, `<array>`, `<functional>`)
2. Project-relative includes in quotes: `#include "../../include/enjin2/..."`
3. Lua includes last if needed: `#include "lua_engine.hpp"`

**Include Guards:**
- Use `#pragma once` (modern style, seen throughout codebase)

**Path Aliases:**
- Not used in C++ code; relative paths from source directory are standard
- CMake handles include directories via `target_include_directories()`

## Error Handling

**Error Policy Pattern:**
- Component-scoped error handling via `ScriptErrorPolicy` enum (seen in `C_LuaScript`)
- **Three modes:**
  1. `Disable` (default): Sets `scriptError` flag to true after first error, skips subsequent calls
  2. `Log`: Logs errors to stderr but continues script execution (no `scriptError` flag)
  3. `Panic`: Errors trigger process abort (rarely used, intentional kill)

**Lua Error Handling (in C++):**
- Use `luaL_error(L, "message")` to raise Lua errors; these cause immediate longjmp
- Check userdata validity before dereferencing: `if (!proxy || !proxy->valid) { luaL_error(...) }`
- Use `luaL_checkstring()`, `luaL_checkinteger()`, `luaL_checknumber()` for argument validation
- Use `lua_tostring()`, `lua_tonumber()` for optional conversions (return nil if type mismatch)
- Stack safety: Always validate stack indices before access

**C++ Error Handling (in Components):**
- Use `assertRequires<T>()` in component `awake()` to declare dependencies
  - Debug builds: `assert(false)` with message, call stack identifies component
  - Release builds (ESP32): `printf()` warning, disable component, no abort
- Memory allocation failures: Use `std::unique_ptr` for RAII; if allocation fails during `addComponent<T>()`, returns `nullptr`
- No exceptions thrown; errors reported via return codes or boolean flags

**Null Pointer Checks:**
- Explicit null checks before dereferencing: `if (owner) { owner->setActive(...) }`
- Lua proxy validity checked on every access: `if (!proxy || !proxy->valid || !proxy->component)`

## Logging

**Framework:** C stdio (`cstdio`)

**Patterns:**
- Use `printf()` for informational messages
- Use `fprintf(stderr, "...")` for errors and test failures
- Lua errors logged via `luaL_error(L, ...)` which raises exception
- Component errors use `printf("[enjin2] ...")` prefix for clarity

**When to Log:**
- Release builds on ESP32: Log component dependency failures instead of aborting
- Test failures: Use `fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, msg)`
- Debug information: Optional Lua GC stats via `engine.lua.memory()`

## Comments

**Documentation Style:**
- Doxygen-style comments for public APIs: `/** @brief ... @param ... @return ... */`
- Single-line descriptions in `@brief` tags
- Multi-line descriptions in body between comment markers
- Inline code comments use `//` for short notes

**JSDoc/TSDoc Pattern:**
- Not used for C++ (Doxygen used instead)
- Doxygen tags: `@brief`, `@param`, `@return`, `@tparam` (for templates), `@file`, `@note`

**When to Comment:**
- Public class/function declarations always have Doxygen comment
- Complex algorithms or non-obvious behavior
- Lua-exposed functions always documented with their Lua API surface
- Error handling strategy explained inline if non-standard
- Phase milestone comments: `// Phase 29 complete: Object::getName() available`

## Function Design

**Size Guidelines:**
- Lua C API functions typically 20-50 lines (handle stack, validation, conversion, return)
- Helper functions (`pushVec2`, `checkVec2`) kept to 3-10 lines
- Member functions prefer brevity; complex logic extracted to static helpers

**Parameters:**
- Use const references for input: `const Point& other`
- Use pointers for optional/nullable inputs: `Object* owner`
- Lua C API always takes `lua_State* L` as first parameter
- Use positional parameters for component setters: `setPosition(int16_t x, int16_t y)`

**Return Values:**
- Constructors return nothing (normal C++ semantics)
- Lua C API functions return `int` (number of values pushed to stack)
- Getters return by const reference for heavy types: `const Point& getPosition() const`
- Boolean methods use `is*()` or `has*()` prefix (e.g., `isActive()`, `hasTag()`)
- Factory methods return pointers or `unique_ptr<T>` for owning allocation

## Module Design

**Exports:**
- All public APIs in `include/enjin2/` hierarchy
- Forward declarations in headers to break circular includes
- Private implementation in `src/` with corresponding header structure
- Example: `include/enjin2/scripting/bindings.hpp` → `src/scripting/bindings.cpp`

**Barrel Files:**
- Not heavily used; each component has its own header
- Top-level namespace: `namespace enjin2`
- No global `using namespace` in headers; explicit `enjin2::` in headers

**Circular Dependencies:**
- Avoided via forward declarations: `class C_LuaScript;` in `bindings.hpp`
- Full includes only in `.cpp` files where implementations can tolerate cycles
- `#pragma once` guards all headers

## Memory Management

**Smart Pointers:**
- `std::unique_ptr<Component>` for owned components in `Object`
- Used in container: `std::array<std::unique_ptr<Component>, MAX_COMPONENTS>`
- No `shared_ptr`; ownership is always clear

**Stack Allocation:**
- Preferred: Fixed-size types (`Point`, `Vec2`, `Rect`) allocated on stack
- Struct fields use value semantics, not pointers (e.g., `Point position;` not `Point* position;`)
- Canvas types: Templated with compile-time dimensions (e.g., `Canvas4<160, 128>`)

**Manual Memory (Lua):**
- Lua objects (`ScriptProxy`, `userdata`) managed by Lua GC
- C++ invalidates Lua references before destruction (set `proxy->valid = false` in destructor)
- No direct `new`/`delete` in high-level code; wrapped in `unique_ptr` when needed

## Platform-Specific Conventions

**Lua Integration:**
- LuaJIT for desktop (system `lua` package)
- LuaJIT custom build for WebAssembly (Emscripten)
- ESP-IDF Lua or NodeMCU for ESP32 (user-provided)

**Type-Erased Lua Canvas:**
- `LuaCanvas` wraps `Canvas4<W,H>` or `Canvas8<W,H>` or abstract `ICanvas<PixelT>`
- Discriminant flag `is4Bit` determines which template at runtime

**Static Assertions for Component Types:**
- `static_assert(std::is_base_of<Component, T>::value, "...")` ensures T is a Component

---

*Convention analysis: 2026-02-28*
