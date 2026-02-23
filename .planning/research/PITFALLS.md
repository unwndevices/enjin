# Pitfalls Research

**Domain:** Adding palette system, SDL2 desktop runner, and input abstraction to enjin2
**Researched:** 2026-02-23
**Confidence:** HIGH (based on direct codebase analysis + verified external sources)

---

## Critical Pitfalls

### Pitfall 1: Palette as Display-Time Lookup Breaks Existing Canvas API Consumers

**What goes wrong:**
The existing `LuaBindings`, `LuaCanvas`, `emscripten_bindings.cpp`, and all drawing code pass raw `uint8_t` color values directly as Pixel4 indices (0–15). If the palette is applied at the wrong layer — e.g., inside `setPixel()` or `LuaBindings::lua_setColor()` — then the canvas stores RGB values instead of palette indices, breaking the packed 4-bit storage format entirely. Canvas4 packs two pixels per byte; storing anything other than a 0–15 index corrupts the packing.

**Why it happens:**
Developers instinctively apply palette lookup at draw time (when the color is "used") rather than at blit/present time (when pixels are sent to the display). The temptation is to write `setPixel(x, y, palette[color])` in the draw path. But the canvas is a framebuffer of indices, not colors — the palette is a display-time mapping, not a drawing-time mapping.

**How to avoid:**
The palette must live entirely outside the canvas and be applied only at present/blit time. The canvas stores indices 0–15. The palette maps those indices to RGB at the moment the framebuffer is read out to the display (SDL texture update, WASM canvas blit, or hardware display driver). Never modify `setPixel` or `clear` to accept palette-resolved values.

**Warning signs:**
- Canvas colors "look right" in one rendering path but wrong in another
- `getPixel()` returns unexpected values when round-tripped through set/get
- ESP32 display and SDL2 display show different colors for the same script
- Packed buffer `clear()` produces wrong fill patterns (clear packs both nibbles with the same value)

**Phase to address:**
Palette definition phase — establish the lookup-at-present-time contract before writing any palette code.

---

### Pitfall 2: SDL2 Pixel Format Mismatch Produces Silently Wrong Colors

**What goes wrong:**
SDL2's texture pixel formats have a naming convention that describes bit-packed integer layout, not byte-order in memory. On little-endian x86/ARM (all SDL2 desktop targets), `SDL_PIXELFORMAT_ARGB8888` and `SDL_PIXELFORMAT_RGBA8888` are different memory layouts. Using the wrong format constant when calling `SDL_UpdateTexture()` with your RGB output buffer produces colors that appear subtly or dramatically wrong — red and blue channels swap, or alpha is misinterpreted — with no error or warning from SDL.

Additionally, `SDL_CreateTexture()` may silently substitute a different format if the requested format is unsupported by the backend, making format verification essential.

**Why it happens:**
The RGBA/ARGB naming is counterintuitive. `SDL_PIXELFORMAT_RGBA32` is actually an endian-aware alias that resolves to `ABGR8888` on little-endian systems. Most developers assume "RGBA" means bytes in R, G, B, A order, which is wrong in SDL2's packed format model. The palette-to-SDL pipeline will likely use a simple `uint8_t[3]` or `uint32_t` RGB lookup; the byte order of that struct must exactly match the SDL texture format chosen.

**How to avoid:**
Choose `SDL_PIXELFORMAT_RGB24` for 24-bit output (explicit byte order, no endian surprises) or explicitly use `SDL_PIXELFORMAT_ARGB8888` and construct pixel values as `(0xFF << 24) | (r << 16) | (g << 8) | b`. Verify the chosen format immediately after texture creation by checking `SDL_QueryTexture()` and asserting the format matches. Do this verification in a startup check, not in the render loop.

**Warning signs:**
- Blue elements appear red or vice versa
- Colors correct on developer's machine but wrong on a different platform or driver
- `SDL_GetPixelFormatName()` returns a different name than the constant you passed
- Any use of `SDL_PIXELFORMAT_RGBA32` or `SDL_PIXELFORMAT_BGRA32` (endian-aliased constants)

**Phase to address:**
SDL2 runner implementation — add format verification at SDL2 runner startup before any rendering.

---

### Pitfall 3: SDL2 Texture Access Mode Mismatch Causes Rendering Failures

