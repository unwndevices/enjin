# Stack Research

**Domain:** C++ embedded graphics engine — palette system, SDL3 desktop runner, input abstraction
**Researched:** 2026-02-23
**Confidence:** HIGH

## Context

This is an additive stack for v1.3 Tomodachi Readiness. The existing stack (C++17, CMake 3.16+, LuaJIT/system Lua, stb_image_write, Emscripten) is not touched. This document covers only the new dependencies needed for:

1. 16-color indexed palette system (15 colors + transparent, no dynamic allocation)
2. SDL3 desktop runner (third platform backend, C++ app with Lua scripting)
3. Flexible input abstraction (buttons, pots, joysticks, touchpads, keyboard — platform-agnostic)

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| SDL3 | 3.4.2 (latest stable as of 2026-02-23) | Desktop window, surface rendering, event loop | Officially stable since January 2025. SDL2 receives no new features. SDL3 provides `SDL_GetKeyboardState`, joystick/gamepad APIs, and `SDL_PIXELFORMAT_INDEX4LSB` for palette-indexed surfaces — all needed for the runner. SDL2 is a dead end for new projects in 2026. |
| No palette library | N/A | Palette is pure C++ header, zero deps | A 16-entry array of `uint32_t` (RGBA) with one reserved transparent index is 64 bytes. No library justifies that footprint. The existing `Pixel4` type already stores 4-bit indices 0–15; the palette is a parallel lookup table attached at display time. |
| No input library | N/A | Input abstraction is a thin enjin2 header | The Tomodachi input set (buttons, pots, joysticks, touchpad) maps cleanly to a 3-type model: digital (pressed/released), analog (0.0–1.0), and position (x/y pair). SDL3's `SDL_GetKeyboardState` + joystick API covers the desktop backend. ESP32 drives its own hardware directly. A custom `IInputProvider` interface with per-platform implementations is the right shape — no external library adds value here. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| SDL3 (system install) | 3.4.2 | Linked by `enjin2_sdl` target | Always required for the SDL desktop runner. NOT vendored — system install keeps the CMake opt-in consistent with how Lua is handled (`find_package(SDL3 QUIET)`, `ENJIN2_BUILD_SDL=ON/OFF`). |
| Lua 5.4 (system) | 5.4.8 | Scripting in SDL desktop runner | Already handled by existing `find_package(Lua QUIET)` + `ENJIN2_BUILD_LUA`. No change needed. Lua 5.4 is the current system default on Arch Linux and most modern distros. LuaJIT remains for WASM. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `sdl3` (apt/pacman/brew) | System SDL3 install for CMake `find_package` | `apt install libsdl3-dev` / `pacman -S sdl3` / `brew install sdl3`. SDL3 ships its own CMake config file — no custom Find module needed. |
| `cmake --build . -- ENJIN2_BUILD_SDL=ON` | Opt-in SDL3 runner build | Mirrors existing `ENJIN2_BUILD_LUA=ON/OFF` pattern. Keeps WASM and ESP32 builds unaffected. |

---

## Installation

```bash
# Arch Linux (current dev environment)
sudo pacman -S sdl3

# Debian/Ubuntu
sudo apt install libsdl3-dev

# macOS
brew install sdl3
```

CMake integration (follows existing enjin2 pattern):

```cmake
option(ENJIN2_BUILD_SDL "Build SDL3 desktop runner" OFF)

if(ENJIN2_BUILD_SDL)
    find_package(SDL3 QUIET CONFIG)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR
            "SDL3 was requested (ENJIN2_BUILD_SDL=ON) but could not be found.\n"
            "Install SDL3: pacman -S sdl3 | apt install libsdl3-dev | brew install sdl3"
        )
    endif()

    add_executable(enjin2_sdl src/platform/sdl/main.cpp)
    target_link_libraries(enjin2_sdl PRIVATE enjin2 SDL3::SDL3)
    target_compile_definitions(enjin2_sdl PRIVATE ENJIN2_PLATFORM_SDL)
endif()
```

