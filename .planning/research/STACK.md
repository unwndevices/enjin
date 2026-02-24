# Stack Research

**Domain:** C++ embedded graphics engine — multi-layer composition, sprite system rework, Lua hot reload, Docusaurus navigation fix
**Researched:** 2026-02-24
**Confidence:** HIGH

## Context

This is an additive stack for v1.4. The existing stack is unchanged:
- C++17, CMake 3.16+, Lua 5.4, SDL3 3.4.2, stb_image_write, Emscripten
- Canvas4 (4-bit packed, 2px/byte), ICanvas<TPixel>, static arrays only, zero heap

This document covers only what is new or modified for:

1. Multi-layer composition: 4 independent Canvas4 buffers composited at blit time
2. Sprites rework: clean API, uniform grid sprite sheets, FPS frame animation
3. Lua hot reload: F5 in SDL3 reloads script with full state reset
4. Docusaurus MDX navigation fix: stale placeholder text and guide cross-links

**No new C++ dependencies are needed.** All four features are implementable with existing stack. The entries below explain the patterns, API references, and in-engine implementation choices.

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++17 (existing) | — | All new features | No upgrade. Static arrays with template parameters cover fixed-layer composition. No library adds value here. |
| SDL3 (existing) | 3.4.2 | Hot reload key event (`SDL_SCANCODE_F5`), blit of 4 composited layers | SDL3 `SDL_EVENT_KEY_DOWN` with `event.key.scancode == SDL_SCANCODE_F5` and `!event.key.repeat` is the idiomatic hot-reload trigger pattern per the SDL3 official wiki. No file-watching API needed — the reload is user-initiated via key, not filesystem-driven. |
| Lua 5.4 (existing) | 5.4.x | Script reload via `lua_close` + `lua_newstate` sequence | `LuaEngine::shutdown()` already calls `lua_close`. Reload = `shutdown()` → re-`initialize()` → re-`loadScript(path)`. The path must be stored statically (C string, no heap). Canvas and input state do not need reset — only the Lua state. |
| Docusaurus 3.9.2 (existing) | 3.9.2 | API reference navigation | Already at latest stable. MDX escaping already implemented in `generate-api-docs.js` via `escapeForMdx()`. No upgrade needed. The remaining DOC-01/02 issue is stale placeholder text in guide `.md` files — not a Docusaurus version or config problem. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| None new | — | — | All four v1.4 features use existing dependencies only. See patterns below. |

### Reference Designs (API Inspiration Only — Not Dependencies)

| Reference | Version | What to Borrow | What to Ignore |
|-----------|---------|---------------|----------------|
| PICO-8 sprite API | (fantasy console) | `spr(n, x, y)` call shape, uniform 8×8 grid indexing, sprite number = row*cols + col, frame animation via incrementing sprite index | PICO-8's shared spritesheet/tilemap memory layout, its fixed 128×128 display constraint, `fget`/`fset` sprite flags (out of scope for v1.4) |
| BEEP-8 SDK | (embedded PICO-8 port in C++) | Static VRAM bank concept, `lsp()` as a load-sprite-sheet call, 4-bit color throughout | VRAM bank switching, full hardware abstraction layer (enjin2 handles differently) |
| LÖVE 2D (love2d.graphics) | 11.x | `love.graphics.draw(sprite, x, y)` call shape, `Quad` for sub-region selection | love2d's heap allocation model, its Image/Texture GPU upload pattern |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `docusaurus-mdx-checker` | Validate MDX syntax in generated API docs | Run `npx docusaurus-mdx-checker` from `docs/` to scan for remaining MDX issues. Useful after regenerating API pages. Not needed for guide `.md` files. |
| Doxygen (existing) + `generate-api-docs.js` (existing) | Regenerate API markdown after sprite/layer classes added | No changes to the toolchain. New C++ classes (e.g., `SpriteSheet`, `LayerStack`) must follow existing Doxygen comment standard. |

---

## Installation

No new packages. Existing system dependencies sufficient:

```bash
# Verify SDL3 present (needed for hot reload testing)
pacman -Qi sdl3   # Arch Linux

# Verify Lua present
lua -v            # should show 5.4.x

# Verify Docusaurus build works
cd docs && npm run build
```

