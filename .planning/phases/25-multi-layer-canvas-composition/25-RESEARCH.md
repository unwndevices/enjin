# Phase 25: Multi-Layer Canvas Composition - Research

**Researched:** 2026-02-26
**Domain:** C++ canvas compositor, Lua layer API, SDL3 blit pipeline
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Composition & Stacking (C++ Engine)**
- Layer 0 = backmost (background), layer 3 = frontmost (top) — standard painter's order
- Layer 0 initializes to index 0 (black); layers 1-3 initialize to index 15 (transparent passthrough)
- Compositor walks layers 0→3, takes the topmost non-index-15 value per pixel
- Compositing writes to a **separate output buffer** — individual layer buffers remain untouched after compositing
- Index 15 is **always** passthrough/transparent — it is never a drawable color and drawing index 15 is equivalent to erasing
- All layers are auto-cleared at the start of each frame (layer 0 → index 0, layers 1-3 → index 15)

**DrawLayer Enum Overhaul (C++ Engine)**
- The existing `DrawLayer` enum (Default, Background, Entities, Foreground, Overlay, UI) in `drawable.hpp` is **replaced entirely** by a `uint8_t` buffer index (0-3) on `C_Drawable`
- `sort_order` is **dropped entirely** — draw ordering within a buffer is not needed; use different layers for z-ordering
- `shouldDrawBefore()` comparator is either removed or simplified to compare buffer indices only
- `BlendMode` enum stays on `C_Drawable` but is only active for grayscale (Canvas8) builds; for Pixel4/color the compositor ignores it

**Lua API Surface**
- Required functions: `setLayer(n)`, `clearLayer(n, color)`, `getLayerCount()`, `getLayer()`
- Additional: `setLayerVisible(n, bool)`, `isLayerVisible(n)` — compositor skips hidden layers
- **One-indexed** in Lua (1-4), internally mapped to C++ buffer indices (0-3) — idiomatic Lua convention
- Named global constants exposed to Lua: `LAYER_BG = 1`, `LAYER_MID = 2`, `LAYER_FG = 3`, `LAYER_UI = 4`

**Default Behavior & Persistence**
- Default active layer at startup/init = layer 1 (Lua index, i.e., C++ buffer 0, background)
- `setLayer()` persists across frames — active layer does NOT reset between draw() calls
- All layers are auto-cleared at start of each frame before draw() is called

**Compile-Time Configuration (C++ Engine)**
- Layer count is a simple `constexpr` value (e.g., `constexpr uint8_t ENJIN_LAYER_COUNT = 4`)
- Changing this value and rebuilding adjusts the system — no runtime configuration needed

**Intended Usage Convention**
- Layer 1 (BG): static background art, tilemap
- Layer 2 (Midground): main gameplay entities, sprites
- Layer 3 (FG): foreground obstacles, near objects, parallax
- Layer 4 (UI): score, health, HUD, menus

### Claude's Discretion
- Out-of-range layer index handling in Lua (clamp vs error — decide based on codebase conventions)
- How layer canvas array is passed to C_LuaScript::draw() (signature change vs stored on LuaBindings)
- Where layer buffers live (new LayerCompositor class vs runner-owned array — decide based on runner structure)
- Compositor performance approach (inner loop optimization for the passthrough check)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LAYER-01 | Engine renders up to 4 independent Canvas4 layers composited in draw order | LayerCompositor holds `Canvas4<W,H> layers[ENJIN_LAYER_COUNT]`; compositor inner loop walks 0→3 |
| LAYER-02 | Each drawable is assigned to exactly one layer and renders only to that layer's buffer | `C_Drawable::layer` changes from `DrawLayer` enum to `uint8_t` buffer index; draw system passes correct per-layer canvas |
| LAYER-03 | Layers composited at blit time using index 15 as passthrough transparency | Compositor uses `getPixel()` or raw buffer walk; skips byte pairs where both nibbles == 0xF; writes to separate output Canvas4 |
| LAYER-04 | Layer count is compile-time configurable (default 4) | `constexpr uint8_t ENJIN_LAYER_COUNT = 4` in LayerCompositor header; array size depends on it; `static_assert` guards sensible range |
| LAYER-05 | SDL3 runner composites all layers before blitting to GPU texture | `expand_canvas_to_rgb()` replaced by compositor call + palette expansion from `LayerCompositor::output` |
| LAYER-06 | Lua API exposes `setLayer(n)` and `clearLayer(n, color)` | New `lua_CFunction` bindings in `bindings.cpp`; `LuaBindings` stores active layer index; `currentCanvas` pointer swapped to matching layer |
</phase_requirements>

