# Milestones

## v1.9 Tech Debt Resolved (Shipped: 2026-03-03)

**Phases completed:** 1 phase (59), 2 plans, 4 tasks
**Timeline:** 1 day (2026-03-03)
**Git range:** 10 commits, 12 files changed, +1,051 / -28 lines
**Requirements:** DEBT-01 through DEBT-05 — all resolved

**Key accomplishments:**
- `const T* getComponent() const` overload added to `object.hpp` — `hasComponent<T>() const` is now well-formed C++ without non-const call (DEBT-01)
- `setLuaProxy()` debug-build warning for double-registration — `#ifndef NDEBUG` printf fires when non-null proxy is overwritten with a different non-null proxy (DEBT-02)
- EventBus `emit()` `m_L=nullptr` window documented with hot-reload ordering invariant — resolves silent footgun in scene-change sequence (DEBT-03)
- `getPaletteRGB` snapshot semantics documented at WASM binding site — callers know static buffer is not live-updated (DEBT-04)
- WASM `setInputState()`+`updateFrame()` free functions added to Emscripten bindings — mirrors SDL3 runner order; function-local statics for zero-alloc per-frame state (DEBT-05)
- ESP32 example upgraded from idle loop to jitter-free FreeRTOS `vTaskDelayUntil` game loop at ~62.5fps (DEBT-05)

---

## v1.8 Ship Ready (Shipped: 2026-03-03)

**Phases completed:** 6 phases (53-58), 13 plans
**Timeline:** 2 days (2026-03-02 → 2026-03-03)
**Git range:** 57 commits, 78 files changed, +10,336 / -320 lines
**Requirements:** 19/19 satisfied (all phases complete, checkboxes stale)

**Key accomplishments:**
- `scripts/setup-dev.sh` — idempotent one-shot installer for Emscripten 3.1.73 + ESP-IDF v5.5 to XDG paths (Phase 53)
- `build.sh` — unified build script replacing build_wasm.sh, single entry point for SDL3/WASM/ESP32 (Phase 53)
- All three platform builds verified: SDL3, WASM (Emscripten), ESP32 (IDF) compile clean with v1.7 feature set (Phase 53)
- `LuaStore::writeStoreToBuffer()` — allocation-free JSON serializer shared across all 3 platforms (Phase 54)
- WASM localStorage bridge — `engine.store.save/flush/load` survives page reloads (Phase 55)
- ESP32 NVS backend — `engine.store` persists across power cycles with 15-char key validation (Phase 55)
- `engine.tween.await(id)` suspends coroutine until tween completes; `engine.async.wait_frames(n)` yields for N frames (Phase 57)
- `engine.camera.setDeadZone(w, h)` — camera stops following target within dead zone boundary (Phase 57)
- Docusaurus tutorials: Lua syntax highlighting, "Your First Script" + "Async Coroutines" tutorials (Phase 58)

---

## v1.7 Developer Experience & New Capability (Shipped: 2026-03-02)

**Phases completed:** 10 phases (43-52), 19 plans
**Timeline:** 3 days (2026-02-28 → 2026-03-02)
**Git range:** 62 files changed, +7,178 lines
**Requirements:** 26/26 satisfied (audit passed)

**Key accomplishments:**
- C_Tilemap 64x64 grid with viewport-culled rendering, scroll, coordinate helpers, and full Lua proxy
- C_Camera with lerp follow, screen shake, bounds clamping, drawWithOffset() render pipeline, and engine.camera.* API
- Stateless engine.physics.* toolkit — gravity, drag, springs, bounce, raycast, TrigLUT pre-computed trig tables
- Bindings refactoring: 1390-line monolith split via bindings_internal.hpp, systematic null safety guards, overflow tests
- engine.debug.* top-layer debug canvas with zero-cost toggle; engine.camera.follow/stopFollow per-frame tracking
- LuaStore SDL3 JSON I/O with engine.store.flush/path; engine.async.* 8-slot coroutine scheduler with wait/cancel
- engine.tween.* 8-slot pool with 4 inline easing functions; engine.scene.persist/unpersist with PersistentObjectRegistry
- engine.ui.* stateless immediate-mode draw calls (progressBar, statBar, panel, label)

**Tech debt (non-blocking):**
- m_followTargetProxy not cleared in registerAll/setActiveScene (safe due to lua_ok gate, defensive fix recommended)
- PERSIST-01/02/03 are silent no-ops in SDL standalone mode (no SceneStateMachine by design)
- Full Emscripten toolchain build not verified (code inspection conclusive)

**See:** [milestones/v1.7-ROADMAP.md](milestones/v1.7-ROADMAP.md) | [milestones/v1.7-REQUIREMENTS.md](milestones/v1.7-REQUIREMENTS.md)

---

## v1.6 Game Ready (Shipped: 2026-02-28)

**Phases completed:** 4 phases (39-42), 4 plans, 8 tasks
**Timeline:** 30 days (2026-01-29 → 2026-02-28)
**Git range:** e8d8fcc..59a75ca, 22 commits, 38 files changed, +9,457 / -75 lines

