# Phase 14: Fix extractText() Cross-Reference Rendering - Research

**Researched:** 2026-02-23
**Domain:** xml2js DOM traversal / Doxygen XML text extraction
**Confidence:** HIGH

## Summary

The `extractText()` function in `scripts/generate-api-docs.js` (lines 105-115) iterates all `Object.values()` of xml2js-parsed nodes, which includes the `$` attribute object that xml2js uses to store XML element attributes. When Doxygen XML contains `<ref refid="classenjin2_1_1Sprite" kindref="compound">Sprite</ref>`, xml2js parses this into `{ _: "Sprite", $: { refid: "classenjin2_1_1Sprite", kindref: "compound" } }`. The current `extractText()` recurses into `$`, producing `Pointstructenjin2_1_1Pointcompound` instead of `Point`.

This affects three distinct garbling patterns visible in generated markdown: (1) cross-reference type names in method return types and parameters, (2) `parameterlist` `$` attributes leaking `param`/`return` kind strings, and (3) `simplesect` `$` attributes leaking `return`/`note` kind strings. All three share the same root cause. The fix is a single-line addition to `extractText()` to skip the `$` key.

**Primary recommendation:** Add a `$` key filter to `extractText()` so it skips xml2js attribute objects, then regenerate all 86 API markdown pages.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DOC-02 | All public APIs documented with Doxygen comments (complete coverage) | extractText() fix produces clean human-readable text in all 86 API pages; regeneration covers all classes/namespaces/modules |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| xml2js | (existing) | Parse Doxygen XML into JS objects | Already used by project since Phase 6 |
| Node.js | (existing) | Run generate-api-docs.js | Already used |

### Supporting
No new libraries needed. This is a fix to existing code.

## Architecture Patterns

### Pattern 1: xml2js Object Structure
**What:** xml2js represents XML element attributes in a `$` property and text content in a `_` property. Child elements become named array properties.
**Confidence:** HIGH (verified by direct testing)

For `<ref refid="classenjin2_1_1Sprite" kindref="compound">Sprite</ref>`, xml2js produces:
```javascript
{
  "_": "Sprite",           // text content
  "$": {                   // XML attributes
    "refid": "classenjin2_1_1Sprite",
    "kindref": "compound"
  }
}
```

For `<parameterlist kind="param">`, xml2js produces:
```javascript
{
  "$": { "kind": "param" },    // leaked as "param" in output
  "parameteritem": [...]
}
```

For `<simplesect kind="return">`, xml2js produces:
```javascript
{
  "$": { "kind": "return" },   // leaked as "return" in output
  "para": [...]
}
```

### Pattern 2: The Fix
**What:** Skip the `$` key when iterating object properties in `extractText()`
**Confidence:** HIGH

Current broken code (line 105-115):
```javascript
function extractText(node) {
  if (!node) return '';
  if (typeof node === 'string') return node;
  if (Array.isArray(node)) {
    return node.map(extractText).join('');
  }
  if (typeof node === 'object') {
    return Object.values(node).map(extractText).join('');
  }
  return '';
}
```

Fixed code:
```javascript
function extractText(node) {
  if (!node) return '';
  if (typeof node === 'string') return node;
  if (Array.isArray(node)) {
    return node.map(extractText).join('');
  }
  if (typeof node === 'object') {
    return Object.entries(node)
      .filter(([key]) => key !== '$')
      .map(([, value]) => extractText(value))
      .join('');
  }
  return '';
}
```

### Pattern 3: formatType() Also Affected
**What:** `formatType()` (line 158-171) falls through to `extractText()` for non-string types. When a `<type>` element contains a `<ref>`, it produces garbled return types.
**Confidence:** HIGH (verified in Sprite.md line 143: `Pointstructenjin2_1_1Pointcompound`)

The fix to `extractText()` automatically fixes `formatType()` since it delegates to `extractText()`.

### Anti-Patterns to Avoid
- **Filtering only in specific callers:** Do NOT add `$` filtering in `processClass`, `formatType`, etc. individually. The fix belongs in `extractText()` itself since it's the central text extraction function.
- **Stripping by regex post-hoc:** Do NOT try to regex-strip refid patterns from output strings. That's fragile and misses future XML attribute patterns.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| XML attribute filtering | Per-caller attribute stripping | Single `$` key filter in extractText() | One fix point, covers all current and future callers |

## Common Pitfalls

### Pitfall 1: Missing the `_` Property
**What goes wrong:** xml2js stores mixed content text in the `_` property. If you filter to only extract `_`, you miss child element text.
**Why it happens:** Misunderstanding xml2js structure — `_` is text content, named properties are child elements.
**How to avoid:** Filter OUT `$` only; keep `_` and all other properties.
**Warning signs:** Descriptions become empty strings.

