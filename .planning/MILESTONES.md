# Milestones

## v1.1 Project Infrastructure & Documentation Enhancement (Shipped: 2026-02-23)

**Phases completed:** 9 phases (7-15), 17 plans
**Timeline:** 22 days (2026-02-02 → 2026-02-23)
**Git range:** 86 commits, 546 files changed, +59,387 / -13,956 lines

**Key accomplishments:**
- Professional README with badges, features list, and documentation navigation
- Lua build dependency resolved with CMake options and comprehensive dependency documentation
- Complete Doxygen documentation across all public APIs — 0 warnings (down from 372)
- Module overview pages generated for all 9 modules with Docusaurus integration
- CI Doxygen warning threshold gate to prevent documentation regression
- Fixed documentation pipeline — 76+ clean API pages with proper cross-references on GitHub Pages

**Tech debt (non-blocking):**
- Brief description duplication in extractText() (5 pages, cosmetic)
- Template parameter concatenation producing fused text (4 pages)
- `const const` duplication in formatMethod() (136 occurrences)
- WASM+LUA OFF CMake edge case (WASM off by default)

**See:** [milestones/v1.1-ROADMAP.md](milestones/v1.1-ROADMAP.md) | [milestones/v1.1-REQUIREMENTS.md](milestones/v1.1-REQUIREMENTS.md)

---

## v1.0 Migration + Documentation (Shipped: 2026-02-01)

**Phases completed:** 6 phases (1-6), 21 plans
**Timeline:** 3 days (2026-01-29 → 2026-02-01)

**Key accomplishments:**
- enjin2 fully independent of enjin1 — zero dependencies verified at source and build levels
- Comprehensive compatibility layer with namespace enjin wrappers
- Validation infrastructure: shadow mode testing, BMP comparison pipeline
- Documentation pipeline: Doxygen + Docusaurus with 59 API pages across 9 modules
- All 14 v1 requirements satisfied

**See:** [milestones/v1.0-ROADMAP.md](milestones/v1.0-ROADMAP.md) | [milestones/v1.0-REQUIREMENTS.md](milestones/v1.0-REQUIREMENTS.md)

---
