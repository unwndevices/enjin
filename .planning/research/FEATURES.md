# Feature Research

**Domain:** Embedded/WASM 2D graphics engine — v1.4 capabilities milestone
**Researched:** 2026-02-24
**Confidence:** HIGH (code-verified; all claims grounded in existing src/ and include/ inspection)

---

## Feature Landscape

This milestone adds four independent capability clusters to an already-working engine.
Each cluster is described separately with its own table-stakes/differentiator/anti-feature breakdown.

---

## Cluster A: Multi-Layer Canvas Composition

**What is being added:** 4 independent `Canvas4<W,H>` buffers (indexed 0–3) composited into
a single output buffer at blit time, using index-15 passthrough transparency. Index 15 on a
layer above lets the layer below show through; it does not mean "black."

**Existing baseline:** Single `g_canvas` (`Canvas4<128,128>`), one draw surface, 6 draw layers
sorted at render time (not independent pixel buffers). The `expand_canvas_to_rgb()` function
walks one canvas and expands palette values to RGB24.

### Table Stakes

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Fixed number of layers (4) | Embedded constraints — no dynamic allocation. Layer count must be compile-time or config-time. PICO-8, TIC-80, and similar constrained engines all use a fixed small count. | LOW | 4 is the spec. `Canvas4<W,H>` arrays are static. |
| Index-15 = transparent in composition | Already established in the palette system (PAL-02). Users expect the same semantic at the compositor: index 15 on layer N reveals layer N-1. | LOW | One condition in the composition loop. No per-pixel alpha math. |
| Top-down composition order | Layer 0 = bottom (background), layer 3 = top (foreground/HUD). This is the universal convention from every layered 2D system (NES PPU, PICO-8, LÖVE). | LOW | Loop from 0 to 3, skip index-15 pixels. |
| Clear individual layers | Scripts must be able to clear layer N without touching others. `canvas[N].clear(Pixel4(15))` fills with transparent. | LOW | Wraps existing `Canvas4::clear()`. |
| Lua API: draw-to-layer by index | `setLayer(n)` selects which canvas receives subsequent draw calls. All existing draw bindings route through `LuaCanvas::canvasPtr`. | MEDIUM | Requires `LuaCanvas` to hold a pointer per layer plus a `currentLayer` index. `setCanvas()` in `LuaBindings` stays the same concept. |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Zero-copy composition into existing RGB24 staging buffer | `expand_canvas_to_rgb()` already does one pass. Multi-layer composition adds N passes (one per layer) over the same staging buffer, writing only non-transparent pixels. No extra heap alloc. | LOW | Walk layers 0..3; write to `g_rgb_staging` only when `px.value != 15`. |
| Layer clear convenience (`clearLayer(n, color)`) | Lets scripts reset a single layer each frame without rebuilding all layers. Useful for animated foreground layers. | LOW | Calls `canvas[n].clear(Pixel4(color))`. |

### Anti-Features

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Per-pixel alpha blending (0–255 transparency) | "More flexible than chroma-key" | Requires 8-bit per channel arithmetic per pixel — incompatible with Pixel4 (4-bit indexed). Breaks zero-alloc constraint and palette semantics. Explicitly Out of Scope in PROJECT.md. | Use index-15 chroma-key transparency. It covers the actual use case (sprite cutouts, HUD overlays). |
| Dynamic layer count at runtime | "Could add more layers when needed" | Dynamic allocation; contradicts zero-alloc constraint. | Fixed 4 layers at compile time. If 4 is insufficient for a future milestone, change the constant and rebuild. |
| Z-ordering per sprite across layers | "Sprites should sort globally" | Requires a sort pass across all draw calls; incompatible with immediate-mode indexed layers. | Use layer index as the Z-bucket. Draw HUD on layer 3, background on layer 0. |
| Blend modes between layers (add, multiply) | "Makes layers composable" | 4-bit palette indices are not color values — arithmetic on indices is meaningless. | Layer composition is strictly chroma-key passthrough. |

---

## Cluster B: Sprites System Rework

**What is being added:** Uniform grid sprite sheets (rows × cols), FPS-driven auto-advance
animation, loop modes (once / loop / ping-pong), clean C++ API with no legacy public members,
Lua bindings for sprite creation and draw calls.

**Existing baseline:** `Sprite` class in `include/enjin2/graphics/sprite.hpp`. Problems:
- Frame layout: raw offset `frame * width * height` — no sheet grid concept.
- Animation: manual only (`setTexture(frame_id)`), no time tracking, no auto-advance.
- API hygiene: `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` are public
  legacy members alongside private counterparts — causes confusion and breaks encapsulation.
