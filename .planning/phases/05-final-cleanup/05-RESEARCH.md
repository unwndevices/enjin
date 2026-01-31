# Phase 5: Final Cleanup - Research

**Researched:** January 31, 2026
**Domain:** CMake build configuration cleanup and conditional compilation removal
**Confidence:** HIGH

## Summary

This phase completes the migration from enjin1 to enjin2 by removing all conditional build configuration that allowed enjin1 backend selection. The cleanup involves removing the `USE_ENJIN1` CMake option, `USE_ENJIN1_BACKEND` compile definitions, and all conditional compilation blocks that were part of the Strangler Fig pattern implementation.

The enjin1 backend was never actually integrated - all conditional blocks in the seam files contain `#error "enjin1 backend not yet integrated"` directives. This makes the cleanup straightforward: we simply remove the `#if USE_ENJIN1_BACKEND` blocks and keep only the enjin2 implementation.

**Primary recommendation:** Remove all conditional compilation and CMake options referencing enjin1, then clean build directories to remove stale cache entries.

## Standard Stack

The established tools for CMake cleanup:

### Core
| Tool/Library | Version | Purpose | Why Standard |
|-------------|----------|---------|--------------|
| CMake | 3.16+ | Build configuration system | Project minimum version, provides all necessary commands |
| cmake command | 3.16+ | Command-line interface for CMake operations | Standard interface for reconfiguring builds |

### Supporting
| Tool | Purpose | When to Use |
|------|---------|-------------|
| grep | Pattern searching for references | Finding all occurrences of enjin1 references |
| rm/rmdir | Build directory cleanup | Removing CMakeCache.txt and build artifacts |

**No installation required** - All tools are standard Unix utilities already present on the system.

## Architecture Patterns

### Pattern 1: Removing CMake Options
**What:** Delete `option()` commands and their conditional logic from CMakeLists.txt
**When to use:** When a feature is completely removed and no longer needs to be selectable
**Example:**
```cmake
# Source: CMakeLists.txt (lines 9-15) - TO BE REMOVED
# Backend selection option
option(USE_ENJIN1 "Use enjin1 legacy backend" OFF)

if(USE_ENJIN1)
    message(STATUS "Building with enjin1 backend")
else()
    message(STATUS "Building with enjin2 backend")
endif()
```

### Pattern 2: Removing Compile Definitions
**What:** Delete `target_compile_definitions()` commands that set conditional macros
**When to use:** When compile-time feature flags are no longer needed
**Example:**
```cmake
# Source: enjin2/CMakeLists.txt (lines 136-140) - TO BE REMOVED
# Backend compile definitions
if(USE_ENJIN1)
    target_compile_definitions(enjin2 INTERFACE USE_ENJIN1_BACKEND=1)
else()
    target_compile_definitions(enjin2 INTERFACE USE_ENJIN1_BACKEND=0)
endif()
```

### Pattern 3: Removing Conditional Compilation Blocks
**What:** Delete `#if MACRO` / `#else` / `#endif` blocks, keeping only the active branch
**When to use:** When compile-time feature selection is removed
**Example:**
```cpp
// Source: enjin2/include/enjin2/seams/scene_seam.hpp (lines 49-58)
// BEFORE (with conditional compilation):
void onCreate() override {
#if USE_ENJIN1_BACKEND
    #error "enjin1 backend not yet integrated"
    // When enjin1 is integrated:
    // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
    //     // Route to enjin1 implementation
    // }
#else
    // Scene creation handled by scene state machine
    // This is a no-op for the seam itself
#endif
}

// AFTER (cleanup - keep only enjin2 branch):
void onCreate() override {
    // Scene creation handled by scene state machine
    // This is a no-op for the seam itself
}
```

### Pattern 4: Build Directory Cleanup
**What:** Remove entire build directory to eliminate stale cache entries
**When to use:** After removing CMake options to clear cache variables
**Example:**
```bash
# Remove build directory entirely
rm -rf enjin2/build
rm -rf verify-off/build

# Reconfigure fresh
mkdir -p build
cd build
cmake ..
```