**What goes wrong:**
SDL2 textures have three access modes: `SDL_TEXTUREACCESS_STATIC`, `SDL_TEXTUREACCESS_STREAMING`, and `SDL_TEXTUREACCESS_TARGET`. A framebuffer-style renderer that updates pixel data every frame must use `SDL_TEXTUREACCESS_STREAMING`. Using `SDL_TEXTUREACCESS_STATIC` and calling `SDL_UpdateTexture()` every frame works on some drivers but is undefined behavior and fails silently on others. Additionally, mixing `SDL_LockTexture/SDL_UnlockTexture` with `SDL_UpdateTexture` on the same texture is incorrect — choose one path and use it consistently.

**Why it happens:**
SDL tutorials often use static textures for sprite loading and streaming textures for framebuffer rendering, but beginners conflate the two patterns. The rendering path for enjin2's SDL2 runner (expand palette, blit to texture, present) is unambiguously a streaming pattern, but if the texture is created with the wrong access mode, `SDL_UpdateTexture` may appear to work in debug builds while failing in release or on different hardware.

**How to avoid:**
Create the SDL2 framebuffer texture with `SDL_TEXTUREACCESS_STREAMING` and use `SDL_UpdateTexture()` exclusively. Do not use `SDL_LockTexture` in the same code path. Set `SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest")` before texture creation to preserve pixel-art appearance during window scaling.

**Warning signs:**
- Texture appears black or shows stale data on some systems but not others
- `SDL_LockTexture` returns a different pitch than expected
- Mixing lock and update paths in review

**Phase to address:**
SDL2 runner implementation — framebuffer texture creation and update loop.

---

### Pitfall 4: SDL2 Event Loop Blocks on Window Move/Resize (Windows OS)

**What goes wrong:**
On Windows, SDL2's event loop fully blocks the main thread while the user is dragging or resizing the window. This is a known SDL2 design issue caused by the Windows `WndProc` modal loop. For enjin2's SDL2 runner, which will be a tight game-loop (poll input → update Lua → render → present), a window drag lasting several seconds will stall the loop, potentially causing Lua scripts to accumulate large `dt` values or miss input events.

**Why it happens:**
SDL2 must pump the Windows message queue from the main thread. When the OS enters its modal resize loop, SDL2 has no way to interrupt it without platform-specific workarounds. This is not a bug developers introduce — it is a structural SDL2 constraint on Windows.

**How to avoid:**
For the enjin2 SDL2 runner (desktop development tool, not shipping firmware), the simplest mitigation is to clamp `dt` to a maximum value (e.g., `min(dt, 0.1f)` seconds) in the Lua update call. This prevents Lua scripts from seeing impossibly large time deltas after a window drag stall. Document this as a known desktop runner limitation. Do not attempt to fix it with threading — that introduces SDL OpenGL/rendering threading constraints and significant complexity.

**Warning signs:**
- Lua scripts produce "physics explosions" or other incorrect behavior after window manipulation
- Scripts that use `time()` for animation drift after window drag
- Any code that passes raw frame delta to Lua without clamping

**Phase to address:**
SDL2 runner game loop implementation — add dt clamping in the update step.

---

### Pitfall 5: Input Abstraction Leaks Platform Types Into Shared API

**What goes wrong:**
The input abstraction must work identically on SDL2 desktop (keyboard/mouse), ESP32 (GPIO buttons, ADC for pots), and WASM (JavaScript events via Emscripten). If the shared input interface uses SDL2 types (e.g., `SDL_Keycode`, `SDL_GameControllerAxis`) or ESP32 types (e.g., `gpio_num_t`) in its public API, the interface cannot be included in the core library without platform guards, defeating the purpose of the abstraction.

**Why it happens:**
It is natural to design the input abstraction starting from the concrete platform (SDL2 desk runner, since that is what is being built) and extract an interface afterward. This results in the SDL2 concrete types bleeding into the header. The existing codebase pattern (VCV_RACK compile-time definition) suggests the team already understands platform switching, but the pattern must be extended correctly to input.

**How to avoid:**
Define the input interface in terms of enjin2-native logical types only: `ButtonID` (enum of logical inputs: A, B, encoder click), `AxisID` (enum: encoder delta, pot0–potN, joystick axes), and value types (`bool` for buttons, `float` normalized for axes). The interface header must include no SDL2, ESP32, or WASM-specific headers. Each platform provides a concrete implementation class in a platform-gated source file (analogous to how `emscripten_bindings.cpp` is only compiled for WASM).