- No Lua bindings for sprites at all (bindings.cpp has no sprite functions).

### Table Stakes

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Uniform grid sprite sheet layout (rows x cols) | Every 2D engine from GBA era onward treats sprite sheets as a grid. Users of PICO-8, Aseprite, GB Studio all think in terms of rows x cols, not raw byte offsets. | LOW | `frame_x = (frame % cols) * frame_w`, `frame_y = (frame / cols) * frame_h`. Frame data is still a `const uint8_t*` pointer to the full sheet. |
| FPS-driven frame auto-advance | Manual `setTexture(frame_id)` every frame is error-prone and non-portable (frame rate varies). Users expect `update(dt)` to advance frames automatically. | MEDIUM | Add `float fps`, `float elapsed`, `uint8_t frameCount` to `SpriteSheet`. `update(dt)`: `elapsed += dt; if elapsed >= 1/fps: advance frame; elapsed -= 1/fps`. |
| Loop mode: loop (default) | Wraps frame index at `frameCount`. Universal default in Aseprite, Godot, LÖVE, Unity. | LOW | `frame = (frame + 1) % frameCount`. |
| Loop mode: once (play then stop) | Plays to last frame, stops. Required for one-shot animations (explosions, pickups). | LOW | On last frame, `playing = false`. |
| Loop mode: ping-pong | Reverses direction at ends. Required for idle animations that should not hard-cut. | LOW | Add `int8_t direction = 1`. Flip at boundaries. |
| draw(canvas, x, y) clean call | Callers should not need to set position then call draw separately. A single `draw(canvas, x, y)` is the idiom in LÖVE, PICO-8, GB Studio. | LOW | Pass position at call site rather than storing it. Removes state mutation side effect. |
| No public legacy members | `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` are uninitialized public members that shadow private ones. | LOW | Delete them. No callers outside legacy compat code. |
| Index-15 matte transparency | Already established (Pixel4 semantics). Sprite draw loop must skip pixels where `value == 15`. | LOW | Already in existing `draw()` — preserve the condition, fix the frame offset math. |
| Lua bindings: newSprite, drawSprite, updateSprite | Scripts cannot use sprites today. This is the primary usage path for Tomodachi. | MEDIUM | `newSprite(data_ptr, frameW, frameH, cols, rows, fps)` returns a sprite ID (integer index into a static pool). `drawSprite(id, layer, x, y)`. `updateSprite(id, dt)`. |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Static sprite pool (no heap) | Consistent with zero-alloc constraint. `SpriteSheet pool[MAX_SPRITES]` as a static array. Lua gets integer handles (indices). | LOW | `MAX_SPRITES = 16` or similar compile-time constant is sufficient for Tomodachi. |
| setFrame(n) for manual override | Allows scripts to drive animation from game logic (not time), e.g., sync to MIDI beat. `setFrame(n)` bypasses auto-advance. | LOW | Just set `currentFrame = n % frameCount; playing = false`. |

### Anti-Features

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Non-uniform frame sizes in one sheet | "Some frames are different sizes" | Requires per-frame metadata array (heap or large static overhead). Incompatible with simple grid math. | Use multiple sprites with different sheets. Frame size is fixed at sheet creation time. |
| Runtime texture upload from Lua (passing pixel arrays) | "Load sprites from files at runtime" | ESP32 has no filesystem write path from outside. Lua to C++ array passing is expensive and unsafe with static buffers. | Embed sprite data as `const uint8_t[]` arrays in C++ and pass a pointer to `newSprite`. Lua can reference named globals. |
| Blend modes (Add, Sub) per sprite in Lua | "Special effects" | The existing `BlendMode::Add/Sub` operates on raw uint8_t values (pixel indices), not colors — mathematically wrong for indexed palette. Confuses users. | Remove Add/Sub blend modes from the reworked API. Index-15 transparency is the correct compositing primitive for 4-bit. |
| Sprite rotation and scaling | "Need to rotate sprites" | Requires floating-point pixel mapping, bilinear interpolation — unacceptable on ESP32. No hardware rasterizer available. | Pre-rotate frames in the sprite sheet. Rotation at blit time is out of scope for all constrained platforms. |

---

## Cluster C: Lua Hot Reload (SDL3 Runner)

**What is being added:** F5 keypress in the SDL3 runner triggers a full Lua state reset and
reloads the current script file from disk without restarting the process.

