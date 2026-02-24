# Architecture Research

**Domain:** Embedded/WASM 2D graphics engine — v1.4 feature integration
**Researched:** 2026-02-24
**Confidence:** HIGH (based on direct codebase inspection, no external lookup required)

---

## Current System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         SDL3 Runner (sdl_main.cpp)                  │
│  g_canvas (Canvas4<128,128>)  g_input (InputState)  g_lua          │
│  expand_canvas_to_rgb() → SDL_UpdateTexture → SDL_RenderTexture     │
└───────────────────────┬────────────────────────────────────────────┘
                        │ Canvas4<W,H>* / InputState*
┌───────────────────────▼────────────────────────────────────────────┐
│                     Scripting Layer (enjin2_lua)                    │
│  LuaScriptSystem → LuaEngine → LuaBindings → LuaCanvas            │
│  LuaCanvas wraps Canvas4* as void* (is4Bit flag)                   │
│  bindings.cpp: lua_clear, lua_setPixel, lua_rectangle, etc.        │
│  bindings.cpp: lua_setPaletteColor, input polling                  │
└───────────────────────┬────────────────────────────────────────────┘
                        │ ICanvas<Pixel4>&
┌───────────────────────▼────────────────────────────────────────────┐
│                    Graphics Layer (enjin2_graphics)                 │
│  Canvas4<W,H>  Canvas8<W,H>  Palette  Primitives  Effects         │
│  Single buffer: (WIDTH*HEIGHT)/2 bytes packed 4-bit                │
└───────────────────────┬────────────────────────────────────────────┘
                        │ ICanvas<Pixel4> interface
┌───────────────────────▼────────────────────────────────────────────┐
│                      Core Layer (enjin2_core)                       │
│  Object  Component  Scene  SceneStateMachine  Signal               │
│  ObjectCollection  memory.hpp  types.hpp (Pixel4, Point, Rect)     │
└─────────────────────────────────────────────────────────────────────┘
```

**Existing render path (v1.3):**

```
Frame start
  input_advance_frame(&g_input)
  input_platform_poll(&g_input)
  g_lua.callFunction("update", dt)      → Lua update(dt)
  g_lua.callFunction("draw")            → Lua draw(), writes to g_canvas
  expand_canvas_to_rgb()                → g_canvas → g_rgb_staging[]
  SDL_UpdateTexture(texture, ...)       → upload RGB24
  SDL_RenderTexture(renderer, ...)      → display
Frame end
```

---

## Feature 1: Multi-Layer Canvas Composition

### What Changes

**Existing:** One `Canvas4<128,128> g_canvas` in `sdl_main.cpp`. All draw calls target it. Blit reads from it directly.

**New:** Four `Canvas4<128,128>` buffers, one per logical layer. The `expand_canvas_to_rgb()` step composites them bottom-to-top (Background → Entities → Foreground → UI), treating `PALETTE_TRANSPARENT` (index 15) as passthrough.

### New Components

**`LayerStack` struct (new, in `include/enjin2/graphics/layer_stack.hpp`):**

```cpp
namespace enjin2 {

enum class LayerIndex : uint8_t {
    Background = 0,
    Entities   = 1,
    Foreground = 2,
    UI         = 3,
    COUNT      = 4
};

template<uint16_t W, uint16_t H>
struct LayerStack {
    Canvas4<W, H> layers[static_cast<size_t>(LayerIndex::COUNT)];

    Canvas4<W, H>& get(LayerIndex idx) {
        return layers[static_cast<size_t>(idx)];
    }

    void clearAll(Pixel4 color = Pixel4(0)) {
        for (auto& layer : layers) layer.clear(color);
    }

