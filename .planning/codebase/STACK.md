# Technology Stack

**Analysis Date:** 2026-03-01

## Languages

**Primary:**
- C++ 17 - Core engine implementation, graphics, physics, scene management
- C - Lua VM integration and platform-specific code
- Lua 5.1+ - Game logic scripting and Lua script support

**Secondary:**
- JavaScript/Emscripten - WebAssembly/browser bindings
- Bash - Build scripts (e.g., `build_wasm.sh`)

## Runtime

**Environment:**
- Desktop: Native C++17 execution (Linux, macOS, Windows via CMake)
- Embedded: ESP32-S3 microcontroller with FreeRTOS
- Web: WebAssembly via Emscripten

**Package Manager:**
- npm - For JavaScript dependencies (xml2js for documentation generation)
  - Lockfile: `package-lock.json` present

## Frameworks

**Core:**
- Custom C++ game engine (enjin2 v2.0.0) - Lightweight, static allocation, embedded-focused
- LuaJIT (v5.1-compatible) - JIT compilation on desktop, static compilation for embedded

**Testing:**
- CppUnit-style manual assertions - Simple unit tests in C++ (no external framework required)
- GTest (optional) - For sprite asset loading tests only (`sprite_load_test.cpp`)

**Build/Dev:**
- CMake 3.16+ - Build configuration and cross-compilation
- Doxygen - API documentation generation (integrated into build)
- clang-tidy - Static analysis linting (optional, `-DCLANG_TIDY=ON`)

## Key Dependencies

**Critical:**
- Lua 5.1+ (`liblua5.1-dev` on Debian/Ubuntu, `lua` via Homebrew on macOS) - Scripting engine
  - Optional: Can build without Lua with `-DENJIN2_BUILD_LUA=OFF`
  - Sources: System package or embedded LuaJIT from `luajit/src/`

**Graphics:**
- SDL3 (v3.4.2) - Desktop window management, input handling, rendering
  - Fetched via CMake FetchContent (`-DENJIN2_BUILD_SDL=ON`)
  - Source: `https://github.com/libsdl-org/SDL.git`
  - Used in: `src/platform/sdl/sdl_main.cpp`

**Vendor Libraries (embedded in repo):**
- Adafruit GFX Library (`../Libs/Adafruit-GFX-Library`) - Font rendering, text display
- stb_image.h (v2.27) - Image loading utilities (`vendor/stb_image.h`)
- stb_image_write.h (v1.16) - BMP/PNG image export (`vendor/stb_image_write.h`)

**Documentation:**
- xml2js (npm) - API documentation generation from Doxygen XML
  - Called by: `docs/Doxyfile` post-processing script

## Configuration

**Environment:**
- VCV_RACK - Desktop platform define (enables full Lua, debug, file I/O)
- ESP32 - Embedded platform define (minimal Lua, no debug, restricted I/O)
- EMSCRIPTEN - WebAssembly platform define (LuaJIT without FFI, single-threaded)

**Build Options (CMake):**
```cmake
ENJIN2_BUILD_TESTS=ON      # Build unit tests
ENJIN2_BUILD_EXAMPLES=ON   # Build example programs
ENJIN2_BUILD_LUA=ON        # Build Lua bindings (default: ON)
ENJIN2_BUILD_WASM=OFF      # Build WebAssembly target
ENJIN2_USE_SIMD=ON         # SIMD optimizations
CLANG_TIDY=OFF             # Enable static analysis linting
ENJIN2_BUILD_SDL=OFF       # Build SDL3 desktop runner
```

**Build System:**
- CMakeLists.txt - Root configuration
  - Modular library targets: `enjin2_core`, `enjin2_graphics`, `enjin2_ui`, `enjin2_input`, `enjin2_lua`
  - Linked interface: `enjin2` (INTERFACE library combining all targets)

**Compiler Requirements:**
- C++17 standard required
- Clang/AppleClang: `-Woverride` enabled on all targets (DT-03 contract)
- GCC: Native enforcement of override keyword mismatches

## Platform Requirements

**Development:**
- CMake 3.16 or higher
- C++17-capable compiler (Clang 3.5+, GCC 5.0+, MSVC 2017+)
- Optional: Doxygen for documentation generation
- Optional: Lua 5.1+ development headers
- Optional: GTest for sprite asset tests
- Optional: SDL3 (auto-fetched on first configure with `-DENJIN2_BUILD_SDL=ON`)

**Production (Desktop):**
- SDL3 for windowed/fullscreen rendering
- System Lua library (optional, can embed LuaJIT)
- Memory: ~1MB Lua heap limit (configurable in `LuaPlatformConfig`)

**Production (Embedded/ESP32-S3):**
- ESP-IDF toolchain (Xtensa compiler)
- FreeRTOS kernel
- Memory: 256KB Lua heap limit (static allocation, no malloc)
- No file I/O: Scripts loaded directly into memory or from SPIFFS/LITTLEFS

**Production (WebAssembly):**
- Emscripten SDK (1.39.0+)
- LuaJIT compiled without FFI/JIT
- Memory: 64MB virtual memory, 1MB stack (configurable in CMakeLists.txt)

## Memory Management

**Static Allocation:**
- No dynamic memory in hot paths (engine requirement)
- Lua memory pool: Fixed buffer (`LuaEngine::memoryPool`)
  - Desktop: 1MB (`LuaPlatformConfig::MEMORY_LIMIT`)
  - ESP32: 256KB
- Custom allocator: `LuaEngine::luaAllocator()` with static memory pool

**Garbage Collection:**
- Lua GC tuned per-platform: `LuaPlatform::tuneGarbageCollector()`
- Manual collection available: `engine.lua.collect()`
- Memory monitoring: `engine.lua.memory()` returns current usage

---

*Stack analysis: 2026-03-01*
