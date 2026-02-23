# Phase 15: Cleanup CI and README Tech Debt - Research

**Researched:** 2026-02-23
**Domain:** CI pipeline optimization, README metadata
**Confidence:** HIGH

## Summary

Phase 15 closes the final two tech debt items identified in the v1.1 milestone audit: a duplicate invocation of `generate-api-docs.js` in the CI pipeline, and a TBD placeholder in the README license badge.

The duplicate invocation occurs because the CMake `docs` target already runs `node scripts/generate-api-docs.js` as its second command (after Doxygen), but the CI workflow at `.github/workflows/docs.yml` line 78 runs it again explicitly as a separate step. The same duplication exists in `scripts/deploy-docs.sh` at line 18. Both need the explicit invocation removed.

The license is MIT, as declared in `library.json` line 17. The README badge (line 3) and license section (lines 106-108) both need updating.

**Primary recommendation:** Remove the explicit `node scripts/generate-api-docs.js` step from `docs.yml` and `deploy-docs.sh`, and replace the TBD license badge and placeholder text with MIT.

## Standard Stack

Not applicable -- this phase modifies existing YAML, shell, and markdown files only. No new libraries or tools.

## Architecture Patterns

### Pattern 1: CMake docs target as single source of truth

**What:** The CMake `docs` target in `CMakeLists.txt` (line 18-24) chains two commands: (1) run Doxygen, (2) run generate-api-docs.js. This means `cmake --build . --target docs` produces both Doxygen XML and Docusaurus markdown in a single invocation.

**Current state in CMakeLists.txt:**
```cmake
add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
    COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-api-docs.js
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen and Docusaurus"
    BYPRODUCTS ${DOXYGEN_OUT}/index.xml
)
```

**Implication:** Any caller that runs `cmake --build . --target docs` already gets generate-api-docs.js execution. The explicit `node scripts/generate-api-docs.js` lines in `docs.yml` and `deploy-docs.sh` are redundant.

### Pattern 2: Shields.io badge format

**What:** README badges use shields.io static badge URLs.

**Current (line 3):**
```
![License](https://img.shields.io/badge/license-TBD-green)
```

**Corrected:**
```
![License](https://img.shields.io/badge/license-MIT-green)
```

### Anti-Patterns to Avoid
- **Removing generate-api-docs.js from CMake target instead of from CI:** The CMake target is the correct single source of truth. The CI step is the duplicate. Do NOT remove it from CMakeLists.txt.
- **Creating a LICENSE file in this phase:** The success criteria only mention the badge and placeholder text. Creating a LICENSE file is a separate concern (the user has not committed to doing it yet). Only fix what the criteria require.

## Don't Hand-Roll

Not applicable -- no custom solutions needed.

## Common Pitfalls

### Pitfall 1: Removing from wrong location
**What goes wrong:** Removing generate-api-docs.js from CMakeLists.txt instead of from the CI workflow and deploy script would break the `cmake --build . --target docs` workflow.
**Why it happens:** Misidentifying which invocation is the "original" vs the "duplicate."
**How to avoid:** The CMake target is the canonical location (established in Phase 6, decision 06-04). Remove from docs.yml and deploy-docs.sh only.
**Warning signs:** If `cmake --build . --target docs` no longer produces API markdown files, the wrong invocation was removed.

### Pitfall 2: License mismatch
**What goes wrong:** Using a license other than MIT in the badge.
**Why it happens:** Not checking existing project metadata.
**How to avoid:** `library.json` declares `"license": "MIT"` at line 17. Use MIT consistently.

### Pitfall 3: Forgetting deploy-docs.sh
**What goes wrong:** Only fixing docs.yml but leaving the duplicate in deploy-docs.sh.
**Why it happens:** The audit only explicitly mentions CI pipeline, but deploy-docs.sh has the same pattern.
**How to avoid:** Fix both files. deploy-docs.sh line 18 also runs generate-api-docs.js after the CMake docs target.

## Code Examples

### CI workflow fix (docs.yml)

Remove the "Generate API documentation" step entirely (lines 76-78):

**Before:**
```yaml
      - name: Generate API documentation
        run: |
          node scripts/generate-api-docs.js

      - name: Build Docusaurus site
```

**After:**
```yaml
      - name: Build Docusaurus site
```

### deploy-docs.sh fix

Remove lines 17-18 (`echo "Generating API documentation..."` and `node scripts/generate-api-docs.js`):

**Before:**
```bash
cmake --build . --target docs
cd ..

echo "Generating API documentation..."
node scripts/generate-api-docs.js

echo "Building Docusaurus site..."
```

**After:**
```bash
cmake --build . --target docs
cd ..

echo "Building Docusaurus site..."
```

### README badge fix

**Line 3 change:**
```
![License](https://img.shields.io/badge/license-MIT-green)
```

**Lines 106-108 change:**
```
## License

MIT License
```

## State of the Art

Not applicable -- straightforward file edits.

## Open Questions

1. **Should a LICENSE file be created?**
   - What we know: No LICENSE file exists in the repository root. `library.json` declares MIT. The README has a placeholder license section.
   - What's unclear: Whether the user wants a full MIT LICENSE file created in this phase.
   - Recommendation: The success criteria only require the badge to use the correct license. Updating the badge and README section text to say "MIT" satisfies the criteria. Creating a LICENSE file is additional scope the planner can include as an optional task, but it is not required by the success criteria.

## Affected Files

| File | Change | Lines |
|------|--------|-------|
| `.github/workflows/docs.yml` | Remove "Generate API documentation" step | Lines 76-78 |
| `scripts/deploy-docs.sh` | Remove redundant generate-api-docs.js call | Lines 17-18 |
| `README.md` | Update license badge from TBD to MIT | Line 3 |
| `README.md` | Update license section text | Lines 106-108 |

## Sources

### Primary (HIGH confidence)
- `.github/workflows/docs.yml` -- directly inspected CI pipeline, confirmed duplicate at line 78
- `CMakeLists.txt` lines 18-24 -- confirmed generate-api-docs.js runs inside cmake docs target
- `scripts/deploy-docs.sh` lines 14-18 -- confirmed same duplication pattern
- `library.json` line 17 -- confirms `"license": "MIT"`
- `README.md` lines 1-3 and 106-108 -- confirmed TBD placeholder badge and empty license section
- `.planning/v1.1-MILESTONE-AUDIT.md` -- audit identified both tech debt items

## Metadata

**Confidence breakdown:**
- CI duplicate identification: HIGH - directly verified in source files
- License determination: HIGH - library.json explicitly declares MIT
- Fix approach: HIGH - straightforward line removals and text replacements

**Research date:** 2026-02-23
**Valid until:** No expiration -- findings are based on current file contents
