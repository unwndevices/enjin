# Architecture Research

**Domain:** enjin2 v1.3 — Palette system, SDL2 desktop runner, input abstraction
**Researched:** 2026-02-23
**Confidence:** HIGH

## System Overview

Current state after v1.2: three library targets, one WASM executable, no SDL2 platform, no palette,
no general input abstraction.

```
┌──────────────────────────────────────────────────────────────────────┐
│                   Application Layer (SDL2 runner / WASM / ESP32)     │
├──────────────────────────────────────────────────────────────────────┤
│  ┌───────────────────┐  ┌───────────────────┐  ┌──────────────────┐  │
│  │  enjin2_lua        │  │  enjin2_wasm       │  │  enjin2_sdl2     │  │
│  │  LuaScriptSystem  │  │  emscripten_bindings│  │  (NEW)          │  │
│  │  LuaCanvas        │  │                    │  │  SDL2 main loop │  │
│  │  LuaBindings      │  │                    │  │  SDL2 input     │  │
│  └────────┬──────────┘  └────────┬───────────┘  └────────┬─────────┘  │
│           │                      │                        │            │
├───────────┴──────────────────────┴────────────────────────┴────────────┤
│                           enjin2_ui                                    │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │  InputSystem (NEW — abstract)  |  InputEvent  |  InputComponent │  │
│  └──────────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────────┤
│                          enjin2_graphics                               │
│  ┌────────────┐  ┌─────────────────────────────────────────────────┐  │
│  │  Palette   │  │  ICanvas<TPixel>  Canvas4  Canvas8              │  │
│  │  (NEW)     │  │  Canvas4_ESP32S3                                │  │
│  └────────────┘  └─────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────────┤
│                           enjin2_core                                  │
│  types.hpp  math.hpp  memory.hpp  object.hpp  scene.hpp               │
│  Component  System  EntityManager  SceneStateMachine                  │
└──────────────────────────────────────────────────────────────────────┘
```

Three new pieces fit into this stack in three distinct places:

| New Feature | Target Library | Fits Above | New Files |
|-------------|---------------|------------|-----------|
| Palette | enjin2_graphics | ICanvas / types | `graphics/palette.hpp` |
| Input abstraction | enjin2_ui | ECS component/system | `ui/input_system.hpp`, `ui/input_event.hpp` |
| SDL2 runner | new executable | enjin2_lua + input | `platforms/sdl2/` |

---

## Component Responsibilities

| Component | Responsibility | Status |
|-----------|---------------|--------|
| `Palette` | 16-entry RGB lookup table, index 0 = transparent | NEW in enjin2_graphics |
| `InputEvent` | Platform-agnostic description of a single input event | NEW in enjin2_ui |
| `InputSource` | Abstract interface: polls events, pushes to queue | NEW in enjin2_ui |
| `InputSystem` | Drains InputSource, dispatches to listeners | NEW in enjin2_ui |
| `SDL2Runner` | SDL2 window, event loop, feeds InputSource | NEW executable |
| `SDL2InputSource` | SDL2 implementation of InputSource | NEW in SDL2 runner |
| `ICanvas<TPixel>` | Pixel-level drawing surface | UNCHANGED |
| `Canvas4<W,H>` | 4-bit packed pixel buffer | UNCHANGED |
| `LuaCanvas` | Type-erased canvas wrapper for Lua | UNCHANGED |
| `LuaScriptSystem` | Load/execute Lua scripts, call update() | UNCHANGED |
| `LuaPlatform` | Per-platform Lua state config | NEEDS EXTENSION for SDL2 |

---

## Feature 1: Palette System

### What it is

Pixel values in Canvas4 are 4-bit indices (0–15). Currently they are treated as raw grayscale.
A `Palette` maps those 16 indices to concrete RGB values at display time, not at draw time.
This means all canvas drawing code, Lua scripts, and ESP32 pixel buffers remain 4-bit index-only —
palette application happens only during the final "blit to output" step.

Index 15 is transparent (skip during blit). The 15 usable color slots (0–14)
match Tomodachi's 15-color-plus-transparent display requirement. This preserves
existing `Colors::BLACK = Pixel4(0)` behavior.

### Where it lives

`include/enjin2/graphics/palette.hpp` — header-only, no new .cpp file needed.

