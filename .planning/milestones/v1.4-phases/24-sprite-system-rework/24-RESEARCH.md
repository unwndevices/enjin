# Phase 24: Sprite System Rework - Research

**Researched:** 2026-02-24
**Domain:** C++ sprite/animation system, Lua C-function bindings, ICanvas<Pixel4> draw target
**Confidence:** HIGH — all findings come from direct codebase inspection

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Transparency / matte handling**
- Index 15 is the transparent palette index — pixels with value 15 are skipped during sprite blit
- This is a compile-time constant baked into the draw logic, not a per-sprite or per-draw-call parameter
- Consistent with Phase 25's layer system, which also uses index 15 as the passthrough transparency index
- Pixels that are drawn are written as raw palette index values — no color remapping or tinting
- No mode concept: the only drawing behavior is blit-with-matte-skip (draw non-15, skip 15)

### Claude's Discretion

- Lua sprite pool size and handle representation
- Frame animation tick model (delta-time vs. game ticks)
- API naming conventions for new Sprite/SpriteSheet types
- Behavior at end of "once" animation mode (freeze on last frame vs. stop advancing)
- Ping-pong direction reversal implementation

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SPR-01 | Sprite class redesigned with clean API (no legacy public members, consistent naming, targets `ICanvas<Pixel4>`) | Old `Sprite` in `include/enjin2/graphics/sprite.hpp` has public `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` fields and `ICanvas<uint8_t>` target — both must be removed; new design detailed below |
| SPR-02 | Sprite sheet loaded as uniform grid defined by cell width, cell height, rows, and cols | Sheet data is raw `const uint8_t*` with 1 byte per pixel (lower nibble = palette index); frame stride = cell_w × cell_h; grid math: total_frames = rows × cols |
| SPR-03 | Frame addressed by linear index or (row, col) grid position | Linear: index as-is; grid: index = row × cols + col — no lookup table needed |
| SPR-04 | Frame animation with FPS playback rate and loop modes (once, loop, ping-pong) | Delta-time accumulator pattern; accumulator += dt; advance when accumulator >= 1.0/fps; three mode enum |
| SPR-05 | C_Sprite component updated to use new Sprite API | `C_Drawable::draw()` currently takes `ICanvas<uint8_t>&`; SPR-05 requires changing it to `ICanvas<Pixel4>&` — this is the biggest structural impact |
| SPR-06 | Lua API exposes sprite sheet draw and frame animation control via static sprite pool | Four lua_CFunction bindings (`newSprite`, `drawSprite`, `updateSprite`, `setFrame`) added to `LuaBindings`; pool is a fixed-size static array in `LuaBindings` |
</phase_requirements>

---

## Summary

This phase replaces the existing `Sprite` class (in `include/enjin2/graphics/sprite.hpp`) and `C_Sprite` component (in `include/enjin2/components/sprite.hpp`) wholesale. The legacy API uses `ICanvas<uint8_t>` as its draw target and exposes six public legacy member fields (`_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode`). Both must be removed entirely.

The new design introduces a `SpriteSheet` struct — a zero-alloc value type holding a pointer to raw pixel data plus grid dimensions — and a separate `C_Sprite` component that owns animation state (FPS, mode, accumulator, current frame, ping-pong direction). The draw target switches to `ICanvas<Pixel4>`, which requires the `C_Drawable` base class `draw()` signature to change from `ICanvas<uint8_t>&` to `ICanvas<Pixel4>&`. This is the largest impact in the codebase: it ripples into all existing drawable components.

The Lua sprite pool adds four new `lua_CFunction` bindings registered in `LuaBindings::registerAll()`. The pool is a fixed-size static array within `LuaBindings`, making it zero-alloc and consistent with the project constraint. The binding pattern follows the established project convention: static `lua_CFunction` methods retrieving `LuaBindings*` from the Lua registry key `"enjin_bindings"`.

