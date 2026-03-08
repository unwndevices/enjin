# Phase 62: Frame Timing Instrumentation - Research

**Researched:** 2026-03-08
**Domain:** C++ lock-free atomics, SDL3 high-resolution timing, debug overlay on Canvas4
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FRAME-01 | FrameTimingInstrumentation struct with lock-free uint32_t atomics tracking updateTime_us, renderTime_us, luaTime_us, compositeTime_us | `std::atomic<uint32_t>` with `memory_order_relaxed` store/load is the correct primitive; uint32_t covers up to ~4295ms which is more than adequate per-phase |
| FRAME-02 | Per-phase timing instrumented into SDL3 runner game loop | `SDL_GetPerformanceCounter()` + `SDL_GetPerformanceFrequency()` gives sub-microsecond resolution in SDL3; the four measurement sites are already identifiable in `sdl_main.cpp` |
| FRAME-03 | Frame budget usage exposed via debug overlay or polling API | Debug overlay uses the existing debug layer (layer index 4, `m_debugCanvas`); `LuaCanvas::drawText` with the built-in 5x7 font is the right write path; `FrameTimingInstrumentation::get()` is the polling API |
</phase_requirements>

---

## Summary

Phase 62 adds a lightweight per-frame timing subsystem to the SDL3 runner. The four logical phases of the game loop (Lua update + coroutines/tweens, Lua draw, compositor, SDL render/present) need to be wrapped with high-resolution timestamps, with the deltas stored in lock-free atomic fields so host code can read them without synchronization overhead.

The entire feature is gated by a compile-time macro (`ENJIN2_FRAME_TIMING`) so WASM and ESP32 builds include the header cleanly but see no code generation. Runtime control via `--show-timing` argv toggles the canvas overlay while leaving the polling API (`FrameTimingInstrumentation::get()`) always available when the macro is on.

The implementation fits in three tightly scoped artifacts: a new header `include/enjin2/instrumentation/frame_timing.hpp`, minimal surgical edits to `src/platform/sdl/sdl_main.cpp` (four measurement sites + `--show-timing` arg + overlay draw), and a lightweight unit test `tests/frame_timing_test.cpp` that verifies the struct API compiles and returns plausible values without SDL.

**Primary recommendation:** Put the struct, enum-like field names, and all logic in a single header. Zero .cpp file needed. SDL-specific measurement calls stay in `sdl_main.cpp` — the header only defines the storage and the zero-overhead disabled path.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `<atomic>` (C++17 stdlib) | C++17 (project already requires this) | Lock-free uint32_t storage for timing fields | No external dep; ESP32 GCC/Clang and Emscripten both support `std::atomic<uint32_t>` with `relaxed` ordering without libatomic on 32-bit targets |
| `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency` (SDL3 3.4.2) | Already vendored via FetchContent | Sub-microsecond wall-clock timestamps | SDL3 docs: QueryPerformanceCounter on Windows, clock_gettime(MONOTONIC) on Linux/macOS; frequency is constant per SDL init; division yields microseconds |
| `snprintf` (`<cstdio>`) | C stdlib | Format timing numbers into stack buffer for overlay | Already used project-wide; zero allocation; safe for ESP32/WASM (not that they use the overlay) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `LuaCanvas::drawText` | Project-internal | Rasterize overlay text to debug layer | Used in `bindings_debug.cpp` already — same path for the timing HUD |
| `Canvas4::drawText` | Project-internal | Alternative if called outside Lua binding path | Prefer `LuaCanvas::drawText` since the debug layer pointer is already `LuaCanvas*` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `SDL_GetPerformanceCounter` | `SDL_GetTicks()` (used in current frame pacing) | `SDL_GetTicks()` returns milliseconds — insufficient for sub-ms resolution. `SDL_GetPerformanceCounter` returns nanosecond-capable ticks, matches the "sub-millisecond" requirement |
| `std::atomic<uint32_t>` | Raw `uint32_t` with no atomics | FRAME-01 spec explicitly demands lock-free atomics; also protects against future multi-threaded host code |
| `std::chrono::high_resolution_clock` | `SDL_GetPerformanceCounter` | `chrono` is heavier and not available in the same idiomatic way on all ESP32 toolchains; project already uses SDL3 timing in the runner |

**Installation:** No new dependencies. SDL3 and C++ stdlib are already present.

---

## Architecture Patterns

### Recommended Project Structure

New files for this phase:

