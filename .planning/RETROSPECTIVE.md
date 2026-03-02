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

## Milestone: v1.6 — Game Ready

**Shipped:** 2026-02-28
**Phases:** 4 | **Plans:** 4 | **Tasks:** 8

### What Was Built
- ComponentProxy self:get() — typed proxy userdata for accessing sibling components with Component::~Component() stale-safe invalidation
- C_Timer with delayed/repeating Lua callbacks (after/every/cancel) — 8-slot zero-alloc timer array with luaL_ref lifecycle management
- C_StateMachine with deferred transitions — named states with enter/update/exit hooks matching SceneStateMachine semantics
- EventBus scene-scoped pub/sub — on/off/emit API with fixed-capacity arrays, re-entrant-safe emit via ref snapshotting

### What Worked
- v1.5 patterns (ComponentProxy, luaL_ref cleanup, callWithProxy dispatch) directly applied — Phases 40-42 each executed in under 10 minutes
- Each phase established a clear pattern that the next phase replicated (39 -> 40 -> 41 -> 42): proxy metatable registration, hot-reload cleanup in executeScript/loadScriptFile, destruction-order safety in C_LuaScript destructor
- Zero deviations from plan on Phases 39-41; Phase 42 had minor deviations (forward declarations, executeScript hook) all auto-fixed
- Pre-milestone audit (v1.6-MILESTONE-AUDIT.md) confirmed 19/19 requirements, 25/25 observable truths — no gap closure phase needed

### What Was Inefficient
- `gsd-tools milestone complete` CLI still miscounts phases (counted 7 instead of 4) — this is the third milestone where manual correction was needed
- The REQUIREMENTS.md grew to include phases 43-45 content that belongs to v1.7 — mixed-scope requirements doc reduced archival clarity
- Phases 40-42 plans marked as "TBD" in ROADMAP.md phase details despite being executed — stale plan references in roadmap

### Patterns Established
- **clearX() sets m_L=nullptr sentinel**: prevents double-unref across clearTimers(), clearStates(), clearHandlers() — universal pattern for luaL_ref-owning components
- **C_LuaScript destructor cleanup chain**: call sibling component clearX() before scriptSystem->shutdown() — handles component array destruction order
- **emit() ref snapshotting**: copy callback refs to local array before pcall loop for re-entrant safety
- **Fixed-capacity arrays for zero-alloc components**: Channel[16], Subscriber[8], TimerEntry[8], StateEntry[8] — consistent capacity limits across all game components

### Key Lessons
1. When a milestone establishes a clear pattern (Phase 39 ComponentProxy), subsequent phases execute extremely fast by replication — 4-5 minutes per phase for 40-42
2. Pre-milestone audit as a mandatory gate (established in v1.5) continues to prove its value — v1.6 passed cleanly because patterns were well-established
3. REQUIREMENTS.md scope should be strictly milestone-scoped — mixing v1.6 and v1.7 requirements in one file creates archival confusion

### Cost Observations
- Model mix: balanced profile (sonnet for research/planning, opus for execution)
- Sessions: 1-2 (all 4 phases executed in rapid succession)
- Notable: 4 plans in ~50 min total — fastest milestone execution, driven by pattern replication from Phase 39

---

## Milestone: v1.7 — Developer Experience & New Capability

**Shipped:** 2026-03-02
**Phases:** 10 | **Plans:** 19 | **Requirements:** 26/26

### What Was Built
- C_Tilemap 64x64 grid with viewport-culled rendering, coordinate helpers, and full Lua proxy (Phase 43)
- C_Camera with lerp follow, screen shake, bounds, drawWithOffset() render pipeline, engine.camera.* API (Phase 44)
- Stateless engine.physics.* toolkit — gravity, drag, springs, bounce, raycast, TrigLUT (Phase 45)
- Bindings refactored: 1390-line monolith split via bindings_internal.hpp, systematic null safety guards (Phase 46)
- engine.debug.* top-layer debug canvas with zero-cost toggle (Phase 47)
- engine.camera.follow/stopFollow + LuaStore SDL3 JSON I/O with flush/path (Phase 48)
- engine.async.* 8-slot coroutine scheduler with wait/cancel (Phase 49)
- engine.tween.* 8-slot pool with 4 inline easing functions (Phase 50)
- engine.scene.persist/unpersist with PersistentObjectRegistry (Phase 51)
- engine.ui.* stateless immediate-mode draw calls + internal component guide (Phase 52)

