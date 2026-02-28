# Testing Patterns

**Analysis Date:** 2026-02-28

## Test Framework

**Runner:**
- CMake test framework (CTest)
- Executables defined in `tests/CMakeLists.txt` with `add_executable()` + `add_test()`
- Optional: Google Test (GTest) for some fixture-based tests (e.g., `sprite_load_test`)

**Assertion Library:**
- Manual assertion macro: `#define ASSERT(cond, msg)` (see pattern below)
- Google Test assertions: `ASSERT_*` and `EXPECT_*` (used in GTest-integrated tests)

**Run Commands:**
```bash
cmake -B build                  # Configure with tests enabled (default: ENJIN2_BUILD_TESTS=ON)
cmake --build build             # Build all targets including tests
ctest --output-on-failure       # Run all tests with failure output
ctest -V                        # Verbose test output
ctest -R collision_test         # Run specific test by name pattern
```

**Test Coverage:**
- No automated coverage tool enforced
- Coverage measurable via `gcov` if desired (not configured by default)
- Tests focus on C++ + Lua bindings, not unit code coverage metrics

## Test File Organization

**Location:**
- Co-located with source: `tests/` directory parallel to `src/`
- Test file naming: `{feature}_test.cpp` (e.g., `collision_test.cpp`, `math_binding_test.cpp`)
- Fixtures and helpers: Sometimes in separate header (e.g., `pikachu.h` in `tests/`)

**Naming:**
- Test functions: `test_<area>_<scenario>()` (e.g., `test_cpp_aabb()`, `test_Vec2_constructor_and_type()`)
- Requirements numbered: `test_err01_default_policy_is_disable()` (maps to ERR-01 requirement)
- Test data files: `test_pikachu.njn` (binary sprite asset for testing)

**Structure:**
```
tests/
├── CMakeLists.txt              # Build configuration for all tests
├── *_test.cpp                  # Individual test modules
├── pikachu.h                    # Shared test data (Pikachu image header)
└── test_pikachu.njn             # Binary sprite asset (.njn format)
```

## Test Structure

**Fixture Pattern:**

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

**Assertion Macro Pattern:**

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

**Test Function Pattern:**

```cpp
static void test_Vec2_constructor_and_type() {
    printf("--- Vec2 constructor and type ---\n");
    MathBindingFixture f;
    LuaResult r = f.exec(
        "local v = Vec2(3, 4)\n"
        "ok = (v ~= nil) and 1 or 0\n"
        "tx = (type(v) == 'userdata') and 1 or 0\n"
    );
    ASSERT(r.success, "Vec2(3,4) script should succeed");
    ASSERT(f.getNum("ok") == 1.0, "Vec2() should return non-nil");
    ASSERT(f.getNum("tx") == 1.0, "type(Vec2()) should be userdata");
}
```

**Setup/Teardown Pattern:**
- Constructor `MathBindingFixture()` calls `engine.initialize()` and `bindings.registerAll()`
- Destructor (implicit): LuaEngine and LuaBindings destroyed when fixture goes out of scope
- Each test function creates its own fixture instance for isolation
- No shared state between tests

**Test Entry Pattern (main function):**

```cpp
int main() {
    printf("=== Math Binding Tests ===\n\n");

    test_Vec2_constructor_and_type();
    test_Vec2_fields();
    test_Vec2_operators();
    // ... more tests

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", passes);
    printf("Failed: %d\n", failures);
    return failures > 0 ? 1 : 0;
}
```

## Test Types

**Unit Tests (C++):**
- Test core C++ classes and functions in isolation
- Example: `collision_test.cpp` tests `collision::aabb()`, `collision::circleCircle()`
- Example: `sprite_test.cpp` tests `SpriteSheet` animation and frame logic
- Use minimal fixtures; often just instantiate class and call methods
- Run without Lua when testing C++ only

**Integration Tests (C++ + Lua Bindings):**
- Test Lua bindings by executing Lua scripts against C++ engine
- Example: `math_binding_test.cpp` creates `Vec2` in Lua, tests arithmetic operations
- Example: `collision_test.cpp` tests both C++ collision functions AND Lua `engine.collision.*` API
- Fixture: `LuaEngine` + `LuaBindings` + optional canvas/scene setup
- Scripts embedded as C++ string literals; executed via `engine.executeString(code)`