```
enjin2_graphics
  include/enjin2/graphics/
    palette.hpp         <-- NEW: Palette struct + RGB24 type
    canvas.hpp          <-- UNCHANGED
    canvas_esp32s3.hpp  <-- UNCHANGED
```

### Interface design

```cpp
namespace enjin2 {

// 24-bit RGB color for palette entries
struct RGB24 {
    uint8_t r, g, b;
    constexpr RGB24() : r(0), g(0), b(0) {}
    constexpr RGB24(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

// 16-entry indexed color palette
// Index 15 = transparent (skip on blit)
// Indices 0-14 = opaque colors
struct Palette {
    static constexpr uint8_t SIZE = 16;
    static constexpr uint8_t TRANSPARENT = 15;

    RGB24 entries[SIZE];

    constexpr Palette() : entries{} {}

    void setColor(uint8_t index, RGB24 color) {
        if (index < SIZE) entries[index] = color;
    }

    RGB24 getColor(uint8_t index) const {
        if (index >= SIZE) return RGB24{};
        return entries[index];
    }

    bool isTransparent(uint8_t index) const {
        return index == TRANSPARENT;
    }
};

// Default greyscale palette (for backward compat during dev)
constexpr Palette makeGreyscalePalette();

} // namespace enjin2
```

### Integration points

- **Canvas4**: zero changes. It stores 4-bit indices; palette is applied externally.
- **SDL2 runner**: at blit time, iterates canvas pixels, maps through palette, writes RGB to SDL_Texture.
- **WASM**: same blit-time mapping in JS or in the Emscripten binding.
- **ESP32**: at DMA transfer, map 4-bit index through palette to hardware color format.
- **Lua API**: expose `setColor(index, r, g, b)` and `getColor(index)` via LuaBindings.

### What does NOT change

Canvas4 pixel storage, draw calls, LuaCanvas, ICanvas interface — all unchanged.
Palette is strictly an output-stage concern.

---

## Feature 2: Input Abstraction

### The problem

The existing `enjin2_ui` has mouse-only UI components (`InputComponent`, `InputSystem` in
`ui/component.hpp` and `ui/system.hpp`). These are typed to screen coordinates and click events.
Tomodachi needs buttons, potentiometers, joysticks, touchpads, and keyboard — none of which fit
a mouse-click model. SDL2 keyboard/gamepad events also need to be routed through the same path.

### Design: event + source + consumer

Three concepts, all in `enjin2_ui`:

**InputEvent** — a tagged union of all possible physical input types.
**InputSource** — abstract interface that produces InputEvents (one per platform).
**InputConsumer** — callback/handler interface; anything that wants input registers with InputSystem.

```cpp
namespace enjin2 {

// All physical input kinds Tomodachi cares about
enum class InputKind : uint8_t {
    Button,      // pressed/released (buttons, keyboard keys)
    Pot,         // analog 0.0-1.0 (potentiometer, slider)
    Joystick,    // two-axis analog (-1.0 to 1.0 each)
    Touchpad,    // 2D position, optional press
};

struct InputEvent {
    InputKind kind;
    uint8_t   id;       // device-specific channel index

    union {
        struct { bool pressed; }            button;
        struct { float value; }             pot;
        struct { float x; float y; }        joystick;
        struct { float x; float y;
                 bool  pressed; }           touchpad;
    };
};

// Platform-agnostic source interface
class InputSource {
public:
    virtual ~InputSource() = default;
    // Called once per frame; implementations push events into the provided buffer
    virtual void poll(InputEvent* buffer, uint8_t& count, uint8_t capacity) = 0;
};

// Consumer callback
using InputHandler = void(*)(const InputEvent&, void* userData);

struct InputListener {
    InputHandler handler;
    void*        userData;
    InputKind    filter;   // only receive this kind (or 0xFF for all)
};

// System: owns listeners[], calls InputSource::poll each frame
class InputSystem {
public:
    static constexpr uint8_t MAX_LISTENERS = 16;
    static constexpr uint8_t EVENT_BUFFER  = 32;

    void setSource(InputSource* src);
    void addListener(InputListener listener);
    void removeListener(InputHandler handler);
    void update();   // call once per frame; drains source, dispatches

private:
    InputSource*    source = nullptr;
    InputListener   listeners[MAX_LISTENERS];
    uint8_t         listenerCount = 0;
    InputEvent      eventBuffer[EVENT_BUFFER];
};

} // namespace enjin2
```