### What Worked
- Phases 43-45 (tilemap, camera, physics) were content-heavy but self-contained — each introduced a new component/system with no cross-dependencies between them
- Phase 46 (bindings refactoring) as structural foundation was correctly sequenced — all subsequent binding files (47-52) built on bindings_internal.hpp and null-guard patterns
- Fixed-capacity pool pattern (CoroutineSlot[8], TweenSlot[8], PersistentObjectRegistry[4]) reused across phases 49-51 — consistent API and cleanup semantics
- registerEngineTable() sub-table registration pattern scaled cleanly from 10 to 18+ sub-tables
- REQUIREMENTS.md was strictly milestone-scoped (lesson from v1.6) — clean archival, no mixed-scope confusion
- Pre-milestone audit passed clean (26/26) — 42/42 integration exports wired, 6/6 E2E flows verified

### What Was Inefficient
- Phases 43-45 were originally created without a formal REQUIREMENTS.md — their requirements (TMAP, CAM, PHYS) are phase-scoped but not tracked in the milestone traceability table
- Some ROADMAP.md plan checkboxes were left unchecked (`- [ ]`) despite plans being complete — stale state in roadmap
- SUMMARY.md one_liner field was null for all v1.7 summaries — the template field was never populated, making automated accomplishment extraction fail
- STATE.md progress counts were stale (counted all 16 project phases, not 10 v1.7 phases) — gsd-tools milestone counting issue persists from v1.4

### Patterns Established
- **REQUIRE_CANVAS/REQUIRE_DEBUG_CANVAS macros**: zero-cost early-return guards for optional canvas pointers — reusable for any future canvas-dependent binding
- **tickX() / clearX() pair**: each subsystem (coroutines, tweens, camera follow) provides tick + clear; SDL runner calls tick each frame, registerAll/setActiveScene calls clear
- **engine.ui.* stateless immediate-mode draw**: bypasses C++ component layer entirely; LuaCanvas draw calls only — correct pattern when C++ components use incompatible types (std::string)
- **PersistentObjectRegistry as SSM-owned**: object ownership stays in C++ layer, not Lua bindings — correct ownership model for cross-scene lifecycle

### Key Lessons
1. Structural refactoring (Phase 46) should be planned as a prerequisite when multiple new binding files will follow — the split paid for itself across 6 subsequent phases
2. Fixed-capacity pool pattern (8-slot array + clearSlot) is the universal pattern for zero-alloc subsystems — established for timers (v1.6), reused for coroutines, tweens, persistent objects
3. SUMMARY.md one_liner field should be populated during execution for automated milestone reporting — missing data means manual accomplishment extraction
4. The tick-order contract (camera -> coroutines -> tweens) is architecturally significant and should be documented as a system invariant, not just an implementation detail

### Cost Observations
- Model mix: balanced profile (sonnet for research/planning, opus for execution)
- Sessions: ~6 (Phases 43-45 content sprint, Phase 46 refactoring, Phases 47-48 DX sprint, Phases 49-50 async/tween sprint, Phases 51-52 persistence/UI sprint)
- Notable: 19 plans across 10 phases in 3 days — largest v1.x milestone by phase count; all 26 requirements passed audit

---

## Milestone: v1.8 — Ship Ready

**Shipped:** 2026-03-03
**Phases:** 6 | **Plans:** 13 | **Commits:** 57 | **Files changed:** 78

### What Was Built
- `scripts/setup-dev.sh` — idempotent toolchain installer for Emscripten 3.1.73 + ESP-IDF v5.5 to XDG paths (Phase 53)
- `build.sh` — unified build entry replacing build_wasm.sh; all 3 platforms verified building (Phase 53)
- `LuaStore::writeStoreToBuffer()` — allocation-free JSON serializer shared by SDL3/WASM/ESP32 (Phase 54)
- WASM localStorage bridge — `engine.store` persists across page reloads (Phase 55)
- ESP32 NVS backend — `engine.store` persists across power cycles with 15-char key validation (Phase 55)
- `engine.tween.await(id)` + `engine.async.wait_frames(n)` + `engine.camera.setDeadZone()` (Phase 57)
- Docusaurus tutorials with Lua syntax highlighting, Getting Started + "Your First Script" + "Async Coroutines" (Phase 58)

### What Worked
- Small, well-scoped phases (1 plan each for 54, 56) executed very quickly with clear acceptance criteria
- Phase 54 extracting writeStoreToBuffer() before the platform backends was the correct sequencing — both Phase 55 backends used it directly
- Platform guard structure established in 55-01 (WASM) was immediately reused in 55-02 (ESP32) — minimal rework
- QoL features (Phase 57) were tightly coupled and correctly planned in 3 atomic plans (coroutine slot extension, camera dead zone, integration tests)
- Documentation phase (58) was effective as a cap phase — 3 parallel deliverables with no inter-dependencies executed cleanly

