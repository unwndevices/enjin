# Phase 17: Documentation Generation Fix - Research

**Researched:** 2026-02-23
**Domain:** xml2js DOM traversal / Doxygen XML text extraction / Docusaurus markdown generation
**Confidence:** HIGH

## Summary

Phase 17 fixes four remaining documentation generation bugs in `scripts/generate-api-docs.js`. Two bugs share a root cause in xml2js's default parsing mode, which collapses mixed content (interleaved text and child elements) into separate properties, destroying document order. This causes cross-reference text to fuse words together (e.g., "SpriteSprite" instead of "Sprite class for... Sprite"). A third bug is in `formatMethod()`, which appends `const` from the XML attribute while `argsstring` already contains it, producing `const const`. The fourth bug is the stale `compat` module entry in the config that would recreate deleted content during regeneration.

The fix requires: (1) switching xml2js parse options to `preserveChildrenOrder: true` + `charsAsChildren: true`, (2) rewriting `extractText()` to walk the ordered `$$` children array instead of `Object.entries()`, (3) fixing `formatMethod()` to avoid `const` duplication, (4) removing the stale `compat` config entry, and (5) regenerating all API markdown files.

**Primary recommendation:** Enable xml2js ordered parsing and rewrite `extractText()` to use `$$` children arrays, fix `const const` in `formatMethod()`, remove stale compat config, regenerate all 83+ API pages.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DOCG-01 | `extractText()` filters xml2js `$` attribute objects to prevent text garbling | `$` filter already exists (Phase 14). Remaining garbling is from mixed content ordering — fix requires `preserveChildrenOrder` parse option and `$$`-based traversal |
| DOCG-02 | `formatMethod()` eliminates `const const` duplication in method signatures | `argsstring` already contains ` const` suffix; `formatMethod()` must not append it again. 134 occurrences across 42 files |
| DOCG-03 | All API markdown files regenerated with clean output | After code fixes, run `node scripts/generate-api-docs.js` to regenerate ~83 API pages. Must also remove stale `compat` module from config first |
| DOCG-04 | Cross-reference text no longer produces fused/garbled strings | Same root cause as DOCG-01. Ordered `$$` traversal produces correct text like "Sprite class for bitmap image rendering (matches original Enjin Sprite)." |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| xml2js | 0.6.2 | Parse Doxygen XML into JS objects | Already used; `preserveChildrenOrder` + `charsAsChildren` options available since 0.4.x |
| Node.js | (existing) | Run generate-api-docs.js | Already used |
| Docusaurus | 3.9.2 | Documentation site framework | Already used for docs site |

### Supporting
No new libraries needed. All fixes use existing xml2js features.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| xml2js with ordered parsing | sax/expat stream parser | Would give perfect ordering but requires full rewrite of all XML processing — not justified for this fix |
| xml2js `$$` traversal | Regex-based post-processing | Fragile, misses edge cases, won't scale to new XML patterns |

## Architecture Patterns

### Pattern 1: xml2js Mixed Content Problem (Root Cause)
**What:** xml2js's default parse mode groups same-named child elements into arrays and stores text in `_`. For `<para><ref>Sprite</ref> class for <ref>Sprite</ref>)</para>`, it produces `{ _: " class for )", ref: [{ _: "Sprite" }, { _: "Sprite" }] }`. Document order is lost.
**Confidence:** HIGH (verified by direct testing)

Default mode output:
```javascript
// <para><ref refid="...">Sprite</ref> class for bitmap image rendering (matches original Enjin <ref refid="...">Sprite</ref>). </para>
{
  "_": " class for bitmap image rendering (matches original Enjin ). ",
  "ref": [
    { "_": "Sprite", "$": { "refid": "classenjin2_1_1Sprite", "kindref": "compound" } },
    { "_": "Sprite", "$": { "refid": "classenjin2_1_1Sprite", "kindref": "compound" } }
  ]
}
// extractText() → " class for bitmap image rendering (matches original Enjin ). SpriteSprite"
//                                                                                 ^^^^^^^^^^^^^ fused, wrong order
```