**Key accomplishments:**
- ComponentProxy self:get() infrastructure — Lua scripts access sibling components with typed proxy userdata and Component::~Component() stale-safe invalidation
- C_Timer with delayed/repeating Lua callbacks — 8-slot zero-alloc timer array with luaL_ref lifecycle management (after/every/cancel)
- C_StateMachine with deferred transitions — named states with enter/update/exit hooks, deferred transition model matching SceneStateMachine semantics
- EventBus scene-scoped pub/sub — on/off/emit API with fixed-capacity arrays, re-entrant-safe emit via ref snapshotting, hot-reload cleanup

**Tech debt (non-blocking):**
- Single-proxy-per-component constraint: multiple self:get() calls overwrite proxy registration (last wins)
- EventBus m_L=nullptr window between scene change and script load (safe for all Lua-reachable paths)

**See:** [milestones/v1.6-ROADMAP.md](milestones/v1.6-ROADMAP.md) | [milestones/v1.6-REQUIREMENTS.md](milestones/v1.6-REQUIREMENTS.md)

---

## v1.4 Engine Capabilities (Shipped: 2026-02-26)

**Phases completed:** 4 phases (23-26), 8 plans, 15 tasks
**Timeline:** 2 days (2026-02-24 → 2026-02-26)
**Git range:** fedeb12..5a70871, 52 commits, 73 files changed, +9,893 / -750 lines

**Key accomplishments:**
- Fixed Docusaurus API docs — MDX-safe escaping across 84 pages, zero-error build
- SpriteSheet zero-alloc struct with grid addressing, frame animation (Once/Loop/PingPong), and 16-slot Lua sprite pool
- C_Drawable signature migrated from ICanvas<uint8_t> to ICanvas<Pixel4> across all derived classes
- LayerCompositor with 4 independent Canvas4 buffers, painter's-order composition with index-15 transparency
- SDL3 multi-layer rendering + Lua layer API (setLayer/clearLayer/getLayerCount/visibility)
- F5 hot-reload with full Lua state reset, error recovery, and LuaCallback dangling-pointer fix

**Tech debt (non-blocking):**
- Full Emscripten toolchain build not verified (code inspection conclusive)
- `getPaletteRGB()` snapshot semantics — callers must re-invoke after palette mutation
- ESP32 PSRAM availability for 4-layer stack — may require compile-time layer count reduction to 2

**See:** [milestones/v1.4-ROADMAP.md](milestones/v1.4-ROADMAP.md) | [milestones/v1.4-REQUIREMENTS.md](milestones/v1.4-REQUIREMENTS.md)

---

## v1.3 Tomodachi Readiness (Shipped: 2026-02-24)

**Phases completed:** 4 phases (19-22), 7 plans
**Timeline:** 1 day (2026-02-24)
**Git range:** feat(19-01) → feat(22-02), 39 files changed, +5,766/-61 lines

**Key accomplishments:**
- 16-color indexed PICO-8 palette with transparent index 15, runtime swap, and no canvas re-render
- Lua and WASM palette bindings: `setPaletteColor`, `getPaletteColor`, `getPaletteRGB`, `loadPalette`
- Platform-agnostic `InputState` with uint16_t bitmask, float axes[8], and edge detection (justPressed/held/justReleased)
- SDL3 opt-in runner with Canvas4→RGB24 blit, 4× nearest-neighbor scaling, fixed-rate game loop, and keyboard input
- Lua input polling API (`isButtonHeld`, `isButtonJustPressed`, `isButtonJustReleased`, `getAxis`) + `e2e_parity.lua` cross-platform test
- Lua scripting wired into SDL3 runner via conditional CMake linking — same scripts run on SDL3, WASM, and ESP32 without modification

**Tech debt (non-blocking):**
- `getPaletteRGB()` delivers snapshot buffer (not live view) — callers must re-invoke after palette mutation; SDL runner unaffected
- Full Emscripten toolchain build not verified (code inspection conclusive)
- API navigation disabled in Docusaurus due to MDX syntax issues (carried from v1.0)

**See:** [milestones/v1.3-ROADMAP.md](milestones/v1.3-ROADMAP.md) | [milestones/v1.3-REQUIREMENTS.md](milestones/v1.3-REQUIREMENTS.md)

---

## v1.2 Tech Debt Cleanup (Shipped: 2026-02-23)

**Phases completed:** 3 phases (16-18), 5 plans
**Timeline:** 1 day (2026-02-23)
**Git range:** 18 commits, 330 files changed, +2,787 / -36,083 lines

**Key accomplishments:**
- Removed dead enjin1 compat headers, benchmark files, and CMake references
- Untracked generated LaTeX files from git and updated .gitignore
- Fixed xml2js ordered parsing and rewrote extractText() for correct document-order traversal
- Eliminated const const duplication in formatMethod() and regenerated 84 clean API pages
- Made WASM build Lua-optional with CMake generator expressions and C++ preprocessor guards

