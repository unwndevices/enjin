# Phase 9: Documentation Coverage - Context

**Gathered:** 2026-02-03
**Status:** Ready for planning

## Phase Boundary

Improve Doxygen documentation quality across all public APIs — reduce warnings from 210 to under 20, ensure all public APIs have documentation, establish consistent formatting standards, and add module overview pages.

## Implementation Decisions

### Warning prioritization
- Fix all 210 warnings (strict zero-warning goal, not just critical types)
- Prioritize functions and classes over variables and enums - APIs users call directly first
- Deprecated APIs get minimal documentation (basic @deprecated note only, not full docs)
- Claude's discretion on internal APIs (code in src/ not include/)

### Documentation depth and style
- Essential level only: brief description + parameters + return value (no examples, no pre/postconditions)
- Parameter constraints documented only when non-obvious (ranges, nullability, special values)
- Claude's discretion on template documentation detail level
- No code examples in API reference (examples belong in tutorials/guides)

### Module overview scope
- Purpose only: one-paragraph explanation of what the module does (no design notes, no usage examples)
- Claude's discretion on overview length (may vary by module complexity)
- Claude's discretion on linking to related modules and dependencies
- Claude's discretion on which modules need overview pages (based on size/complexity)

### Handling missing documentation
- Add boilerplate documentation for currently undocumented APIs (don't skip or just mark TODO)
- Claude's discretion on boilerplate detail level (may vary by API complexity)
- Always add @brief descriptions to undocumented classes and functions
- Maintain separate list tracking APIs with placeholder documentation for future review

### Claude's Discretion
- Which internal APIs (in src/) to document vs skip
- Level of detail for template parameter documentation
- Module overview length and structure
- Which modules need overview pages vs which to skip
- When to link between module overviews
- Detail level for boilerplate documentation on undocumented APIs

## Specific Ideas

None — open to standard approaches for documentation improvement.

## Deferred Ideas

None — discussion stayed within phase scope.

---

*Phase: 09-documentation-coverage*
*Context gathered: 2026-02-03*