**Lua Tests (Lua scripts):**
- Interactive demos and feature verification scripts in `scripts/` directory
- Example: `features_demo.lua` showcases sprite flipping, collision response, RNG
- Example: `pikachu_demo.lua` demos sprite animation and control
- Run via host application: `./sprite_sdl_test --lua scripts/features_demo.lua`
- Require host to push objects (sprites, canvas) via Lua API

**E2E Tests (Visual/Manual):**
- Not automated; see `examples/` directory for demo applications
- Example: `examples/lua_scripting/main.cpp` shows full integration
- Verify output visually or via image comparison (`image_comparison.cpp`)

## Mocking

**Framework:** Manual mocking via fixture setup

**Patterns:**

1. **Mock Object Creation:**
```cpp
Object obj;
C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
// Returns nullptr if component already exists or limit reached
```

2. **Mock Scene:**
```cpp
Scene scene;
Object* obj = scene.spawn<Object>();
// Spawn returns non-owning pointer; owned by scene
```

3. **Mock Input State:**
```cpp
InputState input;
input.setButtonDown(Button::A, true);
bool pressed = input.isButtonJustPressed(Button::A);
```

4. **Mock Canvas:**
```cpp
LuaBindings bindings(&engine);
bindings.setCanvas<Pixel4>(canvas4Ptr);
// Type-erases canvas for Lua operations
```

**No External Mocking Library:**
- No gmock, mockito, or similar used
- Mocking done via constructor injection or test-specific wrappers
- Example: `LuaBindings` accepts `LuaEngine*` pointer for dependency injection

**What to Mock:**
- External resources (files, network) — not done in these tests (no I/O tests shown)
- Scene/Object hierarchies — mock via `Scene::spawn()` and fixture setup
- Canvas rendering — pass test fixture canvas
- Lua scripts — embed as string literals in test code

**What NOT to Mock:**
- Core math types (`Vec2`, `Point`, `Rect`) — test with real instances
- Collision functions — test with real collision detection
- Component lifecycle — use real `Object::awake()`, `start()`, `update()`
- LuaEngine and LuaBindings — test with real instances (no mock)

## Fixtures and Factories

**Test Data Pattern:**

```cpp
// Static Lua code embedded in test file
static const char* k_buggyScript =
    "function update(self, dt)\n"
    "    error('boom')\n"
    "end\n";

static const char* k_goodScript =
    "function update(self, dt)\n"
    "    -- no error\n"
    "end\n";
```

