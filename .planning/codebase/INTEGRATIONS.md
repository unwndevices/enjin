# External Integrations

**Analysis Date:** 2026-02-27

## APIs & External Services

**Scripting Engine - Lua C API:**
- Lua 5.1 C API - Lua virtual machine execution and state management
  - SDK/Client: `lua.h`, `lauxlib.h`, `lualib.h` from Lua 5.1 or LuaJIT
  - Implementation: `src/scripting/lua_engine.cpp`, `src/scripting/bindings*.cpp`
  - Bindings header: `include/enjin2/scripting/bindings.hpp` (23KB, comprehensive API)

**Graphics Rendering:**
- SDL3 (v3.4.2) - Window management, input polling, and pixel framebuffer (desktop only)
  - SDK/Client: `#include <SDL3/SDL.h>`, linked as `SDL3::SDL3`
  - Source: Fetched from GitHub: `https://github.com/libsdl-org/SDL.git` tag `release-3.4.2`
  - Platform integration: `src/platform/sdl/sdl_main.cpp`
  - Window: 512x512 (128x128 canvas × 4 scale), 30 FPS default
  - Input mapping: SDL3 keyboard scancodes → button bitmask (WASD/arrows = directions, Z/X = A/B, Enter = Start)

## Data Storage

**Databases:**
- None - Pure in-memory engine, no persistent storage layer

**File Storage:**
- Platform-dependent file I/O:
  - Desktop (VCV Rack): Standard C++ file I/O enabled (`std::ifstream` in `LuaFileSystem::readDesktopFile()`)
    - Reads Lua scripts from filesystem via `LuaFileSystem::readScriptFile()`
    - Location: `include/enjin2/scripting/lua_platform.hpp` line 127
  - ESP32: Restricted (no file I/O enabled - `LuaPlatformConfig::ENABLE_FILE_IO = false`)
    - Scripts must be embedded or provided via other means
  - WebAssembly: Via Emscripten VFS (virtual file system)

**Caching:**
- Lua script cache - Maintains loaded script filenames in `LuaEngine::loadedScripts` vector
  - Cache populated on successful `executeFile()` calls
  - Cleared on `LuaEngine::shutdown()`
  - Location: `src/scripting/lua_engine.cpp`

## Authentication & Identity

**Auth Provider:**
- None - Engine is library-level; no user authentication or identity system

## Monitoring & Observability

**Error Tracking:**
- No external error tracking service
- Lua errors captured in `LuaResult` struct with error message strings
  - Structure: `src/scripting/lua_engine.hpp`, lines 18-32
  - Panic handler: `LuaEngine::luaPanic()` called on unrecoverable Lua errors

**Logs:**
- Console logging via `std::cout` and `std::cerr`
- Lua `print()` function directs to stdout
- Engine provides `LuaEngine::getErrorLog()` and similar debug functions
- GC memory stats via `engine.lua.memory()` bindings (`src/scripting/bindings_engine.cpp`)

**Profiling:**
- Memory profiling via Lua:
  - `engine.lua.collect()` - Trigger garbage collection
  - `engine.lua.memory()` - Get memory usage in bytes
  - Location: `include/enjin2/scripting/bindings.hpp` lines 500+
- Time state accessible via `engine.time.dt`, `engine.time.totalTime`, `engine.time.frameCount`

## CI/CD & Deployment

**Hosting:**
- Supports deployment to:
  - Desktop: Linux, macOS, Windows (via SDL3)
  - WebAssembly: Any modern browser (via Emscripten)
  - Embedded: ESP32 microcontroller (via ESP-IDF)

**Build System:**
- CMake-based build (no GitHub Actions config detected)
- Conditional targets:
  - `enjin2_sdl` - Desktop SDL3 executable (ENJIN2_BUILD_SDL=ON)
  - `enjin2_wasm` - WebAssembly module (ENJIN2_BUILD_WASM=ON)
  - Static libraries: `enjin2_core`, `enjin2_graphics`, `enjin2_ui`, `enjin2_input`, `enjin2_lua`

**Documentation:**
- Doxygen-based API docs (optional if Doxygen found)
- Node.js script for Docusaurus integration: `scripts/generate-api-docs.js`
- Build target: `cmake --build . --target docs`

## Environment Configuration

**Lua Configuration (Platform-Specific):**

Desktop (VCV Rack):
```cpp
MEMORY_LIMIT = 1MB
ENABLE_ALL_LIBS = true     // Load all standard Lua libraries (math, string, table, etc.)
ENABLE_FILE_IO = true      // Allow dofile(), loadfile(), require()
ENABLE_DEBUG = true        // Load debug library
```

