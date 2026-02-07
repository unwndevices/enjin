# External Integrations

## Hardware & Platform Integrations
- **ESP32-S3:** Direct integration for DMA-capable memory management and display drivers (see `src/scripting/lua_platform.cpp` and `include/enjin2/graphics/canvas_esp32s3.hpp`).
- **VCV Rack:** Conditional compilation support for integration into the VCV Rack virtual modular synthesizer environment.
- **WebAssembly/Emscripten:** Integration via `src/bindings/emscripten_bindings.cpp` for web-based execution.

## CI/CD & Deployment
- **GitHub Actions:** Automated documentation deployment to GitHub Pages via `.github/workflows/docs.yml`.
- **PlatformIO:** Support for the PlatformIO ecosystem via `library.json`.

## Data Storage & Filesystems
- **SPIFFS/VFS:** Integrated for ESP32 file system support in the Lua engine.
