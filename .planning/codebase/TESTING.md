# Testing Patterns

**Analysis Date:** 2026-03-01

## Test Framework

**Runner:**
- CMake `add_test()` with CTest (no external test framework like Catch2, Google Test, etc.)
- Each test is a standalone executable (C++ main program) that returns 0 on success, non-zero on failure
- Tests built and run via: `cmake --build build && ctest --output-on-failure`

**Test Location:**
- All tests: `tests/` directory at repository root
- CMakeLists.txt in `tests/` defines test executables and links to `enjin2` library

**Assertion Library:**
- Custom macro `ASSERT(condition, message)` — no external library
- Defined in each test file for flexibility
- Pattern:
  ```cpp
  static int passes = 0;
  static int failures = 0;

  #define ASSERT(cond, msg) \
      do { \
          if (!(cond)) { \
              fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
              failures++; \
          } else { \
              passes++; \
          } \
      } while(0)
  ```

**Run Commands:**
```bash
# Build all tests
cmake --build /home/unwn/dev/enjin/build

# Run all tests
ctest --test-dir /home/unwn/dev/enjin/build --output-on-failure

# Run specific test
ctest --test-dir /home/unwn/dev/enjin/build -R physics_lua_test --output-on-failure

# Build and run from scratch
cd /home/unwn/dev/enjin && cmake -B build && cmake --build build && ctest --test-dir build
```

## Test File Organization

**Location:**
- All test files in `tests/` directory (co-located with library, not alongside source)
- Files: `tests/*_test.cpp`

**Naming:**
- Test files: `<feature>_test.cpp` (e.g., `physics_lua_test.cpp`, `engine_table_test.cpp`)
- Standalone files for fixtures/helpers: `tests/pikachu.h` (sprite test data)

**Structure:**
```
tests/
├── physics_lua_test.cpp        # Lua physics binding tests (Phase 45 PHYS-09..PHYS-13)
├── engine_table_test.cpp       # engine.* global table tests (Phase 31 ENG-01..ENG-06)
├── math_binding_test.cpp       # Vec2, Point, Rect Lua binding tests
├── error_policy_test.cpp       # Error handling policies for C_LuaScript
├── camera_lua_test.cpp         # Camera component Lua binding tests
├── collision_test.cpp          # Collision system tests
├── input_test.cpp              # Input abstraction tests
├── CMakeLists.txt              # Test build configuration
└── pikachu.h                   # Sprite test data (header-only)
```

## Test Structure

**Suite Organization:**

Tests follow a functional/scenario structure, NOT xUnit-style suites. Each test is a standalone function:

```cpp
// tests/physics_lua_test.cpp
static void test_gravity_round_trip() {
    printf("--- setGravity/getGravity round-trip ---\n");

    LuaEngine eng;
    eng.initialize();
    LuaBindings bindings(&eng);
    bindings.registerAll();

    runLua(eng, R"(
        engine.physics.setGravity(0, 500)
        local gx, gy = engine.physics.getGravity()
        assert(math.abs(gy - 500) < 0.01)
    )", "setGravity/getGravity");

    printf("  PASS: gravity round-trip\n");
}
```

**Main Function Pattern:**

```cpp
int main() {
    printf("=== test_suite_name ===\n");

    test_gravity_round_trip();
    test_applyGravity_form();
    test_bounce_elastic();
    // ... more tests

    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
```

**Test Grouping:**
- Group related tests into logical sections (e.g., gravity tests, collision tests)
- Print section headers with `printf()` before each group
- Count global `passes` and `failures` counters across all tests

**Patterns:**

1. **Setup Fixture Pattern:**
   ```cpp
   struct LuaEngineFixture {
       LuaEngine engine;
       LuaBindings bindings;

       LuaEngineFixture() : bindings(&engine) {
           engine.initialize();
           bindings.registerAll();
       }

       LuaResult exec(const char* code) {
           return engine.executeString(code);
       }
   };
   ```

