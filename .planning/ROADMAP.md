# Roadmap: enjin2

## Milestones

- [x] **v1.0 Migration + Documentation** - Phases 1-6 (shipped 2026-02-01)
- [x] **v1.1 Project Infrastructure & Documentation Enhancement** - Phases 7-15 (shipped 2026-02-23)
- [x] **v1.2 Tech Debt Cleanup** - Phases 16-18 (shipped 2026-02-23)
- [x] **v1.3 Tomodachi Readiness** - Phases 19-22 (shipped 2026-02-24)
- [x] **v1.4 Engine Capabilities** - Phases 23-26 (shipped 2026-02-26)
- [x] **v1.5 Lua Scripting Foundation** - Phases 27-38 (shipped 2026-02-28)
- [ ] **v1.6 Game Ready** - Phases 39-42 (in progress)

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

### v1.6 Game Ready (In Progress)

**Milestone Goal:** Make enjin2 capable of building complete small games (Arkanoid, physics sandbox, tamagotchi) purely from Lua on SDL3.

- [ ] **Phase 39: ComponentProxy** - Lua scripts access sibling components via `self:get("TypeName")`
- [ ] **Phase 40: C_Timer** - Delayed and repeating Lua callbacks via `timer:after()` / `timer:every()`
- [ ] **Phase 41: C_StateMachine** - Named states with enter/update/exit Lua callbacks and deferred transitions
- [ ] **Phase 42: EventBus** - Scene-scoped publish/subscribe event bus exposed as `engine.event.*`

## Phase Details

### Phase 39: ComponentProxy
**Goal**: Lua scripts can retrieve typed proxies to sibling components on the same object
**Depends on**: Phase 38 (ScriptProxy, ObjectProxy, live-wiring)
**Requirements**: PROXY-01, PROXY-02, PROXY-03, PROXY-04
**Success Criteria** (what must be TRUE):
  1. A Lua script can call `self:get("C_Timer")` in `init()` and receive a valid proxy with component-specific methods (e.g., `timer:after()`)
  2. Accessing a proxy after its component is destroyed raises a `luaL_error` (not silent nil or crash)
  3. `Component` base class carries the proxy registration field; `Component::~Component()` marks all outstanding proxies invalid
  4. `ScriptProxy.__index` dispatch checks `"get"` first before any property or method lookup, preventing name collision
**Plans**: TBD

### Phase 40: C_Timer
**Goal**: Objects can schedule one-shot and repeating Lua callbacks without busy-polling in update()
**Depends on**: Phase 39 (ComponentProxy — timer proxy returned by `self:get()`)
**Requirements**: TIMER-01, TIMER-02, TIMER-03, TIMER-04, TIMER-05
**Success Criteria** (what must be TRUE):
  1. A Lua script can call `timer:after(2.0, fn)` and have `fn(self)` called once after 2 seconds
  2. A Lua script can call `timer:every(0.5, fn)` and have `fn(self)` called repeatedly every 0.5 seconds
  3. A timer can be cancelled by ID before it fires; cancelled timers do not invoke their callback
  4. All `luaL_ref` handles are released on component destruction and on hot-reload, leaving no dangling refs
**Plans**: TBD

### Phase 41: C_StateMachine
**Goal**: Objects can manage per-object named states with Lua enter/update/exit callbacks and deferred transitions
**Depends on**: Phase 39 (ComponentProxy — FSM proxy returned by `self:get()`)
**Requirements**: FSM-01, FSM-02, FSM-03, FSM-04, FSM-05
**Success Criteria** (what must be TRUE):
  1. A Lua script can define named states with `fsm:addState(name, {enter, exit, update})` and transition between them with `fsm:setState(name)`
  2. Entering a state invokes its `enter(self)` callback; leaving invokes `exit(self)`; the active state's `update(self, dt)` is called each frame
  3. `fsm:getState()` returns the name of the currently active state
  4. State transitions called during `update()` or a callback take effect on the next frame (deferred), preventing re-entrant FSM corruption
**Plans**: TBD

### Phase 42: EventBus
**Goal**: Lua scripts on different objects can communicate via named events without direct object references
**Depends on**: Phase 38 (engine.* table infrastructure for adding engine.event sub-table)
**Requirements**: EVENT-01, EVENT-02, EVENT-03, EVENT-04, EVENT-05
**Success Criteria** (what must be TRUE):
  1. A script can call `engine.event.on("brick_hit", fn)` and have `fn()` invoked when another script calls `engine.event.emit("brick_hit")`
  2. Handlers can be unregistered by subscription ID before the scene ends
  3. All event handlers are automatically cleared when the scene deactivates — no cross-scene stale callbacks
  4. All `luaL_ref` handles are released on hot-reload, leaving no dangling refs after F5
**Plans**: TBD

## Progress

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-6. Migration + Docs | v1.0 | 21/21 | Complete | 2026-02-01 |
| 7-15. Infrastructure | v1.1 | 17/17 | Complete | 2026-02-23 |
| 16-18. Tech Debt | v1.2 | 5/5 | Complete | 2026-02-23 |
| 19-22. Tomodachi Readiness | v1.3 | 7/7 | Complete | 2026-02-24 |
| 23-26. Engine Capabilities | v1.4 | 8/8 | Complete | 2026-02-26 |
| 27-38. Lua Scripting Foundation | v1.5 | 21/21 | Complete | 2026-02-28 |
| 39. ComponentProxy | v1.6 | 0/? | Not started | - |
| 40. C_Timer | v1.6 | 0/? | Not started | - |
| 41. C_StateMachine | v1.6 | 0/? | Not started | - |
| 42. EventBus | v1.6 | 0/? | Not started | - |
