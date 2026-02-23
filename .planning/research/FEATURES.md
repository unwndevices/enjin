# Feature Research

**Domain:** enjin2 v1.3 — Palette System, SDL2 Desktop Runner, Input Abstraction
**Researched:** 2026-02-23
**Confidence:** HIGH

## Context

This research covers the three new features for the v1.3 Tomodachi Readiness milestone only.
Existing features (Canvas4/Canvas8, sprites, blend modes, Lua scripting, WASM/ESP32/VCV Rack runners)
are already built and out of scope here.

Existing codebase state relevant to these features:
- `Pixel4` stores values 0-15 as grayscale — currently no RGB color mapping exists
- `Canvas4` / `Canvas8` are statically allocated templates with no palette concept
- `InputSystem` in `ui/systems.hpp` handles only mouse events (not physical controls)
- `LuaPlatform` guards are `VCV_RACK` and `ESP32` — no SDL2 platform defined yet
- CMake targets: `enjin2_core`, `enjin2_graphics`, `enjin2_ui`, `enjin2_lua`, `enjin2_wasm`

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features that must exist for each new subsystem to be considered complete. Missing these = the
feature is unusable or broken relative to its stated purpose.

#### Palette System

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **16-entry RGB color table** | Indexed palette requires a map from 4-bit index to display color | LOW | `uint32_t palette[16]` or `RGB888 palette[16]`; static allocation, no heap. Index 15 = transparent by convention. |
| **Lookup at display/present time** | Pixel values on canvas stay as 4-bit indices; RGB only resolved when pushing to screen | LOW | Display loop reads `Pixel4.value`, looks up `palette[value]` to get RGB. Canvas data never changes when palette changes. |
| **Index 15 as transparent** | Tomodachi sprites need transparency; preserves existing `Colors::BLACK = Pixel4(0)` | LOW | Index 15 reserved as transparent/skip during blit. Indices 0–14 are usable colors. |
| **Palette swap at runtime** | Recolor sprites without redrawing (teams, states, damage flash) | LOW | Update `palette[n] = new_color`; next present uses new color automatically. No re-render needed. |
| **Default palette defined in code** | First run must display something sensible without user setup | LOW | PICO-8's 16-color palette or a custom set. Should be a constexpr array in the palette header. |
| **Lua-accessible palette API** | Lua scripts need to set/get palette colors to drive visual behavior | MEDIUM | Expose `setPaletteColor(index, r, g, b)` and `getPaletteColor(index)` through `LuaCanvas` bindings. |

#### SDL2 Desktop Runner

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Window creation and main loop** | SDL2 runner needs a window to display into | LOW | `SDL_CreateWindow` + `SDL_CreateRenderer` + `SDL_PollEvent` event loop. Standard SDL2 pattern. |
| **Canvas-to-texture blit with palette** | Must convert enjin2 Canvas4 (indexed) to SDL2 RGB texture for display | MEDIUM | Read each `Pixel4`, look up `palette[index]`, write RGB pixel to SDL2 streaming texture, then `SDL_RenderCopy`. |
| **Integer pixel scaling** | Tomodachi's pixel display is small (e.g., 128x128); desktop window needs to be visible | LOW | `SDL_RenderSetLogicalSize` or explicit dst rect scale. Nearest-neighbor only — `SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")`. |
| **Frame timing / delta time** | Update loop must provide stable delta time to Lua and components | LOW | `SDL_GetPerformanceCounter` before/after frame; pass delta as `float` seconds to engine update. |
| **Clean shutdown** | Resources must be freed on exit | LOW | `SDL_DestroyTexture`, `SDL_DestroyRenderer`, `SDL_DestroyWindow`, `SDL_Quit`. Standard cleanup. |
| **Lua scripting integration** | SDL2 runner is a development platform — Lua must work same as other platforms | MEDIUM | Reuse existing `LuaEngine` + `LuaCanvas`. SDL2 platform gets `ENABLE_FILE_IO = true` (same as VCV Rack). |
| **CMake target for SDL2 runner** | Must integrate with existing CMake build system | LOW | New `enjin2_sdl2` target; `find_package(SDL2 REQUIRED)`; optional via `ENJIN2_BUILD_SDL2=ON`. |

