# Phase 19: Palette Foundation - Research

**Researched:** 2026-02-24
**Domain:** C++ indexed color palette, Lua scripting bindings, Emscripten/WASM bindings
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Default palette**
- PICO-8 palette minus `#94b0c2`, giving exactly 15 colors for indices 0–14
- Index 0 = `#1a1c2c` (dark navy) — serves as default background / canvas clear color
- Full index map: 0:`#1a1c2c`, 1:`#5d275d`, 2:`#b13e53`, 3:`#ef7d57`, 4:`#ffcd75`, 5:`#a7f070`, 6:`#38b764`, 7:`#257179`, 8:`#29366f`, 9:`#3b5dc9`, 10:`#41a6f6`, 11:`#73eff7`, 12:`#f4f4f4`, 13:`#566c86`, 14:`#333c57`
- Index 15 = transparent (always, regardless of palette size)

**Named presets**
- Ship multiple named palettes (e.g., `default`, `gameboy`, etc.)
- `loadPalette('name')` replaces the entire active palette
- Palettes can be smaller than 16 colors — out-of-range indices wrap around (modulo palette size)
- Palette sizes are preset-defined — no runtime `setPaletteSize()` in this phase
- No `resetPalette()` shortcut — use `loadPalette('default')`

**Transparency behavior**
- Index 15 = source skip ("don't write this pixel") — used for sprite/canvas compositing
- Not a color — the destination pixel is preserved when source is index 15
- Output canvas clears to index 0 at start of frame
- Optional debug mode: render index 15 pixels as bright magenta for development visibility

**Palette API (Lua)**
- Explicit naming: `setPaletteColor(index, ...)`, `getPaletteColor(index)`, `loadPalette(name)`, `getPaletteSize()`
- `setPaletteColor` accepts hex string (`'#ff0000'`) or RGB components (`255, 0, 0`)
- Changes take effect immediately — already-drawn pixels update at next blit since palette is applied at display time
- Out-of-range index in `setPaletteColor` wraps silently (consistent with drawing behavior)

**Color precision**
- Full 8-bit RGB per channel (no alpha component)
- No retro color space constraints — the 15-slot limit provides the retro feel
- Colors passed as-is to platform display drivers (no quantization)

### Claude's Discretion

- WASM `getPaletteRGB()` return format (flat array vs per-index — optimize for JS renderer consumption)
- `getPaletteColor` return format (likely always RGB for consistency)
- Which named presets to ship beyond `default` and `gameboy`
- Debug magenta mode toggle mechanism (flag, API call, or build-time)
- Internal storage layout of palette struct
- Exact preset color values for non-default palettes

### Deferred Ideas (OUT OF SCOPE)

- Multi-canvas compositing (sprite canvas onto background canvas) — future phase
- HSV color input format — could add later if needed for programmatic palette generation
- Runtime palette resizing (`setPaletteSize(n)`) — if needed, add in a future phase
- Per-entry alpha / RGBA support — not needed with index-based transparency model
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PAL-01 | Canvas4 palette maps 16 indices to RGB colors at display time (not draw time) | Palette struct stored separately from canvas buffer; lookup applied during blit/display step only |
| PAL-02 | Index 15 is transparent, indices 0–14 are user colors | Special-case `index == 15` in blit loop — skip write; this is purely a display-time rule |
| PAL-03 | Runtime palette swap via `setPaletteColor(index, r, g, b)` without canvas re-render | Palette is a standalone struct with a single write; canvas pixels remain index values, no redraw |
| PAL-04 | Lua API exposes `setPaletteColor()`, `getPaletteColor()`, `loadPalette()`, `getPaletteSize()` | Pattern follows existing LuaBindings registration; new functions registered in `registerAll()` |
| PAL-05 | WASM bindings expose `getPaletteRGB()` for JavaScript renderer | New emscripten binding added alongside existing canvas bindings in `emscripten_bindings.cpp` |
</phase_requirements>

---

## Summary

The palette feature is a display-time indirection layer: Canvas4 pixels store 4-bit indices (0–15), and those indices are resolved to RGB triples only when the canvas is blitted to the screen. The canvas buffer itself never changes during a palette swap — this is the core insight that makes PAL-03 trivially correct. The implementation is a new `Palette` struct (16 RGB entries, static allocation, no heap) that lives alongside the canvas rather than inside it.