**Primary recommendation:** Complete the rework in two decoupled sub-tasks — (1) redesign the C++ `SpriteSheet` and `C_Sprite` types plus fix `C_Drawable::draw()` signature, (2) add the four Lua bindings. Handle the `C_Drawable` signature change carefully: every existing drawable component that overrides `draw(ICanvas<uint8_t>&)` must be updated.

---

## Standard Stack

### Core

| Component | Location | Purpose | Status |
|-----------|----------|---------|--------|
| `ICanvas<Pixel4>` | `include/enjin2/abstract/icanvas.hpp` + `include/enjin2/graphics/canvas.hpp` | Draw target for new sprite blit | Exists; `Canvas4<W,H>` implements it |
| `Pixel4` | `include/enjin2/core/types.hpp` | 4-bit pixel type, value 0-15 | Exists; `Pixel4(15)` == transparent per locked decision |
| `lua_CFunction` | LuaJIT / system Lua | Binding functions — project mandates this over `LuaCallback` | Established pattern in `src/scripting/bindings.cpp` |
| `LuaBindings` | `include/enjin2/scripting/bindings.hpp` | Central registry for all Lua functions; holds `currentCanvas` via `LuaCanvas*` | Extend for sprite pool |

### Key Existing Infrastructure

| Asset | Path | Relevance |
|-------|------|-----------|
| `Canvas4<W,H>::setPixel(int16_t, int16_t, Pixel4)` | `canvas.hpp` line 145 | Used in new blit loop; bounds-checked |
| `Canvas4<W,H>::setPixelBatch(...)` | `canvas.hpp` line 277 | Optional optimization for full-row blits in sprite sheets |
| `LuaBindings::getBindings(L)` | `bindings.cpp` line 224 | Static helper retrieving `LuaBindings*` from registry — all Lua functions use this pattern |
| `g_currentBindings` / `"enjin_bindings"` registry key | `bindings.cpp` line 9, 135 | How `LuaBindings*` is stored and retrieved in Lua CFunctions |

### No New Dependencies

This phase introduces no new libraries. Everything is built from existing project infrastructure.

---

## Architecture Patterns

### Recommended File Structure

```
include/enjin2/graphics/
├── sprite.hpp          # SpriteSheet struct + blit free function (REPLACED)
include/enjin2/components/
├── sprite.hpp          # C_Sprite component (REPLACED)
src/scripting/
├── bindings.cpp        # Four new lua_CFunction implementations added here
include/enjin2/scripting/
├── bindings.hpp        # Pool array + four static method declarations added here
```

No new source files are needed. All changes are in-place replacements and additions to existing files.

### Pattern 1: SpriteSheet — Zero-Alloc Value Type

**What:** A plain struct with no dynamic allocation. Holds a pointer to external pixel data (caller owns lifetime) plus the four grid dimensions.

**When to use:** Passed by value or const reference. No virtual methods. No heap.

```cpp
// include/enjin2/graphics/sprite.hpp  (new content)
#pragma once
#include "../core/types.hpp"
#include "../abstract/icanvas.hpp"
#include <cstdint>

namespace enjin2 {

struct SpriteSheet {
    const uint8_t* data;   ///< Raw pixel data: 1 byte per pixel, lower nibble = palette index
    uint8_t        cellW;  ///< Cell width in pixels
    uint8_t        cellH;  ///< Cell height in pixels
    uint8_t        cols;   ///< Number of columns in grid
    uint8_t        rows;   ///< Number of rows in grid

    SpriteSheet() : data(nullptr), cellW(0), cellH(0), cols(0), rows(0) {}

    SpriteSheet(const uint8_t* d, uint8_t cw, uint8_t ch, uint8_t c, uint8_t r)
        : data(d), cellW(cw), cellH(ch), cols(c), rows(r) {}

    /// Total number of frames in the sheet
    uint8_t frameCount() const { return static_cast<uint8_t>(cols * rows); }

    /// Convert (row, col) to linear index
    uint8_t toIndex(uint8_t row, uint8_t col) const {
        return static_cast<uint8_t>(row * cols + col);
    }

    /// Blit frame at linear index to canvas at (x, y).
    /// Pixels with palette index 15 are skipped (transparent).
    void draw(ICanvas<Pixel4>& canvas, uint8_t frameIndex, int16_t x, int16_t y) const;
};

} // namespace enjin2
```