```
include/enjin2/instrumentation/
    frame_timing.hpp          # The entire feature (header-only)
src/platform/sdl/
    sdl_main.cpp              # MODIFIED — four measurement sites + overlay + arg parse
tests/
    frame_timing_test.cpp     # Unit test: struct API, zero-overhead when disabled
```

No new CMakeLists.txt targets are needed. `frame_timing.hpp` is header-only and will be included by `sdl_main.cpp`. The test `frame_timing_test.cpp` must be added to `tests/CMakeLists.txt` and links only against `enjin2_core` (no Lua, no SDL).

### Pattern 1: Compile-Time Gate via Macro

**What:** Define `ENJIN2_FRAME_TIMING` when the feature is enabled. All struct fields and measurement calls are inside `#ifdef ENJIN2_FRAME_TIMING` blocks. The disabled path is a no-op empty struct plus no-op inline functions — the compiler eliminates everything.

**When to use:** Always — project uses this exact pattern for `ENJIN2_BUILD_LUA`, `ESP32`, and `EMSCRIPTEN` guards throughout.

**Example (disabled path — zero overhead):**

```cpp
// include/enjin2/instrumentation/frame_timing.hpp
#pragma once
#include <cstdint>

namespace enjin2 {

#ifdef ENJIN2_FRAME_TIMING
#include <atomic>

struct FrameTimingInstrumentation {
    std::atomic<uint32_t> updateTime_us{0};
    std::atomic<uint32_t> renderTime_us{0};
    std::atomic<uint32_t> luaTime_us{0};
    std::atomic<uint32_t> compositeTime_us{0};

    static FrameTimingInstrumentation& get() {
        static FrameTimingInstrumentation s_instance;
        return s_instance;
    }

    // Disallow copy/move — singleton semantics
    FrameTimingInstrumentation(const FrameTimingInstrumentation&) = delete;
    FrameTimingInstrumentation& operator=(const FrameTimingInstrumentation&) = delete;

private:
    FrameTimingInstrumentation() = default;
};

#else // ENJIN2_FRAME_TIMING not defined

struct FrameTimingInstrumentation {
    // Zero-size stub: no fields, no atomics written when instrumentation is off
    uint32_t updateTime_us{0};
    uint32_t renderTime_us{0};
    uint32_t luaTime_us{0};
    uint32_t compositeTime_us{0};

    static FrameTimingInstrumentation& get() {
        static FrameTimingInstrumentation s_instance;
        return s_instance;
    }

private:
    FrameTimingInstrumentation() = default;
};

#endif // ENJIN2_FRAME_TIMING

} // namespace enjin2
```

### Pattern 2: SDL3 High-Resolution Timing Measurement

**What:** Snapshot `SDL_GetPerformanceCounter()` before and after each phase, convert the delta to microseconds using `SDL_GetPerformanceFrequency()`.

**When to use:** Only in `sdl_main.cpp` inside `#ifdef ENJIN2_FRAME_TIMING`. The frequency is cached once at startup (it is constant per SDL session).

**Example:**

```cpp
// Cache once at top of main(), inside #ifdef ENJIN2_FRAME_TIMING
#ifdef ENJIN2_FRAME_TIMING
const Uint64 perf_freq = SDL_GetPerformanceFrequency();
#endif

// Inside the game loop, wrapping the Lua update phase:
#ifdef ENJIN2_FRAME_TIMING
Uint64 t_lua_start = SDL_GetPerformanceCounter();
#endif

// ... [lua update/draw calls] ...

#ifdef ENJIN2_FRAME_TIMING
Uint64 t_lua_end = SDL_GetPerformanceCounter();
enjin2::FrameTimingInstrumentation::get().luaTime_us.store(
    static_cast<uint32_t>((t_lua_end - t_lua_start) * 1000000u / perf_freq),
    std::memory_order_relaxed
);
#endif
```

### Pattern 3: Overlay Draw on Debug Layer

**What:** When `--show-timing` is parsed from argv, set a `bool show_timing` flag. At the end of each frame (after all Lua calls, before `composite()`), if the flag is set, draw 4 text lines to the debug canvas (`g_lua_layer4`, exposed as `g_lua.getBindings().getDebugCanvas()`).

**When to use:** SDL3 runner only. The debug layer (layer index 4) is already cleared per frame by `g_compositor.clearAll()` and is composited on top of all user layers. This is the established pattern for developer-visible overlays (see `engine.debug.*` in Phase 47).

**Example:**

