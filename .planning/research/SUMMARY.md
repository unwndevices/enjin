# Project Research Summary

**Project:** enjin2 v1.4
**Domain:** Embedded/WASM 2D graphics engine — multi-layer composition, sprite system rework, Lua hot reload, Docusaurus navigation fix
**Researched:** 2026-02-24
**Confidence:** HIGH

## Executive Summary

enjin2 v1.4 is a capabilities milestone that adds four independent feature clusters to an already-working zero-alloc 2D graphics engine. No new dependencies are required. All four features are implementable with the existing stack (C++17, SDL3 3.4.2, Lua 5.4, Emscripten, Docusaurus 3.9.2). The recommended approach follows zero-alloc principles throughout: static Canvas4 layer arrays in sdl_main.cpp for multi-layer composition, a plain-struct SpriteSheet with compile-time frame layout, a Lua state teardown/reinit cycle for hot reload, and a surgical `generate-api-docs.js` escaping fix for Docusaurus. Every architectural decision is grounded in direct codebase inspection — there are no external library choices to evaluate.

The critical ordering constraint is that multi-layer C++ plumbing must precede multi-layer Lua bindings, and hot reload must come last because it must re-wire the layer canvas array after shutdown. The Docusaurus fix is fully independent and carries the oldest-standing tech debt in the project (carried from v1.0). The sprite rework is also independent of layers at the C++ struct level, though the Lua `drawSprite(id, layer, x, y)` call requires both the sprite pool and the layer canvas pointer array to exist before it can be wired up.

The highest-risk pitfalls are: (1) a pre-existing dangling-pointer bug in `LuaEngine::registerFunction(LuaCallback)` that hot reload will expose — must be fixed before F5 is wired up; (2) the multi-layer compositor must replace `expand_canvas_to_rgb()` wholesale — a partial integration silently renders only layer 0 with no error; and (3) the sprite rework is a hard API break (ICanvas<uint8_t> canvas target → ICanvas<Pixel4>), requiring a grep-and-update of all callers in the same change. All three are preventable with a defined fix sequence and a verification checklist.

---

## Key Findings

### Recommended Stack

No new dependencies for v1.4. The existing stack covers all four feature clusters entirely. Implementation patterns are drawn from established fantasy-console references (PICO-8 for sprite grid indexing and layer semantics, LÖVE 2D for draw call shape), but these are reference designs only — not dependencies.

**Core technologies:**
- **C++17 (existing)** — all new features; static template arrays cover fixed-layer composition; no library adds value
- **SDL3 3.4.2 (existing)** — hot reload trigger via `SDL_SCANCODE_F5` with `!event.key.repeat` guard; no file-watching API needed
- **Lua 5.4 (existing)** — hot reload via `lua_close` + `lua_newstate` full state reset; partial reset leaves dangling upvalues and is incorrect
- **Docusaurus 3.9.2 (existing)** — already at current stable; navigation breakage is stale placeholder text and `<Type>` escaping in the doc generator, not a framework version issue

See `.planning/research/STACK.md` for full pattern implementations, alternatives considered, and per-platform notes.

### Expected Features

**Must have (v1.4 table stakes):**
- Multi-layer: 4 Canvas4 buffers composited at blit time, index-15 transparent passthrough, `setLayer(n)` / `clearLayer(n)` Lua API
- Sprites: `SpriteSheet` struct with uniform grid layout, FPS auto-advance, 3 loop modes (once/loop/ping-pong), clean C++ API with no legacy public members
- Sprites: Lua sprite pool — `newSprite`, `drawSprite`, `updateSprite`, `setFrame` bindings
- Hot reload: F5 key in SDL3 runner, full Lua state reset, script path from argv, error display on syntax failure
- Docusaurus: angle brackets escaped in `generate-api-docs.js`, all 85 API pages reachable, `npm run build` passes with zero MDX errors

**Should have (v1.x post-validation):**
- Sprite flip (horizontal/vertical) — low complexity; useful for Tomodachi character facing direction
- Layer visibility toggle `setLayerVisible(n, bool)` — skip a layer in compositor; deferred until a concrete hide use case emerges
- File-watch auto-reload — only if F5 proves inconvenient in practice

