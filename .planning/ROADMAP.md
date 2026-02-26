# Roadmap: Enjin Migration

## Overview

Complete migration from enjin to enjin2 with full independence, validation, and comprehensive documentation. enjin2 is now a self-contained library with Lua/WASM integration, non-dynamic memory allocation, and clean intelligent structure.

## Milestones

- ✅ **v1.0 Migration + Documentation** — Phases 1-6 (shipped 2026-02-01)
- ✅ **v1.1 Project Infrastructure & Documentation Enhancement** — Phases 7-15 (shipped 2026-02-23)
- ✅ **v1.2 Tech Debt Cleanup** — Phases 16-18 (shipped 2026-02-23)
- ✅ **v1.3 Tomodachi Readiness** — Phases 19-22 (shipped 2026-02-24)
- ✅ **v1.4 Engine Capabilities** — Phases 23-26 (shipped 2026-02-26)
- 🚧 **v1.5 Lua Scripting Foundation** — Phases 27-35 (in progress)

## Phases

<details>
<summary>✅ v1.0 Migration + Documentation (Phases 1-6) — SHIPPED 2026-02-01</summary>

**See full details:** [.planning/milestones/v1.0-ROADMAP.md](.planning/milestones/v1.0-ROADMAP.md)

- [x] Phase 1: Dependency Analysis (3/3 plans) — completed 2026-01-30
- [x] Phase 2: Core Migration (3/3 plans) — completed 2026-01-30
- [x] Phase 3: Feature Support (3/3 plans) — completed 2026-01-30
- [x] Phase 4: Validation (4/4 plans) — completed 2026-01-31
- [x] Phase 5: Final Cleanup (1/1 plan) — completed 2026-01-31
- [x] Phase 6: Create library docs, using doxygen + Docusaurus (7/7 plans) — completed 2026-02-01

**Total:** 6 phases, 21 plans, all complete
</details>

<details>
<summary>✅ v1.1 Project Infrastructure & Documentation Enhancement (Phases 7-15) — SHIPPED 2026-02-23</summary>

**See full details:** [.planning/milestones/v1.1-ROADMAP.md](.planning/milestones/v1.1-ROADMAP.md)

- [x] Phase 7: README Enhancement (1/1 plan) — completed 2026-02-02
- [x] Phase 8: Build System Fixes (2/2 plans) — completed 2026-02-03
- [x] Phase 9: Documentation Coverage (5/5 plans) — completed 2026-02-03
- [x] Phase 10: Module Overview Generation (2/2 plans) — completed 2026-02-03
- [x] Phase 11: Documentation Tracking Improvements (1/1 plan) — completed 2026-02-23
- [x] Phase 12: Fix Doxygen Warning Regression (2/2 plans) — completed 2026-02-23
- [x] Phase 13: Fix Documentation Pipeline & API Landing (2/2 plans) — completed 2026-02-23
- [x] Phase 14: Fix extractText() Cross-References (1/1 plan) — completed 2026-02-23
- [x] Phase 15: Cleanup CI and README Tech Debt (1/1 plan) — completed 2026-02-23

**Total:** 9 phases, 17 plans, all complete
</details>

<details>
<summary>✅ v1.2 Tech Debt Cleanup (Phases 16-18) — SHIPPED 2026-02-23</summary>

**See full details:** [.planning/milestones/v1.2-ROADMAP.md](.planning/milestones/v1.2-ROADMAP.md)

- [x] Phase 16: Repository Cleanup (2/2 plans) — completed 2026-02-23
- [x] Phase 17: Documentation Generation Fix (2/2 plans) — completed 2026-02-23
- [x] Phase 18: Build System Fix (1/1 plan) — completed 2026-02-23

**Total:** 3 phases, 5 plans, all complete
</details>

<details>
<summary>✅ v1.3 Tomodachi Readiness (Phases 19-22) — SHIPPED 2026-02-24</summary>

**See full details:** [.planning/milestones/v1.3-ROADMAP.md](.planning/milestones/v1.3-ROADMAP.md)

- [x] Phase 19: Palette Foundation (2/2 plans) — completed 2026-02-24
- [x] Phase 20: Input Abstraction (1/1 plan) — completed 2026-02-24
- [x] Phase 21: SDL3 CMake + Runner (2/2 plans) — completed 2026-02-24
- [x] Phase 22: Lua Integration + E2E Validation (2/2 plans) — completed 2026-02-24

**Total:** 4 phases, 7 plans, all complete
</details>

