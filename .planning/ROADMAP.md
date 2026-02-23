# Roadmap: Enjin Migration

## Overview

Complete migration from enjin to enjin2 with full independence, validation, and comprehensive documentation. enjin2 is now a self-contained library with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure.

## Milestones

- ✅ **v1.0 Migration + Documentation** — Phases 1-6 (shipped 2026-02-01)
- ✅ **v1.1 Project Infrastructure & Documentation Enhancement** — Phases 7-15 (shipped 2026-02-23)
- 🚧 **v1.2 Tech Debt Cleanup** — Phases 16-18 (in progress)

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

### 🚧 v1.2 Tech Debt Cleanup (In Progress)

**Milestone Goal:** Eliminate all enjin1 remnants and fix documentation generation issues, preparing a clean codebase for Tomodachi integration.

- [x] **Phase 16: Repository Cleanup** - Remove dead compat headers, dead examples, and generated LaTeX from git tracking (completed 2026-02-23)
- [ ] **Phase 17: Documentation Generation Fix** - Fix extractText() and formatMethod() issues, regenerate all API docs clean
- [ ] **Phase 18: Build System Fix** - Fix WASM build with ENJIN2_BUILD_LUA=OFF

## Phase Details

### Phase 16: Repository Cleanup
**Goal**: Codebase contains only live code with no enjin1 remnants or generated artifacts tracked in git
**Depends on**: Nothing (first phase of v1.2)
**Requirements**: DEAD-01, DEAD-02, DEAD-03, DEAD-04, REPO-01, REPO-02
**Success Criteria** (what must be TRUE):
  1. `include/enjin2/compat/` directory no longer exists in the repository
  2. `enjin_comparison_benchmark.cpp` and `eisei_game_benchmark.cpp` no longer exist in `examples/`
  3. CMake configuration succeeds with no references to removed files (no broken includes, no missing targets)
  4. `docs/latex/` is not tracked by git and is listed in `.gitignore`
**Plans**: 2 plans
- [ ] 16-01-PLAN.md — Remove dead compat headers, benchmark files, and clean CMake/doc references
- [ ] 16-02-PLAN.md — Untrack LaTeX files from git and update .gitignore

### Phase 17: Documentation Generation Fix
**Goal**: API documentation pages render with correct text -- no garbled strings, no duplicated keywords
**Depends on**: Phase 16 (dead code removal may affect generated docs)
**Requirements**: DOCG-01, DOCG-02, DOCG-03, DOCG-04
**Success Criteria** (what must be TRUE):
  1. Method signatures on API pages show single `const` where appropriate, not `const const`
  2. Cross-reference text in documentation renders as readable, properly-spaced strings (no fused words)
  3. `extractText()` output does not include xml2js `$` attribute object content
  4. All API markdown files are regenerated and the documentation site builds without errors
**Plans**: 2 plans
Plans:
- [ ] 17-01-PLAN.md — Fix extractText() ordered parsing, formatMethod() const const, and remove stale compat config
- [ ] 17-02-PLAN.md — Regenerate all API docs and verify Docusaurus build

### Phase 18: Build System Fix
**Goal**: WASM build path works correctly when Lua is disabled
**Depends on**: Nothing (independent of other v1.2 phases)
**Requirements**: BLDS-01
**Success Criteria** (what must be TRUE):
  1. CMake configuration succeeds with `-DENJIN2_BUILD_WASM=ON -DENJIN2_BUILD_LUA=OFF`
  2. Build completes without errors under the WASM+LUA-OFF configuration
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 16 → 17 → 18

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
| 16. Repository Cleanup | 2/2 | Complete    | 2026-02-23 | - |
| 17. Doc Generation Fix | 1/2 | In Progress|  | - |
| 18. Build System Fix | v1.2 | 0/? | Not started | - |

**Total Progress:**
- v1.0: 21/21 plans complete (100%)
- v1.1: 17/17 plans complete (100%)
- v1.2: 0/? plans complete (0%)