### Pitfall 2: The `const const` Duplication
**What goes wrong:** `formatMethod()` reads `argsstring` (which already contains `const`) AND checks `$.const === 'yes'` to append ` const`. Result: `const const`.
**Why it happens:** Doxygen's `argsstring` includes trailing `const` (e.g., `() const`), and the code also checks the `const` attribute separately.
**How to avoid:** This is a secondary issue. If in scope, either remove the `$.const` append or strip `const` from `argsstring` before appending.
**Warning signs:** Every const method shows `const const` in output.

### Pitfall 3: Forgetting to Regenerate Pages
**What goes wrong:** Fixing the code but not re-running the generator leaves old garbled pages in place.
**Why it happens:** Generated markdown files are committed to git; they don't auto-regenerate.
**How to avoid:** Run `node scripts/generate-api-docs.js` after the fix and commit updated markdown files.
**Warning signs:** `git diff` shows no changes to docs/api/ after code fix.

### Pitfall 4: Parameter Documentation Formatting
**What goes wrong:** Even after `$` fix, `extractText()` on `detaileddescription` produces flat text like `texture_dataPointer to texture bitmap datawWidth in pixels` — parameter names and descriptions run together without formatting.
**Why it happens:** `extractText()` is a flat text joiner with no semantic awareness of Doxygen's `<parameterlist>` structure.
**How to avoid:** This is a content quality improvement beyond the `$` fix. Could add semantic extraction for `parameterlist` and `simplesect` elements to produce formatted parameter/return documentation. Consider if this is in scope.
**Warning signs:** Parameters render as run-together text.

## Code Examples

### Verified: Current extractText() Bug
```javascript
// Input XML: <ref refid="classenjin2_1_1Sprite" kindref="compound">Sprite</ref>
// xml2js output: { _: "Sprite", $: { refid: "classenjin2_1_1Sprite", kindref: "compound" } }
// Current extractText() → "Pointstructenjin2_1_1Pointcompound"
// Fixed extractText()  → "Point"
```

### Verified: parameterlist $-leak
```javascript
// Input XML: <parameterlist kind="param"><parameteritem>...
// xml2js $.kind = "param" leaks into output
// Current extractText() → "paramxX coordinate"
// Fixed extractText()  → "xX coordinate"
```

### Verified: simplesect $-leak
```javascript
// Input XML: <simplesect kind="return"><para>Current position</para></simplesect>
// xml2js $.kind = "return" leaks into output
// Current extractText() → "returnCurrent position"
// Fixed extractText()  → "Current position"
```

## Affected Files Inventory

| File | Change Type | Details |
|------|-------------|--------|
| `scripts/generate-api-docs.js` | Code fix | `extractText()` function (~3 line change) |
| `docs/api/**/*.md` (86 files) | Regenerated | All API markdown pages regenerated with clean text |

## Scope Assessment

### Minimum viable fix (closes INT-01)
1. Add `$` key filter to `extractText()`
2. Regenerate all pages
3. Verify no garbled refid/kindref strings remain

### Optional quality improvements (if time permits)
- Fix `const const` duplication in `formatMethod()`
- Add structured parameter formatting (instead of flat text joining)
- Add structured return value formatting

**Recommendation:** The minimum viable fix is sufficient to close INT-01 and satisfy the phase success criteria. The `$` filter is a 3-line change. The optional improvements are nice-to-have but not required by DOC-02.

## Open Questions

1. **Parameter formatting quality**
   - What we know: After `$` fix, parameters still render as flat text (e.g., `texture_dataPointer to texture bitmap data`)
   - What's unclear: Whether flat parameter text is acceptable for DOC-02, or if structured formatting is needed
   - Recommendation: Flat text is vastly better than garbled text. Accept flat text for this phase; structured formatting can be a future improvement.

## Sources

### Primary (HIGH confidence)
- Direct testing of xml2js parse output with `node -e` on actual Doxygen XML patterns
- `scripts/generate-api-docs.js` source code analysis
- `docs/xml/classenjin2_1_1Sprite.xml` — representative Doxygen XML with `<ref>` elements
- `docs/api/graphics/Sprite.md` — representative garbled output page
- `.planning/v1.1-MILESTONE-AUDIT.md` — INT-01 gap definition and 63/76 affected pages count

### Secondary (MEDIUM confidence)
- xml2js documentation (well-known `$` and `_` conventions)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new libraries, single-file fix
- Architecture: HIGH - root cause verified with direct testing, fix confirmed to work
- Pitfalls: HIGH - all patterns verified empirically with node -e tests

**Research date:** 2026-02-23
**Valid until:** No expiration — xml2js `$`/`_` conventions are stable
