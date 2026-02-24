# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-24)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** Phase 23 — Docusaurus Navigation Fix

## Current Position

Phase: 23 of 26 (Docusaurus Navigation Fix)
Plan: 0 of TBD in current phase
Status: Ready to plan
Last activity: 2026-02-24 — v1.4 roadmap created; Phase 23 is next

Progress: [████████░░] 80% (22/26 phases through v1.3; 4 phases remaining in v1.4)

## Performance Metrics

**Velocity:**
- Total plans completed: 46
- v1.0: 21 plans
- v1.1: 17 plans
- v1.2: 5 plans
- v1.3: 7 plans (19-01, 19-02, 20-01, 21-01, 21-02, 22-01, 22-02)

*Updated after each plan completion*

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.
Recent decisions affecting v1.4 work:

- [Phase 21]: SDL_SetRenderScale(4,4) workaround for SDL3 bug #11335 (logical presentation ignores SCALEMODE_NEAREST)
- [Phase 22]: All new Lua bindings use lua_CFunction exclusively — never LuaCallback (std::function)
- [Research]: Compositor replaces expand_canvas_to_rgb() wholesale — partial integration silently drops layers 1-3

### Pending Todos

None.

### Blockers/Concerns

- [Phase 26 prereq] LuaCallback dangling-pointer bug confirmed in lua_engine.cpp — fix as first step of Phase 26; all new bindings use lua_CFunction only
- [Phase 25 spec] ICanvas<TPixel> buffer access decision (per-pixel virtual vs getRawBuffer() extension) must be resolved before writing compositor
- [Phase 25 spec] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2; add static_assert or startup check

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- `getPaletteRGB()` snapshot semantics — callers must re-invoke after palette mutation

## Session Continuity

Last session: 2026-02-24
Stopped at: v1.4 roadmap created; ready to plan Phase 23
Resume file: None
