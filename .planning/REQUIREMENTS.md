# Requirements: enjin2 v1.8 Ship Ready

**Defined:** 2026-03-02
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1 Requirements

Requirements for v1.8 milestone. Each maps to roadmap phases.

### Build Infrastructure

- [x] **BLDINFRA-01**: Developer can run a single setup script on Arch Linux to install Emscripten (emsdk 3.1.73) and ESP-IDF (v5.5) toolchains
- [x] **BLDINFRA-02**: Developer can build for SDL3, WASM, or ESP32 via `build.sh --target [sdl3|wasm|esp32]` helper scripts
- [x] **BLDINFRA-03**: Build scripts detect `$EMSDK` environment variable and fall back gracefully with actionable error

### Platform Verification

- [ ] **PLAT-01**: All v1.7 features compile under Emscripten and produce `.js` + `.wasm` output
- [ ] **PLAT-02**: All v1.7 features compile under ESP-IDF and produce flashable firmware
- [ ] **PLAT-03**: ESP32 layer count decision documented in code (`ENJIN_LAYER_COUNT` set appropriately for target hardware)

### Platform Storage

- [ ] **STORE-01**: `writeStoreToBuffer()` helper extracted from existing `saveToFile` for shared JSON serialization
- [ ] **STORE-02**: `engine.store.save/flush/load` works on WASM via localStorage with flush-only persistence pattern
- [ ] **STORE-03**: `engine.store.save/flush/load` works on ESP32 via NVS with single JSON blob and `nvs_commit()`
- [ ] **STORE-04**: NVS backend validates and rejects keys longer than 15 characters

### Tech Debt

- [x] **DEBT-01**: `m_followTargetProxy` cleared on scene transition and hot reload (no dangling reference)
- [x] **DEBT-02**: `engine.scene.persist()` emits `lua_warning()` when called without SceneStateMachine context

### QoL Features

- [x] **QOL-01**: `engine.tween.await()` inside a coroutine suspends until tween completes (polling implementation)
- [x] **QOL-02**: `engine.async.wait_frames(n)` Lua helper yields for exactly n frames
- [x] **QOL-03**: `engine.camera.setDeadZone(w, h)` with offset-clamp boundary tracking

### Documentation

- [ ] **DOC-01**: Getting Started guide updated to reflect v1.7+ SDL3 runner API (no stale Canvas8 references)
- [ ] **DOC-02**: "Your First Script" Docusaurus tutorial based on tamagotchi.lua walkthrough
- [ ] **DOC-03**: "Async Coroutines" Docusaurus tutorial covering engine.async + engine.tween.await
- [ ] **DOC-04**: Lua syntax highlighting enabled in Docusaurus (`'lua'` in prism additionalLanguages)

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Platform Storage (Advanced)

- **STORE-05**: WASM OPFS/IndexedDB storage for large save data
- **STORE-06**: ESP32 hot reload via SPIFFS

### Documentation (Advanced)

- **DOC-05**: Interactive WASM demo embedded in docs site
- **DOC-06**: API doc inline usage examples (extract from tutorials)

## Out of Scope

| Feature | Reason |
|---------|--------|
| ASYNCIFY for WASM coroutines | 50%+ binary bloat; existing tickCoroutines + lua_resume works correctly on WASM |
| IndexedDB/IDBFS storage | Async API conflicts with synchronous saveToFile signature; localStorage sufficient for 16-key LuaStore |
| ESP-IDF v6.0-beta | Use stable v5.5.x; beta introduces unnecessary risk |
| Per-key NVS storage | NVS key enumeration is diagnostic-only; single JSON blob is correct |
| System Emscripten (pacman -S) | Not version-pinned; use manually-cloned emsdk for reproducibility |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BLDINFRA-01 | Phase 53 | Complete |
| BLDINFRA-02 | Phase 53 | Complete |
| BLDINFRA-03 | Phase 53 | Complete |
| PLAT-01 | Phase 53 | Pending |
| PLAT-02 | Phase 53 | Pending |
| PLAT-03 | Phase 53 | Pending |
| STORE-01 | Phase 54 | Pending |
| STORE-02 | Phase 55 | Pending |
| STORE-03 | Phase 55 | Pending |
| STORE-04 | Phase 55 | Pending |
| DEBT-01 | Phase 56 | Complete |
| DEBT-02 | Phase 56 | Complete |
| QOL-01 | Phase 57 | Complete |
| QOL-02 | Phase 57 | Complete |
| QOL-03 | Phase 57 | Complete |
| DOC-01 | Phase 58 | Pending |
| DOC-02 | Phase 58 | Pending |
| DOC-03 | Phase 58 | Pending |
| DOC-04 | Phase 58 | Pending |

**Coverage:**
- v1 requirements: 19 total
- Mapped to phases: 19
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-02*
*Last updated: 2026-03-02 after roadmap creation (phases 53-58)*
