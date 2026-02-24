# Phase 21: SDL3 CMake + Runner - Research

**Researched:** 2026-02-24
**Domain:** SDL3 CMake integration, streaming texture rendering, keyboard input, fixed-rate game loop
**Confidence:** HIGH

## Summary

Phase 21 delivers a single opt-in executable (`enjin2_sdl`) guarded by `ENJIN2_BUILD_SDL=ON/OFF`. It creates a fixed-size SDL3 window, uploads a Canvas4's palette-resolved pixels to a streaming RGB24 texture each frame, runs a fixed-rate game loop with delta-time clamping, and maps SDL3 keyboard state to the Phase 20 `InputState`. The CMake integration uses `FetchContent` to auto-download SDL3 at a pinned release tag with `EXCLUDE_FROM_ALL` to prevent SDL's install rules from bleeding into the project.

The work splits cleanly into three areas: (1) CMake option + FetchContent SDL3 acquisition, (2) the window/renderer/texture/render loop, and (3) the SDL3 → InputState keyboard poll implementation (`input_platform_poll`). All three are well-bounded with stable SDL3 APIs confirmed in the official documentation (SDL 3.2.0+, latest stable SDL 3.4.2 as of 2026-02-21).

The critical architectural insight is that `input_platform_poll` was deliberately left undefined in Phase 20 — Phase 21 is where the SDL3 `.cpp` implementing it is finally provided. This is the platform integration contract defined in Phase 20's CONTEXT.md.

