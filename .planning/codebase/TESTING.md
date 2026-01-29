# Testing Patterns

**Analysis Date:** 2026-01-29

## Test Framework

**Runner:** None (manual testing approach)

**Assertion Library:** None (manual printf-based verification)

**Config:** `enjin2/tests/CMakeLists.txt` contains only placeholder:
```cmake
cmake_minimum_required(VERSION 3.16)
# Placeholder for tests - will be implemented later
```

**Run Commands:**
```bash
# Build examples (act as integration tests)
cd enjin2 && mkdir -p build && cd build
cmake .. -DENJIN2_BUILD_EXAMPLES=ON
make

# Run individual test examples
./basic_drawable_test
./simple_graphics_test
./hardware_integration_test
./vcvrack_integration_test
```

## Test File Organization

**Location:** Co-located with examples in `enjin2/examples/`

**Naming:** Pattern: `[feature]_[test_type].cpp`
- `basic_drawable_test.cpp` - Basic drawable components test
- `simple_graphics_test.cpp` - Graphics export functionality test
- `hardware_integration_test.cpp` - Hardware integration simulation test
- `vcvrack_integration_test.cpp` - VCVRack compatibility test
- `optimization_test.cpp` - Performance optimization test
- `esp32_compatibility_test.cpp` - ESP32 platform compatibility test

**Structure:**
```
enjin2/
├── examples/
│   ├── basic_drawing/
│   │   └── CMakeLists.txt
│   ├── ecs_demo/
│   │   └── CMakeLists.txt
│   ├── basic_drawable_test.cpp
│   ├── simple_graphics_test.cpp
│   ├── hardware_integration_test.cpp
│   ├── vcvrack_integration_test.cpp
│   └── ...
└── tests/
    └── CMakeLists.txt (placeholder only)
```

## Test Structure

**Suite Organization:**
```cpp
/**
 * @brief Test suite for [feature]
 *
 * Tests [what this suite validates]
 */
class TestClassName {
private:
    // Test fixtures/setup data
    Canvas8<128, 64> main_canvas;
    uint32_t frame_count;

public:
    // Test methods
    void testSpecificFeature() {
        printf("=== Test Name ===\n");
        // Test implementation
        printf("✓ Test passed\n");
    }

    void runAllTests() {
        // Run all test methods
    }
};

int main() {
    TestClassName test;
    test.runAllTests();
    return 0;
}
```

**Patterns:**
- **Setup:** Constructor initializes test fixtures (canvas, counters)
- **Teardown:** Automatic (RAII pattern with destructors)
- **Assertion pattern:** Manual printf with checkmark (✓) or cross (✗)
- **Test grouping:** Related tests grouped in test class methods

**Example from `basic_drawable_test.cpp`:**
```cpp
int main() {
    printf("Basic Drawable Components Test - Enjin2\n");
    printf("======================================\n");

    Canvas8<128, 64> canvas;
    canvas.clear(0);

    // Test 1: C_Draw component with simple lambda
    printf("Testing C_Draw component...\n");
    Object draw_obj;
    auto draw_pos = draw_obj.addComponent<C_Position>(10, 10);
    auto draw_comp = draw_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
        // Draw logic
    });

    draw_comp->draw(canvas);
    printf("✓ C_Draw component working\n");

    return 0;
}
```

## Mocking

**Framework:** None

**Patterns:**
- **Manual mock objects:** Create test-specific canvas instances for isolated testing
- **Lambda-based components:** Use `C_Draw` with lambda to create custom test renderers
- **File interface mocking:** In `hardware_integration_test.cpp`:
```cpp
class TestFile : public FileInterface {
private:
    const uint8_t* data;
    size_t file_size;
    size_t pos;
    bool open_flag;
public:
    TestFile(const uint8_t* d, size_t s) : data(d), file_size(s), pos(0), open_flag(false) {}
    bool open() override { pos = 0; open_flag = true; return true; }
    void close() override { open_flag = false; }
    size_t read(uint8_t* buffer, size_t length) override { /* ... */ }
    bool seek(size_t position) override { /* ... */ }
    size_t size() const override { return file_size; }
};
```

**What to Mock:**
- File I/O operations for testing without filesystem
- Hardware interfaces (display, input) using canvas simulation
- Platform-specific code (Arduino Serial, ESP32 components)

**What NOT to Mock:**
- Core component logic (test real implementations)
- Graphics primitives (test actual rendering)
- Canvas operations (test actual pixel manipulation)