<details>
<summary>✅ v1.4 Engine Capabilities (Phases 23-26) — SHIPPED 2026-02-26</summary>

**See full details:** [.planning/milestones/v1.4-ROADMAP.md](.planning/milestones/v1.4-ROADMAP.md)

- [x] Phase 23: Docusaurus Navigation Fix (1/1 plan) — completed 2026-02-24
- [x] Phase 24: Sprite System Rework (3/3 plans) — completed 2026-02-24
- [x] Phase 25: Multi-Layer Canvas Composition (3/3 plans) — completed 2026-02-26
- [x] Phase 26: Lua Hot Reload (1/1 plan) — completed 2026-02-26

**Total:** 4 phases, 8 plans, all complete
</details>

### v1.5 Lua Scripting Foundation (Phases 27-35)

**Milestone Goal:** Transform enjin2 from a rendering toolkit into a fully scriptable game runtime — C++ foundations (float dt, named objects, scene self-transitions) plus the complete Lua scripting surface (engine.* table, self proxy, input events, error policy, GC control, dependency assertions).

- [ ] **Phase 27: Fix onRender Pixel4 Bug** - Correctness fix: scene-derived onRender fires for Pixel4 canvas
- [x] **Phase 28: float dt Migration** - Pervasive signature change: uint16_t ms -> float seconds everywhere (completed 2026-02-26)
- [ ] **Phase 29: Named Objects + Tags** - Object name field and tag array with collection lookup methods
- [ ] **Phase 30: Scene Self-Transitions** - SceneStateMachine* injection into Scene; deferred self-transition support
- [ ] **Phase 31: engine.* Global Table** - Complete engine.scene/input/time/lua/log Lua namespace
- [ ] **Phase 32: ScriptProxy Userdata** - self as first callback arg; x/y/visible/layer/name mapped to C++
- [ ] **Phase 33: ScriptErrorPolicy** - Disable/Log/Panic enum on C_LuaScript with hot-reload reset
- [ ] **Phase 34: Input Event Callbacks** - on_button_pressed/on_button_released Lua callbacks
- [ ] **Phase 35: GC Control + Component Assertions** - engine.lua.collect/memory and assertRequires<T>()

## Phase Details

### Phase 27: Fix onRender Pixel4 Bug
**Goal**: Scene-derived onRender(ICanvas<Pixel4>&) is called correctly during Scene::render() for all derived scenes
**Depends on**: Phase 26 (correctness precondition for all subsequent rendering tests)
**Requirements**: RENDER-01
**Success Criteria** (what must be TRUE):
  1. A derived scene with an onRender override receives a Pixel4 canvas argument and can draw to it
  2. The engine does not silently skip onRender on Pixel4 canvas — pixels written in onRender appear in the rendered output
  3. All existing scene rendering tests pass without false negatives
**Plans**: TBD

### Phase 28: float dt Migration
**Goal**: float dt in seconds flows consistently through the entire C++ update chain — Object, Component, Scene, SceneStateMachine, and all subclasses
**Depends on**: Phase 27
**Requirements**: DT-01, DT-02, DT-03
**Success Criteria** (what must be TRUE):
  1. update() on every Object, Component, and Scene receives float dt in seconds, not uint16_t milliseconds
  2. All concrete component subclasses compile without override warnings under -Woverride on all three platform builds
  3. No / 1000 division patterns remain in the update call chain — delta time is seconds at the source
  4. A component that accumulates dt to a timer (e.g., 1.0f = one second) works correctly without conversion
**Plans:** 2/2 plans complete
- [ ] 28-01-PLAN.md — Migrate all update signatures to float dt, convert accumulators, remove /1000 divisions
- [ ] 28-02-PLAN.md — Add -Woverride to CMake targets and verify clean build

### Phase 29: Named Objects + Tags
**Goal**: Objects can be named and tagged at construction, and scripts/code can locate them by name or tag with no heap allocation
**Depends on**: Phase 28
**Requirements**: OBJ-01, OBJ-02, OBJ-03, OBJ-04
**Success Criteria** (what must be TRUE):
  1. An Object can be constructed or mutated with a name string and that name is retrievable via getter
  2. ObjectCollection::findByName("player") returns the matching Object or nullptr when no match exists
  3. An Object can hold up to 8 string literal tag pointers with zero heap allocation
  4. ObjectCollection::findAllWithTag("enemy") returns all Objects carrying that tag
**Plans**: TBD