**Fixture Factory:**
```cpp
struct StoreFixture {
    LuaEngine engine;
    LuaBindings bindings;

    StoreFixture() : bindings(&engine) {
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

**Location:**
- Fixtures defined in test `.cpp` file; no shared fixture header
- Helper functions for common operations (e.g., `pushVec2`, `checkVec2`) in implementation file
- Test data constants (`k_*`) defined at file scope before test functions
- Epsilon constant: `static const double EPS = 1e-5;` for floating-point comparisons

## Coverage

**Requirements:** Not enforced by automated tools

**Test Naming Strategy:**
- Tests map to requirements using prefixes: `test_err01_*` → ERR-01 requirement
- Example tests:
  - `error_policy_test.cpp`: ERR-01 through ERR-05 (ScriptErrorPolicy)
  - `gc_assert_test.cpp`: GC-01, GC-02 (Lua GC control)
  - `script_proxy_lifetime_test.cpp`: PROXY-STALE, TAG-01 through TAG-03

**View Coverage (Manual):**
```bash
# Generate gcov data (requires build with --coverage flag)
cmake -B build -DCMAKE_CXX_FLAGS="--coverage"
cmake --build build
ctest
gcov src/core/*.cpp
```

## Test Categories by Module

### Core Tests
- `named_objects_test.cpp`: Object naming and tagging (OBJ-01 through OBJ-04)
- `scene_transition_test.cpp`: Scene state machine transitions (SCENE-01 through SCENE-03)
- `scene_render_test.cpp`: Scene rendering with Pixel4 dispatch (RENDER-01)

### Graphics Tests
- `sprite_test.cpp`: SpriteSheet animation and frame calculation
- `sprite_flip_test.cpp`: Sprite flipping and rotation (flipH, flipV, rotate90)
- `sprite_sdl_test.cpp`: Visual sprite test with SDL3 rendering
- `sprite_load_test.cpp`: Binary sprite asset (.njn) loading with GTest
- `palette_test.cpp`: Palette color operations
- `shadow_mode_test.cpp`: Parallel backend testing (real vs. comparison output)
- `image_comparison.cpp`: Visual output validation helpers

### Scripting Tests
- `math_binding_test.cpp`: Vec2, Point, Rect userdata and operations
- `collision_test.cpp`: C++ collision functions + Lua engine.collision.* API
- `collision_response_test.cpp`: Collision response helpers (aabbOverlap, circleResponse, reflect)
- `rng_test.cpp`: Seeded RNG (engine.random.seed, integer, float)
- `store_test.cpp`: Persistent KV store (engine.store.*)
- `engine_table_test.cpp`: engine.* global table structure (ENG-01 through ENG-06)
- `error_policy_test.cpp`: ScriptErrorPolicy (ERR-01 through ERR-05)
- `input_event_callback_test.cpp`: Input callbacks (INPUT-01 through INPUT-03)
- `text_binding_test.cpp`: Text rendering (text, textWrapped, measurement)
- `layer_binding_test.cpp`: Layer management (setLayer, getLayer, clearLayer)
- `hot_reload_test.cpp`: Script hot-reload lifecycle
- `gc_assert_test.cpp`: Lua GC control + component assertions (GC-01, GC-02, DEP-01 through DEP-03)
- `script_proxy_lifetime_test.cpp`: Object proxy safety (PROXY-STALE, TAG-01 through TAG-03)
- `object_proxy_test.cpp`: ObjectProxy validation and stale invalidation

### Input Tests
- `input_test.cpp`: Button and axis input state machine
- `input_event_callback_test.cpp`: Edge-triggered callbacks (just_pressed, just_released)

### Component Tests
- `drawable_decoupling_test.cpp`: C_Drawable component isolation (Phase 36 regression)

### Build/Linking Tests
- `compositor_test.cpp`: Multi-layer composition (LAYER-01 through LAYER-04)

## Special Test Features

**Requirement Numbering:**
- Tests prefixed with phase/requirement code (e.g., PROXY-STALE, TAG-01)
- Each test function documents which requirement it validates
- File header comments list all requirements tested

**Lua Script Embedding:**
- Lua code embedded as C++ raw string literals
- Multi-line scripts indented for readability
- No external Lua files needed for core tests (files in `scripts/` are for demos)

**Asset File Testing:**
- Binary sprite file `test_pikachu.njn` copied to build directory via CMakeLists
- GTest uses `sprite_load_test.cpp` with GoogleTest framework for structured assertions
- Image comparison helpers in `image_comparison.cpp` for visual test validation

**Lua Execution Model:**
- Each fixture creates fresh `LuaEngine` instance
- `bindings.registerAll()` initializes all Lua tables and functions
- Tests execute Lua code via `engine.executeString()` which returns `LuaResult{success, error}`
- Global variables set in Lua are read back via `engine.getGlobalNumber()`, `engine.getGlobalString()`

## Common Patterns

**Async Testing (Simulation):**
```cpp
Object obj;
C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
script->loadScript(k_code);

// Simulate frame updates
script->update(0.016f);  // Frame 1
script->update(0.016f);  // Frame 2

// Check state after updates
ASSERT(script->hasErrors() || !script->hasErrors(), "expected state");
```

**Error Testing:**
```cpp
LuaResult r = f.exec("error('test error')");
ASSERT(!r.success, "error() should set success=false");
ASSERT(r.error.length() > 0, "error message should be populated");
```

**Type Testing (Userdata):**
```cpp
LuaResult r = f.exec(
    "local v = Vec2(1, 2)\n"
    "tx = (type(v) == 'userdata') and 1 or 0\n"
);
ASSERT(f.getNum("tx") == 1.0, "Vec2 should be userdata type");
```

**Boundary Testing:**
```cpp
// AABB collision at edges
ASSERT(!collision::aabb(0, 0, 10, 10, 10, 0, 5, 5), "touching edge — no overlap");
ASSERT(collision::aabb(0, 0, 10, 10, 9, 9, 9, 9), "overlap at corner");
```

**Performance Regression Testing:**
- No formal performance tests
- Benchmark examples in `examples/` (not part of test suite)
- Tests focus on correctness, not timing

---

*Testing analysis: 2026-02-28*