The existing codebase already has all the plumbing needed: `LuaBindings::registerAll()` is the correct insertion point for Lua palette functions, and `EMSCRIPTEN_BINDINGS` in `emscripten_bindings.cpp` is where the WASM `getPaletteRGB()` function goes. No existing classes need to be restructured. The `blit()` method on `Canvas4` already skips a transparent value — the new behavior is to skip index 15 specifically (currently it skips index 0 by default), and to do the RGB lookup during the blit step rather than returning a raw Pixel4 value.

The primary risk areas are: (1) deciding the ownership/access model for the `Palette` struct so Lua, WASM, and the blit path all share the same instance; and (2) the hex-string parser for `setPaletteColor('#rrggbb', ...)` which is a small but error-prone piece. Neither requires third-party libraries.

**Primary recommendation:** Implement `Palette` as a plain C++ struct in `include/enjin2/graphics/palette.hpp` with a global or engine-level singleton pointer. Add a `blitWithPalette()` free function (or augment the existing `blit()`) that performs the index-to-RGB lookup. Wire Lua and WASM bindings to the same `Palette*`.

---

## Standard Stack

### Core

| Component | Version/Location | Purpose | Why Standard |
|-----------|-----------------|---------|--------------|
| `Palette` struct | New: `include/enjin2/graphics/palette.hpp` | Holds 16 RGB triples + palette size | Zero allocation, matches project constraint |
| `LuaBindings` | Existing: `src/scripting/bindings.cpp` | Lua function registration | Established pattern — all Lua functions live here |
| Emscripten `--bind` | Existing: `src/bindings/emscripten_bindings.cpp` | WASM function export | All WASM surface lives here already |
| C++ `uint8_t[3]` or `struct RGB` | New inline in palette.hpp | Per-entry color storage | 3 bytes/entry, 16 entries = 48 bytes total, static |

### Supporting

| Component | Purpose | When to Use |
|-----------|---------|-------------|
| `emscripten::val` (typed_memory_view) | Return flat RGB array to JS | Used in `getPaletteRGB()` — same pattern as `getCanvasData128()` |
| Named preset map | Associate preset name strings to `Palette` data | Ship as `constexpr` or `const` static tables |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| 48-byte flat `uint8_t[48]` | `struct PaletteEntry { uint8_t r, g, b; }[16]` | Struct is clearer; flat array is more trivial for C-API / WASM raw memory views — recommend flat `uint8_t[3][16]` or named struct |
| Global palette singleton | Palette member on engine/scene object | Singleton is simpler for this phase; member ownership is cleaner for multi-scene future but deferred |
| `luaL_checkstring` hex parser | `sscanf` on the hex string | Both viable; `sscanf` is simplest and already available in libc |

---

## Architecture Patterns

### Recommended Project Structure

```
include/enjin2/graphics/
├── canvas.hpp          # existing — Canvas4, Canvas8
├── palette.hpp         # NEW — Palette struct, preset tables, blitWithPalette()
└── ...

src/graphics/
├── canvas.cpp          # existing
├── palette.cpp         # NEW — preset definitions, hex parser
└── ...

src/scripting/
└── bindings.cpp        # MODIFIED — add setPaletteColor, getPaletteColor, loadPalette, getPaletteSize

src/bindings/
└── emscripten_bindings.cpp  # MODIFIED — add getPaletteRGB binding
```

### Pattern 1: Palette Struct (Static, Zero-Alloc)

**What:** A plain struct with 16 RGB entries stored as `uint8_t r, g, b` per entry, plus a `uint8_t size` field (preset-defined). Index 15 is always transparent — never stored as a color.

**When to use:** This is the single palette type used everywhere.

