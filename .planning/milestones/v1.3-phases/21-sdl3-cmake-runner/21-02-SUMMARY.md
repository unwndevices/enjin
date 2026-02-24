---
phase: 21-sdl3-cmake-runner
plan: 02
subsystem: platform
tags: [sdl3, canvas, input, game-loop, platform-runner]

# Dependency graph
requires:
  - phase: 21-01-sdl3-cmake-runner
    provides: ENJIN2_BUILD_SDL CMake option, FetchContent SDL3 3.4.2, enjin2_sdl executable target
  - phase: 20-01-input-abstraction
    provides: InputState, input_advance_frame, input_platform_poll declaration
  - phase: 19-01-palette
    provides: Palette, g_palette, RGB resolution, isTransparent
  - phase: 18-canvas
    provides: Canvas4 template, getPixel
provides:
  - src/platform/sdl/sdl_main.cpp — full SDL3 runner with Canvas4->RGB24 blit, 4x nearest-neighbor scaling, fixed-rate game loop, keyboard input_platform_poll
  - input_platform_poll definition for SDL3 platform (arrows+WASD directional, Z=A, X=B, Enter=Start, Escape=quit)
  - enjin2_sdl binary: 512x512 window, 30fps default, --fps N override, clean SDL_Quit shutdown
affects: [22-wasm-runner, future-platform-ports]

# Tech tracking
tech-stack:
  added: [SDL3 3.4.2 (FetchContent, EXCLUDE_FROM_ALL)]
  patterns:
    - SDL3 streaming texture with SDL_TEXTUREACCESS_STREAMING + SDL_UpdateTexture for Canvas4->RGB24 each frame
    - SDL_SetRenderScale instead of SDL_SetRenderLogicalPresentation (workaround SDL3 bug #11335)
    - SDL_GetKeyboardState for held-key polling (not SDL_EVENT_KEY_DOWN which has OS repeat delay)
    - input_advance_frame before input_platform_poll — advance clears current state, poll writes new state
    - SDL headers contained entirely within sdl_main.cpp — zero SDL includes in core library headers

key-files:
  created:
    - src/platform/sdl/sdl_main.cpp
  modified: []

key-decisions:
  - "SDL_SetRenderScale(renderer, 4, 4) used instead of SDL_SetRenderLogicalPresentation due to confirmed SDL3 bug #11335 that ignores scale mode in logical presentation"
  - "SDL_GetKeyboardState used for all button polling (not event-driven) to detect held keys without OS key-repeat delay"
  - "input_advance_frame called before input_platform_poll each frame — ensures current-frame snapshot is cleared before new state is written"
  - "SDL3 headers confined to sdl_main.cpp only — no SDL includes may cross into core library headers (enjin2_core, enjin2_input)"

patterns-established:
  - "Platform runner pattern: single translation unit contains all platform-specific code including input_platform_poll definition"
  - "Canvas expand pattern: 4-bit indexed pixels expanded to RGB24 via palette lookup in a tight loop before SDL_UpdateTexture"
  - "Frame pacing pattern: SDL_GetTicks delta + 4-frame ceiling clamp + SDL_Delay remainder for fixed-rate loop"

requirements-completed: [SDL-02, SDL-03, SDL-04, INP-04]

# Metrics
duration: ~15min
completed: 2026-02-24
---

# Phase 21 Plan 02: SDL3 Runner Summary

**SDL3 runner in ~175-line sdl_main.cpp: Canvas4->RGB24 texture blit, 4x nearest-neighbor integer scaling, 30fps fixed game loop with delta-time clamping, and full keyboard-to-InputState mapping via input_platform_poll**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-02-24T14:20:00Z
- **Completed:** 2026-02-24T14:37:17Z
- **Tasks:** 2 (1 auto + 1 human-verify checkpoint)
- **Files modified:** 1

## Accomplishments

- Implemented `src/platform/sdl/sdl_main.cpp` as the sole SDL3 platform translation unit (~175 lines)
- Canvas4 pixels expanded to RGB24 via palette lookup and uploaded via SDL_UpdateTexture each frame (SDL-02)
- SDL_SetTextureScaleMode(NEAREST) + SDL_SetRenderScale(4,4) for pixel-perfect integer 4x scaling (SDL-03)
- Fixed-rate game loop at 30fps default, --fps N override, 4-frame delta-time ceiling, clean SDL_Quit shutdown (SDL-04)
- input_platform_poll provides SDL3 keyboard -> InputState mapping: arrows+WASD=directional, Z=A, X=B, Enter=Start, Escape=quit-only (INP-04)
- Human verified: 512x512 window opens, blank canvas displays without blur or garbling, all keys handled without crash, Escape closes cleanly

## Task Commits

Each task was committed atomically:

1. **Task 1: Create src/platform/sdl/sdl_main.cpp** - `22ae2c3` (feat)
2. **Task 2: Human verification checkpoint** - (no commit — verification only)

**Plan metadata:** (docs commit — this summary)

## Files Created/Modified

- `src/platform/sdl/sdl_main.cpp` - SDL3 runner: window creation, RGB24 streaming texture, Canvas4->RGB expand, game loop, input_platform_poll definition, clean shutdown

## Decisions Made

- SDL_SetRenderScale used instead of SDL_SetRenderLogicalPresentation due to confirmed SDL3 bug #11335 (logical presentation ignores SDL_SCALEMODE_NEAREST). This is the correct workaround documented in SDL3 issue tracker.
- SDL_GetKeyboardState used for all held-key detection rather than SDL_EVENT_KEY_DOWN to avoid the ~1 second OS key-repeat delay on held keys.
- input_advance_frame called before input_platform_poll each frame to ensure the previous frame's "current" state is snapshotted as "previous" before new input is written — reversing this order would cause all inputs to appear as justReleased every frame.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required. SDL3 is fetched automatically via CMake FetchContent when ENJIN2_BUILD_SDL=ON.

## Next Phase Readiness

- Phase 21 complete: ENJIN2_BUILD_SDL CMake option, SDL3 FetchContent, enjin2_sdl binary, all SDL/input requirements satisfied
- Phase 22 (WASM runner) can proceed: input_platform_poll contract established, Canvas4 expand pattern demonstrated, core library SDL-isolation confirmed
- Requirements SDL-02, SDL-03, SDL-04, INP-04 all satisfied

## Self-Check: PASSED

- FOUND: src/platform/sdl/sdl_main.cpp
- FOUND: .planning/phases/21-sdl3-cmake-runner/21-02-SUMMARY.md
- FOUND: commit 22ae2c3 (feat(21-02): SDL3 runner)

---
*Phase: 21-sdl3-cmake-runner*
*Completed: 2026-02-24*
