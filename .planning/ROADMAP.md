# Roadmap: enjin2

## Milestones

- [x] **v1.0 Migration + Documentation** - Phases 1-6 (shipped 2026-02-01)
- [x] **v1.1 Project Infrastructure & Documentation Enhancement** - Phases 7-15 (shipped 2026-02-23)
- [x] **v1.2 Tech Debt Cleanup** - Phases 16-18 (shipped 2026-02-23)
- [x] **v1.3 Tomodachi Readiness** - Phases 19-22 (shipped 2026-02-24)
- [x] **v1.4 Engine Capabilities** - Phases 23-26 (shipped 2026-02-26)
- [x] **v1.5 Lua Scripting Foundation** - Phases 27-38 (shipped 2026-02-28)
- [x] **v1.6 Game Ready** - Phases 39-42 (shipped 2026-02-28)
- [ ] **v1.7 Developer Experience & New Capability** - Phases 43-52 (in progress)

## Phases

<details>
<summary>v1.0 Migration + Documentation (Phases 1-6) - SHIPPED 2026-02-01</summary>

Phases 1-6 complete. See milestones/v1.0-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.1 Project Infrastructure & Documentation Enhancement (Phases 7-15) - SHIPPED 2026-02-23</summary>

Phases 7-15 complete. See milestones/v1.1-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.2 Tech Debt Cleanup (Phases 16-18) - SHIPPED 2026-02-23</summary>

Phases 16-18 complete. See milestones/v1.2-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.3 Tomodachi Readiness (Phases 19-22) - SHIPPED 2026-02-24</summary>

Phases 19-22 complete. See milestones/v1.3-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.4 Engine Capabilities (Phases 23-26) - SHIPPED 2026-02-26</summary>

Phases 23-26 complete. See milestones/v1.4-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.5 Lua Scripting Foundation (Phases 27-38) - SHIPPED 2026-02-28</summary>

Phases 27-38 complete. See milestones/v1.5-ROADMAP.md for full detail.

</details>

<details>
<summary>v1.6 Game Ready (Phases 39-42) - SHIPPED 2026-02-28</summary>

Phases 39-42 complete. See milestones/v1.6-ROADMAP.md for full detail.

</details>

### v1.7 Developer Experience & New Capability (In Progress)

**Milestone Goal:** Polish the developer experience with debug draw, save/load, persistent objects, and camera follow; harden the engine with overflow tests, bindings refactoring, and null safety; add coroutines, tweens, and UI components for Lua.

- [x] **Phase 43: Tilemap System** - 64x64 tile grid, viewport-culled rendering, scroll, coordinate helpers (completed 2026-02-28)
- [x] **Phase 44: 2D Camera System** - Scene render pipeline, drawWithOffset(), engine.camera.* Lua API (completed 2026-02-28)
- [x] **Phase 45: Optimized 2D Physics Engine** - engine.physics.* stateless helpers, TrigLUT, DDA raycast (completed 2026-03-01)
- [x] **Phase 46: Bindings Refactoring + Null Safety** - Split bindings.cpp monolith, harden null guards, fix sprite test, add overflow tests (completed 2026-03-01)
- [x] **Phase 47: Debug Draw Bindings** - engine.debug.* sub-table routing to dedicated debug canvas with zero-cost toggle (completed 2026-03-01)
- [ ] **Phase 48: Camera Follow + Save/Load** - engine.camera.follow/stopFollow, LuaStore SDL3 I/O, engine.store.flush/path
- [ ] **Phase 49: Coroutine/Async Scheduler** - engine.async.* 8-slot coroutine pool with wait/cancel, ESP32 library open
- [ ] **Phase 50: Tween Helpers** - engine.tween.* 8-slot pool with 4 inline easing functions, proxy-field animation
- [ ] **Phase 51: Persistent Objects** - engine.scene.persist/unpersist, PersistentObjectRegistry, find() searches persistent registry
- [ ] **Phase 52: UI Component Bindings** - engine.ui.* stateless draw calls: progressBar, statBar, panel, label

## Phase Details