    void clearLayer(LayerIndex idx, Pixel4 color = Pixel4(0)) {
        get(idx).clear(color);
    }
};

} // namespace enjin2
```

Memory cost: 4 × (128×128)/2 = 4 × 8192 = 32768 bytes (32 KB). Acceptable on SDL3/WASM. Must be gated for bare ESP32 targets without PSRAM.

### Modified Components

**`sdl_main.cpp`:**
- Replace `Canvas4<128,128> g_canvas` with `LayerStack<128,128> g_layers`
- Replace `LuaCanvas g_lua_canvas(&g_canvas)` with `LuaCanvas g_lua_canvas(&g_layers.layers[0])` (Background as default)
- `expand_canvas_to_rgb()` becomes a composite loop: for each pixel, walk layers[0..3] bottom-up, take first non-transparent index

```cpp
static void expand_canvas_to_rgb() {
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            int i = (y * CANVAS_W + x) * 3;
            bool hit = false;
            for (int layer = static_cast<int>(LayerIndex::COUNT) - 1; layer >= 0; --layer) {
                Pixel4 px = g_layers.layers[layer].getPixel(x, y);
                if (!g_palette.isTransparent(px.value)) {
                    RGB rgb = g_palette.resolve(px.value);
                    g_rgb_staging[i + 0] = rgb.r;
                    g_rgb_staging[i + 1] = rgb.g;
                    g_rgb_staging[i + 2] = rgb.b;
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                g_rgb_staging[i + 0] = 0;
                g_rgb_staging[i + 1] = 0;
                g_rgb_staging[i + 2] = 0;
            }
        }
    }
}
```

**`LuaCanvas` / `LuaBindings` (`include/enjin2/scripting/bindings.hpp`, `src/scripting/bindings.cpp`):**

`LuaCanvas` currently holds `void* canvasPtr` plus `is4Bit` flag. To support layer switching, add:
- A `LayerStack<W,H>*` pointer alongside the existing canvas pointer
- `setActiveLayer(uint8_t idx)` which updates `canvasPtr` to point at `layerStack->layers[idx]`

Recommended approach: Add a second constructor for LayerStack and an `activeLayer` field. The `canvas.setLayer(n)` Lua binding calls `LuaCanvas::setActiveLayer(n)`.

**Lua bindings additions (`src/scripting/bindings.cpp`):**
- `canvas.setLayer(layerIndex)` — switches active draw target (0=Background, 1=Entities, 2=Foreground, 3=UI)
- `canvas.clearLayer(layerIndex, colorIndex)` — clears one specific layer
- `canvas.getLayerCount()` — returns 4 (constant)

### Data Flow (Multi-Layer)

```
Lua draw():
  canvas.setLayer(0)         → LuaCanvas.activeCanvas = &g_layers.layers[0]
  canvas.clear(0)            → clears Background layer to black
  canvas.fillRect(...)       → draws to Background layer
  canvas.setLayer(1)         → switch to Entities layer
  canvas.clear(15)           → clear Entities to transparent
  canvas.circle(...)         → draws to Entities layer

expand_canvas_to_rgb() [modified]:
  for each pixel (x, y):
    walk layers top-to-bottom (UI=3 down to Background=0)
    first non-transparent pixel wins → resolve palette → write RGB
    no non-transparent pixel found → write black (0,0,0)
