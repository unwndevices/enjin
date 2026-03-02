# Project Research Summary

**Project:** enjin2 v1.8 Ship Ready
**Domain:** Cross-platform 2D embedded game engine — WASM/ESP32 hardening, platform storage backends, developer tooling, tutorial documentation
**Researched:** 2026-03-02
**Confidence:** HIGH

## Executive Summary

enjin2 v1.8 is a hardening milestone, not a feature milestone. The engine's three-platform architecture (SDL3 desktop, WASM/Emscripten, ESP32) already exists at the v1.7 code level, but two of three platforms have unverified builds and stub-only storage implementations. The primary work is: (1) prove all three platforms compile and produce runnable artifacts, (2) replace `return false` storage stubs with real platform backends (localStorage on WASM, NVS flash on ESP32), (3) wire two v1.7 subsystems together that were built but not integrated (tween completion into coroutine scheduler), and (4) create onboarding tooling and documentation that makes the engine usable by someone other than its author.

The recommended approach follows a strict dependency order: build verification must come before platform-specific storage work, and the shared JSON serializer refactor must precede both storage backends. The WASM and ESP32 backends share the same serialization strategy — the entire LuaStore is serialized to a single JSON blob written to one storage key — which minimizes divergence and allows both platforms to reuse the existing `readJson*` parsing infrastructure. Emscripten 3.1.73 should be pinned for reproducibility (the project already satisfies the C++17 constraint required by 4.x). ESP-IDF v5.5.x (current stable) is the correct target. No new external libraries are needed for any feature in scope.

The top risks are platform-specific and well-understood: WASM build gaps introduced by v1.7 features that have never been compiled under Emscripten; ESP32 heap exhaustion if the 5-layer canvas stack is allocated in internal RAM without PSRAM; NVS key collisions from silent 15-character truncation; and re-entrant coroutine resume if tween-await is implemented naively via `done_cb`. All of these have clear, established mitigations. No risk requires architectural rethinking — the existing platform-guard pattern (`#if !defined(ESP32) && !defined(__EMSCRIPTEN__)`) is the correct integration structure.

---

## Key Findings

### Recommended Stack

v1.8 requires no new framework dependencies. All stack additions are toolchain configuration and standard platform APIs that ship with the existing build environments. The Emscripten `EM_JS` macro (bundled with emsdk) is the correct bridge for WASM localStorage — it is synchronous, zero-overhead, and avoids the ASYNCIFY bloat trap. The ESP-IDF NVS component (bundled with ESP-IDF v5.5) satisfies the LuaStore KV model exactly within its constraints. Docusaurus 3.9.2 is already installed and requires only content files and a one-line prism config change to support Lua syntax highlighting in tutorials.

**Core technologies:**
- **Emscripten emsdk 3.1.73 (pinned):** WASM cross-compilation — pin for reproducibility; project already uses C++17 so 4.x is also safe, but 3.1.73 matches what `build_wasm.sh` was written against
- **ESP-IDF v5.5.x (current stable):** ESP32 build and flash — NVS API is unchanged from v4.x to v5.x at the call level; v6.0-beta1 exists but should not be used
- **EM_JS macro (Emscripten bundled):** localStorage bridge from C++ — synchronous, no Asyncify, matches LuaStore's synchronous `saveToFile` signature
- **nvs_flash.h + nvs.h (ESP-IDF bundled):** ESP32 persistent KV storage — 15-char key limit fits LuaStore's constraint; 4000-byte string limit vastly exceeds worst-case JSON payload
- **Docusaurus 3.9.2 (already installed):** Tutorial authoring — add `'lua'` to prism `additionalLanguages` and create `docs/src/tutorials/` directory; no new plugins

**What NOT to add:**
- `pacman -S emscripten` — gives system Emscripten 5.0.2 (not version-pinned); use manually-cloned emsdk pinned to 3.1.73
- `-sASYNCIFY` — 50%+ binary bloat for zero benefit; the existing `tickCoroutines(dt)` + `lua_resume` pattern is correct on WASM as-is
- IndexedDB / IDBFS — asynchronous; conflicts with synchronous `saveToFile` signature; localStorage is sufficient for 16-key LuaStore
- ESP-IDF v6.0-beta1 — use stable v5.5.x
- Separate NVS key per LuaStore slot — NVS key enumeration is diagnostic-only; serialize as a single JSON blob