### Phase 43: Tilemap System

**Goal:** Grid-based tilemap rendering and management for level-based games — C_Tilemap component with fixed-size 64x64 tile grid, SpriteSheet-based tileset, viewport-culled rendering, tilemap-scoped camera offset, coordinate conversion helpers, and full Lua bindings via ComponentProxy
**Requirements**: TMAP-01, TMAP-02, TMAP-03, TMAP-04, TMAP-05, TMAP-06, TMAP-07, TMAP-08
**Depends on:** Phase 42
**Success Criteria** (what must be TRUE):
  1. A C_Tilemap component stores a 64x64 uint8_t tile grid with zero dynamic allocation and renders only visible tiles via viewport culling
  2. Tile ID 0 is transparent (not drawn); IDs 1-255 are rendered using SpriteSheet::draw()
  3. Built-in camera offset (scrollX, scrollY) shifts the visible tilemap viewport
  4. Lua scripts access C_Tilemap via self:get("C_Tilemap") with setTile/getTile/setTiles/setSheet/setScroll/getScroll/pixelToTile/tileToPixel/tileAtPixel/getMapSize
  5. Map data can be initialized from a flat Lua table via tilemap:setTiles(table, w, h)
**Plans:** 2/2 plans complete

Plans:
- [x] 43-01-PLAN.md — C_Tilemap C++ component (C_Drawable-derived), tile data structure, viewport-culled draw(), coordinate helpers, C++ test suite (completed 2026-02-28)
- [x] 43-02-PLAN.md — C_Tilemap_Proxy Lua bindings (ComponentProxy dispatch, all proxy methods), Lua integration test suite (completed 2026-02-28)

### Phase 44: 2D Camera System

**Goal:** Engine-wide 2D camera that applies a world-to-screen transform to ALL C_Drawable entities in the scene — C_Camera component with float-precision position, smooth lerp follow, screen shake, viewport bounds clamping, screen-space opt-out for UI elements, and full Lua bindings via ComponentProxy and engine.camera.* global API
**Requirements**: CAM-01, CAM-02, CAM-03, CAM-04, CAM-05, CAM-06, CAM-07, CAM-08, CAM-09
**Depends on:** Phase 43
**Success Criteria** (what must be TRUE):
  1. C_Camera component stores float-precision world position; Scene::renderObjects() applies camera offset to all visible drawables via drawWithOffset()
  2. C_Drawable with screenSpace=true skips camera offset (UI/HUD elements stay fixed on screen)
  3. Camera supports smooth lerp follow toward a target, screen shake with decay, and viewport bounds clamping
  4. Lua scripts access C_Camera via self:get("C_Camera") with setPosition/getPosition/lookAt/shake/setBounds/clearBounds
  5. engine.camera.* global sub-table provides scene-level camera control without needing a ComponentProxy
  6. C_Tilemap drawWithOffset integrates camera offset additively with its internal scroll
**Plans:** 2/2 plans complete

Plans:
- [x] 44-01-PLAN.md — C_Camera C++ component, C_Drawable drawWithOffset() + screen-space flag, Scene render pipeline modification, C++ test suite (completed 2026-02-28)
- [x] 44-02-PLAN.md — C_Camera_Proxy Lua bindings, engine.camera.* sub-table, C_Tilemap integration, Lua integration test suite (completed 2026-02-28)

### Phase 45: Optimized 2D Physics Engine

**Goal:** Stateless physics helper toolkit exposed as `engine.physics.*` Lua functions — gravity, drag, springs, attraction, bounce, orbiting, raycasting, and velocity integration — with pre-computed trig tables for embedded performance
**Requirements**: PHYS-01, PHYS-02, PHYS-03, PHYS-04, PHYS-05, PHYS-06, PHYS-07, PHYS-08, PHYS-09, PHYS-10, PHYS-11, PHYS-12, PHYS-13
**Depends on:** Phase 44
**Success Criteria** (what must be TRUE):
  1. Header-only C++ physics helpers in `physics.hpp` provide stateless inline functions: applyGravity, bounce, applyDrag, springForce, attract, orbitVelocity, applyVelocity
  2. TrigLUT completed with real 256-entry precomputed sine table (not std::sin delegation)
  3. `engine.physics.*` Lua sub-table exposes all helpers plus setGravity/getGravity global state
  4. applyGravity accepts both 3-arg (global gravity) and 5-arg (override gravity) forms
  5. All physics functions accept both Vec2 userdata and plain number pairs
  6. DDA tilemap raycast + linear object scan via `engine.physics.raycast()`