**Primary recommendation:** Create `src/platform/sdl/sdl_main.cpp` as a single-file SDL3 runner (SDL init, window, renderer, texture, game loop, keyboard poll, shutdown) with a CMake `option(ENJIN2_BUILD_SDL ...)` block in the root `CMakeLists.txt`. Pixel upload: `Canvas4::getBuffer()` → RGB24 expand via palette → `SDL_UpdateTexture`. Scale: `SDL_SetRenderScale(renderer, 4.0f, 4.0f)` + `SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**SDL3 dependency acquisition**
- Use CMake FetchContent to auto-download SDL3 — zero contributor setup required
- Pin to the latest stable SDL3 release tag at implementation time (reproducible builds)
- If FetchContent fails when `ENJIN2_BUILD_SDL=ON`, issue a hard CMake error and stop configure — no silent fallbacks
- SDL3 is a build-only dependency; it must NOT appear in install/package targets

**Window & integer scaling**
- Default scale factor: **4x** (e.g. a 128×128 canvas renders as a 512×512 window)
- Window is **fixed size, not resizable** — pixel-perfect integer scaling guaranteed always
- Window title: **"Enjin2"**
- No fullscreen toggle — windowed only for this phase

**Keyboard mapping**
- **WASD mirrors arrow keys** — both sets write the same directional button bits in InputState
- **Escape = quit only** — closes the runner cleanly, not mapped to any InputState button
- Mapping is **hardcoded**, not configurable
- Button indices follow **whatever Phase 20's InputState defines** — SDL mapping aligns to existing indices (Up/Down/Left/Right/A/B/Start as defined there)
- Z = A button, X = B button, Enter = Start (per phase spec)

**Game loop & timing**
- Target frame rate is **configurable via `--fps N` flag**, default **30fps**
- Delta-time is clamped to a **4-frame ceiling** (at 30fps ≈ 133ms max dt) — prevents spiral-of-death on stall
- Loop shuts down cleanly on window close or Escape
- Before any script is loaded, display a **blank canvas with the default palette** (no test pattern, no fake content)

### Claude's Discretion
- Exact timing source (SDL_GetTicks64 vs high-resolution clock)
- Sleep/yield strategy for frame pacing (SDL_Delay vs busy-wait)
- Canvas-to-SDL texture format and upload path

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SDL-01 | CMake ENJIN2_BUILD_SDL=ON/OFF option with no impact on WASM or ESP32 builds | `option(ENJIN2_BUILD_SDL ...)` block in root CMakeLists.txt; SDL3 FetchContent inside `if(ENJIN2_BUILD_SDL)` guard; `EXCLUDE_FROM_ALL` prevents install bleed. The WASM guard (`if(EMSCRIPTEN)`) and ESP32 guard remain untouched. |
| SDL-02 | SDL3 window with Canvas4-to-RGB texture blit via palette lookup | `SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, W, H)` + per-frame expand loop using `g_palette.resolve(pixel_index)` to fill RGB24 staging buffer, then `SDL_UpdateTexture`. |
| SDL-03 | Integer pixel scaling with nearest-neighbor filtering | `SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)` + `SDL_SetRenderScale(renderer, 4.0f, 4.0f)`. Window created at `canvas_w * scale` × `canvas_h * scale` pixels. |
| SDL-04 | Game loop with event polling, delta time, and clean shutdown | `SDL_PollEvent` loop for quit/Escape, `SDL_GetTicks()` (returns Uint64 ms in SDL3) for delta, `SDL_Delay` for frame pacing, clean `SDL_Quit()` on exit. |
| INP-04 | SDL3 keyboard-to-button default mapping (arrows, Z/X, Enter) | `SDL_GetKeyboardState` after `SDL_PollEvent` drain; scancode array indexed by `SDL_SCANCODE_*`; `input_platform_poll` implementation provided in this phase. |
</phase_requirements>

## Standard Stack

### Core

| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| SDL3 | 3.4.2 (latest stable, 2026-02-21) | Window, renderer, texture, events, timing | Official stable SDL3; SDL3 is stable since Jan 2025 (per REQUIREMENTS.md out-of-scope note) |
| `SDL3::SDL3` CMake target | Ships with SDL3 | Link target after FetchContent | Guaranteed alias to SDL3-shared or SDL3-static; always available |
| `CMake FetchContent` | CMake 3.16+ (project minimum) | Auto-download SDL3 | Already in project; zero setup for contributors |
| `SDL_PIXELFORMAT_RGB24` | SDL3 3.2.0+ | 3-byte/pixel texture for palette-expanded canvas | Simple pitch calculation; no alpha needed (palette index 15 handled by skipping transparent pixels or writing a background color) |
| `SDL_TEXTUREACCESS_STREAMING` | SDL3 3.2.0+ | Mutable texture updated each frame | Canvas pixels change every frame |
| `SDL_GetTicks()` | SDL3 3.2.0+ | Millisecond timer returning Uint64 | SDL3 merged SDL_GetTicks64 into SDL_GetTicks — returns 64-bit now; no overflow |

### Supporting

| Component | Purpose | When to Use |
|-----------|---------|-------------|
| `SDL_SetRenderScale(renderer, 4.0f, 4.0f)` | Integer 4x scale of all render output | Scale once after renderer creation; avoids need for `SDL_SetRenderLogicalPresentation` |
| `SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)` | Nearest-neighbor filtering on texture | Required — SDL3's default changed to bilinear |
| `SDL_GetKeyboardState(NULL)` | Per-frame scan of all key states | After `SDL_PollEvent` drain; feeds `input_platform_poll` for smooth held-key detection |
| `SDL_Delay(ms)` | Sleep between frames | Frame pacing; ~millisecond accuracy — sufficient for 30fps |
| `SDL_CreateWindowAndRenderer` | Create window + renderer together | Avoids window flicker compared to separate create calls |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `SDL_UpdateTexture` | `SDL_LockTexture/SDL_UnlockTexture` | Lock/unlock is the streaming best practice for high-frequency updates, but `SDL_UpdateTexture` is simpler and sufficient for 30fps Canvas4 (128×128 = 16KB RGB). Use `SDL_UpdateTexture` unless profiling reveals bottleneck. |
| `SDL_PIXELFORMAT_RGB24` | `SDL_PIXELFORMAT_ABGR8888` (32-bit) | 32-bit has better hardware compatibility; RGB24 is simpler for the palette expand (3-byte writes). Prefer RGB24 for simplicity unless rendering issues arise on specific platforms. |
| `SDL_SetRenderScale` | `SDL_SetRenderLogicalPresentation` | `SDL_SetRenderLogicalPresentation` has a known issue where texture scale mode (NEAREST) is not applied correctly via the logical presentation path (GitHub issue #11335). `SDL_SetRenderScale` is the confirmed workaround. |
| `SDL_GetTicks()` (ms) | `SDL_GetPerformanceCounter()` (sub-ms) | Sub-millisecond timing is overkill at 30fps; `SDL_GetTicks()` millisecond precision is sufficient. Simpler code. |
| `FetchContent_Declare` only | `FetchContent_Declare` + `FIND_PACKAGE_ARGS` | The `FIND_PACKAGE_ARGS` variant prefers a system-installed SDL3. For enjin2 (zero-setup intent) the pure FetchContent path is simpler and more reproducible. |

**Installation:** No manual steps. CMake `FetchContent_MakeAvailable(SDL3)` downloads SDL3 from GitHub on first configure when `ENJIN2_BUILD_SDL=ON`.

## Architecture Patterns

### Recommended Project Structure

```
src/
└── platform/
    └── sdl/
        └── sdl_main.cpp     # SDL3 runner: window, renderer, texture, game loop, input_platform_poll