---

## Implementation Patterns

### Pattern 1: Multi-Layer Canvas4 Composition (zero-alloc)

Four Canvas4 buffers are statically allocated. Composition happens at blit time in `expand_canvas_to_rgb()`, not at draw time.

```cpp
// Static layer buffers — declared in sdl_main.cpp alongside g_canvas
static enjin2::Canvas4<CANVAS_W, CANVAS_H> g_layers[4];

// Compositing order: layer 0 (bottom) to layer 3 (top)
// Index 15 = transparent (existing convention), skip during composite
static void expand_layers_to_rgb() {
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            enjin2::RGB out = {0, 0, 0};
            for (int l = 0; l < 4; l++) {
                enjin2::Pixel4 px = g_layers[l].getPixel(x, y);
                if (!enjin2::g_palette.isTransparent(px.value)) {
                    out = enjin2::g_palette.resolve(px.value);
                }
            }
            int i = (y * CANVAS_W + x) * 3;
            g_rgb_staging[i + 0] = out.r;
            g_rgb_staging[i + 1] = out.g;
            g_rgb_staging[i + 2] = out.b;
        }
    }
}
```

The pattern is painter's algorithm: each non-transparent pixel in a higher layer overwrites lower layers. No alpha blending (consistent with existing chroma-key transparency model). Memory cost: 4 × (128×128÷2) = 32,768 bytes total for 4 layers — fits comfortably in ESP32 PSRAM and desktop.

Lua API maps layer indices 0–3 directly: `canvas.setActiveLayer(n)` routes subsequent draw calls to `g_layers[n]`. LuaCanvas wraps a pointer-to-layer, updated on each `setActiveLayer()` call.

### Pattern 2: Sprite Sheet (zero-alloc, uniform grid)

PICO-8's model is the right reference: sprites are identified by index, sheet is a uniform grid, sprite size is fixed at construction.

```cpp
struct SpriteSheet {
    const uint8_t* data;   // pointer to ROM/flash data (4-bit packed, no heap)
    uint8_t sprite_w;      // sprite cell width in pixels
    uint8_t sprite_h;      // sprite cell height in pixels
    uint8_t cols;          // number of columns in sheet

    // Sprite index -> pixel address in packed 4-bit buffer
    // row = index / cols, col = index % cols
    // byte offset = (row * sprite_h * sheet_width_bytes) + (col * sprite_w / 2)
};

// Draw call: blit sprite index `n` at (x, y) to target canvas
// Transparent: index 15 (existing convention, no change)
void drawSprite(ICanvas<Pixel4>& canvas, const SpriteSheet& sheet,
                uint8_t n, int16_t x, int16_t y);
```

Frame animation: caller advances `n` by 1 per desired interval. The `SpriteAnimator` struct holds `{ sheet, start_frame, frame_count, fps, float elapsed }` — all stack-allocated. `update(float dt)` increments elapsed, advances frame when `elapsed >= 1.0f/fps`.

No heap. No dynamic frame list. `frame_count` is a compile-time-known constant.

Lua API: `spr(n, x, y)` (PICO-8 style, familiar), `spr_anim(start, count, fps, x, y)` for animated calls.

### Pattern 3: Lua Hot Reload (SDL3 event, full state reset)

The existing SDL3 event loop already handles `SDL_EVENT_KEY_DOWN`. Add F5 detection with `!event.key.repeat` guard.

```cpp
// In the event pump inside main():
} else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
    if (event.key.scancode == SDL_SCANCODE_F5) {
        g_reload_requested = true;  // static bool, set flag, execute after pump
    }
}

// After event pump, before Lua calls:
if (g_reload_requested) {
    g_reload_requested = false;
    // Clear all layers
    for (auto& layer : g_layers) layer.clear(enjin2::Pixel4(0));
    // Full Lua state reset
    g_lua.shutdown();
    if (g_lua.initialize()) {
        g_lua.setCanvas(&g_lua_canvas);
        g_lua.getBindings().setInput(&g_input);
        g_lua.loadScript(g_script_path);  // static char[] path, no heap
    }
}
```