### Anti-Patterns to Avoid
- **Manual CMakeCache.txt editing:** Don't try to edit individual lines in CMakeCache.txt. Remove the entire build directory and reconfigure instead.
- **Leaving dead code:** Don't comment out code instead of deleting it. Remove the entire conditional block.
- **Partial cleanup:** Don't leave the `#if` or `#endif` after removing the conditional content. Delete the entire block structure.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Cleaning CMake cache | Manual editing of CMakeCache.txt | `rm -rf build` + `cmake ..` | CMakeCache.txt has interdependent entries; manual editing leads to corruption |
| Removing conditional blocks | Custom sed/awk scripts | Direct file editing | Conditional blocks span multiple lines; automated tools risk leaving partial blocks |
| Finding all references | Manual grep without verification | Multi-pattern grep with -rn | Single patterns miss variations (USE_ENJIN1, enjin1, enjin1_backend) |
| Verifying cleanup | Visual inspection | Automated grep verification | Human review misses subtle references in comments or disabled code |

**Key insight:** CMake caches are complex state machines. The only reliable way to remove an option is to delete the cache entirely and reconfigure from scratch.

## Common Pitfalls

### Pitfall 1: Incomplete Conditional Block Removal
**What goes wrong:** Removing only the `#if` line or only the `#else` branch, leaving broken preprocessor directives
**Why it happens:** Conditional blocks span multiple lines and nested structures can be confusing
**How to avoid:** Always delete from the `#if` line through the matching `#endif`, keeping only the active implementation
**Warning signs:** Compilation errors about "unterminated #if" or "unexpected #else"

### Pitfall 2: Stale Cache Entries
**What goes wrong:** CMakeCache.txt still contains `USE_ENJIN1:BOOL=OFF` after cleanup
**Why it happens:** CMake caches persist across configurations unless explicitly cleared
**How to avoid:** Always remove build directory after removing options
**Warning signs:** CMake messages about "ignoring cache entry" or warnings about undefined variables

### Pitfall 3: Leaving Deprecated Enum Members
**What goes wrong:** Backend enum classes with ENJIN1 members remain in the code
**Why it happens:** Enums are part of the public API of seam classes
**How to avoid:** Remove entire enum definition and update all references to remove type annotations
**Warning signs:** Unused enum values, compiler warnings about "unused private member"

### Pitfall 4: Orphaned Comments
**What goes wrong:** Comments like "deprecated after enjin1 deletion" remain after cleanup is complete
**Why it happens:** Comments don't cause compilation errors, so they're easily missed
**How to avoid:** Use grep to find all "enjin1" references including those in comments
**Warning signs:** Inconsistent documentation, confusing future maintainers

### Pitfall 5: Build Directory Not Regenerated
**What goes wrong:** Code compiles but includes stale object files or headers
**Why it happens:** CMake doesn't always detect when cache variables change
**How to avoid:** Always run `cmake ..` after removing options to regenerate build system
**Warning signs:** Unexpected compilation errors, symbols not found, or wrong code being executed

## Code Examples

Verified patterns from official sources:

### Removing a CMake Option
```cmake
# Source: CMakeLists.txt (root) - Lines to delete
option(USE_ENJIN1 "Use enjin1 legacy backend" OFF)

if(USE_ENJIN1)
    message(STATUS "Building with enjin1 backend")
else()
    message(STATUS "Building with enjin2 backend")
endif()
```
**Action:** Delete lines 9-15 entirely. No replacement needed.

### Removing Conditional Compile Definitions
```cmake
# Source: enjin2/CMakeLists.txt - Lines 135-140 to delete
# Backend compile definitions
if(USE_ENJIN1)
    target_compile_definitions(enjin2 INTERFACE USE_ENJIN1_BACKEND=1)
else()
    target_compile_definitions(enjin2 INTERFACE USE_ENJIN1_BACKEND=0)
endif()
```
**Action:** Delete lines 135-140 entirely. The enjin2 interface no longer needs this definition.

### Cleaning Up Conditional Compilation (Scene Seam)
```cpp
// Source: enjin2/include/enjin2/seams/scene_seam.hpp
// Pattern repeated 6 times (onCreate, onActivate, onDeactivate, onDestroy, onUpdate, onRender)

// BEFORE:
void onCreate() override {
#if USE_ENJIN1_BACKEND
    #error "enjin1 backend not yet integrated"
    // When enjin1 is integrated:
    // if (currentBackend == Backend::ENJIN1 && enjin1SM != nullptr) {
    //     // Route to enjin1 implementation
    // }
#else
    // Scene creation handled by scene state machine
    // This is a no-op for the seam itself
#endif
}

// AFTER:
void onCreate() override {
    // Scene creation handled by scene state machine
    // This is a no-op for the seam itself
}
```

