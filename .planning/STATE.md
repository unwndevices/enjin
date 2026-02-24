# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-23)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** Phase 22 — Lua Integration + E2E Validation (v1.3 Tomodachi Readiness)

## Current Position

Phase: 22 of 22 (Lua Integration + E2E Validation)
Plan: 1 of 1 in current phase (complete)
Status: In Progress
Last activity: 2026-02-24 — Phase 22 Plan 01 complete (LuaBindings input polling API: isButtonHeld/isButtonJustPressed/isButtonJustReleased/getAxis with null guards; scripts/e2e_parity.lua with 5x3 palette grid and input indicators)

Progress: [████████████████░░░░] ~91% (20/22 phases complete across all milestones)

## Performance Metrics

**Velocity:**
- Total plans completed: 46
- v1.0: 21 plans
- v1.1: 17 plans
- v1.2: 5 plans
- v1.3: 6 plans (19-01, 19-02, 20-01, 21-01, 21-02, 22-01)

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- v1.3 start: Index 15 = transparent, indices 0-14 are user colors — preserves Colors::BLACK = Pixel4(0)
- v1.3 start: SDL3 (not SDL2) — canonical names: ENJIN2_PLATFORM_SDL, ENJIN2_BUILD_SDL, enjin2_sdl
- v1.3 start: WASM palette — expose raw palette via getPaletteRGB(), apply in JavaScript (thin C++ binding)
- 19-01: Transparency-before-modulo — isTransparent(15) checked BEFORE index % size in all palette methods to prevent index 15 folding into smaller presets
- 19-01: parseHexColor uses %02x (unsigned) which the C standard specifies accepts both upper and lower hex digits
- 19-01: debugTransparent stored on Palette struct for future runtime magenta debug mode without API change
- 19-02: WASM palette bindings placed outside ENJIN2_BUILD_LUA guard — palette is core graphics, not Lua-only
- 19-02: getPaletteRGB uses static buffer + typed_memory_view for live zero-copy view to JavaScript (not a copy)
- 19-02: lua_setPaletteColor uses lua_isstring dispatch for hex vs RGB integer overloads
- 20-01: input_platform_poll declared in header but NOT defined in core — each platform (SDL3, ESP32, WASM) provides exactly one definition
- 20-01: input_state.hpp includes only <stdint.h> — zero SDL3, Emscripten, Arduino, or ESP32 headers cross the abstraction boundary
- 20-01: InputState uses uint16_t bitmask (max 16 buttons) and float axes[8] per INP-02 spec
- 20-01: input_advance_frame uses memcpy/memset for frame snapshot — simple and consistent with zero-allocation constraint
- [Phase 21-sdl3-cmake-runner]: ENJIN2_BUILD_SDL defaults OFF with FetchContent SDL3 release-3.4.2, EXCLUDE_FROM_ALL, enjin2_lua excluded from enjin2_sdl link
- 21-02: SDL_SetRenderScale(4,4) used instead of SDL_SetRenderLogicalPresentation — workaround for SDL3 bug #11335 (logical presentation ignores SCALEMODE_NEAREST)
- 21-02: SDL_GetKeyboardState used for held-key polling; Escape handled via SDL_EVENT_KEY_DOWN for quit-only, never mapped to InputState
- 21-02: input_advance_frame called BEFORE input_platform_poll each frame — advance clears current, poll writes new state
- 22-01: InputState* currentInput initialized to nullptr — null guard in all 4 input bindings returns 0/false before host calls setInput()
- 22-01: lua_getAxis bounds-checks axis index (0-7) before dereferencing axes[] array

### Pending Todos

None.

### Blockers/Concerns

None.

### Technical Debt

- API navigation disabled in Docusaurus due to MDX syntax issues (carried from v1.0)
- parameterlist name/description concatenation in 5 API docs (Doxygen XML limitation)
- Full Emscripten toolchain build not verified (code inspection conclusive)

## Session Continuity

Last session: 2026-02-24
Stopped at: Completed 22-01-PLAN.md — LuaBindings input polling API (4 functions), scripts/e2e_parity.lua. Phase 22 Plan 01 complete.
Resume file: None
