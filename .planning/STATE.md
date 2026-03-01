---
gsd_state_version: 1.0
milestone: v1.6
milestone_name: Game Ready
status: unknown
last_updated: "2026-03-01T12:55:24.293Z"
progress:
  total_phases: 13
  completed_phases: 12
  total_plans: 31
  completed_plans: 30
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-28)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.7 Tilemap + Camera — Phase 44: 2D Camera System COMPLETE

## Current Position

Phase: 45 of 45 (Optimized 2D Physics Engine) — COMPLETE
Plan: 02 COMPLETE — engine.physics.* Lua bindings (10 functions), global gravity state, DDA raycast, 22/22 tests pass
Status: Phase 45 COMPLETE — Plan 01: C++ helpers (physics.hpp, 28/28 tests), Plan 02: Lua bindings (engine.physics.*, 22/22 tests)
Last activity: 2026-03-01 - Completed Phase 45 Plan 02: engine.physics.* Lua bindings + DDA raycast + 22/22 integration tests

Progress: [████████████████████] 100% (44 phases complete)

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
- v1.7: 4 plans (Phase 43: Tilemap System Plan 01+02, Phase 44: 2D Camera Plan 01+02)

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
- [Phase 43-tilemap-lua]: getBindings() made public — static non-member Lua proxy functions in bindings.cpp cannot access private members; public is safe (reads only Lua registry)
- [Phase 43-tilemap-lua]: getSpriteSheet() public const accessor — returns pointer to sprite pool slot for C_Tilemap::setSheet() integration without exposing mutable sprite pool
- [Phase 43-tilemap-lua]: CTILEMAP_PROXY_CHECK macro — reduces stale-check boilerplate across 10 C_Tilemap_Proxy methods; consistent with C_Timer/C_StateMachine proxy pattern
- [Phase 44-camera]: Camera pos = top-left world corner; screenOffset = -(pos + shakeOffset); screen_pos = world_pos - cam_pos; matches tilemap scroll semantics
- [Phase 44-camera]: shake elapsed incremented before sin computation — avoids sin(0)=0 zero-offset bug on first frame
- [Phase 44-camera]: drawWithOffset saves/restores anchor_offset; screen-space flag (m_screenSpace) on C_Drawable for HUD/UI opt-out
- [Phase 44-camera]: camera.cpp in enjin2_lua STATIC target — Plan 02 Lua bindings link against enjin2_lua
- [Phase 44-camera-lua]: engine.camera.* uses getBindings(L)->getActiveCamera() — follows existing input pointer pattern (not registry pointer-to-pointer); host calls bindings.setActiveCamera(cam)
- [Phase 44-camera-lua]: C_Tilemap::drawWithOffset saves/restores m_scrollX/m_scrollY, subtracts camera offset (negative) to produce additive scroll
- [Phase 44-camera-lua]: m_activeCamera cleared in setActiveScene() on scene change — same lifecycle as m_eventBus.clearHandlers()
- [Phase 45]: bindings_physics.cpp uses forEach lambda for C_Tilemap scan to avoid hasComponent() const/non-const chain issue
- [Phase 45-02]: applyGravity uses lua_gettop to disambiguate 3-arg (global) vs 5-arg (override) gravity — explicit overload selection without Lua function name collision
- [Phase 45-02]: raycast: DDA tilemap scan (Stage 1) always runs before linear object scan (Stage 2) — tilemap hits take priority; 8px fixed hit radius for objects

### Pending Todos

None.

### Quick Tasks Completed

| # | Description | Date | Commit | Status | Directory |
|---|-------------|------|--------|--------|-----------|
| 007 | Implement 7 scripting API improvements: built-in constants, configurable resolution, engine.graphics namespace, text scale parameter, float-to-int coordinate casting, game-state manager, and text centering helpers | 2026-02-28 | 993d89f | Verified | [007-implement-7-scripting-api-improvements-b](./quick/007-implement-7-scripting-api-improvements-b/) |
| Phase 45 P02 | 10 | 2 tasks | 6 files |

### Blockers/Concerns

- [v1.6 OPEN] Single-proxy-per-component constraint: multiple scripts calling `self:get("C_Timer")` on same object silently overwrites proxy registration. Mitigate with dev-mode warning on `setLuaProxy()` overwrite; cache-in-init pattern documented.
- [v1.6 RESOLVED] ObjectCollection::update() loop snapshot: C_StateMachine implemented and tested cleanly — Object::update() iterates components in array order; C_LuaScript (index 0) fires before C_StateMachine (index 1) as required.
- [Phase 25 CARRIED] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2

### Roadmap Evolution

- Phases 39-42 added: v1.6 Game Ready milestone
- Phase 43 added: Tilemap system
- Phase 44 added: 2d Camera System
- Phase 45 added: Optimized 2D physics engine

### Phase 45 Decisions

- [Phase 45-01]: physics.hpp fully header-only inline — matches collision.hpp pattern; no .cpp needed
- [Phase 45-01]: TrigLUT constexpr sin_table[256] in math.hpp; getSineValue() moved inline; math.cpp stub eliminated
- [Phase 45-01]: cos(x) = sin(x+64) via quarter-turn phase offset — single 256-entry table covers both sin and cos
- [Phase 45-01]: applyDrag factor clamped to [0,1] — prevents velocity sign flip on overdrag (large_drag * large_dt)
- [Phase 45-01]: attract() uses distSq += 1e-4f epsilon guard — prevents NaN on coincident points without branching
- [Phase 45-02]: forEach lambda scan for C_Tilemap (not findObjectWithComponent) — avoids hasComponent() const/non-const chain compile error
- [Phase 45-02]: applyGravity uses lua_gettop to disambiguate 3-arg (global) vs 5-arg (override) — no Lua function name collision
- [Phase 45-02]: raycast DDA (Stage 1) before linear object scan (Stage 2) — tilemap hits take priority; 8px fixed radius for objects

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- getPaletteRGB() snapshot semantics — callers must re-invoke after palette mutation
- hasComponent() const calls non-const getComponent<T>() — pre-existing design smell

## Session Continuity

Last session: 2026-03-01
Stopped at: Completed 45-02-PLAN.md — engine.physics.* Lua bindings (10 functions), global gravity state, DDA raycast, 22/22 Lua integration tests pass
Resume file: None