The `draw()` implementation (in a new `.cpp` or inline):

```cpp
void SpriteSheet::draw(ICanvas<Pixel4>& canvas, uint8_t frameIndex, int16_t x, int16_t y) const {
    if (!data) return;
    const uint8_t* frame = data + static_cast<int>(frameIndex) * cellW * cellH;
    for (uint8_t fy = 0; fy < cellH; ++fy) {
        for (uint8_t fx = 0; fx < cellW; ++fx) {
            uint8_t px = frame[fy * cellW + fx] & 0x0F;  // lower nibble
            if (px != 15) {  // compile-time transparent index
                canvas.setPixel(x + fx, y + fy, Pixel4(px));
            }
        }
    }
}
```

**Why lower nibble only:** The raw pixel data format uses 1 byte per pixel where the palette index is stored in the lower 4 bits. This is consistent with how `Pixel4` works and matches the existing `Canvas8::drawGrayscaleBitmap` approach (confirmed in canvas.hpp line 985+).

### Pattern 2: C_Sprite Component — Animation State Machine

**What:** Component wrapping a `SpriteSheet` reference plus animation state. Advances frames via delta-time accumulator.

**Key design choices (Claude's discretion):**
- **Tick model:** Delta-time accumulator (float seconds). The SDL runner already passes `float dt` to `update(dt)` in Lua; the C++ ECS lateUpdate receives `uint16_t deltaTime` in milliseconds. Use the millisecond form for `C_Sprite::lateUpdate`.
- **Once mode end behavior:** Freeze on the last frame (animation stops at frame count-1, stops advancing). This is the least surprising behavior and prevents index-out-of-bounds.
- **Ping-pong direction:** Track a `bool forward` flag; when forward reaches the last frame, flip to reverse; when reverse reaches frame 0, flip back. Direction reversal happens AT the terminal frames (inclusive).
- **Pool size:** 16 sprite instances. Fits embedded (16 × ~20 bytes = ~320 bytes). Large enough for typical Lua scripts.

```cpp
// Animation mode enum (in sprite.hpp or a dedicated enum header)
enum class AnimMode : uint8_t {
    Once,      ///< Play once, freeze on last frame
    Loop,      ///< Loop back to frame 0 after last frame
    PingPong   ///< Reverse direction at each end
};
```

```cpp
// include/enjin2/components/sprite.hpp  (new C_Sprite)
class C_Sprite : public C_Drawable {
public:
    C_Sprite(Object* owner, uint8_t width, uint8_t height);

    void setSheet(const SpriteSheet& sheet);
    void setFPS(float fps);
    void setMode(AnimMode mode);
    void setFrame(uint8_t index);
    uint8_t getFrame() const { return _frame; }

    void draw(ICanvas<Pixel4>& canvas) override;
    void lateUpdate(uint16_t deltaTimeMs) override;

private:
    SpriteSheet _sheet;
    float       _fps;
    float       _accumMs;   ///< Accumulated milliseconds since last frame advance
    uint8_t     _frame;
    AnimMode    _mode;
    bool        _forward;   ///< For ping-pong direction
    bool        _done;      ///< True when Once mode has completed
};
```

### Pattern 3: C_Drawable Signature Change

**Critical impact:** The existing `C_Drawable::draw()` is declared as:
```cpp
virtual void draw(ICanvas<uint8_t>& canvas) = 0;   // current
```
It must change to:
```cpp
virtual void draw(ICanvas<Pixel4>& canvas) = 0;   // new
```

**All derived components that override this must be updated.** Scan reveals these files contain the old signature:

| File | Override |
|------|---------|
| `include/enjin2/components/sprite.hpp` | `C_Sprite::draw(ICanvas<uint8_t>&)` |
| `include/enjin2/components/canvas.hpp` | `C_Canvas::draw(ICanvas<uint8_t>&)` |
| `src/components/canvas.cpp` | Implementation |

Grep for `draw(ICanvas<uint8_t>` to find all callers before editing.

### Pattern 4: Lua Pool — Fixed-Size Static Array

**What:** A fixed-size array of `SpriteState` structs stored as a member of `LuaBindings`. Lua scripts receive an integer handle (0-based index into the array). No heap allocation.

```cpp
// Add to LuaBindings (bindings.hpp)
static constexpr int LUA_SPRITE_POOL_SIZE = 16;

struct SpriteState {
    SpriteSheet sheet;
    float       fps{8.0f};
    float       accumMs{0.0f};
    uint8_t     frame{0};
    AnimMode    mode{AnimMode::Loop};
    bool        forward{true};
    bool        done{false};
    bool        active{false};
};

SpriteState spritePool[LUA_SPRITE_POOL_SIZE];
```

**Handle allocation:** `newSprite` scans for the first `!active` slot and returns its index. Returns -1 if pool is full (Lua scripts must check). Slots are never freed in this phase (pool is reset on hot reload, which is Phase 26).

### Pattern 5: Four Lua Bindings

All follow the established `lua_CFunction` pattern in `bindings.cpp`:

```cpp
// newSprite(sheet_data_lightuserdata, cell_w, cell_h, cols, rows) -> handle
// Returns: integer handle (0..15) or -1 on pool full

// drawSprite(handle, x, y)
// Draws current frame to currentCanvas

// updateSprite(handle, dt_ms)
// Advances animation by dt_ms milliseconds

// setFrame(handle, frame_index)
// Directly sets frame, clamps to valid range
```

**Canvas access in Lua bindings:** `drawSprite` uses `bindings->currentCanvas`. Since `LuaCanvas` wraps an `ICanvas<Pixel4>*` (when `is4Bit == true`), cast via:
```cpp
auto* canvas = static_cast<ICanvas<Pixel4>*>(bindings->currentCanvas->getRawCanvasPtr());
```
However, `LuaCanvas` currently exposes only `void* canvasPtr` (private). Either:
1. Add a `getPixel4Canvas()` accessor to `LuaCanvas`, OR
2. Have `drawSprite` call through `LuaCanvas::setPixel()` in a loop (simpler, avoids adding API)

Option 2 avoids modifying `LuaCanvas` interface for this phase, but is slower (per-pixel virtual dispatch via `LuaCanvas`). Given the canvas size (128×128) and typical sprite sizes (≤32×32), option 2 is acceptable. Option 1 is preferable for performance. **Recommendation: add a minimal `getCanvasPtr()` accessor returning the typed pointer if `is4Bit`, null otherwise.**

### Anti-Patterns to Avoid

- **Using `matte` as a parameter:** Locked decision — transparency is compile-time constant index 15. No matte parameter on any new API.
- **`BlendMode` on new sprite:** Old `Sprite` had `BlendMode`. New design has none — blit-with-skip is the only behavior.
- **`LuaCallback` / `std::function`:** Phase 22 decision — all bindings use `lua_CFunction` only. STATE.md explicitly documents this.
- **Dynamic allocation for pool or animation state:** Zero-alloc constraint. Pool must be a fixed-size member array.
- **Exposing public `_`-prefixed fields:** SPR-01 requires all legacy public members removed from the new type.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Frame index → pixel data pointer | Custom stride calculation | `frame_ptr = data + frameIndex * cellW * cellH` | Already the correct formula; no library needed |
| Bounds checking in blit | Custom bounds logic | `Canvas4::setPixel` already does bounds checking (line 145-158 in canvas.hpp) | Calling setPixel handles it; no double-check needed |
| Lua handle pool | Dynamic std::vector | Fixed-size `SpriteState[16]` array | Matches zero-alloc constraint; pool size 16 is a Claude's discretion item |

---

## Common Pitfalls

### Pitfall 1: C_Drawable Signature Not Updated in All Derived Classes

**What goes wrong:** If `C_Drawable::draw()` is changed to `ICanvas<Pixel4>&` but derived classes (`C_Canvas`, old `C_Sprite`) still override with `ICanvas<uint8_t>&`, the old overrides become dead code (C++ does NOT override with different parameter types — they become separate overloads). The pure virtual remains unimplemented, causing link errors or silent non-dispatch.

**Why it happens:** The signature change is easy to miss in derived classes not directly touched by this phase.

**How to avoid:** Grep `draw(ICanvas<uint8_t>` across all source and include files before declaring victory. The main files to update: `include/enjin2/components/canvas.hpp`, `src/components/canvas.cpp`.

**Warning signs:** Linker error "pure virtual function called" or "undefined vtable".

### Pitfall 2: Pixel Data Format Assumption

**What goes wrong:** The old `Sprite` stored data as `const uint8_t*` where each byte = one pixel value (full 8-bit, range 0-255). The new `SpriteSheet` also uses `const uint8_t*`, but only the lower 4 bits are meaningful (palette index 0-15).

**Why it happens:** If sprite sheet data is authored with values > 15, the `& 0x0F` mask silently produces wrong colors.

**How to avoid:** Document clearly that pixel values must be in range 0-15. The blit code masks with `& 0x0F` as a defensive measure regardless.

### Pitfall 3: Accumulator Drift in Animation

**What goes wrong:** Using integer milliseconds accumulated as float can cause frame timing drift over long runs.

**Why it happens:** `float` has ~7 significant decimal digits; at 30fps over an hour, accumulated error is negligible (< 0.1ms).

**How to avoid:** Reset the accumulator by subtracting the frame duration rather than setting to zero: `_accumMs -= frame_duration_ms`. This preserves sub-frame carry-over and prevents drift.

### Pitfall 4: Ping-Pong Boundary Conditions

**What goes wrong:** Advancing ping-pong incorrectly can skip the first/last frame or oscillate between two frames near boundaries.

**Why it happens:** Direction reversal logic not precisely defined.

**How to avoid:** Use this rule — reverse direction AFTER reaching terminal frame, not before. When `_forward` and `_frame == frameCount-1`: set `_forward = false`, then on next advance go to `frameCount-2`. When `!_forward` and `_frame == 0`: set `_forward = true`, then on next advance go to 1.

### Pitfall 5: Lua Pool Handle -1 Not Checked

**What goes wrong:** If `newSprite` returns -1 (pool full) and the Lua script passes -1 to `drawSprite`, the array index is -1 → undefined behavior (buffer underread on the `spritePool[-1]` access).

**Why it happens:** C++ has no bounds checking on static arrays.

**How to avoid:** All four binding functions must guard: `if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !pool[handle].active) return 0;`

---

## Code Examples

### Blit Loop for SpriteSheet (verified from canvas.hpp `blit` method pattern)

```cpp
// Source: canvas.hpp line 364-377 (blit pattern) + types.hpp (Pixel4)
void SpriteSheet::draw(ICanvas<Pixel4>& canvas, uint8_t frameIndex, int16_t x, int16_t y) const {
    if (!data || frameIndex >= frameCount()) return;
    const uint8_t* frame = data + static_cast<uint16_t>(frameIndex) * cellW * cellH;
    for (int16_t fy = 0; fy < cellH; ++fy) {
        for (int16_t fx = 0; fx < cellW; ++fx) {
            uint8_t px = frame[fy * cellW + fx] & 0x0F;
            if (px != 15) {
                canvas.setPixel(x + fx, y + fy, Pixel4(px));
            }
        }
    }
}
```

### Delta-Time Animation Advance

```cpp
// Source: SDL runner (sdl_main.cpp) passes float dt in seconds to Lua.
// C++ lateUpdate receives uint16_t deltaTimeMs.
void C_Sprite::lateUpdate(uint16_t deltaTimeMs) {
    if (!_sheet.data || _fps <= 0.0f || _done) return;
    _accumMs += static_cast<float>(deltaTimeMs);
    const float frameMs = 1000.0f / _fps;
    while (_accumMs >= frameMs) {
        _accumMs -= frameMs;  // preserve carry-over, not zeroed
        advanceFrame();
    }
}

void C_Sprite::advanceFrame() {
    const uint8_t total = _sheet.frameCount();
    if (total == 0) return;
    switch (_mode) {
        case AnimMode::Once:
            if (_frame < total - 1) ++_frame;
            else _done = true;
            break;
        case AnimMode::Loop:
            _frame = static_cast<uint8_t>((_frame + 1) % total);
            break;
        case AnimMode::PingPong:
            if (_forward) {
                if (_frame < total - 1) ++_frame;
                else { _forward = false; --_frame; }
            } else {
                if (_frame > 0) --_frame;
                else { _forward = true; ++_frame; }
            }
            break;
    }
}
```

### Lua newSprite Binding

```cpp
// Source: bindings.cpp binding pattern (getBindings + lua_CFunction)
// newSprite(data_ptr_as_integer, cell_w, cell_h, cols, rows) -> handle
int LuaBindings::lua_newSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, -1); return 1; }
    // Find free slot
    int handle = -1;
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        if (!b->spritePool[i].active) { handle = i; break; }
    }
    if (handle < 0) { lua_pushinteger(L, -1); return 1; }
    // Initialize slot
    auto& s = b->spritePool[handle];
    // data: passed as lightuserdata or integer address
    s.sheet.data  = reinterpret_cast<const uint8_t*>(lua_topointer(L, 1));
    s.sheet.cellW = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    s.sheet.cellH = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    s.sheet.cols  = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    s.sheet.rows  = static_cast<uint8_t>(luaL_checkinteger(L, 5));
    s.frame   = 0;
    s.fps     = 8.0f;
    s.accumMs = 0.0f;
    s.mode    = AnimMode::Loop;
    s.forward = true;
    s.done    = false;
    s.active  = true;
    lua_pushinteger(L, handle);
    return 1;
}
```

**Note on data pointer in Lua:** Passing raw pixel data pointers from Lua is an advanced use-case. For the typical Lua game script, sprite data will be a compile-time C++ constant array. The Lua binding should accept either `lightuserdata` (pointer) or a future string-based asset path. For this phase, `lightuserdata` is sufficient per SPR-06.

### Lua drawSprite Binding

```cpp
// drawSprite(handle, x, y)
int LuaBindings::lua_drawSprite(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) return 0;
    int handle = static_cast<int>(luaL_checkinteger(L, 1));
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !b->spritePool[handle].active) return 0;
    int16_t x = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t y = static_cast<int16_t>(luaL_checkinteger(L, 3));
    // Draw via LuaCanvas::setPixel (type-erased path, is4Bit assumed true)
    const auto& sheet = b->spritePool[handle].sheet;
    uint8_t fi = b->spritePool[handle].frame;
    if (!sheet.data || fi >= sheet.frameCount()) return 0;
    const uint8_t* frame = sheet.data + static_cast<uint16_t>(fi) * sheet.cellW * sheet.cellH;
    for (int16_t fy = 0; fy < sheet.cellH; ++fy) {
        for (int16_t fx = 0; fx < sheet.cellW; ++fx) {
            uint8_t px = frame[fy * sheet.cellW + fx] & 0x0F;
            if (px != 15) {
                b->currentCanvas->setPixel(x + fx, y + fy, px);
            }
        }
    }
    return 0;
}
```

---

## State of the Art (Codebase)

| Old Approach | New Approach | Impact |
|--------------|-------------|--------|
| `Sprite` class with public `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` | `SpriteSheet` struct with private grid metadata | Removes legacy surface area; SPR-01 compliance |
| `ICanvas<uint8_t>` draw target | `ICanvas<Pixel4>` draw target | Aligns with Canvas4 system; requires `C_Drawable::draw()` signature change |
| `matte(16)` / `matte = 16` (default in old Sprite) | Compile-time constant 15 | Old code used 16 as transparent (which is invalid for 4-bit, 0-15 range); new system uses 15 correctly |
| Frame animation via manual `setTexture(frameId)` calls | `C_Sprite::lateUpdate` accumulator with AnimMode | Automatic advance removes caller burden |
| No Lua sprite API | `newSprite`, `drawSprite`, `updateSprite`, `setFrame` bindings | Enables Lua-scripted games to use sprite sheets |

**Deprecated / to be deleted:**
- `include/enjin2/graphics/sprite.hpp` — entire old `Sprite` class (replace file content)
- `include/enjin2/components/sprite.hpp` — entire old `C_Sprite` class (replace file content)
- `BlendMode` usage in sprite — `BlendMode` enum stays in `drawable.hpp` for other components but is not used by new `C_Sprite`

---

## Open Questions

1. **`C_Drawable::draw()` signature ripple to `C_Canvas`**
   - What we know: `C_Canvas` in `include/enjin2/components/canvas.hpp` and `src/components/canvas.cpp` overrides `draw(ICanvas<uint8_t>&)`. It draws a child canvas onto the parent.
   - What's unclear: Whether `C_Canvas` should also be migrated to `ICanvas<Pixel4>` in this phase or left with the `uint8_t` variant via a separate overload. REQUIREMENTS.md notes ENG-01 (ECS drawable pipeline supporting Canvas4) is a v2 deferred requirement.
   - Recommendation: In this phase, change `C_Drawable::draw()` to `ICanvas<Pixel4>&` AND update `C_Canvas::draw()` as a stub (assert not reached or no-op) if it can't sensibly draw to Pixel4 yet. The alternative — keeping two overloads — adds complexity. Confirm with planner.

2. **Pixel data format for newSprite from Lua**
   - What we know: Raw C++ `const uint8_t*` arrays can be passed as `lightuserdata`. This is the only in-scope mechanism.
   - What's unclear: How a Lua script obtains a pointer to a sprite sheet data array without C++-side setup.
   - Recommendation: For this phase, the C++ host registers a specific sprite sheet as a lightuserdata global before calling Lua `init()`, or the test Lua script hard-codes sprite data via a C++ registration helper. The SPR-06 success criterion only requires the four bindings work — the data source is an implementation detail.

3. **`C_Sprite::draw()` position source**
   - What we know: Old `C_Sprite::draw()` calls `sprite.setPosition(GetOffsetPosition())` pulling from `C_Position` component. The position plumbing is in `C_Drawable`.
   - What's unclear: Whether new `C_Sprite::draw(ICanvas<Pixel4>& canvas)` should query position the same way.
   - Recommendation: Yes — call `GetOffsetPosition()` same as before, pass to `_sheet.draw(canvas, _frame, pos.x, pos.y)`.

---

## Sources

### Primary (HIGH confidence)

- **Direct codebase inspection** — all findings verified by reading source files:
  - `include/enjin2/graphics/sprite.hpp` — legacy API documented
  - `include/enjin2/components/sprite.hpp` — legacy C_Sprite documented
  - `include/enjin2/abstract/icanvas.hpp` + `include/enjin2/graphics/canvas.hpp` — ICanvas<Pixel4> and Canvas4 confirmed
  - `include/enjin2/core/types.hpp` — Pixel4 struct, value range 0-15 confirmed; Colors::WHITE = Pixel4(15)
  - `src/scripting/bindings.cpp` + `include/enjin2/scripting/bindings.hpp` — lua_CFunction pattern, getBindings pattern, LuaCanvas internals
  - `src/platform/sdl/sdl_main.cpp` — game loop tick model (float dt in seconds, uint16_t deltaTimeMs in components)
  - `CMakeLists.txt` — build targets, no existing sprite .cpp source file
  - `.planning/STATE.md` — Phase 22 decision: all new bindings use lua_CFunction exclusively

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — verified directly from headers and source
- Architecture: HIGH — patterns derived from existing working code in the repo
- Pitfalls: HIGH — derived from actual code structure analysis (signature mismatch, boundary conditions)

**Research date:** 2026-02-24
**Valid until:** 2026-03-26 (stable codebase; changes only if Phase 25 merges and alters ICanvas<Pixel4> interface)