```cpp
// After Lua draw() completes, before composite():
#ifdef ENJIN2_FRAME_TIMING
if (show_timing) {
    LuaCanvas* dbg = g_lua.getBindings().getDebugCanvas();
    if (dbg) {
        auto& ft = enjin2::FrameTimingInstrumentation::get();
        char buf[48];
        // 5x7 font: each char is 6px wide, 8px tall (size=1)
        // Line spacing: 9px to avoid overlap
        snprintf(buf, sizeof(buf), "lua  %4u us", ft.luaTime_us.load(std::memory_order_relaxed));
        dbg->drawText(buf, 1, 1, 8, 1, nullptr);
        snprintf(buf, sizeof(buf), "upd  %4u us", ft.updateTime_us.load(std::memory_order_relaxed));
        dbg->drawText(buf, 1, 10, 8, 1, nullptr);
        snprintf(buf, sizeof(buf), "comp %4u us", ft.compositeTime_us.load(std::memory_order_relaxed));
        dbg->drawText(buf, 1, 19, 8, 1, nullptr);
        snprintf(buf, sizeof(buf), "rdr  %4u us", ft.renderTime_us.load(std::memory_order_relaxed));
        dbg->drawText(buf, 1, 28, 8, 1, nullptr);
    }
}
#endif
```

Note: The debug canvas is cleared each frame by `g_compositor.clearAll()` (which sets `layers[4]` to transparent). The overlay text must be drawn after clearAll() and before composite().

### Pattern 4: Argv Parsing (Consistent with --fps / --script)

**What:** Parse `--show-timing` the same way `--fps` and `--script` are parsed in `sdl_main.cpp` — a simple `strcmp` loop over argv.

**Example:**

```cpp
bool show_timing = false;
for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--show-timing") == 0) {
        show_timing = true;
    }
}
```

### Identified Measurement Sites in sdl_main.cpp

The game loop has these logical phases that map to the four timing fields:

| Field | What to measure | Start marker | End marker |
|-------|----------------|-------------|------------|
| `luaTime_us` | All Lua scripting work (input callbacks + update() + coroutines + tweens + draw()) | Before `dispatchInputCallbacks` loop | After `draw()` pcall returns |
| `updateTime_us` | The C++ `scene::update` / engine state update (currently this is Lua-only in SDL runner; see note below) | Same as luaTime if there is no separate C++ update | After `tickTweens(dt)` |
| `compositeTime_us` | `g_compositor.composite()` | Before `g_compositor.composite()` | After returns |
| `renderTime_us` | SDL GPU upload + RenderPresent | Before `expand_canvas_to_rgb()` | After `SDL_RenderPresent()` |

**Note on updateTime_us vs luaTime_us:** In the current SDL runner, there is no separate C++ ECS `scene::update()` call — all update logic goes through Lua. The spec names four distinct fields. The cleanest mapping is:
- `luaTime_us` = time in Lua scripting calls (update() + draw() + coroutines + tweens)
- `updateTime_us` = for SDL runner, this can measure the same region OR be scoped to just the `update()` + coroutine + tween tick calls (excluding the draw() call)
- The planner should decide on the exact boundary; both interpretations are valid

The safest split for the SDL runner is:
- `luaTime_us` = entire Lua section (input callbacks through draw())
- `updateTime_us` = update() + tickCameraFollow + tickCoroutines + tickTweens only
- `renderTime_us` = SDL texture upload + RenderClear + RenderTexture + RenderPresent
- `compositeTime_us` = `g_compositor.composite()` + `expand_canvas_to_rgb()`

### Anti-Patterns to Avoid

