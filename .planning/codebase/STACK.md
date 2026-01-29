# Technology Stack

**Analysis Date:** 2026-01-29

## Languages

**Primary:**
- C++17 - Core library implementation across all platforms
- C - LuaJIT embedded engine integration
- Lua - Embedded scripting language for user scripting

**Secondary:**
- JavaScript - Emscripten WebAssembly bindings layer
- Bash - Build scripts (WASM build)

## Runtime

**Environment:**
- C++17 Standard Library - Runtime for desktop/embedded platforms

**Package Manager:**
- CMake 3.16+ - Build system and dependency management
- Lockfile: None (CMake handles dependencies directly)

**External Package Managers:**
- PlatformIO (optional) - ESP32 build configuration and library management
- npm (optional) - VCV Rack plugin library.json format

## Frameworks

**Core:**
- CMake 3.16+ - Cross-platform build configuration
- C++17 STL - Standard library for all platforms
- LuaJIT 2.1 - Embedded scripting engine (amalgamated source in `enjin2/luajit/`)

**Testing:**
- CMake Testing - Built-in test framework support
- Custom test examples - Platform-specific test files in `enjin2/examples/`

**Build/Dev:**
- Emscripten SDK - WebAssembly compilation for browser deployment
- PlatformIO/ESP-IDF - ESP32-S3 embedded development toolchain
- ESP-IDF Build System - Official Espressif IoT Development Framework

## Key Dependencies

**Critical:**
- LuaJIT 2.1 - Embedded JIT-compiled Lua scripting engine for real-time scripting
  - Embedded sources in `enjin2/luajit/src/`
  - Custom memory allocator for embedded constraints
  - Platform-specific builds (VCV Rack, ESP32, WebAssembly)

- Adafruit GFX Library - Graphics font rendering and text display
  - External library referenced in `enjin2/include/enjin2/graphics/canvas.hpp`
  - GFX font support for text rendering (`gfxfont.h`)
  - Required for `VCV_RACK` builds but path not in tree
  - Compatible with ESP32 Adafruit displays (SSD1306, etc.)

**Infrastructure:**
- Emscripten SDK - WebAssembly (WASM) compilation target
  - JavaScript bindings via `enjin2/src/bindings/emscripten_bindings.cpp`
  - Browser deployment via `enjin2/build_wasm.sh`
  - ES6 module output for modern web apps

- ESP-IDF Framework - Official Espressif IoT Development Framework
  - FreeRTOS RTOS for ESP32-S3 real-time operations
  - SPIFFS filesystem for script storage on embedded
  - PSRAM support for larger displays and buffers

- FreeRTOS - Real-time operating system (ESP32-S3)
  - Task scheduling and dual-core rendering support
  - Queue and semaphore primitives for thread safety

## Configuration

**Environment:**
- Platform-specific builds via CMake options:
  - `VCV_RACK` - Desktop VCV Rack plugin builds
  - `ESP32` - ESP32-S3 embedded device builds
  - `EMSCRIPTEN` - WebAssembly browser builds
- Build configuration toggles:
  - `ENJIN2_BUILD_TESTS` - Enable/disable test compilation
  - `ENJIN2_BUILD_EXAMPLES` - Enable/disable example compilation
  - `ENJIN2_BUILD_LUA` - Enable/disable Lua scripting support
  - `ENJIN2_BUILD_WASM` - Enable/disable WebAssembly builds
  - `ENJIN2_USE_SIMD` - Enable/disable SIMD optimizations

**Key configs required:**
- For VCV Rack: `VCV_RACK` macro defined, Adafruit GFX library available
- For ESP32-S3: `ESP32` macro defined, `LUA_INCLUDE_DIRS` and `LUA_LIBRARIES` set
- For WebAssembly: Emscripten SDK environment configured, `EMSCRIPTEN` macro defined
- For desktop: System Lua available or LuaJIT embedded build used

**Build:**
- `enjin2/CMakeLists.txt` - Main build configuration
- `enjin2/library.json` - VCV Rack/PlatformIO metadata
- `enjin2/build_wasm.sh` - WebAssembly build script
- `enjin2/examples/platformio_esp32_lua.ini` - ESP32 PlatformIO configuration
- `enjin2/examples/esp32_idf_example/CMakeLists.txt` - ESP-IDF project template

## Platform Requirements

**Development:**
- CMake 3.16 or higher
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- For VCV Rack: Adafruit GFX Library, VCV Rack SDK
- For ESP32-S3: ESP-IDF 5.0+, PlatformIO CLI
- For WebAssembly: Emscripten SDK 3.1+

**Production:**
- VCV Rack Plugin - Desktop (Linux, macOS, Windows)
- ESP32-S3 with PSRAM - Embedded device (240MHz, SPIRAM)
- Modern browsers with WebAssembly - Web deployment (Chrome, Firefox, Safari, Edge)
- Drop integration - Web project requiring WASM builds at `/home/unwn/dev/DROP/public/`

---

*Stack analysis: 2026-01-29*