Key constraints: `g_script_path` must be a `static char[256]` (not `std::string`, to be safe in constrained builds). The reload clears canvas layers to prevent visual artifacts from the previous script run. Input state (`g_input`) is NOT reset — frame state is mid-frame irrelevant.

The `!event.key.repeat` guard is critical: without it, holding F5 triggers reload on every repeat event (typically 30+ times/second), causing churn. SDL3 wiki documents this as the canonical guard for action triggers.

### Pattern 4: Docusaurus Navigation Fix

The build already succeeds (`npm run build` passes clean). The API plugin is correctly configured with `docsPluginId: 'api'` and `sidebarId: 'apiSidebar'`. The API pages exist under `/enjin/api/`.

The remaining issue is cosmetic: guide `.md` files (`intro.md`, `getting-started.md`, `canvas.md`, `sprites.md`, `components.md`, `text-rendering.md`, `scene-management.md`, `scene-transitions.md`) contain stale placeholder text:

> "API Reference documentation will be available in the next phase."

These lines must be replaced with actual cross-links to the now-live API pages, e.g.:

```markdown
# Before (stale)
*Note: API Reference documentation will be available in the next phase.*

# After (correct)
See the [Canvas4 API reference](/api/graphics/Canvas4) for complete method signatures.
```

No MDX syntax fix is needed — `generate-api-docs.js` already escapes `<` → `&lt;` in method signatures. The one remaining unescaped instance in `CanvasExtended.md` line 12 (`ICanvas<TPixel>`) is in a paragraph node (not a heading or method signature), MDX v3 renders it fine as text content. Verify with `npm run build` before closing DOC-02.

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Static `Canvas4[4]` layer array | `std::array<Canvas4<W,H>, 4>` | Identical — use `std::array` if iterating with range-for is cleaner; both are stack-allocated |
| Painter's algorithm composite | Porter-Duff alpha compositing | Only if full per-pixel alpha is needed. Index 15 chroma-key is sufficient for 4-bit palette sprites; PROJECT.md explicitly excludes full alpha blending |
| `SpriteSheet` as plain struct + free functions | `Sprite` class with methods | The existing `Sprite` class stores per-sprite state (position, frame, matte) but lacks the sprite-sheet abstraction. The rework adds `SpriteSheet` as a separate concept (the sheet data) and keeps/replaces `Sprite` as the per-instance drawable |
| F5 hot reload (user-initiated) | `inotify`/filesystem watcher | inotify is Linux-only and adds platform complexity. F5 is simpler, cross-platform, and sufficient for the rapid-iteration use case. HOT-01 spec says "F5 full reset" |
| Lua `shutdown()` + `initialize()` | `lua_settop(L, 0)` + `luaL_dofile()` | Resetting only the stack leaves registered globals, loaded modules, and any C closures from the previous script run intact. Full `lua_close` + `lua_newstate` is the only correct clean-slate reload |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| Heap-allocated layer buffers (`new Canvas4`) | Breaks zero-alloc constraint; forbidden on ESP32 | Static `Canvas4<W,H>` array declared at file scope in sdl_main.cpp |
| Alpha blending for layer composite | Requires float or fixed-point multiply per pixel per frame; too expensive on ESP32; PROJECT.md explicitly excludes it | Painter's algorithm: last non-transparent index 15 pixel wins |
| `inotify` / `FSEvents` file watching | Platform-specific (Linux/macOS only), adds a file descriptor, complicates shutdown | SDL3 `SDL_EVENT_KEY_DOWN` with `SDL_SCANCODE_F5` — explicit user trigger, zero platform complexity |
| `std::string` for the script path in hot reload | May trigger heap allocation in constrained builds; C string avoids any edge case | `static char g_script_path[256]` set at startup from `argv` or hardcoded default |
| `SDL_SCANCODE_R` for reload (instead of F5) | The SDL3 wiki uses R in its example, but R conflicts with any game that uses R as a gameplay key | `SDL_SCANCODE_F5` — function keys have no gameplay meaning, matches IDE/browser "refresh" convention |
| Docusaurus upgrade to 3.10+ | No navigation bug exists in 3.9.2 that requires an upgrade; the issue is stale content, not a framework bug | Stay on 3.9.2, fix guide `.md` placeholder text only |
| `npx docusaurus-mdx-checker` for every rebuild | Unnecessary overhead; MDX escaping is already correct in the generator | Run once after adding new classes to verify no regressions |