```cpp
// include/enjin2/graphics/palette.hpp
namespace enjin2 {

struct RGB {
    uint8_t r, g, b;
    constexpr RGB() : r(0), g(0), b(0) {}
    constexpr RGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

constexpr uint8_t PALETTE_TRANSPARENT = 15;
constexpr uint8_t PALETTE_MAX_ENTRIES = 15; // indices 0-14

struct Palette {
    RGB colors[PALETTE_MAX_ENTRIES]; // indices 0-14; index 15 = transparent (no storage needed)
    uint8_t size;                    // number of named entries (wrapping uses modulo size)

    Palette();                       // initializes to default (PICO-8 minus one)
    void setColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
    RGB  getColor(uint8_t index) const;
    bool loadPreset(const char* name);
    uint8_t getSize() const { return size; }

    // Returns the RGB for a Pixel4 index, handling transparency and wrapping
    // Returns {0,0,0,transparent=true} for index 15
    bool isTransparent(uint8_t index) const { return index == PALETTE_TRANSPARENT; }
    RGB resolve(uint8_t index) const;  // wraps if index < 15
};

// Global active palette — a single engine-level instance
extern Palette g_palette;

} // namespace enjin2
```

**Key insight:** `size` is the preset's defined count. When a script calls `setPaletteColor(6, r, g, b)` on a 4-color Gameboy palette, `6 % 4 = 2` wraps silently. This is consistent with how Canvas4 drawing wraps out-of-range colors.

### Pattern 2: Display-Time Blit with Palette Lookup

**What:** A free function (or augmented `blit`) that reads Canvas4 pixel indices, checks for transparency, and maps surviving indices to RGB for output.

**When to use:** Called by the SDL runner (Phase 21) and WASM JS renderer at display time, never at draw time.

```cpp
// include/enjin2/graphics/palette.hpp (or primitives.hpp)
template <uint16_t W, uint16_t H>
void blitWithPalette(
    const Canvas4<W, H>& src,
    const Palette& palette,
    uint8_t* rgbOut,       // output: W*H*3 bytes (R,G,B per pixel)
    bool debugTransparent = false
) {
    for (uint16_t y = 0; y < H; ++y) {
        for (uint16_t x = 0; x < W; ++x) {
            uint8_t idx = src.getPixel(x, y).value;
            size_t outOff = (y * W + x) * 3;
            if (palette.isTransparent(idx)) {
                if (debugTransparent) {
                    // Bright magenta for visibility
                    rgbOut[outOff]   = 255;
                    rgbOut[outOff+1] = 0;
                    rgbOut[outOff+2] = 255;
                }
                // else: leave destination pixel unchanged (not applicable for flat blit)
                // For WASM/SDL, transparent pixels should render as index 0 color (background)
                // since there is no compositing target — this is a single-canvas phase
            } else {
                RGB c = palette.resolve(idx);
                rgbOut[outOff]   = c.r;
                rgbOut[outOff+1] = c.g;
                rgbOut[outOff+2] = c.b;
            }
        }
    }
}
```

