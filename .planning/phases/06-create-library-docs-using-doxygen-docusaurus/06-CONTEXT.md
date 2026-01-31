# Phase 6: Create library docs, using doxygen + Docusaurus - Context

**Gathered:** 2026-01-31
**Status:** Ready for planning

<domain>
## Phase Boundary

Create comprehensive library documentation using Doxygen for API extraction and Docusaurus for modern web presentation, deployed to GitHub Pages.

This phase delivers:
- Doxygen configuration extracting XML from C++ headers
- Docusaurus site with enjin2 branding and navigation
- Initial documentation content (intro, getting started, architecture)
- API reference pages (Doxygen auto-generated)
- CI/CD pipeline deploying to GitHub Pages

Scope anchor: Setup documentation pipeline and create initial content. Improving source comments for better documentation quality is part of this phase.
</domain>

<decisions>
## Implementation Decisions

### Reader journey & navigation flow
- Getting Started is first page users see (skip intro/landing page)
- Minimal Getting Started: Clone → Configure → Build + 1-line example (3 steps)
- Topic-based exploration after Getting Started (not linear tutorial path)
- Navigation: Sidebar hierarchy for browsing APIs by module, search bar in top for known lookups
- Cross-linking: Minimal but strategic (link to next logical step, not every mention)
- Platform-specific guides will be separate later (keep Getting Started minimal)
- Both sidebar and search bar needed - sidebar for overview/browsing, search for when you know what you want

### Code examples depth
- Examples illustrate concepts (minimal snippets showing 'here's how X works')
- Just the code - minimal explanation, let code speak (reference style)
- Examples embedded in text (not collapsible)
- Keep it simple overall
- Examples not needed for everything right now
- Single concept per example (C_Position, then Scene, etc.)

### Target audience
- Primary: Experienced C++ developers
- Internal team use initially (not public-facing from day 1)
- Minimal handling of non-primary audiences (beginners, embedded folks)
- Focus: Show usage patterns - how enjin2 components typically used together internally
- Internal-first documentation structure, evolve to public layer when open-sourcing
- What matters: Usage patterns over quick API lookup or consistency with code

### API reference approach
- Initial API reference: Doxygen auto-generated
- Improve Doxygen comments now (add @brief, @param, @return)
- This is part of Phase 6 (not separate phase)
- Detail level: Minimal but complete (signatures + brief descriptions, no long explanations)
- Organization: By module (Components, Scenes, Graphics, etc.) not alphabetical A-Z

### Tone & style
- Tone: Practical / concise (direct, utility-focused, get-to-the-point)
- Explanation length: One sentence per concept (maximum brevity)
- Advanced topics: Inline with complexity warnings (not separate sections)
- Formatting: Short paragraphs (1-2 sentence blocks, white space)
- Assume readers fill gaps (very concise)

### Content completeness
- Document all public APIs now (comprehensive first pass)
- Code examples: Examples for major patterns (assume experienced devs extrapolate)
- Guides: Getting Started + Architecture only (minimal guide set initially)
- 'Done' definition: Working site with all content (intro, guides, API reference), builds and deploys successfully
- No placeholder TODOs - all listed content exists and works

### Claude's Discretion
- exact sidebar structure and ordering
- specific navigation layout (mobile vs desktop)
- search bar implementation details
- Docusaurus theme customization depth
- exact formatting of code examples (syntax highlighting, line numbers, etc.)
- specific cross-linking strategy (which terms to link)
</decisions>

<specifics>
## Specific Ideas

- "One sentence per concept" - maximum brevity, assume experienced readers
- "Getting Started in 3 steps" - Clone → Configure → Build + 1-line example
- "Topic-based exploration" - users pick what they need: Component system, Scene management, etc.
- "Both sidebar and search" - sidebar for overview, search for when you know what you want
- "Minimal but strategic cross-linking" - link to next logical step, not every mention
- "Examples not needed for everything" - only major patterns need examples

</specifics>

<deferred>
## Deferred Ideas

- Platform-specific guides (ESP32-S3, Desktop, WASM) - separate phase
- Comprehensive tutorial series - beyond minimal guides
- Interactive API playground or examples - future enhancement
- Video tutorials or walkthroughs - separate phase
- Community-contributed examples section - future phase
</deferred>

---

*Phase: 06-create-library-docs-using-doxygen-docusaurus*
*Context gathered: 2026-01-31*
