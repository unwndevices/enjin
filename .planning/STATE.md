# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-23)

**Core value:** enjin2 works independently without any enjin1 dependencies
**Current focus:** Phase 18 - Build System Fix (complete)

## Current Position

Phase: 18 of 18 (Build System Fix)
Plan: 1 of 1 (complete)
Status: Complete
Last activity: 2026-02-23 — Completed 18-01 (WASM build made Lua-optional via generator expressions and preprocessor guards)

Milestone: v1.2 Tech Debt Cleanup
Previous: v1.1 shipped (2026-02-23)

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**
- Total plans completed: 41
- v1.0: 21 plans
- v1.1: 17 plans
- v1.2: 5 plans

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [v1.1 Phase 14]: extractText() $ filter — skip xml2js attribute objects in text extraction
- [v1.1 Phase 13]: classNameToXmlFilename encoding for Doxygen XML
- [Phase 16]: Generated LaTeX docs excluded from git tracking via .gitignore
- [Phase 17-01]: xml2js ordered parsing: all three options (explicitChildren + preserveChildrenOrder + charsAsChildren) required together for correct $$ mixed-content traversal
- [Phase 17-01]: extractText() primary path uses $$ array traversal — Object.entries() loses document order for mixed-content nodes
- [Phase 17-01]: formatMethod() strips trailing ' const' from argsstring when $.const=yes to avoid const const duplication
- [Phase 17-02]: formatMethod() must extract ._ from xml2js object nodes for name and argsstring — ordered parsing wraps text nodes as { _: 'text', $$: [...] }
- [Phase 18-01]: CMake generator expressions ($<$<BOOL:${ENJIN2_BUILD_LUA}>:...>) used for WASM conditional Lua linking — consistent with existing enjin2 interface target pattern
- [Phase 18-01]: ENJIN2_BUILD_LUA=1 injected as compile definition from CMake so C++ source detects Lua availability at compile time

### Pending Todos

None yet.

### Blockers/Concerns

None.

### Technical Debt (being addressed in v1.2)

- ~~Compat headers cleanup (enjin2/compat/)~~ (fixed in 16-01)
- ~~Dead examples with enjin1 references~~ (fixed in 16-01)
- ~~extractText() cosmetic issues: brief description duplication, template param concatenation, const const duplication~~ (fixed in 17-01)
- ~~84 API markdown pages regenerated with clean text~~ (completed in 17-02)
- ~~WASM+LUA OFF CMake edge case~~ (fixed in 18-01)
- ~~Generated LaTeX files tracked in git~~ (fixed in 16-02)

## Session Continuity

Last session: 2026-02-23
Stopped at: Completed 18-01-PLAN.md
Resume file: None