```

### Integration Points

| Touches | What Changes |
|---------|-------------|
| `sdl_main.cpp` | `g_canvas` → `g_layers`; compositor replaces direct blit; `g_lua_canvas` constructor updated |
| `include/enjin2/scripting/bindings.hpp` | `LuaCanvas` gains `setActiveLayer(uint8_t)` method and layer stack pointer |
| `src/scripting/bindings.cpp` | Add `lua_setLayer`, `lua_clearLayer`, `lua_getLayerCount` |
| `include/enjin2/graphics/layer_stack.hpp` | **NEW FILE** — header-only, no `.cpp` needed |
| CMakeLists.txt | No change — `layer_stack.hpp` is header-only |
| WASM bindings (`emscripten_bindings.cpp`) | Out of scope for v1.4; single-canvas blit unchanged on WASM |

### Zero-Alloc Compliance

`LayerStack` uses the same static template storage as `Canvas4`. Declared as a static global in `sdl_main.cpp` — no heap. The compositor loop writes into the existing `g_rgb_staging[]` static buffer.

---

## Feature 2: Sprite System Rework

### Current State

- `graphics/sprite.hpp` — `Sprite` class: raw `const uint8_t* texture`, `frame` index, manual `setTexture()` calls, `draw(ICanvas<uint8_t>&)` — takes 8-bit canvas only
- `components/sprite.hpp` — `C_Sprite : C_Drawable`: wraps `Sprite`, syncs from `C_Position`, calls `sprite.draw(canvas)`
- `components/image_cache.hpp` — `C_ImageCache`: 32KB static linear buffer, `ImageEntry` struct (offset, size, width, height, frames)
- Frame stride: `texture + (frame * width * height)` — sequential flat array, no sheet grid

**Problems:**
1. `Sprite::draw()` only accepts `ICanvas<uint8_t>&` — incompatible with `Canvas4` (`ICanvas<Pixel4>`) which is the production format
2. No sprite sheet support — frames must be stored as sequential raw pixel strips
3. No time-based animation — frame changes are manual
4. Duplicate legacy public members (`_width`, `_height`, etc.) alongside private members
5. `C_Sprite` cannot draw to the new layer-aware canvas without modification

### New Components

**`SpriteSheet` struct (new, in `include/enjin2/graphics/sprite_sheet.hpp`):**

```cpp
namespace enjin2 {

struct SpriteSheet {
    const uint8_t* data;   // raw pixel data (4-bit packed or 8-bit flat)
    uint8_t frameW;        // width of one frame in pixels
    uint8_t frameH;        // height of one frame in pixels
    uint8_t cols;          // number of columns in the sheet grid
    uint8_t rows;          // number of rows in the sheet grid
    uint8_t frameCount;    // total frames (may be < cols*rows for partial last row)

    // Byte offset for frame N (8-bit storage, sequential)
    size_t frameOffset(uint8_t frameIdx) const {
        return static_cast<size_t>(frameIdx) * frameW * frameH;
    }
};

} // namespace enjin2
```

**`FrameAnimation` struct (new, in `include/enjin2/graphics/frame_animation.hpp`):**

```cpp
namespace enjin2 {

enum class LoopMode : uint8_t { Once, Loop, PingPong };

struct FrameAnimation {
    float fps         = 8.0f;
    uint8_t first     = 0;
    uint8_t last      = 0;
    LoopMode mode     = LoopMode::Loop;

    // Mutable playback state
    float accumulator = 0.0f;
    uint8_t current   = 0;
    int8_t direction  = 1;    // +1 or -1 for PingPong

    // Returns true if a full cycle completed this step
    bool advance(float dt);
    void reset();
    bool done() const;   // true when Once mode has reached last frame
};

} // namespace enjin2
```

**New `Sprite` class (replacement in `include/enjin2/graphics/sprite.hpp`):**

```cpp
namespace enjin2 {

class Sprite {
public:
    Sprite() = default;

    void setSheet(const SpriteSheet* sheet);
    void setPosition(int16_t x, int16_t y);
    void setFrame(uint8_t frameIdx);
    void setTransparentIndex(uint8_t idx);   // replaces setMatte()
    void setBlendMode(BlendMode mode);

    // Template draw: works with Canvas4<W,H> (ICanvas<Pixel4>)
    // and Canvas8<W,H> (ICanvas<uint8_t>)
    template<typename TPixel>
    void draw(ICanvas<TPixel>& canvas) const;