#### Input Abstraction

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Unified input state struct** | Game logic must not care which platform provides input | LOW | `InputState` struct with: buttons (bitmask or array), analog axes (float normalized -1..1 or 0..1). |
| **Button pressed / held / released** | Edge-detection is required for one-shot actions vs. held actions | LOW | Per-button: `justPressed`, `held`, `justReleased`. Computed by diffing previous and current raw state. |
| **Analog axis for pots/joysticks** | Tomodachi has potentiometers and joysticks — not just digital buttons | LOW | `float axes[N]` in `InputState`. SDL2 maps joystick axes; ESP32 maps ADC reads; keyboard maps to 0/1. |
| **SDL2 keyboard-to-button mapping** | Desktop runner needs keyboard as input source during development | LOW | Define a default mapping: arrow keys = d-pad, Z/X = A/B, Enter = start. Configurable at startup. |
| **ESP32 / platform injection** | Platform-specific code (ADC, GPIO) feeds the abstraction from outside | MEDIUM | Platform provides a function or callback that populates `InputState` each frame. Engine consumes it generically. |
| **Lua input query API** | Lua scripts must be able to read button and axis state | MEDIUM | Expose `isButtonHeld(n)`, `isButtonJustPressed(n)`, `getAxis(n)` through scripting bindings. |

---

### Differentiators (Competitive Advantage)

Features that go beyond the minimum and add meaningful value specific to enjin2's design goals.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Two-stage palette (draw + screen palette)** | PICO-8-style: draw palette remaps indices at draw time; screen palette maps to RGB at display time. Enables sprite recoloring without index changes. | MEDIUM | Draw palette: `drawPalette[16]` remaps `Pixel4` index before writing to canvas. Screen palette: maps screen index to RGB at present. Adds expressive power without canvas format change. |
| **Per-entry transparency flag in draw palette** | Sprite transparency becomes a palette property, not hardcoded to index 0. Any color can be transparent. | LOW | `bool transparent[16]` in draw palette struct. Consistent with PICO-8 model; avoids hardcoded matte value scattered through draw calls. |
| **SDL2 + Lua REPL / hot-reload** | Scripts can be reloaded from disk without restarting the runner — critical for fast development iteration on Tomodachi UI | MEDIUM | `LuaEngine::executeFile` already exists. SDL2 runner watches file mtime or listens for key shortcut to reload. Immediate feedback loop. |
| **Keyboard mapping table in Lua** | Lua scripts can redefine which keyboard keys map to which logical buttons at runtime | LOW | `input.setKeyMap(button_index, sdl_scancode)`. Makes SDL2 runner customizable without recompile. |
| **Input event callbacks in Lua** | Beyond polling, scripts can register `onButtonPressed` callbacks for event-driven UI | MEDIUM | Complements polling API. Reduces boilerplate in Lua scripts for UI widgets (ButtonDial already exists as a component). |

