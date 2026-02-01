# Roadmap: Enjin Migration

## Overview

Complete migration from enjin to enjin2 with full independence, validation, and comprehensive documentation. enjin2 is now a self-contained library with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure.

## Milestones

- ✅ **v1.0 Migration + Documentation** — Phases 1-6 (shipped 2026-02-01)
- 📋 **v1.1 Project Infrastructure & Documentation Enhancement** — Phases 7-9 (not started)

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

### 📋 v1.1 Project Infrastructure & Documentation Enhancement (Not Started)

**Milestone Goal:** Improve project accessibility with comprehensive README, fix build dependencies, and enhance documentation coverage.

#### Phase 7: README Enhancement
**Goal**: Professional project entry point with clear description and navigation
**Depends on**: Phase 6 (v1.0 complete)
**Requirements**: RDME-01, RDME-02, RDME-03
**Success Criteria** (what must be TRUE):
  1. User visiting repository can understand enjin2's purpose and capabilities from README
  2. User can see list of key features (static allocation, Lua/WASM, multi-platform)
  3. User can navigate to API documentation, guides, and GitHub Pages from README
**Plans**: TBD

Plans:
- [ ] 07-01: [TBD during planning]

#### Phase 8: Build System Fixes
**Goal**: Build system works correctly with all dependencies documented
**Depends on**: Phase 7
**Requirements**: BLD-01, BLD-02
**Success Criteria** (what must be TRUE):
  1. User can build enjin2 successfully without Lua dependency errors
  2. User can find complete list of dependencies in README or DEPENDENCIES.md
**Plans**: TBD

Plans:
- [ ] 08-01: [TBD during planning]

#### Phase 9: Documentation Coverage
**Goal**: Comprehensive, high-quality documentation across all public APIs
**Depends on**: Phase 8
**Requirements**: DOC-01, DOC-02, DOC-03, DOC-04
**Success Criteria** (what must be TRUE):
  1. Doxygen runs with fewer than 20 warnings (down from 210)
  2. All public APIs have documentation when browsing Doxygen output
  3. Documentation pages follow consistent formatting and style
  4. Each module has an overview page explaining its purpose
**Plans**: TBD

Plans:
- [ ] 09-01: [TBD during planning]

## Progress

**Execution Order:**
Phases execute in numeric order: 7 → 8 → 9

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Dependency Analysis | v1.0 | 3/3 | Complete | 2026-01-30 |
| 2. Core Migration | v1.0 | 3/3 | Complete | 2026-01-30 |
| 3. Feature Support | v1.0 | 3/3 | Complete | 2026-01-30 |
| 4. Validation | v1.0 | 4/4 | Complete | 2026-01-31 |
| 5. Final Cleanup | v1.0 | 1/1 | Complete | 2026-01-31 |
| 6. Create library docs | v1.0 | 7/7 | Complete | 2026-02-01 |
| 7. README Enhancement | v1.1 | 0/TBD | Not started | - |
| 8. Build System Fixes | v1.1 | 0/TBD | Not started | - |
| 9. Documentation Coverage | v1.1 | 0/TBD | Not started | - |

**Total Progress:**
- v1.0: 21/21 plans complete (100%)
- v1.1: 0/TBD plans (0%)