    const SpriteSheet* getSheet() const;
    uint8_t getFrame() const;
    Point getPosition() const;
    uint8_t getTransparentIndex() const;

private:
    const SpriteSheet* sheet_          = nullptr;
    Point position_                    = {0, 0};
    uint8_t frame_                     = 0;
    uint8_t transparentIdx_            = 15;   // PALETTE_TRANSPARENT
    BlendMode blendMode_               = BlendMode::Normal;
};

} // namespace enjin2
```

### Modified Components

**`C_Sprite` (`include/enjin2/components/sprite.hpp`):**
- Gains `FrameAnimation animation_` member (value, not pointer)
- `update(uint16_t deltaTime)` override calls `animation_.advance(dt / 1000.0f)` then `sprite_.setFrame(animation_.current)`
- `draw(ICanvas<uint8_t>& canvas)` override unchanged in signature — `Sprite::draw<uint8_t>()` is called internally

**Critical architectural constraint:** `C_Drawable::draw(ICanvas<uint8_t>& canvas)` is the ECS pipeline virtual interface. CONCERNS.md documents that Canvas4 (4-bit) is not supported by `Scene::renderObjects()` — this is a pre-existing limitation. The sprite rework does NOT fix this ECS pipeline gap. `Sprite::draw<TPixel>()` becomes template-capable but `C_Sprite::draw()` keeps the `uint8_t` signature. Lua scripts calling `canvas.drawSprite()` directly bypass `C_Sprite` entirely and call `Sprite::draw<Pixel4>()` on the active layer canvas.

**`C_ImageCache` (`include/enjin2/components/image_cache.hpp`):**
- `ImageEntry` gains `uint8_t cols` and `uint8_t rows` fields for sheet grid metadata
- `AddImage()` gains optional `cols` and `rows` parameters, defaulting to `cols=1, rows=frameCount` for backward-compatible sequential storage
- `GetImageData()` unchanged — returns pointer to frame start, grid interpretation lives in `SpriteSheet`

**`LuaBindings` (`src/scripting/bindings.cpp`):**

Stateless Lua sprite API (preferred over stateful object API — no allocation, no per-sprite Lua table):

```lua
-- Draw a single frame from raw data
canvas.drawSprite(data, x, y, frameW, frameH, frameIdx, transparentIdx)
-- data: lightuserdata pointer to pixel buffer (from ImageCache)
```

This is the simplest zero-alloc design. The Lua script manages frame state in Lua variables (integers), not C++ objects.

### Integration Points

| Touches | What Changes |
|---------|-------------|
| `include/enjin2/graphics/sprite.hpp` | Full rewrite: template `draw<TPixel>()`, clean API, no legacy `_*` members |
| `include/enjin2/graphics/sprite_sheet.hpp` | **NEW FILE** |
| `include/enjin2/graphics/frame_animation.hpp` | **NEW FILE** |
| `src/graphics/sprite.cpp` | **NEW FILE** — `FrameAnimation::advance()` implementation |
| `include/enjin2/components/sprite.hpp` | Add `FrameAnimation animation_` member; `update()` advances it |
| `include/enjin2/components/image_cache.hpp` | `ImageEntry` adds `cols`/`rows` |
| `src/components/image_cache.cpp` | `AddImage()` populates grid metadata |
| `src/scripting/bindings.cpp` | Add `canvas.drawSprite(...)` Lua binding |
| `include/enjin2/scripting/bindings.hpp` | Declare `lua_drawSprite` static method |
| CMakeLists.txt | Add `src/graphics/sprite.cpp` to `enjin2_graphics` sources |

### Zero-Alloc Compliance

- `SpriteSheet` is a plain struct with a `const uint8_t*` into static `C_ImageCache::textureCache`
- `FrameAnimation` is a value-type member inside `C_Sprite` — no heap
- Stateless Lua sprite binding allocates nothing — frame pointer computed on C++ stack per call

---

## Feature 3: Lua Hot Reload

### Current State

`sdl_main.cpp` event loop:

```cpp
while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) { running = false; }
    else if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE) { running = false; }
    }
}
```

`LuaScriptSystem::shutdown()` calls `LuaEngine::shutdown()` → `lua_close(L)`. `LuaScriptSystem::initialize()` calls `LuaEngine::initialize()` → `lua_newstate(luaAllocator, this)`.

**Key fragility (from CONCERNS.md):** `LuaEngine::memoryPool` and `LuaEngine::memoryUsed` are `static` class members. A second `initialize()` call resets `memoryUsed = 0`. In `sdl_main.cpp` all Lua calls go through `g_lua` exclusively, so this is safe — no external code holds stale `lua_State*` pointers.

**Key fragility:** `static LuaBindings* g_currentBindings = nullptr` in `bindings.cpp` is set by `LuaBindings` constructor. After `shutdown()` + `initialize()`, `registerAll()` must be called to re-register bindings and re-set `g_currentBindings`.

### New Logic

No new files. Changes within `sdl_main.cpp` and `include/enjin2/scripting/bindings.hpp`.

**`LuaScriptSystem` — add reload support:**

```cpp
// bindings.hpp additions to LuaScriptSystem:
private:
    std::string lastScriptPath_;