**Defer (v2+):**
- Sprite collision detection — bounding-box overlap; needs a concrete use case from Tomodachi
- Tiled map rendering — significant scope expansion on top of layered canvases
- Getting started guide (docs) — explicitly deferred in PROJECT.md

See `.planning/research/FEATURES.md` for full prioritization matrix, anti-feature rationale, and feature dependency graph.

### Architecture Approach

The architecture is additive: three new header-only or small-impl files (`layer_stack.hpp`, `sprite_sheet.hpp`, `frame_animation.hpp`) plus modifications to `sdl_main.cpp`, `bindings.cpp`, `bindings.hpp`, and `generate-api-docs.js`. The compositor replaces `expand_canvas_to_rgb()` with a top-to-bottom layer walk that writes the first non-transparent pixel per position into the existing `g_rgb_staging[]` buffer — no extra allocation. Layer switching in Lua is an O(1) pointer swap on `LuaCanvas`, not a per-draw-call dispatch. Frame animation state is a value-type member inside `C_Sprite`, not a separate ECS component (avoids consuming hard-capped component slots and inter-component dynamic_cast overhead).

**Major components after v1.4:**
1. `LayerStack<W,H>` (new) — owns 4 Canvas4 buffers, exposes by `LayerIndex` enum; declared as static global in `sdl_main.cpp`
2. `SpriteSheet` + `FrameAnimation` (new) — plain structs, no heap; `FrameAnimation` embedded in `C_Sprite` as a value member
3. `LuaCanvas` + `LuaBindings` (modified) — gains `setActiveLayer(uint8_t)` + static `LuaCanvas layerCanvases[4]` array; all new Lua bindings use `lua_CFunction`, never `LuaCallback` (std::function)
4. `LuaScriptSystem` (modified) — gains `lastScriptPath_` + `reload()` method; `g_reload_requested` flag prevents mid-frame teardown
5. `generate-api-docs.js` (modified) — all template parameter and description text wrapped with `escapeForMdx()` before output; regenerated `docs/api/` files committed alongside

**Build order:** Docusaurus fix → Sprite rework (C++ + Lua) → Multi-layer (C++ + Lua) → Hot reload.

See `.planning/research/ARCHITECTURE.md` for full component responsibility table, data flow diagrams, and anti-patterns to avoid.

### Critical Pitfalls

1. **Dangling LuaCallback pointer on hot reload** — `registerFunction(LuaCallback)` stores a pointer to a local parameter; after reload the Lua closure holds a dangling pointer. Fix before wiring F5: eliminate all `LuaCallback` (std::function) registrations; use `lua_CFunction` exclusively for all new bindings. Verify by reloading twice in succession with no crash.

2. **Multi-layer compositor silently renders only layer 0** — if `expand_canvas_to_rgb()` is not replaced wholesale, drawing to layers 1–3 is silently discarded. Verify by drawing exclusively to layer 2 and confirming it appears in the SDL3 window.

3. **Sprite API is a hard compile-time break** — old `Sprite::draw(ICanvas<uint8_t>&)` is incompatible with `Canvas4` (ICanvas<Pixel4>). Grep all callers of legacy `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` fields before removing them and update all call sites in the same change. Change `matte` default from 16 to 15 (PALETTE_TRANSPARENT).

4. **Hot reload must not execute inside the SDL event handler** — calling `g_lua.shutdown()` directly in the event pump means `update()` and `draw()` run on an empty Lua state in the same frame. Use a `g_reload_requested` flag; execute reload at the top of the next frame before input advance.

5. **Transparency sentinel is index 15, not 0** — `Canvas4::clear()` defaults to `Pixel4(0)` (opaque black). Every `clearLayer()` call and the compositor skip condition must use `PALETTE_TRANSPARENT = 15` explicitly. Clearing layers with index 0 blocks lower layers and makes solid black undrawable.

See `.planning/research/PITFALLS.md` for all 10 critical pitfalls, integration gotchas table, performance traps, and the "looks done but isn't" verification checklist.

---

## Implications for Roadmap