## Fixtures and Factories

**Test Data:**
- **Inline test patterns:** Hard-coded pixel arrays for test sprites
- **Procedural generation:** Create test data in test methods
- **Canvas as fixture:** Canvas8/Canvas4 instances created per test

**Example from `simple_graphics_test.cpp`:**
```cpp
void createOrbitalFrame(Canvas4<128, 64>& canvas, float time) {
    canvas.clear(Pixel4(2));

    // Add stars procedurally
    for (int i = 0; i < 15; ++i) {
        int x = (i * 23 + 17) % 128;
        int y = (i * 37 + 29) % 64;
        canvas.setPixel(x, y, Pixel4(4 + (i % 2)));
    }

    // Draw planet with glow
    // ...
}
```

**Location:**
- Test data defined inline in test functions
- No separate fixtures/ directory
- Test patterns created per-test basis

**No factory pattern detected:** Test objects created directly with constructors

## Coverage

**Requirements:** None enforced (no coverage target specified)

**View Coverage:**
```bash
# No coverage reporting configured
# Would require gcov/lcov setup in CMakeLists.txt
```

**Current state:** No coverage measurement or reporting

## Test Types

**Unit Tests:**
- Not present in traditional sense
- Test examples function more like integration tests
- Component testing happens through examples

**Integration Tests:**
- Primary test type in codebase
- **Hardware Integration Test (`hardware_integration_test.cpp`):** Tests multiple systems working together
  - Orbital visualization
  - Parameter display system
  - Spectral visualization
  - Component integration
  - PostFx performance
  - Memory allocation patterns
  - Frame rate performance

- **VCVRack Integration Test (`vcvrack_integration_test.cpp`):** Tests VCVRack plugin compatibility
  - Module display simulation
  - Threading compatibility
  - Memory stability
  - Canvas export
  - Parameter animation timing

**E2E Tests:**
- Not present
- No end-to-end user flow tests
- Tests are component-level and system-level

**Examples as Tests:**
- **Basic Drawing:** `basic_drawing/` directory
- **ECS Demo:** `ecs_demo/` directory
- **Lua Scripting:** `lua_scripting/` directory
- **ESP32 Example:** `esp32_idf_example/` directory

## Common Patterns

**Async Testing:**
- Not applicable (real-time loop simulation used instead)
- Pattern: Simulate multiple frames with for loops:
```cpp
const int numFrames = 60;
for (int frame = 0; frame < numFrames; ++frame) {
    // Update and draw frame
    float time = frame * timeStep;
    createFrame(canvas, time);
}
```

**Error Testing:**
- Pattern: Try-catch blocks for exception testing:
```cpp
try {
    ImageEntry entry = C_ImageCache::AddImage(test_file, 8, 8, 1);
    printf("✓ ImageCache allocation successful\n");
} catch (const ImageCacheException& e) {
    printf("❌ ImageCache error: %s\n", e.what());
}
```

**Performance Testing:**
- Pattern: Measure time with `std::chrono`:
```cpp
auto start = std::chrono::high_resolution_clock::now();
// Run code to benchmark
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
printf("✓ Operation completed in %ld microseconds\n", duration.count());
```

**Visual Testing:**
- Pattern: Export canvas to PGM for visual verification:
```cpp
canvas.exportToPGM("test_output.pgm");
printf("✓ Canvas exported to test_output.pgm\n");
```

**Component Creation Pattern:**
```cpp
Object test_obj;
auto position = test_obj.addComponent<C_Position>(10, 10);
auto drawable = test_obj.addComponent<C_Draw>([](ICanvas<uint8_t>& canvas) {
    // Custom drawing logic
});
drawable->draw(canvas);
```

## Testing Gaps

**Missing Test Framework:**
- No Catch2, GoogleTest, or similar framework
- No automated test discovery
- No test runners with reporting

**No Mocking Framework:**
- Manual mocking only
- No Google Mock or similar

**No Coverage Reporting:**
- No gcov/lcov integration
- Cannot measure test coverage

**No CI/CD Testing:**
- No GitHub Actions, Travis CI, or similar detected
- Tests must be run manually

**Limited Test Scope:**
- No unit tests for individual functions
- Tests focus on integration scenarios
- Edge cases not systematically tested

**No Regression Tests:**
- No automated regression testing
- No baseline comparisons

**Performance Testing:**
- Ad-hoc performance measurements in examples
- No systematic benchmark suite

---

*Testing analysis: 2026-01-29*
