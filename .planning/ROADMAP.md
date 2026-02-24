# Roadmap: Enjin Migration

## Overview

Complete migration from enjin to enjin2 with full independence, validation, and comprehensive documentation. enjin2 is now a self-contained library with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure.

## Milestones

- ✅ **v1.0 Migration + Documentation** — Phases 1-6 (shipped 2026-02-01)
- ✅ **v1.1 Project Infrastructure & Documentation Enhancement** — Phases 7-15 (shipped 2026-02-23)
- ✅ **v1.2 Tech Debt Cleanup** — Phases 16-18 (shipped 2026-02-23)
- 🚧 **v1.3 Tomodachi Readiness** — Phases 19-22 (in progress)

## Phases

<details>
<summary>✅ v1.0 Migration + Documentation (Phases 1-6) — SHIPPED 2026-02-01</summary>

**See full details:** [.planning/milestones/v1.0-ROADMAP.md](.planning/milestones/v1.0-ROADMAP.md)

- [x] Phase 1: Dependency Analysis (3/3 plans) — completed 2026-01-30
- [x] Phase 2: Core Migration (3/3 plans) — completed 2026-01-30
- [x] Phase 3: Feature Support (3/3 plans) — completed 2026-01-30
- [x] Phase 4: Validation (4/4 plans) — completed 2026-01-31
- [x] Phase 5: Final Cleanup (1/1 plan) — completed 2026-01-31
- [x] Phase 6: Create library docs, using doxygen + Docusaurus (7/7 plans) — completed 2026-02-01

**Total:** 6 phases, 21 plans, all complete
</details>

<details>
<summary>✅ v1.1 Project Infrastructure & Documentation Enhancement (Phases 7-15) — SHIPPED 2026-02-23</summary>

**See full details:** [.planning/milestones/v1.1-ROADMAP.md](.planning/milestones/v1.1-ROADMAP.md)

- [x] Phase 7: README Enhancement (1/1 plan) — completed 2026-02-02
- [x] Phase 8: Build System Fixes (2/2 plans) — completed 2026-02-03
- [x] Phase 9: Documentation Coverage (5/5 plans) — completed 2026-02-03
- [x] Phase 10: Module Overview Generation (2/2 plans) — completed 2026-02-03
- [x] Phase 11: Documentation Tracking Improvements (1/1 plan) — completed 2026-02-23
- [x] Phase 12: Fix Doxygen Warning Regression (2/2 plans) — completed 2026-02-23
- [x] Phase 13: Fix Documentation Pipeline & API Landing (2/2 plans) — completed 2026-02-23
- [x] Phase 14: Fix extractText() Cross-References (1/1 plan) — completed 2026-02-23
- [x] Phase 15: Cleanup CI and README Tech Debt (1/1 plan) — completed 2026-02-23

**Total:** 9 phases, 17 plans, all complete
</details>

<details>
<summary>✅ v1.2 Tech Debt Cleanup (Phases 16-18) — SHIPPED 2026-02-23</summary>

**See full details:** [.planning/milestones/v1.2-ROADMAP.md](.planning/milestones/v1.2-ROADMAP.md)

- [x] Phase 16: Repository Cleanup (2/2 plans) — completed 2026-02-23
- [x] Phase 17: Documentation Generation Fix (2/2 plans) — completed 2026-02-23
- [x] Phase 18: Build System Fix (1/1 plan) — completed 2026-02-23

**Total:** 3 phases, 5 plans, all complete
</details>

### 🚧 v1.3 Tomodachi Readiness (In Progress)

**Milestone Goal:** Make enjin2 ready for Tomodachi integration — 16-color indexed palette, SDL3 desktop runner, and platform-agnostic input abstraction.

- [x] **Phase 19: Palette Foundation** - 16-color palette struct, runtime swap, WASM and Lua bindings (completed 2026-02-24)
- [x] **Phase 20: Input Abstraction** - Platform-agnostic interface, unified InputState, edge detection (completed 2026-02-24)
- [x] **Phase 21: SDL3 CMake + Runner** - SDL3 build option, window/game loop, Canvas4-to-RGB blit, keyboard mapping (completed 2026-02-24)
- [ ] **Phase 22: Lua Integration + E2E Validation** - Lua input polling API, cross-platform parity sign-off

## Phase Details

### Phase 19: Palette Foundation
**Goal**: Canvas4 pixels map to RGB colors at display time via a swappable 16-entry palette, with index 15 as transparent, accessible from Lua and WASM
**Depends on**: Phase 18 (clean build system)
**Requirements**: PAL-01, PAL-02, PAL-03, PAL-04, PAL-05
**Success Criteria** (what must be TRUE):
  1. A Lua script calling setPalette(0, 255, 0, 0) makes all pixels drawn with color index 0 render red — without redrawing the canvas
  2. Pixels drawn at index 15 are skipped (transparent) at the blit step on all platforms
  3. The WASM JavaScript renderer reads palette colors via getPaletteRGB() and applies them during canvas-to-screen compositing
  4. The default palette provides 15 distinct colors (indices 0-14) visible on screen before any script call
  5. Palette swap at runtime does not require canvas re-render or reallocation
**Plans**: 2 plans
Plans:
- [x] 19-01-PLAN.md — Palette core struct, presets, hex parser, unit test (completed 2026-02-24)
- [ ] 19-02-PLAN.md — Lua and WASM palette bindings

