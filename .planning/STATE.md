# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-23)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** Phase 21 — SDL3 CMake + Runner (v1.3 Tomodachi Readiness)

## Current Position

Phase: 21 of 22 (SDL3 CMake + Runner)
Plan: 1 of 2 in current phase (complete)
Status: In Progress
Last activity: 2026-02-24 — Phase 21 Plan 01 complete (ENJIN2_BUILD_SDL CMake option, FetchContent SDL3 release-3.4.2, enjin2_sdl executable target)

Progress: [████████████░░░░░░░░] ~82% (18/22 phases complete across all milestones)

## Performance Metrics

**Velocity:**
- Total plans completed: 44
- v1.0: 21 plans
- v1.1: 17 plans
- v1.2: 5 plans
- v1.3: 4 plans (19-01, 19-02, 20-01, 21-01)

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
Stopped at: Completed 21-01-PLAN.md — ENJIN2_BUILD_SDL CMake option, FetchContent SDL3 release-3.4.2, enjin2_sdl executable target
Resume file: None
