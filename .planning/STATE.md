---
gsd_state_version: 1.0
milestone: v1.7
milestone_name: Tilemap + Camera
status: in-progress
last_updated: "2026-02-28T17:20:16Z"
progress:
  total_phases: 44
  completed_phases: 43
  total_plans: 26
  completed_plans: 25
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.7 Tilemap + Camera — Phase 43: Tilemap System (Plan 01 COMPLETE)

## Current Position

Phase: 43 of 44 (Tilemap System)
Plan: 01 COMPLETE
Status: Phase 43 Plan 01 complete — C_Tilemap C++ foundation (64x64 tile grid, viewport culling, coordinate helpers, TMAP-01..04 tests)
Last activity: 2026-02-28 — Phase 43 Plan 01 complete (C_Tilemap: stack-allocated tile grid, draw() viewport culling, setScroll, pixelToTile/tileToPixel/tileAtPixel)

Progress: [████████████████████] ~98% (43/44 phases complete — Plan 02 (Lua bindings) remaining)

## Performance Metrics

**Velocity:**
- Total plans completed: 81
- v1.0: 21 plans (Phases 1-6)
- v1.1: 17 plans (Phases 7-15)
- v1.2: 5 plans (Phases 16-18)
- v1.3: 7 plans (Phases 19-22)
- v1.4: 8 plans (Phases 23-26)
- v1.5: 21 plans (Phases 27-38)
- v1.6: 4 plans (Phase 39: ComponentProxy Plan 01, Phase 40: C_Timer Plan 01, Phase 41: C_StateMachine Plan 01, Phase 42: EventBus Plan 01)
- v1.7: 1 plan (Phase 43: Tilemap System Plan 01)

*Updated after each plan completion*

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.

Recent decisions relevant to v1.6:
- [Phase 37]: ObjectProxy valid flag pattern — ComponentProxy replicates this exactly
- [Phase 32]: ScriptProxy full userdata (not lightuserdata) — ComponentProxy must also use full userdata
- [Phase 38]: pointer-to-pointer registry pattern — EventBus injection follows same approach
- [Phase 39]: ComponentProxy placed in standalone header to avoid circular includes; mirrors ObjectProxy pattern
- [Phase 39]: self:get() 'get' key checked FIRST in ScriptProxy.__index before all other properties for collision prevention
- [Phase 40-c-timer]: fireCallback(cbRef) takes ref as param — one-shot timers set callbackRef=LUA_NOREF before pcall; clearTimers() sets m_L=nullptr to prevent double-unref
- [Phase 40-c-timer]: C_LuaScript destructor calls C_Timer::clearTimers() before shutdown() — handles component array destruction order safety (C_LuaScript at index 0 destructs before C_Timer at index 1)
- [Phase 41-c-statemachine]: C_StateMachine uses deferred transition model: setState() queues m_pendingState, applied at END of update() after state's update callback — matching SceneStateMachine pattern
- [Phase 41-c-statemachine]: clearStates() sets m_L=nullptr after unref — prevents double-unref in C_StateMachine destructor (mirrors C_Timer clearTimers sentinel pattern)
- [Phase 42-eventbus]: LuaEventBus uses fixed-capacity arrays (Channel[16], Subscriber[8]) with zero heap allocation — same sentinel pattern (m_L=nullptr after clearHandlers)
- [Phase 42-eventbus]: emit() snapshots refs to local array before pcall loop for re-entrant safety — off()/on() inside handler modifies channel array but not the snapshot
- [Phase 42-eventbus]: EVENT-05 hot-reload implemented in C_LuaScript::executeScript() not registerAll() — registerAll() only runs at initialize(); hot-reload goes through executeScript()
- [Phase 43-tilemap]: Stride = m_mapW (not MAX_MAP_W) — tile array logically m_mapW*m_mapH contiguous bytes; matches setTiles copy and Lua flat-table indexing
- [Phase 43-tilemap]: Tile ID 0 transparent sentinel with direct frameIndex pass-through — no subtract-1 in hot path; tileset frame 0 is intentionally wasted
- [Phase 43-tilemap]: Floor division helper for negative pixel/scroll coords — C++ truncates toward zero, floorDiv() corrects for negative world coordinates
- [Phase 43-tilemap]: setScroll() stores without clamping; draw() clamps startTX/startTY to 0 for negative scroll values

### Pending Todos

None.

### Blockers/Concerns

- [v1.6 OPEN] Single-proxy-per-component constraint: multiple scripts calling `self:get("C_Timer")` on same object silently overwrites proxy registration. Mitigate with dev-mode warning on `setLuaProxy()` overwrite; cache-in-init pattern documented.
- [v1.6 RESOLVED] ObjectCollection::update() loop snapshot: C_StateMachine implemented and tested cleanly — Object::update() iterates components in array order; C_LuaScript (index 0) fires before C_StateMachine (index 1) as required.
- [Phase 25 CARRIED] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2

### Roadmap Evolution

- Phases 39-42 added: v1.6 Game Ready milestone
- Phase 43 added: Tilemap system
- Phase 44 added: 2d Camera System

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- getPaletteRGB() snapshot semantics — callers must re-invoke after palette mutation
- hasComponent() const calls non-const getComponent<T>() — pre-existing design smell

## Session Continuity

Last session: 2026-02-28
Stopped at: Completed 43-01-PLAN.md — C_Tilemap C++ foundation (stack-allocated tile grid, viewport culling, coordinate helpers, TMAP-01..04 tests)
Resume file: None