**Existing baseline:** SDL3 event loop in `sdl_main.cpp` handles `SDL_EVENT_QUIT` and
`SDLK_ESCAPE`. Lua is initialized once at startup via `g_lua.initialize()` and
`g_lua.loadScript(path)`. `LuaEngine::shutdown()` calls `lua_close(L)` and resets state.
`LuaEngine::initialize()` creates a fresh `lua_State*`. The script path is currently hardcoded
to `"scripts/e2e_parity.lua"`.

### Table Stakes

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| F5 triggers reload | Industry-standard key for "refresh" (browser, IDEs, game engines). PICO-8 uses Ctrl+R. F5 is the SDL3-natural choice since it maps to `SDLK_F5`. | LOW | Detect `SDL_EVENT_KEY_DOWN` with `event.key.key == SDLK_F5`. |
| Full Lua state teardown then reinit | A partial reload (re-executing script into existing state) leaves stale globals, closures, and upvalues. Full teardown is correct. | LOW | Call `g_lua.shutdown()` then `g_lua.initialize()` + `registerAll()` + `setCanvas()` + `setInput()` + `loadScript()`. |
| Re-register all bindings after reload | After `lua_close` + `lua_open`, the global table is empty. All C functions and the `enjin_bindings` registry entry must be re-registered. | LOW | `g_lua.getBindings().registerAll()` already does this. The reload sequence is: shutdown, initialize, registerAll, setCanvas, setInput, loadScript. |
| Script path from argv or config | Hardcoded path `"scripts/e2e_parity.lua"` must become a variable so the runner knows what to reload. | LOW | Parse `argv` for a script path argument (e.g., `--script path/to/script.lua`) and store it in a static `char[]` buffer. |
| Error display on reload failure | If the reloaded script has a syntax error, the engine must not crash. Show the error and stay running with a cleared canvas. | LOW | `loadScript()` returns `LuaResult`. On failure, log error to `stderr` and clear canvas to a visible error color (e.g., color 13). |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Reload does not restart SDL | The window, renderer, and texture persist across reloads. Users see a seamless reset without window flicker. | LOW | Only `g_lua` is torn down and rebuilt. SDL objects and canvas are untouched. Canvas content is implicitly overwritten on the next draw call. |
| Console log of reload event | `[reload] script.lua` to stdout gives the developer confirmation the reload fired. | LOW | `printf("[reload] %s\n", scriptPath)`. |

### Anti-Features

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| File-watch auto-reload (inotify/kqueue) | "Reload without pressing F5" | Platform-specific APIs (inotify on Linux, kqueue on macOS, ReadDirectoryChangesW on Windows). Adds OS-specific code to a platform-abstraction layer. Not needed for Tomodachi dev loop. | F5 manual reload is sufficient. The developer explicitly triggers reload after saving. |
| Partial state reload (hot-patching functions) | "Preserve runtime state across reload" | Impossible to do correctly in general: closures capture upvalues, metatables reference C pointers. Any preserved state from the old Lua universe becomes a dangling reference after `lua_close`. | Full reset is the correct semantic. If state persistence is needed, serialize to canvas pixels or a side channel before reload. |
| WASM/ESP32 hot reload | "Should work everywhere" | ESP32 has no filesystem write path from outside. WASM runs in a browser sandbox without inotify. Hot reload is a developer tool for the SDL3 runner only. | Scope this exclusively to `#ifdef ENJIN2_BUILD_SDL`. Zero impact on WASM and ESP32 builds. |

---

## Cluster D: Docusaurus MDX Navigation Fix

**What is being added:** The "API Reference" navbar item and sidebar are currently disabled or
broken. The fix enables the `apiSidebar` so users can navigate the 85 generated API pages.

**Existing baseline:**
- `docusaurus.config.js` configures a second docs plugin with `id: 'api'`, `path: 'api'`,
  `sidebarPath: 'api-sidebar.js'`. Navbar has a `docSidebar` item pointing to `apiSidebar`.
- `api-sidebar.js` uses `{ type: 'autogenerated', dirName: '.' }` — auto-generates from
  the `docs/api/` directory tree.
- 85 `.md` files exist across 10 subdirectories (`core`, `graphics`, `scripting`, etc.).
- Known MDX-unsafe content: `ICanvas<TPixel>`, `ICanvas< Pixel4 >`, `Canvas4< WIDTH, HEIGHT >`
  appear as raw angle brackets in prose text (non-code-fence contexts). MDX treats these as
  JSX element open tags and throws a parse error. Evidence: `CanvasExtended.md` line 12 has
  `ICanvas<TPixel>` raw; `Scene.md` has the same content already escaped as `ICanvas&lt; Pixel4 &gt;`
  — the generator escapes inconsistently.