**Tech debt (non-blocking):**
- API navigation disabled in Docusaurus due to MDX syntax issues (carried from v1.0)
- parameterlist name/description concatenation in 5 API docs (Doxygen XML limitation)
- Full Emscripten toolchain build not verified (code inspection conclusive, toolchain not in dev env)

**See:** [milestones/v1.2-ROADMAP.md](milestones/v1.2-ROADMAP.md) | [milestones/v1.2-REQUIREMENTS.md](milestones/v1.2-REQUIREMENTS.md)

---

## v1.1 Project Infrastructure & Documentation Enhancement (Shipped: 2026-02-23)

**Phases completed:** 9 phases (7-15), 17 plans
**Timeline:** 22 days (2026-02-02 → 2026-02-23)
**Git range:** 86 commits, 546 files changed, +59,387 / -13,956 lines

**Key accomplishments:**
- Professional README with badges, features list, and documentation navigation
- Lua build dependency resolved with CMake options and comprehensive dependency documentation
- Complete Doxygen documentation across all public APIs — 0 warnings (down from 372)
- Module overview pages generated for all 9 modules with Docusaurus integration
- CI Doxygen warning threshold gate to prevent documentation regression
- Fixed documentation pipeline — 76+ clean API pages with proper cross-references on GitHub Pages

**Tech debt (non-blocking):**
- Brief description duplication in extractText() (5 pages, cosmetic)
- Template parameter concatenation producing fused text (4 pages)
- `const const` duplication in formatMethod() (136 occurrences)
- WASM+LUA OFF CMake edge case (WASM off by default)

**See:** [milestones/v1.1-ROADMAP.md](milestones/v1.1-ROADMAP.md) | [milestones/v1.1-REQUIREMENTS.md](milestones/v1.1-REQUIREMENTS.md)

---

## v1.0 Migration + Documentation (Shipped: 2026-02-01)

**Phases completed:** 6 phases (1-6), 21 plans
**Timeline:** 3 days (2026-01-29 → 2026-02-01)

**Key accomplishments:**
- enjin2 fully independent of enjin1 — zero dependencies verified at source and build levels
- Comprehensive compatibility layer with namespace enjin wrappers
- Validation infrastructure: shadow mode testing, BMP comparison pipeline
- Documentation pipeline: Doxygen + Docusaurus with 59 API pages across 9 modules
- All 14 v1 requirements satisfied

**See:** [milestones/v1.0-ROADMAP.md](milestones/v1.0-ROADMAP.md) | [milestones/v1.0-REQUIREMENTS.md](milestones/v1.0-REQUIREMENTS.md)

---



## v1.5 Lua Scripting Foundation (Shipped: 2026-02-28)

**Phases completed:** 12 phases (27-38), 21 plans
**Timeline:** 2 days (2026-02-26 → 2026-02-28)
**Git range:** b46463a..HEAD, 127 commits, 152 files changed, +25,583 / -4,893 lines

**Key accomplishments:**
- Fixed Pixel4 onRender dispatch via `if constexpr` two-branch scene render — RENDER-01 satisfied (Phase 27)
- Migrated all 8 component `update()` consumers to `float dt` seconds API with `-Woverride` enforcement (Phase 28)
- Named objects + 8-slot zero-allocation tag system with `findByName()`/`findAllWithTag()` on ObjectCollection (Phase 29)
- `engine.*` Lua global table with live pointer injection — scene, input, time, lua, log sub-tables (Phases 30–31)
- `ScriptProxy` full userdata with `__index`/`__newindex` metamethods; `self` as first callback arg in init/update/draw (Phases 32–33)
- `ScriptErrorPolicy` (Disable/Log/Panic) + on-edge input callbacks `on_button_pressed`/`on_button_released` (Phases 33–34)
- `engine.lua.collect()`/`memory()` + `assertRequires<T>()` component dependency assertions (Phase 35)
- Decoupled `Object` from `C_Drawable` — generic `getComponents<T>()` template; `ObjectProxy` from `engine.scene.find()` (Phases 36–37)
- Closed all audit gaps: live ENG-01/02 registry pointer-to-pointer wiring, `loadScriptFile()` proxy fix, SDL input dispatch (Phase 38)

**Tech debt (non-blocking):**
- Phase 35 VERIFICATION.md documents implementation; DEP-02 debug assert verified by code inspection only
- `hasComponent()` const calls non-const `getComponent<T>()` — pre-existing, no regression
- Single-proxy-per-object limitation (ObjectProxy design documented constraint)
- `C_LuaScript::setInput()` must be called per-frame by host in production (SDL runner wired; WASM/ESP32 not yet)

**See:** [milestones/v1.5-ROADMAP.md](milestones/v1.5-ROADMAP.md) | [milestones/v1.5-REQUIREMENTS.md](milestones/v1.5-REQUIREMENTS.md)

---