### Phase 30: Scene Self-Transitions
**Goal**: A scene can request a transition to any other scene — including itself — from within its own update callback, executing correctly without re-entrancy or initialization guard bypass
**Depends on**: Phase 29
**Requirements**: SCENE-01, SCENE-02, SCENE-03
**Success Criteria** (what must be TRUE):
  1. A derived Scene holds a non-owning SceneStateMachine* pointer available during onUpdate()
  2. Calling scene->switchTo(id) from within onUpdate() defers the transition until after the current frame completes
  3. Switching a scene to itself triggers a full reset and re-initialization (onCreate() is called again, not skipped by the initialized guard)
  4. Scene transitions initiated from within onDeactivate() do not cause re-entrant state machine corruption
**Plans**: TBD

### Phase 31: engine.* Global Table
**Goal**: Lua scripts access a fully-populated engine.* namespace with scene control, input polling, time, logging, and GC sub-tables available before any script loads
**Depends on**: Phase 30 (engine.scene.switch needs SSM pointer from Phase 30; engine.scene.find needs findByName from Phase 29)
**Requirements**: ENG-01, ENG-02, ENG-03, ENG-04, ENG-05, ENG-06
**Success Criteria** (what must be TRUE):
  1. A Lua script accessing engine.scene, engine.input, engine.time, engine.lua, or engine.log at module level (outside any function) receives a valid table, not nil
  2. engine.scene.switch(id) triggers a deferred scene transition in the running engine
  3. engine.scene.find("name") returns a proxy for a named object or nil when no match exists
  4. engine.input.held(btn), engine.input.just_pressed(btn), engine.input.axis(n) return correct values based on the current frame's input state
  5. engine.time.delta() returns the current frame's float dt in seconds; engine.time.frame() returns the frame counter
  6. engine.log("msg") outputs the message via the platform logging channel without crashing on any target
**Plans**: TBD

### Phase 32: ScriptProxy Userdata
**Goal**: Every Lua callback receives self as its first argument, and scripts can read and write component properties through self with dangling-pointer safety after object destruction
**Depends on**: Phase 31 (engine.* table wiring establishes SDL runner callback injection path; Object::name from Phase 29 needed for self.name)
**Requirements**: PROXY-01, PROXY-02, PROXY-03, PROXY-04
**Success Criteria** (what must be TRUE):
  1. init(self), update(self, dt), and draw(self) all receive a ScriptProxy userdata as their first argument
  2. Scripts can read and write self.x, self.y, self.visible, self.layer, self.name, and self.active with changes reflected immediately in the C++ component
  3. Storing self in a Lua upvalue across frames does not crash when the underlying Object is destroyed — the proxy detects invalidity and returns nil or raises a safe error
  4. All existing Lua scripts (reload_test.lua, layer_demo.lua, pikachu_demo.lua, e2e_parity.lua) run correctly with the new (self, ...) callback signature
**Plans**: TBD

### Phase 33: ScriptErrorPolicy
**Goal**: C_LuaScript has a configurable error policy that controls how Lua errors are handled, with hot-reload clearing the error state
**Depends on**: Phase 32 (shares C_LuaScript callback infrastructure; float dt from Phase 28 required)
**Requirements**: ERR-01, ERR-02, ERR-03, ERR-04, ERR-05
**Success Criteria** (what must be TRUE):
  1. C_LuaScript exposes a ScriptErrorPolicy field that can be set to Disable, Log, or Panic before script execution
  2. With Disable policy: a Lua runtime error disables the script, logs once, and the engine continues running all other scripts and rendering
  3. With Log policy: a Lua runtime error logs the error every frame but the script continues executing on subsequent frames
  4. With Panic policy: a Lua runtime error invokes the platform panic handler (halts on ESP32, asserts on desktop)
  5. After F5 hot-reload, a previously disabled script's error state is cleared and the script executes again from a fresh Lua state
**Plans**: TBD

### Phase 34: Input Event Callbacks
**Goal**: Lua scripts can respond to button press and release edges via named callbacks that fire in the correct frame order
**Depends on**: Phase 31 (engine.* table context; input polling infrastructure from Phase 22)
**Requirements**: INPUT-01, INPUT-02, INPUT-03
**Success Criteria** (what must be TRUE):
  1. A Lua script defining on_button_pressed(btn) has that function called with the button index on the frame the button transitions from not-pressed to pressed
  2. A Lua script defining on_button_released(btn) has that function called with the button index on the frame the button transitions from pressed to not-pressed
  3. Input event callbacks fire after input polling completes and before update() is called in the same frame — never stale, never from the OS event pump
