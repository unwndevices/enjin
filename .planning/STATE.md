---
gsd_state_version: 1.0
milestone: v1.7
milestone_name: Developer Experience & New Capability
status: unknown
last_updated: "2026-03-01T21:32:14.449Z"
progress:
  total_phases: 16
  completed_phases: 12
  total_plans: 34
  completed_plans: 33
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-01)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.7 Phase 50 — Tween Helpers

## Current Position

Phase: 50 of 52 (Tween Helpers)
Plan: 1 of 1 in current phase (complete)
Status: In Progress
Last activity: 2026-03-01 — Phase 50 Plan 01 complete: 8-slot TweenSlot pool with engine.tween.to/cancel/cancelAll; four inline easing functions (multiply/add only); tickTweens in SDL runner; clearTweens on hot-reload/scene-change

Progress: [######░░░░░░░░░░░░░░] 34% (phases 43-47 complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 91
- v1.0: 21 plans (Phases 1-6)
- v1.1: 17 plans (Phases 7-15)
- v1.2: 5 plans (Phases 16-18)
- v1.3: 7 plans (Phases 19-22)
- v1.4: 8 plans (Phases 23-26)
- v1.5: 21 plans (Phases 27-38)
- v1.6: 4 plans (Phases 39-42)
- v1.7 (in progress): 14 plans (Phases 43-49 complete, 50-01 complete)

*Updated after each plan completion*

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.

Key decisions affecting v1.7 phases 46-52:
- bindings_internal.hpp created (Phase 46-01): uses static constexpr for metatable constants — TU-local, ODR-safe, no companion .cpp needed
- bindings_internal.hpp must be created before any bindings file is extracted (pitfall: static linkage breakage)
- Coroutine scheduler resumes via lua_resume from C outside pcall scope (pitfall: yield-across-pcall boundary)
- engine.ui.* bypasses C++ Label/FillUpGauge entirely — stateless LuaCanvas draw calls only
- PersistentObjectRegistry owned by SceneStateMachine (not LuaBindings) — object ownership is C++ level
- [Phase 46]: LuaWrapper is header-only: delegates to LuaEngine + LuaBindings; engine declared before bindings (C++ member init order)
- [Phase 46]: overflow_test uses C_LuaScript fixture (not raw LuaEngine) because LuaEventBus::subscribe() requires m_L != nullptr
- [Phase 47]: Debug layer (index 4) excluded from g_lua_layers — accessible only via engine.debug.* not setLayer()
- [Phase 47]: REQUIRE_DEBUG_CANVAS macro provides zero-cost early-return guard when debug is disabled or canvas is null
- [Phase 48-01]: lua_engine_camera_follow/stopFollow implemented as LuaBindings member functions (not file-scope statics) to access private m_followTargetProxy/m_followSpeed — same pattern as Phase 47 debug bindings
- [Phase 48-01]: setActiveScene() must be called before setActiveCamera() (setActiveScene clears m_activeCamera on scene change)
- [Phase 48-01]: Scripts must store follow proxy in Lua global (not local) for it to survive GC between frames
- [Phase 48-camera-follow-save-load]: Platform detection uses !defined(ESP32) && !defined(__EMSCRIPTEN__) for bindings_store.cpp file I/O guard — VCV_RACK was an accidental enabler
- [Phase 48-camera-follow-save-load]: engine.store.flush() supplements auto-persist (does not clear the store); engine.store.path() is a void setter that auto-loads existing data
- [Phase 49-coroutine-async-scheduler]: Coroutine library placed unconditionally in ESP32 openEmbeddedLibraries() — lightweight, no heap allocation, required for engine.async.* correctness
- [Phase 49-01]: Float epsilon 0.001f used in tickCoroutines wait timer — 5*0.1f != 0.5f exactly in IEEE 754 float
- [Phase 49-01]: Post-yield dt subtraction: after lua_resume returns LUA_YIELD, subtract current frame dt so start tick counts toward wait duration
- [Phase 49-01]: clearSlot uses template<typename Slot> to access private CoroutineSlot from file-scope static in bindings_async.cpp
- [Phase 50-01]: TweenEasing private enum — cast to uint8_t at call site so file-scope tweenEase() in separate TU doesn't hit private-access error
- [Phase 50-01]: cancel leaves tween at current interpolated position; does NOT snap to end, does NOT fire done_cb
- [Phase 50-01]: tickTweens called after tickCoroutines in SDL runner; clearTweens called alongside clearCoroutines in both registerAll() and setActiveScene()

### Pending Todos

None.

### Blockers/Concerns

- [Phase 25 CARRIED] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2
- [Phase 49 RESEARCH FLAG] LuaJIT CoCo availability on WASM/ESP32 must be verified before writing coroutine resume path
- [Phase 51 RESEARCH FLAG] ObjectCollection::m_external[] update ordering requires design review before implementation

### Roadmap Evolution

- v1.0-v1.6: All milestones archived. See MILESTONES.md.
- Phases 43-45 complete as v1.7 Tilemap + Camera + Physics content
- Phases 46-52 roadmapped 2026-03-01 as v1.7 DX + new capability content

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- getPaletteRGB() snapshot semantics — callers must re-invoke after palette mutation
- hasComponent() const calls non-const getComponent<T>() — pre-existing design smell
- Single-proxy-per-component constraint (last-wins overwrite)
- EventBus m_L=nullptr window between scene change and script load

## Session Continuity

Last session: 2026-03-01
Stopped at: Completed 50-01-PLAN.md (tween helpers — engine.tween.* pool and SDL integration)
Resume file: None
