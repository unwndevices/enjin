# Phase 58: Documentation and Build Tooling - Research

**Researched:** 2026-03-03
**Domain:** Docusaurus 3 documentation authoring — Lua syntax highlighting, tutorial structure, sidebar configuration
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Tutorial audience and tone**
- Target reader: knows game dev basics, new to this engine — don't explain what a game loop is, do explain how this engine's Lua scripting model works
- Concise, practical tone: engine API first, enough context to understand what to do next

**"Your First Script" tutorial structure**
- Concept-first: Introduce the engine.* API concepts (state, input, graphics, time), then reference `tamagotchi.lua` snippets to illustrate each concept
- Do NOT walk through tamagotchi.lua line-by-line — pull relevant excerpts as examples
- Goal: reader understands how to write a working Lua script for this engine

**"Async Coroutines" tutorial scope**
- Cover all three async primitives: `engine.async.wait()`, `engine.async.wait_frames()`, `engine.tween.await()`
- All belong to the same coroutine model — one tutorial is more useful than splitting
- Include a small working example that demonstrates each primitive in context

### Claude's Discretion
- Getting Started update extent: Remove Canvas8_128x64 references, add SDL3 context — Claude decides whether to rewrite the C++ example section or just patch stale mentions
- Sidebar placement for new tutorials (new "Tutorials" category or under "Guides")
- Tutorial filenames and URL slugs
- Whether to add an index/overview of the Lua API alongside the tutorials

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DOC-01 | Getting Started guide updated to reflect v1.7+ SDL3 runner API (no stale Canvas8 references) | Getting-started.md has one Canvas8_128x64 reference on line 39 inside a Quick Example block — replace with SDL3 runner usage |
| DOC-02 | "Your First Script" Docusaurus tutorial based on tamagotchi.lua walkthrough | tamagotchi.lua (210 lines) is fully available; covers engine.config, engine.state, engine.input, engine.graphics, engine.time |
| DOC-03 | "Async Coroutines" Docusaurus tutorial covering engine.async + engine.tween.await | bindings_async.cpp and bindings_tween.cpp document all three primitives with exact Lua API signatures |
| DOC-04 | Lua syntax highlighting enabled in Docusaurus (`'lua'` in prism additionalLanguages) | Single-line change in docs/docusaurus.config.js; prism-lua is a confirmed Prism.js built-in component |
</phase_requirements>

---

## Summary

Phase 58 is a pure documentation phase — no engine code changes. The work splits into four concrete tasks: (1) patch one stale Canvas8 mention in getting-started.md and add SDL3 runner context, (2) write a "Your First Script" tutorial drawing excerpts from tamagotchi.lua, (3) write an "Async Coroutines" tutorial covering all three async primitives, and (4) add `'lua'` to the Docusaurus prism additionalLanguages array in docusaurus.config.js.

The Docusaurus infrastructure is Docusaurus 3.9.2 with prism-react-renderer ^2.3.0. Prism 1.29+ ships `prism-lua.js` as a built-in component — adding `'lua'` to `additionalLanguages` is a single-word config change and requires no npm install. The existing sidebar structure uses `module.exports` CommonJS format with `type: 'category'` objects; new tutorials need a new "Tutorials" category entry following the same pattern.

All Lua API facts needed for tutorials are ground-truth from the source: `bindings_async.cpp` documents `engine.async.start/wait/cancel/cancelAll/wait_frames`, `bindings_tween.cpp` documents `engine.tween.await(id)`, and `tamagotchi.lua` demonstrates every `engine.*` namespace used in "Your First Script". No research gaps remain — tutorial content can be written from the codebase directly.

**Primary recommendation:** Write tutorials content-first using tamagotchi.lua and bindings source as ground truth; the infrastructure changes (Lua highlight, sidebar) are one-line each and can be bundled into a Wave 0 task.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Docusaurus | 3.9.2 | Static site generator for docs | Already installed; version locked in package.json |
| prism-react-renderer | ^2.3.0 | Code block syntax highlighting | Ships with Docusaurus classic preset; wraps Prism.js |
| Prism.js (built-in) | 1.29+ | Language grammars | Ships `prism-lua.js` — no additional install needed |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| MDX (via Docusaurus) | ^3.0.0 | Markdown with JSX | Already present; use standard Markdown for tutorials (not MDX) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| prism `additionalLanguages` | Custom Prism theme component | Overkill for a single language; `additionalLanguages` is the documented Docusaurus pattern |

**Installation:**
No npm install needed. Prism Lua grammar is included in the prism-react-renderer package that Docusaurus already depends on.