public:
    LuaResult loadScript(const std::string& filename) {
        lastScriptPath_ = filename;
        return engine.executeFile(filename);
    }

    LuaResult reload() {
        if (lastScriptPath_.empty()) return LuaResult("No script loaded");
        shutdown();
        if (!initialize()) return LuaResult("Lua init failed on reload");
        setCanvas(canvas);                         // re-wire canvas
        bindings.setInput(/* stored input ptr */); // re-wire input
        return loadScript(lastScriptPath_);
    }
```

The SDL runner stores `g_input` as a static global, so after reload `g_lua.getBindings().setInput(&g_input)` re-wires it.

**`sdl_main.cpp` event loop addition:**

```cpp
} else if (event.type == SDL_EVENT_KEY_DOWN) {
    if (event.key.key == SDLK_ESCAPE) {
        running = false;
#ifdef ENJIN2_BUILD_LUA
    } else if (event.key.scancode == SDL_SCANCODE_F5) {
        g_lua.shutdown();
        if (!g_lua.initialize()) {
            std::cerr << "[hot reload] Lua init failed\n";
        } else {
            g_lua.setCanvas(&g_lua_canvas);
            g_lua.getBindings().setInput(&g_input);
            LuaResult r = g_lua.loadScript(last_script_path); // tracked string
            if (!r.success) {
                std::cerr << "[hot reload] " << r.error << "\n";
            }
        }
#endif
    }
}
```

Alternatively, using `g_lua.reload()` if the method is added to `LuaScriptSystem`. Either approach is valid.

### Integration Points

| Touches | What Changes |
|---------|-------------|
| `src/platform/sdl/sdl_main.cpp` | Add `SDL_SCANCODE_F5` handler in event loop |
| `include/enjin2/scripting/bindings.hpp` | Add `lastScriptPath_` member and `reload()` to `LuaScriptSystem` |
| `src/scripting/lua_engine.cpp` | No change — `shutdown()`/`initialize()` already work correctly |

### Zero-Alloc Compliance

The `reload()` path performs no heap allocation beyond what `LuaEngine::initialize()` already does (Lua memory pool reset). `lastScriptPath_` is `std::string` — consistent with existing `std::string` use throughout `LuaEngine` (see `loadedScripts: std::vector<std::string>`).

---

## Feature 4: Docusaurus MDX Navigation Fix

### Current State

`docusaurus.config.js` navbar item:

```js
{
  type: 'docSidebar',
  docsPluginId: 'api',
  sidebarId: 'apiSidebar',
  position: 'left',
  label: 'API Reference',
}
```

`api-sidebar.js` exports `{ apiSidebar: [{ type: 'autogenerated', dirName: '.' }] }`.

`npm run build` completes without errors. The tech debt note in `PROJECT.md` says "API navigation disabled in Docusaurus due to MDX syntax issues (carried from v1.0)".

### Root Cause Analysis

Docusaurus 3.x uses MDX v3 by default. MDX v3 parses `<` as a JSX tag opener. C++ API docs routinely contain template syntax like `ICanvas<TPixel>`. When `generate-api-docs.js` inserts extracted text containing raw `<` characters into markdown output, the MDX parser fails or produces garbled output.

The build succeeds because `markdown.format: 'detect'` causes `.md` files to be parsed as CommonMark, not MDX — but only when the parser sees no JSX-like tokens. A single unescaped `<TypeName>` in one file is enough to trigger MDX mode parsing on that file, causing a build-time error or blank page.

Inspected: `docs/api/graphics/CanvasExtended.md` line 12 contains `ICanvas<TPixel>` unescaped in a template parameter description. The `escapeForMdx()` function in `generate-api-docs.js` exists but is not called consistently for all text insertion points — specifically template parameter list extraction paths.

### Fix

Two surgical changes to `scripts/generate-api-docs.js`:

1. Audit all locations where `extractText()` result is interpolated directly into the markdown string template. Wrap those insertions with `escapeForMdx()`.

2. In the method signature formatter (`formatMethod()`), ensure return type and parameter type strings go through `escapeForMdx()` after type formatting — `formatType()` already handles HTML entity decode/re-encode but the final output may still emit `<` in rare edge cases.

Then regenerate all API docs:

```bash
node scripts/generate-api-docs.js
```

This regeneration must be committed alongside the script fix so the published `docs/api/` files reflect the fix.

**No change to `docusaurus.config.js` or `api-sidebar.js` — the configuration is correct.**

### Integration Points

| Touches | What Changes |
|---------|-------------|
| `scripts/generate-api-docs.js` | Wrap template param and description insertions with `escapeForMdx()` |
| `docs/api/**/*.md` | Regenerated — any unescaped `<Type>` becomes `&lt;Type&gt;` |
| `docs/docusaurus.config.js` | No change |
| `docs/api-sidebar.js` | No change |

---

## Build Order (Phase Dependencies)

### Dependency Graph

```
[Phase A: Docusaurus MDX Fix]   — independent, no C++ changes
        |
        done

