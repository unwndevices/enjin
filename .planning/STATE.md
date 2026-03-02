---
gsd_state_version: 1.0
milestone: v1.7
milestone_name: Developer Experience & New Capability
status: unknown
last_updated: "2026-03-02T00:37:38.602Z"
progress:
  total_phases: 16
  completed_phases: 14
  total_plans: 40
  completed_plans: 38
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-01)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.7 Phase 52 — UI Component Bindings

## Current Position

Phase: 52 of 52 (UI Component Bindings)
Plan: 2 of 2 in current phase (complete)
Status: In Progress
Last activity: 2026-03-02 — Phase 52 Plan 02 complete: UI-COMPONENT-GUIDE.md internal developer guide (355 lines) covering stateless canvas-call pattern, wiring checklist, hot-reload contract, color model, anti-patterns, and test fixture reference

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
- v1.7 (in progress): 19 plans (Phases 43-49 complete, 50-01, 50-02, 51-01, 51-02, 52-01, 52-02 complete)

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
- [Phase 50]: hot-reload safety verified by pool exhaustion after registerAll() — all 8 tween slots must be allocatable again to confirm clearTweens fired correctly
- [Phase 51-persistent-objects]: persistObject() immediately re-injects extracted object as external into current scene so object remains live during the scene it was persisted in
- [Phase 51-persistent-objects]: PersistentObjectRegistry is nested public struct so tests can instantiate it directly without SSM overhead
- [Phase 51-persistent-objects]: flushPendingRemovals fires at START of applyDeferredTransition before clearExternal so destroyed objects are never re-injected
- [Phase 51-persistent-objects]: persist() binding returns nil on overflow consistent with coroutine/tween pool pattern
- [Phase 51-persistent-objects]: find() fallback uses getBindings(L)->m_ssm; active scene priority maintained (local shadows persistent)
- [Phase 52]: Guide placed in phase directory; full C++ code examples included for direct copy-paste; quick-reference file map at end for orientation
- [Phase 52-01]: REQUIRE_CANVAS(b, L) macro used (not REQUIRE_DEBUG_CANVAS) — no enabled toggle; uses currentCanvas not m_debugCanvas
- [Phase 52-01]: luaL_checknumber for value/current/max params (float); luaL_checkinteger silently truncates 0.5 to 0
- [Phase 52-01]: statBar division-by-zero: (max > 0.0f) ? (current/max) : 0.0f
- [Phase 52-01]: fillW overflow guard: if (fillW > w) fillW = w after uint16_t cast

### Pending Todos

None.

### Blockers/Concerns

- [Phase 25 CARRIED] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2
- [Phase 49 RESEARCH FLAG] LuaJIT CoCo availability on WASM/ESP32 must be verified before writing coroutine resume path
- [Phase 51 RESEARCH FLAG RESOLVED] ObjectCollection::m_external[] update ordering: externals iterate AFTER owned objects in all methods — design resolved and implemented in 51-01

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

Last session: 2026-03-02
Stopped at: Completed 52-01-PLAN.md (engine.ui.* Lua sub-table — progressBar/statBar/panel/label, 7-case test suite, 43/43 tests pass)
Resume file: None