- **Writing to atomics when `ENJIN2_FRAME_TIMING` is not defined:** The disabled stub must have zero write paths. No `store()` calls outside the `#ifdef` guard.
- **Using `SDL_GetTicks()` for sub-ms measurement:** Returns milliseconds only. Use `SDL_GetPerformanceCounter()`.
- **Placing overlay draw AFTER `composite()`:** The debug layer is composited at the same time as user layers. Drawing to it after `composite()` means the text is not visible on the current frame's output. Draw before composite.
- **Placing overlay draw BEFORE `clearAll()`:** `clearAll()` wipes the debug layer. Draw overlay text after `clearAll()` and before `composite()`.
- **Adding overhead when overlay is disabled at runtime:** The atomic reads for overlay display should be gated behind the `show_timing` bool — no reads when the overlay is off.
- **`snprintf` into a stack buffer too small for the format:** `"lua  %4u us"` with a uint32_t max (4294967295) is 14 chars label + 10 digits + 3 chars = 27 chars. 48-byte buffer is safe.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| High-resolution timestamps | Custom POSIX `clock_gettime` wrapper | `SDL_GetPerformanceCounter()` | Already linked; works on all SDL3 platforms; handles platform differences internally |
| Lock-free atomic storage | Manual volatile + memory barriers | `std::atomic<uint32_t>` | C++17, already required; correct memory model without undefined behavior |
| Text rasterization in overlay | Custom pixel font blit | `LuaCanvas::drawText(str, x, y, col, 1, nullptr)` | Existing facility; built-in 5x7 font; already used in `bindings_debug.cpp` |
| Compile-time feature toggle | CMake compile_definitions + bool flag | `#ifdef ENJIN2_FRAME_TIMING` preprocessor macro injected via CMake `target_compile_definitions` | Matches project pattern for `ENJIN2_BUILD_LUA=1`, `ESP32`, etc. |

**Key insight:** The debug layer (layer 4, `g_lua_layer4` / `m_debugCanvas`) already exists and is already cleared + composited each frame. The overlay is a draw-to-debug-layer + a text format — no new rendering infrastructure needed.

---

## Common Pitfalls

### Pitfall 1: Drawing Overlay at Wrong Point in Frame
**What goes wrong:** Text appears one frame late or is invisible.
**Why it happens:** Debug layer is cleared by `clearAll()`. If overlay draw happens before `clearAll()`, the text is wiped. If it happens after `composite()`, the text never reaches the output buffer.
**How to avoid:** Draw overlay text in the window: `after clearAll()` and `before composite()`. In `sdl_main.cpp`, the natural insertion point is immediately after the Lua section closes and before the `g_compositor.composite()` call.
**Warning signs:** Blank overlay with no text visible despite `--show-timing` flag being set.

### Pitfall 2: Measuring Time While Still Inside Lua pcall
**What goes wrong:** Lua timing includes error-handling branch overhead when a Lua error occurs.
**Why it happens:** The timing start/stop wraps the entire pcall block including error branches.
**How to avoid:** This is acceptable behavior — error handling is rare and the overhead is negligible. Do not add timing checkpoints inside the pcall error branch.

### Pitfall 3: Atomic Include Under ESP32/WASM
**What goes wrong:** `<atomic>` causes compile error or pulls in libatomic dependency on some toolchains.
**Why it happens:** On some 32-bit ARM (ESP32) targets, 64-bit atomics require libatomic. `uint32_t` atomics (4 bytes) are natively lock-free on both Xtensa LX7 (ESP32-S3) and wasm32 — no libatomic needed.
**How to avoid:** Use `uint32_t` (not `uint64_t`) for all four fields. Confirm with `static_assert(std::atomic<uint32_t>::is_always_lock_free)` — this will fire at compile time if not lock-free.
**Warning signs:** Link error mentioning `__atomic_load_4` or `libatomic` on ESP32 build.

### Pitfall 4: `ENJIN2_FRAME_TIMING` Not Injected for SDL Target
**What goes wrong:** The code compiles but timing fields are always zero (disabled stub).
**Why it happens:** CMake `target_compile_definitions` was not added to `enjin2_sdl` target.
**How to avoid:** Add `ENJIN2_FRAME_TIMING=1` to `enjin2_sdl`'s compile definitions in `CMakeLists.txt`. The WASM and ESP32 targets must NOT have this definition (header compiles cleanly without it).
**Warning signs:** `FrameTimingInstrumentation::get().luaTime_us` always reads 0 even with actual Lua work being done.

### Pitfall 5: `perf_freq` Division by Zero
**What goes wrong:** Division by zero on a platform where `SDL_GetPerformanceFrequency()` returns 0.
**Why it happens:** Theoretically impossible on any supported SDL3 platform, but defensive code is better.
**How to avoid:** Cache freq once after `SDL_Init`, assert it is non-zero. In practice on Linux/macOS/Windows it is always > 0.

### Pitfall 6: Overlay Text Overflows Canvas Bounds
**What goes wrong:** Text is partially off-screen or wraps incorrectly.
**Why it happens:** Canvas is 128x128. The built-in 5x7 font at size 1 gives 6px per char. Four lines of 11 chars = 66px wide, 4 lines at 9px each = 36px tall. This fits comfortably in the top-left corner.
**How to avoid:** Fix x=1, y=1, y+=9 per line. Do not use `drawTextWrapped` — fixed positions are more predictable for debug HUD.

