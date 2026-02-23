# Enjin Migration

## What This Is

enjin2 is a lightweight, statically-allocated 2D graphics engine for embedded devices and WASM. It runs on ESP32, WebAssembly (Emscripten), and VCV Rack, with Lua scripting support. enjin2 powers Tomodachi — a portable MIDI/audio control gadget with a pixel display.

## Core Value

enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation.

## Current State

**Shipped: v1.2 Tech Debt Cleanup (2026-02-23)**
- Dead enjin1 compat headers, benchmarks, and CMake references removed
- extractText() rewritten with xml2js ordered parsing for correct document-order traversal
- formatMethod() const const duplication eliminated, 84 API pages regenerated clean
- WASM build made Lua-optional via CMake generator expressions and C++ preprocessor guards
- Generated LaTeX files untracked from git
- 3 phases, 5 plans, 330 files changed, -33k net lines (cleanup)

**Previously shipped: v1.1 Project Infrastructure & Documentation Enhancement (2026-02-23)**
- Professional README with badges, features, and documentation links
- Lua build dependency resolved with CMake options
- 0 Doxygen warnings (down from 372) with CI threshold gate
- 76+ clean API pages across 9 modules with module overviews
- Documentation pipeline fully operational: Doxygen XML → generate-api-docs.js → Docusaurus → GitHub Pages

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
- ✓ Dead compat headers removed — v1.2 (Phase 16, DEAD-01)
- ✓ Dead benchmark examples removed — v1.2 (Phase 16, DEAD-02/DEAD-03)
- ✓ Dead file references cleaned up — v1.2 (Phase 16, DEAD-04)
- ✓ extractText() filters xml2js attribute objects — v1.2 (Phase 17, DOCG-01)
- ✓ formatMethod() eliminates const const duplication — v1.2 (Phase 17, DOCG-02)
- ✓ All API markdown files regenerated clean — v1.2 (Phase 17, DOCG-03)
- ✓ Cross-reference text renders correctly — v1.2 (Phase 17, DOCG-04)
- ✓ WASM build succeeds with LUA=OFF — v1.2 (Phase 18, BLDS-01)
- ✓ Generated LaTeX files removed from git — v1.2 (Phase 16, REPO-01)
- ✓ .gitignore updated for LaTeX exclusion — v1.2 (Phase 16, REPO-02)

## Current Milestone: v1.3 Tomodachi Readiness

**Goal:** Make enjin2 ready for Tomodachi integration with color palettes, desktop development, and flexible input.

**Target features:**
- 16-color indexed palette system (15 colors + transparent, lookup at display time)
- SDL2 desktop runner (C++ app with Lua scripting, third platform backend)
- Flexible input abstraction (buttons, pots, joysticks, touchpads, keyboard — platform-agnostic)

### Active

- [ ] 16-color indexed palette system
- [ ] SDL2 desktop runner with Lua scripting
- [ ] Platform-agnostic input abstraction

### Out of Scope

- [Keeping enjin1] — Target is enjin2-only
- [Features not already in enjin2] — Focus on migration, not new features
- Strangler Fig incremental migration — Pivoted to enjin2-only approach
- Dual-backend compile-time switching — Removed in Phase 5
- Usage examples in API documentation — Deferred to future milestone
- Getting started guide — Deferred to future milestone
- Multi-layer composition — Deferred to v1.4+
- MIDI/audio integration — Tomodachi-side, not enjin2

## Context

**After v1.2:**
enjin2 is a fully independent, clean library with:
- Zero enjin1 dependencies or remnants
- 84 clean API pages, 0 Doxygen warnings, CI gate
- WASM build with optional Lua support
- No tech debt blocking Tomodachi integration

**Current architecture (relevant to v1.3):**
- Canvas system: 4-bit (Canvas4) and 8-bit (Canvas8) templates, Pixel4 stores values 0-15 as grayscale
- Platform backends: WASM (Emscripten bindings), ESP32 (Arduino), VCV Rack — no SDL2
- Input: UI-level InputComponent/InputSystem (mouse-only, not a general input abstraction)
- Build: CMake multi-target (enjin2_core, enjin2_graphics, enjin2_ui, enjin2_lua, enjin2_wasm)

**Tomodachi context:**
Portable MIDI/audio control gadget with pixel display and Lua scripting. Physical inputs include buttons, potentiometers, joysticks, and touchpads. Desktop development via SDL2 enables rapid iteration before deploying to ESP32.

## Constraints

- **Structure**: Clean and intelligent organization, no fuss
- **Validation**: Manual testing (no automated test suite)
- **Memory**: No dynamic allocation (static arrays, no heap)
- **Platforms**: Must work on ESP32, WASM, and SDL2 desktop

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
| xml2js ordered parsing | explicitChildren + preserveChildrenOrder + charsAsChildren for correct mixed-content traversal | ✓ Working - Phase 17 |
| extractText() $$ array traversal | Object.entries() loses document order for mixed-content nodes | ✓ Working - Phase 17 |
| formatMethod() const dedup | Strip trailing ' const' from argsstring when $.const=yes | ✓ Working - Phase 17 |
| CMake generator expressions for WASM Lua | $<$<BOOL:${ENJIN2_BUILD_LUA}>:...> consistent with existing target pattern | ✓ Working - Phase 18 |
| ENJIN2_BUILD_LUA compile definition | CMake injects ENJIN2_BUILD_LUA=1 so C++ preprocessor gates Lua code | ✓ Working - Phase 18 |

---
*Last updated: 2026-02-23 after starting v1.3 milestone*