**Warning signs:**
- `#include <SDL.h>` or `#include "driver/gpio.h"` appearing in an input interface header
- `SDL_Keycode` or `gpio_num_t` in a shared struct definition
- Input polling function that can only be called after SDL2 initialization
- Input header that cannot be included in an ESP32-only build without compile errors

**Phase to address:**
Input abstraction design phase — establish the interface before implementing any platform backend.

---

### Pitfall 6: Button Debouncing Omitted in Embedded Input Backend

**What goes wrong:**
Physical buttons on the ESP32 (Tomodachi's A, B, encoder click) bounce electrically, producing multiple rapid transitions when pressed once. Without debouncing, a single physical press registers as multiple events — `on_button("A", true)` fires 3–5 times in a single tick. This makes any Lua app that responds to button press unusable on hardware, even if it works perfectly in SDL2 (keyboard events do not bounce).

**Why it happens:**
The SDL2 backend is built first (desktop development). SDL2 keyboard events are already debounced by the OS. When the ESP32 backend is implemented later, developers copy the SDL2 polling pattern (check state each tick) without adding the hardware-required debounce logic. The discrepancy only surfaces on device.

**How to avoid:**
The input abstraction layer must include debounce as a built-in concern, not an ESP32-only concern. The interface should expose edge-detection semantics: `wasJustPressed()` and `wasJustReleased()` (true for exactly one tick after the transition), not raw `isDown()` state. Implementing edge detection in the shared logic layer means the SDL2 backend also benefits — keyboard auto-repeat is similarly suppressed.

**Warning signs:**
- Input interface exposes only `isDown()` with no edge semantics
- ESP32 backend does not track previous-tick state per input
- No configurable debounce interval in the embedded backend
- Lua apps receive multiple `on_button` events from a single physical press

**Phase to address:**
Input abstraction implementation — design edge-detection semantics into the shared interface from the start.

---

### Pitfall 7: SDL2 CMake Integration Conflicts with Existing VCV_RACK Build

**What goes wrong:**
The existing CMakeLists.txt compiles the main library with `VCV_RACK` defined globally on `enjin2_core` and `enjin2_graphics`. This definition is used to conditionally exclude `#include <Arduino.h>` in `canvas.hpp`. Adding an SDL2 desktop target requires a build configuration that is neither VCV_RACK nor WASM nor ESP32. If SDL2 is integrated carelessly by adding it to the existing library targets, the `VCV_RACK` define may suppress code that the SDL2 runner needs, or SDL2 headers may conflict with the current include structure.

Additionally, the existing CMake uses the pre-modern `${SDL2_LIBRARIES}` variable-based approach in many SDL2 tutorials, while modern SDL2 ships with imported targets (`SDL2::SDL2`). Mixing these causes linker errors on some platforms, particularly on Linux where the imported target handles `-lm` and `-lpthread` automatically but the variable-based approach does not.

**Why it happens:**
Adding SDL2 to an existing, working CMake project is "just a few lines" — but those lines interact with existing definitions and target structure in non-obvious ways. The WASM build already has a separate `enjin2_wasm` executable target. The SDL2 runner should follow the same pattern (separate executable target, not a modification to the core library), but this is not obvious to someone who just wants SDL2 working quickly.

**How to avoid:**
Add SDL2 as a new `enjin2_sdl` executable target (mirroring `enjin2_wasm`), gated behind a new `ENJIN2_BUILD_SDL` option. Use `find_package(SDL2 REQUIRED CONFIG)` with the modern imported target `SDL2::SDL2`. Do not add `SDL2_INCLUDE_DIRS` to the core library targets. Do not define `VCV_RACK` on the SDL2 target — use a new `ENJIN2_PLATFORM_SDL` define instead.

**Warning signs:**
- SDL2 headers added to `enjin2_core` or `enjin2_graphics` include directories
- `VCV_RACK` defined on the SDL2 executable target
- Using `${SDL2_LIBRARIES}` variable instead of `SDL2::SDL2` imported target
- SDL2 target shares include directories with the core library

**Phase to address:**
SDL2 CMake integration — first step before writing any SDL2 C++ code.

---

### Pitfall 8: Lua Color API Semantic Ambiguity After Palette Addition

**What goes wrong:**
The current Lua API passes colors as raw integers 0–15 (e.g., `setColor(15)` = white, `setColor(0)` = black). After adding a palette, color index 5 might map to red in one palette and blue in another. The existing `LuaBindings::currentColor` stores the raw index as `uint8_t`. If palette color registration functions are added to Lua (e.g., `setPalette(index, r, g, b)`), Lua scripts will mix two distinct concepts — "index into my palette" and "literal grayscale 0–15" — unless the distinction is explicit in the API design.

The Tomodachi design doc specifies "15 colors + transparent" which exactly fills indices 1–15 (index 0 = transparent). The current `Colors::BLACK = Pixel4(0)` definition conflicts with "index 0 = transparent." Any existing Lua scripts using `setColor(0)` for black will behave differently after palette assignment.

**Why it happens:**
The grayscale system (0=black, 15=white) and the indexed palette system (0=transparent, 1–15=user colors) both use the same 0–15 value range but with incompatible semantics for index 0. The existing code has `Colors::BLACK = Pixel4(0)` which becomes transparent after palette introduction if the Tomodachi convention is followed.

**How to avoid:**
Decide on the transparent-index convention before writing any palette code. Two options: (a) index 0 = transparent, indices 1–15 = colors (Tomodachi spec), requiring existing scripts to change `setColor(0)` to a non-zero background color; (b) index 15 = transparent, indices 0–14 = colors (preserves existing scripts). Option (a) aligns with the design doc and should be adopted, but must be documented clearly and any existing examples updated. Do not leave this decision implicit.

**Warning signs:**
- `Colors::BLACK = Pixel4(0)` still present after palette is added
- Existing examples use `clear(0)` or `setColor(0)` without updating for palette semantics
- `setPalette()` and `setColor()` both accept integer 0–15 with no type distinction
- WASM and SDL2 runners render index 0 differently (one clears to black, one to transparent)

**Phase to address:**
Palette design phase — resolve index 0 semantics in the design document before writing code.

---

### Pitfall 9: WASM Emscripten Bindings Not Updated for Palette

**What goes wrong:**
`emscripten_bindings.cpp` manually exposes `Canvas4<128, 128>` operations to JavaScript. After adding a palette, the WASM rendering path (JavaScript calls `getCanvasData128()` to get raw pixel indices, then renders to Canvas2D) must apply the palette in the JavaScript layer. If the palette data is not exposed through the WASM bindings, the JavaScript side has no way to know which RGB color each index maps to, and will render raw 0–15 indices as grayscale (or incorrectly scaled values).

The existing `getCanvasData128()` returns a `val(typed_memory_view(...))` of raw indices. This is correct for an indexed framebuffer — but the consuming JavaScript must now query the palette to render correctly.

**Why it happens:**
The palette is added in C++ and tested with the SDL2 runner. The SDL2 runner applies the palette natively in C++. The WASM path is not revisited because "the canvas still works." The JavaScript side continues to treat indices as grayscale until someone notices the WASM renderer shows wrong colors.

**How to avoid:**
When designing the palette C++ API, explicitly include a "get palette as flat array" function (e.g., `getPaletteRGB()` returning 16×3 bytes) and expose it through Emscripten bindings. The JavaScript renderer switches from grayscale mapping to palette lookup. This must be part of the palette phase, not deferred.

**Warning signs:**
- SDL2 runner shows correct colors; WASM shows grayscale or wrong colors
- No palette getter exposed in `emscripten_bindings.cpp`
- JavaScript renderer still uses `value * 17` (grayscale scaling) after palette added
- `getCanvasData128()` return type unchanged after palette introduction

**Phase to address:**
Palette phase — update Emscripten bindings in the same phase as the C++ palette implementation.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Apply palette in `setPixel()` | "Just works" initially | Corrupts packed 4-bit storage; breaks all existing drawing code | Never |
| Expose `SDL_Keycode` in input interface | Fast SDL2 backend | Cannot compile on ESP32 or WASM without SDL2 headers | Never |
| Single hardcoded palette | Simpler initial implementation | Cannot support multiple palettes per app; blocks Tomodachi app isolation | Only if scope explicitly excludes multiple palettes |
| Skip debounce in embedded input | Faster initial ESP32 integration | Unusable physical buttons; breaks every app on device | Never |
| Add SDL2 headers to core CMake targets | Faster build setup | Contaminates core library with SDL2 dependency; breaks clean builds | Never |
| Store palette globally as a static array | Zero allocation; works immediately | Single palette per process; blocks multi-app Tomodachi shell | Acceptable for v1.3 single-app scope |

---

## Integration Gotchas

Common mistakes when connecting these three new features to the existing system.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Palette + Canvas4 | Palette lookup inside `setPixel()` | Palette is display-time only; canvas stores raw indices |
| Palette + LuaBindings | `currentColor` stores RGB after palette call | `currentColor` always stores palette index 0–15 |
| SDL2 + existing CMake | Modify core library targets | New `enjin2_sdl` executable target with `ENJIN2_BUILD_SDL` option |
| SDL2 + WASM | Share render loop structure | SDL2 uses `while(running)` loop; WASM uses `emscripten_set_main_loop()` — fundamentally different |
| SDL2 texture format | Use `SDL_PIXELFORMAT_RGBA32` | Use `SDL_PIXELFORMAT_RGB24` or explicit `ARGB8888` with verified byte order |
| Input + Lua | Register input polling C functions that read SDL state directly | Input interface decoupled from SDL2; Lua calls `input.isDown()` which delegates to the platform backend |
| Input + existing `InputComponent` | Extend existing mouse-only `InputComponent` | New `InputSystem` for logical device inputs; `InputComponent` is UI widget input — different concern |
| SDL2 + VCV_RACK define | Add `VCV_RACK` to SDL2 target | SDL2 target uses new `ENJIN2_PLATFORM_SDL` define; `VCV_RACK` must not be defined |

---

## Performance Traps

Patterns that work initially but degrade.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Full palette expansion per pixel in render loop | Frame rate drops; SDL2 runner can't sustain 30 Hz | Pre-expand entire framebuffer to RGB in one pass; don't expand per-setPixel call | Always — palette expansion at draw-call time is O(calls), not O(pixels) |
| `SDL_UpdateTexture()` with non-streaming texture | Silent corruption or stall on some drivers | Always create framebuffer texture as `SDL_TEXTUREACCESS_STREAMING` | Platform/driver dependent; may be frequent |
| Input polling inside drawing code | Inconsistent input state mid-frame | Poll all input once per tick at start of game loop; cache state | Any multi-input combination logic |
| Allocating palette on the heap | Forbidden by no-dynamic-allocation constraint | 16-entry `uint32_t[16]` static array — trivially static | Immediately on ESP32 |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Palette system:** Palette defined in C++ but not exposed to Lua — verify `setPalette(index, r, g, b)` callable from scripts
- [ ] **Palette system:** Palette applied in SDL2 runner but not in WASM blit — verify both rendering paths use the palette
- [ ] **Palette system:** Index 0 transparent convention documented and all existing examples updated from `setColor(0)` = black
- [ ] **SDL2 runner:** Window creates and renders but input not connected — verify keyboard events reach Lua `on_button` callbacks
- [ ] **SDL2 runner:** `SDL_TEXTUREACCESS_STREAMING` confirmed on framebuffer texture (not just compiling without error)
- [ ] **SDL2 runner:** `dt` clamped in game loop — verify scripts don't explode after window drag
- [ ] **SDL2 CMake:** `ENJIN2_BUILD_SDL=OFF` still produces a clean build with zero SDL2 references — verify with clean cmake
- [ ] **Input abstraction:** Interface header has zero platform-specific includes — verify compiles with `-DESP32` and no SDL2 headers available
- [ ] **Input abstraction:** Edge detection (`wasJustPressed`, `wasJustReleased`) implemented, not just `isDown` — verify with repeated press test
- [ ] **Input + Lua:** `on_button` callback fires exactly once per physical press in SDL2 runner — verify with key-hold test

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Palette applied at setPixel (wrong layer) | HIGH | Revert palette from canvas layer entirely; reimplement as display-time lookup in runner |
| Wrong SDL2 pixel format (silently bad colors) | LOW | Add format verification assert at startup; fix the constant; re-test |
| SDL2 texture access mode wrong | LOW | Recreate texture with `SDL_TEXTUREACCESS_STREAMING`; update creation code |
| SDL2 types in input interface | MEDIUM | Extract abstract interface; move SDL2 types to implementation file; recompile all consumers |
| Missing debounce (button bounce on device) | LOW | Add debounce delay to embedded input backend; test with physical device |
| WASM palette not updated | LOW | Add palette getter to bindings; update JavaScript renderer to use palette |
| Index 0 semantic conflict (black vs transparent) | MEDIUM | Audit all existing examples for `setColor(0)` usage; update to use explicit background color |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Palette applied at wrong layer | Palette design phase | Canvas round-trip test: set index, get index, confirm unchanged |
| SDL2 pixel format mismatch | SDL2 runner — startup verification | Assert `SDL_QueryTexture` format matches expected at runtime |
| SDL2 texture access mode | SDL2 runner — texture creation | Create texture with `STREAMING`; verify in code review |
| SDL2 event loop blocking | SDL2 game loop implementation | Add dt clamp; test with window drag while script runs |
| Input interface platform leakage | Input abstraction design | Compile input header with `ESP32` defined and no SDL2 present |
| Missing button debounce | Input abstraction implementation | Verify `wasJustPressed()` returns true exactly once per key-down event |
| SDL2 CMake contamination | SDL2 CMake integration | Clean build with `ENJIN2_BUILD_SDL=OFF`; confirm no SDL references in core |
| Lua color API ambiguity | Palette design phase | Document index 0 convention; update all examples before implementing |
| WASM bindings not updated | Palette implementation | Both SDL2 and WASM renderers show identical colors for same script |

---

## Sources

- Codebase analysis: `src/bindings/emscripten_bindings.cpp`, `src/scripting/bindings.cpp`, `src/graphics/canvas.hpp` (2026-02-23)
- Codebase analysis: `CMakeLists.txt` — existing target structure and VCV_RACK define pattern
- Codebase analysis: `project/Tomodachi design doc.md` — palette spec (15 colors + transparent), input API design
- SDL2 Common Mistakes: [nullprogram.com/blog/2023/01/08/](https://nullprogram.com/blog/2023/01/08/)
- SDL2 Pixel Format Endianness: [wiki.libsdl.org/SDL2/SDL_PixelFormatEnum](https://wiki.libsdl.org/SDL2/SDL_PixelFormatEnum)
- SDL2 GLES2 Format Bug: [github.com/libsdl-org/SDL/issues/3899](https://github.com/libsdl-org/SDL/issues/3899)
- SDL2 ARGB/RGBA Mixup: [discourse.libsdl.org](https://discourse.libsdl.org/t/pixel-format-problem-abgr8888-rgba8888-mixup/19989)
- SDL2 Window Drag Blocking: [github.com/libsdl-org/SDL/issues/4614](https://github.com/libsdl-org/SDL/issues/4614)
- SDL2 CMake Integration: [wiki.libsdl.org/SDL2/README-cmake](https://wiki.libsdl.org/SDL2/README-cmake)
- SDL2 Streaming Textures: [lazyfoo.net/tutorials/SDL/42_texture_streaming](https://lazyfoo.net/tutorials/SDL/42_texture_streaming/index.php)
- SDL2 Framebuffer Renderer: [codersplate.wordpress.com (2025)](https://codersplate.wordpress.com/2025/08/01/creating-a-framebuffer-renderer-in-c-with-sdl2/)
- Input Abstraction Pattern: [david-delassus.medium.com](https://david-delassus.medium.com/decoupling-input-bindings-from-game-systems-with-c-sdl-74d94a8b008c)
- Embedded Input (MPG library): [github.com/FeralAI/MPG](https://github.com/FeralAI/MPG)
- Indexed Palette Pitfalls: [cemetech.net TI calculator forum](https://www.cemetech.net/forum/viewtopic.php?t=20053) — palette mode must be active before palette edits take effect
- Dynamic allocation in embedded: [trebledj.me/posts/dynamic-memory-embedded-bad](https://trebledj.me/posts/dynamic-memory-embedded-bad/)

---
*Pitfalls research for: palette system, SDL2 desktop runner, input abstraction — enjin2 v1.3*
*Researched: 2026-02-23*