---

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **RGB pixel storage in canvas** | "Just store RGB directly so we don't need palette lookup" | Triples memory per pixel (3 bytes vs 4 bits). Destroys ESP32 viability. Violates zero-dynamic-allocation constraint for large canvases. | Keep canvas as 4-bit indexed. Resolve to RGB only at display time in the runner. |
| **SDL2 software renderer fallback** | "What if hardware acceleration isn't available?" | Adds complexity for an edge case that doesn't apply to the Tomodachi use case. SDL2 always has some renderer path. | Use `SDL_RENDERER_ACCELERATED | SDL_RENDERER_SOFTWARE` flag combo; SDL2 falls back automatically. |
| **Full SDL2 event system exposed to Lua** | "Let Lua handle SDL events directly for maximum flexibility" | Creates platform coupling in scripts. Scripts written for SDL2 break on ESP32. | Expose only the platform-agnostic `InputState` API. SDL2-specific events stay in the C++ runner layer. |
| **Dynamic palette size (more than 16 colors)** | "17 or 32 colors would give more artistic freedom" | `Pixel4` is 4 bits — 16 values is the hardware constraint. Larger palettes require 8-bit canvas, different memory model, different texture format. | Use `Canvas8` for 256-color indexed work. Keep `Canvas4` strictly 16-color. |
| **Mouse/touch as primary input** | "Add mouse support to the input abstraction for desktop dev" | Tomodachi has no mouse. Adding it creates mismatched API surface between platforms. Existing `InputSystem` in `ui/systems.hpp` already handles mouse for VCV Rack UI. | Keep physical input abstraction (buttons/axes) separate from mouse UI. Use existing `InputSystem` for mouse, new `InputState` for physical controls. |
| **SDL2 audio integration** | "While we have SDL2, might as well add audio" | Out of scope: PROJECT.md explicitly defers MIDI/audio to Tomodachi-side. SDL_mixer adds a large dependency for no milestone value. | Audio is Tomodachi-side concern. Keep SDL2 runner graphics + input only. |
| **Cross-platform input configuration files** | "Save button remapping to a config file" | Adds file I/O concerns, path resolution across platforms, serialization. Tomodachi uses fixed physical controls — remapping is a desktop-only convenience. | Hardcode a sensible default keyboard mapping. Expose it as a Lua-settable table for the SDL2 runner only. |

---

## Feature Dependencies

```
[Palette System: 16-entry RGB table]
    └──requires──> [Pixel4 / Canvas4 (existing)]
    └──enables──> [Canvas-to-texture blit with palette]

[Canvas-to-texture blit with palette]
    └──requires──> [Palette System: 16-entry RGB table]
    └──requires──> [SDL2 window + renderer]
    └──enables──> [SDL2 desktop runner (complete)]

[SDL2 desktop runner]
    └──requires──> [Canvas-to-texture blit with palette]
    └──requires──> [Frame timing / delta time]
    └──requires──> [Input abstraction: SDL2 keyboard mapping]
    └──requires──> [Lua integration (existing LuaEngine)]

[Input abstraction: unified InputState]
    └──standalone (no existing feature required)
    └──enables──> [SDL2 keyboard-to-button mapping]
    └──enables──> [ESP32 input injection]
    └──enables──> [Lua input query API]

[Lua palette API]
    └──requires──> [Palette System]
    └──requires──> [LuaCanvas bindings (existing)]

[Lua input query API]
    └──requires──> [Input abstraction: unified InputState]
    └──requires──> [LuaEngine / scripting bindings (existing)]

[Two-stage palette (draw + screen)]
    └──requires──> [Palette System: 16-entry RGB table]
    └──enhances──> [Palette swap at runtime]

[SDL2 hot-reload]
    └──requires──> [SDL2 desktop runner]
    └──requires──> [LuaEngine::executeFile (existing)]
```

### Dependency Notes

- **Palette system is prerequisite for SDL2 runner**: The runner's blit step reads the palette to convert 4-bit canvas to RGB texture. Palette must exist before runner works visually.
- **Input abstraction is independent**: Can be designed and tested without the palette or SDL2 runner. Depends only on the existing core types (`types.hpp`).
- **Lua APIs depend on both their feature and existing bindings**: `LuaCanvas` already has canvas bindings — palette and input APIs extend it rather than replace it.
- **Two-stage palette enhances but does not block v1**: Single-stage (screen palette only) is sufficient for MVP. Draw palette is a v1.x add-on.
- **SDL2 CMake target is separate**: Optional `ENJIN2_BUILD_SDL2=ON` mirrors existing `ENJIN2_BUILD_LUA` pattern. Must not affect ESP32 or WASM builds.