### Expected Features

All features are P1 for "ship ready" status, with two additional P2 QoL items. The dependency chain is clear: build verification gates storage backends; dev setup script enables build verification; the shared JSON serializer refactor gates both storage backends. Full details in `.planning/research/FEATURES.md`.

**Must have (table stakes):**
- **Dev environment setup script** — Arch Linux; onboarding gate before any platform verification can happen
- **WASM build verified** — all v1.7 features compile under `__EMSCRIPTEN__`; produces `.js` + `.wasm`
- **ESP32 build verified** — all v1.7 features compile under ESP-IDF; PSRAM constraint for 5-layer stack documented or resolved
- **WASM localStorage bridge** — `LuaStore::saveToFile`/`loadFromFile` implemented for `__EMSCRIPTEN__` via `EM_JS`; flush-only pattern to avoid frame blocking
- **ESP32 NVS storage** — `LuaStore::saveToFile`/`loadFromFile` implemented for `ESP32` via single JSON blob in NVS; `nvs_commit()` on write
- **Tech debt: m_followTargetProxy** — 2-line fix clearing dangling camera follow reference on scene transitions and hot reload
- **Tech debt: PERSIST standalone gap** — emit `lua_warning()` when `engine.scene.persist()` called without SceneStateMachine context
- **Tween-await integration** — `engine.tween.await()` inside a coroutine suspends until tween completes; polling implementation, not re-entrant resume
- **Docusaurus Getting Started guide** (updated) — remove stale Canvas8 reference; match v1.7 SDL3 runner API
- **Docusaurus tutorials** — "Your First Script" (tamagotchi walkthrough) and "Async Coroutines" (engine.async + engine.tween.await)

**Should have (competitive):**
- **wait_frames helper** — `engine.async.wait_frames(n)` as a Lua-level wrapper over `engine.async.wait(0)`; no new C bindings
- **Camera dead zone** — `engine.camera.setDeadZone(w, h)`; offset-clamp formula, not toggle pattern
- **Build helper scripts** — `build.sh --target [sdl3|wasm|esp32]` thin wrappers

**Defer (v2+):**
- WASM OPFS/IndexedDB storage (localStorage is sufficient for 16-key LuaStore)
- ESP32 hot reload via SPIFFS
- Interactive WASM demo in docs (lives in DROP project, not docs site)
- API doc inline examples (extract from tutorials after they are stable)

### Architecture Approach

The architecture change for v1.8 is concentrated entirely in `src/scripting/bindings_store.cpp`. The existing `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` platform guard already provides the correct dispatch structure — v1.8 fills in the two stub `#else` branches. A prerequisite refactor extracts a `writeStoreToBuffer(char* out, size_t cap)` helper from the existing `std::ofstream`-based `saveToFile`, enabling both WASM and ESP32 backends to share the same JSON serializer. No new subsystems, no new headers, no API surface changes. The rest of the work is build infrastructure (scripts), documentation (Docusaurus tutorials), and isolated feature additions (camera dead zone, tween-await) that touch well-defined, contained modules.

**Major components:**
1. **`bindings_store.cpp` platform dispatch** — existing guard structure; add `#elif __EMSCRIPTEN__` and `#elif ESP32` branches with real implementations; extract `writeStoreToBuffer` shared helper
2. **WASM host / `emscripten_bindings.cpp`** — verify all v1.7 features compile under Emscripten; fix any missing preprocessor guards from v1.7 additions
3. **ESP32 host** — verify 5-layer canvas stack fits available RAM; wire `nvs_flash_init()` into platform init before LuaBindings construction
4. **Build infrastructure** — `scripts/setup-dev.sh` (new), `scripts/build-wasm.sh` (refactored from root `build_wasm.sh` with path detection), `scripts/build-sdl.sh` and `build-esp32.sh` (new)
5. **Docusaurus tutorials** — `docs/src/tutorials/` directory with `_category_.json`, tutorial pages, and `'lua'` added to prism config; `docs/sidebars.js` updated with tutorials category
6. **`bindings_tween.cpp` + coroutine scheduler** — tween-await via yield + polling (coroutine sets awaited tween ID; `tickCoroutines()` checks completion each tick)
7. **`components/camera.cpp`** — dead zone using offset-clamp formula; `m_deadZoneW`/`m_deadZoneH` fields

