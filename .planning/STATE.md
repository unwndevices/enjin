---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: Lua Scripting Foundation
status: unknown
last_updated: "2026-02-27T02:13:04.665Z"
progress:
  total_phases: 12
  completed_phases: 7
  total_plans: 32
  completed_plans: 24
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.5 Lua Scripting Foundation — Phase 29 in progress

## Current Position

Phase: 29 of 35 (Named Objects + Tags)
Plan: 02 complete (2 of 2 plans for this phase) — Phase 29 COMPLETE
Status: Phase 29 complete — Scene proxy methods added; full Phase 29 named-objects API shipped
Last activity: 2026-02-27 — Phase 29-02 complete: Scene::findByName and Scene::findAllWithTag proxy methods added to scene.hpp

Progress: [████████████░░░░░░░░] 74% (26/35 phases complete — Phase 29 complete, Phase 30+ pending)

## Performance Metrics

**Velocity:**
- Total plans completed: 58
- v1.0: 21 plans (Phases 1-6)
- v1.1: 17 plans (Phases 7-15)
- v1.2: 5 plans (Phases 16-18)
- v1.3: 7 plans (Phases 19-22)
- v1.4: 8 plans (Phases 23-26)
- v1.5: 4 plans (Phase 28-01 — float dt migration; Phase 28-02 — -Woverride verification; Phase 29-01 — Object name/tag identity; Phase 29-02 — Scene proxy methods)

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
- [Phase 31 - engine.*] Must verify with module-level access test script before Phase 32 begins (guards against registration ordering pitfall)
- [Phase 25 spec] ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2

### Technical Debt (carried)

- Full Emscripten toolchain build not verified (code inspection conclusive)
- getPaletteRGB() snapshot semantics — callers must re-invoke after palette mutation

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed Phase 29-02 — Scene::findByName and findAllWithTag proxy methods; Phase 29 fully complete
Resume file: None
