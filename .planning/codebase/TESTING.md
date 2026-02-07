# Testing Patterns

**Analysis Date:** 2025-02-12

## Test Framework

**Runner:**
- CTest (enabled via `enable_testing()` in `CMakeLists.txt`)

**Assertion Library:**
- Manual status checks and exit codes

**Run Commands:**
```bash
# Build and run tests
mkdir build && cd build
cmake .. -DENJIN2_BUILD_TESTS=ON
make
ctest
```

## Test File Organization

**Location:**
- Dedicated `tests/` directory for automated tests
- `examples/` directory for integration/visual demonstrations

**Key Test Files:**
- `tests/image_comparison.cpp`: Utility for comparing BMP output with a tolerance
- `tests/shadow_mode_test.cpp`: Comparative test for different backends

## Test Structure

**Automated Tests:**
- Tests typically create a `Scene` and `Canvas`, run for a fixed number of frames, and export results
- `image_comparison_test` uses `stb_image.h` to compare pixel data:
```cpp
const float TOLERANCE = 3.0f;
if (diffPercent <= TOLERANCE) {
    return 0; // PASS
} else {
    return 1; // FAIL
}
```

**Manual/Visual Tests:**
- Many examples act as visual tests (e.g., `basic_drawable_test.cpp`)
- Use `printf` to report step-by-step progress

## Mocking

**Framework:**
- Manual mocks/fakes

**Patterns:**
- Mocking hardware-specific interfaces (like `FileInterface`) for unit testing without a real filesystem

## Test Types

**Integration Tests:**
- Scene and component lifecycle tests
- Rendering verification via BMP export and comparison

**Benchmarks:**
- Performance analysis in `examples/performance_optimization_analysis.cpp`

---

*Testing analysis: 2025-02-12*
