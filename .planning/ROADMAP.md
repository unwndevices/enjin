# Roadmap: Enjin Migration

## Overview

Complete migration from enjin to enjin2 with full independence, validation, and comprehensive documentation. enjin2 is now a self-contained library with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure.

## Milestones

- ✅ **v1.0 Migration + Documentation** — Phases 1-6 (shipped 2026-02-01)
- ✅ **v1.1 Project Infrastructure & Documentation Enhancement** — Phases 7-15 (shipped 2026-02-23)
- ✅ **v1.2 Tech Debt Cleanup** — Phases 16-18 (shipped 2026-02-23)
- ✅ **v1.3 Tomodachi Readiness** — Phases 19-22 (shipped 2026-02-24)
- 🚧 **v1.4 Engine Capabilities** — Phases 23-26 (in progress)

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

<details>
<summary>✅ v1.3 Tomodachi Readiness (Phases 19-22) — SHIPPED 2026-02-24</summary>

**See full details:** [.planning/milestones/v1.3-ROADMAP.md](.planning/milestones/v1.3-ROADMAP.md)

- [x] Phase 19: Palette Foundation (2/2 plans) — completed 2026-02-24
- [x] Phase 20: Input Abstraction (1/1 plan) — completed 2026-02-24
- [x] Phase 21: SDL3 CMake + Runner (2/2 plans) — completed 2026-02-24
- [x] Phase 22: Lua Integration + E2E Validation (2/2 plans) — completed 2026-02-24

**Total:** 4 phases, 7 plans, all complete
</details>

### 🚧 v1.4 Engine Capabilities (In Progress)

**Milestone Goal:** Rework the sprite system, add multi-layer canvas composition, fix Docusaurus navigation, and add Lua hot reload to SDL3.

- [x] **Phase 23: Docusaurus Navigation Fix** - Repair MDX navigation and escape angle brackets in the doc generator (completed 2026-02-24)
- [x] **Phase 24: Sprite System Rework** - Clean API, sprite sheets, frame animation, and Lua sprite pool (completed 2026-02-24)
- [ ] **Phase 25: Multi-Layer Canvas Composition** - 4 independent Canvas4 buffers composited at blit time with Lua API
- [ ] **Phase 26: Lua Hot Reload** - F5 full Lua state reset in SDL3 runner with error display

## Phase Details

### Phase 23: Docusaurus Navigation Fix
**Goal**: API documentation is fully navigable — every module and class page is reachable from the sidebar, `npm run build` passes with zero MDX errors, and future doc regenerations are safe from angle-bracket breakage.
**Depends on**: Nothing (fully independent)
**Requirements**: DOC-01, DOC-02
**Success Criteria** (what must be TRUE):
  1. The "API Reference" navbar link opens and displays the sidebar with all module and class entries visible and clickable
  2. Every one of the ~85 generated API pages loads without a 404 or MDX parse error
  3. `npm run build` completes with zero errors and zero MDX warnings
  4. Re-running `generate-api-docs.js` and rebuilding produces the same clean result (angle brackets in template type names are escaped in the generator, not just the output files)
**Plans**: 1 plan

Plans:
- [ ] 23-01-PLAN.md — Fix generator escaping + namespace config, regenerate docs, verify clean build

### Phase 24: Sprite System Rework
**Goal**: The sprite system is rebuilt with a clean, zero-alloc API targeting `ICanvas<Pixel4>`, supporting uniform grid sprite sheets, frame animation with three loop modes, and a Lua sprite pool for scripted games.
**Depends on**: Nothing (independent of layer system at C++ struct level)
**Requirements**: SPR-01, SPR-02, SPR-03, SPR-04, SPR-05, SPR-06
**Success Criteria** (what must be TRUE):
  1. A `SpriteSheet` can be loaded with cell width, cell height, rows, and cols; any frame is drawable by linear index or (row, col) position with no legacy public member access
  2. `C_Sprite` component animates through frames automatically at a configurable FPS rate in once, loop, and ping-pong modes, advancing correctly across game loop ticks
  3. Lua script can create a sprite, draw it at a position, advance its animation, and set a specific frame via `newSprite`, `drawSprite`, `updateSprite`, and `setFrame` bindings
  4. The old `Sprite` API (public `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` fields; `ICanvas<uint8_t>` draw target; matte default 16) is gone — the codebase compiles clean with no legacy callers
