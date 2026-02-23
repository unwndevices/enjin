# Testing Patterns

**Analysis Date:** 2026-02-23

## Test Framework

**Runner:**
- No unit test framework (no Google Test, Catch2, doctest, or CTest test cases detected)
- Tests are standalone executable programs built with CMake
- Config: `tests/CMakeLists.txt`

**Assertion Library:**
- None. Tests use stdio (`fprintf`, `printf`) and process exit codes (0 = pass, 1 = fail)

**Run Commands:**
```bash
# Build (from build directory)
cmake .. && cmake --build .

# Run shadow mode visual test
./tests/shadow_mode_test [backend]          # Renders a scene and exports BMP

# Run image comparison
./tests/image_comparison_test <file1.bmp> <file2.bmp>   # Compares two BMP files

# Example pipeline: render and compare
./shadow_mode_test enjin2
./image_comparison_test output-enjin2.bmp reference.bmp
```

## Test File Organization

**Location:** All tests live in `tests/` at the project root. No co-located test files alongside source.

**Naming:**
- Test executables use descriptive `_test` suffix: `shadow_mode_test`, `image_comparison_test`
- Source files match executable name: `tests/shadow_mode_test.cpp`, `tests/image_comparison.cpp`

**Structure:**
```
tests/
├── CMakeLists.txt          # Builds both test executables
├── shadow_mode_test.cpp    # Integration/visual test: renders scene to BMP
└── image_comparison.cpp    # Utility: pixel-diff comparison of two BMP files
```

## Test Structure

**Test Executable Pattern:**
Tests are `main()`-based programs that return 0 on pass, 1 on fail:

```cpp
int main(int argc, char* argv[]) {
    // Optional: parse backend/mode from argv
    const char* backend = (argc > 1) ? argv[1] : "default";

    // Setup: create canvas and scene
    Canvas8_128x128 canvas;
    Scene testScene(1);
    testScene.initialize();
    testScene.activate();

    // Exercise: create objects and run simulation
    Object* obj1 = testScene.addObject<Object>();
    obj1->addComponent<C_Position>(10, 10);
    obj1->addComponent<C_Rectangle>(30, 30, 80);

    for (uint16_t frame = 0; frame < 60; ++frame) {
        testScene.update(16);   // ~60fps delta
        testScene.render(canvas);
    }

    // Assert: export to file and compare externally
    canvas.exportToBMP("output-enjin2.bmp");
    return 0;
}
```

**Image Comparison Pattern:**
Visual correctness is verified by pixel-diff comparison using `stb_image`:

```cpp
float compareBMP(const char* file1, const char* file2) {
    // Load both images with stb_image
    // Verify dimensions match
    // Count differing pixels
    // Return percentage difference
}

int main(int argc, char* argv[]) {
    float diffPercent = compareBMP(argv[1], argv[2]);
    const float TOLERANCE = 3.0f;
    if (diffPercent <= TOLERANCE) {
        printf("Result: PASS (within %.1f%% tolerance)\n", TOLERANCE);
        return 0;
    } else {
        printf("Result: FAIL (exceeds %.1f%% tolerance)\n", TOLERANCE);
        return 1;
    }
}
```

**Patterns:**
- Setup: construct canvas + scene, call `initialize()` + `activate()`
- Simulation: run N frames via `update(deltaTime)` + `render(canvas)`
- Assertion: export canvas to BMP, compare externally with `image_comparison_test`
- No teardown needed - objects use RAII

## Mocking

**Framework:** None. No mocking library is used.

**Patterns:**
Test-specific drawable components are implemented as inner classes directly in test files:

```cpp
namespace enjin2 {
/**
 * @brief Simple test drawable that renders a filled rectangle
 */
class C_Rectangle : public C_Drawable {
    uint8_t color;
public:
    C_Rectangle(Object* owner, uint8_t width, uint8_t height, uint8_t rect_color)
        : C_Drawable(owner, width, height), color(rect_color) {}

    void draw(ICanvas<uint8_t>& canvas) override {
        if (!is_visible) return;
        Rect rect(GetOffsetPosition().x, GetOffsetPosition().y, width, height);
        canvas.fill(rect, color);
    }
};
} // namespace enjin2
```

**What to Mock:**
- Drawable behavior: create minimal `C_X` subclasses inside the test file implementing only `draw()`
- Canvas output: use `Canvas8_128x128` directly (it's a lightweight in-memory buffer)

**What NOT to Mock:**
- The scene, object, and component system - test against real implementations
- The canvas - its behavior IS what's being tested

## Fixtures and Factories

**Test Data:**
No shared fixture infrastructure. Each test file sets up its own objects inline:

```cpp
// Object setup pattern
Object* obj = testScene.addObject<Object>();
C_Position* pos = obj->addComponent<C_Position>(x, y);
C_Rectangle* rect = obj->addComponent<C_Rectangle>(w, h, color);
rect->SetDrawLayer(DrawLayer::Background);
```

**Test Constants:**
Defined as local `const` variables or macros within `main()`:
```cpp
const uint16_t FRAME_COUNT = 60;
const uint16_t DELTA_TIME = 16; // ~60 FPS (16ms per frame)
const float TOLERANCE = 3.0f;   // Max allowed pixel difference %
```

**Location:** Fixtures are inline in `tests/*.cpp`. No shared fixture headers.

## Coverage

**Requirements:** None enforced. No code coverage tooling configured in CMake.

**View Coverage:** Not configured.

## Test Types

**Visual / Snapshot Tests (`tests/shadow_mode_test.cpp`):**
- Renders the engine scene for N frames into a `Canvas8_128x128`
- Exports output as a BMP file (`output-enjin2.bmp`)
- Pass/fail determined by comparing with a reference BMP via `image_comparison_test`
- Tolerance: 3% pixel difference accepted

**Image Comparison Utility (`tests/image_comparison.cpp`):**
- Standalone tool that loads two BMP files using `stb_image` (from `vendor/`)
- Compares RGB values pixel-by-pixel
- Returns exit code 0 if within tolerance (3%), 1 otherwise
- Used as the assertion step of visual tests in CI pipelines

**Integration Tests:**
- `shadow_mode_test` constitutes an integration test: it exercises the full stack from `Scene` → `Object` → `Component` → `Canvas` → BMP export
- No unit tests for individual methods or classes are present

**E2E Tests:**
- The shadow mode pipeline (render → export → compare) serves as an E2E verification of rendering fidelity between backends

## CI Integration

Visual tests are integrated into the CI pipeline as a Doxygen warning threshold gate (see `.github/workflows/`). The shadow mode test binary is built and used to verify rendering output doesn't regress. The image comparison tool provides the assertion step.

## Vendor Dependencies for Tests

- `stb_image.h` from `vendor/` directory - used in `image_comparison.cpp` for BMP loading
- Included via `target_include_directories(image_comparison_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor)`

## Common Patterns

**Running a simulation:**
```cpp
const uint16_t DELTA_TIME = 16; // ~60 FPS
for (uint16_t frame = 0; frame < FRAME_COUNT; ++frame) {
    testScene.update(DELTA_TIME);
    testScene.render(canvas);
}
```

**Timing a test:**
```cpp
auto start_time = std::chrono::high_resolution_clock::now();
// ... simulation ...
auto end_time = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
```

**Exporting for visual assertion:**
```cpp
canvas.exportToBMP("output-enjin2.bmp");
// Then: ./image_comparison_test output-enjin2.bmp reference.bmp
```

---

*Testing analysis: 2026-02-23*