**Plans:** 2/2 plans complete

Plans:
- [x] 45-01-PLAN.md — C++ physics helpers (physics.hpp), TrigLUT completion, C++ unit tests (completed 2026-03-01)
- [x] 45-02-PLAN.md — Lua bindings (bindings_physics.cpp), engine.physics.* sub-table, raycast, Lua integration tests (completed 2026-03-01)

### Phase 46: Bindings Refactoring + Null Safety

**Goal:** Split the 1390-line bindings.cpp monolith into focused translation units via bindings_internal.hpp, add systematic null safety guards to all binding chains, fix the sprite_load_test.cpp compilation error, and deliver overflow tests for event bus, sprite pool, and component destruction — establishing the structural foundation all subsequent binding files in v1.7 depend on
**Depends on:** Phase 45
**Requirements**: BIND-01, BIND-02, TEST-01, TEST-02
**Success Criteria** (what must be TRUE):
  1. bindings.cpp is split into focused files (e.g., bindings_proxy.cpp, bindings_scene.cpp) sharing declarations through bindings_internal.hpp with zero linker errors and all 27+ ctests still passing
  2. Numeric-returning binding functions return 0 (not nil) when called on a null or invalid target, preventing Lua arithmetic errors in scripts
  3. sprite_load_test.cpp compiles and links without errors
  4. Overflow tests for event bus (beyond 16 channels / 8 subscribers), sprite pool (beyond pool capacity), and component destruction execute successfully via ctest
**Plans:** 2/2 plans complete

Plans:
- [x] 46-01-PLAN.md — bindings.cpp split: bindings_internal.hpp shared constants, bindings_proxy.cpp extraction (component + object proxy metatables), CMakeLists.txt update, ctest verification (completed 2026-03-01)
- [x] 46-02-PLAN.md — Null safety pass + test fixes: lua_wrapper.hpp for sprite_load_test, null guards on all binding chains, overflow_test.cpp (event bus, sprite pool, component destruction) (completed 2026-03-01)

### Phase 47: Debug Draw Bindings

**Goal:** Add engine.debug.* Lua sub-table that routes debug draw calls (rect, circle, line, cross, text) to a dedicated top-layer debug canvas — with a boolean toggle that costs nothing when disabled — establishing the layer routing pattern before coroutines and tweens are introduced
**Depends on:** Phase 46
**Requirements**: DEBUG-01, DEBUG-02, DEBUG-03
**Success Criteria** (what must be TRUE):
  1. Lua scripts can call engine.debug.rect/circle/line/cross with pixel coordinates and color indices and see shapes rendered on screen during the frame
  2. engine.debug.text renders a string overlay at the specified position on the debug canvas
  3. Setting engine.debug.enabled = false suppresses all debug draw calls with zero per-frame cost (no draw calls issued)
  4. Debug shapes appear above all game content (top layer) and are cleared automatically each frame without script intervention
**Plans:** 1/1 plans complete

Plans:
- [ ] 47-01-PLAN.md — ENJIN_LAYER_COUNT to 5, bindings_debug.cpp with engine.debug.* sub-table (rect/circle/line/cross/text/setEnabled/getEnabled), SDL runner 5th layer wiring, debug_draw_test.cpp Lua integration test

### Phase 48: Camera Follow + Save/Load