---

## Code Examples

Verified patterns from project sources:

### SDL3 High-Resolution Timer (from SDL3 documentation, verified via SDL3 source tag release-3.4.2)
```cpp
// One-time cache at startup
Uint64 perf_freq = SDL_GetPerformanceFrequency();

// Per-measurement
Uint64 t0 = SDL_GetPerformanceCounter();
// ... work ...
Uint64 t1 = SDL_GetPerformanceCounter();
uint32_t elapsed_us = static_cast<uint32_t>((t1 - t0) * 1000000u / perf_freq);
```

### Atomic Store with Relaxed Ordering (C++17)
```cpp
#include <atomic>
std::atomic<uint32_t> luaTime_us{0};

// Writer (game loop thread — sole writer):
luaTime_us.store(elapsed_us, std::memory_order_relaxed);

// Reader (polling API — same thread or any thread):
uint32_t val = luaTime_us.load(std::memory_order_relaxed);
```

Rationale for `relaxed`: The timing fields are independent values with no ordering dependency between them. A reader that sees luaTime_us written does not need to also see a specific value of compositeTime_us. `relaxed` is correct and produces optimal code on all targets.

### Overlay Text Draw (from bindings_debug.cpp pattern)
```cpp
// LuaCanvas* dbg = g_lua.getBindings().getDebugCanvas();
// Already established pattern — drawText at fixed coords:
char buf[48];
snprintf(buf, sizeof(buf), "lua  %4u us", val);
dbg->drawText(buf, 1, 1, 8, 1, nullptr);  // color=8 (mid-grey), size=1, default font
```

### Argv Parsing (from sdl_main.cpp existing pattern)
```cpp
bool show_timing = false;
for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--show-timing") == 0) {
        show_timing = true;
    }
}
```