### Critical Pitfalls

1. **`emscripten_set_main_loop` destroys stack objects** — engine state allocated on the stack in `main()` is destroyed when the loop takes control. All engine state must be static or heap-allocated before registering the loop callback. Use `fps=0` (requestAnimationFrame). Never place initialization code after the call.

2. **ASYNCIFY bloat — do not use it** — the existing `tickCoroutines(dt)` + `lua_resume` scheduler pattern works correctly in WASM without ASYNCIFY. Adding `-sASYNCIFY` would instrument the entire Lua VM and inflate the binary 50%+. Never use `emscripten_sleep()` as a coroutine wait mechanism.

3. **ESP32 NVS key silent truncation at 15 characters** — NVS silently truncates keys longer than 15 characters, causing undetected collisions. Validate and reject keys longer than 15 chars in the NVS backend. Document the constraint at the Lua API level.

4. **ESP32 5-layer canvas stack exhausts internal RAM** — 5 layers at 320x240 4-bit = ~187 KB static allocation. With Lua VM overhead, this can exhaust the 320 KB internal RAM without PSRAM. Default to `ENJIN_LAYER_COUNT=3` for ESP32 targets unless PSRAM is confirmed. Add a compile-time `static_assert`.

5. **localStorage write blocks render frames** — calling `localStorage.setItem()` on every `engine.store.save()` causes 10-100ms frame spikes. Use flush-only pattern: dirty flag + explicit `flush()` + `window.beforeunload` handler. Never persist on every individual key write.

6. **Re-entrant coroutine resume from tween `done_cb`** — calling `coroutine.resume()` from inside `tickTweens()` is undefined behavior that can corrupt the coroutine pool. Implement tween-await as yield + polling: the coroutine yields with a tween ID stored; `tickCoroutines()` checks if that tween is still active each frame without any re-entrant resume.

---

## Implications for Roadmap

Based on combined research, the dependency graph enforces a clear phase sequence. Build verification is the critical path — nothing platform-specific can be written or tested until the respective platform builds succeed. The serializer refactor is a small prerequisite that unlocks both storage backends in parallel.

### Phase 1: Environment and Build Verification

**Rationale:** The dev setup script and build verification are the mandatory foundation. Nothing else in this milestone is testable until all three platforms compile. Build verification and the setup script are independent of each other and can be worked in parallel, but both must complete before Phase 2.

**Delivers:** Working Arch Linux dev setup script (idempotent, pinned versions, venv isolation); confirmed WASM build (`.js` + `.wasm` output); confirmed ESP32 build (IDF firmware); PSRAM layer count decision documented in code.

**Addresses:** Dev environment setup script, WASM build verification, ESP32 build verification (all P1 table stakes)

**Avoids:** Pitfall 1 (emscripten_set_main_loop stack objects — caught during WASM verification), Pitfall 2 (ASYNCIFY temptation — explicitly excluded at this phase), Pitfall 4 (ESP32 5-layer heap exhaustion — resolved here with PSRAM/layer count decision), Pitfall 9 (Arch Python version drift — setup script uses pinned emsdk version and venv isolation)

**Research flag:** MEDIUM — actual WASM build status is LOW confidence (PROJECT.md flags "Full Emscripten toolchain build not verified"). Expect to find and fix unknown preprocessor guard gaps introduced by v1.7 features. Budget time for discovery, not just execution. Also note: existing `build_wasm.sh` hardcodes `../emsdk` path; setup script installs to `$HOME/emsdk` — path detection logic must be resolved here.