### What Was Inefficient
- REQUIREMENTS.md checkboxes were never updated during milestone execution — all 11 "pending" items were actually complete, causing a misleading traceability table at archival time
- No milestone audit was run — the audit would have surfaced the stale REQUIREMENTS.md and confirmed E2E storage flows across all 3 platforms
- SUMMARY.md one_liner field still not populated (same issue as v1.7) — automated accomplishment extraction returns nothing

### Patterns Established
- **writeStoreToBuffer as platform-universal serializer**: placed before all platform #ifdef guards so any future storage backend gets it for free
- **Platform storage guard structure**: `#ifdef __EMSCRIPTEN__` → `#elif ESP32` → `#else` (SDL3 default) pattern used in both saveToFile and loadFromFile — replicable for future platform I/O
- **waitTweenId gate in tickCoroutines**: skipping frame/time check and resuming in tickTweens avoids double-resume — correct pattern for any future "await external event" coroutine primitive

### Key Lessons
1. Update REQUIREMENTS.md checkboxes during execution, not only during archival — stale traceability wastes archival audit time and loses historical correctness
2. The `gsd:audit-milestone` step catches not just code gaps but documentation gaps — skipping it means requirements tracking stays stale
3. Cap phases (documentation, tutorials) benefit from upfront content research: knowing what to write about before planning results in tighter, more accurate plans

### Cost Observations
- Model mix: balanced profile (sonnet for research/planning, sonnet for execution)
- Sessions: ~5 (Phase 53 toolchain sprint, Phase 54 serializer, Phase 55 storage backends, Phase 56+57 QoL, Phase 58 docs)
- Notable: 13 plans in 2 days — efficient cadence driven by clear phase decomposition; storage phases were the deepest work

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
| v1.6 | 4 | 4 | Game ready — pattern replication from Phase 39, fastest milestone, audit passed clean |
| v1.7 | 10 | 19 | DX + new capability — structural refactoring as prerequisite, fixed-capacity pool pattern, 26/26 requirements |
| v1.8 | 6 | 13 | Ship ready — platform storage backends, QoL coroutine additions, onboarding docs, all 3 targets verified |

### Cumulative Quality

| Milestone | Test Suites | Key Additions |
|-----------|-------------|---------------|
| v1.0 | 0 | Migration validation pipeline |
| v1.1 | 0 | CI Doxygen warning gate |
| v1.2 | 0 | Clean regeneration verified |
| v1.3 | 2 | input_test, palette_test |
| v1.4 | 4 | sprite_test, compositor_test |
| v1.5 | 22+ | scene_render, named_objects, scene_transition, engine_table, script_proxy, error_policy, input_callback, gc_assert, drawable_decoupling, proxy_lifetime, live-wiring |
| v1.6 | 27+ | component_proxy_test, timer_test, state_machine_test, eventbus_test |
| v1.7 | 43 | tilemap_test, camera_test, camera_lua_test, physics_test, physics_lua_test, sprite_load_test, overflow_test, debug_draw_test, camera_follow_test, store_test, coroutine_async_test, tween_test, persistent_objects_test, persistent_lua_test, ui_binding_test |
| v1.8 | 46+ | qol_test (tween.await, wait_frames, camera dead zone — 148 assertions), store_test extended |

### Top Lessons (Verified Across Milestones)

1. Research phases that identify exact targets make execution predictable (v1.3, v1.4, v1.5, v1.7, v1.8)
2. lua_CFunction-only bindings eliminate UB risks from std::function capture (v1.3, v1.4, v1.5)
3. Header-only zero-alloc structs are the optimal pattern for this codebase (v1.3 InputState, v1.4 SpriteSheet/LayerCompositor)
4. Integration paths need live-wiring tests, not just unit tests — unit tests pass with null pointers; live tests catch initialization-order bugs (v1.5)
5. Structural refactoring as a prerequisite phase pays dividends across all subsequent phases in the same milestone (v1.7 Phase 46)
6. Fixed-capacity pool pattern (N-slot array + clear) is the universal zero-alloc subsystem pattern — replicated 4x across v1.6-v1.7
7. Update REQUIREMENTS.md checkboxes during execution — stale traceability at archival time causes misleading gap analysis (v1.7, v1.8)