### Where it lives

```
enjin2_ui
  include/enjin2/ui/
    input_event.hpp     <-- NEW: InputEvent, InputKind, InputSource
    input_system.hpp    <-- NEW: InputSystem, InputListener
    component.hpp       <-- UNCHANGED (existing mouse UI components left alone)
    system.hpp          <-- UNCHANGED
  src/ui/
    input_system.cpp    <-- NEW: InputSystem::update() implementation
```

### Integration points

- **SDL2 runner**: `SDL2InputSource` implements `InputSource`; maps SDL keyboard/gamepad events to
  `InputEvent`. Registered with `InputSystem` at runner startup.
- **ESP32**: `ESP32InputSource` implements `InputSource`; reads GPIO button states,
  ADC potentiometer values, joystick ADC axes. Same `InputSystem` is used unmodified.
- **Lua**: expose `enjin.input.getButton(id)`, `enjin.input.getPot(id)`, etc. via LuaBindings.
  The Lua binding layer queries last-known state; `InputSystem` maintains simple state cache.
- **Existing UI**: `InputComponent` / existing mouse handling is untouched. The new `InputSystem`
  is a parallel addition, not a replacement.

### No input in enjin2_core

Input is kept in `enjin2_ui` because it is part of the interactive layer, not the core engine.
This matches the existing library split: core knows nothing about user interaction.

---

## Feature 3: SDL2 Desktop Runner

### What it is

A new CMake executable (`enjin2_sdl2`) that provides the "third platform backend" alongside
WASM and ESP32. It creates an SDL2 window, runs the main loop, feeds `SDL2InputSource` into
`InputSystem`, and at each frame maps Canvas4 pixel indices through the `Palette` to an
SDL_Texture for display.

### Where it lives

```
platforms/
  sdl2/
    main.cpp            <-- SDL2 main loop, window creation
    sdl2_runner.hpp     <-- SDL2Runner class
    sdl2_runner.cpp     <-- SDL2Runner implementation
    sdl2_input.hpp      <-- SDL2InputSource : InputSource
    sdl2_input.cpp      <-- SDL2InputSource::poll()
```

Not under `src/` because it is a platform executable, not a library. `examples/` is also
wrong because it is infrastructure, not a demo. `platforms/` is the correct location and sets
precedent for future platform ports.

### CMake target

```cmake
option(ENJIN2_BUILD_SDL2 "Build SDL2 desktop runner" OFF)

if(ENJIN2_BUILD_SDL2)
    find_package(SDL2 REQUIRED)

    add_executable(enjin2_sdl2)
    target_sources(enjin2_sdl2 PRIVATE
        platforms/sdl2/main.cpp
        platforms/sdl2/sdl2_runner.cpp
        platforms/sdl2/sdl2_input.cpp
    )
    target_include_directories(enjin2_sdl2 PRIVATE
        include
        ${SDL2_INCLUDE_DIRS}
    )
    target_link_libraries(enjin2_sdl2 PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_ui
        $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>
        ${SDL2_LIBRARIES}
    )
    target_compile_definitions(enjin2_sdl2 PRIVATE SDL2_RUNNER)
endif()
```

Using `SDL2_RUNNER` define instead of `VCV_RACK` separates SDL2 builds cleanly.

### LuaPlatform extension

`lua_platform.hpp` currently compiles only if `VCV_RACK` or `ESP32` is defined; otherwise
it emits `#error`. SDL2 builds need a third case:

```cpp
// lua_platform.hpp — extend existing #ifdef chain
#ifdef VCV_RACK
    // LuaJIT (existing)
#elif defined(ESP32)
    // ESP32 Lua (existing)
#elif defined(SDL2_RUNNER)
    // Desktop Lua — same as VCV_RACK; system Lua via find_package
    extern "C" { #include "lua.h" ... }
#else
    #error "Platform not supported for Lua integration"
#endif
```

`LuaPlatformConfig` for SDL2 mirrors VCV_RACK (ENABLE_FILE_IO=true, ENABLE_ALL_LIBS=true).

### Main loop data flow