The dependency graph drives a natural 4-phase sequence. The Docusaurus fix is independent and clears the oldest tech debt before new classes add more API pages. Sprite rework at the C++ level is independent of layers. Multi-layer lands after sprites so that the combined `drawSprite(id, layer, x, y)` binding has both prerequisites. Hot reload comes last because it is trivially small once multi-layer is in place and must re-wire the full layer canvas array.

### Phase 1: Docusaurus Navigation Fix

**Rationale:** Fully independent — zero C++ changes, zero runtime risk. Clears the oldest-standing tech debt item (carried from v1.0 per PROJECT.md). Unblocks the API documentation workflow before new classes (LayerStack, SpriteSheet, FrameAnimation) are added and require their own doc generation runs. Lowest risk, clearest scope.
**Delivers:** Working "API Reference" navbar link, all 85 API pages accessible in autogenerated sidebar, `generate-api-docs.js` produces MDX-safe output for all future regenerations (angle brackets escaped in prose text for template types), `npm run build` passes with zero MDX errors, guide `.md` placeholder text replaced with real cross-links.
**Addresses:** DOC-01 / DOC-02 from PROJECT.md; Cluster D from FEATURES.md.
**Avoids:** Pitfall 7 (MDX angle bracket failures — fix is in the generator, not just the generated files); run `npm run build` as the completion gate.

### Phase 2: Sprite System Rework

**Rationale:** Independent of the layer system at the C++ struct level. New structs and the reworked `Sprite` class can be added and verified without any multi-layer code. The hard API break (uint8_t → Pixel4 canvas target) is isolated to this phase, keeping it away from the multi-layer integration surface. Lua `canvas.drawSprite()` in its stateless form targets whatever canvas is currently active — it is immediately usable before layer switching exists.
**Delivers:** `SpriteSheet` + `FrameAnimation` structs in new header files, rewritten `Sprite` class with template `draw<TPixel>()`, clean API with no legacy public members, `FrameAnimation` value-type member in `C_Sprite` with FPS auto-advance and once/loop/ping-pong loop modes, Lua sprite pool with `newSprite` / `drawSprite` / `updateSprite` / `setFrame`, `sprite.cpp` added to `enjin2_graphics` sources in CMakeLists.txt.
**Uses:** Zero-alloc pattern from STACK.md; PICO-8 `spr(n, x, y)` API shape.
**Avoids:** Pitfall 4 (sprite API break — treat as clean break, grep legacy callers, update in same change, change matte default to 15); Pitfall 10 (WASM/ESP32 — all new Lua bindings use lua_CFunction, no lambda captures).

### Phase 3: Multi-Layer Canvas Composition

**Rationale:** Modifies `sdl_main.cpp` globals and `LuaCanvas` routing — the shared integration point for both sprite bindings and hot reload. Landing after sprites means `canvas.drawSprite()` works correctly with the layer-aware canvas on arrival. Landing before hot reload ensures the reload sequence has the full layer canvas array to re-wire.
**Delivers:** `LayerStack<W,H>` header-only struct with `LayerIndex` enum, `g_layers[4]` static global replacing `g_canvas` in `sdl_main.cpp`, composite `expand_canvas_to_rgb()` (top-to-bottom layer walk, PALETTE_TRANSPARENT skip, no scratch buffer), `setLayer(n)` / `clearLayer(n)` / `getLayerCount()` Lua bindings, `LuaCanvas::setActiveLayer(uint8_t)`, static `LuaCanvas layerCanvases[4]` pre-allocated in `LuaBindings`.
**Avoids:** Pitfall 2 (ESP32 SRAM — verify 4×8192 + 64KB Lua pool fits before declaring buffers; gate with PSRAM annotation or compile-time layer count option); Pitfall 3 (template mismatch — add `getRawBuffer()` to ICanvas or template compositor on `<W,H>` before writing the blend loop); Pitfall 5 (LuaCanvas void* routing — pre-allocate static array, no dynamic LuaCanvas allocation); Pitfall 8 (transparency — all clear calls use `Pixel4(PALETTE_TRANSPARENT)`, compositor skip checks `PALETTE_TRANSPARENT` not 0); Pitfall 9 (SDL3 render path — replace expand function wholesale, verify with layer-2-only draw test).

