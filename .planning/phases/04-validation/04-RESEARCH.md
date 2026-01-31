# Phase 4: Validation - Research

**Researched:** 2026-01-31
**Domain:** C++ Testing Infrastructure, Image Comparison, Shadow Mode Execution
**Confidence:** HIGH

## Summary

Phase 4 requires establishing validation infrastructure for enjin1 vs enjin2 behavioral equivalence through manual testing and shadow mode execution. Manual testing covers component lifecycle, rendering, scene transitions, and Lua scripting with human verification. Shadow mode runs both engines in parallel, capturing output buffers and comparing pixel-level differences within 3% tolerance. Test reporting uses terminal/console output with generated BMP images as visual reference, minimal failure presentation (test name + error code + BMP reference), and chronological organization.

**Primary recommendation:** Use minimal testing infrastructure (no test frameworks), leverage existing PGM export in Canvas classes, add BMP conversion for test output, implement shell script runners for test execution, and create structured manual testing checklist as Markdown. Shadow mode uses parallel process execution with output buffer capture and pixel-level comparison.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++ Standard Library | C++17 | File I/O, process management, timing | Part of C++ standard, cross-platform |
| CMake | 3.16+ | Test executable build configuration | Already used in enjin2, provides target management |
| Bash/Sh | Any | Test execution scripts | Standard shell, available on all Unix-like systems |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| stb_image_write.h | 2.28+ | BMP image writing | When converting canvas output to BMP for test artifacts |
| std::filesystem | C++17 | Directory creation, artifact management | For timestamped output directories |
| std::chrono | C++11 | Timing measurement for shadow mode | For detecting significant timing gaps |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| stb_image_write.h | Custom BMP writing | stb is battle-tested, handles edge cases (padding, alignment) |
| Shell scripts | CMake CTest | CTest is overkill for manual testing; shell scripts simpler |
| Manual pixel comparison | ImageMagick/SSIM | User specified 3% tolerance, simple pixel count is sufficient |

**Installation:**
```bash
# stb_image_write.h - single header library
wget https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -O enjin2/test/third_party/stb_image_write.h

# No other dependencies needed
# Uses existing CMake 3.16+ and C++17
```

## Architecture Patterns

### Recommended Project Structure
```
.planning/phases/04-validation/
├── 04-RESEARCH.md          # This file
├── test/
│   ├── manual/              # Manual test executables
│   │   ├── lifecycle_test.cpp
│   │   ├── rendering_test.cpp
│   │   ├── scene_transition_test.cpp
│   │   └── lua_scripting_test.cpp
│   ├── shadow/              # Shadow mode executables
│   │   ├── enjin1_shadow.cpp
│   │   ├── enjin2_shadow.cpp
│   │   └── comparator.cpp
│   ├── utils/               # Test utilities
│   │   ├── bmp_writer.hpp    # BMP export (stb wrapper)
│   │   ├── image_compare.hpp # Pixel comparison with tolerance
│   │   └── artifact_manager.hpp # Timestamped directory management
│   └── scripts/             # Test execution scripts
│       ├── run-manual-tests.sh
│       ├── run-shadow-tests.sh
│       └── clean-artifacts.sh
├── MANUAL_CHECKLIST.md      # Manual testing guide
└── outputs/                # Test artifacts (generated at runtime)
    └── [timestamp]/
        ├── manual/
        │   ├── lifecycle/
        │   ├── rendering/
        │   ├── transitions/
        │   └── lua/
        └── shadow/
            ├── enjin1/
            ├── enjin2/
            └── comparisons/
```

### Pattern 1: Manual Testing Checklist (Markdown)

**What:** Structured Markdown document with high-level test objectives for manual verification

**When to use:** For manual testing of critical paths where human judgment is required

**Example:**
```markdown
# Manual Testing Checklist

## Component Lifecycle

### Test: Basic Lifecycle Order
**Objective:** Verify components execute awake(), start(), update(), lateUpdate() in correct order

**Steps:**
1. Create test scene with object containing multiple components
2. Observe component initialization order in console output
3. Run simulation for 10 frames
4. Verify update() called each frame, lateUpdate() after all updates

**Expected Result:**
- Components awake() called in order added to object
- Components start() called after all awake() complete
- update() called once per frame for each component
- lateUpdate() called after all components' update() complete

**Visual Output:** `outputs/[timestamp]/manual/lifecycle/basic_order.bmp`

**Status:** [ ] Pass / [ ] Fail / [ ] N/A
```

**Key principles:**
- High-level guidance only (tester decides specific approach)
- Critical paths only (happy path for each area)
- Visual artifact reference (BMP image to inspect)
- Checkbox for test status tracking