CMakeLists.txt               # ENJIN2_BUILD_SDL option + FetchContent + enjin2_sdl executable
```

### Pattern 1: CMake FetchContent with Hard Error on Failure

**What:** `option(ENJIN2_BUILD_SDL "Build SDL3 desktop runner" OFF)` block in root `CMakeLists.txt`. When `ON`, `FetchContent_Declare` pinned to a stable tag with `EXCLUDE_FROM_ALL`. `FetchContent_MakeAvailable` will error-exit CMake configure if the download fails (no silent fallback). SDL3 never leaks into other targets.

**When to use:** Always for this phase. Matches locked decision: "If FetchContent fails when `ENJIN2_BUILD_SDL=ON`, issue a hard CMake error and stop configure."

**Example:**
```cmake
option(ENJIN2_BUILD_SDL "Build SDL3 desktop runner executable" OFF)

if(ENJIN2_BUILD_SDL)
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.2
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(SDL3)

    add_executable(enjin2_sdl
        src/platform/sdl/sdl_main.cpp
    )
    target_include_directories(enjin2_sdl PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_link_libraries(enjin2_sdl PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_input
        SDL3::SDL3
    )
endif()
```

Note: `enjin2_sdl` links `enjin2_input` (not the aggregate `enjin2`) to avoid pulling in Lua and WASM targets. `SDL3::SDL3` is only linked to this target — SDL3 symbols never enter any core library.

### Pattern 2: Canvas4 → RGB24 Pixel Expand

**What:** Per-frame loop over every Canvas4 pixel. Unpack each 4-bit index from the packed buffer, call `g_palette.resolve()` for non-transparent pixels, and write 3 bytes into a staging `uint8_t` buffer. Pass the staging buffer to `SDL_UpdateTexture`.

**When to use:** Every frame. Canvas dimensions are known at compile time (template params), so the loop is bounded.

**Example:**
```cpp
// Source: palette.hpp API + Canvas4::getBuffer() API (codebase inspection)
static uint8_t rgb_staging[CANVAS_W * CANVAS_H * 3]; // stack or static

void expand_canvas_to_rgb(const enjin2::Canvas4<CANVAS_W, CANVAS_H>& canvas) {
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            enjin2::Pixel4 px = canvas.getPixel(x, y);
            int i = (y * CANVAS_W + x) * 3;
            if (!enjin2::g_palette.isTransparent(px.value)) {
                enjin2::RGB rgb = enjin2::g_palette.resolve(px.value);
                rgb_staging[i + 0] = rgb.r;
                rgb_staging[i + 1] = rgb.g;
                rgb_staging[i + 2] = rgb.b;
            } else {
                // Transparent index: write a background color (e.g., black)
                rgb_staging[i + 0] = 0;
                rgb_staging[i + 1] = 0;
                rgb_staging[i + 2] = 0;
            }
        }
    }
}

// Then upload:
SDL_UpdateTexture(texture, nullptr, rgb_staging, CANVAS_W * 3);
```

### Pattern 3: SDL3 Game Loop With Delta-Time Clamping

**What:** Fixed-rate game loop at configurable FPS (default 30). Delta-time is clamped to 4 frames maximum (~133ms at 30fps). `SDL_PollEvent` drains all events first (required to update keyboard state), then `SDL_GetKeyboardState` reads held keys for `InputState`.

**When to use:** Main loop body. This matches locked decisions on timing, clamping, and loop shutdown.

**Example:**
```cpp
// Source: SDL3 wiki SDL_PollEvent + SDL_GetKeyboardState pattern
// + BestKeyboardPractices wiki recommendation

const int fps = 30; // or from --fps arg
const Uint64 frame_ms = 1000 / fps;
const float max_dt_sec = (4.0f / fps);  // 4-frame ceiling

Uint64 last_ticks = SDL_GetTicks();
bool running = true;

while (running) {
    // 1. Drain all events (also updates keyboard state)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) running = false;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
            running = false;
    }

    // 2. Compute delta time (clamped)
    Uint64 now = SDL_GetTicks();
    float dt = (now - last_ticks) / 1000.0f;
    if (dt > max_dt_sec) dt = max_dt_sec;
    last_ticks = now;

    // 3. Read keyboard state into InputState (input_platform_poll)
    input_advance_frame(&g_input);
    input_platform_poll(&g_input);

    // 4. Game update (future: call Lua script)
    // (Phase 21: blank canvas with default palette — no update needed)

    // 5. Render
    expand_canvas_to_rgb(g_canvas);
    SDL_UpdateTexture(texture, nullptr, rgb_staging, CANVAS_W * 3);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    // 6. Frame pacing
    Uint64 frame_elapsed = SDL_GetTicks() - now;
    if (frame_elapsed < frame_ms) {
        SDL_Delay(static_cast<Uint32>(frame_ms - frame_elapsed));
    }
}
```

### Pattern 4: SDL3 keyboard → InputState (`input_platform_poll`)

**What:** `input_platform_poll` is the Phase 20 platform hook. This is where Phase 21 provides its SDL3 definition. Uses `SDL_GetKeyboardState` (scancode-based, layout-independent) for movement keys (arrows, WASD), and also maps Z/X/Enter to buttons.

**When to use:** Called once per frame after `input_advance_frame`.

**Example:**
```cpp
// Source: BestKeyboardPractices wiki + SDL_Scancode wiki
// Using scancodes for layout-independent arrow/WASD mapping