### Phase 4: Lua Hot Reload

**Rationale:** Smallest change set of the four clusters (F5 event + reload flag + `LuaScriptSystem::reload()`). Must come last because the reload re-wiring must know about `g_layers` (not just the old `g_canvas`). The pre-existing `LuaCallback` dangling-pointer bug must be fixed as the first step of this phase before F5 is wired up.
**Delivers:** `g_reload_requested` flag pattern, F5 handler in SDL3 event loop (`#ifdef ENJIN2_BUILD_LUA` scoped), `LuaScriptSystem::reload()` with `lastScriptPath_` storage, script path parsed from argv into `static char[256]` buffer, stderr error display on reload failure, console log `[reload] script.lua`.
**Avoids:** Pitfall 1 (dangling LuaCallback — fix as step 1 before wiring F5; audit and eliminate all LuaCallback registrations); Pitfall 6 (mid-frame reload — reload_requested flag, never shutdown inside event handler; verify with F5 during active Lua update call producing zero "Function not found" errors); Pitfall 10 (Emscripten — scope hot reload to `#ifdef ENJIN2_BUILD_SDL` only, zero WASM/ESP32 impact).

### Phase Ordering Rationale

- Docusaurus first: clears oldest tech debt with zero C++ risk before the codebase gains more classes that need doc generation runs.
- Sprite rework second: hard API break is safest landed in isolation; Lua sprite pool needed before multi-layer Lua binding intersection (`drawSprite(id, layer, x, y)`) can be wired.
- Multi-layer third: largest sdl_main.cpp integration surface; benefits from having sprite Lua pool already stable; establishes layer canvas array that hot reload must re-wire.
- Hot reload last: trivially small once multi-layer is in place; the one pre-existing bug (LuaCallback) must be fixed as its first step regardless of phase order.

### Research Flags

**Phases with well-documented patterns — skip research-phase:**
- **Phase 1 (Docusaurus):** Root cause identified, fix location confirmed (`generate-api-docs.js`), verified by direct file inspection. Standard MDX angle-bracket escaping pattern.
- **Phase 4 (Hot Reload):** SDL3 F5 pattern from official wiki; Lua shutdown/initialize cycle confirmed idempotent by code inspection; flag pattern is a standard game-loop idiom.

**Phases that may benefit from targeted micro-research during spec writing:**
- **Phase 3 (Multi-Layer):** The `ICanvas<TPixel>` interface extension (`getRawBuffer()`) vs. templated compositor decision needs a brief architecture review to choose an approach before writing the compositor. Both options are documented in ARCHITECTURE.md and PITFALLS.md; the final choice should be made explicit in the phase spec before implementation begins.
- **Phase 2 (Sprite Rework):** The `C_Sprite` / ECS pipeline canvas type mismatch (`C_Drawable::draw(ICanvas<uint8_t>&)` virtual interface) is a known pre-existing limitation in CONCERNS.md. The phase spec must define the scope boundary: sprite rework targets the Lua scripting path only; the ECS pipeline canvas type gap is explicitly out of scope for v1.4.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All four features use existing dependencies; no external source lookup required; verified against current codebase headers and build files |
| Features | HIGH | Code-verified; all claims grounded in direct src/ and include/ inspection; exact API shapes confirmed against live code |
| Architecture | HIGH | Based on direct codebase inspection of all relevant source files; integration points enumerated with file-level precision |
| Pitfalls | HIGH | Sourced from live code analysis: dangling-pointer bug confirmed in lua_engine.cpp, SRAM budget computed from actual BUFFER_SIZE constant, transparency sentinel from palette.hpp |

**Overall confidence:** HIGH

### Gaps to Address

- **ESP32 PSRAM availability:** The 4-layer memory budget (32 KB for layers + 64 KB Lua pool) fits on SDL3 trivially but requires verification on the specific ESP32-S3 board. The Phase 3 spec should include an `ESP.getFreePsram()` check at startup or a `static_assert` on available memory. If no PSRAM, layer count must be reduced to 2 at compile time via CMake option.