---

## MVP Definition

### Launch With (v1.3 — Tomodachi Readiness)

Minimum needed to enable Tomodachi integration development on desktop.

- [ ] **16-entry palette with default colors** — Pixel4 indices map to RGB at display time
- [ ] **Index 15 = transparent** — Consistent with existing blit/matte convention
- [ ] **Runtime palette swap** — `setPaletteColor(index, r, g, b)` at minimum
- [ ] **SDL2 window + game loop** — Window, renderer, event polling, clean shutdown
- [ ] **Canvas4-to-RGB texture blit** — Indexed canvas presented via palette lookup
- [ ] **Integer pixel scaling (nearest-neighbor)** — Small canvas readable on desktop monitor
- [ ] **Lua scripting in SDL2 runner** — `LuaEngine` runs same Lua scripts as other platforms
- [ ] **Unified InputState struct** — Buttons (bitmask) + analog axes (float array)
- [ ] **Button edge detection** — `justPressed`, `held`, `justReleased` per button
- [ ] **SDL2 keyboard-to-button mapping** — Default: arrows, Z/X, Enter
- [ ] **Lua input polling API** — `isButtonHeld(n)`, `isButtonJustPressed(n)`, `getAxis(n)`
- [ ] **CMake optional SDL2 target** — `ENJIN2_BUILD_SDL2=ON/OFF`, does not affect other targets

### Add After Validation (v1.3.x)

Features to add once core v1.3 is confirmed working end-to-end.

- [ ] **Lua hot-reload in SDL2 runner** — Triggered by key shortcut (e.g., F5) or file watch
- [ ] **Draw palette (index remapping)** — Two-stage palette for sprite recoloring without index changes
- [ ] **Per-entry transparency flag in draw palette** — Any index can be made transparent, not just index 0
- [ ] **Lua keyboard mapping table** — `input.setKeyMap(button, scancode)` for SDL2 runner

### Future Consideration (v1.4+)

Defer until v1.3 is validated and Tomodachi integration is underway.

- [ ] **Lua input event callbacks** — `onButtonPressed` / `onButtonReleased` callbacks
- [ ] **Canvas8 palette support** — 256-color indexed palette for `Canvas8` (different use case)
- [ ] **Multi-layer composition** — Already deferred per PROJECT.md
- [ ] **Input device hot-plug** — Connect/disconnect gamepad at runtime via SDL2 events

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| 16-entry palette + default | HIGH | LOW | P1 |
| Canvas4-to-RGB texture blit | HIGH | MEDIUM | P1 |
| SDL2 window + game loop | HIGH | LOW | P1 |
| Integer pixel scaling | HIGH | LOW | P1 |
| Lua scripting in SDL2 runner | HIGH | LOW | P1 |
| Unified InputState struct | HIGH | LOW | P1 |
| Button edge detection | HIGH | LOW | P1 |
| SDL2 keyboard mapping | HIGH | LOW | P1 |
| Lua input polling API | HIGH | MEDIUM | P1 |
| CMake SDL2 target | HIGH | LOW | P1 |
| Runtime palette swap | MEDIUM | LOW | P1 |
| Lua palette API | MEDIUM | MEDIUM | P1 |
| Lua hot-reload | MEDIUM | LOW | P2 |
| Draw palette (two-stage) | MEDIUM | MEDIUM | P2 |
| Per-entry transparency flag | MEDIUM | LOW | P2 |
| Lua keyboard mapping table | LOW | LOW | P2 |
| Lua input event callbacks | MEDIUM | MEDIUM | P3 |
| Canvas8 palette support | LOW | MEDIUM | P3 |

**Priority key:**
- P1: Must have for v1.3 launch (Tomodachi integration enablement)
- P2: Should have, add before or shortly after launch
- P3: Nice to have, future milestone

