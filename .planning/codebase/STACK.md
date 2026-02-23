# Technology Stack

**Analysis Date:** 2026-02-23

## Languages

**Primary:**
- C++17 - Core engine library, graphics, UI, scripting bindings, all source under `src/` and `include/`
- C - LuaJIT amalgamated build for WebAssembly target (`luajit/src/ljamalg.c`)

**Secondary:**
- Lua 5.1+ - Embedded game scripting via `LuaEngine` in `src/scripting/`
- JavaScript - Documentation tooling scripts (`scripts/generate-api-docs.js`)

## Runtime

**Environment:**
- Native C++ runtime - desktop (Linux/macOS) and embedded (ESP32-S3)
- WebAssembly (WASM) via Emscripten for browser targets
- ESP-IDF FreeRTOS for ESP32-S3 embedded targets

**Package Manager:**
- npm - used only for documentation tooling (`package.json` at root, `docs/package.json`)
- Lockfile: `package-lock.json` present
- No C++ package manager - all C++ dependencies are vendored or system-installed

## Frameworks

**Core:**
- No external C++ framework - the library IS the framework
- Adafruit GFX Library (vendored at `Libs/Adafruit-GFX-Library/`) - font rendering, GFX compatibility API, `GFXfont`/`GFXglyph` types used in `include/enjin2/graphics/canvas.hpp`

**Documentation:**
- Docusaurus 3.9.2 (`docs/package.json`) - static site generation for API docs
- Doxygen - XML extraction from C++ headers, consumed by `scripts/generate-api-docs.js`
- Graphviz - dependency diagrams in Doxygen output

**Build/Dev:**
- CMake 3.16+ (`CMakeLists.txt`) - primary build system
- Emscripten (`emcc`) - WebAssembly compilation via `build_wasm.sh`
- PlatformIO (ESP32) - configured in `examples/platformio_esp32_lua.ini`

**Testing:**
- Custom CMake test executables (no external test framework)
  - `tests/image_comparison_test` - image comparison for shadow mode verification
  - `tests/shadow_mode_test` - parallel execution backend testing
  - Vendor headers (`vendor/stb_image.h`, `vendor/stb_image_write.h`) used in tests

## Key Dependencies

**Critical (C++ library):**
- Lua 5.1+ (system) - Scripting support; optional, can be disabled with `ENJIN2_BUILD_LUA=OFF`
  - Desktop: system Lua via `find_package(Lua)`
  - WebAssembly: LuaJIT 2.x built from source (`luajit/src/ljamalg.c`)
  - ESP32: Provided externally via `LUA_INCLUDE_DIRS` / `LUA_LIBRARIES` CMake vars
- Adafruit GFX Library (vendored at `Libs/Adafruit-GFX-Library/`) - font/glyph types only
- STB Image (vendored at `vendor/stb_image.h`, `vendor/stb_image_write.h`) - image I/O in tests

**Embedded Platform:**
- ESP-IDF (ESP32-S3) - FreeRTOS, `esp_attr.h`, IRAM/DRAM placement macros
  - Used in `include/enjin2/graphics/canvas_esp32s3.hpp`
- Arduino SDK - optional, conditioned on `#ifndef VCV_RACK` in `include/enjin2/graphics/canvas.hpp`

**Documentation Tooling (npm):**
- `xml2js` ^0.6.2 (root `package.json`) - parses Doxygen XML in `scripts/generate-api-docs.js`
- `@docusaurus/core` 3.9.2 - static site framework
- `@docusaurus/preset-classic` 3.9.2 - classic docs theme
- `react` ^18.2.0, `react-dom` ^18.2.0 - Docusaurus runtime
- `prism-react-renderer` ^2.3.0 - syntax highlighting

## Configuration

**Environment:**
- No `.env` files detected
- No runtime environment variables - this is a compiled library
- ESP32 build: `LUA_INCLUDE_DIRS` and `LUA_LIBRARIES` must be set as CMake variables

**Build Flags:**
- `VCV_RACK` - defined for all modules via `target_compile_definitions(...PUBLIC VCV_RACK)`; disables Arduino SDK includes
- `ENJIN2_BUILD_TESTS` (default ON) - enables test targets
- `ENJIN2_BUILD_EXAMPLES` (default ON) - enables example targets
- `ENJIN2_BUILD_LUA` (default ON) - enables Lua scripting library
- `ENJIN2_BUILD_WASM` (default OFF) - enables WebAssembly target
- `ENJIN2_USE_SIMD` (default ON) - SIMD optimizations
- `ESP32` - set for ESP32 platform builds; enables FreeRTOS and IRAM attributes

**Build Config Files:**
- `CMakeLists.txt` - root CMake config
- `tests/CMakeLists.txt` - test target definitions
- `examples/platformio_esp32_lua.ini` - PlatformIO config for ESP32-S3 builds
- `build_wasm.sh` - Emscripten WASM build script
- `docs/Doxyfile` - Doxygen configuration (referenced in CMakeLists.txt)

## Platform Requirements

**Development:**
- CMake 3.16+
- C++17 compatible compiler (GCC, Clang)
- Optional: Lua 5.1+ headers and libraries
- Optional: Doxygen + Graphviz (for docs target)
- Optional: Node.js 18+ + npm (for documentation site)
- WebAssembly builds: Emscripten SDK (emsdk) at `../emsdk` relative to project root

**Production Targets:**
- Desktop: Any C++17-capable OS (Linux, macOS confirmed by CI using ubuntu-latest)
- Embedded: ESP32-S3 via ESP-IDF + PlatformIO
- Browser: WebAssembly via Emscripten, outputs `enjin2.js` + `enjin2.wasm` as ES6 module named `Enjin2Module`

**Library Outputs:**
- `libenjin2_core.a` - core types, math, memory, scene, animation
- `libenjin2_graphics.a` - canvas, primitives, effects
- `libenjin2_ui.a` - drawable components, polar utilities
- `libenjin2_lua.a` - Lua scripting engine and bindings
- `enjin2` (INTERFACE target) - aggregates all modules

---

*Stack analysis: 2026-02-23*