### Pattern 2: xml2js Ordered Parsing (The Fix)
**What:** With `{ explicitChildren: true, preserveChildrenOrder: true, charsAsChildren: true }`, xml2js produces a `$$` array with children in document order. `__text__` nodes represent text, element nodes represent children.
**Confidence:** HIGH (verified by direct testing)

Ordered mode output:
```javascript
// Same XML as above, with ordered options
{
  "$$": [
    { "#name": "ref", "_": "Sprite", "$": { "refid": "..." } },
    { "#name": "__text__", "_": " class for bitmap image rendering (matches original Enjin " },
    { "#name": "ref", "_": "Sprite", "$": { "refid": "..." } },
    { "#name": "__text__", "_": "). " }
  ],
  // Legacy properties also present: "_", "ref" — ignore these, use $$ only
}
```

New `extractText()` walks `$$` in order:
```javascript
function extractText(node) {
  if (!node) return '';
  if (typeof node === 'string') return node;
  if (Array.isArray(node)) return node.map(extractText).join('');
  if (typeof node === 'object') {
    // Ordered children available — walk in document order
    if (node.$$) {
      return node.$$.map(child => {
        if (child['#name'] === '__text__') return child._ || '';
        // For element children (ref, para, etc.), recurse
        return extractText(child);
      }).join('');
    }
    // Fallback: use _ property (leaf text content)
    if (node._ !== undefined) return String(node._);
    // Fallback: iterate non-$ properties
    return Object.entries(node)
      .filter(([key]) => key !== '$' && key !== '#name' && key !== '$$')
      .map(([, value]) => extractText(value))
      .join('');
  }
  return '';
}
// Result: "Sprite class for bitmap image rendering (matches original Enjin Sprite). "  ← correct!
```

### Pattern 3: const const Fix in formatMethod()
**What:** Doxygen XML `<argsstring>() const</argsstring>` already includes trailing `const`. `formatMethod()` also checks `$.const === 'yes'` and appends ` const`. Result: `() const const`.
**Confidence:** HIGH (134 occurrences across 42 files verified)

Fix: Strip trailing ` const` from `argsstring` before the method constructs its output, OR skip appending when `argsstring` already ends with `const`:
```javascript
function formatMethod(method) {
  const name = method.name[0];
  const type = formatType(method.type);
  let args = method.argsstring ? method.argsstring[0] : '()';
  const isConst = method.$.const === 'yes';
  const isStatic = method.$.static === 'yes';
  const isVirtual = method.$.virt === 'virtual';
  const isConstexpr = method.$.constexpr === 'yes';

  // Strip trailing const from argsstring to avoid duplication
  // (Doxygen includes it in argsstring AND as a separate attribute)
  if (isConst && args.endsWith(' const')) {
    args = args.slice(0, -6);
  }

  let modifiers = [];
  if (isStatic) modifiers.push('static');
  if (isVirtual) modifiers.push('virtual');
  if (isConstexpr) modifiers.push('constexpr');

  const modifiersStr = modifiers.length > 0 ? modifiers.join(' ') + ' ' : '';
  return escapeForMdx(`${modifiersStr}${type} ${name}${args}${isConst ? ' const' : ''}`);
}
```

### Pattern 4: Stale compat Module Removal
**What:** `scripts/generate-api-docs.js` lines 25-28 still declare a `compat` module with class `Vector3`. Phase 16 deleted the source headers. Running the generator will recreate `docs/api/compat/` directory with stale/empty content.
**Confidence:** HIGH (identified in v1.2 milestone audit)

Fix: Remove the `compat` entry from `config.modules` before regeneration.

