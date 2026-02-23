# Enjin Migration

## What This Is

Complete migration from enjin to enjin2 - making enjin2 fully self-contained with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure. enjin2 is now a fully independent library with comprehensive documentation deployed to GitHub Pages, professional README, and zero Doxygen warnings.

## Core Value

enjin2 works independently without any enjin1 dependencies.

## Current State

**Shipped: v1.1 Project Infrastructure & Documentation Enhancement (2026-02-23)**
- Professional README with badges, features, and documentation links
- Lua build dependency resolved with CMake options
- 0 Doxygen warnings (down from 372) with CI threshold gate
- 76+ clean API pages across 9 modules with module overviews
- Documentation pipeline fully operational: Doxygen XML → generate-api-docs.js → Docusaurus → GitHub Pages
- 9 phases completed over 22 days

**Previously shipped: v1.0 Migration + Documentation (2026-02-01)**
- enjin2 fully independent with zero enjin1 dependencies
- 28,271 LOC C++ codebase
- Documentation pipeline: Doxygen + Docusaurus (59 initial API pages)
- All 14 v1 requirements validated

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
- ✓ README provides clear project description — v1.1 (Phase 7, RDME-01)
- ✓ Features list highlighting key capabilities — v1.1 (Phase 7, RDME-02)
- ✓ Documentation links to API, guides, GitHub Pages — v1.1 (Phase 7, RDME-03)
- ✓ Lua dependency resolved — v1.1 (Phase 8, BLD-01)
- ✓ Dependencies documented — v1.1 (Phase 8, BLD-02)
- ✓ Doxygen warnings reduced to 0 (target was < 20) — v1.1 (Phase 12, DOC-01)
- ✓ Public APIs documented — v1.1 (Phases 9, 13, 14, DOC-02)
- ✓ Consistent documentation style — v1.1 (Phase 12, DOC-03)
- ✓ Module overviews added — v1.1 (Phases 9, 10, 13, DOC-04)

### Active

## Current Milestone: v1.2 Tech Debt Cleanup

**Goal:** Eliminate all enjin1 remnants and fix documentation generation issues, preparing a clean codebase for Tomodachi integration.

**Target features:**
- Remove dead compat headers (enjin1 compatibility wrappers)
- Delete dead example files with enjin1 dependencies
- Fix extractText() documentation generation issues (const const, cross-reference garbling)
- Fix WASM+LUA OFF CMake edge case
- Clean up generated LaTeX files from git tracking

### Out of Scope

- [Keeping enjin1] — Target is enjin2-only
- [Features not already in enjin2] — Focus on migration, not new features
- Strangler Fig incremental migration — Pivoted to enjin2-only approach
- Dual-backend compile-time switching — Removed in Phase 5
- Usage examples in API documentation — Deferred to future milestone
- Getting started guide — Deferred to future milestone

## Context

**After v1.1:**
enjin2 is a fully independent, self-contained library with:
- Zero enjin1 dependencies (verified at source and build levels)
- Professional README with badges, features, and documentation navigation
- Comprehensive API documentation (76+ pages across 9 modules with overviews)
- 0 Doxygen warnings with CI gate to prevent regression
- Deployment pipeline to GitHub Pages (fully operational)
- Technical debt: compat headers, examples cleanup, extractText() cosmetic issues

**Upcoming: Tomodachi**
enjin2 will serve as the graphics engine for Tomodachi — a portable MIDI/audio control gadget with Lua scripting. v1.2 cleans up tech debt; v1.3 will address Tomodachi-specific readiness (multi-layer composition, WASM build, API surface gaps).

## Constraints

- **Structure**: Clean and intelligent organization, no fuss
- **Validation**: Manual testing (no automated test suite)
- **Outcome**: Only enjin2 directory remains

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Fully independent enjin2 | User wants to keep only enjin2 in the end | ✓ Complete - v1.0 |
| Namespace enjin for compatibility | Separates migration code from enjin2 core | ✓ Implemented - kept as artifact |
| Compile-time backend selection | Allow gradual migration | Removed - pivot to enjin2-only |
| xml2js for Doxygen XML parsing | Handles C++ templates, namespaces, overloads | ✓ Working - Phase 6 |
| Module-based API organization | Better navigation than alphabetical A-Z | ✓ Working - Phase 6 |
| Docusaurus dual-plugin setup | Separate guides and API reference | ✓ Working - Phase 6 |
| Optional Lua via CMake | find_package(Lua QUIET) allows ENJIN2_BUILD_LUA=OFF | ✓ Working - Phase 8 |
| Essential-level doc standard | @brief, @param, @return only — no verbose descriptions | ✓ Good - achieves 0 warnings |
| CI Doxygen warning gate | Prevents regression above 20 warnings | ✓ Working - Phase 11 |
| classNameToXmlFilename encoding | Encodes underscores before _1_1 join for Doxygen XML | ✓ Working - Phase 13 |
| extractText() $ filter | Skip xml2js attribute objects in text extraction | ✓ Working - Phase 14 |

---
*Last updated: 2026-02-23 after v1.2 milestone started*