### Unit Test Pattern (from debug_draw_test.cpp)
```cpp
// Minimal test: construct bindings, call API, assert no crash
struct TimingFixture {
    enjin2::LuaEngine engine;
    enjin2::LuaBindings bindings;
    TimingFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }
};

// Test: FrameTimingInstrumentation::get() returns consistent singleton
static void test_singleton_identity() {
    auto& a = enjin2::FrameTimingInstrumentation::get();
    auto& b = enjin2::FrameTimingInstrumentation::get();
    ASSERT(&a == &b, "get() must return the same instance");
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `SDL_GetTicks()` (ms resolution) | `SDL_GetPerformanceCounter()` (sub-ms) | SDL3 established; project currently uses SDL_GetTicks for frame pacing only | Enables the "sub-millisecond resolution" requirement (FRAME-01) |
| Per-thread lock | `std::atomic` with relaxed ordering | C++11+ | Zero overhead on single-threaded paths; correct for future multi-threaded access |
| Separate profiling library (Tracy, Remotery) | In-process atomic fields | Project decision (OUT OF SCOPE per REQUIREMENTS.md) | Matches zero-alloc, no-background-thread constraint |

**Deprecated/outdated:**
- Tracy Profiler: Explicitly out of scope (REQUIREMENTS.md: "Requires background thread + network; violates zero-alloc/zero-threading model")
- `SDL_GetTicks()` for sub-ms measurement: ms granularity only, insufficient for frame phase breakdown

---

## Open Questions

1. **Exact boundary between luaTime_us and updateTime_us**
   - What we know: SDL runner has no separate C++ ECS update path. Both lua update and Lua draw run in the same `lua_ok` block.
   - What's unclear: Whether `updateTime_us` should measure (a) just the update()+coroutines+tweens calls, or (b) be identical to luaTime_us (Lua-only runner has no separate C++ update phase).
   - Recommendation: Planner should split as: `luaTime_us` = full Lua section (input dispatch + update + coroutines/tweens + draw); `updateTime_us` = update() + tickCameraFollow + tickCoroutines + tickTweens only (excluding draw). This gives four distinct non-overlapping measurements.

2. **Whether to add CMake option `ENJIN2_FRAME_TIMING` as a CMake OPTION or always-on for SDL builds**
   - What we know: FRAME-01 spec says "zero overhead... when instrumentation is off". The success criteria says WASM/ESP32 compile cleanly. SDL3 runner is the only target that needs it on.
   - What's unclear: Whether users will ever want SDL builds without instrumentation.
   - Recommendation: Always enable for `enjin2_sdl` target via `target_compile_definitions`; leave it absent for WASM and ESP32 targets. No CMake OPTION needed — the SDL build is the target for this feature.

3. **`--show-timing` with no Lua (`ENJIN2_BUILD_LUA=OFF` SDL build)**
   - What we know: sdl_main.cpp can be built without Lua; luaTime_us would be 0.
   - What's unclear: Whether the overlay should still display (showing 0 for Lua fields).
   - Recommendation: Overlay draw is outside `#ifdef ENJIN2_BUILD_LUA`, so it will display even with no Lua. luaTime_us will read 0. This is correct and informative behavior.

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Project-own assertion macros (ASSERT macro, fprintf stderr) — same as debug_draw_test.cpp |
| Config file | tests/CMakeLists.txt — add `frame_timing_test` entry |
| Quick run command | `cmake --build build --target frame_timing_test && ./build/tests/frame_timing_test` |
| Full suite command | `cmake --build build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FRAME-01 | FrameTimingInstrumentation singleton exists, fields are atomic uint32_t, `get()` returns same instance | unit | `./build/tests/frame_timing_test` | Wave 0 |
| FRAME-01 | Disabled stub compiles cleanly (no ENJIN2_FRAME_TIMING defined) | compile-check | `cmake --build build -DENJIN2_FRAME_TIMING=OFF` | Wave 0 |
| FRAME-02 | SDL runner accepts `--show-timing` flag without crash | smoke/manual | Run `./build/sdl3/enjin2_sdl --show-timing --script scripts/layer_demo.lua` | Manual |
| FRAME-02 | Timing values update each frame (non-zero after Lua work) | smoke/manual | Read overlay values for several frames | Manual |
| FRAME-03 | `FrameTimingInstrumentation::get()` returns readable fields without overlay active | unit | `frame_timing_test` — store known value, read it back | Wave 0 |
| FRAME-03 | WASM build compiles cleanly with new header present | compile-check | `build.sh --target wasm` | Manual |
| FRAME-03 | ESP32 build compiles cleanly with new header present | compile-check | `build.sh --target esp32` | Manual |

### Sampling Rate
- **Per task commit:** `cmake --build build/sdl3 --target enjin2_sdl && cmake --build build --target frame_timing_test && ./build/tests/frame_timing_test`
- **Per wave merge:** Full `ctest` suite
- **Phase gate:** All ctest tests green + manual smoke of `--show-timing` overlay displaying non-zero values

### Wave 0 Gaps
- [ ] `tests/frame_timing_test.cpp` — covers FRAME-01 and FRAME-03 unit assertions
- [ ] Entry in `tests/CMakeLists.txt` for `frame_timing_test` executable (links `enjin2_core` only, no Lua)
- [ ] `include/enjin2/instrumentation/frame_timing.hpp` — the new header (Wave 0 creates it as part of the first task)

---

## Sources

### Primary (HIGH confidence)
- SDL3 `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency` — SDL3 release-3.4.2 source and docs confirm sub-microsecond capability; returns `Uint64` counter with platform-specific frequency
- Project source: `src/platform/sdl/sdl_main.cpp` — full game loop read; identified all four measurement sites
- Project source: `include/enjin2/scripting/bindings.hpp` + `src/scripting/bindings_debug.cpp` — established `LuaCanvas::drawText` + `m_debugCanvas` pattern for overlay
- Project source: `include/enjin2/graphics/layer_compositor.hpp` — confirmed layer 4 is cleared by `clearAll()` every frame
- Project source: `tests/CMakeLists.txt` + `tests/debug_draw_test.cpp` — test structure pattern

### Secondary (MEDIUM confidence)
- C++17 `std::atomic<uint32_t>` lock-free guarantee on 32-bit targets (Xtensa LX7, wasm32): all mainstream embedded/WASM compilers implement 32-bit atomics without libatomic; verified by project's stated C++17 requirement
- `memory_order_relaxed` correctness for independent counter fields: standard C++ reference; no ordering relationship needed between the four timing fields in a single-threaded game loop

### Tertiary (LOW confidence)
- None

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — SDL3 and C++ stdlib are established; no new dependencies
- Architecture: HIGH — game loop in sdl_main.cpp is fully read; measurement sites are unambiguous; debug layer pattern is established
- Pitfalls: HIGH — overlay draw ordering pitfall is verified against actual clearAll()/composite() call order in the real source

**Research date:** 2026-03-08
**Valid until:** 2026-06-08 (SDL3 API is stable; C++17 stdlib is stable)