```
SDL_Init → create SDL_Window + SDL_Renderer + SDL_Texture (W*H, ARGB8888)
               ↓
main loop:
  SDL2InputSource::poll()        -- read SDL events, convert to InputEvents
  InputSystem::update()          -- dispatch events to Lua/app listeners
  LuaScriptSystem::callFunction("update", delta)
    → Lua script draws to Canvas4 via LuaCanvas
  blit Canvas4 → SDL_Texture:
    for each pixel index in Canvas4::getBuffer():
      rgb = palette.getColor(index)
      if not transparent: write ARGB to texture
  SDL_UpdateTexture → SDL_RenderCopy → SDL_RenderPresent
```

The blit step is the only place the Palette is consulted; drawing code never touches RGB.

### Scale / zoom

SDL2 allows the texture to be rendered at any size via SDL_RenderCopy dst rect.
A 2x or 4x integer scale renders Tomodachi's small display at desktop-legible size.
This is a one-liner in `SDL_RenderCopy` — no pixel processing change needed.

---

## Recommended Project Structure (new files only)

```
include/enjin2/
  graphics/
    palette.hpp             <-- NEW
  ui/
    input_event.hpp         <-- NEW
    input_system.hpp        <-- NEW

src/ui/
  input_system.cpp          <-- NEW

platforms/
  sdl2/
    main.cpp                <-- NEW
    sdl2_runner.hpp         <-- NEW
    sdl2_runner.cpp         <-- NEW
    sdl2_input.hpp          <-- NEW
    sdl2_input.cpp          <-- NEW
```

Zero existing files under `src/` or `include/` are deleted. Five files are added to
existing library locations; five are added under a new `platforms/sdl2/` directory.

---

## Data Flow: Frame Rendering with Palette

```
Lua update()
    │
    ▼
LuaCanvas::setPixel(x, y, index)      -- index 0-15, no RGB here
    │
    ▼
Canvas4::setPixel(x, y, Pixel4)       -- packed 4-bit buffer
    │
    ▼
SDL2Runner::blit()
    │  for each (x,y):
    │    index = canvas.getPixel(x, y)
    │    if !palette.isTransparent(index):
    │        rgb = palette.getColor(index)
    │        texture[x,y] = rgb
    ▼
SDL_UpdateTexture → SDL_RenderPresent
```

Drawing code never changes. Palette is applied once per frame at the output boundary.

---

## Data Flow: Input to Lua

```
SDL keyboard/gamepad events
    │
    ▼
SDL2InputSource::poll()               -- converts SDL events → InputEvent structs
    │
    ▼
InputSystem::update()                 -- dispatches to registered listeners
    │
    ▼
LuaBindings listener                  -- updates internal state table
    │
    ▼
Lua script: enjin.input.getButton(0)  -- queries state table
```

The Lua API exposes last-known state queries rather than per-frame event callbacks,
matching the Tomodachi usage pattern (check button state in update loop).

---

## Integration Points

### New vs Modified

| File | New or Modified | Notes |
|------|----------------|-------|
| `include/enjin2/graphics/palette.hpp` | NEW | No impact on existing code |
| `include/enjin2/ui/input_event.hpp` | NEW | No impact on existing code |
| `include/enjin2/ui/input_system.hpp` | NEW | No impact on existing code |
| `src/ui/input_system.cpp` | NEW | Added to enjin2_ui sources |
| `platforms/sdl2/*.cpp/hpp` | NEW | New executable, not a library |
| `include/enjin2/scripting/lua_platform.hpp` | MODIFIED | Add `SDL2_RUNNER` case |
| `src/scripting/lua_platform.cpp` | MODIFIED | Add `SDL2_RUNNER` implementations |
| `CMakeLists.txt` | MODIFIED | Add `ENJIN2_BUILD_SDL2` option + target |

Everything else (Canvas4, Canvas8, ICanvas, LuaCanvas, LuaBindings, existing UI, emscripten
bindings) is untouched.

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| SDL2 runner → enjin2_graphics | Direct include of palette.hpp + canvas.hpp | No library boundary — runner includes headers |
| SDL2 runner → enjin2_ui | Links InputSystem, InputEvent | InputSystem owns event dispatch |
| SDL2 runner → enjin2_lua | Links LuaScriptSystem | Lua still runs all game logic |
| SDL2InputSource → InputSystem | Pointer injection at startup | Platform provides source, system is generic |
| LuaBindings → InputSystem | InputSystem pointer held by LuaBindings | Lua queries state, not events |
| Palette → Canvas4 | No coupling — palette applied externally | Canvas stores indices only |

