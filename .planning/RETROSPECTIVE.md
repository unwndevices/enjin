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

## Milestone: v1.5 — Lua Scripting Foundation

**Shipped:** 2026-02-28
**Phases:** 12 | **Plans:** 21 | **Commits:** 127

### What Was Built
- `engine.*` Lua global table with live pointer-to-pointer registry injection for scene/input/time/lua/log
- `ScriptProxy` full userdata with `__index`/`__newindex` metamethods — `self` as first arg in all callbacks
- `ScriptErrorPolicy` (Disable/Log/Panic) + `on_button_pressed`/`on_button_released` edge callbacks
- Named objects + 8-slot zero-allocation tag system; `findByName()`/`findAllWithTag()` on ObjectCollection
- Deferred scene self-transitions via `SceneStateMachine*` back-pointer injection
- `engine.lua.collect()`/`memory()` + `assertRequires<T>()` component dependency assertions
- `ObjectProxy` from `engine.scene.find()` with Object-destructor invalidation; stale proxy raises `luaL_error`
- Object decoupled from C_Drawable via `getComponents<T>()` generic template
- Phase 38 gap closure: pointer-to-pointer registry wiring (ENG-01/02), `loadScriptFile()` proxy fix (PROXY-01)

### What Worked
- Audit-driven gap closure (Phase 38) was highly effective — the audit report gave precise file:line diagnosis for all 3 broken paths
- Phase decomposition at the requirement level kept plans narrow and testable (each plan addressed 1-2 REQ-IDs)
- `callWithProxy()` dispatch pattern unified all callback sites — init/update/draw/input callbacks share one code path
- Pointer-to-pointer registry pattern (Phase 38) is the correct solution for post-initialization pointer injection without re-registering all closures
- TDD-first approach for Phases 29, 30, 34 — tests written before implementation caught edge cases (self-transition, tag overflow, callback ordering)

### What Was Inefficient
- The milestone audit occurred after phases were "done" — gaps like ENG-01/02 live wiring and PROXY-01 file-load path should have been caught in Phase 31/32 VERIFICATION.md
- Phase 36 and 37 were added mid-milestone (not in the original v1.5 roadmap) — the Object/Drawable coupling concern was known but not planned; would have benefited from upfront inclusion
- `gsd-tools milestone complete` CLI still miscounts phases (counts all phases, not just milestone scope) — manual correction needed again
- Phase 35 shipped without a VERIFICATION.md — documentation debt that the Phase 38 plan had to retroactively close

### Patterns Established
- **Pointer-to-pointer registry**: store `T**` in Lua registry so post-initialization pointer updates are automatically visible to all closures
- **Audit-before-complete**: running the milestone audit before declaring complete exposed 3 real integration bugs — the audit is now a mandatory gate
- **Full userdata for proxies**: metatables require full userdata; lightuserdata has no per-object metatable — use full userdata for any proxy that needs `__index`/`__newindex`
- **stale proxy raises error (not nil)**: silent nil produces confusing distant failures; `luaL_error` with a clear message directs the author to the fix immediately
- **`assertRequires<T>()` on Component base**: single declaration site covers all components; debug abort vs release log+disable covers all deployment targets

### Key Lessons
1. Integration paths (live wiring) need their own test tier — unit tests verify behavior in isolation but can miss initialization-order bugs; live-wiring tests should be a standard plan artifact
2. Auditing before archiving is worth the context cost — it revealed gaps that would have been silent bugs in production (ENG-01/02 silently returned nil instead of executing transitions)
3. When a phase inserts work retroactively (Phase 38 fixing Phase 27's initial "complete" status), the audit report date becomes stale — always re-read audit dates before treating status as current

### Cost Observations
- Model mix: balanced profile (sonnet for research/planning, opus for execution)
- Sessions: 6-8 (Phase 27-29 sprint, Phase 30-31 sprint, Phase 32-33 sprint, Phase 34-35 sprint, Phase 36-37 sprint, Phase 38 gap closure)
- Notable: 21 plans across 12 phases in 2 days — most complex milestone yet; gap-closure phase (38) added meaningful quality

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
| v1.5 | 12 | 21 | Scripting foundation — audit-driven gap closure, pointer-to-pointer registry, live-wiring tests |

### Cumulative Quality

| Milestone | Test Suites | Key Additions |
|-----------|-------------|---------------|
| v1.0 | 0 | Migration validation pipeline |
| v1.1 | 0 | CI Doxygen warning gate |
| v1.2 | 0 | Clean regeneration verified |
| v1.3 | 2 | input_test, palette_test |
| v1.4 | 4 | sprite_test, compositor_test |
| v1.5 | 22+ | scene_render, named_objects, scene_transition, engine_table, script_proxy, error_policy, input_callback, gc_assert, drawable_decoupling, proxy_lifetime, live-wiring |

### Top Lessons (Verified Across Milestones)

1. Research phases that identify exact targets make execution predictable (v1.3, v1.4, v1.5)
2. lua_CFunction-only bindings eliminate UB risks from std::function capture (v1.3, v1.4, v1.5)
3. Header-only zero-alloc structs are the optimal pattern for this codebase (v1.3 InputState, v1.4 SpriteSheet/LayerCompositor)
4. Integration paths need live-wiring tests, not just unit tests — unit tests pass with null pointers; live tests catch initialization-order bugs (v1.5)