---

## Architecture Patterns

### Existing Project Structure
```
docs/
├── docusaurus.config.js   # prism.additionalLanguages lives here
├── sidebars.js            # guidesSidebar categories defined here
├── src/                   # All Markdown docs live here (routeBasePath: '/')
│   ├── getting-started.md # DOC-01: update this file
│   ├── intro.md
│   ├── architecture.md
│   └── ...
├── api/                   # Doxygen-generated API docs (separate plugin)
└── package.json
```

### Where New Tutorial Files Go
```
docs/src/
├── getting-started.md          # DOC-01: patch existing
├── tutorials/                  # New: tutorials subdirectory (recommended)
│   ├── your-first-script.md    # DOC-02
│   └── async-coroutines.md     # DOC-03
```

OR flat in `docs/src/`:
```
docs/src/
├── tutorial-your-first-script.md
├── tutorial-async-coroutines.md
```

Recommendation: use a `tutorials/` subdirectory — Docusaurus resolves doc IDs relative to `src/`, so a file at `src/tutorials/your-first-script.md` gets id `tutorials/your-first-script`. The sidebar entry would reference this full path. Flat placement is also valid if preferred (simpler IDs).

### Pattern 1: Docusaurus Sidebar Category
**What:** Add a "Tutorials" category to guidesSidebar containing both new pages
**When to use:** Two or more related docs that form a logical group

```javascript
// Source: docs/sidebars.js — current pattern + new Tutorials category
module.exports = {
  guidesSidebar: [
    'intro',
    'getting-started',
    {
      type: 'category',
      label: 'Core Concepts',
      items: ['architecture', 'components'],
    },
    {
      type: 'category',
      label: 'Graphics',
      items: ['canvas', 'sprites', 'text-rendering'],
    },
    {
      type: 'category',
      label: 'Scenes',
      items: ['scene-management', 'scene-transitions'],
    },
    // NEW:
    {
      type: 'category',
      label: 'Tutorials',
      items: [
        'tutorials/your-first-script',
        'tutorials/async-coroutines',
      ],
    },
  ],
};
```

### Pattern 2: Docusaurus Frontmatter for Docs
**What:** Every `docs/src/*.md` file uses YAML frontmatter for title and sidebar_label
**When to use:** All doc files — required for title to render correctly

```markdown
---
title: Your First Script
sidebar_label: Your First Script
---

# Your First Script
...
```

### Pattern 3: Prism Language Config
**What:** One-line addition to docusaurus.config.js to enable Lua highlighting
**When to use:** Any time a new language needs syntax highlighting

```javascript
// Source: docs/docusaurus.config.js — themeConfig.prism
prism: {
  additionalLanguages: ['cpp', 'cmake', 'bash', 'lua'],  // add 'lua'
},
```

### Anti-Patterns to Avoid
- **Checking `node_modules/prismjs/components/` before adding a language:** node_modules are not committed; `'lua'` is a confirmed Prism built-in — just add it.
- **Using JSX/MDX for tutorials:** Existing docs use plain Markdown (`.md`), not `.mdx`. Keep tutorials as `.md` files for consistency.
- **Walking tamagotchi.lua line by line:** The CONTEXT.md decision is concept-first with excerpts. Do not produce a sequential code walkthrough.
- **Splitting async primitives into separate pages:** All three (`wait`, `wait_frames`, `tween.await`) belong to the same coroutine model — one tutorial.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Lua syntax highlighting | Custom regex tokenizer, client-side JS injected in MDX | `prism additionalLanguages: ['lua']` | Prism ships the Lua grammar; configuration is one word |
| Sidebar navigation | Manual HTML nav links between tutorial pages | `sidebars.js` category entry | Docusaurus auto-generates prev/next navigation and breadcrumbs |

**Key insight:** Every infrastructure need for this phase (highlighting, navigation) is a config-only change. No custom plugin code, no MDX, no custom components.

---

## Common Pitfalls

### Pitfall 1: Forgetting to restart Docusaurus after prism config change
**What goes wrong:** Lua code blocks still render as plain text after adding `'lua'` to additionalLanguages
**Why it happens:** Docusaurus builds prism languages at startup; dev server does not hot-reload config changes
**How to avoid:** Stop `npm start`, restart — or run `npm run build && npm run serve` for production check
**Warning signs:** Code block renders without color even after config is saved

