# Testing Patterns

**Analysis Date:** 2026-02-27

## Test Framework

**Runner:**
- CMake test framework (CTest) with hand-written assertions
- No Google Test, Catch2, or Doctest dependency
- Tests are standalone executables linked against `enjin2` libraries

**Build Configuration:**
- Tests enabled by default: `ENJIN2_BUILD_TESTS ON`
- Located in: `tests/` directory
- CMake config: `tests/CMakeLists.txt` defines all test targets and linking

**Test Execution:**
```bash
cd build
ctest                              # Run all tests
ctest -R sprite_test              # Run specific test
ctest --output-on-failure         # Show output only if failed
```

**Assertion Library:**
- Hand-written macro-based assertions (no external library)
- Standard pattern across all tests:
  ```cpp
  static int passes = 0;
  static int failures = 0;

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

## Test File Organization

**Location:**
- All tests in `tests/` directory at project root
- No subdirectories; flat file structure

**Naming Convention:**
- Format: `{feature}_test.cpp` or `{feature}_{aspect}_test.cpp`
- Examples:
  - `sprite_test.cpp` - SpriteSheet and C_Sprite component
  - `error_policy_test.cpp` - Lua script error handling
  - `math_binding_test.cpp` - Math Lua bindings
  - `gc_assert_test.cpp` - GC control and component assertions

**Test Discovery:**
- CMakeLists.txt explicitly declares each test as `add_executable(...)` → `add_test(...)`
- Not automatically discovered; new tests require CMakeLists.txt entry

**File Structure Example:**
```
tests/
├── CMakeLists.txt                 # Test build configuration
├── sprite_test.cpp                # Graphics/sprite system tests
├── error_policy_test.cpp          # Lua script error policies (ERR-01..ERR-05)
├── gc_assert_test.cpp             # GC bindings + component dependency assert (GC-01..DEP-03)
├── math_binding_test.cpp          # Math Lua bindings (Vec2, Point, Rect, utilities)
├── collision_test.cpp             # Collision Lua bindings
├── input_event_callback_test.cpp  # Input event callbacks (INPUT-01..INPUT-03)
├── named_objects_test.cpp         # Object name/tag identity (OBJ-01..OBJ-04)
├── scene_transition_test.cpp      # Scene transitions (SCENE-01..SCENE-03)
├── layer_binding_test.cpp         # Lua layer API
├── pikachu.h                       # Shared sprite test data (Aseprite export)
└── ... (other tests)
```

## Test Structure

**Suite Organization:**
- No test suite grouping (traditional unittest/gtest-style suites)
- Instead: individual test functions prefixed with test name + requirement ID
- Main function calls test functions in order, collects pass/fail counts

**Example Test Pattern from `error_policy_test.cpp`:**
```cpp
static void test_err01_default_policy_is_disable() {
    printf("--- ERR-01: default policy is Disable ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "ERR-01: addComponent<C_LuaScript> should succeed");

    ASSERT(script->getErrorPolicy() == ScriptErrorPolicy::Disable,
           "ERR-01: default errorPolicy should be Disable");
}

// In main():
int main() {
    printf("error_policy_test\n");
    printf("=================\n");

    test_err01_default_policy_is_disable();
    test_err02_disable_policy_stops_after_error();
    test_err03_log_policy_continues_after_error();
    // ... more tests ...

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
```

**Patterns:**
- **Setup:** Create objects/components in the test function (no shared fixtures except when needed)
- **Teardown:** Automatic via scope (destructors called when test function exits)
- **Assertion:** `ASSERT(condition, "message")` macro - increments pass or failure counter
- **Output:** printf for test names and progress; fprintf(stderr, ...) for failures

## Fixtures and Factories

**Test Data Structures:**
- Lightweight fixtures defined inline in test functions
- Struct fixtures with setup in constructor:

**Example from `math_binding_test.cpp`:**
```cpp
struct MathBindingFixture {
    LuaEngine engine;
    LuaBindings bindings;

    MathBindingFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name, -999.0);
    }

    std::string getStr(const char* name) {
        return engine.getGlobalString(name, "<<not set>>");
    }
};
```

**Example from `gc_assert_test.cpp`:**
```cpp
struct GCFixture {
    LuaEngine engine;
    LuaBindings bindings;

    GCFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }
};
```

**Shared Test Data:**
- Sprite data (Aseprite exports): `tests/pikachu.h` - included by sprite tests
- Hardcoded arrays: uint8_t arrays for sprite/canvas tests (no external asset files)
- String literals: Lua code embedded as `static const char*` in test files

## Coverage

**Requirements:** No coverage targets or enforced thresholds

**View Coverage:**
- Not configured in CMake
- Manual inspection via test execution output only

**Test Count by System:**

| System | Test | Files Covered |
|--------|------|----------------|
| **Core Components** | `sprite_test.cpp` | C_Sprite, SpriteSheet, animation |
| **Error Handling** | `error_policy_test.cpp` | C_LuaScript error policies |
| **GC & Assertions** | `gc_assert_test.cpp` | engine.lua.collect/memory, component dependencies |
| **Math Bindings** | `math_binding_test.cpp` | Vec2, Point, Rect, utility functions |
| **Collision** | `collision_test.cpp` | engine.collision.* Lua API |
| **Input** | `input_event_callback_test.cpp` | on_button_pressed/released callbacks |
| **Layer API** | `layer_binding_test.cpp` | setLayer, getLayer, clearLayer |
| **Text API** | `text_binding_test.cpp` | text(), textWrapped(), font switching |
| **Objects** | `named_objects_test.cpp` | Object names and tags (OBJ-01..OBJ-04) |
| **Scenes** | `scene_transition_test.cpp` | Scene switching and transitions |
| **Graphics** | `compositor_test.cpp`, `shadow_mode_test.cpp` | Compositor and rendering |
| **Input** | `input_test.cpp` | Input abstraction |
| **Palette** | `palette_test.cpp` | Palette color conversion |

## Test Types

**Unit Tests (Dominant):**
- Scope: Individual components or small subsystems
- Approach: Create objects/components, exercise API, assert state changes
- Example: `sprite_test.cpp` - tests SpriteSheet::draw() pixel-by-pixel
- Location: `tests/{feature}_test.cpp`

**Integration Tests:**
- Scope: C++ code + Lua scripting system interaction
- Approach: Create LuaEngine → register bindings → execute Lua code → check results
- Examples:
  - `error_policy_test.cpp` - Lua script loading + error policy enforcement
  - `math_binding_test.cpp` - Lua math userdata operations
  - `gc_assert_test.cpp` - Lua GC control bindings
- Location: Tests requiring `enjin2_lua` library (gated with `if(ENJIN2_BUILD_LUA)`)

**Visual/Regression Tests:**
- Scope: Rendering output verification (optional)
- Approach: Draw to canvas, compare pixels to expected values
- Examples:
  - `sprite_test.cpp` - SpriteSheet draw verification (pixel-level)
  - `shadow_mode_test.cpp` - Parallel backend comparison (verify both produce same output)
  - `image_comparison.cpp` - Generic image diff utility
- Location: Standalone executables with pixel-level assertions

**E2E Tests:**
- Scope: Not used in this codebase
- Framework: None configured

## Common Patterns

**Async Testing:**
- Not applicable (no async/coroutines)
- Lua scripts execute synchronously via `executeString()` or `loadScript()` + manual `update()` calls

**Error Testing:**
- Script errors: Load buggy Lua code, call update(), check `hasErrors()` and error message
- Example from `error_policy_test.cpp`:
  ```cpp
  static const char* k_buggyScript =
      "function update(self, dt)\n"
      "    error('boom')\n"  // This Lua error will be caught
      "end\n";

  script->loadScript(k_buggyScript);
  script->update(0.016f);  // Triggers the error
  ASSERT(script->hasErrors(), "Script should report error after update");
  ```

**State Verification:**
- After operations, check object/component state via getters
- Example from `gc_assert_test.cpp`:
  ```cpp
  // DEP-01: assertRequires() happy path
  Object obj;
  obj.addComponent<C_TestDep>();
  C_RequiresTestDep* c = obj.addComponent<C_RequiresTestDep>();
  obj.awake();  // Triggers assertRequires<C_TestDep>()
  ASSERT(c->isEnabled(), "Component should remain enabled when dep present");
  ```

**Lua Execution Pattern:**
- All Lua tests follow this structure:
  1. Create LuaEngine + LuaBindings fixture
  2. Execute Lua code via `engine.executeString(code)`
  3. Query global variables via `engine.getGlobalNumber()` / `getGlobalString()`
  4. Assert expected values
- Example from `math_binding_test.cpp`:
  ```cpp
  MathBindingFixture f;
  LuaResult r = f.exec(
      "local v = Vec2(3, 4)\n"
      "ok = (v ~= nil) and 1 or 0\n"
  );
  ASSERT(r.success, "Script should execute successfully");
  ASSERT(f.getNum("ok") == 1.0, "Vec2() should return non-nil");
  ```

**Conditional Compilation in Tests:**
- Tests gated by `if(ENJIN2_BUILD_LUA)` in CMakeLists.txt
- Runtime NDEBUG checks for release-specific behavior:
  ```cpp
  #ifdef NDEBUG
  static void test_dep03_release_missing_disables() {
      // Release-only test: missing dep disables component
      // (Debug path calls assert(false) → abort, deliberately excluded)
  }
  #endif
  ```

## Linking & Dependencies

**Test Linking (from `tests/CMakeLists.txt`):**

**Core-only tests:**
```cmake
target_link_libraries(sprite_test PRIVATE enjin2)
```

**Lua-dependent tests (gated by ENJIN2_BUILD_LUA):**
```cmake
if(ENJIN2_BUILD_LUA)
    add_executable(error_policy_test error_policy_test.cpp)
    target_include_directories(error_policy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    target_link_libraries(error_policy_test PRIVATE
        enjin2
        enjin2_lua  # Adds Lua bindings
    )
    add_test(NAME error_policy_test COMMAND error_policy_test)
endif()
```

**SDL-dependent tests (gated by ENJIN2_BUILD_SDL):**
```cmake
if(ENJIN2_BUILD_SDL)
    add_executable(sprite_sdl_test sprite_sdl_test.cpp)
    target_link_libraries(sprite_sdl_test PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_input
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
        SDL3::SDL3
    )
endif()
```

## Phase/Requirement Tracking

**Convention:** Test functions named with requirement ID suffix
- Pattern: `test_{featurename}_{reqid}_{description}()`
- Example: `test_err01_default_policy_is_disable()`, `test_dep03_release_missing_disables()`

**Mapping to Implementation Plans:**
- Test names include phase IDs: `ERR-01`, `GC-02`, `INPUT-01`, `OBJ-04`, `SCENE-01`, `LAYER-01`
- Allows cross-reference from `.planning/phases/` documentation to test verification
- Test output shows requirement ID for traceability:
  ```
  --- ERR-01: default policy is Disable ---
  --- ERR-02: Disable policy stops script after error ---
  ```

---

*Testing analysis: 2026-02-27*