### Anti-Patterns to Avoid
- **Partial parser option change:** Do NOT add `preserveChildrenOrder` without also adding `charsAsChildren`. Both are needed: `preserveChildrenOrder` creates `$$` arrays, `charsAsChildren` includes text nodes in those arrays.
- **Dual traversal paths:** Do NOT keep the old `Object.entries()` path alongside the new `$$` path in `extractText()`. The `$$` path should be primary. Only fall back for leaf nodes without `$$`.
- **Regex stripping of fused text:** Do NOT try to fix garbled output post-hoc with regex patterns. Fix the extraction at the source.
- **Removing the `$` filter:** The existing `$` key filter (from Phase 14) remains important as a fallback for any node without `$$` children.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Ordered mixed content | Custom XML parser | xml2js `preserveChildrenOrder` + `charsAsChildren` | Built-in feature, well-tested, no new dependencies |
| const deduplication | Regex post-processing of signatures | Conditional stripping in formatMethod() | Clean, targeted, doesn't mask other bugs |

**Key insight:** xml2js already has the features needed to preserve document order. The issue was that the original implementation used default options which collapse mixed content. Switching parse options is a configuration change, not an architecture change.

## Common Pitfalls

### Pitfall 1: Breaking Existing Working Descriptions
**What goes wrong:** Changing `extractText()` to walk `$$` arrays could break descriptions that worked fine with the old approach (e.g., plain text paragraphs without cross-references).
**Why it happens:** `$$` structure is slightly different from default structure. Some nodes may not have `$$` if they're simple strings.
**How to avoid:** Keep fallback paths: if no `$$`, check for `_` property, then fall back to `Object.entries()` with `$` filter.
**Warning signs:** Previously correct descriptions become empty or truncated after the fix.

### Pitfall 2: The `explicitChildren` Side Effects
**What goes wrong:** `explicitChildren: true` changes the overall parse tree structure. Properties that were previously direct children may now be nested differently.
**Why it happens:** With `explicitChildren`, child elements are ONLY in `$$`, not as named properties (unless `preserveChildrenOrder` also keeps them — it does in xml2js 0.6.x).
**How to avoid:** In xml2js 0.6.x, both `$$` (ordered) AND named properties (grouped) are present simultaneously. Existing code using `compound.sectiondef`, `member.name[0]`, etc. continues to work. Only `extractText()` needs to change.
**Warning signs:** `compound.sectiondef` becomes undefined or structured differently.

### Pitfall 3: Recreating compat Directory
**What goes wrong:** Running `node scripts/generate-api-docs.js` without removing the compat config entry recreates `docs/api/compat/` directory, partially undoing Phase 16 cleanup.
**Why it happens:** The generator creates output directories for all entries in `config.modules`.
**How to avoid:** Remove compat entry from `config.modules` BEFORE running the generator.
**Warning signs:** `git diff` shows new files in `docs/api/compat/`.

### Pitfall 4: Parameter Names Still Fused With Descriptions
**What goes wrong:** Even with ordered parsing, `detaileddescription` containing `<parameterlist>` will produce parameter names concatenated with descriptions: `texture_dataPointer to texture bitmap data`.
**Why it happens:** `extractText()` is a flat text joiner — it has no semantic awareness of `parameterlist` structure. Parameter names and descriptions are just text nodes.
**How to avoid:** This is a known limitation documented in Phase 14 research as "future work (Phase 15)". For DOCG-04 scope, the key criterion is that cross-reference text renders as "readable, properly-spaced strings (no fused words)". Parameter name/description joining is a separate formatting concern not listed in Phase 17 requirements. If the phase success criteria specifically mention this, add space-insertion heuristics for `parametername` → `parameterdescription` boundaries.
**Warning signs:** Detailed descriptions still show `nameDescription` patterns.

### Pitfall 5: formatType() Interaction with New extractText()
**What goes wrong:** `formatType()` falls through to `extractText(type)` for non-string types. If `extractText()` changes behavior, return type rendering could break.
**Why it happens:** `formatType()` receives xml2js-parsed type nodes that may contain `<ref>` elements.
**How to avoid:** The new `extractText()` should produce BETTER results for type nodes (correct ordering). Verify that return types like `Point` (which are `<ref>` elements) still render correctly.
**Warning signs:** Return types become empty or show extra text.

## Code Examples

### Verified: Current Garbled Output (Sprite.md line 9)
```
 class for bitmap image rendering (matches original Enjin ). SpriteSprite
```
Expected after fix:
```
Sprite class for bitmap image rendering (matches original Enjin Sprite).
```

