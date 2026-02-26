---
gsd_state_version: 1.0
milestone: v1.5
milestone_name: Lua Scripting Foundation
status: active
last_updated: "2026-02-26"
progress:
  total_phases: 35
  completed_phases: 26
  total_plans: 58
  completed_plans: 58
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation
**Current focus:** v1.5 Lua Scripting Foundation — Phase 27 ready to plan

## Current Position

Phase: 27 of 35 (Fix onRender Pixel4 Bug)
Plan: — (not started)
Status: Ready to plan
Last activity: 2026-02-26 — v1.5 roadmap created (Phases 27-35)

Progress: [████████████░░░░░░░░] 74% (26/35 phases complete)

## Performance Metrics

**Velocity:**
- Total plans completed: 58
- v1.0: 21 plans (Phases 1-6)
- v1.1: 17 plans (Phases 7-15)
- v1.2: 5 plans (Phases 16-18)
- v1.3: 7 plans (Phases 19-22)
- v1.4: 8 plans (Phases 23-26)

*Updated after each plan completion*

## Accumulated Context

### Decisions

All decisions logged in PROJECT.md Key Decisions table.

Recent decisions relevant to v1.5:
- [Phase 26]: assertRequires<T>() naming chosen over requires<T>() to avoid C++20 keyword collision
- [Phase 26]: ScriptProxy uses full userdata (not lightuserdata) — lightuserdata has no metatable in Lua 5.1
- [Phase 26]: float dt uses float not double — ESP32-S3 has hardware single-precision FPU; double is soft-float

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

Last session: 2026-02-26
Stopped at: v1.5 roadmap created — Phase 27 ready to plan
Resume file: None