// Project-defined button indices (from Phase 20 InputState + INP-04 mapping):
// These must match whatever Phase 20's implementation defines.
// Based on INP-04: Up/Down/Left/Right/A(Z)/B(X)/Start(Enter)
enum GameButton {
    BTN_UP    = 0,
    BTN_DOWN  = 1,
    BTN_LEFT  = 2,
    BTN_RIGHT = 3,
    BTN_A     = 4,  // Z key
    BTN_B     = 5,  // X key
    BTN_START = 6,  // Enter key
};

namespace enjin2 {
void input_platform_poll(InputState* state) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    auto set_btn = [&](int btn, bool pressed) {
        if (pressed) state->buttons |= static_cast<uint16_t>(1u << btn);
    };

    set_btn(BTN_UP,    keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]);
    set_btn(BTN_DOWN,  keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]);
    set_btn(BTN_LEFT,  keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]);
    set_btn(BTN_RIGHT, keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]);
    set_btn(BTN_A,     keys[SDL_SCANCODE_Z]);
    set_btn(BTN_B,     keys[SDL_SCANCODE_X]);
    set_btn(BTN_START, keys[SDL_SCANCODE_RETURN]);
}
} // namespace enjin2
```

Note: Escape is handled directly in the event loop (`SDL_EVENT_KEY_DOWN` + `SDLK_ESCAPE` = quit), not mapped to any button.

### Pattern 5: Window and Renderer Initialization

**What:** `SDL_CreateWindowAndRenderer` to avoid flicker on some platforms. Fixed size = canvas dimensions × scale factor. No `SDL_WINDOW_RESIZABLE`.

**Example:**
```cpp
// Source: SDL_CreateWindow wiki + SDL_CreateWindowAndRenderer recommendation
const int CANVAS_W = 128, CANVAS_H = 128;
const int SCALE = 4;
const int WIN_W = CANVAS_W * SCALE;
const int WIN_H = CANVAS_H * SCALE;

SDL_Window*   window   = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_CreateWindowAndRenderer("Enjin2", WIN_W, WIN_H, 0, &window, &renderer);
// 0 flags = fixed size (no SDL_WINDOW_RESIZABLE)

SDL_Texture* texture = SDL_CreateTexture(renderer,
    SDL_PIXELFORMAT_RGB24,
    SDL_TEXTUREACCESS_STREAMING,
    CANVAS_W, CANVAS_H);

SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
SDL_SetRenderScale(renderer, static_cast<float>(SCALE), static_cast<float>(SCALE));
```

### Pattern 6: Command-Line Argument Parsing (`--fps N`)

**What:** Simple manual `argc/argv` parse. No argument parsing library needed.

**Example:**
```cpp
int fps = 30; // default
for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--fps") == 0) {
        fps = atoi(argv[i + 1]);
        if (fps <= 0 || fps > 300) fps = 30; // clamp to sane range
    }
}
```

### Anti-Patterns to Avoid

- **Including `<SDL3/SDL.h>` in any core library header:** SDL3 must only be included in `src/platform/sdl/sdl_main.cpp`. Any transitive include from `enjin2_core`, `enjin2_graphics`, or `enjin2_input` to an SDL3 header violates SDL-01.
- **Linking `SDL3::SDL3` to `enjin2` or `enjin2_graphics`:** SDL symbols must only be in `enjin2_sdl`. Core libraries must have no SDL3 link dependencies.
- **Using `SDL_SetRenderLogicalPresentation` for integer scaling:** There is a confirmed SDL3 bug (GitHub issue #11335) where this function ignores the texture scale mode and applies linear filtering. Use `SDL_SetRenderScale` instead.
- **Using `SDL_GetTicks64()`:** Removed in SDL3. Use `SDL_GetTicks()` which now returns `Uint64`.
- **Using key event `SDL_EVENT_KEY_DOWN` for held-key detection:** Key-repeat events have OS-level delay (~1s initial). Always drain events first, then use `SDL_GetKeyboardState` for per-frame held detection.
- **Providing a definition of `input_platform_poll` in any core library:** Phase 20 left it undefined intentionally. Phase 21 defines it exactly once in `sdl_main.cpp`. Multiple definitions → linker error.
- **Using `SDL_SCALEMODE_PIXELART` if pinned to SDL 3.4.0+:** This mode is only available in SDL 3.4.0+. If pinning to 3.4.x this is fine, but ensure the GIT_TAG matches. `SDL_SCALEMODE_NEAREST` is the safe fallback (available since 3.2.0).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| SDL3 download | Manual git clone step | `FetchContent_Declare` + `FetchContent_MakeAvailable` | Zero contributor setup; CMake handles clone, configure, build |
| Frame rate limiting | Busy-wait spin loop | `SDL_Delay` | SDL_Delay cedes CPU to OS scheduler; spin loop burns 100% CPU |
| Keyboard state tracking (held vs pressed) | Manual per-frame key state array | `SDL_GetKeyboardState` | SDL maintains this array correctly across platforms and layout differences |
| Pixel scaling | Render each pixel as N×N rect | `SDL_SetRenderScale` + `SDL_SetTextureScaleMode(NEAREST)` | GPU-accelerated; O(1) cost regardless of canvas size |
| Integer argument parsing | getopt or argparse | Simple `argv` loop | No external dep needed for one `--fps` flag |

**Key insight:** SDL3 already handles all platform complexity (DPI, window management, key mapping, thread safety). The runner's job is to connect the enjin2 canvas and input APIs to SDL3 — not to re-implement what SDL3 already provides.

## Common Pitfalls

### Pitfall 1: SDL3 Symbols Leaking Into Core Libraries
**What goes wrong:** A `#include <SDL3/SDL.h>` lands in a core header (or a core header transitively includes it), causing `cmake -DENJIN2_BUILD_SDL=OFF` to require SDL3 headers.
**Why it happens:** Accidentally placing SDL init code in a shared utility function.
**How to avoid:** `sdl_main.cpp` is the only file that includes SDL headers. All SDL calls stay in that one translation unit.
**Warning signs:** `cmake -DENJIN2_BUILD_SDL=OFF && cmake --build build` produces errors about missing SDL3 headers.