**Goal:** Deliver two independent low-complexity features: engine.camera.follow/stopFollow bindings that track a named object per-frame via C_Camera, and LuaStore SDL3 JSON I/O by replacing the VCV_RACK preprocessor guard with correct platform branching — including engine.store.flush() and engine.store.path() for explicit save control
**Depends on:** Phase 46
**Requirements**: CAM-01, CAM-02, STORE-01, STORE-02
**Success Criteria** (what must be TRUE):
  1. A Lua script can call engine.camera.follow(proxy, speed) and the camera smoothly tracks the target object position every frame without additional per-frame script code
  2. engine.camera.stopFollow() clears the follow target so the camera stops tracking
  3. LuaStore reads and writes JSON files on SDL3 desktop builds without a VCV_RACK compile flag
  4. engine.store.flush() explicitly writes pending store data to disk; engine.store.path(filepath) redirects the save file location at runtime
**Plans:** TBD

Plans:
- [ ] 48-01: Camera follow — C_Camera::setFollowTarget()/clearFollowTarget(), engine.camera.follow/stopFollow bindings, proxy validity guard
- [ ] 48-02: Save/load SDL3 — VCV_RACK guard replaced, bindings_store.cpp updated, engine.store.flush() and engine.store.path() bindings

### Phase 49: Coroutine/Async Scheduler

**Goal:** Add engine.async.* Lua API backed by an 8-slot fixed coroutine pool in LuaBindings — supporting start/cancel/cancelAll and per-frame wait() yield — with the scheduler resumed from C each frame outside any pcall scope to avoid the yield-across-pcall boundary, and the coroutine library explicitly opened on ESP32
**Depends on:** Phase 46
**Requirements**: ASYNC-01, ASYNC-02, ASYNC-03, ASYNC-04
**Success Criteria** (what must be TRUE):
  1. A Lua script can call engine.async.start(function) and the function executes as a coroutine, resuming from where it left off each frame
  2. engine.async.wait(seconds) called inside a coroutine pauses that coroutine for the specified duration and resumes it automatically without script polling
  3. engine.async.cancel(id) stops a running coroutine and engine.async.cancelAll() stops all coroutines; both clean up Lua refs correctly
  4. Coroutine scripts work on ESP32 (coroutine library opened in openEmbeddedLibraries()) and survive F5 hot reload without dangling thread refs
**Plans:** TBD

Plans:
- [ ] 49-01: LuaCoroutineScheduler — CoroutineSlot[8] pool, tickCoroutines() in SDL runner, clearCoroutines() in hot-reload path, int threadRef lifecycle
- [ ] 49-02: engine.async.* bindings — start/cancel/cancelAll/wait, ESP32 luaopen_coroutine, Lua integration tests

### Phase 50: Tween Helpers

**Goal:** Add engine.tween.* Lua API backed by an 8-slot TweenSlot fixed array in LuaBindings — animating Lua table fields over time via four inline easing functions — with the pool ticked from C each frame and clearTweens() called on hot reload to prevent stale callback refs
**Depends on:** Phase 49
**Requirements**: TWEEN-01, TWEEN-02, TWEEN-03
**Success Criteria** (what must be TRUE):
  1. A Lua script can call engine.tween.to(target, {props}, duration, easing, done_cb) and the specified table fields animate to their target values over the given duration
  2. engine.tween.cancel(id) stops a specific tween and engine.tween.cancelAll() stops all tweens, both freeing Lua refs correctly
  3. The four easing functions (linear, easeIn, easeOut, easeInOut) produce visually distinct motion curves; no FPU-heavy math (no std::pow with float exponents)
**Plans:** TBD

Plans:
- [ ] 50-01: TweenSlot[8] pool — tickTweens(dt) in SDL runner, clearTweens() in hot-reload path, 4 inline easing functions, proxy validity check per tick
- [ ] 50-02: engine.tween.* bindings — to/cancel/cancelAll, Lua integration tests (all easing modes, cancel mid-tween, hot-reload safety)

### Phase 51: Persistent Objects

