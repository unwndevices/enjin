# Requirements: enjin2

**Defined:** 2026-02-28
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1.6 Requirements

Requirements for v1.6 Game Ready. Each maps to roadmap phases.

### Component Proxy

- [ ] **PROXY-01**: Lua script can access sibling components via `self:get("TypeName")`
- [ ] **PROXY-02**: Returned proxy is full userdata with typed method table (e.g., `timer:after()`, `fsm:setState()`)
- [ ] **PROXY-03**: Component destruction invalidates all outstanding proxies (valid flag pattern)
- [ ] **PROXY-04**: Stale ComponentProxy access raises `luaL_error` (not silent nil)

### Timer

- [ ] **TIMER-01**: C_Timer component supports one-shot delayed callback via `timer:after(seconds, callback)`
- [ ] **TIMER-02**: C_Timer component supports repeating callback via `timer:every(seconds, callback)`
- [ ] **TIMER-03**: Timer can be cancelled via `timer:cancel(id)`
- [ ] **TIMER-04**: Timer callbacks receive `self` (ScriptProxy) as first argument
- [ ] **TIMER-05**: Timer `luaL_ref` handles are cleaned up on component destruction and hot-reload

### State Machine

- [ ] **FSM-01**: C_StateMachine component supports named states with `fsm:addState(name, {enter, exit, update})`
- [ ] **FSM-02**: State transitions via `fsm:setState(name)` with enter/exit callback invocation
- [ ] **FSM-03**: Current state queryable via `fsm:getState()`
- [ ] **FSM-04**: State transitions are deferred (applied after current frame's update, same as SceneStateMachine)
- [ ] **FSM-05**: State update callback called each frame with `(self, dt)` while state is active

### Event Bus

- [ ] **EVENT-01**: Lua scripts can register event handlers via `engine.event.on(name, callback)`
- [ ] **EVENT-02**: Lua scripts can emit events via `engine.event.emit(name)`
- [ ] **EVENT-03**: Handlers can be manually unregistered
- [ ] **EVENT-04**: Event bus is scene-scoped — all handlers cleared on scene deactivation
- [ ] **EVENT-05**: Handler `luaL_ref` handles cleaned up properly (no leaks across hot-reload)

## Future Requirements

Deferred to future release. Tracked but not in current roadmap.

### Persistence

- **PERSIST-01**: Objects survive scene transitions via SSM-level persistent ObjectCollection
- **PERSIST-02**: `engine.scene.find()` searches persistent collection as fallback
- **PERSIST-03**: `engine.scene.spawn_persistent()` creates SSM-owned objects

*Note: LuaStore (engine.store.*) may already cover game-level persistence needs (score, lives, pet stats). Verify with target games before implementing full persistent object registry.*

### Platform Parity

- **PLAT-01**: `C_LuaScript::setInput()` called per-frame in WASM host path
- **PLAT-02**: `C_LuaScript::setInput()` called per-frame in ESP32 host path

### Layer System

- **LAYER-01**: Integer layer system replacing current approach

## Out of Scope

| Feature | Reason |
|---------|--------|
| Event bus data payload | Adds complexity; basic on/emit sufficient for v1.6 target games |
| WASM/ESP32 hot reload | Developer tool for SDL3 runner only |
| Integer layer system rework | Independent change, not needed for target games |
| Game demo scripts | User builds games as validation; engine capabilities only |
| Full persistent object registry | Verify LuaStore sufficiency first; defer to v1.7 if needed |
| Component signals (C++ Signal<T>) | Different from Lua event bus; C++ side not needed for Lua games |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PROXY-01 | Phase 39 | Pending |
| PROXY-02 | Phase 39 | Pending |
| PROXY-03 | Phase 39 | Pending |
| PROXY-04 | Phase 39 | Pending |
| TIMER-01 | Phase 40 | Pending |
| TIMER-02 | Phase 40 | Pending |
| TIMER-03 | Phase 40 | Pending |
| TIMER-04 | Phase 40 | Pending |
| TIMER-05 | Phase 40 | Pending |
| FSM-01 | Phase 41 | Pending |
| FSM-02 | Phase 41 | Pending |
| FSM-03 | Phase 41 | Pending |
| FSM-04 | Phase 41 | Pending |
| FSM-05 | Phase 41 | Pending |
| EVENT-01 | Phase 42 | Pending |
| EVENT-02 | Phase 42 | Pending |
| EVENT-03 | Phase 42 | Pending |
| EVENT-04 | Phase 42 | Pending |
| EVENT-05 | Phase 42 | Pending |

**Coverage:**
- v1.6 requirements: 19 total
- Mapped to phases: 19
- Unmapped: 0

---
*Requirements defined: 2026-02-28*
*Last updated: 2026-02-28 after roadmap creation (Phases 39-42)*
