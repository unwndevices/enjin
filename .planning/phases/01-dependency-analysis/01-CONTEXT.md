# Phase 1: Dependency Analysis - Context

**Gathered:** 2026-01-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Understand enjin1→enjin2 dependencies and establish compilation isolation. This phase produces a dependency graph showing all enjin1 references in enjin2, creates separate build targets with isolated include paths, and ensures no namespace enjin references exist in enjin2 codebase. Discussion clarifies HOW to implement this analysis, not whether to add new capabilities.

</domain>

<decisions>
## Implementation Decisions

### Dependency graph format
- Structured data format (JSON/YAML) for machine readability
- Minimal information per dependency: what depends on what and how (source file, target file, type)
- Flat list by file organization
- Unidirectional tracking only (enjin2 depends on enjin1)

### Scope of analysis
- Claude's Discretion: Dependency levels to capture (direct vs indirect)
- All dependency types (compile-time and runtime)
- Claude's Discretion: Namespace usage pattern tracking approach
- Exclude test code from analysis

### Reporting format
- Claude's Discretion: Documentation structure (single vs multiple documents)
- Executive summary level of detail
- Required sections: Overview, counts, key findings
- Markdown output only (no JSON/YAML alongside)

### Build isolation approach
- Separate CMake target for enjin2 with independent include paths
- Strict separation: no enjin1 paths allowed in enjin2 target
- Duplicate shared external dependencies for each target
- Success criteria: fail if any enjin1 references exist

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-dependency-analysis*
*Context gathered: 2026-01-30*