**Goal:** Implement engine.scene.persist/unpersist Lua API backed by a PersistentObjectRegistry owned by SceneStateMachine — objects flagged as persistent survive scene transitions via a fixed-size ownership array, and engine.scene.find() searches the persistent registry in addition to the active scene
**Depends on:** Phase 48
**Requirements**: PERSIST-01, PERSIST-02, PERSIST-03
**Success Criteria** (what must be TRUE):
  1. A Lua script can call engine.scene.persist(proxy) and the referenced object survives a scene transition — its components continue updating and its scripts are not reloaded
  2. engine.scene.unpersist(proxy) removes the persistence flag and the object is destroyed on the next scene transition
  3. engine.scene.find(name) returns a valid proxy for a persistent object even when called from a scene that did not originally own that object
**Plans:** TBD

Plans:
- [ ] 51-01: PersistentObjectRegistry C++ — SSM-owned fixed array, ObjectCollection::m_external[] non-owning injection, applyDeferredTransition() modified, overflow test
- [ ] 51-02: engine.scene.persist/unpersist/find bindings — Lua integration tests (persist across transition, unpersist, find from new scene)

### Phase 52: UI Component Bindings

**Goal:** Add engine.ui.* Lua sub-table with four stateless immediate-mode draw functions (progressBar, statBar, panel, label) implemented as LuaCanvas fillRect/drawRect/text calls — bypassing the existing C++ Label/FillUpGauge components entirely due to std::string incompatibility with the zero-alloc Pixel4 pipeline — plus an internal guide for building new engine.ui.* components
**Depends on:** Phase 46
**Requirements**: UI-01, UI-02, UI-03, UI-04, UI-05
**Success Criteria** (what must be TRUE):
  1. A Lua script can call engine.ui.progressBar(x,y,w,h,value,fg,bg) and see a filled progress bar drawn to the canvas at the correct position and dimensions
  2. engine.ui.statBar(x,y,w,h,current,max,fg,bg) renders a proportional stat bar correctly for boundary values (0, max, and mid-range)
  3. engine.ui.panel(x,y,w,h,bg,border) draws a filled rectangle with a distinct border color; engine.ui.label(x,y,text,fg) renders text at the specified position
  4. All four draw calls are stateless — no per-call allocation, no retained state between frames
  5. An internal guide document exists explaining how to add a new engine.ui.* component (canvas call pattern, stateless contract, hot-reload considerations)
**Plans:** TBD

Plans:
- [ ] 52-01: bindings_ui.cpp — engine.ui.* sub-table (progressBar/statBar/panel/label), stateless LuaCanvas draw calls, resetUIState() in registerAll()
- [ ] 52-02: Internal guide — ui-component-guide.md documenting the stateless draw pattern, canvas call conventions, and hot-reload rules

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-6. Migration + Docs | v1.0 | 21/21 | Complete | 2026-02-01 |
| 7-15. Infrastructure | v1.1 | 17/17 | Complete | 2026-02-23 |
| 16-18. Tech Debt | v1.2 | 5/5 | Complete | 2026-02-23 |
| 19-22. Tomodachi Readiness | v1.3 | 7/7 | Complete | 2026-02-24 |
| 23-26. Engine Capabilities | v1.4 | 8/8 | Complete | 2026-02-26 |
| 27-38. Lua Scripting Foundation | v1.5 | 21/21 | Complete | 2026-02-28 |
| 39-42. Game Ready | v1.6 | 4/4 | Complete | 2026-02-28 |
| 43. Tilemap System | v1.7 | 2/2 | Complete | 2026-02-28 |
| 44. 2D Camera System | v1.7 | 2/2 | Complete | 2026-02-28 |
| 45. Physics Engine | v1.7 | 2/2 | Complete | 2026-03-01 |
| 46. Bindings Refactoring + Null Safety | 2/2 | Complete    | 2026-03-01 | - |
| 47. Debug Draw Bindings | 1/1 | Complete    | 2026-03-01 | - |
| 48. Camera Follow + Save/Load | v1.7 | 0/2 | Not started | - |
| 49. Coroutine/Async Scheduler | v1.7 | 0/2 | Not started | - |
| 50. Tween Helpers | v1.7 | 0/2 | Not started | - |
| 51. Persistent Objects | v1.7 | 0/2 | Not started | - |
| 52. UI Component Bindings | v1.7 | 0/2 | Not started | - |
