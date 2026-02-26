# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v1.4 — Engine Capabilities

**Shipped:** 2026-02-26
**Phases:** 4 | **Plans:** 8 | **Tasks:** 15

### What Was Built
- SpriteSheet zero-alloc struct with grid addressing, frame animation (Once/Loop/PingPong), and 16-slot Lua sprite pool
- LayerCompositor with 4 independent Canvas4 buffers, painter's-order composition, index-15 transparency
- SDL3 multi-layer rendering + Lua layer API (setLayer/clearLayer/getLayerCount/visibility)
- F5 hot-reload with full Lua state reset, error recovery, and LuaCallback dangling-pointer fix
- Docusaurus API docs fully navigable — MDX-safe escaping across 84 pages

### What Worked
- Research phases pre-identified exact call sites and integration points — zero surprises on Phases 23, 25-02, 25-03, 26
- lua_CFunction-only pattern (established in v1.3) eliminated an entire class of dangling-pointer bugs
- Atomic plan decomposition: each plan in Phase 24 and 25 had clear boundaries (struct, component, bindings)
- Plans 25-02 and 25-03 executed in ~2-3 min each because 25-01 laid a clean foundation

### What Was Inefficient
- Phase 24 plan ordering: 24-02 and 24-03 could have been planned as parallel since they modify different subsystems (components vs scripting)
- C_Canvas ENG-01 stub created in 24-02 — deferred tech debt that will need cleanup if Canvas8 compositing is ever needed
- The `gsd-tools milestone complete` CLI counted all project phases (10) instead of v1.4 phases (4) — manual correction needed

### Patterns Established
- `performReload()` pattern: shutdown + initialize + wire bindings + loadScript — reusable for any future scripting system reset
- `lua_ok` gate pattern for error recovery in game loops
- `buffer_index` direct slot assignment replacing enum-based DrawLayer — simpler, no naming bugs
- `setLayers()` as single-call compositor wiring replaces per-layer `setCanvas()` calls
- Lua 1-indexed layer convention with clamped-to-range safety

### Key Lessons
1. Pre-flight research that identifies exact file:line targets eliminates guesswork and keeps execution under 5 minutes per plan
2. When a legacy API (LuaCallback) can't be safely removed, neutering it to a no-op is safer than deletion — preserves ABI
3. Header-only structs (SpriteSheet, LayerCompositor) work well for the zero-alloc codebase — no .cpp needed, no link-time overhead

### Cost Observations
- Model mix: balanced profile (sonnet for research/planning, opus for execution)
- Sessions: 3 (Phase 23-24 sprint, Phase 25 sprint, Phase 26 sprint)
- Notable: 8 plans in 2 days — highest velocity milestone yet

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Phases | Plans | Key Change |
|-----------|--------|-------|------------|
| v1.0 | 6 | 21 | Initial migration — established GSD workflow |
| v1.1 | 9 | 17 | Documentation focus — CI gates added |
| v1.2 | 3 | 5 | Tech debt cleanup — net-negative LOC |
| v1.3 | 4 | 7 | Feature milestone — lua_CFunction pattern established |
| v1.4 | 4 | 8 | Engine capabilities — fastest execution, research-driven plans |

### Cumulative Quality

| Milestone | Test Suites | Key Additions |
|-----------|-------------|---------------|
| v1.0 | 0 | Migration validation pipeline |
| v1.1 | 0 | CI Doxygen warning gate |
| v1.2 | 0 | Clean regeneration verified |
| v1.3 | 2 | input_test, palette_test |
| v1.4 | 4 | sprite_test, compositor_test |

### Top Lessons (Verified Across Milestones)

1. Research phases that identify exact targets make execution predictable (v1.3, v1.4)
2. lua_CFunction-only bindings eliminate UB risks from std::function capture (v1.3, v1.4)
3. Header-only zero-alloc structs are the optimal pattern for this codebase (v1.3 InputState, v1.4 SpriteSheet/LayerCompositor)
