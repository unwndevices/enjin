---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: Lua Scripting Foundation
status: unknown
last_updated: "2026-02-27T02:25:30Z"
progress:
  total_phases: 12
  completed_phases: 8
  total_plans: 32
  completed_plans: 25
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.5 Lua Scripting Foundation — Phase 31 in progress

## Current Position

Phase: 31 of 35 (engine.* Global Table)
Plan: 01 complete (1 of 3 plans for this phase) — Phase 31 Plan 01 COMPLETE
Status: Phase 31-01 complete — engine.* global Lua table wired with sub-tables and 10 stub C functions
Last activity: 2026-02-27 — Phase 31-01 complete: EngineTimeState struct + registerEngineTable() + engine.scene/input/time/lua/log registered; ENG-06 met

Progress: [████████████░░░░░░░░] 74% (26/35 phases complete — Phase 31-01 complete, Phase 31 plans 02-03 pending)

## Performance Metrics

**Velocity:**
- Total plans completed: 58
- v1.0: 21 plans (Phases 1-6)
- v1.1: 17 plans (Phases 7-15)
- v1.2: 5 plans (Phases 16-18)
- v1.3: 7 plans (Phases 19-22)
- v1.4: 8 plans (Phases 23-26)
- v1.5: 6 plans (Phase 28-01 — float dt migration; Phase 28-02 — -Woverride verification; Phase 29-01 — Object name/tag identity; Phase 29-02 — Scene proxy methods; Phase 30-01 — SSM back-pointer + deferred self-transition; Phase 31-01 — engine.* global table wiring)

*Updated after each plan completion*

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.

Recent decisions relevant to v1.5:
- [Phase 26]: assertRequires<T>() naming chosen over requires<T>() to avoid C++20 keyword collision
- [Phase 26]: ScriptProxy uses full userdata (not lightuserdata) — lightuserdata has no metatable in Lua 5.1
- [Phase 26]: float dt uses float not double — ESP32-S3 has hardware single-precision FPU; double is soft-float
- [Phase 28]: float seconds dt replaces uint16_t milliseconds throughout entire update chain
- [Phase 28]: Lua updateSprite API now expects dt in seconds (accumSec replaces accumMs)
- [Phase 28]: PostFx uses noisePeriodAccum sub-accumulator instead of integer modulo on float time
- [Phase 28-02]: -Woverride is Clang-specific; applied via $<CXX_COMPILER_ID:Clang,AppleClang> generator expression — GCC enforces override correctness as a hard compiler error natively
- [Phase 29-01]: Tests using Object directly without C_Drawable symbols need --start-group/--end-group to resolve typeinfo for C_Drawable between enjin2_core.a and enjin2_ui.a
- [Phase 29-01]: Zero-heap-allocation name/tag identity stores raw const char* pointers; caller owns string lifetime
- [Phase 29-02]: Scene::findByName and findAllWithTag are one-liner proxies to ObjectCollection — placed after findObjectWithComponent<T>() for consistent find* API grouping
- [Phase 30-01]: hasPendingTransition cleared BEFORE applyDeferredTransition() — switchTo() from onDeactivate() queues for next frame, not re-enters current dispatch
- [Phase 30-01]: Forward declaration only in scene.hpp for SceneStateMachine — no reverse #include to avoid circular include
- [Phase 30-01]: m_ssm is protected (not private) — matches m_ prefix convention; enables subclasses to expose via getSSM()
- [Phase 30-01]: switchTo() last-wins semantics — multiple calls per frame overwrite pendingSceneId (no allocation, simplest correct policy)
- [Phase 31-01]: setters (setSceneStateMachine/setActiveScene/setTimeState) are public so host code can inject without friend declarations
- [Phase 31-01]: Lua registry stores C++ pointers at registerAll() time — not at call time — so Plan 02 replacements need no re-registration
- [Phase 31-01]: engine.lua registered as empty table at Plan 01 stage; Phase 35 adds gc/memory functions
- [Phase 31-01]: Forward declarations only in bindings.hpp; scene.hpp/scene_state_machine.hpp included in bindings.cpp to avoid circular includes

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

- [Phase 32 - ScriptProxy] Decide validity mechanism (generation token vs valid flag) before writing any proxy code — cannot retrofit safely
- [Phase 31-01 RESOLVED] engine.* global table now registered; module-level access no longer a concern for Phase 31-02 onward
- [Phase 25 spec] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- getPaletteRGB() snapshot semantics — callers must re-invoke after palette mutation

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed Phase 31-01 — engine.* global Lua table wiring with stubs (ENG-06)
Resume file: None
