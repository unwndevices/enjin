# Technology Stack

## Languages
- **Primary:** C++17 (Core engine, Graphics, UI)
- **Secondary:** C (LuaJIT, stb libraries), JavaScript (Doc generation, Docusaurus), Lua (Scripting logic)

## Runtime Environments
- **Desktop:** Linux/Windows
- **Embedded:** ESP32-S3 (DMA-capable memory management, display drivers)
- **Web:** WebAssembly (via Emscripten)

## Build System
- **CMake:** Primary build system for all targets.
- **Emscripten/WASM:** Managed via `build_wasm.sh`.

## Frameworks & Core Modules
- **Core Engine:** Custom Entity Component System (ECS).
- **Scripting:** LuaJIT integration for embedded scripting logic.
- **Documentation:** Doxygen for API extraction, Docusaurus for the web portal.

## Key Dependencies
- **LuaJIT:** Scripting engine integrated as a submodule/source in `luajit/`.
- **stb_image / stb_image_write:** Header-only libraries for image I/O, located in `vendor/`.
- **Adafruit GFX:** Integrated for font rendering and graphics primitives (referenced in `src/graphics/canvas.hpp`).
