# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-24)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** Phase 24 — Sprite System Rework

## Current Position

Phase: 24 of 26 (Sprite System Rework)
Plan: 3 of 3 in current phase — COMPLETE
Status: Phase 24 complete (all 3 plans done); ready for Phase 25 (Compositor)
Last activity: 2026-02-26 - Completed quick task 2: Aseprite-to-enjin asset conversion tooling

Progress: [████████░░] 80% (22/26 phases through v1.3; 4 phases remaining in v1.4)

## Performance Metrics

**Velocity:**
- Total plans completed: 47
- v1.0: 21 plans
- v1.1: 17 plans
- v1.2: 5 plans
- v1.3: 7 plans (19-01, 19-02, 20-01, 21-01, 21-02, 22-01, 22-02)
- v1.4: 3 plans completed (24-01, 24-02, 24-03)

*Updated after each plan completion*

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.
Recent decisions affecting v1.4 work:

- [Phase 21]: SDL_SetRenderScale(4,4) workaround for SDL3 bug #11335 (logical presentation ignores SCALEMODE_NEAREST)
- [Phase 22]: All new Lua bindings use lua_CFunction exclusively — never LuaCallback (std::function)
- [Research]: Compositor replaces expand_canvas_to_rgb() wholesale — partial integration silently drops layers 1-3
- [Phase 23]: generate-api-docs.js — always use escapeForMdx(extractText(...)) for briefDesc/detailedDesc; namespace names must not appear in module.classes arrays (only in config.namespaces)
- [Phase 24-01]: SpriteSheet draw() inline in header (no .cpp); transparency index 15 is compile-time constant baked into draw(), no matte parameter; AnimMode defined before SpriteSheet for standalone use
- [Phase 24-03]: sprite.hpp uses canvas.hpp instead of icanvas.hpp to avoid ICanvas<TPixel> redefinition when compiled with Lua target
- [Phase 24-03]: lua_drawSprite blits via LuaCanvas::setPixel (type-erased path) rather than SpriteSheet::draw() directly
- [Phase 24-sprite-system-rework]: C_Drawable::draw() pure virtual changed from ICanvas<uint8_t>& to ICanvas<Pixel4>& — C_Canvas draw() is a stub (ENG-01 deferred to Phase 25 compositor)
- [Phase 24-sprite-system-rework]: C_Sprite holds SpriteSheet by value; delta-time accumulator pattern with carry-over; advanceFrame() handles Once/Loop/PingPong

### Pending Todos

None.

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 001 | Write simple design document of the library | 2026-02-03 | 24dd586 | [001-write-simple-design-document](./quick/001-write-simple-design-document-of-the-libr/) |
| 2 | Aseprite-to-enjin asset conversion tooling | 2026-02-26 | fb6c875 | [2-aseprite-to-enjin-asset-conversion-tooli](./quick/2-aseprite-to-enjin-asset-conversion-tooli/) |

### Blockers/Concerns

- [Phase 26 prereq] LuaCallback dangling-pointer bug confirmed in lua_engine.cpp — fix as first step of Phase 26; all new bindings use lua_CFunction only
- [Phase 25 spec] ICanvas<TPixel> buffer access decision (per-pixel virtual vs getRawBuffer() extension) must be resolved before writing compositor
- [Phase 25 spec] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2; add static_assert or startup check

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- `getPaletteRGB()` snapshot semantics — callers must re-invoke after palette mutation

## Session Continuity

Last session: 2026-02-26
Stopped at: Completed quick/2-PLAN.md — Aseprite-to-enjin asset conversion tooling (palette files + Python3 converter + README)
Resume file: None
