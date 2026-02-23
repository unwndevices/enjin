# Project Research Summary

**Project:** enjin2 v1.3 — Tomodachi Readiness
**Domain:** C++ embedded graphics engine — palette system, SDL3 desktop runner, input abstraction
**Researched:** 2026-02-23
**Confidence:** HIGH

## Executive Summary

enjin2 v1.3 adds three tightly coupled features to an existing, stable engine: a 16-color indexed palette system, an SDL3 desktop runner as a third platform backend, and a platform-agnostic input abstraction for physical controls (buttons, pots, joysticks). All three features are strictly additive — existing Canvas4/Canvas8, Lua scripting, WASM, and ESP32 targets are untouched. The correct mental model is a display-time color resolution pipeline: canvas pixels remain 4-bit indices (0–15) throughout all draw operations; the palette converts indices to RGB only at the final blit to the output surface (SDL3 texture, WASM canvas, hardware DMA). This separation is the single most important architectural decision in the milestone and must be established as a contract before any implementation begins.

The recommended stack is minimal: SDL3 3.4.2 (system install via `find_package`, opt-in via `ENJIN2_BUILD_SDL=ON`), zero new libraries for the palette (64-byte static array), and zero new libraries for input (thin `IInputProvider` interface with per-platform implementations). All three features follow the same pattern already established for Lua and WASM: optional CMake targets, compile-time platform defines, no changes to core library targets. The SDL3 desktop runner is the primary integration surface — once it works end-to-end with palette and input wired in, the milestone is complete.

The critical risks are all design-time decisions, not implementation complexity. Index 0 semantics (transparent vs black) must be resolved before writing any palette code — existing scripts use `setColor(0)` for black, which conflicts with the Tomodachi spec of index 0 = transparent. Input abstraction interface headers must contain zero platform-specific types or the abstraction fails on ESP32. SDL3 pixel format selection must be verified at startup or colors will be silently wrong. None of these are hard to fix if caught early; all are expensive to fix after integration. The research also flags a naming inconsistency: FEATURES.md was written referencing SDL2 while STACK.md correctly specifies SDL3 — the implementation must adopt SDL3 and `ENJIN2_PLATFORM_SDL` as canonical names throughout.

---

## Key Findings

### Recommended Stack

The v1.3 stack is intentionally minimal. SDL3 3.4.2 (stable since January 2025) is the only new external dependency, used exclusively for the desktop runner. It installs via system package managers on all platforms and ships its own CMake config file — no vendoring, no custom Find module needed. SDL2 is explicitly rejected: it receives no new features and Fedora is replacing it with sdl2-compat in 2026. The palette and input abstraction are pure C++ headers with zero external dependencies. Both follow the same zero-dynamic-allocation constraint that governs the rest of the engine.

**Core technologies:**
- **SDL3 3.4.2** — window, surface rendering, event loop for desktop runner — only stable SDL option for new projects in 2026; SDL2 is a dead end
- **Inline `uint32_t palette[16]`** — palette storage — 64 bytes, zero allocation, no library justifies the footprint; pure header implementation
- **Custom `IInputProvider` interface** — input abstraction — static arrays with compile-time size constants (`MAX_BUTTONS=16`, `MAX_AXES=8`), platform implementations in platform-gated files