### Pitfall 2: Sidebar ID mismatch
**What goes wrong:** Docusaurus throws `onBrokenLinks: 'throw'` error for missing doc IDs
**Why it happens:** `docusaurus.config.js` sets `onBrokenLinks: 'throw'` — a sidebar reference to a doc that doesn't exist or has a wrong ID causes build failure
**How to avoid:** The doc ID is the file path relative to `docs/src/` without extension. `src/tutorials/your-first-script.md` → ID is `tutorials/your-first-script`. Verify the sidebar entry matches exactly.
**Warning signs:** Build fails with "Doc 'X' used in sidebar but not found in any doc file"

### Pitfall 3: Canvas8 reference in canvas.md (not in scope, but watch for confusion)
**What goes wrong:** Reviewer asks why canvas.md still mentions `Canvas8_128x64`
**Why it happens:** DOC-01 scope is `getting-started.md` only. canvas.md uses Canvas8 as a type name — that reference is accurate API documentation, not stale
**How to avoid:** Only patch `getting-started.md` line 39 (the Quick Example block). Leave `canvas.md` unchanged.

### Pitfall 4: Tutorial code examples using wrong Lua API
**What goes wrong:** Tutorial shows `engine.wait()` or `engine.coroutine.start()` — APIs that don't exist
**Why it happens:** Claude confuses this engine's API with other Lua/coroutine APIs
**How to avoid:** All API calls must be verified against bindings source. The correct namespaces are:
  - `engine.async.start(fn)` — not `engine.coroutine.start`
  - `engine.async.wait(seconds)` — not `coroutine.yield` or `engine.wait`
  - `engine.async.wait_frames(n)` — added Phase 57 (QOL-02)
  - `engine.tween.await(id)` — not `tween.wait` or `engine.tween.wait`

### Pitfall 5: tamagotchi.lua uses bare draw globals, not engine.graphics.*
**What goes wrong:** Tutorial explains that drawing must go through `engine.graphics.*` but the code excerpts show `clear()`, `setColor()`, `rectangle()` as bare globals
**Why it happens:** tamagotchi.lua line 3 notes: "all drawing globals are also available via engine.graphics.*" — both forms work
**How to avoid:** In "Your First Script" tutorial, acknowledge both forms. The bare globals (`clear`, `setColor`, `text`, etc.) are valid because the SDL3 runner exposes them. Don't silently switch to `engine.graphics.clear()` in tutorial code if the tamagotchi excerpts use the short form.

---

## Code Examples

Verified from codebase source files:

### SDL3 Runner — how to run a script
```bash
# Source: src/platform/sdl/sdl_main.cpp (argv parsing at lines 147-153)
./build/sdl3/enjin2_sdl --script scripts/tamagotchi.lua
./build/sdl3/enjin2_sdl --script scripts/tamagotchi.lua --fps 60
```

### Lua script structure — mandatory global functions
```lua
-- Source: scripts/tamagotchi.lua
-- Two globals the runner calls each frame:
function update(dt)
    -- dt: delta time in seconds (float, clamped to 0.05 max)
end

function draw()
    -- no arguments; draw to current layer
end
```

### engine.config — read resolution
```lua
-- Source: scripts/tamagotchi.lua line 5
local W, H = engine.config.resolution()
```

### engine.state — finite state machine
```lua
-- Source: scripts/tamagotchi.lua lines 33-36, 51-55
engine.state.switch("alive")           -- transition to named state
local cur = engine.state.current()     -- get current state name as string

if cur == "dead" then
    -- handle dead state
end
```

### engine.input — button polling
```lua
-- Source: scripts/tamagotchi.lua lines 52, 68, 78
engine.input.just_pressed(BTN.START)   -- true on first frame button is down
engine.input.just_pressed(BTN.A)
engine.input.just_pressed(BTN.LEFT)
engine.input.just_pressed(BTN.RIGHT)
-- BTN table: UP, DOWN, LEFT, RIGHT, A, B, START
```

### engine.time — elapsed time
```lua
-- Source: scripts/tamagotchi.lua line 177
engine.time.now()      -- float: total seconds since start
```

### engine.async — coroutine primitives
```lua
-- Source: src/scripting/bindings_async.cpp (file header + lua_engine_async_wait_frames)
-- Start a coroutine (returns integer ID, or nil if pool full — 8 slots max)
local id = engine.async.start(function()
    engine.async.wait(1.5)        -- suspend for 1.5 seconds
    engine.async.wait_frames(10)  -- suspend for exactly 10 frames
    -- ... continue after resume
end)

-- Cancel by ID, or cancel all
engine.async.cancel(id)
engine.async.cancelAll()
```