## Summary

Phase 25 adds a fixed array of 4 `Canvas4<128,128>` layer buffers and a compositor that merges them before SDL3's `expand_canvas_to_rgb()` step. The architecture is entirely codebase-internal: no new external libraries are needed. The compositor is a pure C++ function that walks layers 0→3 per pixel, taking the first non-index-15 value and writing it to a separate output buffer.

The Lua side requires 6 new `lua_CFunction` registrations in `LuaBindings::registerAll()` plus 4 integer global constants. The key mechanism is `LuaBindings::currentCanvas` pointer-swap: `setLayer(n)` stores an active layer index and each drawing call routes through `currentCanvas` which already points at the correct layer's `LuaCanvas`. No changes to `ICanvas<Pixel4>` interface are required; `Canvas4::getBuffer()` (already public) gives the raw `PackedPixel4*` array needed for the fast compositor inner loop.

The largest structural changes are in `sdl_main.cpp` (replace single `g_canvas` with a compositor object owning the layer array), `drawable.hpp` (replace `DrawLayer` enum with `uint8_t buffer_index`), and `bindings.hpp`/`bindings.cpp` (add layer state + 6 functions). The `shouldDrawBefore()` comparator simplifies to comparing two `uint8_t` values. Frame auto-clear must happen before `g_lua.callFunction("draw")` in the game loop.

**Primary recommendation:** Implement a `LayerCompositor` struct (header-only or minimal .cpp) owned by `sdl_main.cpp` as a static global. It holds the layer array, the output canvas, and the compositor function. This keeps sdl_main self-contained and avoids proliferating the layer pointer array through multiple subsystems.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `Canvas4<W,H>` | project-internal | Per-layer pixel buffer | Already the project's pixel storage; `BUFFER_SIZE=(W*H)/2`, `getBuffer()` gives raw `PackedPixel4*` |
| `PackedPixel4` | project-internal | Nibble-packed pixel pairs | Enables single-byte compositor reads covering two pixels at once |
| Lua C API (`lua_CFunction`) | 5.1 (system) | Layer Lua bindings | Project rule: all new bindings use `lua_CFunction` exclusively, never `LuaCallback` |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `g_palette.isTransparent(idx)` / `g_palette.resolve(idx)` | project-internal | Palette expansion in revised blit path | Already used in `expand_canvas_to_rgb()`; compositor replaces this function |
| `constexpr` + `static_assert` | C++17 | Compile-time layer count guard | `ENJIN_LAYER_COUNT` must be 1–8; `static_assert(ENJIN_LAYER_COUNT >= 1 && ENJIN_LAYER_COUNT <= 8)` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Raw `PackedPixel4` buffer walk in compositor | `getPixel()` per pixel | `getPixel()` has bounds check overhead; raw buffer walk is 2x faster for 128x128x4 = 65536 nibble comparisons per frame |
| `LayerCompositor` struct in sdl_main | Passing layer pointer array to `LuaBindings` | Pointer array in bindings couples Lua layer to runner resolution; compositor struct keeps ownership clear |
| `uint8_t` active layer in `LuaBindings` + pointer swap | Separate `LuaCanvas layers[N]` stored on bindings | Bindings already stores one `LuaCanvas*`; swapping pointer is zero-overhead vs storing full array |

## Architecture Patterns

### Recommended Project Structure

Changes to existing files (no new directories required):

```
include/enjin2/
├── graphics/
│   └── layer_compositor.hpp   # NEW: LayerCompositor<W,H,N> template struct
├── components/
│   └── drawable.hpp           # MODIFY: DrawLayer enum -> uint8_t buffer_index
└── scripting/
    └── bindings.hpp           # MODIFY: add layer state + 6 new lua_CFunction decls

src/
├── platform/sdl/
│   └── sdl_main.cpp           # MODIFY: static LayerCompositor; replace g_canvas; auto-clear; compositor call
└── scripting/
    └── bindings.cpp           # MODIFY: register 6 layer functions + 4 Lua globals
```

### Pattern 1: LayerCompositor Template Struct

**What:** A fixed-size array of `Canvas4<W,H>` buffers plus one output `Canvas4<W,H>` for the composited result. Compositor function walks layers 0→3 per pixel.

**When to use:** Instantiate once as a `static` global in `sdl_main.cpp`.