---

## Competitor Feature Analysis

These are reference implementations consulted during research — not competitors in a market sense,
but systems that solved the same design problems.

| Feature | PICO-8 | TIC-80 | Our Approach |
|---------|--------|--------|--------------|
| Palette size | 16 colors (fixed) | 16 colors (fixed default) | 16 colors — `Pixel4` is 4-bit, this is the hardware constraint |
| Transparent index | Index 0 (draw palette flag) | Color 0 = transparent | Index 15 = transparent; consistent with existing Canvas4::blit() matte convention |
| Palette swap | Two-stage (draw + screen palette) | Screen palette only | Start with screen palette only (index → RGB); add draw palette in v1.3.x |
| Canvas storage | 4-bit indexed | 4-bit indexed | `Pixel4` packed in `Canvas4` (existing) — identical model |
| Input API | `btn(n)`, `btnp(n)` (polling) | Same pattern | `isButtonHeld(n)`, `isButtonJustPressed(n)` in Lua — same pattern |
| Input source | Keyboard / gamepad | Keyboard / gamepad | Platform-agnostic `InputState` injected per platform; SDL2 maps keyboard |
| Desktop runner | Runtime (sandboxed) | Runtime (sandboxed) | SDL2 runner as development tool — not sandboxed, full file access |
| Scripting language | Lua (subset) | Lua (subset) | Lua via LuaJIT — already integrated (existing) |

---

## Sources

- PICO-8 Palette documentation (PICO-8 Wiki): https://pico-8.fandom.com/wiki/Palette — Two-stage palette system (draw palette + screen palette), transparency flag per index (HIGH confidence)
- PICO-8 Manual (Lexaloffle): https://www.lexaloffle.com/dl/docs/pico-8_manual.html — Input API (`btn`, `btnp`), 16-color constraint (HIGH confidence)
- SDL2 indexed texture streaming: https://discourse.libsdl.org/t/indexed-texture-streaming-with-custom-palette/25084 — `SDL_PIXELFORMAT_INDEX8` streaming texture approach (MEDIUM confidence — archived forum post)
- SDL2 pixel art scaling: https://discourse.libsdl.org/t/scaling-resolution-and-pixel-art/21342 — Logical resolution and nearest-neighbor scaling (HIGH confidence)
- SDL2 game loop patterns: https://thelinuxcode.com/sdl2-in-c-and-c-2026-practical-patterns-real-examples-and-the-mental-model-that-makes-it-click/ — Init/loop/shutdown, delta time via `SDL_GetPerformanceCounter` (MEDIUM confidence)
- SDL2 framebuffer rendering: https://codersplate.wordpress.com/2025/08/01/creating-a-framebuffer-renderer-in-c-with-sdl2/ — Raw pixel manipulation to SDL2 surface/texture (MEDIUM confidence)
- SDL2 GameController API: https://blog.rubenwardy.com/2023/01/24/using_sdl_gamecontroller/ — Unified controller abstraction over raw joystick (HIGH confidence)
- Handmade Penguin input abstraction: https://davidgow.net/handmadepenguin/ch6.html — Keyboard + gamepad unified input pattern (MEDIUM confidence)
- Fantasy console palette survey: https://lospec.com/palette-list/tag/fantasyconsole — Reference palettes (16-color, PICO-8, etc.) (HIGH confidence)
- enjin2 codebase direct inspection: `include/enjin2/graphics/canvas.hpp`, `include/enjin2/core/types.hpp`, `include/enjin2/ui/systems.hpp`, `include/enjin2/scripting/lua_platform.hpp` — Existing API surface, constraints, and integration points (HIGH confidence)

---
*Feature research for: enjin2 v1.3 — Palette System, SDL2 Desktop Runner, Input Abstraction*
*Researched: 2026-02-23*
*Confidence: HIGH*