### Table Stakes

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| "API Reference" navbar link is clickable and loads | Documentation sites without working navigation are unusable. This is the primary UX regression from v1.0. | LOW | Fix requires identifying the exact parse failure and patching it. |
| All 85 API pages reachable via sidebar | If pages exist in the filesystem but are absent from the sidebar tree, users cannot discover them. | LOW | `autogenerated` sidebar handles this automatically once MDX errors are cleared. |
| Angle brackets escaped in generated markdown | `ICanvas<TPixel>` outside a code fence is invalid MDX (treated as JSX tag). Must be escaped as `ICanvas&lt;TPixel&gt;` or wrapped in a code span. | LOW | The existing `generate-api-docs.js` script already escapes some cases. The gap is in `CanvasExtended.md` and possibly others. |
| No broken links on build (`onBrokenLinks: 'throw'`) | `docusaurus.config.js` sets `onBrokenLinks: 'throw'`. The build fails if any internal link is invalid. Fix must not introduce new broken links. | LOW | Sidebar uses autogenerated from filesystem; as long as `.md` files have correct `id` frontmatter, links resolve. |

### Differentiators

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Generator script produces MDX-safe output for all future regenerations | If the fix is only applied to existing files, the next doc regeneration will reintroduce the bug. The correct fix patches `generate-api-docs.js` to escape all `<` / `>` in prose text. | MEDIUM | One pass in the generator to HTML-encode angle brackets in prose text (not in code fences or existing `&lt;` entities). |
| Category-level sidebar grouping (autogenerated from dirs) | 10 subdirectories map to 10 collapsible sidebar categories without manual maintenance. `autogenerated` handles this. | LOW | Already configured; just needs the MDX parse errors gone. |

### Anti-Features

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Manual sidebar definition for all 85 pages | "More control over ordering" | Every new API page requires a manual sidebar update. 85 entries is unmaintainable. | Keep `autogenerated`. If ordering within a module matters, use numeric prefixes on filenames (Docusaurus respects alphabetical order). |
| Switching away from MDX format globally | "MDX causes problems" | The `format: 'detect'` setting means `.md` files parse as MDX by default in Docusaurus v3. Changing to CommonMark globally would disable JSX in all docs. | Keep MDX. Fix the specific angle bracket escaping issue in the generator. The `.md` extension with `format: 'detect'` is correct — just escape the offending characters. |

---

## Feature Dependencies

```
[Multi-Layer Canvas C++ API (4x Canvas4 buffers + compositor)]
    └──required by──> [Lua Layer Bindings (setLayer, clearLayer)]
                          └──required by──> [drawSprite(id, layer, x, y)]

[Sprite Sheet C++ struct (SpriteSheet)]
    └──required by──> [FPS auto-advance (update method)]
    └──required by──> [Lua sprite pool + bindings (newSprite, drawSprite, updateSprite)]

[Script path as argv variable]
    └──required by──> [Lua hot reload (F5 knows what to reload)]

[generate-api-docs.js MDX-safe output]
    └──required by──> [Docusaurus build passes (onBrokenLinks: throw)]
    └──required by──> [API sidebar autogenerated correctly]
```

### Dependency Notes

- **Multi-layer Lua bindings require multi-layer C++ API first:** `setLayer(n)` in Lua must
  route through a pointer array of `Canvas4` instances. The C++ composition must exist before
  the Lua binding can point at individual buffers.
- **drawSprite(id, layer, x, y) requires both sprite pool and layer API:** It is the
  intersection of Cluster A and Cluster B. Phase ordering must ship sprite structs and layer
  pointer array before this binding.
- **Lua hot reload depends on idempotent initialize/shutdown:** `LuaEngine::shutdown()` and
  `LuaEngine::initialize()` are already idempotent based on code inspection. The reload
  sequence is safe to call multiple times.
- **Docusaurus fix is fully independent:** No C++ changes required. Can be done in any
  phase order relative to the engine features.

---

## MVP Definition

### Launch With (v1.4)

All four clusters are the milestone. There is no sub-MVP: each cluster is an atomic deliverable.

