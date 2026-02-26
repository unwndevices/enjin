---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: Engine Capabilities
status: unknown
last_updated: "2026-02-26T11:48:11.526Z"
progress:
  total_phases: 9
  completed_phases: 8
  total_plans: 28
  completed_plans: 27
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-24)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** Phase 25 — Multi-Layer Canvas Composition

## Current Position

Phase: 25 of 26 (Multi-Layer Canvas Composition)
Plan: 3 of 3 in current phase — COMPLETE (Phase 25 done)
Status: Phase 25 complete (3/3 plans done); Phase 26 (LuaCallback fix) is next
Last activity: 2026-02-26 - Completed 25-03: Lua layer API bindings

Progress: [█████████░] 90% (Phase 25 complete; 1 phase remaining in v1.4)

## Performance Metrics

**Velocity:**
- Total plans completed: 47
- v1.0: 21 plans
- v1.1: 17 plans
- v1.2: 5 plans
- v1.3: 7 plans (19-01, 19-02, 20-01, 21-01, 21-02, 22-01, 22-02)
- v1.4: 6 plans completed (24-01, 24-02, 24-03, 25-01, 25-02, 25-03)

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
- [Phase 25-01]: Canvas4::BUFFER_SIZE is private — LayerCompositor uses getBufferSize() in composite() hot loop
- [Phase 25-01]: DrawLayer enum deleted entirely; uint8_t buffer_index is a direct layer slot number, not a sort key; sort_order also removed
- [Phase 25-01]: C_Drawable callers use SetBufferIndex(0/1/2/3); shouldDrawBefore() is a single buffer_index comparison
- [Phase 25-02]: LuaCanvas wrappers are static locals inside main() (no default constructor); default active canvas = layers[0]; clearAll/composite sandwich in frame loop
- [Phase 25]: setLayers() replaces setCanvas() in sdl_main.cpp — sets currentCanvas=layerCanvases[0] internally, making explicit setCanvas call redundant
- [Phase 25]: lua layer indices are 1-indexed in Lua; cpp_idx = lua_idx - 1; out-of-range silently clamped to [0, layerCount-1] matching setFrame pattern

### Pending Todos

None.

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 001 | Write simple design document of the library | 2026-02-03 | 24dd586 | [001-write-simple-design-document](./quick/001-write-simple-design-document-of-the-libr/) |
| 2 | Aseprite-to-enjin asset conversion tooling | 2026-02-26 | fb6c875 | [2-aseprite-to-enjin-asset-conversion-tooli](./quick/2-aseprite-to-enjin-asset-conversion-tooli/) |
| 3 | Aseprite Lua export plugin for enjin C header format | 2026-02-26 | 5a124e9 | [3-aseprite-lua-plugin-for-enjin-export](./quick/3-aseprite-lua-plugin-for-enjin-export/) |
| 4 | Update .gitignore with build artifact patterns | 2026-02-26 | 4c1ec72 | [4-update-gitignore](./quick/4-update-gitignore/) |

### Blockers/Concerns

- [Phase 26 prereq] LuaCallback dangling-pointer bug confirmed in lua_engine.cpp — fix as first step of Phase 26; all new bindings use lua_CFunction only
- [Phase 25 spec RESOLVED] Buffer access: getBufferSize() + getBuffer() on Canvas4 are sufficient for compositor (no getRawBuffer() extension needed)
- [Phase 25 spec] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2; ENJIN_LAYER_COUNT constexpr + static_assert already in place

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- `getPaletteRGB()` snapshot semantics — callers must re-invoke after palette mutation

## Session Continuity

Last session: 2026-02-26
Stopped at: Completed 25-03-PLAN.md — Lua layer API bindings (bindings.hpp/cpp, sdl_main.cpp, layer_demo.lua)
Resume file: None