Note: SDL3 config file on macOS has a known bug where specifying `SDL3-shared` component fails (`SDL3_FOUND=FALSE`). Use `find_package(SDL3 CONFIG REQUIRED)` without component to fix. Do not use `REQUIRED COMPONENTS SDL3-shared`.

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| SDL3 3.4.x | SDL2 2.x | Never for new projects in 2026. SDL2 receives no new features. SDL3 is the stable successor with `sdl2-compat` for legacy code. |
| SDL3 3.4.x | SFML | If the project ever needed a full 2D game framework with built-in audio, networking, and OpenGL. Overkill for a pixel-scale display runner. |
| SDL3 system install | SDL3 vendored (`add_subdirectory`) | Only if targeting a CI environment where SDL3 is not available as a package. Add a `MYGAME_VENDORED` CMake option to toggle — SDL3 wiki documents this pattern. |
| Custom IInputProvider | Gainput | Gainput is a full-featured input library with recording/playback and network sync. Unnecessary for a 4-button + 2-pot + 1-joystick device. The interface can be added later if Tomodachi grows. |
| Inline palette array `uint32_t[16]` | pigment/any palette lib | No palette library handles the specific constraint of "15 usable colors + 1 transparent, Pixel4 indices, lookup at display time, static storage, zero alloc." A 16-element array is the correct implementation. |
| Lua 5.4 (system) | LuaJIT (desktop) | LuaJIT is already used for WASM. On desktop, system Lua 5.4 works fine and is consistent with the existing CMake setup. LuaJIT on desktop would only matter for performance-sensitive scripting that doesn't exist in this context. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| SDL2 | No new features, community moving to SDL3, Fedora is replacing SDL2 with sdl2-compat in 2026. Starting a new SDL backend on SDL2 in 2026 is actively choosing a dead end. | SDL3 3.4.x |
| `SDL_PIXELFORMAT_INDEX8` for palette surfaces | Uses 1 byte per pixel. Canvas4 already uses 4-bit packed storage (2 pixels/byte). Promoting to 8-bit per pixel just to use SDL's palette surface wastes memory and breaks the zero-alloc pixel layout. | Convert to RGBA at display time: read packed Pixel4 buffer, index into `uint32_t palette[16]`, write to `SDL_Surface` in `SDL_PIXELFORMAT_RGBA8888`. |
| `std::map`, `std::vector` for input state | Dynamic allocation breaks the no-heap constraint. | Static arrays with fixed MAX_BUTTONS / MAX_AXES counts. Platform backend fills them; engine reads them. |
| SDL's `SDL_GameController` API for ESP32 input | `SDL_GameController` is desktop-only. Putting it in the input abstraction layer would create a platform dependency in the core input interface. | Keep `IInputProvider` interface in core with no SDL headers. SDL3 backend implements it using `SDL_GetKeyboardState` + `SDL_GetJoystickAxis`. ESP32 backend reads GPIO directly. |
| A separate `enjin2_input` CMake library target | Input abstraction is a header-only interface + platform implementations. Adding a CMake library target adds build complexity with no benefit if the implementations live alongside the platform runners (SDL runner, ESP32 platform). | Inline the `IInputProvider` interface in `enjin2_core` headers; platform-specific implementations go in the SDL runner and ESP32 platform files respectively. |

---

## Stack Patterns by Variant

**SDL3 desktop runner (primary new target):**
- CMake target: `enjin2_sdl` (executable)
- Deps: `enjin2` (interface lib) + `SDL3::SDL3` + optional `enjin2_lua`
- Pattern: `SDL_Init` → create window + renderer → allocate `Canvas4<W,H>` on stack → game loop (Lua callbacks or direct C++) → on each frame, blit Canvas4 buffer through palette to `SDL_Surface` → `SDL_UpdateWindowSurface`
- Compile def: `ENJIN2_PLATFORM_SDL=1`