- [ ] **Multi-layer:** 4 Canvas4 buffers composited at blit, index-15 transparent, `setLayer(n)` Lua API, `clearLayer(n)` Lua API
- [ ] **Sprites:** `SpriteSheet` struct with grid layout + FPS auto-advance + 3 loop modes, clean C++ (no legacy public members), Lua pool (newSprite/drawSprite/updateSprite/setFrame)
- [ ] **Hot reload:** F5 key in SDL3 runner, full state reset, error display, script path from argv
- [ ] **Docusaurus:** API sidebar accessible, angle brackets escaped in generator, 85 pages reachable, build passes `onBrokenLinks: 'throw'`

### Add After Validation (v1.x)

- [ ] **Sprite flip (horizontal/vertical)** — useful for Tomodachi character facing direction; low complexity (invert x or y index in frame read)
- [ ] **Layer visibility toggle** — `setLayerVisible(n, bool)` skip a layer in composition; deferred until a UI-hide use case emerges
- [ ] **File-watch reload** — only if the SDL3 dev loop is frequently used and F5 is inconvenient

### Future Consideration (v2+)

- [ ] **Sprite collision detection** — bounding-box overlap checks; needs a clear use case from Tomodachi
- [ ] **Tiled map rendering** — structured tilemap on top of layered canvases; significant scope expansion
- [ ] **Getting started guide in docs** — explicitly deferred in PROJECT.md

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Multi-layer C++ API (4 Canvas4 buffers + compositor) | HIGH | LOW | P1 |
| Multi-layer Lua bindings (setLayer, clearLayer) | HIGH | LOW | P1 |
| SpriteSheet struct (grid, FPS, loop modes) | HIGH | LOW | P1 |
| Lua sprite bindings (newSprite, drawSprite, updateSprite) | HIGH | MEDIUM | P1 |
| Legacy public member removal | MEDIUM | LOW | P1 (do with sprite rework, not separately) |
| Hot reload F5 + full state reset | HIGH | LOW | P1 |
| Script path from argv | MEDIUM | LOW | P1 (prerequisite for reload) |
| Docusaurus MDX angle bracket fix in generator | HIGH | LOW | P1 |
| API sidebar autogenerated | HIGH | LOW | P1 (already configured; unblock by fixing MDX) |
| Sprite flip (H/V) | MEDIUM | LOW | P2 |
| Layer visibility toggle | LOW | LOW | P3 |
| File-watch auto-reload | LOW | HIGH | P3 |

---

## Existing System Interfaces (Dependency Context)

These are the key existing APIs that new features must integrate with cleanly:

| System | Relevant Interface | Notes |
|--------|-------------------|-------|
| `Canvas4<W,H>` | `clear(Pixel4)`, `setPixel(x,y,Pixel4)`, `getPixel(x,y)` | Multi-layer uses 4 instances of this. |
| `LuaCanvas` | `canvasPtr`, `is4Bit`, wraps `ICanvas<Pixel4>` | Must be extended to hold pointer per layer. |
| `LuaBindings` | `setCanvas(LuaCanvas*)`, `registerAll()` | Hot reload calls `registerAll()` after reinit. |
| `LuaEngine` | `initialize()`, `shutdown()`, `executeFile()` | Hot reload uses the existing teardown/reinit cycle. |
| `g_rgb_staging[W*H*3]` | RGB24 output buffer for SDL texture upload | Multi-layer compositor writes into this buffer. |
| `expand_canvas_to_rgb()` | Static function in `sdl_main.cpp` | Becomes `composite_layers_to_rgb()` looping N=4. |
| `g_palette.isTransparent(v)` | Returns `v == 15` | Compositor skip condition per pixel per layer. |
| `generate-api-docs.js` | Node.js doc generator writing `.md` files | MDX fix patches the prose text escaping here. |

---

## Sources

- Direct code inspection: `src/scripting/bindings.cpp`, `include/enjin2/graphics/sprite.hpp`, `src/platform/sdl/sdl_main.cpp`, `src/scripting/lua_engine.cpp`
- Direct config inspection: `docs/docusaurus.config.js`, `docs/api-sidebar.js`, `docs/sidebars.js`
- MDX content inspection: `docs/api/graphics/CanvasExtended.md`, `docs/api/core/Scene.md` (grep for raw angle brackets vs. escaped)
- `.planning/PROJECT.md` — authoritative requirements list, out-of-scope decisions, validated constraints
- Domain knowledge: PICO-8 layer semantics, GBA/NES sprite sheet grid conventions, Docusaurus v3 MDX format detection behavior

---

*Feature research for: enjin2 v1.4 — multi-layer canvas, sprite rework, Lua hot reload, Docusaurus fix*
*Researched: 2026-02-24*
