# External Integrations

**Analysis Date:** 2026-01-29

## APIs & External Services

**Build Systems:**
- VCV Rack SDK - Modular synth plugin environment for desktop builds
  - Integration: `VCV_RACK` macro defined in `enjin2/CMakeLists.txt`
  - Graphics: Adafruit GFX Library compatibility layer in `enjin2/include/enjin2/graphics/canvas.hpp`
  - Font rendering: `gfxfont.h` from Adafruit GFX (external dependency)
  - Entry point: VCV Rack plugin hooks via `library.json`

- PlatformIO/ESP-IDF - Espressif IoT development framework for ESP32-S3
  - Integration: ESP32 build config in `enjin2/examples/platformio_esp32_lua.ini`
  - RTOS: FreeRTOS task scheduling and synchronization
  - File system: SPIFFS for Lua script storage on embedded devices
  - Hardware access: ESP32-S3 specific APIs (PSRAM, dual-core, SPI)

- Emscripten - WebAssembly SDK for browser deployment
  - Integration: Emscripten build via `enjin2/build_wasm.sh` and `enjin2/src/bindings/emscripten_bindings.cpp`
  - Bindings: `emscripten/bind.h` for JavaScript/C++ interop
  - Output: ES6 modules (`enjin2.js`, `enjin2.wasm`)
  - Target: DROP project deployment at `/home/unwn/dev/DROP/public/`

## Data Storage

**Databases:**
- None - Library uses in-memory buffers only
  - Canvas pixel buffers: `Canvas4<WIDTH, HEIGHT>` and `Canvas8<WIDTH, HEIGHT>` templates
  - Static allocation for embedded constraints (no dynamic heap usage in ESP32 builds)

**File Storage:**
- Platform-specific filesystem access:
  - ESP32-S3: SPIFFS filesystem via FreeRTOS `LuaFileSystem::readScriptFile()` in `enjin2/src/scripting/lua_engine.cpp`
  - Desktop: Standard C++ file I/O via `<fstream>`
  - WebAssembly: Virtual filesystem via Emscripten VFS (scripts loaded from memory or fetch)

**Caching:**
- Image cache component - In-memory sprite caching
  - Implementation: `enjin2/include/enjin2/components/image_cache.hpp`
  - Purpose: Cache frequently used sprites to reduce redraw overhead
  - Storage: Static buffers allocated at compile time

## Authentication & Identity

**Auth Provider:**
- None - No external authentication required
  - Library is a graphics/scripting engine with no user accounts
  - All security is platform-level (filesystem permissions, web sandbox)

## Monitoring & Observability

**Error Tracking:**
- None - No external error tracking services
  - Error handling via `LuaResult` struct in `enjin2/include/enjin2/scripting/lua_engine.hpp`
  - Panic handler via `LuaEngine::luaPanic()` for Lua runtime errors

**Logs:**
- Platform-specific logging:
  - ESP32-S3: `ESP_LOG` macros via ESP-IDF logging framework
  - Desktop: Standard C++ `std::cout`/`std::cerr` or platform-specific logging
  - WebAssembly: `console.log()` via Emscripten JavaScript interop
  - Lua: `print()` function exposed to scripts for debug output

## CI/CD & Deployment

**Hosting:**
- Local deployment to `/home/unwn/dev/DROP/public/` for WebAssembly builds
  - Automated via `enjin2/build_wasm.sh`
  - Copies `enjin2.js` and `enjin2.wasm` to DROP public directory

**CI Pipeline:**
- None detected - No GitHub Actions, GitLab CI, or other CI configuration
  - Manual builds via CMake for each platform
  - Platform-specific build scripts (`build_wasm.sh`, `platformio_esp32_lua.ini`)

## Environment Configuration

**Required env vars:**
- For ESP32-S3 builds:
  - `LUA_INCLUDE_DIRS` - Path to Lua headers (e.g., NodeMCU or ESP-IDF Lua component)
  - `LUA_LIBRARIES` - Lua library name (e.g., `lua` or `nodemcu_lua`)

- For WebAssembly builds:
  - `EMSDK_DIR` - Path to Emscripten SDK (default: `../emsdk`)
  - `EMSDK` - Emscripten SDK directory
  - `EM_CONFIG` - Emscripten configuration file path
  - `PATH` - Must include `EMSDK_DIR` and `EMSDK_DIR/upstream/emscripten`

- For ESP-IDF builds:
  - `IDF_PATH` - Path to ESP-IDF framework installation

**Secrets location:**
- No secrets required - Library has no API keys, tokens, or credentials
  - Build configuration files are committed (CMakeLists.txt, library.json, platformio.ini)
  - No `.env` files or secret management detected

## Webhooks & Callbacks

**Incoming:**
- None - No HTTP endpoints or webhook receivers
  - Library is a rendering engine without network services

**Outgoing:**
- None - No HTTP requests or webhook triggers
  - No external API calls from the library
  - All interactions are local (file I/O, memory operations)

## Hardware Integrations

**Display Controllers:**
- Adafruit GFX compatibility layer - Support for common OLED/LCD displays
  - External library: Adafruit GFX Library (`Libs/Adafruit-GFX-Library/` referenced but not in tree)
  - Supported displays: SSD1306 (128x64 OLED), other Adafruit GFX-compatible displays
  - Font support: `GFXfont` structure for custom fonts

- ESP32-S3 display integration - Direct hardware access for embedded builds
  - SPI display support via ESP-IDF driver
  - PSRAM integration for larger display buffers (256x128, 320x240)
  - IRAM-optimized drawing functions via `IRAM_ATTR` attribute

**Platform-Specific APIs:**
- VCV Rack - Desktop plugin API
  - Frame buffer integration with Rack's display system
  - Parameter and module lifecycle hooks
  - Real-time audio/visual synchronization

- FreeRTOS - ESP32-S3 real-time operating system
  - Task scheduling for dual-core rendering
  - Queue-based command passing between cores
  - Semaphore-based frame synchronization
  - RTOS primitives in `enjin2/include/enjin2/graphics/canvas_esp32s3.hpp`

---

*Integration audit: 2026-01-29*