### engine.tween.await — wait for a tween to finish
```lua
-- Source: src/scripting/bindings_tween.cpp lines 305-342
local tid = engine.tween.to(...)         -- returns tween ID
engine.async.start(function()
    engine.tween.await(tid)              -- yields until tween completes
    -- runs after tween is done
end)
-- If tid is invalid or already complete: resumes immediately (no yield)
```

### Docusaurus config — add Lua highlighting
```javascript
// Source: docs/docusaurus.config.js (themeConfig.prism, line 117)
// Before:
additionalLanguages: ['cpp', 'cmake', 'bash'],
// After:
additionalLanguages: ['cpp', 'cmake', 'bash', 'lua'],
```

### Markdown code block with Lua highlighting
````markdown
```lua
local W, H = engine.config.resolution()

function update(dt)
    if engine.input.just_pressed(BTN.A) then
        engine.state.switch("playing")
    end
end
```
````

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Canvas8_128x64 in Quick Example | SDL3 runner with `--script` flag | v1.7 (Phase 57 complete) | getting-started.md Quick Example is stale; SDL3 runner is the canonical desktop entry point |
| No Lua highlighting | Add `'lua'` to additionalLanguages | Phase 58 | Lua code blocks will render with color |
| No tutorials | Your First Script + Async Coroutines | Phase 58 | First time this engine's Lua API is documented for new users |

**Deprecated/outdated:**
- `Canvas8_128x64` in getting-started.md Quick Example (line 39): replace with SDL3 runner invocation or a minimal Lua script example

---

## Open Questions

1. **Getting Started Quick Example replacement content**
   - What we know: Line 39 has `Canvas8_128x64 canvas;` which is the stale reference. The SDL3 runner does not expose a C++ `main()` entry point to users — users write Lua scripts.
   - What's unclear: Should the Quick Example become a Lua snippet (showing a minimal script), a shell snippet (showing `./enjin2_sdl --script`), or both?
   - Recommendation: Replace the C++ block with a two-part example: (1) shell snippet showing how to run the SDL3 binary with a script, (2) a minimal Lua `update`/`draw` snippet. This shows both the runner API and what a Lua script looks like — directly relevant to DOC-01's "reflects v1.7+ SDL3 runner API" criterion.

2. **Tutorial filenames and URL slugs**
   - What we know: Claude has discretion here. Two candidates: flat in `src/` vs `src/tutorials/` subdirectory.
   - Recommendation: Use `src/tutorials/your-first-script.md` and `src/tutorials/async-coroutines.md` — the subdirectory groups tutorials cleanly and matches the category label in the sidebar.

3. **Optional Lua API index/overview page**
   - What we know: Claude has discretion on whether to add this.
   - Recommendation: Skip for this phase. The two tutorials plus the existing architecture.md provide sufficient onboarding. An API index belongs in a future dedicated API-docs phase.

---

## Sources

### Primary (HIGH confidence)
- `/home/unwn/git/enjin/docs/docusaurus.config.js` — confirmed version 3.9.2, existing additionalLanguages array, prism-react-renderer version
- `/home/unwn/git/enjin/docs/sidebars.js` — confirmed CommonJS module.exports format and existing category pattern
- `/home/unwn/git/enjin/docs/src/getting-started.md` — confirmed one Canvas8_128x64 reference at line 39
- `/home/unwn/git/enjin/scripts/tamagotchi.lua` — confirmed all engine.* API calls used in "Your First Script"
- `/home/unwn/git/enjin/src/scripting/bindings_async.cpp` — confirmed exact Lua API: start, wait, wait_frames, cancel, cancelAll
- `/home/unwn/git/enjin/src/scripting/bindings_tween.cpp` lines 305-342 — confirmed engine.tween.await(id) semantics
- `https://prismjs.com/index.html#supported-languages` — confirmed `lua` is a built-in Prism component name
- `https://docusaurus.io/docs/markdown-features/code-blocks` — confirmed additionalLanguages config pattern

### Secondary (MEDIUM confidence)
- `https://docusaurus.io/docs/sidebar/items` — confirmed sidebar category type: 'category' / items array syntax for Docusaurus 3

### Tertiary (LOW confidence)
- None

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all tools already present in the repo; versions read from package.json
- Architecture: HIGH — all config files read directly; patterns observed from existing docs
- Tutorial content accuracy: HIGH — all API calls verified from C++ source and Lua script files
- Pitfalls: HIGH — most derived from direct code reading (onBrokenLinks: 'throw' in config, prism hot-reload behavior from official docs)

**Research date:** 2026-03-03
**Valid until:** 2026-04-03 (Docusaurus 3.x is stable; Prism Lua grammar is stable)
