# Milestones

## v1.3 Tomodachi Readiness (Shipped: 2026-02-24)

**Phases completed:** 4 phases (19-22), 7 plans
**Timeline:** 1 day (2026-02-24)
**Git range:** feat(19-01) → feat(22-02), 39 files changed, +5,766/-61 lines

**Key accomplishments:**
- 16-color indexed PICO-8 palette with transparent index 15, runtime swap, and no canvas re-render
- Lua and WASM palette bindings: `setPaletteColor`, `getPaletteColor`, `getPaletteRGB`, `loadPalette`
- Platform-agnostic `InputState` with uint16_t bitmask, float axes[8], and edge detection (justPressed/held/justReleased)
- SDL3 opt-in runner with Canvas4→RGB24 blit, 4× nearest-neighbor scaling, fixed-rate game loop, and keyboard input
- Lua input polling API (`isButtonHeld`, `isButtonJustPressed`, `isButtonJustReleased`, `getAxis`) + `e2e_parity.lua` cross-platform test
- Lua scripting wired into SDL3 runner via conditional CMake linking — same scripts run on SDL3, WASM, and ESP32 without modification

**Tech debt (non-blocking):**
- `getPaletteRGB()` delivers snapshot buffer (not live view) — callers must re-invoke after palette mutation; SDL runner unaffected
- Full Emscripten toolchain build not verified (code inspection conclusive)
- API navigation disabled in Docusaurus due to MDX syntax issues (carried from v1.0)

**See:** [milestones/v1.3-ROADMAP.md](milestones/v1.3-ROADMAP.md) | [milestones/v1.3-REQUIREMENTS.md](milestones/v1.3-REQUIREMENTS.md)

---

## v1.2 Tech Debt Cleanup (Shipped: 2026-02-23)

**Phases completed:** 3 phases (16-18), 5 plans
**Timeline:** 1 day (2026-02-23)
**Git range:** 18 commits, 330 files changed, +2,787 / -36,083 lines

**Key accomplishments:**
- Removed dead enjin1 compat headers, benchmark files, and CMake references
- Untracked generated LaTeX files from git and updated .gitignore
- Fixed xml2js ordered parsing and rewrote extractText() for correct document-order traversal
- Eliminated const const duplication in formatMethod() and regenerated 84 clean API pages
- Made WASM build Lua-optional with CMake generator expressions and C++ preprocessor guards

**Tech debt (non-blocking):**
- API navigation disabled in Docusaurus due to MDX syntax issues (carried from v1.0)
- parameterlist name/description concatenation in 5 API docs (Doxygen XML limitation)
- Full Emscripten toolchain build not verified (code inspection conclusive, toolchain not in dev env)

**See:** [milestones/v1.2-ROADMAP.md](milestones/v1.2-ROADMAP.md) | [milestones/v1.2-REQUIREMENTS.md](milestones/v1.2-REQUIREMENTS.md)

---

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