[Phase B: Sprite Rework]        — independent, existing Canvas4/ICanvas unchanged
        |
        V
[Phase C: Multi-Layer Composition] — sdl_main.cpp touches same globals as sprite bindings
        |
        V
[Phase D: Lua Hot Reload]       — reload must re-wire layer canvas pointers; needs C done first
```

### Phase Rationale

**Phase A (Docusaurus):** Zero runtime risk. Pure tooling change. Can be done first or last — no C++ dependency. Do it first to clear the oldest-standing tech debt item before adding new complexity.

**Phase B (Sprite Rework):** Independent of layers. New structs (`SpriteSheet`, `FrameAnimation`) and a reworked `Sprite` class. `C_ImageCache` change is additive. Lua `canvas.drawSprite()` can be added before layer switching exists because it targets whatever canvas is currently active — single or multi-layer.

**Phase C (Multi-Layer):** Modifies `sdl_main.cpp` globals and the `LuaCanvas` layer pointer. Must come after sprite bindings are in place so `canvas.drawSprite()` works correctly with layer-aware canvas. The compositor loop replaces `expand_canvas_to_rgb()` — one focused change.

**Phase D (Hot Reload):** Trivially small if done after Phase C because the canvas re-wiring logic in `reload()` already knows about `g_layers` (not just `g_canvas`). If done before Phase C, `reload()` would need to be updated again when layers land.

---

## Component Responsibilities After v1.4

| Component | Status | What Changes |
|-----------|--------|-------------|
| `Canvas4<W,H>` | Unchanged | Instantiated 4× in `LayerStack` |
| `LayerStack<W,H>` | **NEW** | Owns 4 `Canvas4` buffers, exposes by `LayerIndex` |
| `expand_canvas_to_rgb()` | Modified | Composite walk: top layer wins per pixel |
| `Sprite` | Rewritten | Template `draw<TPixel>()`, clean API, no legacy members |
| `SpriteSheet` | **NEW** | Grid dimensions + frame count descriptor |
| `FrameAnimation` | **NEW** | FPS, loop mode, float accumulator |
| `C_Sprite` | Modified | Holds `FrameAnimation`; `update()` advances it |
| `C_ImageCache` | Modified | `ImageEntry` adds `cols`/`rows`; backward compatible |
| `LuaCanvas` | Modified | Gains `setActiveLayer(uint8_t)` + layer stack pointer |
| `LuaBindings` | Modified | + `canvas.setLayer()`, `canvas.clearLayer()`, `canvas.drawSprite()` |
| `LuaScriptSystem` | Modified | Gains `lastScriptPath_` + `reload()` |
| `sdl_main.cpp` | Modified | 4-layer global; F5 hot reload; composite blit |
| `generate-api-docs.js` | Modified | Template param text escaping; regenerate `docs/api/` |

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Layer Canvas Pointer Dispatch Per Draw Call

**What people do:** Store the `LayerStack` pointer in `LuaCanvas` and index it on every draw call using the layer index passed to each drawing function.

**Why it's wrong:** Conflates "which layer am I targeting" with every individual draw operation. Adds a branch and index dereference to every `setPixel`, `fillRect`, etc. Makes the Lua API verbose and error-prone.

**Do this instead:** `canvas.setLayer(n)` updates the `Canvas4*` pointer in `LuaCanvas` once. All subsequent draw calls use the cached pointer. Layer switching is explicit and O(1).

### Anti-Pattern 2: Full Canvas Clear on Hot Reload

**What people do:** Clear all layer canvases and reset palette on F5.

**Why it's wrong:** Canvas state is stale for exactly one frame after reload — the next Lua `draw()` call overwrites it. Resetting palette is actively harmful if the user has set a custom palette via Lua.

**Do this instead:** Only reset the Lua state. Do not touch canvas buffers, palette, or input state.

### Anti-Pattern 3: FrameAnimation as a Separate Component

**What people do:** Create `C_FrameAnimation : Component` that `C_Sprite` queries via `getComponent<C_FrameAnimation>()`.

**Why it's wrong:** Consumes one of the hard-capped 16 component slots per `Object`. Requires inter-component `dynamic_cast` lookup (O(n) per frame per component, per CONCERNS.md). Adds complexity for no gain.

**Do this instead:** `FrameAnimation` is a value-type member held directly inside `C_Sprite`. No extra component slot used.

### Anti-Pattern 4: Emitting `.mdx` from generate-api-docs.js

**What people do:** Change the generator to emit `.mdx` files to enable Docusaurus MDX features.

**Why it's wrong:** C++ API docs are dense with template syntax (`Canvas4<W,H>`, `ICanvas<TPixel>`). Every `<` must be escaped in MDX. Every future Doxygen change that introduces new template syntax creates an escaping defect. The maintenance burden is perpetual.

**Do this instead:** Keep `.md` output. The `format: 'detect'` setting already handles this correctly when the generated markdown contains only escaped HTML entities for angle brackets.

---

## Memory Budget

| Item | Size | Platform |
|------|------|----------|
| Existing `Canvas4<128,128>` | 8192 bytes | All |
| 3 additional layer canvases | 24576 bytes | SDL3/WASM |
| `LayerStack<128,128>` total | 32768 bytes | SDL3/WASM |
| `g_rgb_staging[128*128*3]` | 49152 bytes | SDL3 only |
| `C_ImageCache::textureCache` | 32768 bytes | All |
| Lua memory pool | 1MB (desktop) / 64KB (ESP32) | Scripting |

Adding 3 extra layer canvases costs 24 KB. On SDL3 this is trivial. On bare ESP32 (no PSRAM), the 4-layer approach is likely not viable — multi-layer should be treated as an SDL3/WASM feature for v1.4. The `LayerStack` template can be instantiated with `COUNT=1` as a fallback, or simply not used on ESP32 builds.

---

## Sources

- Direct codebase inspection: `include/enjin2/graphics/canvas.hpp`, `graphics/sprite.hpp`, `graphics/palette.hpp`
- `src/platform/sdl/sdl_main.cpp` — full SDL3 runner logic
- `include/enjin2/scripting/bindings.hpp`, `src/scripting/bindings.cpp` — Lua binding internals
- `include/enjin2/scripting/lua_engine.hpp` — `shutdown()`/`initialize()` lifecycle
- `.planning/codebase/ARCHITECTURE.md` — existing layer/component map (2026-02-23)
- `.planning/codebase/CONCERNS.md` — Canvas4 in ECS pipeline limitation, static memory pool fragility
- `docs/docusaurus.config.js`, `scripts/generate-api-docs.js`, `docs/api/` sample files
- `CMakeLists.txt` — target structure, conditional Lua link

---

*Architecture research for: enjin2 v1.4 — multi-layer, sprite rework, hot reload, Docusaurus fix*
*Researched: 2026-02-24*