ESP32:
```cpp
MEMORY_LIMIT = 256KB
ENABLE_ALL_LIBS = false    // Minimal libraries only
ENABLE_FILE_IO = false     // Disable dofile, loadfile, require, io
ENABLE_DEBUG = false       // No debug library to save memory
```

**Required CMake Variables (ESP32 Lua):**
- `-DLUA_INCLUDE_DIRS=/path/to/lua/include`
- `-DLUA_LIBRARIES=lua` (or compatible name)

**Build Configuration Variables:**
```bash
cmake .. \
  -DENJIN2_BUILD_LUA=ON      # Build Lua bindings (default: ON)
  -DENJIN2_BUILD_SDL=ON      # Build SDL3 runner (default: OFF)
  -DENJIN2_BUILD_WASM=ON     # Build WebAssembly (default: OFF)
  -DENJIN2_USE_SIMD=ON       # Enable SIMD (default: ON)
```

## WebAssembly Integration

**Emscripten Configuration:**
- Toolchain detection via `EMSCRIPTEN` CMake variable
- Memory: 64MB max, 1MB stack
- Export: ES6 module with `Enjin2Module` export name
- Bindings file: `src/bindings/emscripten_bindings.cpp`
- Pre-JS shim: `src/bindings/pre.js` (loaded before WASM module)
- Runtime methods exported: `ccall`, `cwrap` for C↔JS interop

**LuaJIT for WebAssembly:**
- Built from `luajit/src/ljamalg.c` (amalgamated build)
- Disabled features (for WASM compatibility):
  - FFI (`LUAJIT_DISABLE_FFI`) - Foreign Function Interface unavailable
  - JIT (`LUAJIT_DISABLE_JIT`) - JIT compilation disabled, bytecode interpreter only
- Enabled: `LUA_USE_APICHECK` for debugging

## ESP32 Integration

**Hardware Target:**
- Microcontroller: ESP32 / ESP32-S3 with PSRAM support
- SDK: ESP-IDF (managed externally)

**Lua Integration:**
- Users provide Lua via ESP-IDF component or NodeMCU-Lua
- Custom memory allocator: Uses `heap_caps_malloc()` with DMA-capable memory preference
- Location: `src/scripting/lua_platform.cpp` lines 24+

**Platform Features:**
- SPIFFS file system (optional): `#include "esp_spiffs.h"`
- Heap management: `#include "esp_heap_caps.h"`
- System info: `#include "esp_system.h"`

**Graphics Hardware (ESP32-S3 specific):**
- Custom Canvas4<> templated implementation with Xtensa SIMD hints
- Header: `include/enjin2/graphics/canvas_esp32s3.hpp`

## Webhooks & Callbacks

**Incoming:**
- None - Library mode, no server or listening endpoints

**Outgoing:**
- Lua script callback system via C++ component system
  - Callbacks: `C_LuaScript` component methods (`include/enjin2/components/lua_script.hpp`)
  - Triggered via scene/object lifecycle events
  - Script proxy userdata: `ScriptProxy` struct (`include/enjin2/scripting/bindings.hpp` lines 26-36)

## Lua Scripting Bindings

**Graphics API (love2d-style):**
- `clear(color)` - Clear canvas
- `setColor(color)` - Set drawing color
- `setPixel(x, y, color)` - Set individual pixel
- `getPixel(x, y)` - Read pixel value
- `line(x1, y1, x2, y2)` - Draw line
- `rectangle(mode, x, y, w, h)` - Draw rectangle ("fill" or "line")
- `circle(mode, x, y, r)` - Draw circle
- `getWidth()`, `getHeight()` - Canvas dimensions
- Location: `src/scripting/bindings_draw.cpp`, `include/enjin2/scripting/bindings.hpp` lines 200+

**Input API:**
- `getInput(button_index)` - Check button state
- Location: `src/scripting/bindings_input_sprites.cpp`

**System API:**
- `print(...)` - Output to console (Lua standard)
- `time()` - Get current time in seconds
- Location: `src/scripting/bindings_system.cpp`

**Engine API (engine.* namespace):**
- `engine.lua.collect()` - Force garbage collection
- `engine.lua.memory()` - Get Lua memory usage in bytes
- `engine.time.dt` - Frame delta time (seconds)
- `engine.time.totalTime` - Accumulated total time
- `engine.time.frameCount` - Frame counter
- Location: `src/scripting/bindings_engine.cpp`

**Math API:**
- `math.sin()`, `math.cos()`, `math.floor()`, etc. (standard Lua math library)
- Location: Lua standard library (provided by Lua 5.1)

**Scene/Object API:**
- Scene access and manipulation (phase 31+ feature)
- Location: `src/scripting/bindings.cpp`

---

*Integration audit: 2026-02-27*