### Pitfall 2: Wrong Scale Mode (Bilinear Default in SDL3)
**What goes wrong:** Canvas pixels appear blurry — SDL3 changed the default texture scale mode from nearest to bilinear.
**Why it happens:** Not calling `SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)` after creating the texture.
**How to avoid:** Always set `SDL_SCALEMODE_NEAREST` immediately after `SDL_CreateTexture`.
**Warning signs:** Canvas renders but colors appear smoothly blended between pixels instead of sharp pixel edges.

### Pitfall 3: `SDL_SetRenderLogicalPresentation` Ignores Scale Mode
**What goes wrong:** Using `SDL_SetRenderLogicalPresentation` for scaling shows linear/blurry pixels despite passing `SDL_SCALEMODE_NEAREST` as argument.
**Why it happens:** Known SDL3 bug (GitHub issue #11335) — the logical presentation path overrides texture scale mode.
**How to avoid:** Use `SDL_SetRenderScale(renderer, 4.0f, 4.0f)` instead for the integer upscale.
**Warning signs:** Pixels appear blurry even after setting scale mode.

### Pitfall 4: Calling `input_advance_frame` After `input_platform_poll`
**What goes wrong:** Button presses read in `input_platform_poll` are immediately cleared by `advance_frame` called in the wrong order. All inputs appear as `justReleased` every frame.
**Why it happens:** Order matters: advance (prev = current, clear current) → poll (write to current) → game reads.
**How to avoid:** Always call `input_advance_frame(&state)` BEFORE `input_platform_poll(&state)`.
**Warning signs:** `held()` always returns false; `justReleased()` fires every frame for keys.

### Pitfall 5: Canvas Dimension Mismatch Between Texture and Render Scale
**What goes wrong:** The SDL texture is created at the wrong size, or window size doesn't match canvas × scale, resulting in letterboxing or pixel misalignment.
**Why it happens:** Hardcoding texture size independent of canvas template parameters.
**How to avoid:** Use `canvas.getWidth()` / `canvas.getHeight()` for texture creation; window size = `canvas_w * scale`.
**Warning signs:** Window has black bars, or canvas fills only part of the window.

### Pitfall 6: `SDL_GetTicks64()` Not Found
**What goes wrong:** Linker or compile error: `SDL_GetTicks64` undeclared/undefined.
**Why it happens:** SDL3 removed `SDL_GetTicks64`. SDL3's `SDL_GetTicks()` now returns `Uint64`.
**How to avoid:** Use `SDL_GetTicks()` everywhere. Variable type is `Uint64`.
**Warning signs:** Compile error in SDL3: `undefined reference to SDL_GetTicks64`.

### Pitfall 7: RGB24 Pitch Calculation Error
**What goes wrong:** `SDL_UpdateTexture` writes with wrong stride — canvas appears skewed or garbled.
**Why it happens:** Passing `CANVAS_W * 4` as pitch for RGB24 (correct for ABGR8888, wrong for RGB24).
**How to avoid:** RGB24 pitch = `CANVAS_W * 3`. Always match pitch to format bytes-per-pixel.
**Warning signs:** Canvas renders but with horizontal shear (each row appears offset).

### Pitfall 8: FetchContent SDL3 Appearing in Install Targets
**What goes wrong:** `cmake --install` also installs SDL3 shared libraries.
**Why it happens:** Missing `EXCLUDE_FROM_ALL` in `FetchContent_Declare`.
**How to avoid:** Always include `EXCLUDE_FROM_ALL` in `FetchContent_Declare(SDL3 ...)`.
**Warning signs:** `cmake --install` copies SDL3 `.so`/`.dll` files to install prefix.

## Code Examples

### Complete CMake SDL3 Block (root CMakeLists.txt addition)

```cmake
# Source: SDL3 README-cmake wiki + FetchContent official CMake docs
option(ENJIN2_BUILD_SDL "Build SDL3 desktop runner executable (requires internet on first build)" OFF)

if(ENJIN2_BUILD_SDL)
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.2
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(SDL3)
    # FetchContent_MakeAvailable will CMake-error if download fails — no fallback needed

    add_executable(enjin2_sdl
        src/platform/sdl/sdl_main.cpp
    )
    target_include_directories(enjin2_sdl PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_link_libraries(enjin2_sdl PRIVATE
        enjin2_core
        enjin2_graphics
        enjin2_input
        SDL3::SDL3
    )
endif()
```

### Minimal SDL3 Runner Skeleton (`src/platform/sdl/sdl_main.cpp`)

```cpp
// Source: SDL3 wiki patterns — SDL_CreateWindowAndRenderer, SDL_PollEvent,
//         SDL_GetKeyboardState, SDL_GetTicks, SDL_UpdateTexture
//         + enjin2 palette.hpp and canvas.hpp APIs (codebase inspection)

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdlib>
#include <cstring>

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/palette.hpp>
#include <enjin2/input/input_state.hpp>

// Canvas dimensions — adjust to match Phase 19/20 conventions
static constexpr int CANVAS_W = 128;
static constexpr int CANVAS_H = 128;
static constexpr int DEFAULT_SCALE = 4;

// Global engine state (static lifetime — no heap allocation)
static enjin2::Canvas4<CANVAS_W, CANVAS_H> g_canvas;
static enjin2::InputState g_input{};
static uint8_t g_rgb_staging[CANVAS_W * CANVAS_H * 3]; // RGB24 staging buffer

// Button index constants (match Phase 20 InputState indices for INP-04)
enum : int {
    BTN_UP = 0, BTN_DOWN = 1, BTN_LEFT = 2, BTN_RIGHT = 3,
    BTN_A = 4, BTN_B = 5, BTN_START = 6
};

// Provide the input_platform_poll definition (Phase 20 declared, Phase 21 defines)
namespace enjin2 {
void input_platform_poll(InputState* state) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    auto set_btn = [&](int btn, bool on) {
        if (on) state->buttons |= static_cast<uint16_t>(1u << btn);
    };
    set_btn(BTN_UP,    keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]);
    set_btn(BTN_DOWN,  keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]);
    set_btn(BTN_LEFT,  keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]);
    set_btn(BTN_RIGHT, keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]);
    set_btn(BTN_A,     keys[SDL_SCANCODE_Z]);
    set_btn(BTN_B,     keys[SDL_SCANCODE_X]);
    set_btn(BTN_START, keys[SDL_SCANCODE_RETURN]);
}
} // namespace enjin2

static void expand_canvas_to_rgb() {
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            enjin2::Pixel4 px = g_canvas.getPixel(x, y);
            int i = (y * CANVAS_W + x) * 3;
            if (!enjin2::g_palette.isTransparent(px.value)) {
                enjin2::RGB rgb = enjin2::g_palette.resolve(px.value);
                g_rgb_staging[i + 0] = rgb.r;
                g_rgb_staging[i + 1] = rgb.g;
                g_rgb_staging[i + 2] = rgb.b;
            } else {
                g_rgb_staging[i + 0] = 0;
                g_rgb_staging[i + 1] = 0;
                g_rgb_staging[i + 2] = 0;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // Parse --fps argument
    int fps = 30;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--fps") == 0) {
            int v = atoi(argv[i + 1]);
            if (v > 0 && v <= 300) fps = v;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const int WIN_W = CANVAS_W * DEFAULT_SCALE;
    const int WIN_H = CANVAS_H * DEFAULT_SCALE;

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Enjin2", WIN_W, WIN_H, 0, &window, &renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        CANVAS_W, CANVAS_H);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderScale(renderer,
        static_cast<float>(DEFAULT_SCALE),
        static_cast<float>(DEFAULT_SCALE));

    // Initialize canvas to blank with default palette (already initialized on construct)
    // g_canvas is zero-initialized (all pixels = 0 = palette index 0 = dark navy)

    const Uint64 frame_ms = 1000u / static_cast<Uint64>(fps);
    const float  max_dt   = 4.0f / static_cast<float>(fps);
    Uint64 last_ticks = SDL_GetTicks();
    bool   running    = true;

    while (running) {
        // 1. Drain events (also updates SDL keyboard state)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                running = false;
        }

        // 2. Delta time with 4-frame clamp
        Uint64 now = SDL_GetTicks();
        float dt = static_cast<float>(now - last_ticks) / 1000.0f;
        if (dt > max_dt) dt = max_dt;
        last_ticks = now;

        // 3. Input
        input_advance_frame(&g_input);
        enjin2::input_platform_poll(&g_input);

        // 4. Update (Phase 21: blank canvas — no game logic yet)
        (void)dt;

        // 5. Render
        expand_canvas_to_rgb();
        SDL_UpdateTexture(texture, nullptr, g_rgb_staging, CANVAS_W * 3);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // 6. Frame pacing
        Uint64 elapsed = SDL_GetTicks() - now;
        if (elapsed < frame_ms) {
            SDL_Delay(static_cast<Uint32>(frame_ms - elapsed));
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

### Verification Commands

```bash
# SDL-01: OFF build — no SDL3 symbols/headers in core libs
cmake -B build -DENJIN2_BUILD_SDL=OFF -DENJIN2_BUILD_LUA=OFF -DENJIN2_BUILD_TESTS=ON
cmake --build build
# Expect: no errors; enjin2_sdl target does not exist

# SDL-01: ON build — enjin2_sdl executable exists
cmake -B build_sdl -DENJIN2_BUILD_SDL=ON -DENJIN2_BUILD_LUA=OFF -DENJIN2_BUILD_TESTS=OFF
cmake --build build_sdl --target enjin2_sdl
# Expect: SDL3 downloads and builds; enjin2_sdl produced

# SDL-01: Verify no SDL3 symbols in core libraries
nm build/libenjin2_core.a 2>/dev/null | grep -i sdl && echo "FAIL: SDL in core" || echo "PASS: no SDL in core"
nm build/libenjin2_graphics.a 2>/dev/null | grep -i sdl && echo "FAIL: SDL in graphics" || echo "PASS"
nm build/libenjin2_input.a 2>/dev/null | grep -i sdl && echo "FAIL: SDL in input" || echo "PASS"
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SDL2 | SDL3 | Jan 2025 (3.2.0 stable) | SDL2 receives no new features; SDL3 is the current platform |
| `SDL_GetTicks64()` | `SDL_GetTicks()` returns `Uint64` | SDL3 | `SDL_GetTicks64` removed; `SDL_GetTicks` is now 64-bit |
| `SDL_RenderCopy` | `SDL_RenderTexture` | SDL3 | New name for blit-texture function |
| `SDL_QueryTexture` | Direct struct access or properties API | SDL3 | `SDL_QueryTexture` still exists in SDL3 but properties API preferred |
| `SDL_SetRenderLogicalPresentation` for integer scale | `SDL_SetRenderScale` | SDL3 3.x (ongoing bug) | Logical presentation path has scale mode bug; `SDL_SetRenderScale` is the workaround |
| Default bilinear scale in SDL3 | Must explicitly set `SDL_SCALEMODE_NEAREST` | SDL3 | SDL3 changed default — must opt-in to nearest |
| `SDL_SCALEMODE_NEAREST` only | `SDL_SCALEMODE_PIXELART` (3.4.0+) | SDL 3.4.0 | Enhanced pixel art scaling; usable with pinned 3.4.x tag |

**Deprecated/outdated:**
- `SDL_GetTicks64()`: Removed in SDL3; replaced by `SDL_GetTicks()` (now 64-bit).
- `SDL_RenderCopy()`: SDL2 name; SDL3 uses `SDL_RenderTexture()`.
- `SDL_QueryTexture()`: Exists but SDL3 prefers direct struct member access for `format`, `w`, `h`.

## Open Questions

1. **What canvas dimensions does Phase 19/20 establish as the "standard" Canvas4 size?**
   - What we know: `Canvas4_128x128` is a named alias in `canvas.hpp`. The CONTEXT.md example uses 128×128. At 4x scale = 512×512 window.
   - What's unclear: Phase 20's `input_platform_poll` test won't define button index constants — `sdl_main.cpp` must define its own local enum that maps to the indices used.
   - Recommendation: Use `Canvas4<128, 128>` as the hardcoded canvas type for Phase 21. If canvas dimensions become configurable later, that's Phase 22+.

2. **What are the exact button index numbers from Phase 20's implementation?**
   - What we know: Phase 20 CONTEXT.md says "Projects define their own semantic enum." Phase 21 CONTEXT.md maps: Up/Down/Left/Right/A(Z)/B(X)/Start(Enter) per INP-04.
   - What's unclear: Phase 20's plan shows the struct but no built-in enum constants. `sdl_main.cpp` must define its own local enum (indices 0-6) matching INP-04.
   - Recommendation: Define a local `enum` in `sdl_main.cpp` (not exported) mapping BTN_UP=0 through BTN_START=6. This is the INP-04 contract.

3. **Should `SDL_SCALEMODE_PIXELART` be used instead of `SDL_SCALEMODE_NEAREST`?**
   - What we know: `SDL_SCALEMODE_PIXELART` is available since SDL 3.4.0, and we're pinning to 3.4.2. It provides improved pixel art upscaling.
   - What's unclear: The difference between NEAREST and PIXELART at integer scales is minimal; PIXELART targets non-integer scales.
   - Recommendation: Use `SDL_SCALEMODE_NEAREST` for clarity and intent. Switch to `SDL_SCALEMODE_PIXELART` only if non-integer DPI scaling causes visual issues.

4. **How does the `--fps` flag interact with SDL3's `main()` signature?**
   - What we know: SDL3 on some platforms (Windows, iOS) redefines `main` via `SDL_main.h`. Including `<SDL3/SDL_main.h>` handles this.
   - Recommendation: Always `#include <SDL3/SDL_main.h>` and use `int main(int argc, char* argv[])`. This is safe on all platforms.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: `/home/unwn/dev/enjin/CMakeLists.txt` — existing CMake pattern, option guards, target structure
- Direct codebase inspection: `/home/unwn/dev/enjin/include/enjin2/graphics/canvas.hpp` — Canvas4 API, `getPixel`, `getBuffer`, template params, `Canvas4_128x128` alias
- Direct codebase inspection: `/home/unwn/dev/enjin/include/enjin2/graphics/palette.hpp` — `g_palette`, `resolve()`, `isTransparent()`, `RGB` struct
- Direct codebase inspection: `/home/unwn/dev/enjin/.planning/phases/20-input-abstraction/20-CONTEXT.md` — InputState layout, `input_platform_poll` contract
- Direct codebase inspection: `/home/unwn/dev/enjin/.planning/phases/20-input-abstraction/20-01-PLAN.md` — Phase 20 implementation plan confirms `input_platform_poll` undefined in core
- [SDL3 README-cmake wiki](https://wiki.libsdl.org/SDL3/README-cmake) — `SDL3::SDL3` target, `EXCLUDE_FROM_ALL` pattern
- [SDL3 SDL_UpdateTexture wiki](https://wiki.libsdl.org/SDL3/SDL_UpdateTexture) — function signature, pitch parameter
- [SDL3 BestKeyboardPractices wiki](https://wiki.libsdl.org/SDL3/BestKeyboardPractices) — `SDL_GetKeyboardState` for movement, drain events first
- [SDL3 SDL_ScaleMode wiki](https://wiki.libsdl.org/SDL3/SDL_ScaleMode) — `SDL_SCALEMODE_NEAREST`, default changed to bilinear in SDL3
- [SDL3 SDL_GetTicks wiki](https://wiki.libsdl.org/SDL3/SDL_GetTicks) — returns Uint64 in SDL3; SDL_GetTicks64 removed
- [SDL3 SDL_CreateWindow wiki](https://wiki.libsdl.org/SDL3/SDL_CreateWindow) — fixed window: pass `0` flags (no `SDL_WINDOW_RESIZABLE`)
- [SDL3 SDL_SetRenderScale wiki](https://wiki.libsdl.org/SDL3/SDL_SetRenderScale) — integer scale; available since SDL 3.2.0

### Secondary (MEDIUM confidence)
- [GitHub SDL releases page](https://github.com/libsdl-org/SDL/releases) — confirmed SDL 3.4.2 is latest stable as of 2026-02-21
- [SDL3 LogicalPresentation scale mode bug #11335](https://github.com/libsdl-org/SDL/issues/11335) — confirmed `SDL_SetRenderLogicalPresentation` ignores texture scale mode; `SDL_SetRenderScale` is the workaround
- [SDL_helloworld CMakeLists.txt](https://github.com/libsdl-org/SDL_helloworld/blob/main/CMakeLists.txt) — official FetchContent pattern (uses GIT_TAG "main"; we pin to release tag)

### Tertiary (LOW confidence)
- None — all critical claims verified against official SDL3 wiki or codebase.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — SDL3 APIs verified against official wiki; CMake pattern from official README-cmake
- Architecture: HIGH — derived from existing codebase patterns + SDL3 official docs
- Pitfalls: HIGH — confirmed bugs cited with GitHub issue numbers; timer API change verified against SDL3 wiki

**Research date:** 2026-02-24
**Valid until:** 2026-05-24 (SDL3 APIs are stable post-3.2.0; GIT_TAG should be re-checked at implementation time for latest patch)