### Phase 2: JSON Serializer Refactor

**Rationale:** Both storage backends need to write LuaStore to a `char[]` buffer rather than `std::ofstream`. Extracting `writeStoreToBuffer()` from the existing `saveToFile` is a prerequisite for both platforms. Isolating this as its own phase keeps the change reviewable and confirms the shared serialization foundation is correct before both backends build on it.

**Delivers:** `writeStoreToBuffer(char* out, size_t cap)` helper extracted and verified on the SDL3 target; existing SDL3 JSON I/O behavior unchanged; `readJson*` parsing functions confirmed reusable as-is for both load paths.

**Addresses:** Internal architecture prerequisite for Phases 3 and 4

**Avoids:** Divergent serializers on WASM and ESP32; the single-blob NVS approach eliminates the NVS key enumeration limitation documented in ARCHITECTURE.md

**Research flag:** SKIP — well-understood internal refactor; full code examples in ARCHITECTURE.md.

### Phase 3: Platform Storage Backends (WASM + ESP32)

**Rationale:** Both backends are independent of each other and both depend on Phase 2. WASM localStorage and ESP32 NVS can be implemented in parallel. They share the single-JSON-blob strategy, so they are reviewed together even if developed independently.

**Delivers:** `engine.store.save/flush/load` working correctly on WASM (localStorage persistence across page reload) and on ESP32 (NVS persistence across power cycles).

**Uses:** EM_JS macro (Emscripten), `nvs_flash.h`/`nvs.h` (ESP-IDF), `writeStoreToBuffer` from Phase 2

**Implements:** `#elif defined(__EMSCRIPTEN__)` and `#elif defined(ESP32)` branches in `bindings_store.cpp`

**Avoids:** Pitfall 3 (NVS key 15-char truncation — explicit validation and warn/reject), Pitfall 4b (NVS RAM overhead — open namespace handle once at init, not per operation), Pitfall 5 (localStorage frame blocking — flush-only pattern with `window.beforeunload`), Pitfall 6 (localStorage quota — try/catch in EM_JS glue, return bool to WASM caller)

**Research flag:** SKIP — both EM_JS localStorage and NVS API patterns are HIGH confidence with official documentation and complete code examples in STACK.md and ARCHITECTURE.md.

### Phase 4: Tech Debt Cleanup

**Rationale:** `m_followTargetProxy` and the PERSIST standalone gap are contained correctness issues that do not depend on platform work and do not block other features. After builds are green, these should be addressed as a focused cleanup pass before adding new QoL behavior.

**Delivers:** Camera follow proxy cleared on scene transition and hot reload (latent state corruption eliminated). `engine.scene.persist()` emits `lua_warning()` in standalone mode (honest behavior replaces silent no-op).

**Addresses:** Both tech debt items from PROJECT.md; both are P1 for correctness.

**Avoids:** Latent null dereference on scene transitions; misleading silent behavior in standalone scripts that use persist without SceneStateMachine.

**Research flag:** SKIP — both are 2-line fixes at well-identified call sites documented in FEATURES.md.

### Phase 5: QoL Features (Tween-Await, wait_frames, Camera Dead Zone)

**Rationale:** These three features are independent of each other and of the platform work. They can be worked together in one phase. Tween-await should be implemented first (highest value, highest risk); camera dead zone last (most precise implementation required for correct boundary behavior).

**Delivers:** `engine.tween.await()` for sequential animation without callback nesting; `engine.async.wait_frames(n)` Lua helper (no new C bindings); `engine.camera.setDeadZone(w, h)` with smooth offset-clamp boundary tracking.

**Implements:** Tween-await via polling (coroutine stores tween ID; `tickCoroutines()` checks completion — no re-entrant `lua_resume` from `done_cb`); `wait_frames` as Lua-level loop over `engine.async.wait(0)`; dead zone in `components/camera.cpp` using offset-clamp formula

**Avoids:** Pitfall 7 (tween-coroutine re-entrant resume — explicitly use polling), Pitfall 8 (camera dead zone boundary jitter — offset-clamp, not toggle pattern)

