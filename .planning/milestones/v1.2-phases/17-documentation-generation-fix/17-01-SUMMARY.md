---
phase: 17-documentation-generation-fix
plan: 01
subsystem: api
tags: [xml2js, doxygen, documentation, markdown, nodejs]

# Dependency graph
requires:
  - phase: 16-repository-cleanup
    provides: compat headers deleted (ensures compat config removal is safe)
  - phase: 14-fix-extracttext-cross-references
    provides: extractText() with $ attribute filter (preserved and extended)
provides:
  - xml2js ordered parsing options (preserveChildrenOrder, charsAsChildren, explicitChildren)
  - extractText() walking $$ children arrays in document order
  - formatMethod() producing single const qualifier
  - compat module entry removed from config.modules
affects: [17-02, documentation-generation]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "xml2js ordered parsing: explicitChildren + preserveChildrenOrder + charsAsChildren for correct mixed-content document order"
    - "extractText() primary path: $$ array traversal; fallbacks: _.property then Object.entries with $/#name/$$ filtered"
    - "formatMethod() const deduplication: strip argsstring trailing const when $.const=yes before appending"

key-files:
  created: []
  modified:
    - scripts/generate-api-docs.js

key-decisions:
  - "Use $$ array as primary extractText() path when available — not Object.entries() — to preserve document order in mixed content nodes"
  - "Keep existing $ attribute filter in Object.entries fallback (Phase 14 fix) and extend it to also filter #name and $$"
  - "Strip trailing ' const' from argsstring (6 chars) rather than conditionally skipping the append — single const at end is the source of truth"
  - "Remove compat config entry from config.modules to prevent regeneration from recreating deleted Phase 16 content"

patterns-established:
  - "Pattern: xml2js ordered parsing requires all three options together — preserveChildrenOrder alone does not include text nodes in $$ arrays"

requirements-completed: [DOCG-01, DOCG-02, DOCG-04]

# Metrics
duration: 6min
completed: 2026-02-23
---

# Phase 17 Plan 01: Documentation Generation Fix Summary

**xml2js ordered parsing with $$ children traversal in extractText() and formatMethod() const deduplication to fix cross-reference garbling and const const duplication**

## Performance

- **Duration:** 6 min
- **Started:** 2026-02-23T15:35:16Z
- **Completed:** 2026-02-23T15:41:00Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments

- Enabled xml2js ordered parsing (explicitChildren + preserveChildrenOrder + charsAsChildren) in parseXmlFile()
- Rewrote extractText() to walk $$ children arrays in document order, fixing cross-reference text garbling (e.g. "SpriteSprite" -> "Sprite class for bitmap image rendering (matches original Enjin Sprite).")
- Fixed formatMethod() const const duplication by stripping trailing ' const' from argsstring when the const attribute is already set
- Removed stale compat module (Vector3) from config.modules to prevent docs/api/compat/ from being recreated during regeneration

## Task Commits

Each task was committed atomically:

1. **Task 1: Enable xml2js ordered parsing and rewrite extractText()** - `2cad964` (feat)
2. **Task 2: Fix formatMethod() const const duplication and remove stale compat config** - `a494fb3` (fix)

## Files Created/Modified

- `scripts/generate-api-docs.js` - parseXmlFile() now uses ordered parse options; extractText() walks $$ arrays; formatMethod() strips duplicate const; compat config removed

## Decisions Made

- Use $$ array as primary extractText() path: document order is preserved only when walking $$ — Object.entries() approach (even with correct $ filter) loses interleaved text ordering
- Keep all three xml2js parse options together: preserveChildrenOrder creates $$ arrays, charsAsChildren includes text nodes in those arrays, explicitChildren enables the feature — any subset would produce incomplete $$ arrays
- Extend fallback filter from `key !== '$'` to `key !== '$' && key !== '#name' && key !== '$$'` — ordered parsing introduces new keys that must be excluded from the fallback path
- Strip 6 chars from argsstring (slice(0, -6)) rather than conditionally skipping the append — keeps the trailing `${isConst ? ' const' : ''}` as the single authoritative source

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

The plan's verification script uses shell-escaped regex `/node\.\$\$/` which, when passed via bash `-e`, loses the dollar-sign escaping and fails to match even though the code is correct. Verified using a file-based approach and direct grep — the pattern `node.$$` is present in the file as required.

## Next Phase Readiness

- scripts/generate-api-docs.js is ready for regeneration run (Phase 17 Plan 02)
- All three code-level bugs fixed: ordered parsing, extractText(), formatMethod()
- compat config removed — safe to regenerate without recreating deleted content

---
*Phase: 17-documentation-generation-fix*
*Completed: 2026-02-23*