**Plans**: 3 plans

Plans:
- [ ] 24-01-PLAN.md — Replace legacy Sprite class with SpriteSheet struct + AnimMode enum
- [ ] 24-02-PLAN.md — Update C_Drawable signature + all derived components + new C_Sprite animation state machine
- [ ] 24-03-PLAN.md — Add Lua sprite pool (SpriteState[16]) + four lua_CFunction bindings

### Phase 25: Multi-Layer Canvas Composition
**Goal**: The engine renders up to 4 independent Canvas4 layers composited in draw order at blit time, with index 15 as the transparency passthrough and a Lua API for layer selection and clearing.
**Depends on**: Phase 24 (Lua `drawSprite` signature requires both sprite pool and layer canvas pointer array)
**Requirements**: LAYER-01, LAYER-02, LAYER-03, LAYER-04, LAYER-05, LAYER-06
**Success Criteria** (what must be TRUE):
  1. Drawing exclusively to layer 2 (with layers 0, 1, 3 empty) produces visible output in the SDL3 window — pixels are not silently discarded
  2. A Lua script calling `setLayer(1)` then drawing, then `setLayer(2)` then drawing produces two distinct visual layers composited correctly in the window, with layer 2 appearing on top of layer 1
  3. `clearLayer(n, color)` clears only the specified layer; pixels on other layers are unaffected and remain visible
  4. `getLayerCount()` returns 4; the layer count is a compile-time constant (default 4) and changing it rebuilds correctly
  5. SDL3 runner blits the composited result — all four layers merged using index 15 as passthrough transparency — with no regression to single-canvas rendering
**Plans**: 3 plans

Plans:
- [ ] 25-01-PLAN.md — LayerCompositor struct + C_Drawable overhaul + compositor unit test
- [ ] 25-02-PLAN.md — SDL3 runner compositor integration (replace g_canvas with LayerCompositor)
- [ ] 25-03-PLAN.md — Lua layer API (setLayer, clearLayer, getLayerCount, visibility) + layer demo

### Phase 26: Lua Hot Reload
**Goal**: Pressing F5 in the SDL3 runner performs a full Lua state reset and reloads the script from disk without crashing, including graceful error display on syntax or runtime failure.
**Depends on**: Phase 25 (reload must re-wire the full layer canvas array, not just the old single canvas)
**Requirements**: HOT-01, HOT-02, HOT-03
**Success Criteria** (what must be TRUE):
  1. Pressing F5 while the SDL3 runner is active reloads the Lua script from disk and resumes execution — visible as a console log `[reload] <script>` and the game state restarting
  2. Pressing F5 twice in rapid succession produces no crash and no "Function not found" or dangling-pointer errors — full reset is idempotent
  3. Saving a Lua script with a syntax error and pressing F5 displays the error message in the console without crashing the runner; the window remains open
  4. WASM and ESP32 builds are entirely unaffected — no new `#ifdef` leakage and no build breakage with `ENJIN2_BUILD_SDL=OFF`
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
| 19. Palette Foundation | v1.3 | 2/2 | Complete | 2026-02-24 |
| 20. Input Abstraction | v1.3 | 1/1 | Complete | 2026-02-24 |
| 21. SDL3 CMake + Runner | v1.3 | 2/2 | Complete | 2026-02-24 |
| 22. Lua Integration + E2E Validation | v1.3 | 2/2 | Complete | 2026-02-24 |
| 23. Docusaurus Navigation Fix | 1/1 | Complete    | 2026-02-24 | - |
| 24. Sprite System Rework | 3/3 | Complete    | 2026-02-24 | - |
| 25. Multi-Layer Canvas Composition | v1.4 | 0/TBD | Not started | - |
| 26. Lua Hot Reload | v1.4 | 0/TBD | Not started | - |

**Total Progress: 46/46 plans complete (100%) through v1.3 — v1.4 starting at Phase 23**