**Research flag:** SKIP for wait_frames (pure Lua helper, HIGH confidence). MEDIUM for tween-await — the polling implementation requires an integration test confirming the coroutine wakes exactly once per tween at the correct frame boundary. The re-entrant resume trap is a real failure mode documented in PITFALLS.md.

### Phase 6: Documentation and Build Tooling

**Rationale:** Documentation and build helper scripts have no engine dependencies — they can begin in parallel with Phases 2-4 and finalize here. Tutorials are placed last to accurately reflect the final v1.8 API (including tween-await and camera dead zone from Phase 5) rather than documenting work-in-progress.

**Delivers:** Updated `getting-started.md` (v1.7 SDL3 runner API, no stale Canvas8 references); "Your First Script" tutorial (tamagotchi walkthrough); "Async Coroutines" tutorial (engine.async + engine.tween.await); `build.sh` target wrappers; `'lua'` added to Docusaurus prism config.

**Addresses:** All documentation features; build helper scripts (P1 and P2 respectively from FEATURES.md).

**Avoids:** MDX build-time failures from angle brackets in prose (fenced code blocks only); Docusaurus dev-only success masking production build errors — always verify with `npm run build`

**Research flag:** SKIP — Docusaurus structure is HIGH confidence; tutorial directory pattern and sidebar wiring are explicit in ARCHITECTURE.md; Arch package list is verified in STACK.md.

### Phase Ordering Rationale

- **Build verification is the critical path blocker.** Nothing platform-specific can be implemented or tested until the build is clean. This is the single most impactful ordering decision in the milestone.
- **Serializer refactor is a small prerequisite that benefits from isolation.** Keeping it separate from the storage backend implementations makes each phase reviewable and reduces risk of mixing a refactor with new behavior.
- **WASM and ESP32 storage are independent** of each other once they share the Phase 2 serializer, enabling parallel work if needed.
- **Tech debt before QoL** — fixes correctness issues in existing behavior before adding new behavior.
- **Documentation finalizes after QoL features complete** to ensure tutorials reflect the actual shipped v1.8 API.

### Research Flags

Phases needing closer attention during planning or execution:
- **Phase 1 (WASM build):** LOW confidence on current WASM build status — PROJECT.md explicitly flags the Emscripten toolchain build as unverified. Expect undocumented compile errors from v1.7 additions (coroutines, tweens, persistent objects, UI, store, camera). Budget time for iterative fix cycles. The emsdk path mismatch between `build_wasm.sh` (`../emsdk`) and `setup-dev.sh` (`$HOME/emsdk`) must also be resolved here.
- **Phase 5 (tween-await):** The polling implementation requires a clear integration test to confirm the coroutine wakes exactly once per tween at the correct frame. The re-entrant resume trap from `done_cb` is a real failure mode with documented corruption consequences.

Phases with well-documented patterns (can proceed without additional research):
- **Phase 2 (serializer refactor):** Internal refactor with complete code example in ARCHITECTURE.md.
- **Phase 3 (storage backends):** Both EM_JS localStorage and NVS API are HIGH confidence with official documentation and code examples in STACK.md and ARCHITECTURE.md.
- **Phase 4 (tech debt):** 2-line fixes at identified call sites.
- **Phase 6 (docs/tooling):** Docusaurus structure and Arch package list are fully specified.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Emscripten/ESP-IDF/Docusaurus APIs verified against official docs. Version pinning rationale is solid. The emsdk 3.1.73 vs 4.x choice is a conservative stability decision — both are technically correct given the project's C++17 baseline. |
| Features | HIGH | All features confirmed against live codebase. Dependency graph is precise and verified by reading actual source files. No speculative features — every item maps to a confirmed integration point. |
| Architecture | HIGH | Integration points confirmed by reading `bindings_store.cpp`, `CMakeLists.txt`, `emscripten_bindings.cpp`, `build_wasm.sh`, `docusaurus.config.js`, and `sidebars.js`. The EM_ASM bridge and NVS single-blob patterns are fully specified with code examples. |
| Pitfalls | HIGH | Pitfalls derived from official docs (Emscripten, ESP-IDF), direct codebase analysis, and community issue trackers. All 10 pitfalls are specific, actionable, and cross-referenced to phases with concrete prevention strategies. |

