# Requirements: enjin2

**Defined:** 2026-02-28
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1.6 Requirements

Requirements for v1.6 Game Ready. Each maps to roadmap phases.

### Component Proxy

- [x] **PROXY-01**: Lua script can access sibling components via `self:get("TypeName")`
- [x] **PROXY-02**: Returned proxy is full userdata with typed method table (e.g., `timer:after()`, `fsm:setState()`)
- [x] **PROXY-03**: Component destruction invalidates all outstanding proxies (valid flag pattern)
- [x] **PROXY-04**: Stale ComponentProxy access raises `luaL_error` (not silent nil)

### Timer

- [x] **TIMER-01**: C_Timer component supports one-shot delayed callback via `timer:after(seconds, callback)`
- [x] **TIMER-02**: C_Timer component supports repeating callback via `timer:every(seconds, callback)`
- [x] **TIMER-03**: Timer can be cancelled via `timer:cancel(id)`
- [x] **TIMER-04**: Timer callbacks receive `self` (ScriptProxy) as first argument
- [x] **TIMER-05**: Timer `luaL_ref` handles are cleaned up on component destruction and hot-reload

### State Machine

- [x] **FSM-01**: C_StateMachine component supports named states with `fsm:addState(name, {enter, exit, update})`
- [x] **FSM-02**: State transitions via `fsm:setState(name)` with enter/exit callback invocation
- [x] **FSM-03**: Current state queryable via `fsm:getState()`
- [x] **FSM-04**: State transitions are deferred (applied after current frame's update, same as SceneStateMachine)
- [x] **FSM-05**: State update callback called each frame with `(self, dt)` while state is active

### Event Bus

- [x] **EVENT-01**: Lua scripts can register event handlers via `engine.event.on(name, callback)`
- [x] **EVENT-02**: Lua scripts can emit events via `engine.event.emit(name)`
- [x] **EVENT-03**: Handlers can be manually unregistered
- [x] **EVENT-04**: Event bus is scene-scoped — all handlers cleared on scene deactivation
- [x] **EVENT-05**: Handler `luaL_ref` handles cleaned up properly (no leaks across hot-reload)

### Tilemap

- [x] **TMAP-01**: C_Tilemap component stores a 64x64 uint8_t tile grid with SpriteSheet tileset (zero dynamic allocation)
- [x] **TMAP-02**: Viewport-culled draw() renders only visible tiles to ICanvas<Pixel4>
- [x] **TMAP-03**: Tile ID 0 is transparent (not drawn); IDs 1-255 are rendered via SpriteSheet::draw()
- [x] **TMAP-04**: Built-in camera offset (scrollX, scrollY) for tilemap-scoped scrolling
- [ ] **TMAP-05**: Lua proxy via self:get("C_Tilemap") with setTile/getTile/setTiles/setSheet/setScroll/getScroll/getMapSize
- [ ] **TMAP-06**: Coordinate conversion helpers: pixelToTile, tileToPixel, tileAtPixel
- [ ] **TMAP-07**: Map data initialized from flat Lua table via tilemap:setTiles(table, w, h)
- [ ] **TMAP-08**: setSheet(handle) uses sprite pool handle to bind tileset

### 2D Camera

- [ ] **CAM-01**: C_Camera component stores float-precision world position with setPosition/getPosition
- [ ] **CAM-02**: Camera offset applied to all C_Drawable components in Scene::renderObjects() via drawWithOffset()
- [ ] **CAM-03**: C_Drawable screen-space flag (setScreenSpace/isScreenSpace) to opt out of camera offset for UI elements
- [ ] **CAM-04**: Camera smooth follow via lookAt(x, y, lerpSpeed) — lerps toward target each frame in update()
- [ ] **CAM-05**: Camera screen shake via shake(intensity, duration) — sin oscillation with decay
- [ ] **CAM-06**: Camera viewport bounds clamping via setBounds/clearBounds
- [ ] **CAM-07**: Lua proxy via self:get("C_Camera") with setPosition/getPosition/lookAt/shake/setBounds/clearBounds
- [ ] **CAM-08**: engine.camera.* global Lua sub-table for scene-level camera access without ComponentProxy
- [ ] **CAM-09**: C_Tilemap drawWithOffset integrates camera offset additively with tilemap-scoped scroll

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
| PROXY-01 | Phase 39 | Complete |
| PROXY-02 | Phase 39 | Complete |
| PROXY-03 | Phase 39 | Complete |
| PROXY-04 | Phase 39 | Complete |
| TIMER-01 | Phase 40 | Complete |
| TIMER-02 | Phase 40 | Complete |
| TIMER-03 | Phase 40 | Complete |
| TIMER-04 | Phase 40 | Complete |
| TIMER-05 | Phase 40 | Complete |
| FSM-01 | Phase 41 | Complete |
| FSM-02 | Phase 41 | Complete |
| FSM-03 | Phase 41 | Complete |
| FSM-04 | Phase 41 | Complete |
| FSM-05 | Phase 41 | Complete |
| EVENT-01 | Phase 42 | Complete |
| EVENT-02 | Phase 42 | Complete |
| EVENT-03 | Phase 42 | Complete |
| EVENT-04 | Phase 42 | Complete |
| EVENT-05 | Phase 42 | Complete |
| TMAP-01 | Phase 43 | Complete |
| TMAP-02 | Phase 43 | Complete |
| TMAP-03 | Phase 43 | Complete |
| TMAP-04 | Phase 43 | Complete |
| TMAP-05 | Phase 43 | Planned |
| TMAP-06 | Phase 43 | Planned |
| TMAP-07 | Phase 43 | Planned |
| TMAP-08 | Phase 43 | Planned |
| CAM-01 | Phase 44 | Planned |
| CAM-02 | Phase 44 | Planned |
| CAM-03 | Phase 44 | Planned |
| CAM-04 | Phase 44 | Planned |
| CAM-05 | Phase 44 | Planned |
| CAM-06 | Phase 44 | Planned |
| CAM-07 | Phase 44 | Planned |
| CAM-08 | Phase 44 | Planned |
| CAM-09 | Phase 44 | Planned |

**Coverage:**
- v1.6 requirements: 19 total (complete)
- Phase 43 requirements: 8 total (4 complete in Plan 01, 4 planned in Plan 02)
- Phase 44 requirements: 9 total (planned)
- Mapped to phases: 36
- Unmapped: 0

---
*Requirements defined: 2026-02-28*
*Last updated: 2026-02-28 after Phase 43 Plan 01 completion (TMAP-01..TMAP-04 complete)*