**Note on transparent in single-canvas context:** PAL-02 says index 15 pixels are "skipped (transparent) at the blit step on all platforms." Since this phase has only one canvas (no compositing target), "skipped" means the JS/SDL layer will see those pixels as whatever was underneath — which in practice is the clear color (index 0's RGB) since the destination is initialized to black/background before blit. The WASM renderer should treat index 15 as "background color" for the flat render pass.

### Pattern 3: Lua Binding Registration

**What:** Four new functions registered alongside the existing `registerAll()` call in `LuaBindings`.

```cpp
// In LuaBindings::registerAll(), append:
engine->registerFunction("setPaletteColor", lua_setPaletteColor);
engine->registerFunction("getPaletteColor", lua_getPaletteColor);
engine->registerFunction("loadPalette",     lua_loadPalette);
engine->registerFunction("getPaletteSize",  lua_getPaletteSize);
```

The static C functions follow the existing `getBindings(L)` pattern to retrieve the bindings instance, then access `g_palette` (or a palette pointer stored in LuaBindings).

**setPaletteColor overload handling in Lua:**

```cpp
// Handles: setPaletteColor(index, '#rrggbb') or setPaletteColor(index, r, g, b)
static int lua_setPaletteColor(lua_State* L) {
    int index = luaL_checkinteger(L, 1);
    if (lua_isstring(L, 2)) {
        // hex string path
        const char* hex = lua_tostring(L, 2);
        uint8_t r, g, b;
        if (parseHexColor(hex, r, g, b)) {
            g_palette.setColor(static_cast<uint8_t>(index), r, g, b);
        }
    } else {
        uint8_t r = static_cast<uint8_t>(luaL_checkinteger(L, 2));
        uint8_t g = static_cast<uint8_t>(luaL_checkinteger(L, 3));
        uint8_t b = static_cast<uint8_t>(luaL_checkinteger(L, 4));
        g_palette.setColor(static_cast<uint8_t>(index), r, g, b);
    }
    return 0;
}
```

### Pattern 4: WASM getPaletteRGB() Binding

**What:** A function in `emscripten_bindings.cpp` that returns palette data to JavaScript.

**Recommended format:** A flat `Uint8Array` of 45 bytes (15 entries × 3 channels). Index into it as `palette[i*3]`, `palette[i*3+1]`, `palette[i*3+2]`. This is the most efficient format for the JS renderer's tight pixel loop — no object allocation overhead, directly indexable.

```cpp
// In EMSCRIPTEN_BINDINGS:
function("getPaletteRGB", +[]() -> val {
    // Returns flat Uint8Array: [r0,g0,b0, r1,g1,b1, ..., r14,g14,b14]
    static uint8_t buf[45];
    for (int i = 0; i < 15; ++i) {
        RGB c = g_palette.getColor(i);
        buf[i*3]   = c.r;
        buf[i*3+1] = c.g;
        buf[i*3+2] = c.b;
    }
    return val(typed_memory_view(45, buf));
});

function("setPaletteColor", +[](int index, int r, int g, int b) {
    g_palette.setColor(static_cast<uint8_t>(index),
                       static_cast<uint8_t>(r),
                       static_cast<uint8_t>(g),
                       static_cast<uint8_t>(b));
});

function("loadPalette", +[](std::string name) -> bool {
    return g_palette.loadPreset(name.c_str());
});
```

The JS renderer composites with palette by calling `getPaletteRGB()` once per frame (or on change), then during the `getCanvasData128` pixel loop, replaces each 4-bit index with the corresponding RGB triple.

### Anti-Patterns to Avoid

- **Storing RGB in the canvas buffer:** Canvas4 stores indices (Pixel4, 0–15). RGB is only resolved at display time. Never change `setPixel` to accept RGB.
- **Calling getPaletteRGB() per pixel in JS:** Call it once per frame, cache the Uint8Array reference. The existing `typed_memory_view` returns a view into static memory — cache the JS-side reference, not a copy.
- **Making Palette a Canvas4 template parameter:** Palette is a runtime-swappable object; Canvas4 is a compile-time-dimensioned buffer. They must remain separate.
- **Implementing `resetPalette()` (deferred):** The API is `loadPalette('default')` to reset. Do not add a shortcut function.
- **Using `std::map` or heap allocation for preset table:** Presets should be `constexpr` or `const` static arrays keyed by name via linear search (small N, no heap).

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Hex color parsing | Custom tokenizer | `sscanf(hex+1, "%02hhx%02hhx%02hhx", &r, &g, &b)` (strip leading `#`) | One line, libc, handles all valid 6-digit hex correctly |
| JS typed array from C++ | Manual memory serialization | `emscripten::val(typed_memory_view(N, ptr))` | Already used in `getCanvasData128` — same pattern, zero extra code |
| Named preset lookup | `std::map<std::string, Palette>` | Linear search over `const PalettePreset presets[]` static array | Small N (2-4 presets), no heap, no includes |

**Key insight:** The three technically tricky pieces (hex parse, WASM typed array, preset lookup) all have one-line solutions already in the codebase's pattern set. There is no novel infrastructure needed.

---

## Common Pitfalls

### Pitfall 1: Palette Ownership — Who Holds the `g_palette` Pointer?

**What goes wrong:** Lua bindings, WASM bindings, and the blit path all need to access the same `Palette` instance. If each creates its own, palette changes from Lua won't be visible at blit time.

**Why it happens:** The existing bindings use a global `static LuaBindings* g_currentBindings` pattern. It's tempting to do the same for Palette but with separate instances in each translation unit.

**How to avoid:** Declare `Palette g_palette;` in `palette.cpp` and `extern Palette g_palette;` in `palette.hpp`. All translation units that include `palette.hpp` will reference the same object. This is the simplest correct approach.

**Warning signs:** `setPaletteColor` in Lua has no visible effect on the rendered output — means blit path is reading a different palette instance.

### Pitfall 2: Transparency Semantics in Flat (Non-Compositing) Blit

**What goes wrong:** Index 15 is defined as "don't write this pixel." But in a flat single-canvas scenario (no compositing target), there is no pixel underneath. The JS renderer needs a clear policy.

**Why it happens:** The CONTEXT says "the destination pixel is preserved when source is index 15." But the destination here is a freshly allocated RGBA ImageData buffer in JS, not a persistent framebuffer.

**How to avoid:** Document that in the single-canvas WASM render path, index 15 pixels render as the background color (palette index 0's RGB). The "transparent = destination preserved" semantics are fully correct in Phase 21+ when compositing becomes available. Log this in code comments.

**Warning signs:** Success criterion 2 says "Pixels drawn at index 15 are skipped (transparent) at the blit step on all platforms" — "skipped" here means not overwritten by the source color, which in the WASM flat case means the pixel appears as whatever the destination ImageData was initialized to (usually black/0).

### Pitfall 3: `typed_memory_view` Returns a Live View — Don't Assume a Copy

**What goes wrong:** `val(typed_memory_view(45, buf))` in the WASM binding returns a live view into the C++ static `buf`. If the JS side holds a reference across multiple frames and the C++ palette changes, the JS view reflects the change automatically.

**Why it happens:** Emscripten's `typed_memory_view` is a zero-copy view, not a snapshot.

**How to avoid:** This is actually the *correct* behavior for this phase — JS should always see the current palette. Make the `buf` in `getPaletteRGB()` a `static` local so it persists. Document the live-view behavior in the TypeScript definition.

### Pitfall 4: Modulo Wrapping at Index 15

**What goes wrong:** The palette wraps out-of-range indices by `index % size`. But index 15 must always be transparent — if `size` is, say, 8, then index `15 % 8 = 7` would make index 15 resolve to a color rather than transparent.

**Why it happens:** The wrapping rule and the transparency rule interact.

**How to avoid:** The transparency check `if (index == PALETTE_TRANSPARENT)` must be applied *before* the modulo wrap. The `resolve()` function should be:
```cpp
RGB Palette::resolve(uint8_t index) const {
    // Transparency check first — index 15 never resolves to a color
    if (index == PALETTE_TRANSPARENT) return RGB{}; // caller handles transparency
    uint8_t wrapped = index % size;
    return colors[wrapped];
}
```
Equivalently, `setColor` and `getColor` should also check: reject or wrap for index >= PALETTE_TRANSPARENT.

### Pitfall 5: `setPaletteColor` with Hex String — Leading `#` and Case

**What goes wrong:** `sscanf("#FF0000", "%02hhx%02hhx%02hhx", &r, &g, &b)` fails because `sscanf` stops at `#`. Must skip the first character.

**How to avoid:**
```cpp
bool parseHexColor(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (!hex) return false;
    const char* start = (hex[0] == '#') ? hex + 1 : hex;
    return sscanf(start, "%02hhx%02hhx%02hhx", &r, &g, &b) == 3;
}
```
`sscanf` with `%02hhx` handles both upper and lowercase hex.

---

## Code Examples

### Palette Struct Initialization with Default PICO-8 Colors

```cpp
// src/graphics/palette.cpp
#include "palette.hpp"

namespace enjin2 {

Palette g_palette; // single engine-level instance

// PICO-8 palette minus #94b0c2 (15 colors for indices 0-14)
static constexpr RGB DEFAULT_COLORS[15] = {
    {0x1a, 0x1c, 0x2c}, // 0:  dark navy  (background/clear)
    {0x5d, 0x27, 0x5d}, // 1:  purple
    {0xb1, 0x3e, 0x53}, // 2:  red
    {0xef, 0x7d, 0x57}, // 3:  orange
    {0xff, 0xcd, 0x75}, // 4:  yellow
    {0xa7, 0xf0, 0x70}, // 5:  light green
    {0x38, 0xb7, 0x64}, // 6:  green
    {0x25, 0x71, 0x79}, // 7:  teal
    {0x29, 0x36, 0x6f}, // 8:  dark blue
    {0x3b, 0x5d, 0xc9}, // 9:  blue
    {0x41, 0xa6, 0xf6}, // 10: light blue
    {0x73, 0xef, 0xf7}, // 11: cyan
    {0xf4, 0xf4, 0xf4}, // 12: white
    {0x56, 0x6c, 0x86}, // 13: slate
    {0x33, 0x3c, 0x57}, // 14: dark slate
};

Palette::Palette() : size(15) {
    for (int i = 0; i < 15; ++i) colors[i] = DEFAULT_COLORS[i];
}

void Palette::setColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= PALETTE_TRANSPARENT) index = index % size; // wrap before transparent check
    // After wrapping, index is guaranteed < 15 if size <= 15
    colors[index] = RGB{r, g, b};
}

RGB Palette::getColor(uint8_t index) const {
    if (index >= PALETTE_TRANSPARENT) return colors[index % size];
    return colors[index];
}

RGB Palette::resolve(uint8_t index) const {
    // isTransparent() must be called before resolve() by blit path
    uint8_t wrapped = index % size;
    return colors[wrapped];
}
```

### Named Preset: Gameboy

```cpp
// src/graphics/palette.cpp (continued)
struct PalettePreset {
    const char* name;
    const RGB*  colors;
    uint8_t     size;
};

static constexpr RGB GAMEBOY_COLORS[4] = {
    {0x0f, 0x38, 0x0f}, // 0: darkest green
    {0x30, 0x62, 0x30}, // 1: dark green
    {0x8b, 0xac, 0x0f}, // 2: light green
    {0x9b, 0xbc, 0x0f}, // 3: lightest green
};

static const PalettePreset PRESETS[] = {
    {"default",  DEFAULT_COLORS,  15},
    {"gameboy",  GAMEBOY_COLORS,   4},
    // Add additional presets here
};
static constexpr size_t PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

bool Palette::loadPreset(const char* name) {
    for (size_t i = 0; i < PRESET_COUNT; ++i) {
        if (strcmp(PRESETS[i].name, name) == 0) {
            size = PRESETS[i].size;
            for (uint8_t j = 0; j < size && j < PALETTE_MAX_ENTRIES; ++j) {
                colors[j] = PRESETS[i].colors[j];
            }
            return true;
        }
    }
    return false; // name not found
}
```

### JS Renderer: Palette-Aware Canvas-to-Screen

```javascript
// Pseudo-code for JS renderer update (WASM side, post Phase 19)
const palette = Module.getPaletteRGB(); // Uint8Array(45), live view
const indexData = Module.getCanvasData128(canvas); // Uint8Array(128*128)
const ctx = screenCanvas.getContext('2d');
const imageData = ctx.createImageData(128, 128);
const pixels = imageData.data; // Uint8ClampedArray RGBA

for (let i = 0; i < 128 * 128; ++i) {
    const idx = indexData[i];
    if (idx === 15) {
        // Transparent: render as background (index 0 color)
        pixels[i*4]   = palette[0];   // r of index 0
        pixels[i*4+1] = palette[1];   // g of index 0
        pixels[i*4+2] = palette[2];   // b of index 0
        pixels[i*4+3] = 255;
    } else {
        const pi = idx * 3;
        pixels[i*4]   = palette[pi];
        pixels[i*4+1] = palette[pi+1];
        pixels[i*4+2] = palette[pi+2];
        pixels[i*4+3] = 255;
    }
}
ctx.putImageData(imageData, 0, 0);
```

### TypeScript Type Addition for getPaletteRGB

```typescript
// src/bindings/enjin2.d.ts additions
export interface Enjin2Module extends EmscriptenModule {
  // ... existing ...
  getPaletteRGB(): Uint8Array;           // 45 bytes: [r0,g0,b0, r1,g1,b1, ..., r14,g14,b14]
  setPaletteColor(index: number, r: number, g: number, b: number): void;
  loadPalette(name: string): boolean;
  getPaletteSize(): number;
}
```

---

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|------------------|--------|
| Canvas pixels store RGB directly | Canvas pixels store indices; RGB resolved at display time | Enables palette swap without redraw — key architectural property |
| `blit()` transparency uses `Pixel4(0)` as default transparent | Index 15 is always transparent regardless of color value | Separates "no color" semantics from index 0 (which is now a real color) |
| Colors::BLACK = Pixel4(0) used as transparent color | Colors::BLACK = Pixel4(0) is index 0 = `#1a1c2c` (dark navy) | CONTEXT and STATE confirm this — Pixel4(0) is now a real palette color, not a sentinel |

**Deprecated/outdated:**
- `blit()` with `Pixel4 transparent = Pixel4(0)` parameter: After Phase 19, the transparent index is always 15. The existing `blit()` signature should either be kept for internal Canvas4-to-Canvas4 operations (where Pixel4(0) still makes sense if the user explicitly passes it) or a new `blitWithPalette()` should be added. The existing `blit()` is used for canvas-to-canvas copy during draw time (not display time), so it does not conflict with the new palette-based display-time transparency.

---

## Open Questions

1. **Where does `g_palette` live in the WASM build?**
   - What we know: In the native desktop build, `g_palette` can be a file-scope object in `palette.cpp`. In WASM, `emscripten_bindings.cpp` and `palette.cpp` are separate translation units linked into the same `enjin2_wasm` executable — they share the global object cleanly.
   - What's unclear: Whether LuaEngine-driven `setPaletteColor` calls (during `executeString`) and subsequent `getPaletteRGB` calls from JS will be in sync in the WASM event loop. They should be, since WASM is single-threaded.
   - Recommendation: Confirm by reading the build output once `palette.cpp` is added to `CMakeLists.txt`. Single-threaded WASM means no sync issue.

2. **Debug magenta mode mechanism**
   - What we know: User wants optional debug mode where index 15 renders as magenta instead of transparent.
   - What's unclear: Toggle as a flag on `Palette`, a global `bool g_debugTransparent`, or a build-time `#define`. Given the CONTEXT says "optional debug mode," a runtime flag is most useful.
   - Recommendation: A `bool debugTransparent` flag on the `Palette` struct itself. Expose via Lua as `setPaletteDebug(true/false)` and optionally via WASM. This keeps the toggle self-contained.

3. **getPaletteColor Lua return format**
   - What we know: CONTEXT says "likely always RGB for consistency."
   - Recommendation: Return three separate Lua numbers: `local r, g, b = getPaletteColor(0)`. This matches idiomatic Lua multi-return and avoids table allocation.

4. **CMakeLists.txt — where does `palette.cpp` go?**
   - What we know: Graphics logic goes in `enjin2_graphics`. The palette is a graphics concept (display-time color lookup). `LuaBindings` changes go in `enjin2_lua`. WASM binding changes go in `enjin2_wasm` executable sources.
   - Recommendation: Add `src/graphics/palette.cpp` to `enjin2_graphics` sources. Palette header included by `emscripten_bindings.cpp` directly.

---

## Sources

### Primary (HIGH confidence)

- Codebase: `/home/unwn/dev/enjin/include/enjin2/graphics/canvas.hpp` — Canvas4, blit(), Pixel4, PackedPixel4 types; verified directly
- Codebase: `/home/unwn/dev/enjin/src/scripting/bindings.cpp` — LuaBindings registration pattern, getBindings() idiom; verified directly
- Codebase: `/home/unwn/dev/enjin/src/bindings/emscripten_bindings.cpp` — WASM binding pattern, typed_memory_view usage; verified directly
- Codebase: `/home/unwn/dev/enjin/CMakeLists.txt` — build targets (enjin2_graphics, enjin2_lua, enjin2_wasm), library structure; verified directly
- Codebase: `/home/unwn/dev/enjin/include/enjin2/core/types.hpp` — Pixel4, PackedPixel4, Colors namespace; verified directly
- Codebase: `/home/unwn/dev/enjin/src/bindings/enjin2.d.ts` — TypeScript interface pattern for WASM surface; verified directly

### Secondary (MEDIUM confidence)

- Emscripten documentation (from training, consistent with code evidence): `typed_memory_view` returns a live zero-copy view into WASM memory, not a snapshot — confirmed by the existing usage pattern in `getCanvasData128`.
- `sscanf` with `%02hhx` for hex color parsing: Standard C library, verified by knowledge of POSIX sscanf spec; no project-specific risk.

### Tertiary (LOW confidence)

- None — all findings are grounded in direct codebase inspection.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — derived from direct codebase inspection; all components exist
- Architecture: HIGH — pattern follows existing `LuaBindings` and `emscripten_bindings` structure exactly
- Pitfalls: HIGH — derived from code analysis (modulo/transparency interaction, `typed_memory_view` semantics, hex parse gotcha all inspected)

**Research date:** 2026-02-24
**Valid until:** 2026-04-24 (stable domain — C++, Emscripten, Lua binding patterns are not fast-moving)