**Overall confidence:** HIGH

### Gaps to Address

- **WASM build current state:** The actual compile error count and nature of v1.7 Emscripten gaps is unknown until `build_wasm.sh` is run. Phase 1 must be treated as an investigation phase, not pure execution. The deliverable includes finding and fixing all gaps, not just confirming a clean build.

- **ESP32 PSRAM availability:** Whether the target hardware has PSRAM determines `ENJIN_LAYER_COUNT` for ESP32. This is a hardware-specific decision that cannot be confirmed in advance. Phase 1 must produce an explicit documented determination: 3-layer if no PSRAM, 5-layer if PSRAM confirmed.

- **NVS string vs blob for JSON payload:** ARCHITECTURE.md recommends `nvs_set_str` for the single JSON blob but mentions `nvs_set_blob` as the fallback if the payload exceeds the 4000-byte NVS string limit. At Phase 3 implementation time, measure the worst-case serialized LuaStore size to confirm which NVS API is appropriate.

- **emsdk path assumption in `build_wasm.sh`:** The existing script expects `../emsdk`; the dev setup script installs to `$HOME/emsdk`. The build script must add path detection logic (check `$EMSDK` env var, fall back to `../emsdk`, fail with actionable error) before Phase 1 verification produces a reproducible clean result.

---

## Sources

### Primary (HIGH confidence)
- [ESP-IDF NVS Flash API Reference v5.5.3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html) — key/namespace limits, API surface, commit requirement
- [Emscripten Interacting with code docs](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html) — EM_JS, EM_ASM, UTF8ToString patterns
- [Emscripten Runtime Environment docs](https://emscripten.org/docs/porting/emscripten-runtime-environment.html) — main loop design, emscripten_set_main_loop behavior
- [Emscripten Asyncify documentation](https://emscripten.org/docs/porting/asyncify.html) — why to avoid it for this use case
- [Docusaurus Autogenerated Sidebar docs](https://docusaurus.io/docs/next/sidebar/autogenerated) — `_category_.json`, `sidebar_position` front matter
- [ESP-IDF Toolchain Setup Linux (stable)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html) — Arch Linux pacman package list
- Codebase: `src/scripting/bindings_store.cpp`, `bindings_async.cpp`, `bindings_tween.cpp`, `components/camera.cpp`, `CMakeLists.txt`, `build_wasm.sh`, `docs/docusaurus.config.js`, `docs/sidebars.js`, `.planning/PROJECT.md`

### Secondary (MEDIUM confidence)
- [AUR esp-idf package](https://aur.archlinux.org/packages/esp-idf) — Arch Linux install path, placement at `/opt/esp-idf`
- [ArchWiki ESP32](https://wiki.archlinux.org/title/ESP32) — `ncurses5-compat-libs` requirement for precompiled gdb on Arch
- [Emscripten GitHub issue #15491](https://github.com/emscripten-core/emscripten/issues/15491) — IDBFS CMake linker flag warning; why localStorage avoids the issue
- [LocalStorage vs IndexedDB vs OPFS — RxDB 2025](https://rxdb.info/articles/localstorage-indexeddb-cookies-opfs-sqlite-wasm.html) — storage API trade-offs for synchronous use cases
- [ESP-IDF install.sh breakage on Arch Linux — GitHub Issue #7809](https://github.com/espressif/esp-idf/issues/7809) — Python version drift on rolling Arch; venv isolation pattern

### Tertiary (LOW confidence)
- [Synchronous localStorage WASM gist](https://gist.github.com/makryl/96d87b23d7a7c3cc5bc1eee1021bb6ff) — community WASM localStorage bridge pattern; single source, cross-validated against official EM_JS docs

---
*Research completed: 2026-03-02*
*Ready for roadmap: yes*