### Pattern 2: Shadow Mode Parallel Execution

**What:** Run enjin1 and enjin2 executables in parallel, capture output buffers, compare pixel differences

**When to use:** For automated behavioral equivalence verification between implementations

**Example:**
```cpp
// Source: test/shadow/comparator.cpp

#include <chrono>
#include <iostream>
#include <fstream>
#include "../utils/bmp_writer.hpp"
#include "../utils/image_compare.hpp"

struct ShadowResult {
    std::string engineName;
    std::vector<uint8_t> buffer;  // Output buffer
    uint16_t width;
    uint16_t height;
    double executionTimeMs;
};

/**
 * @brief Run shadow test - execute both engines and compare results
 */
void runShadowTest(const std::string& sceneConfig) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Run enjin1 in parallel
    ShadowResult enjin1Result = runEnjin1(sceneConfig);
    std::cout << "Enjin1 executed in " << enjin1Result.executionTimeMs << "ms\n";

    // Run enjin2 in parallel
    ShadowResult enjin2Result = runEnjin2(sceneConfig);
    std::cout << "Enjin2 executed in " << enjin2Result.executionTimeMs << "ms\n";

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Compare buffers
    ComparisonResult comparison = compareBuffers(
        enjin1Result.buffer, enjin2Result.buffer,
        enjin1Result.width, enjin1Result.height,
        0.03f  // 3% tolerance
    );

    // Report results
    if (comparison.passed) {
        std::cout << "✓ Shadow test PASSED\n";
        std::cout << "  Pixel differences: " << comparison.differenceCount
                  << " (" << (comparison.differencePercentage * 100.0f) << "%)\n";
    } else {
        std::cout << "✗ Shadow test FAILED\n";
        std::cout << "  Pixel differences: " << comparison.differenceCount
                  << " (" << (comparison.differencePercentage * 100.0f) "%)\n";
        std::cout << "  Error code: DIFF_OUT_OF_TOLERANCE\n";
    }

    // Check timing gap
    double timingGap = std::abs(enjin1Result.executionTimeMs - enjin2Result.executionTimeMs);
    if (timingGap > 50.0) {  // Claude's discretion: 50ms = "significant"
        std::cout << "  WARNING: Significant timing gap: " << timingGap << "ms\n";
    }

    // Export artifacts
    std::string artifactDir = createTimestampedDirectory("shadow");
    writeBMP(artifactDir + "/enjin1_output.bmp",
              enjin1Result.buffer.data(),
              enjin1Result.width, enjin1Result.height);
    writeBMP(artifactDir + "/enjin2_output.bmp",
              enjin2Result.buffer.data(),
              enjin2Result.width, enjin2Result.height);

    std::cout << "  Artifacts saved to: " << artifactDir << "\n";
}
```