### Verified: const const Duplication (Sprite.md line 111)
```
### `const uint8_t * GetTexture() const const`
```
Expected after fix:
```
### `const uint8_t * GetTexture() const`
```

### Verified: Fused Parameter Text (Sprite.md line 35)
```
texture_dataPointer to texture bitmap data wWidth in pixels hHeight in pixels blend_modeBlend mode for compositing
```
Note: This is a separate formatting issue (flat text joining of parameterlist). Phase 17 DOCG-04 focuses on cross-reference garbling. Parameter formatting was deferred to a future phase.

### Verified: Ordered Parsing Produces Correct Output
```javascript
// Tested with: node -e "<test script with preserveChildrenOrder options>"
// Input: Sprite class briefdescription XML
// Output: "Sprite class for bitmap image rendering (matches original Enjin Sprite)."
// Correct document order restored.
```

## Affected Files Inventory

| File | Change Type | Details |
|------|-------------|---------|
| `scripts/generate-api-docs.js` | Code fix | (1) Add parser options, (2) rewrite `extractText()`, (3) fix `formatMethod()`, (4) remove compat config |
| `docs/api/**/*.md` (~83 files) | Regenerated | All API markdown pages regenerated with clean text |

## Scope Assessment

### Required (addresses all DOCG-* requirements)
1. Add `preserveChildrenOrder` + `charsAsChildren` to xml2js parse options
2. Rewrite `extractText()` to walk `$$` arrays in document order
3. Fix `formatMethod()` const const duplication
4. Remove stale `compat` module from config.modules
5. Regenerate all API pages
6. Verify Docusaurus build passes

### Out of Scope
- Structured parameter formatting (param name/description separation) — deferred, not in DOCG-* requirements
- Return value formatting improvements — not in scope
- New documentation content or examples — explicitly out of scope per REQUIREMENTS.md

## Open Questions

1. **Parameter text concatenation**
   - What we know: After ordered parsing fix, parameter names and descriptions still join without separators (e.g., `texture_dataPointer to...`). This is because `extractText()` is a flat text joiner.
   - What's unclear: Whether DOCG-04 "readable, properly-spaced strings (no fused words)" encompasses parameter formatting or only cross-reference text.
   - Recommendation: DOCG-04 specifically says "Cross-reference text" — focus on ref-based garbling. Parameter formatting is a separate concern. If needed, it can be addressed with minimal space-insertion logic for `parametername`→`parameterdescription` boundaries, but this should be scoped carefully.

2. **`$$` availability in all node types**
   - What we know: With ordered parsing options, `$$` arrays appear on nodes that have children. Pure text nodes remain strings. Nodes accessed via named properties (like `compound.sectiondef`) still work as before.
   - What's unclear: Whether any edge cases exist where `$$` is absent on a node that previously had extractable text.
   - Recommendation: Keep fallback logic in `extractText()`. Test with the full XML corpus by regenerating all pages and diffing against current output.

## Sources

### Primary (HIGH confidence)
- Direct testing of xml2js 0.6.2 parse output with `preserveChildrenOrder` + `charsAsChildren` options on actual Doxygen XML
- `scripts/generate-api-docs.js` source code analysis (525 lines)
- `docs/xml/classenjin2_1_1Sprite.xml` — verified garbling root cause and fix
- `.planning/v1.2-MILESTONE-AUDIT.md` — cross-phase integration concern (stale compat config)
- `.planning/milestones/v1.1-phases/14-fix-extracttext-cross-references/14-01-SUMMARY.md` — Phase 14 fix scope and remaining issues

### Secondary (MEDIUM confidence)
- xml2js documentation — `preserveChildrenOrder`, `charsAsChildren`, `explicitChildren` options are stable and well-documented since 0.4.x

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new libraries, xml2js options verified by direct testing
- Architecture: HIGH - ordered parsing approach verified to produce correct output, all four bugs understood with root causes
- Pitfalls: HIGH - interactions between parser options and existing code verified empirically

**Research date:** 2026-02-23
**Valid until:** No expiration — xml2js ordered parsing options are stable