- **`ICanvas<TPixel>` buffer access interface decision:** The compositor can use per-pixel `getPixel()`/`setPixel()` virtual dispatch (65K calls per frame at 128×128 with 4 layers) or a bulk `getRawBuffer()` approach requiring an interface extension. PITFALLS.md recommends the interface extension. The final method signatures should be confirmed in the Phase 3 spec before implementation begins to avoid a mid-phase interface change.

- **ECS canvas type mismatch (deferred, not fixed):** `C_Drawable::draw(ICanvas<uint8_t>&)` is the existing ECS virtual interface; Canvas4 is `ICanvas<Pixel4>`. The sprite rework bypasses ECS for the Lua scripting path, but the gap remains for any ECS-driven rendering. This is a known CONCERNS.md item; explicitly out of scope for v1.4 and must be documented as a known limitation in the Phase 2 spec.

---

## Sources

### Primary (HIGH confidence)
- `src/scripting/lua_engine.cpp` — luaAllocator, memoryPool, registerFunction(LuaCallback) dangling-pointer bug confirmed
- `src/scripting/bindings.cpp` — g_currentBindings global, registerAll(), existing lua_CFunction pattern
- `src/platform/sdl/sdl_main.cpp` — expand_canvas_to_rgb(), event loop structure, single-canvas render path
- `include/enjin2/graphics/canvas.hpp` — Canvas4<W,H> BUFFER_SIZE, ICanvas interface, missing getRawBuffer()
- `include/enjin2/graphics/sprite.hpp` — ICanvas<uint8_t> draw target, matte=16 out-of-range, legacy _ fields
- `include/enjin2/graphics/palette.hpp` — PALETTE_TRANSPARENT = 15 convention confirmed
- `include/enjin2/scripting/lua_platform.hpp` — MEMORY_LIMIT = 64*1024 on ESP32
- `include/enjin2/scripting/bindings.hpp` — LuaCanvas void* type erasure, is4Bit discriminator
- `src/bindings/emscripten_bindings.cpp` — getCanvasData128() reads single canvas, WASM layer exposure gap
- `docs/docusaurus.config.js` — dual-plugin setup, autogenerated sidebar, onBrokenLinks: 'throw'
- `docs/api-sidebar.js` — autogenerated sidebar type confirmed
- `scripts/generate-api-docs.js` — escapeForMdx() function verified; inconsistent application confirmed in CanvasExtended.md
- [SDL3 BestKeyboardPractices — SDL Wiki](https://wiki.libsdl.org/SDL3/BestKeyboardPractices) — F5 scancode pattern, !event.key.repeat guard
- [SDL3 SDL_KeyboardEvent — SDL Wiki](https://wiki.libsdl.org/SDL3/SDL_KeyboardEvent) — scancode, repeat, key fields

### Secondary (MEDIUM confidence)
- [PICO-8 API reference — pico-8.github.io](https://pico-8.github.io/pico8-api/) — spr(n, x, y) API shape, uniform grid sprite numbering (reference design, not dependency)
- [BEEP-8 SDK — beep8.github.io](https://beep8.github.io/beep8-sdk/) — C++ embedded PICO-8 port, 4-bit VRAM sprite banks (reference design)
- [Docusaurus v3 MDX migration](https://docusaurus.io/blog/preparing-your-site-for-docusaurus-v3) — angle bracket escaping cause and fix
- [Docusaurus 3.9.0 changelog](https://docusaurus.io/changelog/3.9.0) — 3.9.2 confirmed current stable
- ESP-IDF docs — ESP32-S3 512KB SRAM total; WiFi/BT buffers ~100KB; PSRAM availability board-dependent

### Tertiary (LOW confidence — validate during implementation)
- ESP32 PSRAM availability for 4-layer stack — depends on specific board; must be verified at runtime with `ESP.getFreePsram()` before declaring 4-layer buffers
- Emscripten `-fno-exceptions` + std::function longjmp interaction — documented UB; practical impact depends on actual build flags in CMakeLists.txt `enjin2_wasm` target

---
*Research completed: 2026-02-24*
*Ready for roadmap: yes*