2. **Helper Function Pattern (for Lua testing):**
   ```cpp
   static void runLua(LuaEngine& eng, const char* code, const char* label) {
       LuaResult r = eng.executeString(code);
       if (!r.success) {
           printf("FAIL: %s — Lua error: %s\n", label, r.error.c_str());
           exit(1);
       }
   }
   ```

3. **Assertions with Context:**
   ```cpp
   ASSERT(script != nullptr, "ERR-01: addComponent<C_LuaScript> should succeed");
   ASSERT(script->getErrorPolicy() == ScriptErrorPolicy::Disable,
          "ERR-01: default errorPolicy should be Disable");
   ```

## Mocking

**Framework:** No mocking library (Google Mock not used)

**Patterns:**

1. **Minimal Fake Objects:**
   - Create lightweight concrete implementations for testing
   - Example: `MinimalScene` in `engine_table_test.cpp`:
     ```cpp
     struct MinimalScene : Scene {
         explicit MinimalScene(uint32_t id) : Scene(id) {}
     };
     ```

2. **Null-Guard Testing:**
   - Test behavior when dependencies are not injected (e.g., no `setActiveScene()`)
   - Verify safe defaults: returns false, nullptr, 0
   - Example: `test_engine_scene_null_guards()` calls `engine.scene.find()` with no scene injected

3. **Pointer Injection (Not Mocks):**
   ```cpp
   // Instead of mocking, use real objects with setter injection
   MinimalScene scene(1u);
   Object* obj = scene.addObject<Object>();
   obj->setName("hero");

   bindings.setActiveScene(&scene);  // Inject real scene

   LuaResult r = f.exec("found = (engine.scene.find('hero') ~= nil) and 1 or 0");
   ```

**What to Mock:**
- Complex external systems (not present in tests currently)
- Simulated time/input state via `setTimeState()`, `setInput()`

**What NOT to Mock:**
- Core engine objects (Scene, Object, Component) — use real instances
- Lua bindings — test live wiring by calling actual Lua and reading results
- Physics calculations — test actual math functions with known inputs/outputs

## Fixtures and Factories

**Test Data:**

Example from `physics_lua_test.cpp`:
```cpp
// Run a Lua snippet and check it succeeds
static void runLua(LuaEngine& eng, const char* code, const char* label) {
    LuaResult r = eng.executeString(code);
    if (!r.success) {
        printf("FAIL: %s — Lua error: %s\n", label, r.error.c_str());
        exit(1);
    }
}
```

Example from `error_policy_test.cpp`:
```cpp
// Lua code for a script that errors on every update() call
static const char* k_buggyScript =
    "function update(self, dt)\n"
    "    error('boom')\n"
    "end\n";

static const char* k_goodScript =
    "function update(self, dt)\n"
    "    -- no error\n"
    "end\n";
```

**Location:**
- Inline in test file as static const strings or local variables
- No separate fixtures directory; fixtures co-located with tests
- Sprite test data: `pikachu.h` (header-only PNG image data)

**Factory Patterns:**
- `Object obj; obj.addComponent<C_LuaScript>(16u, 16u);` — inline creation
- Scene with objects: Create scene, call `scene.addObject<Object>()`, name it, inject via binding
- No factory class; direct use of constructors

## Coverage

**Requirements:** None enforced

**View Coverage:**
- Coverage not automatically tracked in CI
- Manual coverage analysis via `gcov` if needed:
  ```bash
  # Compile with coverage flags
  cmake -B build -DCMAKE_CXX_FLAGS=--coverage
  cmake --build build && ctest --test-dir build

  # Generate report
  gcov tests/physics_lua_test.cpp
  lcov --directory . --capture --output-file coverage.info
  genhtml coverage.info --output-directory coverage_html
  ```

## Test Types

**Unit Tests:**
- Scope: Single function or component behavior
- Approach: Create object, call method, verify output via Lua global variables
- Examples: `test_Vec2_constructor_and_type()`, `test_err02_disable_policy_stops_after_error()`

