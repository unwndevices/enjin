# Codebase Concerns

**Analysis Date:** 2026-01-29

## Tech Debt

**Test Coverage Gap:**
- Issue: No functional tests exist - only placeholder in `enjin2/tests/CMakeLists.txt` stating "Placeholder for tests - will be implemented later"
- Files: `enjin2/tests/CMakeLists.txt`
- Impact: No verification of correctness, refactoring is risky, regression bugs go undetected
- Fix approach: Implement unit tests for core systems (canvas, primitives, components), integration tests for Lua bindings, E2E tests for drawing operations

**Parallel Codebases:**
- Issue: Two parallel versions exist (`enjin/` and `enjin2/`) with overlapping functionality but different implementations
- Files: `enjin/` (12,488 lines), `enjin2/` (3,492 lines)
- Impact: Maintenance burden, duplicate effort, unclear which version to use, feature drift
- Fix approach: Consolidate to single codebase, decide on migration path from v1 to v2, deprecate v1

**Hardcoded Asset Data:**
- Issue: `enjin/UI/assets/resources.cpp` contains 1,330 lines of raw pixel array data instead of loading from files
- Files: `enjin/UI/assets/resources.cpp` (1,330 lines)
- Impact: Large binary size, difficult to modify assets, compile time overhead
- Fix approach: Extract to binary resource files, implement asset loader, convert to build-time resource embedding

**Simple Memory Allocator:**
- Issue: First-fit allocator in `C_ImageCache` with no defragmentation, potential fragmentation over time
- Files: `enjin2/src/components/image_cache.cpp` (140-165)
- Impact: Memory waste, allocation failures after extended use, performance degradation
- Fix approach: Implement best-fit allocator with coalescing, add defragmentation routine, use arena allocator for temporary allocations

## Known Bugs

**Hardcoded Canvas Dimensions:**
- Issue: Post-processing effects assume 128x128 canvas size hardcoded throughout
- Symptoms: Effects break with different canvas dimensions
- Files: `enjin2/src/effects/postfx.cpp` (lines 44-56, 74-99, 109-161, etc.)
- Trigger: Using canvas sizes other than 128x128 with post-fx effects
- Workaround: None - effects fail on non-standard sizes

**Missing Canvas Dependency:**
- Issue: `C_Canvas` component logs warning but continues execution when `C_Position` component is missing
- Symptoms: Drawing may not work correctly, undefined behavior
- Files: `enjin2/src/components/canvas.cpp` (line 38)
- Trigger: Adding `C_Canvas` without `C_Position` to an object
- Workaround: Always add `C_Position` before `C_Canvas`

**TODO Comments:**
- Issue: Uncumbersome animation approach noted but not addressed
- Files: `enjin/Animation.cpp` (line 33), `enjin/Components/C_Probe.cpp` (line 53)
- Impact: Technical debt marker, potential performance issue
- Workaround: None - functional code with noted complexity

## Security Considerations

**Manual Memory Management:**
- Risk: Raw `new[]`/`delete[]` usage with potential for leaks, double-free, or use-after-free
- Files: `enjin2/src/components/image_cache.cpp` (lines 55, 60, 77), `enjin2/src/scripting/lua_platform.cpp` (heap_caps_malloc/free)
- Current mitigation: Exception handling in image cache for cleanup
- Recommendations: Use RAII with smart pointers (std::unique_ptr, std::vector), consider arena allocators for temporary buffers, add memory leak detection (valgrind/ASan)

**Static Mutable Global State:**
- Risk: Global `g_currentBindings` pointer could lead to race conditions in multi-threaded environments
- Files: `enjin2/src/scripting/bindings.cpp` (line 8)
- Current mitigation: No threading detected in codebase
- Recommendations: Remove global state, pass bindings instance through Lua registry or upvalues, document single-threaded requirement

**No Input Validation:**
- Risk: Canvas drawing functions don't validate bounds before pixel access
- Files: `enjin2/src/scripting/bindings.cpp` (lua_setPixel, lua_getPixel), `enjin2/src/effects/postfx.cpp`
- Current mitigation: Not implemented
- Recommendations: Add bounds checking for all canvas operations, clamp coordinates to valid range, return errors or log warnings for out-of-bounds access

**Unbounded Allocations:**
- Risk: `C_ImageCache::AddImage` doesn't enforce maximum image count, could exhaust cache
- Files: `enjin2/src/components/image_cache.cpp`
- Current mitigation: Cache size limit (32KB) prevents complete exhaustion
- Recommendations: Limit maximum entries, implement LRU eviction policy, add usage tracking

## Performance Bottlenecks