```cpp
// include/enjin2/graphics/layer_compositor.hpp
#pragma once
#include "canvas.hpp"

namespace enjin2 {

constexpr uint8_t ENJIN_LAYER_COUNT = 4;
static_assert(ENJIN_LAYER_COUNT >= 1 && ENJIN_LAYER_COUNT <= 8,
              "ENJIN_LAYER_COUNT must be 1-8");

template <uint16_t W, uint16_t H>
struct LayerCompositor {
    Canvas4<W, H> layers[ENJIN_LAYER_COUNT];
    Canvas4<W, H> output;

    // clearAll: layer 0 -> index 0 (black), layers 1..N-1 -> index 15 (transparent)
    void clearAll() {
        layers[0].clear(Pixel4(0));
        for (uint8_t i = 1; i < ENJIN_LAYER_COUNT; ++i) {
            layers[i].clear(Pixel4(15));
        }
    }

    // composite: painter's order 0->N-1, first non-15 wins per pixel
    void composite() {
        const size_t BUFSIZE = Canvas4<W,H>::BUFFER_SIZE; // (W*H)/2
        auto* out = output.getBuffer();
        for (uint8_t l = 0; l < ENJIN_LAYER_COUNT; ++l) {
            const auto* src = layers[l].getBuffer();
            for (size_t i = 0; i < BUFSIZE; ++i) {
                uint8_t byte = src[i].getByte();
                uint8_t lo = byte & 0x0F;
                uint8_t hi = (byte >> 4) & 0x0F;
                uint8_t out_byte = out[i].getByte();
                // low nibble: pixel at even x
                if (lo != 15) {
                    out_byte = (out_byte & 0xF0) | lo;
                }
                // high nibble: pixel at odd x
                if (hi != 15) {
                    out_byte = (out_byte & 0x0F) | (hi << 4);
                }
                out[i] = PackedPixel4(out_byte);
            }
        }
    }
};

} // namespace enjin2
```

**Note on compositor semantics:** The output buffer must be pre-cleared to a known value before composite() runs — or the compositor must write every pixel. The "first non-15 wins" approach is cleanest when the output is pre-cleared to index 15, then layer 0 provides the background (index 0 for black). Actually since layer 0 is already guaranteed black (cleared to 0), the output after walking all layers will have correct results. The output buffer should be initialized to index 15 at the start of composite() so pixels not covered by any layer become transparent (black in SDL3 — palette index 0 resolve path in expand_canvas_to_rgb).