**Palette system (pure enjin2_core, no external dep):**
- New header: `include/enjin2/graphics/palette.hpp`
- `Palette16` struct: `uint32_t colors[16]` (RGBA), transparent index = 0 by convention
- Conversion function: `void Canvas4::toRGBA(uint32_t* dst, const Palette16& pal)` — walks packed buffer, expands each Pixel4 nibble to `pal.colors[nibble]`, skips transparent
- No allocation. No SDL dependency. Works on ESP32 too (palette stored in PROGMEM).

**Input abstraction (pure enjin2_core, no external dep):**
- New header: `include/enjin2/input/input_provider.hpp`
- `IInputProvider` interface: `isPressed(uint8_t id)`, `getAxis(uint8_t id)`, `getPosition(uint8_t id, int16_t& x, int16_t& y)`
- SDL3 backend: `SdlInputProvider` in `src/platform/sdl/sdl_input.cpp` — keyboard state + joystick
- ESP32 backend: `Esp32InputProvider` in Arduino/ESP32 platform code — GPIO reads
- Static arrays: `bool buttons[MAX_BUTTONS]`, `float axes[MAX_AXES]`, `Point positions[MAX_POSITIONS]`
- `MAX_BUTTONS = 16`, `MAX_AXES = 8`, `MAX_POSITIONS = 4` are compile-time constants (covers Tomodachi + headroom)

---

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| SDL3 3.4.2 | CMake 3.16+ | SDL3 ships its own CMake config file. No minimum CMake upgrade needed. |
| SDL3 3.4.2 | C++17 | SDL3 C API is C99; usage from C++17 is idiomatic and well-documented. |
| Lua 5.4.8 | SDL3 3.4.2 | No interaction between them. Both are independent runtime deps of the SDL runner. |
| SDL3 3.4.2 (macOS) | `find_package(SDL3 CONFIG REQUIRED)` | Do NOT use `REQUIRED COMPONENTS SDL3-shared` — known bug in SDL3Config.cmake on macOS (reported May 2025, vcpkg issue #45498). |

---

## Sources

- [SDL3 NewFeatures — SDL Wiki](https://wiki.libsdl.org/SDL3/NewFeatures) — SDL3 capabilities vs SDL2
- [SDL3 README-cmake — SDL Wiki](https://wiki.libsdl.org/SDL3/README-cmake) — CMake `find_package(SDL3 CONFIG)` canonical pattern
- [SDL3 Official Release — Phoronix](https://www.phoronix.com/news/SDL3-Official-Release) — Confirmed stable Jan 2025, 3.4.2 current as of Feb 2026
- [SDL3 SDL_GetKeyboardState — SDL Wiki](https://wiki.libsdl.org/SDL3/SDL_GetKeyboardState) — Polling input pattern for desktop
- [SDL3 SDL_pixels.h — GitHub](https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_pixels.h) — `SDL_PIXELFORMAT_INDEX4LSB`, `SDL_CreatePalette` for indexed surface workflow
- [vcpkg SDL3-shared macOS bug — GitHub](https://github.com/microsoft/vcpkg/issues/45498) — Known macOS component bug, avoid `SDL3-shared` component
- [Arch Linux lua 5.4.8-2 package](https://archlinux.org/packages/extra/x86_64/lua/) — Confirmed system Lua version on current dev environment
- [Gainput — johanneskuhlmann.de](https://gainput.johanneskuhlmann.de/) — Reviewed and rejected: overkill for Tomodachi input set
- [Indexed color — Wikipedia](https://en.wikipedia.org/wiki/Indexed_color) — CLUT pattern, transparent index convention
- [SDL3 Surface and Pixel Manipulation — DeepWiki](https://deepwiki.com/libsdl-org/SDL/3.3-surface-and-pixel-manipulation) — SDL3 indexed surface workflow

---
*Stack research for: enjin2 v1.3 — palette system, SDL3 desktop runner, input abstraction*
*Researched: 2026-02-23*
