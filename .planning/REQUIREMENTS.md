# Requirements: Enjin Migration

**Defined:** 2026-02-23
**Core Value:** enjin2 works independently without any enjin1 dependencies

## v1.2 Requirements

Requirements for tech debt cleanup milestone. Each maps to roadmap phases.

### Dead Code Removal

- [ ] **DEAD-01**: Compat header files removed from `include/enjin2/compat/`
- [ ] **DEAD-02**: `enjin_comparison_benchmark.cpp` deleted from examples
- [ ] **DEAD-03**: `eisei_game_benchmark.cpp` deleted from examples
- [ ] **DEAD-04**: Any remaining references to removed files cleaned up (CMake, includes, docs)

### Documentation Generation Fix

- [ ] **DOCG-01**: `extractText()` filters xml2js `$` attribute objects to prevent text garbling
- [ ] **DOCG-02**: `formatMethod()` eliminates `const const` duplication in method signatures
- [ ] **DOCG-03**: All API markdown files regenerated with clean output
- [ ] **DOCG-04**: Cross-reference text no longer produces fused/garbled strings

### Build System Fix

- [ ] **BLDS-01**: WASM build succeeds with `ENJIN2_BUILD_LUA=OFF`

### Repository Hygiene

- [x] **REPO-01**: Generated LaTeX files (`docs/latex/`) removed from git tracking
- [x] **REPO-02**: `.gitignore` updated to exclude generated LaTeX files

## Future Requirements

Deferred to v1.3 (Tomodachi Readiness):

- **TOMO-01**: Multi-layer composition system (BG, FG, UI, OVR)
- **TOMO-02**: WASM build fully functional with Emscripten
- **TOMO-03**: API surface verified against Tomodachi gfx.* requirements
- **TOMO-04**: Lua binding verification for Tomodachi integration

## Out of Scope

| Feature | Reason |
|---------|--------|
| Usage examples in API documentation | Deferred — not part of tech debt cleanup |
| Getting started guide | Deferred — not part of tech debt cleanup |
| New features or capabilities | v1.2 is cleanup only |
| Tomodachi-specific readiness | Deferred to v1.3 |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| DEAD-01 | Phase 16 | Pending |
| DEAD-02 | Phase 16 | Pending |
| DEAD-03 | Phase 16 | Pending |
| DEAD-04 | Phase 16 | Pending |
| DOCG-01 | Phase 17 | Pending |
| DOCG-02 | Phase 17 | Pending |
| DOCG-03 | Phase 17 | Pending |
| DOCG-04 | Phase 17 | Pending |
| BLDS-01 | Phase 18 | Pending |
| REPO-01 | Phase 16 | Complete |
| REPO-02 | Phase 16 | Complete |

**Coverage:**
- v1.2 requirements: 11 total
- Mapped to phases: 11
- Unmapped: 0

---
*Requirements defined: 2026-02-23*
*Last updated: 2026-02-23 after roadmap creation*
