# Requirements: enjin2

**Defined:** 2026-03-01
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1.7 Requirements

Requirements for v1.7 Developer Experience & New Capability milestone. Each maps to roadmap phases.

### Bindings Refactoring

- [x] **BIND-01**: bindings.cpp split into focused files via bindings_internal.hpp
- [x] **BIND-02**: Null safety guards added to all binding chains (numeric returns default to 0, not nil)

### Testing & Quality

- [x] **TEST-01**: sprite_load_test.cpp compiles without errors (missing lua_wrapper.hpp resolved)
- [x] **TEST-02**: Overflow tests added for event bus, sprite pool, and component destruction

### Debug Draw

- [x] **DEBUG-01**: engine.debug.rect/circle/line/cross draw bindings route to dedicated debug canvas
- [x] **DEBUG-02**: engine.debug.text overlay binding for debug text display
- [x] **DEBUG-03**: engine.debug.enabled boolean toggle (zero cost when disabled)

### Camera Follow

- [x] **CAM-01**: engine.camera.follow(target, speed) resolves named object and tracks per-frame
- [x] **CAM-02**: engine.camera.stopFollow() clears follow target

### Save/Load

- [x] **STORE-01**: LuaStore JSON file I/O enabled for SDL3 builds (VCV_RACK guard replaced)
- [x] **STORE-02**: engine.store.flush() explicit save and engine.store.path() setter

### Coroutine/Async

- [ ] **ASYNC-01**: engine.async.start(fn) registers coroutine in 8-slot scheduler
- [ ] **ASYNC-02**: engine.async.wait(seconds) yields coroutine and resumes after delay
- [ ] **ASYNC-03**: engine.async.cancel(id) and engine.async.cancelAll() cleanup
- [ ] **ASYNC-04**: Coroutine library opened on ESP32 in openEmbeddedLibraries()

### Tween Helpers

- [ ] **TWEEN-01**: engine.tween.to(target, {props}, duration, easing, done_cb) animates Lua table fields
- [ ] **TWEEN-02**: engine.tween.cancel(id) and engine.tween.cancelAll() cleanup
- [ ] **TWEEN-03**: 4+ inline easing functions (linear, easeIn, easeOut, easeInOut) — no FPU-heavy math

### Persistent Objects

- [ ] **PERSIST-01**: engine.scene.persist(proxy) flags object to survive scene transitions
- [ ] **PERSIST-02**: engine.scene.unpersist(proxy) removes persistence flag
- [ ] **PERSIST-03**: engine.scene.find() searches persistent registry in addition to active scene

### UI Components

- [ ] **UI-01**: engine.ui.progressBar(x,y,w,h,value,fg,bg) stateless draw call
- [ ] **UI-02**: engine.ui.statBar(x,y,w,h,current,max,fg,bg) stateless draw call
- [ ] **UI-03**: engine.ui.panel(x,y,w,h,bg,border) stateless draw call
- [ ] **UI-04**: engine.ui.label(x,y,text,fg) stateless draw call
- [ ] **UI-05**: Internal guide document for building new engine.ui.* components

## Future Requirements

Deferred to future release. Tracked but not in current roadmap.

### Camera Enhancements

- **CAM-03**: Camera dead zone (rect within which camera does not follow)

### Save/Load Platform Expansion

- **STORE-03**: ESP32 NVS save path via Preferences API
- **STORE-04**: WASM localStorage bridge via JS interop

### Advanced Async

- **ASYNC-05**: Coroutine-aware tween await (engine.tween.await() yields until tween completes)
- **ASYNC-06**: engine.async.wait_frames(n) yield for frame-count delays

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| External tween libraries (flux.lua, tween.lua) | Breaks zero-alloc and ESP32 constraints; 4 inline functions sufficient |
| External JSON library | LuaStore handwritten writer is sufficient |
| Lua async frameworks (copas, luasocket) | Require sockets, incompatible with embedded targets |
| LVGL or layout engine for UI | Over-engineered for 128x128 pixel display |
| Unlimited persistent objects | Fixed static arrays are foundation of zero-alloc; 4-slot cap appropriate |
| Per-pixel alpha blending for UI | Incompatible with 4-bit indexed palette |
| Coroutine-per-object (unlimited) | Each lua_State thread holds stack memory; would exhaust ESP32 heap |
| std::string Label adaptation | engine.ui.* uses stateless draw calls; bypasses Label entirely |
| True async/threading for Lua | ESP32 and WASM have no pthreads; cooperative coroutines are correct |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BIND-01 | Phase 46 | Complete |
| BIND-02 | Phase 46 | Complete |
| TEST-01 | Phase 46 | Complete |
| TEST-02 | Phase 46 | Complete |
| DEBUG-01 | Phase 47 | Complete |
| DEBUG-02 | Phase 47 | Complete |
| DEBUG-03 | Phase 47 | Complete |
| CAM-01 | Phase 48 | Complete |
| CAM-02 | Phase 48 | Complete |
| STORE-01 | Phase 48 | Complete |
| STORE-02 | Phase 48 | Complete |
| ASYNC-01 | Phase 49 | Pending |
| ASYNC-02 | Phase 49 | Pending |
| ASYNC-03 | Phase 49 | Pending |
| ASYNC-04 | Phase 49 | Pending |
| TWEEN-01 | Phase 50 | Pending |
| TWEEN-02 | Phase 50 | Pending |
| TWEEN-03 | Phase 50 | Pending |
| PERSIST-01 | Phase 51 | Pending |
| PERSIST-02 | Phase 51 | Pending |
| PERSIST-03 | Phase 51 | Pending |
| UI-01 | Phase 52 | Pending |
| UI-02 | Phase 52 | Pending |
| UI-03 | Phase 52 | Pending |
| UI-04 | Phase 52 | Pending |
| UI-05 | Phase 52 | Pending |

**Coverage:**
- v1.7 requirements: 26 total
- Mapped to phases: 26
- Unmapped: 0

---
*Requirements defined: 2026-03-01*
*Last updated: 2026-03-01 after roadmap phases 46-52 created*