**Critical version note:** On macOS, `find_package(SDL3 CONFIG REQUIRED)` must NOT specify `SDL3-shared` as a component — known bug in SDL3Config.cmake (vcpkg issue #45498, reported May 2025).

See `.planning/research/STACK.md` for full rationale, alternatives considered, and CMake integration pattern.

### Expected Features

**Must have (v1.3 table stakes):**
- 16-entry palette with default colors — Pixel4 indices map to RGB at display time, never at draw time
- Index 0 = transparent — consistent with existing blit/matte convention; requires auditing existing `setColor(0)` usage
- Runtime palette swap — `setPaletteColor(index, r, g, b)` with no canvas re-render
- SDL3 window + game loop — init, event poll, clean shutdown with `SDL_TEXTUREACCESS_STREAMING`
- Canvas4-to-RGB texture blit via palette — the core SDL3 render path
- Integer pixel scaling, nearest-neighbor — small canvas readable on desktop monitor
- Lua scripting in SDL3 runner — same Lua scripts run identically on all platforms
- Unified InputState struct — buttons (bitmask) + analog axes (float array), edge detection built in
- Button edge detection — `justPressed`, `held`, `justReleased` per button, shared layer not platform layer
- SDL3 keyboard-to-button default mapping — arrows, Z/X, Enter
- Lua input polling API — `isButtonHeld(n)`, `isButtonJustPressed(n)`, `getAxis(n)`
- Lua palette API — `setPalette(index, r, g, b)` callable from scripts
- WASM bindings updated for palette — `getPaletteRGB()` exposed to JavaScript in same phase as palette implementation
- CMake `ENJIN2_BUILD_SDL=ON/OFF` — no impact on WASM or ESP32 builds

**Should have (v1.3.x after validation):**
- Lua hot-reload in SDL3 runner — F5 key shortcut or file mtime watch
- Two-stage draw palette — index remapping at draw time (PICO-8 model); single-stage is sufficient for v1.3
- Per-entry transparency flag in draw palette — any index can be transparent, not just index 0
- Lua keyboard mapping table — `input.setKeyMap(button, scancode)` for SDL3 runner

**Defer (v1.4+):**
- Lua input event callbacks (`onButtonPressed` / `onButtonReleased`)
- Canvas8 palette support (256-color indexed; different memory model)
- Input device hot-plug (connect/disconnect gamepad at runtime)
- Multi-layer composition (already deferred per PROJECT.md)

See `.planning/research/FEATURES.md` for full prioritization matrix and competitor analysis.

### Architecture Approach

Three new subsystems fit cleanly into the existing four-layer stack without modifying any existing library source. Palette lives in `enjin2_graphics` as a header-only struct (`include/enjin2/graphics/palette.hpp`). Input abstraction lives in `enjin2_ui` as a new parallel system alongside but independent of the existing mouse-oriented UI components. The SDL3 runner is a new `platforms/sdl2/` executable target that links all existing libraries plus SDL3. Three existing files require modification: `lua_platform.hpp/cpp` (add `SDL2_RUNNER` branch mirroring VCV_RACK config) and `CMakeLists.txt` (add `ENJIN2_BUILD_SDL` option + target).

**Major components:**
1. `Palette` (`enjin2_graphics`) — `RGB24 entries[16]`, index 0 = transparent, applied at blit time only; header-only, zero new build artifacts
2. `InputEvent` / `InputSource` / `InputSystem` (`enjin2_ui`) — event + source + consumer pattern; `SDL2InputSource` and `ESP32InputSource` are platform-gated implementations; edge detection in shared layer
3. `SDL2Runner` + `SDL2InputSource` (`platforms/sdl2/`) — SDL3 window, `SDL_TEXTUREACCESS_STREAMING` framebuffer texture, game loop with dt clamping, pixel format verified at startup
4. `LuaPlatform` extension — `SDL2_RUNNER` compile define; mirrors VCV_RACK Lua config (ENABLE_FILE_IO=true, ENABLE_ALL_LIBS=true)

**Recommended implementation order within milestone:**
1. `palette.hpp` — no deps, zero risk, unblocks SDL3 blit; resolve index 0 semantics here
2. Input interface headers (`input_event.hpp`, `input_system.hpp`) — contracts before platform implementations
3. `input_system.cpp` — dispatch implementation with edge detection
4. `lua_platform.hpp/cpp` SDL2_RUNNER case — unblocks LuaScriptSystem in SDL3 build
5. `SDL2Runner` + `SDL2InputSource` — requires palette + input + Lua all in place
6. Lua bindings for palette and input — requires runner wired for end-to-end validation

See `.planning/research/ARCHITECTURE.md` for full interface designs, file structure, and data flow diagrams.

### Critical Pitfalls

1. **Palette applied at draw time, not blit time** — modifying `setPixel()` to resolve palette corrupts packed 4-bit storage; canvas stores indices 0–15 only; palette is applied once per frame at the output boundary; never modify the draw path (recovery cost: HIGH)

2. **Index 0 semantic conflict** — existing `Colors::BLACK = Pixel4(0)` conflicts with index 0 = transparent convention; resolve this decision before writing any palette code; audit all existing examples using `setColor(0)` or `clear(0)` for black (recovery cost: MEDIUM)

3. **Input interface leaking platform types** — `SDL_Keycode` or `gpio_num_t` in a shared header breaks compilation on all other platforms; interface header must compile with `-DESP32` and zero SDL2 headers available (recovery cost: MEDIUM)

4. **SDL3 pixel format mismatch producing silently wrong colors** — `SDL_PIXELFORMAT_RGBA32` is endian-aliased and produces wrong output on little-endian systems without any error; use `SDL_PIXELFORMAT_RGB24` or explicit `ARGB8888`; add `SDL_QueryTexture` assertion at startup (recovery cost: LOW if caught early)

5. **WASM bindings not updated for palette** — SDL3 runner displays correct colors while WASM stays grayscale; expose `getPaletteRGB()` through Emscripten bindings in the same phase as the C++ palette; verify both rendering paths show identical colors for the same script (recovery cost: LOW)

See `.planning/research/PITFALLS.md` for full pitfall list including texture access mode, CMake contamination, Lua color API ambiguity, and the "looks done but isn't" verification checklist.

---

## Implications for Roadmap

The dependency graph dictates a clear 4-phase structure. Palette must precede the SDL3 runner (blit step requires it). Input interface must precede platform implementations. SDL3 CMake setup must precede any SDL3 C++ code. Lua bindings extend existing infrastructure and come last. All four phases are additive — no existing code is deleted, modified in ways that break other platforms, or restructured.

### Phase 1: Palette Foundation

**Rationale:** Zero external dependencies, zero existing code changes, unblocks the SDL3 runner's blit step. The index 0 semantic conflict must be resolved at the start of this phase or it cascades into every subsequent phase. WASM bindings must be updated here — not in Phase 3 — to prevent SDL3 runner and WASM diverging silently in color output.

**Delivers:** `Palette` struct with default 16-color table, runtime palette swap, `getPaletteRGB()` Emscripten binding, updated WASM JavaScript renderer, Lua `setPalette()` / `getPalette()` API

**Features addressed:** 16-entry palette, index 0 = transparent, runtime palette swap, Lua palette API, WASM palette exposure

**Pitfalls to avoid:** Palette at draw time (Pitfall 1), index 0 semantic conflict (Pitfall 8), WASM bindings not updated (Pitfall 9)

**Research flag:** Standard patterns — no additional research needed. Direct codebase inspection confirms all integration points. PICO-8 reference well-documented.

### Phase 2: Input Abstraction

**Rationale:** Fully independent of palette and SDL3. Interface-first design ensures SDL3 backend implements against a stable contract. Edge-detection semantics (`wasJustPressed`, `wasJustReleased`) must be in the shared interface from the start — adding them later requires touching all platform backends.

**Delivers:** `InputEvent`, `InputKind`, `InputSource` interface, `InputSystem` with listener dispatch and edge detection, state cache for Lua polling queries

**Features addressed:** Unified InputState, button edge detection, ESP32 input injection path, foundation for SDL3 keyboard mapping

**Pitfalls to avoid:** Platform type leakage into interface (Pitfall 5), missing debounce/edge semantics (Pitfall 6), extending existing mouse InputComponent instead of adding parallel system (Architecture anti-pattern 4)

**Research flag:** Standard patterns — interface + source + consumer pattern is well-established. All integration points identified from codebase inspection.

### Phase 3: SDL3 CMake + Runner

**Rationale:** CMake integration must be correct before any SDL3 C++ is written — contaminating core library targets requires significant cleanup to undo. Runner requires Phase 1 (palette) and Phase 2 (input interface) to be complete. `SDL2_RUNNER` define and `lua_platform.hpp` extension unblock Lua in the SDL3 build.

**Delivers:** `ENJIN2_BUILD_SDL=ON/OFF` CMake option, `enjin2_sdl` executable target, `SDL2Runner` with streaming texture + game loop + dt clamping, `SDL2InputSource`, `lua_platform.hpp` SDL2_RUNNER branch, integer pixel scaling, pixel format startup assertion

**Features addressed:** SDL3 window + game loop, Canvas4-to-RGB blit via palette, integer scaling (nearest-neighbor), Lua scripting in SDL3 runner, SDL3 keyboard-to-button default mapping

**Pitfalls to avoid:** SDL3 CMake target contaminating core (Pitfall 7), pixel format mismatch (Pitfall 2), texture access mode wrong (Pitfall 3), event loop blocking with unclamped dt (Pitfall 4), VCV_RACK define reused for SDL3 (Architecture anti-pattern 3)

**Research flag:** Warrants careful checklist execution. Multiple specific failure modes documented in PITFALLS.md. Apply the "looks done but isn't" checklist item by item, particularly: streaming texture confirmed, pixel format assertion in place, `ENJIN2_BUILD_SDL=OFF` clean build verified, SDL3 headers absent from core include paths.

### Phase 4: Lua Integration + End-to-End Validation

**Rationale:** Lua bindings extend existing `LuaCanvas` and `LuaScriptSystem`; they require the runner (Phase 3) to be wired before end-to-end validation is possible. This phase is also the integration gate — the same Lua script must produce visually identical output on SDL3 runner and WASM before the milestone is considered complete.

**Delivers:** Lua input polling API (`isButtonHeld`, `isButtonJustPressed`, `getAxis`), Lua palette API callable from scripts, cross-platform color consistency verification (SDL3 vs WASM), full "looks done but isn't" checklist sign-off

**Features addressed:** Lua input polling, Lua palette API, SDL3 keyboard mapping default (verified via Lua test script)

**Pitfalls to avoid:** Lua color API semantic ambiguity follow-through (Pitfall 8), Lua input binding coupling directly to SDL state (Integration Gotchas — input + Lua row)

**Research flag:** Standard patterns — extends existing LuaCanvas binding infrastructure. No novel territory.

### Phase Ordering Rationale

- Palette before runner: the blit step in the SDL3 runner reads the palette; without it the runner cannot display anything correctly
- Input interface before runner: `SDL2InputSource` implements against the interface; the interface must be stable before the platform backend is written
- CMake option before C++ runner code: SDL3 target isolation from core library targets cannot be retrofitted without risk
- WASM bindings updated in Phase 1 (not Phase 3): prevents a cross-platform color discrepancy that makes rendering bugs hard to attribute and is tempting to defer indefinitely
- Lua bindings last: depend on all prior phases; natural integration validation gate

### Research Flags

**Phases with standard, well-documented patterns (no additional research needed):**
- **Phase 1 (Palette):** Trivial data structure; PICO-8/TIC-80 reference well-documented; codebase integration points confirmed by direct inspection
- **Phase 2 (Input Abstraction):** Standard interface + source + consumer pattern; all integration points mapped from codebase
- **Phase 4 (Lua Integration):** Extends existing `LuaCanvas` binding pattern; no novel territory

**Phases warranting careful checklist execution during task planning:**
- **Phase 3 (SDL3 CMake + Runner):** Multiple specific, documented failure modes in PITFALLS.md; apply verification checklist at phase completion, not after

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | SDL3 3.4.2 confirmed stable via official SDL wiki and Phoronix coverage; all integration points verified against live codebase headers and CMakeLists.txt; macOS CMake bug confirmed against vcpkg issue tracker |
| Features | HIGH | PICO-8/TIC-80 as authoritative reference implementations for palette and input semantics; all Lua API extension points confirmed by direct inspection of `bindings.hpp` and `lua_platform.hpp` |
| Architecture | HIGH | Based entirely on direct inspection of existing codebase; no inference or approximation required; file locations, interface designs, and build order all grounded in current code |
| Pitfalls | HIGH | Mix of direct codebase analysis (existing packed-storage constraints, VCV_RACK define patterns) and verified SDL2/SDL3 issue tracker references; specific warning signs documented for each pitfall |

**Overall confidence:** HIGH

### Gaps to Address

- **SDL3 vs SDL2 naming inconsistency:** FEATURES.md was written referencing SDL2; STACK.md correctly specifies SDL3. Roadmap must adopt SDL3 and `ENJIN2_PLATFORM_SDL` as canonical names throughout. CMake target: `enjin2_sdl` (not `enjin2_sdl2`). Compile define: `ENJIN2_PLATFORM_SDL` (not `SDL2_RUNNER`).

- **Index 0 transparent decision:** Research identifies the conflict between `Colors::BLACK = Pixel4(0)` and the Tomodachi spec (index 0 = transparent) but does not make the final call — that requires project-owner sign-off. Recommendation: adopt option (a), index 0 = transparent, indices 1–15 = user colors, matching Tomodachi spec. All existing examples using `setColor(0)` or `clear(0)` for black must be updated before Phase 1 code lands.

- **WASM palette application ownership:** Whether the palette-to-RGB conversion happens in C++ (`emscripten_bindings.cpp`) or in the JavaScript caller is not fully specified. Must be decided in Phase 1 before the Emscripten binding change is written. Recommendation: expose raw palette data via `getPaletteRGB()` and apply in JavaScript, keeping the C++ binding thin.

---

## Sources

### Primary (HIGH confidence)
- Existing codebase — `canvas.hpp`, `canvas_esp32s3.hpp`, `lua_platform.hpp`, `bindings.hpp`, `CMakeLists.txt`, `emscripten_bindings.cpp`, `component.hpp`, `system.hpp` (2026-02-23 direct inspection)
- `.planning/PROJECT.md` — v1.3 requirements and constraints
- SDL3 Wiki (wiki.libsdl.org/SDL3) — NewFeatures, README-cmake, SDL_GetKeyboardState, SDL_pixels.h
- SDL3 Official Release announcement (Phoronix) — confirmed stable January 2025, 3.4.2 current as of Feb 2026
- PICO-8 Manual (lexaloffle.com/dl/docs/pico-8_manual.html) — two-stage palette system, input API (`btn`, `btnp`), 16-color constraint

### Secondary (MEDIUM confidence)
- SDL2 pixel format endianness (wiki.libsdl.org/SDL2/SDL_PixelFormatEnum) — format selection rationale; applies to SDL3
- SDL2 streaming textures (lazyfoo.net) — `SDL_TEXTUREACCESS_STREAMING` pattern
- SDL2 window drag blocking (github.com/libsdl-org/SDL/issues/4614) — dt clamping requirement
- Fantasy console palette survey (lospec.com/palette-list/tag/fantasyconsole) — 16-color conventions
- SDL2 game loop patterns (thelinuxcode.com, 2026) — delta time via `SDL_GetPerformanceCounter`

### Tertiary (LOW confidence, needs verification)
- vcpkg SDL3-shared macOS bug (github.com/microsoft/vcpkg/issues/45498) — macOS-specific; verify on macOS during Phase 3 CMake work
- SDL2 indexed texture streaming (discourse.libsdl.org archived forum) — needs verification against SDL3 API surface

---
*Research completed: 2026-02-23*
*Ready for roadmap: yes*