**Pixel-by-Pixel Processing:**
- Problem: Post-processing effects iterate entire 128x128 canvas pixel-by-pixel (16,384 iterations per effect)
- Files: `enjin2/src/effects/postfx.cpp` (all apply* methods)
- Cause: Naïve implementation without vectorization or SIMD
- Improvement path: Use SIMD intrinsics for batch operations, implement scanline processing, cache intermediate results, use fixed-point arithmetic where possible

**First-Fit Memory Fragmentation:**
- Problem: `C_ImageCache::FindFreeSpace` linear scan with no coalescing, O(n) for each allocation
- Files: `enjin2/src/components/image_cache.cpp` (lines 140-165)
- Cause: Simple first-fit algorithm without optimization
- Improvement path: Implement buddy allocator or bitmap allocator, maintain free list, coalesce adjacent free blocks, use best-fit with size buckets

**No Caching or Memoization:**
- Problem: Canvas operations recalculate values each call (e.g., `getPixel` boundary checks)
- Files: Throughout canvas implementations
- Cause: Stateless design
- Improvement path: Cache frequently accessed data, use dirty flags for changed regions, batch operations where possible

**Double Buffer Copy:**
- Problem: Barrel distortion and blur allocate 16KB temporary buffer on stack
- Files: `enjin2/src/effects/postfx.cpp` (lines 109, 194, 239)
- Cause: Need source buffer for non-destructive reads
- Improvement path: Use ping-pong buffers, reuse allocated buffers, implement in-place variants where possible

## Fragile Areas

**Platform-Specific Fragmentation:**
- Files: `enjin2/CMakeLists.txt`, `enjin2/include/enjin2/graphics/canvas_esp32s3.hpp`, `enjin2/include/enjin2/scripting/lua_platform.hpp`
- Why fragile: Extensive `#ifdef VCV_RACK`, `#ifdef ESP32`, `#ifdef EMSCRIPTEN` branching throughout, different code paths for each platform
- Safe modification: Always test on all target platforms, add CI for each platform, document platform-specific requirements
- Test coverage: No platform-specific tests, gap in verification for ESP32 and WebAssembly builds

**Complex Build System:**
- Files: `enjin2/CMakeLists.txt` (183 lines), multiple example CMakeLists.txt files
- Why fragile: Multiple conditional build options (ENJIN2_BUILD_TESTS, ENJIN2_BUILD_EXAMPLES, ENJIN2_BUILD_LUA, ENJIN2_BUILD_WASM), embedded LuaJIT compilation, external Adafruit-GFX dependency
- Safe modification: Test all build configurations, verify dependency paths are correct, document external requirements
- Test coverage: No build verification tests

**Canvas Template Specializations:**
- Files: `enjin2/include/enjin2/graphics/canvas.hpp`, `enjin2/include/enjin2/graphics/canvas_esp32s3.hpp`
- Why fragile: Multiple template instantiations (ICanvas<Pixel4>, ICanvas<uint8_t>), ESP32-S3 optimized variants, hard-coded buffer sizes
- Safe modification: Maintain API consistency across specializations, document supported canvas sizes
- Test coverage: No cross-platform canvas tests

**Large Function Complexity:**
- Files: `enjin2/src/scripting/bindings.cpp` (605 lines), `enjin2/src/effects/postfx.cpp` (413 lines)
- Why fragile: Many similar functions, high cyclomatic complexity, difficult to test individual functions
- Safe modification: Refactor into smaller, focused functions, add unit tests, maintain function behavior
- Test coverage: No tests for binding functions or post-fx effects

**Lua State Management:**
- Files: `enjin2/src/scripting/lua_engine.cpp`, `enjin2/src/scripting/lua_platform.cpp`
- Why fragile: Global Lua state, platform-specific memory management, custom allocator for ESP32
- Safe modification: Verify Lua state cleanup, test memory usage over time, check for Lua stack balance
- Test coverage: No Lua integration tests

## Scaling Limits

**Image Cache Capacity:**
- Current capacity: 32KB static buffer
- Limit: Fixed CACHE_SIZE, no runtime allocation
- Scaling path: Make cache size configurable, implement dynamic allocation, add compression for cached images