**Integration Tests:**
- Scope: Multiple components working together (e.g., Lua bindings + C++ engine)
- Approach: Initialize LuaEngine, register bindings, execute Lua, verify results
- Examples: `test_engine_scene_live_switch()`, `test_engine_scene_live_find()`

**Lua Binding Tests:**
- Scope: Verify C++ functions are accessible and work correctly from Lua
- Approach: Execute Lua code via `engine.executeString()`, read globals, assert values
- Examples: `physics_lua_test.cpp`, `engine_table_test.cpp`, `math_binding_test.cpp`

**E2E Tests:**
- Scope: Not used in this codebase
- Alternative: Examples in `examples/` directory demonstrate full workflows (but not automated tests)

## Common Patterns

**Async Testing:**
Not applicable to this single-threaded game engine.

**Error Testing:**

Pattern 1: Verify error is raised (Lua)
```cpp
// tests/error_policy_test.cpp
script->update(0.016f);  // Triggers error
ASSERT(script->hasErrors(), "hasErrors() should be true after error");
```

Pattern 2: Verify error message (Lua)
```cpp
// tests/engine_table_test.cpp
LuaResult r3 = f.exec("local _ = brick.name");
ASSERT(!r3.success, "accessing destroyed proxy should raise an error");
```

Pattern 3: Verify null-guard behavior
```cpp
// tests/engine_table_test.cpp — no input set, functions return safe defaults
LuaResult r = f.exec(
    "h  = engine.input.held(0)          and 1 or 0\n"
);
ASSERT(f.getNum("h") == 0.0, "held() should return false when no input");
```

**Floating-Point Assertions:**

```cpp
#define ASSERT_NEAR(a, b, eps, msg) \
    do { if (std::fabs((a)-(b)) > (eps)) { \
        printf("FAIL: %s — got %f, expected %f\n", msg, (double)(a), (double)(b)); \
        exit(1); } } while(0)

// Usage
ASSERT_NEAR(f.getNum("vy"), 500, 0.01, "gravity vy should be ~500");
```

**Lua Assertion Helper:**
```cpp
// Run Lua with assertions inside
runLua(eng, R"(
    assert(type(engine.physics) == "table", "engine.physics must be a table")
    assert(math.abs(gx) < 0.001, "gravity should be 0")
)", "gravity table checks");
```

**Requirement-Driven Testing:**

Tests directly reference phase and requirement IDs in comments and output:

```cpp
// tests/engine_table_test.cpp
static void test_engine_scene_live_switch() {
    printf("--- engine.scene.switch live wiring (ENG-01) ---\n");
    // ...
    ASSERT(mockSSM.getCurrentScene()->getId() == 2u, "ENG-01: current scene is now scene 2");
}
```

## Test Lifecycle

**Setup (Before Each Test):**
- Create test object/fixture (usually inline in test function)
- Initialize engine/bindings
- Prepare test data (Lua code strings, scene objects, etc.)

**Teardown (After Each Test):**
- No explicit cleanup needed; RAII via destructors (scoped fixture objects)
- Lua engine, scene, objects all freed when they go out of scope
- Memory validated by compiler/sanitizer in CI

**Global State:**
- Test functions are independent; no shared state between tests
- Each test creates fresh LuaEngine, Scene, Object instances
- Global counters (`passes`, `failures`) accumulated across all tests

## Test Execution in CI/CD

**Build:**
- CMake configures test executables and links to library
- All tests compiled into `build/tests/` directory

**Run:**
- CTest runs all executable tests in sequence
- Output captured with `--output-on-failure`
- Exit code: 0 if all tests pass, non-zero if any fail

**Reporting:**
- PASS/FAIL printed to stdout
- Summary line: `=== Results: X passed, Y failed ===`
- Lua errors printed with context (line numbers, labels)

---

*Testing analysis: 2026-03-01*
