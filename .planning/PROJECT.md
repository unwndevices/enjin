# Enjin Migration

## What This Is

Complete migration from enjin to enjin2 - making enjin2 fully self-contained with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure. enjin2 is now a fully independent library with comprehensive documentation deployed to GitHub Pages.

## Core Value

enjin2 works independently without any enjin1 dependencies.

## Current State

**Shipped: v1.0 Migration + Documentation (2026-02-01)**
- enjin2 is fully independent with zero enjin1 dependencies
- 28,271 LOC C++ codebase
- Comprehensive documentation: 59 API pages across 9 modules
- All 14 v1 requirements validated
- 6 phases completed: Dependency Analysis, Core Migration, Feature Support, Validation, Final Cleanup, Documentation

## Current Milestone: v1.1 Project Infrastructure & Documentation Enhancement

**Goal:** Improve project accessibility with comprehensive README, fix build dependencies, and enhance documentation coverage.

**Target features:**
- Professional README with project description, features, and links to documentation
- Fixed build system (Lua dependency resolved)
- Comprehensive Doxygen documentation (reduced warnings, complete coverage)
- Usage examples in API documentation
- Filled documentation gaps

## Requirements

### Validated

- ✓ enjin2 fully independent of enjin1 — v1.0 (verified via CMake graphviz, compiler tracking, AST analysis)
- ✓ Compatibility headers created — v1.0 (namespace enjin with type aliases and lifecycle wrappers)
- ✓ Memory mapping documented — v1.0 (shared_ptr to unique_ptr conversion guide)
- ✓ Component lifecycle working — v1.0 (awake/start/update methods)
- ✓ Scene management working — v1.0 (SceneStateMachine with transitions)
- ✓ Manual testing baseline — v1.0 (infrastructure created, user confirmed parity)
- ✓ enjin2 headers compile independently — v1.0 (verified in isolation)
- ✓ enjin2-only build system — v1.0 (all USE_ENJIN1 references removed)
- ✓ BMP export capability — v1.0 (stb_image_write integration)
- ✓ Documentation pipeline — v1.0 (Doxygen + Docusaurus + GitHub Pages)

### Active

- [ ] README provides clear project description — v1.1 (Phase 7, RDME-01)
- [ ] Features list highlighting key capabilities — v1.1 (Phase 7, RDME-02)
- [ ] Documentation links to API, guides, GitHub Pages — v1.1 (Phase 7, RDME-03)
- [ ] Lua dependency resolved — v1.1 (Phase 8, BLD-01)
- [ ] Dependencies documented — v1.1 (Phase 8, BLD-02)
- [ ] Doxygen warnings reduced (< 20) — v1.1 (Phase 9, DOC-01)
- [ ] Public APIs documented — v1.1 (Phase 9, DOC-02)
- [ ] Consistent documentation style — v1.1 (Phase 9, DOC-03)
- [ ] Module overviews added — v1.1 (Phase 9, DOC-04)

### Out of Scope

- [Keeping enjin1] — Target is enjin2-only ✓
- [Features not already in enjin2] — Focus on migration, not new features ✓
- Strangler Fig incremental migration — Pivoted to enjin2-only approach
- Dual-backend compile-time switching — Removed in Phase 5

## Context

**Before v1.0:**
Two libraries existed in separate directories:
- enjin1: Original implementation, fully functional
- enjin2: New implementation with Lua/WASM integration and non-dynamic memory allocation, but depended on enjin1 for core infrastructure, utilities, and feature code

**After v1.0:**
enjin2 is a fully independent, self-contained library with:
- Zero enjin1 dependencies (verified at source and build levels)
- Comprehensive API documentation (59 pages)
- Deployment pipeline to GitHub Pages
- Technical debt: compat headers, examples cleanup

## Constraints

- **Structure**: Clean and intelligent organization, no fuss
- **Validation**: Manual testing (no automated test suite)
- **Outcome**: Only enjin2 directory remains ✓

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Fully independent enjin2 | User wants to keep only enjin2 in the end | ✓ Complete - v1.0 |
| Namespace enjin for compatibility | Separates migration code from enjin2 core | ✓ Implemented - kept as artifact |
| Compile-time backend selection | Allow gradual migration | Removed - pivot to enjin2-only |
| xml2js for Doxygen XML parsing | Handles C++ templates, namespaces, overloads | ✓ Working - Phase 6 |
| Module-based API organization | Better navigation than alphabetical A-Z | ✓ Working - Phase 6 |
| Docusaurus dual-plugin setup | Separate guides and API reference | ✓ Working - Phase 6 |

---
*Last updated: 2026-02-01 after v1.1 roadmap creation*