---

## Build Order

Dependencies flow strictly bottom-up:

```
1. enjin2_core         (no changes needed)
2. enjin2_graphics     (add palette.hpp — header-only, no build impact)
3. enjin2_ui           (add input_event.hpp + input_system.hpp + input_system.cpp)
4. enjin2_lua          (add SDL2_RUNNER to lua_platform.hpp / .cpp)
5. enjin2_sdl2         (new executable; requires all above + SDL2 system lib)
```

Recommended implementation order within the milestone:

| Step | What | Why First |
|------|------|-----------|
| 1 | `palette.hpp` | No deps, zero risk, unblocks SDL2 blit |
| 2 | `input_event.hpp` + `input_system.hpp` (interfaces only) | Contracts needed before platform impl |
| 3 | `input_system.cpp` | Implementation of InputSystem dispatch |
| 4 | `lua_platform.hpp/cpp` SDL2 case | Unblocks LuaScriptSystem in SDL2 build |
| 5 | `SDL2Runner` + `SDL2InputSource` | Needs palette + input + Lua all in place |
| 6 | LuaBindings input API | Needs InputSystem wired into runner |

---

## Anti-Patterns

### Anti-Pattern 1: Palette in the draw path

**What people do:** Modify `Canvas4::setPixel` to accept RGB and apply the palette on write.
**Why it's wrong:** Destroys the 4-bit index semantics, breaks Canvas4's packed storage model,
forces palette knowledge into the canvas, doubles memory if RGB is stored instead of index.
**Do this instead:** Keep Canvas4 as pure index storage. Apply palette only during the final
blit to the output surface (SDL_Texture, display DMA, WASM canvas).

### Anti-Pattern 2: Platform-specific input types leaking into enjin2_ui

**What people do:** Put `SDL_Keycode` or ESP32 GPIO pin numbers into InputEvent or InputSystem.
**Why it's wrong:** Pollutes the platform-agnostic library with platform specifics, breaks
compilation on other platforms, makes enjin2_ui depend on SDL2.
**Do this instead:** SDL2InputSource (in `platforms/sdl2/`) translates SDL events to InputEvent.
Only generic InputKind values cross the boundary into enjin2_ui.

### Anti-Pattern 3: VCV_RACK define used for SDL2

**What people do:** Add `#define VCV_RACK` to the SDL2 build to re-use the desktop Lua path.
**Why it's wrong:** VCV_RACK is a specific platform identity. Conflating it with SDL2 will
cause confusion when VCV Rack–specific behaviors are added later.
**Do this instead:** Add a distinct `SDL2_RUNNER` define; mirror VCV_RACK's Lua config
explicitly in the `lua_platform.hpp` SDL2_RUNNER branch.

### Anti-Pattern 4: New input system replaces existing mouse UI

**What people do:** Refactor the existing `InputComponent` / mouse handling to use the new
InputEvent model.
**Why it's wrong:** Out of scope, risks regression, existing UI components work correctly.
The new input abstraction is additive — for physical device input only.
**Do this instead:** Leave existing UI input untouched. New InputSystem is for physical
hardware inputs (buttons, pots, joystick). These are parallel, not competing systems.

---

## Sources

- Existing codebase analysis (HIGH — direct inspection of all headers and CMakeLists.txt)
- `include/enjin2/abstract/icanvas.hpp` — ICanvas interface (HIGH)
- `include/enjin2/graphics/canvas.hpp` — Canvas4/Canvas8 implementation (HIGH)
- `include/enjin2/graphics/canvas_esp32s3.hpp` — ESP32S3 canvas variant (HIGH)
- `include/enjin2/scripting/lua_platform.hpp` — LuaPlatform with VCV_RACK/ESP32 branches (HIGH)
- `include/enjin2/scripting/bindings.hpp` — LuaCanvas, LuaBindings, LuaScriptSystem (HIGH)
- `include/enjin2/ui/system.hpp` — SystemManager/EntityManager (HIGH)
- `include/enjin2/ui/component.hpp` — ComponentStorage, ECS patterns (HIGH)
- `CMakeLists.txt` — library targets and build structure (HIGH)
- `.planning/PROJECT.md` — v1.3 requirements and constraints (HIGH)

---

*Architecture research for: enjin2 v1.3 palette, SDL2 runner, input abstraction*
*Researched: 2026-02-23*