### Phase 20: Input Abstraction
**Goal**: A platform-agnostic input interface compiles cleanly on ESP32, WASM, and SDL3 — with a shared InputState, button bitmask, float axes, and edge detection
**Depends on**: Phase 18 (clean build system)
**Requirements**: INP-01, INP-02, INP-03
**Success Criteria** (what must be TRUE):
  1. The input interface headers compile without SDL3 or ESP32 headers present — zero platform type leakage
  2. InputState holds a button bitmask and float axis array accessible from any platform implementation
  3. justPressed, held, and justReleased return correct values per button across a frame transition
**Plans**: 1 plan
Plans:
- [ ] 20-01-PLAN.md — InputState header, input_advance_frame, enjin2_input CMake library, and host unit tests

### Phase 21: SDL3 CMake + Runner
**Goal**: An SDL3 desktop runner builds as an opt-in executable target, displaying a Canvas4 via palette lookup with integer scaling, a working game loop, and SDL3 keyboard mapped to the input abstraction
**Depends on**: Phase 19, Phase 20
**Requirements**: SDL-01, SDL-02, SDL-03, SDL-04, INP-04
**Success Criteria** (what must be TRUE):
  1. cmake -DENJIN2_BUILD_SDL=OFF produces a clean build with no SDL3 symbols or headers in any core library target
  2. cmake -DENJIN2_BUILD_SDL=ON builds the enjin2_sdl executable and links SDL3 only to that target
  3. The SDL3 window displays a Canvas4 scene with correct palette colors (not grayscale) at integer-scaled pixels
  4. Pressing arrow keys, Z, X, and Enter during the game loop produces the expected button states in InputState
  5. The game loop runs at a stable rate with delta-time clamping and shuts down cleanly on window close
**Plans**: 2 plans
Plans:
- [ ] 21-01-PLAN.md — CMake ENJIN2_BUILD_SDL option, FetchContent SDL3 at release-3.4.2, enjin2_sdl executable target
- [ ] 21-02-PLAN.md — SDL3 runner implementation: window, Canvas4→RGB24 blit, game loop, input_platform_poll keyboard mapping

### Phase 22: Lua Integration + E2E Validation
**Goal**: Lua scripts run identically on SDL3, WASM, and ESP32 — with input polling and palette APIs available — and a single test script confirms visual and behavioral parity across platforms
**Depends on**: Phase 21
**Requirements**: SDL-05, INP-05
**Success Criteria** (what must be TRUE):
  1. A Lua script calling isButtonHeld(0), isButtonJustPressed(0), and getAxis(0) returns correct values in the SDL3 runner
  2. The same Lua script that draws colored shapes via setPalette and setColor runs without modification on SDL3 runner and WASM, producing visually identical output
  3. Lua scripting in the SDL3 runner uses the same script execution path as WASM and ESP32 (no SDL3-specific Lua extensions required)
**Plans**: TBD

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Dependency Analysis | v1.0 | 3/3 | Complete | 2026-01-30 |
| 2. Core Migration | v1.0 | 3/3 | Complete | 2026-01-30 |
| 3. Feature Support | v1.0 | 3/3 | Complete | 2026-01-30 |
| 4. Validation | v1.0 | 4/4 | Complete | 2026-01-31 |
| 5. Final Cleanup | v1.0 | 1/1 | Complete | 2026-01-31 |
| 6. Create library docs | v1.0 | 7/7 | Complete | 2026-02-01 |
| 7. README Enhancement | v1.1 | 1/1 | Complete | 2026-02-02 |
| 8. Build System Fixes | v1.1 | 2/2 | Complete | 2026-02-03 |
| 9. Documentation Coverage | v1.1 | 5/5 | Complete | 2026-02-03 |
| 10. Module Overview Generation | v1.1 | 2/2 | Complete | 2026-02-03 |
| 11. Documentation Tracking | v1.1 | 1/1 | Complete | 2026-02-23 |
| 12. Fix Doxygen Warnings | v1.1 | 2/2 | Complete | 2026-02-23 |
| 13. Fix Doc Pipeline & API | v1.1 | 2/2 | Complete | 2026-02-23 |
| 14. Fix extractText() | v1.1 | 1/1 | Complete | 2026-02-23 |
| 15. Cleanup CI/README | v1.1 | 1/1 | Complete | 2026-02-23 |
| 16. Repository Cleanup | v1.2 | 2/2 | Complete | 2026-02-23 |
| 17. Doc Generation Fix | v1.2 | 2/2 | Complete | 2026-02-23 |
| 18. Build System Fix | v1.2 | 1/1 | Complete | 2026-02-23 |
| 19. Palette Foundation | 2/2 | Complete    | 2026-02-24 | - |
| 20. Input Abstraction | 1/1 | Complete    | 2026-02-24 | - |
| 21. SDL3 CMake + Runner | 2/2 | Complete    | 2026-02-24 | - |
| 22. Lua Integration + E2E Validation | v1.3 | 0/? | Not started | - |

**Total Progress:**
- v1.0: 21/21 plans complete (100%)
- v1.1: 17/17 plans complete (100%)
- v1.2: 5/5 plans complete (100%)
- v1.3: 1/? plans complete (in progress)
