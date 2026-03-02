# Phase 58: Documentation and Build Tooling - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Update the Getting Started guide to remove stale Canvas8 references and mention the SDL3 runner. Write two new tutorials ("Your First Script" and "Async Coroutines"). Add Lua syntax highlighting to Docusaurus. No new engine features — documentation only.

</domain>

<decisions>
## Implementation Decisions

### Tutorial audience and tone
- Target reader: **knows game dev basics, new to this engine** — don't explain what a game loop is, do explain how this engine's Lua scripting model works
- Concise, practical tone: engine API first, enough context to understand what to do next

### "Your First Script" tutorial structure
- **Concept-first**: Introduce the engine.* API concepts (state, input, graphics, time), then reference `tamagotchi.lua` snippets to illustrate each concept
- Do NOT walk through tamagotchi.lua line-by-line — pull relevant excerpts as examples
- Goal: reader understands how to write a working Lua script for this engine

### "Async Coroutines" tutorial scope
- Cover **all three** async primitives: `engine.async.wait()`, `engine.async.wait_frames()`, `engine.tween.await()`
- All belong to the same coroutine model — one tutorial is more useful than splitting
- Include a small working example that demonstrates each primitive in context

### Claude's Discretion
- Getting Started update extent: Remove Canvas8_128x64 references, add SDL3 context — Claude decides whether to rewrite the C++ example section or just patch stale mentions
- Sidebar placement for new tutorials (new "Tutorials" category or under "Guides")
- Tutorial filenames and URL slugs
- Whether to add an index/overview of the Lua API alongside the tutorials

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `scripts/tamagotchi.lua` (210 lines): Full working Lua script using `engine.config`, `engine.state`, `engine.input`, `engine.graphics`, `engine.time` — source for "Your First Script" code snippets
- `docs/src/getting-started.md`: Existing file to update — remove `Canvas8_128x64` (line 39), add SDL3 runner reference

### Established Patterns
- Docusaurus 3.9.2 with `prism-react-renderer ^2.3.0` — Prism 1.29+ includes Lua support; just add `'lua'` to `additionalLanguages` in `docusaurus.config.js`
- Current `additionalLanguages`: `['cpp', 'cmake', 'bash']` — add `'lua'` here
- Sidebar defined in `sidebars.js` — add new category entry for tutorials

### Integration Points
- `docs/docusaurus.config.js` prism config: Add `'lua'` to `additionalLanguages` (one-line change)
- `docs/sidebars.js`: Add tutorials category with two new pages
- `docs/src/`: New tutorial files go here alongside existing guides

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 58-documentation-and-build-tooling*
*Context gathered: 2026-03-02*