---

## Stack Patterns by Variant

**Multi-layer on SDL3 desktop (primary target):**
- 4x `static Canvas4<128,128>` = 32,768 bytes total — fits in BSS segment
- Lua sets active layer per draw call; compositor reads all 4 layers at blit time
- Layer 0 = background, layers 1-3 = progressively foreground
- Clear individual layers with `layer.clear(Pixel4(0))` (index 0, not transparent 15)

**Multi-layer on WASM:**
- Same static array declaration in WASM entry point
- WASM JS renderer calls `getPaletteRGB()` once; compositor loop is identical C++
- No JS-side changes needed for layer support

**Multi-layer on ESP32:**
- 32,768 bytes fits in PSRAM (ESP32-S3 has 8MB PSRAM typically)
- Must verify PSRAM availability; if not, reduce to 2 layers (16,384 bytes in SRAM)
- ESP32 display driver receives final composited RGB buffer unchanged

**Sprite sheet data storage:**
- Desktop/WASM: `static const uint8_t` arrays in source files (compile-time ROM)
- ESP32: `static const uint8_t PROGMEM` arrays in flash
- `SpriteSheet` struct holds only a raw pointer — no ownership, no lifetime management

---

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| SDL3 3.4.2 | `SDL_SCANCODE_F5` | F5 scancode exists since SDL2; unchanged in SDL3. No version-specific concern. |
| Lua 5.4.x | `lua_close` + `lua_newstate` pattern | Stable Lua C API since 5.1. `lua_close` frees all Lua state; `lua_newstate` + `luaL_openlibs` recreates it clean. |
| Docusaurus 3.9.2 | `docsPluginId` in navbar | The `docsPluginId` field for secondary plugin instances is supported since Docusaurus 2.x. No compatibility issue. |
| Canvas4<128,128> × 4 | ESP32-S3 PSRAM | 32,768 bytes. ESP32-S3 PSRAM (8MB typical) easily accommodates this. Verify with `ESP.getFreePsram()` at startup. |

---

## Sources

- [SDL3 BestKeyboardPractices — SDL Wiki](https://wiki.libsdl.org/SDL3/BestKeyboardPractices) — Official hot-reload scancode pattern, `!event.key.repeat` guard, HIGH confidence
- [SDL3 SDL_KeyboardEvent — SDL Wiki](https://wiki.libsdl.org/SDL3/SDL_KeyboardEvent) — `scancode`, `repeat`, `key` fields verified, HIGH confidence
- [Docusaurus v3 MDX migration — docusaurus.io](https://docusaurus.io/blog/preparing-your-site-for-docusaurus-v3) — `<T>` angle bracket escaping cause and fix, HIGH confidence
- [Docusaurus 3.9.0 changelog — docusaurus.io](https://docusaurus.io/changelog/3.9.0) — Verified 3.9.2 is current stable, HIGH confidence
- [PICO-8 API reference — pico-8.github.io](https://pico-8.github.io/pico8-api/) — `spr(n, x, y)` API shape, uniform grid sprite numbering, MEDIUM confidence (reference design, not dependency)
- [BEEP-8 SDK pico8.h — beep8.github.io](https://beep8.github.io/beep8-sdk/api/BEEP8_HELPER/html/pico8_8h.html) — C++ embedded PICO-8 port, 4-bit VRAM sprite banks, MEDIUM confidence (reference design, not dependency)
- `docs/api/graphics/Canvas4.md` — Verified current API, `escapeForMdx()` working, no unescaped `<` in method signatures
- `scripts/generate-api-docs.js` lines 161-167 — `escapeForMdx()` function verified, `<` → `&lt;` replacement confirmed
- `src/platform/sdl/sdl_main.cpp` — Existing event loop structure, Lua init/shutdown pattern, confirmed prior to this research

---
*Stack research for: enjin2 v1.4 — multi-layer composition, sprite rework, Lua hot reload, Docusaurus navigation fix*
*Researched: 2026-02-24*
