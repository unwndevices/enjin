# External Integrations

**Analysis Date:** 2026-02-23

## APIs & External Services

**None - This is an offline, self-contained embedded/desktop library.**

No HTTP clients, no remote API calls, and no cloud service SDKs are used anywhere in the core library or build system.

## Data Storage

**Databases:**
- None - not applicable to an embedded C++ engine library

**File Storage:**
- Local filesystem only (desktop builds)
  - Canvas export: `Canvas8::exportToPGM()` and `Canvas8::exportToBMP()` write to local files via `fopen`/`fclose`
  - Lua scripts: `LuaEngine::executeFile()` reads Lua `.lua` scripts from local filesystem (`src/scripting/lua_engine.cpp`)
  - Test output: `vendor/stb_image_write.h` used in test executables for PNG/BMP output

**Caching:**
- Image cache component: `src/components/image_cache.cpp` / `include/enjin2/components/` provides in-memory sprite/image caching at runtime, no persistent cache

## Authentication & Identity

**Auth Provider:**
- None - no authentication mechanisms

## Embedded Hardware Interfaces

**ESP32-S3 Platform:**
- ESP-IDF FreeRTOS (`freertos/FreeRTOS.h`, `freertos/queue.h`, `freertos/semphr.h`)
  - Used in `include/enjin2/graphics/canvas_esp32s3.hpp` for dual-core rendering with semaphores and queues
  - IRAM/DRAM placement attributes via `esp_attr.h` for hot-path optimization
- Arduino SDK (optional)
  - Conditionally included via `#ifndef VCV_RACK` guard in `include/enjin2/graphics/canvas.hpp`
  - Provides `Arduino.h` header for Arduino-compatible builds (display drivers etc.)
  - Disabled when `VCV_RACK` compile definition is set (desktop/VCV Rack builds)

**VCV Rack (Eurorack modular synthesizer plugin):**
- `VCV_RACK` compile definition set for all targets via `target_compile_definitions(...PUBLIC VCV_RACK)`
- Disables Arduino includes; enables desktop-compatible code paths

## Scripting Runtime

**Lua:**
- Desktop: system Lua 5.1+ linked as a shared library
  - Headers: system-installed (e.g., `/usr/include/lua5.1/`)
  - Libraries: system `liblua` linked by CMake `find_package(Lua)`
- WebAssembly: LuaJIT 2.x built from source at `luajit/src/ljamalg.c`
  - FFI disabled (`LUAJIT_DISABLE_FFI`), JIT disabled (`LUAJIT_DISABLE_JIT`) for WASM compatibility
- ESP32: externally provided; requires `LUA_INCLUDE_DIRS` and `LUA_LIBRARIES` CMake variables
  - Example: NodeMCU Lua component at `${IDF_PATH}/components/lua/include`

**Emscripten WebAssembly Bindings:**
- `src/bindings/emscripten_bindings.cpp` - C++ to JavaScript bindings via Emscripten `--bind`
- `src/bindings/pre.js` - pre-JS injected before the compiled WASM module
- Output ES6 module named `Enjin2Module`; exports `ccall` and `cwrap` runtime methods
- Memory: 64MB maximum, 1MB stack, `ALLOW_MEMORY_GROWTH=1`

## Monitoring & Observability

**Error Tracking:**
- None - no external error tracking

**Logs:**
- Lua engine errors surfaced via `LuaResult.error` string (`include/enjin2/scripting/lua_engine.hpp`)
- CMake build warnings/errors via `message(WARNING ...)` and `message(FATAL_ERROR ...)`
- CI warning gate: Doxygen warnings counted and compared to threshold of 20 in `.github/workflows/docs.yml`
- No runtime logging framework; ESP32-S3 builds would use `ESP_LOG*` macros if added

## CI/CD & Deployment

**Hosting:**
- GitHub Pages - documentation site deployed to `https://unwndevices.github.io/enjin/`

**CI Pipeline:**
- GitHub Actions (`.github/workflows/docs.yml`)
  - Trigger: push/PR to `main` affecting `docs/**`, `include/**`, or workflow file
  - Runner: `ubuntu-latest`
  - Steps:
    1. Checkout (`actions/checkout@v4`)
    2. Setup Node.js 22 (`actions/setup-node@v4`)
    3. `npm ci` at root + `docs/` directories
    4. Install `doxygen` and `graphviz` via `apt-get`
    5. CMake build (`Release`, `ENJIN2_BUILD_LUA=OFF`) then `cmake --build . --target docs`
    6. Doxygen warning count gate: fails if warnings exceed 20 (counted in `doxygen-warnings.log`)
    7. Run `node scripts/generate-api-docs.js` to produce Docusaurus Markdown from Doxygen XML
    8. `npm run build` in `docs/` to build Docusaurus static site
    9. Deploy to GitHub Pages (`actions/configure-pages@v4`, `actions/upload-pages-artifact@v3`, `actions/deploy-pages@v4`)

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

## Environment Configuration

**Required env vars:**
- None - this is a compiled library with no runtime environment dependencies

**Secrets location:**
- GitHub Actions uses `id-token: write` permission for OIDC-based GitHub Pages deployment; no stored secrets required

## Vendor/Bundled Libraries

All C++ dependencies are either system-installed or vendored - no package registry pull at build time:

| Library | Location | Purpose |
|---------|----------|---------|
| LuaJIT 2.x | `luajit/` | Scripting runtime for WASM builds |
| Adafruit GFX Library | `Libs/Adafruit-GFX-Library/` | Font types (`gfxfont.h`, `GFXfont`, `GFXglyph`) |
| stb_image | `vendor/stb_image.h` | Image loading in tests |
| stb_image_write | `vendor/stb_image_write.h` | Image writing in tests |
| DaisySP | `Libs/DaisySP/` | DSP library (referenced in Libs, not linked in main CMakeLists) |
| libDaisy | `Libs/libDaisy/` | Daisy platform HAL (referenced in Libs) |
| stmlib | `Libs/stmlib/` | STM32 utility library (referenced in Libs) |

---

*Integration audit: 2026-02-23*
