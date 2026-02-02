# Requirements: Enjin Migration

**Defined:** 2026-02-01
**Core Value:** enjin2 works independently without any enjin1 dependencies

## v1.1 Requirements

Requirements for Project Infrastructure & Documentation Enhancement. Each maps to roadmap phases.

### README

- [x] **RDME-01**: README provides clear project description explaining enjin2's purpose and capabilities — satisfied 2026-02-02
- [x] **RDME-02**: README includes features list highlighting key enjin2 capabilities (static allocation, Lua/WASM integration, multi-platform support) — satisfied 2026-02-02
- [x] **RDME-03**: README includes links to API documentation, guides, and GitHub Pages — satisfied 2026-02-02

### Build System

- [ ] **BLD-01**: Lua dependency is resolved - CMake configuration handles Lua properly (not optional, must work)
- [ ] **BLD-02**: All dependencies are documented in README or separate DEPENDENCIES.md file

### Documentation Coverage

- [ ] **DOC-01**: Doxygen warnings are reduced from 210 to < 20 (addressing actual missing documentation issues)
- [ ] **DOC-02**: All public APIs are documented with Doxygen comments (complete coverage)
- [ ] **DOC-03**: Documentation follows consistent style (formatting, parameter descriptions, return values)
- [ ] **DOC-04**: Module overviews added for each module (Core, Graphics, UI, Utils, etc.) explaining purpose and usage

## Out of Scope

| Feature | Reason |
|---------|--------|
| Build/installation instructions in README | Not in scope - focus on project description, features, links |
| Quick start examples in README | Defer to future milestone - examples not priority currently |
| Usage examples in API documentation | Not in scope - focus on coverage and quality first |
| Getting started guide | Defer to future milestone |
| Optional Lua support | User confirmed Lua is NOT optional - must work |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| RDME-01 | Phase 7 | Complete |
| RDME-02 | Phase 7 | Complete |
| RDME-03 | Phase 7 | Complete |
| BLD-01 | Phase 8 | Pending |
| BLD-02 | Phase 8 | Pending |
| DOC-01 | Phase 9 | Pending |
| DOC-02 | Phase 9 | Pending |
| DOC-03 | Phase 9 | Pending |
| DOC-04 | Phase 9 | Pending |

**Coverage:**
- v1.1 requirements: 9 total
- Mapped to phases: 9 ✓
- Unmapped: 0

---
*Requirements defined: 2026-02-01*
*Last updated: 2026-02-01 after v1.1 roadmap creation*