**Entity System:**
- Current capacity: MAX_ENTITIES (file doesn't specify value)
- Limit: Fixed-size arrays in EntityManager
- Scaling path: Use dynamic arrays or pools, implement sparse entity IDs, add entity pooling

**Lua Memory:**
- Current capacity: 64KB for ESP32, unlimited for VCV_RACK
- Limit: ESP32 platform severely constrained
- Scaling path: Implement Lua GC tuning, add memory pressure callbacks, optimize Lua bindings for reduced overhead

## Dependencies at Risk

**Embedded LuaJIT:**
- Risk: Custom LuaJIT amalgamation in `enjin2/luajit/src/ljamalg.c` - diverges from upstream
- Impact: Security patches not applied, bug fixes missed, compatibility issues with standard LuaJIT
- Migration plan: Use system LuaJIT where available, track upstream changes, consider migration to standard Lua 5.4

**Adafruit-GFX External Dependency:**
- Risk: Path hardcoded to `../Libs/Adafruit-GFX-Library` in CMakeLists.txt
- Impact: Build fails if library not present at expected location
- Migration plan: Use FetchContent or find_package in CMake, document dependency requirement, add fallback implementation

**Platform-Specific SDKs:**
- Risk: ESP-IDF, Emscripten, and VCV-Rack SDKs have different versions and requirements
- Impact: Build failures when SDK versions mismatch
- Migration plan: Document supported SDK versions, add CI for each platform, use feature detection over version checks

## Missing Critical Features

**No Asset Loading:**
- Problem: Images are hard-coded in source files, no file loading API
- Blocks: Dynamic content, user customization, easy theme changes
- Fix approach: Implement FileInterface API, add image format parsers (PNG, raw bitmap), create asset bundle system

**No Font Rendering:**
- Problem: No text rendering engine, only glcdfont header included
- Blocks: UI text, labels, localized content
- Fix approach: Integrate glcdfont, implement text layout engine, add font loading from external files

**No Input System:**
- Problem: No button, touch, or input handling components
- Blocks: Interactive UI, user controls, game input
- Fix approach: Define input interface, implement button handlers, add touch detection for touchscreens

**No Audio System:**
- Problem: No audio playback or generation
- Blocks: Sound effects, music, audio feedback
- Fix approach: Add audio component interface, implement simple waveform generation, integrate with platform audio APIs

**No Scene Management:**
- Problem: Scene file exists but is minimal (72 lines), no scene graph
- Blocks: Complex scenes, object relationships, spatial queries
- Fix approach: Implement scene graph, add component relationships, create scene serialization

**No Physics/Collision:**
- Problem: No collision detection or physics engine
- Blocks: game mechanics, object interaction, spatial organization
- Fix approach: Add AABB collision detection, implement simple physics integration, create spatial partitioning (quadtree)

## Test Coverage Gaps

**Untested area: Core Graphics:**
- What's not tested: Canvas drawing primitives, pixel operations, color conversion, clipping
- Files: `enjin2/src/graphics/canvas.cpp`, `enjin2/src/graphics/primitives.cpp`
- Risk: Rendering bugs go undetected, performance regressions
- Priority: High - graphics are core functionality

**Untested area: Post-Processing Effects:**
- What's not tested: All post-fx effects (CRT scanlines, barrel distortion, noise, blur, glow, dither, contrast, brightness)
- Files: `enjin2/src/effects/postfx.cpp`
- Risk: Visual artifacts, incorrect pixel values, out-of-bounds access
- Priority: Medium - affects visual quality

**Untested area: Lua Bindings:**
- What's not tested: All Lua API functions (lua_getWidth, lua_getHeight, lua_setColor, drawing primitives, etc.)
- Files: `enjin2/src/scripting/bindings.cpp`
- Risk: Lua crashes on invalid input, memory corruption, incorrect behavior
- Priority: High - Lua scripting is key feature

**Untested area: Component System:**
- What's not tested: Component lifecycle (awake, update), component dependencies, object/component relationships
- Files: `enjin2/src/core/object.cpp`, component implementations
- Risk: Component initialization order bugs, missing dependencies
- Priority: Medium - core architecture

**Untested area: Image Cache:**
- What's not tested: Allocation, deallocation, fragmentation, cache eviction
- Files: `enjin2/src/components/image_cache.cpp`
- Risk: Memory leaks, allocation failures, data corruption
- Priority: High - memory management is critical

**Untested area: Platform-Specific Code:**
- What's not tested: ESP32 memory management, VCV-RACK integration, WebAssembly exports
- Files: `enjin2/src/scripting/lua_platform.cpp`, `enjin2/src/bindings/emscripten_bindings.cpp`
- Risk: Platform-specific bugs, memory limits exceeded, crashes
- Priority: High - each platform must work correctly

**Untested area: Memory Management:**
- What's not tested: Manual new/delete operations, buffer allocations, stack usage
- Files: All source files with manual memory management
- Risk: Memory leaks, buffer overflows, use-after-free
- Priority: Critical - memory issues can crash entire application

---

*Concerns audit: 2026-01-29*
