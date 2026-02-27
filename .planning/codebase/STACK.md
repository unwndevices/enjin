# Technology Stack

**Analysis Date:** 2026-02-27

## Languages

**Primary:**
- C++17 - Core engine implementation, graphics, input, and component systems (`src/core/`, `src/graphics/`, `src/input/`)
- Lua 5.1 - Embedded scripting language for game logic and Lua bindings (`luajit/src/`, `src/scripting/`)
- C - LuaJIT implementation for WASM/Emscripten and system library bindings

**Secondary:**
- JavaScript - Emscripten bindings for WebAssembly (`src/bindings/pre.js`)
- Shell/CMake - Build system configuration and cross-platform setup

## Runtime

**Environment:**
- CMake 3.16+ - Cross-platform build system
- C++17-compliant compiler (tested with Clang, GCC)
- Emscripten SDK for WebAssembly targets
- ESP-IDF for ESP32 embedded targets

**Package Manager:**
- CMake FetchContent - Downloads dependencies (SDL3 via Git)
- Manual Lua integration - LuaJIT embedded as source (`luajit/`) or system Lua 5.1

## Frameworks

**Core Graphics:**
- Custom Canvas system (4-bit and 8-bit pixel formats) - `include/enjin2/graphics/canvas.hpp`
- Custom Primitives library (lines, circles, rectangles) - `src/graphics/primitives.cpp`
- Palette management - `src/graphics/palette.cpp`

**Scripting:**
- LuaJIT (v2.x) - JIT-compiled Lua for desktop (VCV Rack) - embedded in `luajit/src/`
- Lua 5.1 - Fallback for ESP32 and embedded systems - user-provided via `-DLUA_INCLUDE_DIRS` and `-DLUA_LIBRARIES`
- Custom Lua bindings layer - `src/scripting/bindings*.cpp`, `include/enjin2/scripting/bindings.hpp`

**ECS/Scene Management:**
- Custom Entity Component System - `include/enjin2/core/object.hpp`, `include/enjin2/components/`
- Scene system with state machines - `src/core/scene.cpp`
- Script components - `include/enjin2/components/lua_script.hpp`

**Animation:**
- Keyframe animation system - `src/animation/keyframe.cpp`, `include/enjin2/animation/`

**Input Abstraction:**
- Platform-agnostic input state (`include/enjin2/input/input_state.hpp`)
- SDL3 input polling for desktop (`src/platform/sdl/sdl_main.cpp`)
- Platform-specific input binding in `input_platform_poll()`

**Build/Dev:**
- CMake - Main build system
- Doxygen - API documentation generation (optional, if found)
- Node.js - Documentation generation script (`scripts/generate-api-docs.js`)

## Key Dependencies

**Critical:**
- LuaJIT or Lua 5.1 - Scripting engine (required for ENJIN2_BUILD_LUA=ON)
  - Desktop: System Lua via `find_package(Lua)` or embedded LuaJIT
  - WebAssembly: LuaJIT built from `luajit/src/ljamalg.c` with FFI/JIT disabled
  - ESP32: User-provided via CMake variables
  - Location: `include/enjin2/scripting/lua_engine.hpp`, `src/scripting/`

**Infrastructure:**
- SDL3 (v3.4.2) - Desktop window/input/rendering (optional, ENJIN2_BUILD_SDL=OFF by default)
  - Fetched from GitHub: `https://github.com/libsdl-org/SDL.git` tag `release-3.4.2`
  - Used in `src/platform/sdl/sdl_main.cpp` for window creation and keyboard input
  - Links as `SDL3::SDL3` target

- Adafruit-GFX-Library (optional reference) - Located at `../Libs/Adafruit-GFX-Library` (external to repo)
  - Included in UI library build (`include/enjin2/ui/`)

- Emscripten - WebAssembly toolchain (required for ENJIN2_BUILD_WASM=ON)
  - Provides clang compiler and JavaScript binding support
  - Custom memory settings: 64MB max, 1MB stack

## Configuration

**Environment:**
- Platform definitions via compile flags:
  - `VCV_RACK` - Desktop/VCV Rack platform (default, sets memory to 1MB, enables all Lua libs)
  - `ESP32` - Embedded ESP32 platform (64KB Lua memory, minimal libs, no file I/O)
  - `EMSCRIPTEN` - WebAssembly platform (LuaJIT from source with FFI/JIT disabled)

- Conditional feature flags:
  - `ENJIN2_BUILD_LUA` (ON) - Build Lua scripting bindings and engine
  - `ENJIN2_BUILD_TESTS` (ON) - Build test suite
  - `ENJIN2_BUILD_EXAMPLES` (ON) - Build example programs
  - `ENJIN2_BUILD_SDL` (OFF) - Build SDL3 desktop runner
  - `ENJIN2_BUILD_WASM` (OFF) - Build WebAssembly target
  - `ENJIN2_USE_SIMD` (ON) - Enable SIMD optimizations (Xtensa for ESP32)

**Build:**
- `CMakeLists.txt` - Root build configuration
- `include/enjin2/scripting/lua_platform.hpp` - Platform-specific Lua settings
- `src/scripting/lua_platform.cpp` - Platform-specific implementations

## Platform Requirements

**Development:**
- C++17 compiler (Clang 10+, GCC 8+, MSVC 2019+)
- CMake 3.16+
- Lua 5.1 development headers (for desktop build)
  - Debian/Ubuntu: `liblua5.1-dev`
  - macOS: `brew install lua`
  - Windows: vcpkg or manual installation

**Production:**
- Desktop: Linux, macOS, Windows with SDL3 runtime
- WebAssembly: Modern browser with WebAssembly support (Firefox, Chrome, Safari)
- ESP32: ESP-IDF toolchain with Lua component configured

## Compilation Settings

**C++ Standard:** C++17 with `-std=c++17 -MMD -MP` (dependency tracking)

**Compiler Flags:**
- `-Woverride` (Clang/AppleClang) - Catch detached virtual overrides on all engine targets
- SIMD: Controlled by `ENJIN2_USE_SIMD` option (default ON)

**WebAssembly (Emscripten) Specific:**
- `-sIMPORTED_MEMORY=1` - Share memory between JS and WASM
- `-sALLOW_MEMORY_GROWTH=1` - Allow dynamic memory expansion
- `-sMAXIMUM_MEMORY=67108864` - 64MB max memory
- `-sSTACK_SIZE=1048576` - 1MB stack
- `-sEXPORT_ES6=1` - ES6 module output
- `-sMODULARIZE=1` - Modular export pattern
- `--bind` - Emscripten bindings

---

*Stack analysis: 2026-02-27*
