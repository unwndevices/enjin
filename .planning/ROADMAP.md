# Roadmap: Enjin Migration

## Overview

Complete migration from enjin to enjin2 with full independence, validation, and comprehensive documentation. enjin2 is now a self-contained library with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure.

## Milestones

- ✅ **v1.0 Migration + Documentation** — Phases 1-6 (shipped 2026-02-01)
- ✅ **v1.1 Project Infrastructure & Documentation Enhancement** — Phases 7-9 (completed 2026-02-03)

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

### v1.1 Project Infrastructure & Documentation Enhancement (In Progress - Gap Closure)

**Milestone Goal:** Improve project accessibility with comprehensive README, fix build dependencies, and enhance documentation coverage.

#### Phase 7: README Enhancement
**Goal**: Professional project entry point with clear description and navigation
**Depends on**: Phase 6 (v1.0 complete)
**Requirements**: RDME-01, RDME-02, RDME-03
**Success Criteria** (what must be TRUE):
  1. User visiting repository can understand enjin2's purpose and capabilities from README
  2. User can see list of key features (static allocation, Lua/WASM, multi-platform)
  3. User can navigate to API documentation, guides, and GitHub Pages from README
**Plans**: 1 plan

Plans:
- [x] 07-01: Create professional README with badges, description, features, installation, quick start, and documentation links — completed 2026-02-02

#### Phase 8: Build System Fixes
**Goal**: Build system works correctly with all dependencies documented
**Depends on**: Phase 7
**Requirements**: BLD-01, BLD-02
**Success Criteria** (what must be TRUE):
  1. User can build enjin2 successfully without Lua dependency errors
  2. User can find complete list of dependencies in README or DEPENDENCIES.md
**Plans**: 2 plans
**Completed**: 2026-02-03

Plans:
- [x] 08-01: Make Lua optional in CMake build system — modify CMakeLists.txt to use find_package(Lua QUIET) and add clear error messages — completed 2026-02-03
- [x] 08-02: Document dependencies in README — add Dependencies section with Required vs Optional categorization and installation instructions — completed 2026-02-03

#### Phase 9: Documentation Coverage
**Goal**: Comprehensive, high-quality documentation across all public APIs
**Depends on**: Phase 8
**Requirements**: DOC-01, DOC-02, DOC-03, DOC-04
**Success Criteria** (what must be TRUE):
  1. Doxygen runs with fewer than 20 warnings (down from 210)
  2. All public APIs have documentation when browsing Doxygen output
  3. Documentation pages follow consistent formatting and style
  4. Each module has an overview page explaining its purpose
**Plans**: 5 plans

 Plans:
- [x] 09-01: Enable full Doxygen warnings and create tracking infrastructure — configure Doxyfile with WARN_NO_PARAMDOC=YES, generate and categorize all warnings, create tracking list for undocumented APIs, establish documentation templates
- [x] 09-02: Document core, graphics, and UI module public APIs — add essential-level Doxygen comments (@brief, @param, @return) to all public APIs in these high-priority modules
- [x] 09-03: Document scripting, animation, and utils module public APIs — add essential-level Doxygen comments to all public APIs in these medium-priority modules
- [x] 09-04: Document abstract, compat, components, and effects module public APIs — add essential-level Doxygen comments to all public APIs in these low-priority modules
- [x] 09-05: Create module overview pages and verify final warnings — create @defgroup overview pages for all 10 modules, verify Doxygen warnings reduced to under 20, update tracking list with final results

#### Phase 10: Complete Module Overview Generation
**Goal**: Generate Docusaurus module overview pages from Doxygen @defgroup annotations
**Depends on**: Phase 9
**Gap Closure**: Closes integration gap from v1.1 audit - module overview pages not converted to Docusaurus
**Success Criteria** (what must be TRUE):
  1. scripts/generate-api-docs.js parses group XML files from Doxygen output
  2. Module overview pages generated in docs/api/*/README.md for all 9 modules (excludes compat as it's not in config.modules)
  3. Module overview pages linked in Docusaurus sidebar navigation
  4. Documentation build includes module overview pages without errors
**Plans**: 2 plans

Plans:
- [x] 10-01-PLAN.md — Add group XML parsing and module overview generation functions — completed 2026-02-03
- [x] 10-02-PLAN.md — Integrate module overview generation and build verification — completed 2026-02-03

#### Phase 11: Documentation Tracking Improvements
**Goal**: Add automated Doxygen warning verification to CI
**Depends on**: Phase 10
**Gap Closure**: Closes tech debt from v1.1 audit - Doxygen warning tracking gap
**Success Criteria** (what must be TRUE):
  1. Doxygen WARN_LOGFILE properly created during build
  2. CI/CD workflow verifies warning count is under 20
  3. Fails build if warning threshold exceeded
**Plans**: TBD

#### Phase 12: Fix Doxygen Warning Regression
**Goal**: Reduce Doxygen warnings from 304 back to under 20 threshold
**Depends on**: Phase 9
**Requirements**: DOC-01, DOC-03
**Gap Closure**: Closes gaps from v1.1 audit — DOC-01 unsatisfied (304 warnings vs <20 threshold), DOC-03 partial (@param mismatches)
**Success Criteria** (what must be TRUE):
  1. Doxygen runs with fewer than 20 warnings
  2. All @param mismatches in lua_engine.hpp resolved
  3. Undocumented classes (Canvas8, Canvas4_ESP32S3) and typedefs (PositionTrack, FloatTrack, ColorTrack) documented
**Plans**: TBD

#### Phase 13: Fix Documentation Pipeline & API Landing
**Goal**: Fix generate-api-docs.js encoding bugs and create API landing page
**Depends on**: Phase 12
**Requirements**: DOC-02, DOC-04
**Gap Closure**: Closes gaps from v1.1 audit — DOC-02 partial (12+ classes missing from Docusaurus), DOC-04 partial (compat excluded, Effects conflict), integration gaps, broken E2E flows
**Success Criteria** (what must be TRUE):
  1. generate-api-docs.js correctly handles C_* prefixed classes (underscore encoding fixed)
  2. Nested class config uses :: notation (ComponentQuery::Iterator, ComponentStorage::Iterator)
  3. math::TrigLUT config entry corrected
  4. Compat module included in documentation generator config
  5. Effects routing conflict resolved durably (no Effects.md / effects/ conflict)
  6. API landing page exists at docs/api/ — README /api link works
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 7 → 8 → 9 → 10 → 11 → 12 → 13

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
   | 11. Documentation Tracking Improvements | v1.1 | 0/0 | Pending | - |
   | 12. Fix Doxygen Warning Regression | v1.1 | 0/0 | Pending | - |
   | 13. Fix Documentation Pipeline & API Landing | v1.1 | 0/0 | Pending | - |

**Total Progress:**
- v1.0: 21/21 plans complete (100%)
- v1.1: 10/10 plans complete, 3 phases pending (gap closure)