**Simpler implementation — last non-15 wins (painter's order):**

Because the compositor walks 0→3 and later layers override earlier ones, "last non-15 wins" is equivalent to painter's order. The inner loop becomes:

```cpp
void composite() {
    const size_t BUFSIZE = (W * H) / 2;
    // Init output to layer 0 (background — always valid, cleared to 0)
    memcpy(output.getBuffer(), layers[0].getBuffer(), BUFSIZE);
    // Walk layers 1..N-1, overwrite non-transparent pixels
    for (uint8_t l = 1; l < ENJIN_LAYER_COUNT; ++l) {
        const auto* src = layers[l].getBuffer();
        auto* out = output.getBuffer();
        for (size_t i = 0; i < BUFSIZE; ++i) {
            uint8_t byte = src[i].getByte();
            uint8_t lo = byte & 0x0F;
            uint8_t hi = (byte >> 4) & 0x0F;
            uint8_t out_byte = out[i].getByte();
            if (lo != 15) out_byte = (out_byte & 0xF0) | lo;
            if (hi != 15) out_byte = (out_byte & 0x0F) | (hi << 4);
            out[i] = PackedPixel4(out_byte);
        }
    }
}
```

This avoids the output pre-clear cost by copying layer 0 directly. Layer 0 is never transparent (cleared to black = index 0), so its copy is always valid.

### Pattern 2: LuaBindings Layer Pointer Swap

**What:** `LuaBindings` stores an array of `LuaCanvas` wrappers (one per layer) and an active index. `setLayer(n)` updates the index and swaps `currentCanvas` to the matching wrapper.

**When to use:** Requires a new `setLayers(Canvas4<W,H>* arr, int count)` call from `sdl_main.cpp` after the compositor is constructed.

```cpp
// In LuaBindings (bindings.hpp additions):
private:
    // Layer support (LAYER-06)
    static constexpr int MAX_LUA_LAYERS = 8;  // matches ENJIN_LAYER_COUNT ceiling
    LuaCanvas* layerCanvases[MAX_LUA_LAYERS]; // pointers into compositor.layers[]
    uint8_t    activeLayer{0};                // C++ index 0-3
    uint8_t    layerCount{0};                 // set by host at init

public:
    // Called once from sdl_main after compositor constructed:
    template<uint16_t W, uint16_t H>
    void setLayers(Canvas4<W,H>* layerArr, uint8_t count,
                   LuaCanvas* lcArr) {  // host allocates LuaCanvas array
        layerCount = count;
        for (uint8_t i = 0; i < count && i < MAX_LUA_LAYERS; ++i) {
            layerCanvases[i] = &lcArr[i];
        }
        activeLayer = 0;
        currentCanvas = layerCanvases[0];
    }
```

**Simpler alternative:** Since `sdl_main.cpp` is the only host, just store `LuaCanvas g_lua_layers[ENJIN_LAYER_COUNT]` as static globals in `sdl_main.cpp` and pass the array pointer to bindings. The LuaCanvas constructor takes `Canvas4<W,H>*` — this works cleanly.

### Pattern 3: Lua Binding Registration (lua_CFunction)

**What:** Six new `lua_CFunction` static methods in `LuaBindings`, registered in `registerAll()`. Named globals set via `lua_pushinteger` + `lua_setglobal`.

```cpp
// In bindings.cpp — lua_setLayer
int LuaBindings::lua_setLayer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    // Lua is 1-indexed; clamp to valid range
    int lua_idx = static_cast<int>(luaL_checkinteger(L, 1));
    int cpp_idx = lua_idx - 1;  // convert to 0-based
    if (cpp_idx < 0) cpp_idx = 0;
    if (cpp_idx >= b->layerCount) cpp_idx = b->layerCount - 1;
    b->activeLayer = static_cast<uint8_t>(cpp_idx);
    b->currentCanvas = b->layerCanvases[cpp_idx];
    return 0;
}

// In registerAll() — global constants
lua_pushinteger(L, 1); lua_setglobal(L, "LAYER_BG");
lua_pushinteger(L, 2); lua_setglobal(L, "LAYER_MID");
lua_pushinteger(L, 3); lua_setglobal(L, "LAYER_FG");
lua_pushinteger(L, 4); lua_setglobal(L, "LAYER_UI");
```

### Pattern 4: SDL3 Runner Integration

**What:** Replace `static enjin2::Canvas4<CANVAS_W, CANVAS_H> g_canvas` with a `static LayerCompositor<CANVAS_W, CANVAS_H> g_compositor`. Replace `static enjin2::LuaCanvas g_lua_canvas(&g_canvas)` with a static array of `LuaCanvas`. The `expand_canvas_to_rgb()` function reads from `g_compositor.output` instead of `g_canvas`.

```cpp
// sdl_main.cpp — revised static globals
static enjin2::LayerCompositor<CANVAS_W, CANVAS_H> g_compositor;
#ifdef ENJIN2_BUILD_LUA
static enjin2::LuaCanvas g_lua_layers[enjin2::ENJIN_LAYER_COUNT] = {
    enjin2::LuaCanvas(&g_compositor.layers[0]),
    enjin2::LuaCanvas(&g_compositor.layers[1]),
    enjin2::LuaCanvas(&g_compositor.layers[2]),
    enjin2::LuaCanvas(&g_compositor.layers[3]),
};
#endif
```

Static array initialization for `g_lua_layers` with pointer-to-member syntax requires all `Canvas4` objects to be alive at program start — confirmed safe as `g_compositor` is a static global.

**Game loop revised order:**
```
1. input_advance_frame
2. input_platform_poll
3. g_compositor.clearAll()           // auto-clear all layers
4. g_lua.callFunction("update", dt)
5. g_lua.callFunction("draw")        // draws to active layer
6. g_compositor.composite()          // merge layers -> output
7. expand_canvas_to_rgb()            // reads g_compositor.output
8. SDL_UpdateTexture + SDL_RenderTexture + SDL_RenderPresent
```

### Pattern 5: C_Drawable Overhaul

**What:** Replace `DrawLayer layer` with `uint8_t buffer_index`. Remove `sort_order`. Simplify `shouldDrawBefore()`.

```cpp
// drawable.hpp revised
class C_Drawable : public Component {
protected:
    uint8_t buffer_index{0};    // replaces DrawLayer layer + sort_order
    BlendMode blend_mode;       // retained; ignored for Pixel4 compositor
    // ... rest unchanged

    bool shouldDrawBefore(const C_Drawable& other) const {
        return buffer_index < other.buffer_index;
    }

    void SetBufferIndex(uint8_t idx) { buffer_index = idx; }
    uint8_t GetBufferIndex() const { return buffer_index; }
};
```

`DrawLayer` enum is deleted from `drawable.hpp`. `sort_order` member, `SetSortOrder()`, and `GetSortOrder()` are removed. The `DrawLayer` setters/getters (`SetDrawLayer`, `GetDrawLayer`) are removed.

### Anti-Patterns to Avoid

- **Allocating layer buffers on the heap:** Zero-alloc constraint. All `Canvas4` buffers must be `static` globals or struct members with static storage duration.
- **Clearing layers inside draw():** Auto-clear must happen at the top of the frame (before draw() is called), not inside Lua. The host (sdl_main) owns the clear timing.
- **Passing the whole compositor to LuaBindings:** Only pass `LuaCanvas` pointers, not the compositor struct. Bindings must not depend on the compositor type.
- **Reading from individual layer buffers in expand_canvas_to_rgb:** Always read from `g_compositor.output` after `composite()` runs.
- **Resetting active layer to 1 at frame start:** CONTEXT.md specifies that `setLayer()` persists across frames. Only layer *content* (pixels) is reset, not the active layer selection.
- **Using std::vector for layer canvas storage in LuaBindings:** Fixed-size C arrays only; zero allocation.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Per-pixel compositor | Custom alpha blender | Simple nibble comparison `!= 15` | No alpha channel exists; 4-bit palette is chroma-key only |
| Layer clear timing | Frame state machine | Single `clearAll()` call in host loop | The host already owns frame timing; no need for component-based clear |
| Lua index validation | Range error system | Clamp to `[0, layerCount-1]` | Existing codebase uses silent clamping (see `lua_setFrame` which clamps frame to `[0, total-1]`) |

**Key insight:** The compositor is intentionally minimal — a nibble comparison on packed bytes. Any attempt to add blending, alpha, or per-layer opacity would contradict REQUIREMENTS.md's "Out of Scope" section.

## Common Pitfalls

### Pitfall 1: Static Initializer Order for g_lua_layers

**What goes wrong:** `g_lua_layers` is initialized with addresses of `g_compositor.layers[]`. If the static initializer order is wrong, the Canvas4 objects may not be constructed before LuaCanvas wraps them.

**Why it happens:** C++ guarantees objects in the same translation unit are initialized in declaration order, but only within a TU. Both `g_compositor` and `g_lua_layers` are in `sdl_main.cpp`, so declaration order controls it — declare `g_compositor` before `g_lua_layers`.

**How to avoid:** Declare `g_compositor` before `g_lua_layers` in sdl_main.cpp. The compiler will initialize them in that order within the same TU.

**Warning signs:** Crash or garbage on first `drawSprite` / `setPixel` call when no Lua error is reported.

### Pitfall 2: BUFFER_SIZE Visibility in LayerCompositor

**What goes wrong:** `Canvas4::BUFFER_SIZE` is `static constexpr size_t` defined inside the class template. The compositor's `composite()` function needs to know it. Direct reference `Canvas4<W,H>::BUFFER_SIZE` works inside the template function but requires `<W,H>` to be substituted.

**Why it happens:** Template specialization for `BUFFER_SIZE` is not available at the time `layer_compositor.hpp` is included if `canvas.hpp` is not included first.

**How to avoid:** Include `canvas.hpp` before `layer_compositor.hpp`. Inside `LayerCompositor<W,H>::composite()`, use `layers[0].getBufferSize()` (public method that returns `BUFFER_SIZE`) — this is cleaner than direct static access.

### Pitfall 3: LuaCanvas Array Initialization with Canvas4 Pointers

**What goes wrong:** `LuaCanvas` stores `void* canvasPtr`. The constructor that takes `Canvas4<W,H>*` stores the pointer. If the Canvas4 objects are in a struct (LayerCompositor), taking their addresses in a static initializer requires those objects to exist at static init time.

**Why it happens:** Static initialization order within a TU is well-defined, but if `g_lua_layers` uses brace-initialization with pointer-to-member, it requires valid addresses. The `Canvas4<128,128>` inside `LayerCompositor` is a value member — its address is stable as long as the struct is alive.

**How to avoid:** Either initialize `g_lua_layers` in `main()` after all statics are constructed, or rely on declaration order in sdl_main.cpp (safe because all are in the same TU).

**Recommended:** Initialize `g_lua_layers` inside `main()`, not as a global initializer:
```cpp
// In main(), after g_compositor is confirmed constructed:
enjin2::LuaCanvas g_lua_layers[enjin2::ENJIN_LAYER_COUNT] = { ... };
// Pass to bindings here
```
But since `LuaCanvas` has no default constructor, a static array needs an explicit initializer or a `setLayers()` call post-construction. The safest approach is to declare `g_lua_layers` as a `static` inside `main()` with explicit initialization.

### Pitfall 4: LuaCanvas Has No Default Constructor

**What goes wrong:** `static enjin2::LuaCanvas g_lua_layers[4]` will not compile because `LuaCanvas` has no default constructor (both constructors require a canvas pointer).

**How to avoid:** Use `static LuaCanvas* g_lua_layer_ptrs[ENJIN_LAYER_COUNT]` (pointer array) and construct `LuaCanvas` objects separately, OR use `alignas`/`placement new`, OR restructure so `LuaCanvas` objects are constructed at init time with addresses from the already-constructed compositor.

**Cleanest solution:** Add `LuaCanvas layers[ENJIN_LAYER_COUNT]` to `LayerCompositor` struct itself, constructed from `this->layers[i]`. No separate array needed. Or add an `initLuaCanvases()` method to LayerCompositor that fills an externally-provided array.

**Alternative:** Make `LuaCanvas` default-constructable with `canvasPtr = nullptr` check in all methods. This matches the existing null guard pattern already in every `lua_*` function (e.g., `if (!bindings || !bindings->currentCanvas) return 0;`).

### Pitfall 5: expand_canvas_to_rgb Still Reading g_canvas

**What goes wrong:** After replacing `g_canvas` with `g_compositor`, the old `expand_canvas_to_rgb()` still references `g_canvas` by name. Compiler will catch it, but if the rename is missed during refactor, output will be blank.

**How to avoid:** Delete `g_canvas` entirely. Update `expand_canvas_to_rgb()` to read from `g_compositor.output`. The function signature stays the same; only the source reference changes.

### Pitfall 6: Compositor Must Run BEFORE expand_canvas_to_rgb

**What goes wrong:** If `composite()` is called after `expand_canvas_to_rgb()`, the output buffer will contain stale data from the previous frame.

**How to avoid:** Frame loop order: clearAll() → update() → draw() → composite() → expand_canvas_to_rgb() → SDL blit. Document this order with a comment in sdl_main.cpp.

### Pitfall 7: drawSprite Routes Through currentCanvas (Already Correct)

**What goes wrong:** `lua_drawSprite` calls `b->currentCanvas->setPixel(...)`. After `setLayer(n)`, `currentCanvas` points to layer n's `LuaCanvas`. The sprite blit loop is unchanged — it already routes through `currentCanvas`. No regression expected.

**Verification:** The existing `lua_drawSprite` code does not reference `g_canvas` directly — it goes through `bindings->currentCanvas`. So the layer swap mechanism works with sprites automatically.

## Code Examples

### Canvas4 Buffer Walk (verified from canvas.hpp, line 332-338)

```cpp
// Canvas4<W,H> exposes:
const PackedPixel4* getBuffer() const { return buffer; }
PackedPixel4* getBuffer() { return buffer; }
size_t getBufferSize() const { return BUFFER_SIZE; }
// BUFFER_SIZE = (W*H)/2 — nibble-packed, 2 pixels per byte
```

### PackedPixel4 Nibble Access (verified from types.hpp, lines 191-215)

```cpp
// PackedPixel4 stores two 4-bit pixels:
// Low nibble = pixel at even x column (x % 2 == 0)
// High nibble = pixel at odd x column (x % 2 == 1)
uint8_t getByte() const { return data; }
Pixel4 getLow()  const { return Pixel4(data & 0x0F); }
Pixel4 getHigh() const { return Pixel4((data >> 4) & 0x0F); }
```

### Compositor Inner Loop — Byte-Level (HIGH confidence — derived from existing Canvas4 layout)

```cpp
// For each PackedPixel4 byte in the buffer:
uint8_t src_byte = src[i].getByte();
uint8_t out_byte = out[i].getByte();
uint8_t lo = src_byte & 0x0F;
uint8_t hi = (src_byte >> 4) & 0x0F;
if (lo != 0x0F) out_byte = (out_byte & 0xF0) | lo;
if (hi != 0x0F) out_byte = (out_byte & 0x0F) | (hi << 4);
out[i] = PackedPixel4(out_byte);
```

Note: `0x0F` == 15 (index 15 = transparent). This is the correct nibble value to compare.

### palette.isTransparent() (verified from palette.hpp, lines 49, 121-126)

```cpp
// Palette already has:
constexpr uint8_t PALETTE_TRANSPARENT = 15;
bool isTransparent(uint8_t index) const;  // returns index == 15
// expand_canvas_to_rgb() currently uses this:
if (!enjin2::g_palette.isTransparent(px.value)) { ... }
// After compositor: reads from g_compositor.output, same logic unchanged.
```

### lua_CFunction Registration Pattern (verified from bindings.cpp, lines 128-185)

```cpp
// In registerAll() — consistent with all existing SPR-06 registrations:
engine->registerFunction("setLayer",        lua_setLayer);
engine->registerFunction("clearLayer",      lua_clearLayer);
engine->registerFunction("getLayer",        lua_getLayer);
engine->registerFunction("getLayerCount",   lua_getLayerCount);
engine->registerFunction("setLayerVisible", lua_setLayerVisible);
engine->registerFunction("isLayerVisible",  lua_isLayerVisible);
// Global constants (Lua 1-indexed):
lua_State* L = engine->getState();
lua_pushinteger(L, 1); lua_setglobal(L, "LAYER_BG");
lua_pushinteger(L, 2); lua_setglobal(L, "LAYER_MID");
lua_pushinteger(L, 3); lua_setglobal(L, "LAYER_FG");
lua_pushinteger(L, 4); lua_setglobal(L, "LAYER_UI");
```

### LuaCanvas Null-Check Pattern (verified from bindings.cpp — consistent in all lua_* functions)

```cpp
// Every lua_* function follows this guard:
LuaBindings* b = getBindings(L);
if (!b || !b->currentCanvas) return 0;  // or push 0 if returning value
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Single `g_canvas` in sdl_main | `LayerCompositor` owning 4 layer buffers + output | Phase 25 | SDL3 blit now reads composited output, not a single canvas |
| `DrawLayer` enum (6 values) + `sort_order` int | `uint8_t buffer_index` (0-3) | Phase 25 | Simpler comparison; ECS draw system targets specific layer canvas |
| `expand_canvas_to_rgb()` reads `g_canvas` | Reads `g_compositor.output` | Phase 25 | State.md confirmed: "Compositor replaces expand_canvas_to_rgb() wholesale — partial integration silently drops layers 1-3" |
| Lua scripts draw to single canvas | Lua scripts call `setLayer(n)` then draw to layer n | Phase 25 | Enables multi-layer pixel art composition from Lua |

**Deprecated/outdated:**
- `DrawLayer` enum: removed in Phase 25; replaced by `uint8_t buffer_index`
- `sort_order` field on `C_Drawable`: removed; z-ordering is now "use a different layer"
- `shouldDrawBefore()` enum comparison: replaced by `buffer_index < other.buffer_index`
- Single-canvas `LuaCanvas g_lua_canvas`: replaced by per-layer `LuaCanvas` wrappers

## Open Questions

1. **LuaCanvas default-constructibility**
   - What we know: `LuaCanvas` has two constructors, both requiring a canvas pointer. A static array `LuaCanvas arr[N]` is not compilable without a default constructor.
   - What's unclear: Should we (a) add a default constructor with `nullptr` init (safe, matches existing null-guard pattern), (b) add `LuaCanvas` array as a member of `LayerCompositor`, or (c) use `std::optional`-style manual construction?
   - Recommendation: Add a private default constructor `LuaCanvas() : canvasPtr(nullptr), is4Bit(true), width(0), height(0) {}` plus a `void rewire(Canvas4<W,H>* c)` method. Then static `g_lua_layers[N]` array works, and `rewire()` is called in `main()` after compositor construction. This minimizes churn.

2. **setLayerVisible visibility state location**
   - What we know: CONTEXT.md specifies `setLayerVisible(n, bool)` and `isLayerVisible(n)` — compositor skips hidden layers. These are in the Lua API surface (locked).
   - What's unclear: Where does the visibility array live — on `LayerCompositor` or on `LuaBindings`?
   - Recommendation: Put visibility on `LayerCompositor` since it is the compositor that needs to skip layers. `LuaBindings` calls through to the compositor's visibility array (or holds a pointer to it). But since sdl_main owns the compositor, and bindings shouldn't depend on compositor type, the simplest approach is a separate `bool g_layer_visible[ENJIN_LAYER_COUNT]` static array in sdl_main that bindings update and compositor reads — passed as a pointer. Or: make `LayerCompositor` aware of a visibility array.

3. **Compositor output buffer initial state**
   - What we know: The "copy layer 0 + overwrite non-15 from layers 1-3" approach avoids needing to pre-clear the output buffer.
   - What's unclear: Whether the output buffer needs clearing between frames. If `composite()` always starts with `memcpy(output, layers[0])`, the previous frame's output is fully overwritten — no stale pixels.
   - Recommendation: Use the `memcpy(layer[0])` seed approach. Document it explicitly in the function comment to prevent future confusion.

## Validation Architecture

The project uses a custom CTest-based test infrastructure (no framework config file). Tests are standalone executables registered with `add_test()`. The `nyquist_validation` key is absent from `.planning/config.json`, so this section is included for reference only.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest (CMake built-in) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `ctest --test-dir build -R compositor_test --output-on-failure` |
| Full suite command | `ctest --test-dir build --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Notes |
|--------|----------|-----------|-------|
| LAYER-01 | 4-layer buffer array, compositor output | unit | `tests/compositor_test.cpp` — Wave 0 gap |
| LAYER-02 | Each drawable targets one layer | unit | Can reuse existing `sprite_test.cpp` pattern |
| LAYER-03 | Index 15 is passthrough in compositor | unit | Pixel-level assertion in compositor_test |
| LAYER-04 | `getLayerCount()` returns 4; constexpr rebuild | smoke | Assert in compositor_test; rebuild test is manual |
| LAYER-05 | SDL3 runner blits composited output | visual | `sprite_sdl_test.cpp` extended or new SDL test |
| LAYER-06 | Lua `setLayer`, `clearLayer`, `getLayerCount` | integration | New Lua e2e script + SDL3 runner manual visual |

### Wave 0 Gaps
- [ ] `tests/compositor_test.cpp` — covers LAYER-01, LAYER-02, LAYER-03, LAYER-04
- [ ] Extended Lua test script (`scripts/layer_demo.lua`) — covers LAYER-06

## Memory Budget

For context (SDL3 desktop build — no concern):
- `Canvas4<128,128>`: `BUFFER_SIZE = (128*128)/2 = 8192 bytes = 8KB` per layer
- 4 layers + 1 output buffer: `5 * 8KB = 40KB` static storage
- Existing RGB staging buffer: `128*128*3 = 48KB`
- Total new static memory: `40KB` (vs previous `8KB` for single canvas)

For ESP32 (noted concern in STATE.md):
- IRAM is typically 128–320KB; PSRAM is optional and board-specific
- 4-layer stack = 32KB additional; may be feasible in IRAM on most ESP32-S3 targets
- `static_assert` on `ENJIN_LAYER_COUNT` allows user to reduce to 2 if memory-constrained
- The ESP32 path does not go through sdl_main.cpp — the compositor struct pattern is portable

## Sources

### Primary (HIGH confidence)
- Project source, `include/enjin2/graphics/canvas.hpp` — Canvas4 template, BUFFER_SIZE, getBuffer(), PackedPixel4 interface
- Project source, `include/enjin2/core/types.hpp` — Pixel4, PackedPixel4 nibble layout
- Project source, `src/platform/sdl/sdl_main.cpp` — full runner frame loop, expand_canvas_to_rgb, g_canvas pattern, LuaCanvas wiring
- Project source, `src/scripting/bindings.cpp` — all existing lua_CFunction patterns, sprite pool, lua_CFunction convention
- Project source, `include/enjin2/scripting/bindings.hpp` — LuaBindings fields, LuaCanvas constructors
- Project source, `include/enjin2/components/drawable.hpp` — DrawLayer enum, sort_order, shouldDrawBefore
- Project source, `.planning/STATE.md` — confirmed decisions: compositor replaces expand_canvas_to_rgb wholesale; lua_CFunction only
- Project source, `.planning/phases/25-multi-layer-canvas-composition/25-CONTEXT.md` — all locked decisions

### Secondary (MEDIUM confidence)
- `.planning/REQUIREMENTS.md` — LAYER-01 through LAYER-06 requirement text
- Project source, `include/enjin2/graphics/palette.hpp` — isTransparent(15), PALETTE_TRANSPARENT constant

### Tertiary (LOW confidence)
- ESP32 PSRAM sizing estimates: based on general ESP32-S3 datasheet knowledge (not verified against a specific board config in this project)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries are project-internal; fully verified in source
- Architecture: HIGH — all patterns derived directly from existing codebase code (sdl_main, bindings, canvas); no external assumptions
- Pitfalls: HIGH — LuaCanvas default constructor and static initializer order verified by reading actual class definition; expand_canvas_to_rgb replacement confirmed by STATE.md decision log
- Memory budget: HIGH for SDL3; MEDIUM for ESP32 (general datasheet knowledge)

**Research date:** 2026-02-26
**Valid until:** Phase is stable (no external dependencies); research valid indefinitely until codebase changes