### Cleaning Up Conditional Compilation (Component Seam)
```cpp
// Source: enjin2/include/enjin2/seams/component_seam.hpp
// Pattern repeated 7 times (awake, start, update, lateUpdate, onEnable, onDisable, getOwner)

// BEFORE:
void awake() override {
#if USE_ENJIN1_BACKEND
    #error "enjin1 backend not yet integrated"
    // When enjin1 is integrated:
    // if (impl == Implementation::LEGACY && legacyImpl != nullptr) {
    //     // Route to enjin1 implementation
    // }
#else
    if (impl == Implementation::NEW && newImpl != nullptr) {
        newImpl->awake();
    }
#endif
}

// AFTER:
void awake() override {
    if (impl == Implementation::NEW && newImpl != nullptr) {
        newImpl->awake();
    }
}
```

### Cleaning Up Test File
```cpp
// Source: enjin2/tests/shadow_mode_test.cpp

// BEFORE:
} else {
    // Default: check USE_ENJIN1_BACKEND compile-time macro
#if USE_ENJIN1_BACKEND
    output_file = "output-enjin1.bmp";
#else
    output_file = "output-enjin2.bmp";
#endif
}

// AFTER:
} else {
    // Default: use enjin2 output
    output_file = "output-enjin2.bmp";
}
```

### Build Directory Cleanup
```bash
# Source: Standard Unix shell commands
# Clean all build directories
rm -rf enjin2/build
rm -rf verify-off/build
rm -rf build

# Reconfigure
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Verify no enjin1 references in cache
grep "USE_ENJIN1" CMakeCache.txt
# Should return nothing (exit code 1)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Conditional compilation with #if | Direct implementation with no conditionals | Phase 5 (current) | Cleaner code, smaller binary, no unused branches |
| CMake option for backend selection | Single backend (enjin2 only) | Phase 5 (current) | Simpler build configuration, less user confusion |
| Seam classes with backend enums | Seam classes with single implementation | Phase 5 (current) | Smaller API surface, less complexity |

**Deprecated/outdated:**
- `USE_ENJIN1` CMake option: Backend selection no longer needed
- `USE_ENJIN1_BACKEND` compile definition: No compile-time backend switching
- `SceneSeam::Backend` enum: Runtime backend selection removed
- `ComponentSeam::Implementation` enum: Runtime implementation selection removed
- Strangler Fig seam pattern with multiple backends: Migration complete, strangler removed

## Open Questions

None - all cleanup requirements are well-defined and follow standard patterns.

## Sources

### Primary (HIGH confidence)
- CMake Documentation (4.2.3) - option command: https://cmake.org/cmake/help/latest/command/option.html
- CMake Documentation (4.2.3) - target_compile_definitions: https://cmake.org/cmake/help/latest/command/target_compile_definitions.html
- CMake Documentation (4.2.3) - if command: https://cmake.org/cmake/help/latest/command/if.html
- CMake Documentation (4.2.3) - unset command: https://cmake.org/cmake/help/latest/command/unset.html
- CMake Documentation (4.2.3) - cmake-variables: https://cmake.org/cmake/help/latest/manual/cmake-variables.7.html
- CMake Documentation (4.2.3) - cmake-language: https://cmake.org/cmake/help/latest/manual/cmake-language.7.html

### Secondary (MEDIUM confidence)
- Project codebase analysis (January 2026) - Direct examination of CMakeLists.txt files and source code
- Seam file analysis (January 2026) - Identified 13 conditional compilation blocks with `#error` directives

### Tertiary (LOW confidence)
- None - all findings verified against official CMake documentation or project code

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - CMake 3.16+ documented in official docs
- Architecture: HIGH - Patterns verified against official CMake documentation
- Pitfalls: HIGH - Common issues documented in CMake best practices and project code analysis

**Research date:** January 31, 2026
**Valid until:** 30 days (CMake 3.16+ stable, unlikely to change)
