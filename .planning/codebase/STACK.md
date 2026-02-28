# Technology Stack

**Analysis Date:** 2026-02-28

## Languages

**Primary:**
- C++ 17 - Core engine, graphics, scripting bindings. Uses `-std=c++17` with `-MMD -MP` compiler dependency tracking.
- C - LuaJIT 2.1 embedded runtime, used for the Lua scripting sandbox. Configured with LUAJIT_DISABLE_FFI and LUAJIT_DISABLE_JIT for WebAssembly compatibility.

**Secondary:**
- Lua 5.1 - Game scripting language via LuaJIT. Scripts run in a sandboxed environment with disabled I/O and package systems.

## Runtime

**Environment:**
- LuaJIT 2.1 (embedded in `luajit/src/ljamalg.c`)
- System Lua 5.1 fallback for desktop builds
- Emscripten for WebAssembly builds (disables JIT compilation)

**Package Manager:**
- CMake 3.16+ - Build system and configuration
- FetchContent for SDL3 dependency management (Git-based, downloads on first configure if `ENJIN2_BUILD_SDL=ON`)

## Frameworks

**Core:**
- **Entity-Component-System (ECS)** - Scene-based architecture with Objects as entities, Components attached to Objects. Primary in `src/core/scene.cpp`, `src/core/object.cpp`.
- **Canvas Abstraction** - Type-erased canvas interface supporting both 4-bit (16-color) and 8-bit (256-color) pixel depths. Defined in `include/enjin2/graphics/canvas.hpp`.

**Graphics:**
- **Adafruit-GFX-Library** - Optional font rendering support via `include/enjin2/graphics/gfxfont.h`. Referenced in CMakeLists.txt at line 105.
- **SDL3** - Desktop platform backend (optional, `ENJIN2_BUILD_SDL=ON`). Downloaded via FetchContent from `https://github.com/libsdl-org/SDL.git` tag `release-3.4.2`.
- **Layer Compositor** - Multi-layer rendering system with per-layer visibility control. Defined in `include/enjin2/graphics/layer_compositor.hpp`.

**Scripting:**
- **Lua Bindings** - love2d.graphics-inspired API for familiar Lua development. Implemented across `src/scripting/bindings*.cpp` files.
- **Lua Engine** - Script execution context with support for per-object C_LuaScript components. Located in `src/scripting/lua_engine.cpp`.

**Testing:**
- CMake `enable_testing()` + CTest - Test discovery and execution. Tests in `tests/` subdirectory.

**Build/Dev:**
- **Doxygen** - Documentation generation (optional, if found). Generates XML for Docusaurus pipeline via `scripts/generate-api-docs.js`.
- **clang-tidy** - Static analysis lint target (`cmake -DCLANG_TIDY=ON`, then `cmake --build . --target lint`).

## Key Dependencies

**Critical:**
- **LuaJIT** (`luajit/src/ljamalg.c`) - Embedded Lua runtime. No external dependency needed; included as source.
- **SDL3** (via FetchContent) - Desktop platform support. Downloads on first CMake configure if enabled. Required for `enjin2_sdl` executable.

**Infrastructure:**
- **Adafruit-GFX** - Referenced but path relative to parent directory (`../Libs/Adafruit-GFX-Library`). Optional, only if building UI components.

## Configuration

**CMake Build Options:**
- `ENJIN2_BUILD_TESTS` (ON by default) - Enables CTest test suite
- `ENJIN2_BUILD_EXAMPLES` (ON by default) - Builds example executables
- `ENJIN2_BUILD_LUA` (ON by default) - Builds Lua scripting support
- `ENJIN2_BUILD_WASM` (OFF by default) - WebAssembly target using Emscripten
- `ENJIN2_USE_SIMD` (ON by default) - SIMD optimizations (platform-dependent)
- `ENJIN2_BUILD_SDL` (OFF by default) - SDL3 desktop executable. Requires internet on first configure for FetchContent.
- `CLANG_TIDY` (OFF by default) - Static analysis. Requires clang-tidy in PATH.

**Build Artifacts:**
- **Core Library:** `enjin2_core` (STATIC) - Memory, math, types, scene, object, animation
- **Graphics Library:** `enjin2_graphics` (STATIC) - Canvas, primitives, palette, effects
- **UI Library:** `enjin2_ui` (STATIC) - Components, drawable helpers
- **Lua Library:** `enjin2_lua` (STATIC, conditional) - Scripting bindings
- **Input Library:** `enjin2_input` (STATIC) - Input abstraction
- **Main Interface:** `enjin2` (INTERFACE) - Links all libraries together
- **Executables:** `enjin2_sdl` (desktop, conditional), `enjin2_wasm.js` (WebAssembly, conditional)

**Lua Memory Configuration:**
- Default: 256KB Lua heap (`src/scripting/lua_engine.cpp` line 40 shows `LUA_ALLOCF` limit)
- Configured via CMake and linker flags for WebAssembly (stack 1MB, max memory 64MB with growth enabled)

**Platform-Specific Defines:**
- `VCV_RACK` - Compiled into all targets. Used for VCV Rack integration and headless features.
- `ESP32` - Used for ESP32 builds; requires `LUA_INCLUDE_DIRS` and `LUA_LIBRARIES` to be set.
- `EMSCRIPTEN` - Automatically detected when building for WebAssembly. Disables LuaJIT JIT and FFI.

## Compiler Configuration

**C++ Standards:**
- Standard: C++17 required (`CMAKE_CXX_STANDARD_REQUIRED ON`)
- Compiler flags: `-MMD -MP` for dependency tracking
- Clang/AppleClang specific: `-Woverride` to catch detached virtual overrides (all targets)

**Optimization:**
- No explicit optimization level set in CMakeLists.txt (relies on `CMAKE_BUILD_TYPE`)
- Typical usage: `cmake -B build -DCMAKE_BUILD_TYPE=Release`

## Platform Requirements

**Development:**
- CMake 3.16 or newer
- C++17 capable compiler (GCC 7+, Clang 5+, MSVC 2019+)
- Lua 5.1 development files (for desktop Lua builds; LuaJIT embedded as fallback)
- (Optional) Doxygen for documentation
- (Optional) clang-tidy for static analysis
- (Optional) Emscripten SDK for WebAssembly builds

**Production:**
- **Desktop (SDL):** SDL3 library (dynamically linked)
- **Embedded (ESP32):** ESP-IDF with NodeMCU Lua component or equivalent
- **WebAssembly:** Browser with ES6 module support
- **VCV Rack:** VCV Rack 2.0+ plugin host

## Architecture Overview

The engine is structured in layered, modular libraries:

```
enjin2 (INTERFACE) ─────────────────────────────────────┐
├── enjin2_core (STATIC)                                │
│   └─ Memory, math, ECS core, scene management        │
├── enjin2_graphics (STATIC) → links enjin2_core       │
│   └─ Canvas abstraction, primitives, palette, FX    │
├── enjin2_ui (STATIC) → links enjin2_graphics         │
│   └─ UI components, drawable helpers                │
├── enjin2_input (STATIC)                              │
│   └─ Input abstraction layer                        │
└── enjin2_lua (STATIC, optional) → links graphics/ui │
    └─ Lua scripting bindings, engine.* API           │

Executables:
├── enjin2_sdl → links enjin2 + SDL3
│   └─ Desktop runner with SDL input/rendering
└── enjin2_wasm (JS) → links enjin2 + LuaJIT embedded
    └─ WebAssembly module with Emscripten bindings
```

---

*Stack analysis: 2026-02-28*
