# Roadmap: enjin2

## Milestones

- [x] **v1.0 Migration + Documentation** - Phases 1-6 (shipped 2026-02-01)
- [x] **v1.1 Project Infrastructure & Documentation Enhancement** - Phases 7-15 (shipped 2026-02-23)
- [x] **v1.2 Tech Debt Cleanup** - Phases 16-18 (shipped 2026-02-23)
- [x] **v1.3 Tomodachi Readiness** - Phases 19-22 (shipped 2026-02-24)
- [x] **v1.4 Engine Capabilities** - Phases 23-26 (shipped 2026-02-26)
- [x] **v1.5 Lua Scripting Foundation** - Phases 27-38 (shipped 2026-02-28)
- [x] **v1.6 Game Ready** - Phases 39-42 (shipped 2026-02-28)
- [x] **v1.7 Developer Experience & New Capability** - Phases 43-52 (shipped 2026-03-02)
- 🚧 **v1.8 Ship Ready** - Phases 53-58 (in progress)

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

<details>
<summary>v1.7 Developer Experience & New Capability (Phases 43-52) - SHIPPED 2026-03-02</summary>

Phases 43-52 complete. See milestones/v1.7-ROADMAP.md for full detail.

</details>

### v1.8 Ship Ready (In Progress)

**Milestone Goal:** Make enjin2 deployable on all 3 targets (SDL3, WASM, ESP32), clean up tech debt, and provide onboarding documentation.

- [ ] **Phase 53: Environment and Build Verification** - Dev setup script + confirm all three platforms compile
- [x] **Phase 54: JSON Serializer Refactor** - Extract shared writeStoreToBuffer() helper from saveToFile (completed 2026-03-02)
- [ ] **Phase 55: Platform Storage Backends** - WASM localStorage + ESP32 NVS implementations
- [ ] **Phase 56: Tech Debt Cleanup** - Clear m_followTargetProxy dangling ref + honest persist() warning
- [ ] **Phase 57: QoL Features** - tween.await(), wait_frames(), camera dead zone
- [ ] **Phase 58: Documentation and Build Tooling** - Getting Started update + tutorials + Lua highlighting

## Phase Details

### Phase 53: Environment and Build Verification
**Goal**: Developer can set up the full toolchain and verify all three platform builds succeed
**Depends on**: Nothing (first phase of milestone)
**Requirements**: BLDINFRA-01, BLDINFRA-02, BLDINFRA-03, PLAT-01, PLAT-02, PLAT-03
**Success Criteria** (what must be TRUE):
  1. Developer runs `scripts/setup-dev.sh` on Arch Linux and Emscripten 3.1.73 plus ESP-IDF v5.5 are installed and activated
  2. `build.sh --target wasm` produces `.js` and `.wasm` output with all v1.7 features compiling under Emscripten
  3. `build.sh --target esp32` produces flashable firmware with all v1.7 features compiling under ESP-IDF
  4. `build.sh --target sdl3` succeeds; running without `$EMSDK` set prints an actionable error message
  5. `ENJIN_LAYER_COUNT` for ESP32 target is set and documented in code with PSRAM rationale
**Plans**: 3

Plans:
- [ ] 53-01: Setup script (scripts/setup-dev.sh — Emscripten 3.1.73 + ESP-IDF v5.5 to XDG paths)
- [ ] 53-02: Unified build script (build.sh — replaces build_wasm.sh, all three targets)
- [ ] 53-03: Platform verification + ENJIN_LAYER_COUNT fix (compile SDL3/WASM/ESP32, #ifdef ESP32 guard)

### Phase 54: JSON Serializer Refactor
**Goal**: Shared JSON serialization helper extracted and verified, unlocking both storage backends
**Depends on**: Phase 53
**Requirements**: STORE-01
**Success Criteria** (what must be TRUE):
  1. `writeStoreToBuffer(char* out, size_t cap)` exists and produces the same JSON as the existing `saveToFile` SDL3 path
  2. Existing SDL3 `engine.store.flush/load` behavior is unchanged after refactor
**Plans**: TBD

Plans:
- [ ] 54-01: TBD

### Phase 55: Platform Storage Backends
**Goal**: engine.store persists data across page reloads on WASM and across power cycles on ESP32
**Depends on**: Phase 54
**Requirements**: STORE-02, STORE-03, STORE-04
**Success Criteria** (what must be TRUE):
  1. On WASM, `engine.store.save()` + `engine.store.flush()` persists values that survive a page reload and are readable via `engine.store.load()`
  2. On ESP32, `engine.store.save()` + `engine.store.flush()` persists values that survive a power cycle and are readable via `engine.store.load()`
  3. Calling `engine.store.save()` with a key longer than 15 characters on ESP32 is rejected with a Lua error, not silently truncated
**Plans**: TBD

Plans:
- [ ] 55-01: TBD

### Phase 56: Tech Debt Cleanup
**Goal**: Two latent correctness issues eliminated — no dangling camera proxy after scene change, no silent persist no-op
**Depends on**: Phase 53
**Requirements**: DEBT-01, DEBT-02
**Success Criteria** (what must be TRUE):
  1. Switching scenes or triggering hot reload while camera follow is active does not leave a stale `m_followTargetProxy` reference
  2. Calling `engine.scene.persist()` in a script running without SceneStateMachine context prints a `lua_warning()` instead of silently doing nothing
**Plans**: TBD

Plans:
- [ ] 56-01: TBD

### Phase 57: QoL Features
**Goal**: Coroutines can await tween completion, scripts can yield for N frames, camera follow has a configurable dead zone
**Depends on**: Phase 53
**Requirements**: QOL-01, QOL-02, QOL-03
**Success Criteria** (what must be TRUE):
  1. `engine.tween.await(id)` inside a coroutine suspends execution until the tween with that id completes, then resumes exactly once
  2. `engine.async.wait_frames(n)` yields the coroutine for exactly n frames before resuming
  3. `engine.camera.setDeadZone(w, h)` causes the camera to stop following the target when the target is within the dead zone boundary, and resumes smooth follow once the target exits it
**Plans**: TBD

Plans:
- [ ] 57-01: TBD

### Phase 58: Documentation and Build Tooling
**Goal**: A new developer can read Getting Started, follow a tutorial, and know the engine's async capabilities
**Depends on**: Phase 57
**Requirements**: DOC-01, DOC-02, DOC-03, DOC-04
**Success Criteria** (what must be TRUE):
  1. The Getting Started guide references the SDL3 runner API and contains no stale Canvas8 mentions
  2. The "Your First Script" tutorial walks through tamagotchi.lua with Lua code blocks that render with syntax highlighting
  3. The "Async Coroutines" tutorial demonstrates `engine.async` and `engine.tween.await()` with working code examples
  4. Lua code blocks in any Docusaurus page render with syntax highlighting (not plain text)
**Plans**: TBD

Plans:
- [ ] 58-01: TBD

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
| 43-52. Developer Experience | v1.7 | 19/19 | Complete | 2026-03-02 |
| 53. Environment and Build Verification | 2/3 | In Progress|  | - |
| 54. JSON Serializer Refactor | 1/1 | Complete   | 2026-03-02 | - |
| 55. Platform Storage Backends | v1.8 | 0/? | Not started | - |
| 56. Tech Debt Cleanup | v1.8 | 0/? | Not started | - |
| 57. QoL Features | v1.8 | 0/? | Not started | - |
| 58. Documentation and Build Tooling | v1.8 | 0/? | Not started | - |