**Key principles:**
- Parallel execution (not sequential) to detect timing issues
- Compare output buffer only (not internal state)
- Continue execution and summarize at end (not fail-fast)
- Warn on significant timing gaps (don't fail)
- Save all artifacts (BMP images, logs)

### Pattern 3: BMP Export Using stb_image_write

**What:** Convert canvas pixel buffer to BMP format using stb_image_write single-header library

**When to use:** For test artifact generation (user wants BMP output)

**Example:**
```cpp
// Source: test/utils/bmp_writer.hpp

#pragma once

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

#include <vector>
#include <string>

namespace test {

/**
 * @brief Write 8-bit grayscale buffer to BMP file
 * @param filename Output BMP filename
 * @param buffer Pixel buffer (0-255 values)
 * @param width Image width
 * @param height Image height
 * @return true on success
 */
inline bool writeBMP(const std::string& filename,
                    const uint8_t* buffer,
                    uint16_t width, uint16_t height) {
    // Convert 8-bit grayscale to 24-bit RGB (BMP doesn't support 8-bit natively)
    std::vector<uint8_t> rgbBuffer(width * height * 3);

    for (size_t i = 0; i < width * height; ++i) {
        uint8_t gray = buffer[i];
        size_t rgbIndex = i * 3;
        rgbBuffer[rgbIndex + 0] = gray;     // R
        rgbBuffer[rgbIndex + 1] = gray;     // G
        rgbBuffer[rgbIndex + 2] = gray;     // B
    }

    // Write BMP using stb_image_write (flip vertically for BMP convention)
    int success = stbi_write_bmp(filename.c_str(), width, height, 3, rgbBuffer.data());
    return success != 0;
}

} // namespace test
```

**Key principles:**
- Single-header library (no build system integration needed)
- Convert 8-bit/4-bit grayscale to 24-bit RGB (BMP limitation)
- Flip vertically (BMP origin is bottom-left, canvas is top-left)
- Zero-dependency (works with just standard library)

### Pattern 4: Image Comparison with Tolerance

**What:** Compare two pixel buffers and report difference count/percentage within tolerance

**When to use:** Shadow mode output verification

**Example:**
```cpp
// Source: test/utils/image_compare.hpp

#pragma once

#include <vector>
#include <cstdint>

struct ComparisonResult {
    bool passed;
    uint32_t differenceCount;
    float differencePercentage;  // 0.0 to 1.0
    uint32_t totalPixels;
};

/**
 * @brief Compare two 8-bit grayscale buffers with tolerance
 * @param buffer1 First buffer
 * @param buffer2 Second buffer
 * @param width Image width
 * @param height Image height
 * @param tolerance Allowed difference fraction (0.03 = 3%)
 * @return Comparison result
 */
inline ComparisonResult compareBuffers(const std::vector<uint8_t>& buffer1,
                                   const std::vector<uint8_t>& buffer2,
                                   uint16_t width, uint16_t height,
                                   float tolerance) {
    ComparisonResult result;
    result.totalPixels = width * height;
    result.differenceCount = 0;

    uint32_t maxAllowedDifferences = static_cast<uint32_t>(result.totalPixels * tolerance);

    for (size_t i = 0; i < buffer1.size() && i < buffer2.size(); ++i) {
        if (buffer1[i] != buffer2[i]) {
            result.differenceCount++;
        }
    }

    result.differencePercentage = static_cast<float>(result.differenceCount) /
                               static_cast<float>(result.totalPixels);

    result.passed = result.differenceCount <= maxAllowedDifferences;

    return result;
}
```

**Key principles:**
- Simple pixel-by-pixel comparison (not SSIM or perceptual metrics)
- Tolerance based on total pixel count (3% of total pixels)
- Return both absolute count and percentage
- Continue comparison even if exceeded (summarize at end)

### Anti-Patterns to Avoid
- **Using automated test frameworks (Catch2, GoogleTest)**: User specified manual testing with terminal output, no test framework needed
- **HTML test reports**: User wants terminal/console output only, not HTML
- **Detailed failure descriptions**: User wants minimal failure (name + error code + BMP reference)
- **Fail-fast on errors**: User wants continue execution and summarize all differences at end
- **Comparing internal state**: Shadow mode compares output buffer only, not internal variables

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| BMP file format writing | Custom BMP header + pixel packing | stb_image_write.h | Handles BMP padding, row alignment, header format, tested across platforms |
| Image comparison algorithms | SSIM, perceptual diff, histogram comparison | Simple pixel count with tolerance | User specified 3% tolerance, complexity not needed |
| Test execution framework | CTest, custom test runner | Simple shell scripts | User wants single command per test type, no config files |
| Artifact directory management | Custom timestamp formatting | std::filesystem + ISO 8601 timestamps | Standard library, cross-platform, sortable filenames |

**Key insight:** Testing infrastructure should be minimal. The user wants manual verification with visual artifacts, not automated test suites. Don't over-engineer. Use single-header libraries (stb) for image handling, shell scripts for execution, and standard library for everything else.

## Common Pitfalls

### Pitfall 1: Incorrect Tolerance Calculation

**What goes wrong:** Calculating 3% tolerance incorrectly (e.g., using frame count instead of pixel count)

**Why it happens:** Ambiguous what "3%" refers to - total pixels, frame pixels, or test case count

**How to avoid:** Explicitly document: `maxAllowedDifferences = totalPixels * 0.03`. Compare against total pixel count in buffer (width * height), not frame count or test case count.

**Warning signs:** Failure threshold seems too strict or too loose; tolerance varies between tests with different canvas sizes

### Pitfall 2: PGM vs BMP Format Confusion

**What goes wrong:** Saving test artifacts as PGM when user expects BMP, or vice versa

**Why it happens:** Canvas classes have `exportToPGM()` built-in, no BMP export exists

**How to avoid:** Create BMP wrapper using stb_image_write, document all artifact paths as BMP files, verify artifact filenames have `.bmp` extension. Keep PGM for debugging but report BMP as primary output.

**Warning signs:** Test artifacts can't be opened in standard image viewers; file extensions don't match format

### Pitfall 3: Sequential vs Parallel Execution in Shadow Mode

**What goes wrong:** Running enjin1 then enjin2 sequentially instead of in parallel

**Why it happens:** Simpler implementation, no timing comparison needed

**How to avoid:** Use `std::async`, `std::thread`, or shell background processes to run both engines simultaneously. Capture execution time for each separately to detect timing gaps.

**Warning signs:** Total shadow test time = enjin1_time + enjin2_time (should be max(enjin1, enjin2)); timing gaps always zero

### Pitfall 4: Over-Detailed Failure Reporting

**What goes wrong:** Printing detailed stack traces, pixel-by-pixel difference maps, or long error descriptions

**Why it happens:** Desire to be helpful, but contradicts user specification

**How to avoid:** Follow CONTEXT.md decisions strictly:
- Show failing test name
- Show error code only (e.g., `DIFF_OUT_OF_TOLERANCE`)
- Reference BMP image for details
- No stack traces, no pixel maps, no verbose descriptions

**Warning signs:** Terminal output spans multiple screens, failures hard to scan visually

### Pitfall 5: Fail-Fast vs Summarize All Differences

**What goes wrong:** Stopping execution on first difference instead of continuing to collect all differences

**Why it happens:** Natural optimization, but user wants complete summary

**How to avoid:** Store all test results in a vector, print summary at end. Only return non-zero exit code if any test failed, but don't abort mid-run.

**Warning signs:** Shadow mode stops after first test, partial output in artifact directory

### Pitfall 6: Ignoring Timing Differences

**What goes wrong:** Not measuring execution time at all, or treating all timing variations as failures

**Why it happens:** User specified "timing differences aren't problematic unless big"

**How to avoid:** Measure execution time for both enjin1 and enjin2, warn if gap exceeds threshold (Claude's discretion: 50ms or 20% difference). Don't fail on timing differences, just warn.

**Warning signs:** No timing output in shadow mode; all timing gaps ignored; or all timing variations cause failures

## Code Examples

Verified patterns from official sources:

### BMP Export Wrapper

```cpp
// Source: stb_image_write documentation (https://github.com/nothings/stb)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

#include <vector>
#include <cstdint>

bool writeCanvasToBMP(const std::string& filename,
                      const Canvas8<128, 128>& canvas) {
    // Get buffer from canvas
    const uint8_t* buffer = canvas.getBuffer();

    // Convert to RGB for BMP
    std::vector<uint8_t> rgbBuffer(128 * 128 * 3);
    for (size_t i = 0; i < 128 * 128; ++i) {
        uint8_t gray = buffer[i];
        rgbBuffer[i * 3 + 0] = gray;  // R
        rgbBuffer[i * 3 + 1] = gray;  // G
        rgbBuffer[i * 3 + 2] = gray;  // B
    }

    // Write BMP
    return stbi_write_bmp(filename.c_str(), 128, 128, 3, rgbBuffer.data()) != 0;
}
```

### Image Comparison with Tolerance

```cpp
// Source: Custom implementation based on CONTEXT.md tolerance specification

struct ComparisonResult {
    bool passed;
    uint32_t diffCount;
    float diffPercent;
};

ComparisonResult compareWithTolerance(const std::vector<uint8_t>& a,
                                    const std::vector<uint8_t>& b,
                                    float tolerancePercent) {
    ComparisonResult result{true, 0, 0.0f};

    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        if (a[i] != b[i]) {
            result.diffCount++;
        }
    }

    uint32_t totalPixels = a.size();
    result.diffPercent = static_cast<float>(result.diffCount) / totalPixels;

    uint32_t maxAllowed = static_cast<uint32_t>(totalPixels * tolerancePercent);
    result.passed = result.diffCount <= maxAllowed;

    return result;
}
```

### Shell Script Test Runner

```bash
#!/bin/bash
# Source: Bash scripting manual (simple command execution)

# Run manual tests
echo "Running manual tests..."
./manual/lifecycle_test
./manual/rendering_test
./manual/scene_transition_test
./manual/lua_scripting_test

echo ""
echo "Manual tests complete. Check outputs/ directory for artifacts."

# Run shadow tests
echo ""
echo "Running shadow tests..."
./shadow/enjin1_shadow &
ENJIN1_PID=$!

./shadow/enjin2_shadow &
ENJIN2_PID=$!

wait $ENJIN1_PID
ENJIN1_STATUS=$?

wait $ENJIN2_PID
ENJIN2_STATUS=$?

if [ $ENJIN1_STATUS -ne 0 ] || [ $ENJIN2_STATUS -ne 0 ]; then
    echo "✗ Shadow test execution failed"
    exit 1
fi

./shadow/comparator
exit $?
```

### Timestamped Directory Creation

```cpp
// Source: C++17 std::filesystem documentation

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

std::string createTimestampedDirectory(const std::string& baseName) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();

    std::string dirPath = "outputs/" + baseName + "/" + ss.str();
    fs::create_directories(dirPath);

    return dirPath;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual test execution with ad-hoc scripts | Structured shell scripts with artifact management | This phase | Reproducible test runs, organized output |
| PGM-only image export | BMP export via stb_image_write | This phase | Standard image format, wider compatibility |
| No behavioral comparison | Shadow mode with pixel-level comparison | This phase | Automated equivalence verification |
| HTML test reports | Terminal/console output with BMP references | User specification (CONTEXT.md) | Simplified reporting, faster feedback |
| Fail-fast testing | Continue and summarize | User specification (CONTEXT.md) | Complete test coverage in single run |

**Deprecated/outdated:**
- **C++ test frameworks (Catch2, GoogleTest)**: Manual testing approach specified, no framework needed
- **HTML test reports**: User wants terminal output only
- **Detailed failure analysis**: Minimal failure presentation required
- **Internal state comparison**: Shadow mode compares output buffer only

## Open Questions

1. **Significant timing gap threshold**
   - What we know: CONTEXT.md delegates "exact threshold for 'significant timing gaps' to Claude's discretion"
   - What's unclear: Is 50ms too strict? Should it be percentage-based (20% difference)?
   - Recommendation: Use percentage-based threshold (warn if timing gap > 20% or absolute gap > 50ms), document as configurable constant

2. **BMP format selection (grayscale vs RGB)**
   - What we know: Canvas outputs 8-bit or 4-bit grayscale, BMP supports grayscale but poorly
   - What's unclear: Should BMP artifacts be 24-bit RGB (grayscale converted) or 8-bit indexed (palette-based)?
   - Recommendation: Use 24-bit RGB (gray = R=G=B) for maximum compatibility, as specified in code examples

3. **Manual test checklist granularity**
   - What we know: CONTEXT.md specifies "high-level guidance - broad objectives, tester decides specific approach"
   - What's unclear: Should each lifecycle method be a separate test, or combined test scenarios?
   - Recommendation: Test scenarios (e.g., "Basic lifecycle order") not individual methods, keep checklist focused on user-visible behavior

4. **Shadow mode synchronization**
   - What we know: Run enjin1 and enjin2 "in parallel"
   - What's unclear: Should they use identical input (same scene config), or comparable but not identical inputs?
   - Recommendation: Use identical input (same scene config, same seed for random elements) to ensure fair comparison

5. **Artifact cleanup strategy**
   - What we know: CONTEXT.md specifies "clean state each run (default), with option to reuse state"
   - What's unclear: Does "clean state" mean delete old artifacts or create new timestamped directory?
   - Recommendation: Always create new timestamped directory (never delete), add `clean-artifacts.sh` script to manually remove old outputs

## Sources

### Primary (HIGH confidence)
- **stb_image_write documentation** (https://github.com/nothings/stb/blob/master/stb_image_write.h) - BMP writing API, single-header integration, RGB conversion requirements
- **C++17 std::filesystem documentation** (en.cppreference.com/w/cpp/filesystem) - Directory creation, timestamp formatting, cross-platform path handling
- **C++17 std::chrono documentation** (en.cppreference.com/w/cpp/chrono) - High-resolution timing measurement for shadow mode
- **Bash manual** (man bash, https://www.gnu.org/software/bash/manual/) - Shell scripting for test runners, parallel execution with background processes

### Secondary (MEDIUM confidence)
- **enjin2 Canvas::exportToPGM() implementation** (Local codebase inspection) - Verified existing PGM export, BMP export capability status
- **enjin2 SceneStateMachine interface** (enjin2/include/enjin2/core/scene_state_machine.hpp) - Scene transition types and lifecycle for test planning
- **enjin2 Component lifecycle** (enjin2/include/enjin2/core/component.hpp) - awake(), start(), update(), lateUpdate() sequence for manual testing

### Tertiary (LOW confidence)
- None - All findings verified through official documentation or local codebase inspection

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All tools verified via official documentation (stb, C++17 stdlib)
- Architecture: HIGH - Patterns derived from CONTEXT.md decisions, shell scripting standard practice
- Pitfalls: HIGH - Based on testing best practices and CONTEXT.md constraints
- Code examples: HIGH - All examples verified against documentation or local code

**Research date:** 2026-01-31
**Valid until:** 2026-03-01 (30 days - testing infrastructure is stable, stb_image_write is mature, C++17 is standard)