**Plans**: TBD

### Phase 35: GC Control + Component Assertions
**Goal**: Scripts can control the Lua GC explicitly for embedded frame-budget management, and components can declare their dependencies with assertions that fail loudly in debug and gracefully in release
**Depends on**: Phase 31 (engine.lua subtable created in Phase 31 for GC registration); Phase 28 (float dt required for Component base class compilation)
**Requirements**: GC-01, GC-02, DEP-01, DEP-02, DEP-03
**Success Criteria** (what must be TRUE):
  1. engine.lua.collect() triggers an incremental GC step without causing a mid-frame spike on embedded targets
  2. engine.lua.memory() returns the current Lua heap size in bytes as a number
  3. A component calling assertRequires<C_Sprite>() in its awake() causes an assertion failure at startup in debug builds when C_Sprite is not present on the same object
  4. In release builds, a missing required component logs once and disables the dependent component without aborting the process
**Plans**: TBD

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Dependency Analysis | v1.0 | 3/3 | Complete | 2026-01-30 |
| 2. Core Migration | v1.0 | 3/3 | Complete | 2026-01-30 |
| 3. Feature Support | v1.0 | 3/3 | Complete | 2026-01-30 |
| 4. Validation | v1.0 | 4/4 | Complete | 2026-01-31 |
| 5. Final Cleanup | v1.0 | 1/1 | Complete | 2026-01-31 |
| 6. Create library docs | v1.0 | 7/7 | Complete | 2026-02-01 |
| 7. README Enhancement | v1.1 | 1/1 | Complete | 2026-02-02 |
| 8. Build System Fixes | v1.1 | 2/2 | Complete | 2026-02-03 |
| 9. Documentation Coverage | v1.1 | 5/5 | Complete | 2026-02-03 |
| 10. Module Overview Generation | v1.1 | 2/2 | Complete | 2026-02-03 |
| 11. Documentation Tracking | v1.1 | 1/1 | Complete | 2026-02-23 |
| 12. Fix Doxygen Warnings | v1.1 | 2/2 | Complete | 2026-02-23 |
| 13. Fix Doc Pipeline & API | v1.1 | 2/2 | Complete | 2026-02-23 |
| 14. Fix extractText() | v1.1 | 1/1 | Complete | 2026-02-23 |
| 15. Cleanup CI/README | v1.1 | 1/1 | Complete | 2026-02-23 |
| 16. Repository Cleanup | v1.2 | 2/2 | Complete | 2026-02-23 |
| 17. Doc Generation Fix | v1.2 | 2/2 | Complete | 2026-02-23 |
| 18. Build System Fix | v1.2 | 1/1 | Complete | 2026-02-23 |
| 19. Palette Foundation | v1.3 | 2/2 | Complete | 2026-02-24 |
| 20. Input Abstraction | v1.3 | 1/1 | Complete | 2026-02-24 |
| 21. SDL3 CMake + Runner | v1.3 | 2/2 | Complete | 2026-02-24 |
| 22. Lua Integration + E2E | v1.3 | 2/2 | Complete | 2026-02-24 |
| 23. Docusaurus Navigation Fix | v1.4 | 1/1 | Complete | 2026-02-24 |
| 24. Sprite System Rework | v1.4 | 3/3 | Complete | 2026-02-24 |
| 25. Multi-Layer Canvas Composition | v1.4 | 3/3 | Complete | 2026-02-26 |
| 26. Lua Hot Reload | v1.4 | 1/1 | Complete | 2026-02-26 |
| 27. Fix onRender Pixel4 Bug | v1.5 | 0/? | Not started | - |
| 28. float dt Migration | 2/2 | Complete    | 2026-02-26 | - |
| 29. Named Objects + Tags | v1.5 | 0/? | Not started | - |
| 30. Scene Self-Transitions | v1.5 | 0/? | Not started | - |
| 31. engine.* Global Table | v1.5 | 0/? | Not started | - |
| 32. ScriptProxy Userdata | v1.5 | 0/? | Not started | - |
| 33. ScriptErrorPolicy | v1.5 | 0/? | Not started | - |
| 34. Input Event Callbacks | v1.5 | 0/? | Not started | - |
| 35. GC Control + Component Assertions | v1.5 | 0/? | Not started | - |

**Total Progress: 58/58 plans complete across v1.0-v1.4 + v1.5 in progress**
