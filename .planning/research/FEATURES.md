# Feature Research

**Domain:** 2D embedded game engine — v1.8 Ship Ready (platform hardening, tech debt, QoL, onboarding)
**Researched:** 2026-03-02
**Confidence:** HIGH (all features are well-understood; codebase confirmed in depth; v1.7 baseline is the foundation)

---

## Existing Baseline (Already Built — v1.7 Complete)

These ship with v1.7. They are **inputs** to v1.8 features, not deliverables.

| Already Built | Relevant to v1.8 |
|---------------|-----------------|
| LuaStore in-memory KV + SDL3 JSON file I/O (`engine.store.*`) | WASM localStorage bridge replaces the `saveToFile`/`loadFromFile` stubs on WASM; ESP32 NVS replaces them on ESP32 |
| engine.async.* 8-slot coroutine scheduler with `engine.async.wait(seconds)` | `wait_frames(n)` is a new helper; `engine.tween.await()` wires tween completion into the coroutine yield mechanism |
| engine.tween.* 8-slot pool with 4 easing functions; `done_cb` fires on completion | Tween-await integration needs `done_cb` to resume a waiting coroutine |
| C_Camera with lerp follow, shake, bounds, `engine.camera.follow/stopFollow` | Dead zone adds `m_deadZoneW/H` to C_Camera; follow update checks dead zone before calling `lookAt()` |
| Docusaurus site with API docs, Guides section, dual-plugin config | Tutorials add new pages to the Guides section; no structural changes to docusaurus.config.js |
| WASM build (`build_wasm.sh`, emscripten_bindings.cpp) | WASM verification confirms the build works end-to-end with all v1.7 features |
| ESP32 example project (`examples/esp32_idf_example/`) | ESP32 verification builds against v1.7 API and confirms 5-layer stack fits PSRAM |
| PersistentObjectRegistry + `engine.scene.persist/unpersist` | Tech debt: PERSIST is a no-op in SDL standalone; v1.8 fixes the gap or documents it clearly |
| `m_followTargetProxy` in camera follow path | Tech debt: proxy is not cleared on `registerAll/setActiveScene`; v1.8 fixes or removes it |

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features any developer working toward "ship-ready" expects. Missing these = the engine is not deployable.

| Feature | Why Expected | Complexity | Dependencies | Notes |
|---------|--------------|------------|--------------|-------|
| **WASM build verification** | If the WASM build is broken, the web target does not exist. Any engine claiming three-platform support must prove all three compile. | MEDIUM | Emscripten toolchain, all v1.7 headers compile under `__EMSCRIPTEN__` | Involves finding and fixing include/preprocessor gaps introduced by v1.7 additions (coroutines, tweens, persistent objects, UI, debug, store, camera). The existing `build_wasm.sh` provides the build entry point. Output: WASM build succeeds, produces `.js` + `.wasm`. |
| **ESP32 build verification** | Same reasoning. The Tomodachi device is an ESP32. Without a confirmed ESP32 build, the primary hardware target is unverified. | MEDIUM | ESP-IDF toolchain, 5-layer stack PSRAM check, coroutine library | v1.7 opened the ESP32 coroutine library (`engine.async`). Need to confirm it compiles and links under ESP-IDF. 5-layer canvas stack on ESP32 — each `Canvas4<320,240>` is 38,400 bytes; 5 layers is 192 KB, which requires PSRAM. If PSRAM not available, `ENJIN_LAYER_COUNT` must be reduced at compile time. |
| **Dev environment setup script** | A new contributor (or the project owner on a clean machine) cannot get started without knowing which packages to install and in what order. Missing = onboarding fails before any code runs. | LOW | Arch Linux `pacman`, AUR (for ESP-IDF or `esp-idf-tools-bin`), `emscripten` in `extra` repo | Emscripten is available as `pacman -S emscripten` (package version 5.0.2-1 in Arch `extra` as of 2026). ESP-IDF on Arch: either `yay -S esp-idf` (AUR) or manual clone of ESP-IDF v5.x + sourcing `export.sh`. Arch-specific: `ncurses5-compat-libs` needed for `xtensa-esp32-elf-gdb`. Script outputs: verify builds succeed on each target platform. |
| **WASM localStorage bridge for LuaStore** | `engine.store.save/flush` are no-ops on WASM today (stubs return false). Any game that relies on save data breaks on the web target. Developers expect storage to work on all platforms. | MEDIUM | WASM build verified first; `emscripten.h` JS interop (`EM_JS` or `EM_ASM`); existing LuaStore API unchanged | The bridge calls `localStorage.setItem(key, value)` and `localStorage.getItem(key)` via `EM_JS` or `EM_ASM` macros. localStorage is synchronous — no async overhead. Key constraint: 5MB limit (suitable for the flat 16-key LuaStore). Each entry serialized to a string (number→sprintf, string→raw, bool→"1"/"0"). On `engine.store.path()`, load all known keys from localStorage by scanning a known key-list entry. Confidence: HIGH — pattern is well-established in Emscripten projects. |
| **ESP32 NVS storage for LuaStore** | Same as WASM: `engine.store.save/flush` are stubs on ESP32. Tomodachi needs persistent config/state across power cycles. | MEDIUM | ESP32 build verified first; `nvs_flash.h`, `nvs.h` (ESP-IDF); NVS namespace scoping | ESP32 NVS: keys up to 15 chars (NVS limit). LuaStore `STORE_MAX_KEY = 16` — need to truncate to 15 or use NVS namespace "enjin2" and shorten keys. NVS values: store numbers as double (blob), strings as string (`nvs_set_str`), bools as uint8_t (`nvs_set_u8`). Tables: serialize as a JSON-like blob string (use existing `writeSlotValue` logic). Must call `nvs_commit()` after write (ESP-IDF requirement). Heap cost: ~22KB/MB NVS partition; acceptable with typical 16KB partition. |
| **Docusaurus tutorials with Getting Started guide** | The existing `getting-started.md` is a stub (3 steps + one stale C++ example using `Canvas8_128x64`). Developers expect a tutorial that shows them what the engine actually does today. | LOW-MEDIUM | Docusaurus dual-plugin setup (already working); arkanoid.lua and tamagotchi.lua demo scripts exist in `scripts/` | Tutorial structure: (1) Getting Started guide updated with SDL3 runner setup; (2) "Your First Script" tutorial using tamagotchi.lua as the walkthrough example; (3) "Async Coroutines" tutorial showing `engine.async.start` + `engine.async.wait`; (4) API examples — short inline snippets added to key API pages (engine.store, engine.tween, engine.async). |

### Differentiators (Competitive Advantage)

Features that go beyond baseline correctness and improve the developer experience distinctly.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Tween-await coroutine integration** (`engine.tween.await`) | Eliminates callback nesting for sequenced animations. A coroutine suspends until a tween finishes — same "async/await" feel as modern scripting environments. | MEDIUM | Requires: coroutine scheduler (done) + tween pool (done) + done_cb wiring. The tween's `done_cb` calls `lua_resume()` on the waiting coroutine thread. Tween slot holds a `coroutineRef` in addition to `doneCbRef`. |
| **`wait_frames(n)` coroutine helper** | Frame-based suspension for cases where time-based wait is overkill (e.g., "skip 1 frame for init to settle"). Standard in game scripting environments (PICO-8's `yield()`, Roblox's `task.wait()`, etc.). | LOW | Implemented as a Lua-level helper OR a C binding in `bindings_async.cpp`. Pattern: loop `n` times calling `engine.async.wait(0)` — each wait(0) yields for one tick. Alternatively: a dedicated C binding that stores frame countdown in the coroutine slot. Pure-Lua implementation is simpler and avoids new C binding. |
| **Camera dead zone** (`engine.camera.setDeadZone`) | Prevents micro-jitter from small player movements moving the camera. Standard in any 2D platformer camera. Notably absent from most lightweight embedded engines. | LOW-MEDIUM | Adds `m_deadZoneW`, `m_deadZoneH` to C_Camera. In `tickCameraFollow()`: compute distance from camera center to target; if within dead zone rect, skip `lookAt()`. If outside, call `lookAt()` as before. Dead zone is centered on the current camera position. |
| **Build helpers (CMake wrapper scripts)** | New contributors should not need to memorize emcmake flags or ESP-IDF environment activation. A thin `./build.sh --target wasm` or `./build.sh --target esp32` script reduces friction to zero. | LOW | Script wraps: `source emsdk_env.sh && emcmake cmake ... && emmake make` for WASM; `source $IDF_PATH/export.sh && idf.py build` for ESP32; `cmake -B build && cmake --build build` for SDL3. Idempotent (creates build dir if missing). Outputs success/failure clearly. |

### Anti-Features (Commonly Requested, Often Problematic)

| Anti-Feature | Why Requested | Why Problematic | Alternative |
|--------------|---------------|-----------------|-------------|
| **Auto-sync WASM store after every write** | "I want to save automatically like desktop" | localStorage is synchronous but browser may throttle rapid writes. Auto-sync on every `engine.store.save()` is fine; adding background IndexedDB sync is complex and asynchronous. | Stick with synchronous `localStorage.setItem()` on each write — this is the correct pattern for LuaStore's small payload size. IndexedDB is for files, not 16-key KV stores. |
| **NVS encryption for ESP32** | "Save data should be secure" | NVS encryption requires provisioning keys at flash time — not manageable from the game engine layer. This is a device provisioning concern, not an engine concern. | Document that NVS is unencrypted. For Tomodachi, the data (game state, config) has no security requirements. |
| **Hot reload on WASM/ESP32** | "Would be nice to reload scripts on all platforms" | F5 hot reload works via `std::filesystem` and SDL3 event loop. WASM has no accessible filesystem for scripts. ESP32 uses SPIFFS/LittleFS — possible but adds significant complexity. | Document hot reload as SDL3-only. It's a developer tool, not a runtime feature. WASM and ESP32 workflows reload by rebuilding. |
| **Docusaurus versioning** | "Lock docs to v1.7 vs v1.8" | Docusaurus versioning is a maintenance burden that compounds over time. enjin2 is a single-repo, single-version project at this stage. | No versioning. Single documentation set, kept current. If APIs break, update the docs. |
| **Interactive WASM demo in Docusaurus** | "Show a live engine demo on the docs site" | WASM demo in docs requires a complete JS runtime host (event loop, canvas, palette → texture pipeline). This is the DROP project's job, not the docs site's job. | Link to the DROP project from the docs. Document the WASM API. The interactive demo lives in DROP. |
| **ESP32 NVS namespace-per-scene** | "Each scene should have its own save partition" | NVS namespaces are limited to 16 bytes. Multiple namespaces multiply init overhead and RAM footprint. | Single namespace "enjin2" with prefixed keys. Lua scripts can namespace themselves via key naming convention (e.g., "scene1.score"). |

---

## Feature Dependencies

```
[WASM build verification]
    └──must succeed before──> [WASM localStorage bridge]
    └──must succeed before──> [WASM tutorial content]

[ESP32 build verification]
    └──must succeed before──> [ESP32 NVS storage]
    └──informs──> [ENJIN_LAYER_COUNT adjustment if PSRAM unavailable]

[Dev environment setup script]
    └──enables──> [WASM build verification]
    └──enables──> [ESP32 build verification]
    └──enhances──> [Docusaurus "Getting Started" tutorial]

[WASM localStorage bridge]
    └──requires──> [WASM build verification]
    └──requires──> [LuaStore in-memory KV + flush/path API] (DONE)
    └──conflicts──> [Auto-sync to IndexedDB] (anti-feature)

[ESP32 NVS storage]
    └──requires──> [ESP32 build verification]
    └──requires──> [LuaStore in-memory KV + flush/path API] (DONE)

[Tween-await coroutine integration]
    └──requires──> [engine.async.* scheduler] (DONE — v1.7)
    └──requires──> [engine.tween.* pool + done_cb] (DONE — v1.7)
    └──note──> Both systems already built; this is co-design wiring

[wait_frames helper]
    └──requires──> [engine.async.wait(seconds)] (DONE — v1.7)
    └──can be Lua-level──> No C++ changes required (pure Lua helper function)

[Camera dead zone]
    └──requires──> [C_Camera + engine.camera.follow] (DONE — v1.7)
    └──enhances──> [engine.camera.follow/stopFollow] (DONE — v1.7)
    └──adds to──> C_Camera: m_deadZoneW, m_deadZoneH fields

[Docusaurus tutorials]
    └──requires──> [Docusaurus dual-plugin site] (DONE)
    └──requires──> [arkanoid.lua + tamagotchi.lua demo scripts] (DONE — in scripts/)
    └──enhanced by──> [Dev environment setup script] (Getting Started content)

[Tech debt: m_followTargetProxy]
    └──requires──> [Understanding of registerAll/setActiveScene lifecycle]
    └──fixes──> Safety gap in camera follow on scene transitions

[Tech debt: PERSIST standalone gap]
    └──requires──> [Understanding of SceneStateMachine vs standalone runner paths]
    └──fixes OR documents──> engine.scene.persist() behavior in SDL standalone mode
```

### Dependency Notes

- **Build verification gates everything platform-specific.** WASM localStorage and ESP32 NVS cannot be written or tested until the respective platform builds succeed. Build verification is Phase 1 of v1.8.
- **Dev setup script enables parallel work.** Once the setup script exists, any contributor can verify builds independently. Write this early.
- **Tween-await is the highest-value QoL feature.** Both its dependencies (async scheduler, tween pool) shipped in v1.7. This is a co-design wiring pass — relatively small change with outsized ergonomic improvement.
- **wait_frames is a pure-Lua helper.** No new C bindings needed. Can be included as a utility in the "Getting Started" tutorial itself.
- **Camera dead zone is additive.** New fields on C_Camera, no breaking API changes. Safe to add after WASM/ESP32 verification.
- **Tech debt items are contained.** `m_followTargetProxy` is guarded by a null check (`lua_ok` gate); it's a latent bug, not an active crash. `PERSIST` standalone gap is behavior, not a crash. Both are medium-priority cleanup items, not blockers.

---

## MVP Definition

### Launch With (v1.8 scope — from PROJECT.md)

Minimum set to declare "ship ready" across all three platforms.

- [ ] **Dev environment setup script** — Arch Linux; installs Emscripten, ESP-IDF, validates each build target. Required before any platform verification can happen.
- [ ] **WASM build verified** — All v1.7 features compile under `__EMSCRIPTEN__`; `build_wasm.sh` succeeds; output `.js` + `.wasm` produced.
- [ ] **ESP32 build verified** — All v1.7 features compile under ESP-IDF; PSRAM check for 5-layer stack documented or fixed.
- [ ] **WASM localStorage bridge** — `LuaStore::saveToFile` / `loadFromFile` implemented for `__EMSCRIPTEN__` using `localStorage.setItem/getItem` via `EM_JS`. Existing Lua API unchanged.
- [ ] **ESP32 NVS storage** — `LuaStore::saveToFile` / `loadFromFile` implemented for `ESP32` using `nvs_set_str` / `nvs_get_str`. Flat key serialization. `nvs_commit()` on write.
- [ ] **Tech debt: m_followTargetProxy** — Cleared in `registerAll()` and `setActiveScene()`. Eliminates latent state leak.
- [ ] **Tech debt: PERSIST standalone gap** — Either: (a) wire `PersistentObjectRegistry` into the SDL standalone runner so `persist/unpersist` are not silent no-ops, OR (b) document the gap clearly and emit a Lua-level warning when called outside SceneStateMachine context.
- [ ] **Tween-await integration** — `engine.tween.await(target, props, duration, easing)` call inside a coroutine suspends the coroutine until the tween completes.
- [ ] **wait_frames helper** — `engine.async.wait_frames(n)` (or Lua-level `wait_frames(n)`) suspends the current coroutine for `n` engine ticks.
- [ ] **Camera dead zone** — `engine.camera.setDeadZone(w, h)` / `engine.camera.clearDeadZone()`. Bindings + C_Camera fields.
- [ ] **Docusaurus Getting Started guide** (updated) — Correct SDL3 runner setup steps; matches v1.7 API; no stale Canvas8 references.
- [ ] **Docusaurus tutorials** — Minimum: "Your First Script" (tamagotchi walkthrough) and "Async Coroutines" (engine.async.start + wait).
- [ ] **Build helper scripts** — `build.sh --target [sdl3|wasm|esp32]` thin wrappers.

### Add After Validation (v1.8.x)

- [ ] **API doc examples** — Short inline Lua examples on engine.store, engine.tween, engine.async, engine.camera API pages. Add after tutorials are written (content reuse).
- [ ] **Tween chaining** — `.after(fn)` syntax on tween completion. Depends on tween-await being stable.

### Future Consideration (v2+)

- [ ] **WASM OPFS / IndexedDB storage** — For larger save data. Currently out of scope; localStorage covers the flat 16-key LuaStore adequately.
- [ ] **ESP32 hot reload via SPIFFS** — Complex, platform-specific; deferred until Tomodachi hardware workflow demands it.
- [ ] **Interactive WASM demo in docs** — Lives in DROP project, not docs site. Link from docs once DROP ships.

---

## Feature Prioritization Matrix

| Feature | Developer Value | Implementation Cost | Priority |
|---------|-----------------|---------------------|----------|
| Dev environment setup script | HIGH (onboarding gate) | LOW (bash script) | P1 |
| WASM build verification | HIGH (platform gate) | MEDIUM (find/fix gaps) | P1 |
| ESP32 build verification | HIGH (hardware gate) | MEDIUM (PSRAM check + fixes) | P1 |
| WASM localStorage bridge | HIGH (save data on web) | MEDIUM (EM_JS wiring) | P1 |
| ESP32 NVS storage | HIGH (save data on hardware) | MEDIUM (NVS API + serialization) | P1 |
| Tech debt: m_followTargetProxy | MEDIUM (correctness) | LOW (2-line fix) | P1 |
| Tech debt: PERSIST standalone gap | MEDIUM (correctness) | LOW (document OR wire) | P1 |
| Tween-await integration | HIGH (DX, eliminates callback hell) | MEDIUM (coroutine resume in done_cb) | P1 |
| wait_frames helper | MEDIUM (frame-precise delays) | LOW (Lua-level helper) | P2 |
| Camera dead zone | MEDIUM (platform polish) | LOW (2 fields + follow gate) | P2 |
| Docusaurus Getting Started update | HIGH (onboarding) | LOW (rewrite stub) | P1 |
| Docusaurus tutorials | HIGH (onboarding) | MEDIUM (2 tutorial pages) | P1 |
| Build helper scripts | MEDIUM (DX) | LOW (bash wrappers) | P2 |
| API doc examples | MEDIUM (discoverability) | LOW (copy from tutorials) | P3 |

**Priority key:**
- P1: Required to declare v1.8 "ship ready" — directly from PROJECT.md target features
- P2: Should have; improves DX meaningfully; no blockers
- P3: Nice to have; build after P1+P2 are stable

---

## Implementation Details for Each Feature

### WASM localStorage Bridge

**What:** Platform implementation of `LuaStore::saveToFile` / `loadFromFile` for Emscripten builds using browser `localStorage`.

**Expected behavior:**
```lua
-- On WASM, these work transparently — no API changes
engine.store.path("enjin2")       -- sets namespace prefix; triggers load of existing keys
engine.store.save("score", 1234)  -- writes to localStorage["enjin2.score"]
engine.store.flush()              -- no-op or force-sync (localStorage is already synchronous)
local s = engine.store.load("score")  -- returns 1234
```

**Standard behavior (how localStorage works):**
- `localStorage.setItem(key, value)` — synchronous write; persists until cleared; ~5MB limit
- `localStorage.getItem(key)` — synchronous read; returns null if absent
- Data survives page reload and browser restart (same origin)
- localStorage is scoped per origin (no cross-origin leakage)

**Implementation approach:**
- `#elif defined(__EMSCRIPTEN__)` block in `bindings_store.cpp`
- `EM_JS` macro declares JS functions callable from C: `enjin2_ls_set(key, value)`, `enjin2_ls_get(key, outbuf, maxlen)`
- `saveToFile(path)`: iterate `m_entries`, serialize each to string, call `enjin2_ls_set("enjin2." + entry.key, serialized_value)`; also write a key index: `enjin2_ls_set("enjin2.__keys__", comma_separated_keys)`
- `loadFromFile(path)`: read `enjin2.__keys__`, split, `enjin2_ls_get()` each value, deserialize into `StoreSlot`
- Serialization: number → `snprintf("%.17g")`, string → raw, bool → "1"/"0", table → flat JSON using existing `writeJsonEscaped` logic adapted for `char[]` buffers
- Confidence: HIGH — `EM_JS` is well-documented; localStorage sync is ideal for small KV stores

**What would be surprising if missing:** WASM build has save/load API that silently does nothing — game state is lost on page reload. This is the expected behavior today; v1.8 fixes it.

---

### ESP32 NVS Storage

**What:** Platform implementation of `LuaStore::saveToFile` / `loadFromFile` for ESP32 builds using NVS flash storage.

**Expected behavior:**
```lua
-- On ESP32, engine.store.path() activates NVS namespace; API unchanged
engine.store.path("enjin2")       -- opens NVS namespace "enjin2"; loads existing keys
engine.store.save("volume", 80)   -- writes to NVS key "volume" (up to 15 chars)
engine.store.flush()              -- calls nvs_commit() explicitly
```

**Standard behavior (how ESP32 NVS works):**
- Keys: up to 15 ASCII characters (NVS limit — LuaStore's `STORE_MAX_KEY = 16` needs truncation)
- Value types: integers (`nvs_set_i32`, etc.), strings (`nvs_set_str`), blobs (`nvs_set_blob`)
- All values are persisted in NVS flash; survive power cycles
- `nvs_commit()` must be called after writes for them to be durable
- Initialization: `nvs_flash_init()` called once at boot (in `app_main` or equivalent)
- Wear leveling: built into NVS; 126x reduction in write frequency per entry
- RAM cost: ~22KB per 1MB NVS partition; acceptable

**Implementation approach:**
- `#elif defined(ESP32)` block in `bindings_store.cpp`
- `saveToFile(namespace)`: open NVS handle with `nvs_open(namespace, NVS_READWRITE, &handle)`, iterate `m_entries`, write each as appropriate type (`nvs_set_i32`/`nvs_set_str`/`nvs_set_blob`), also write key index as a string blob, call `nvs_commit()`, `nvs_close()`
- `loadFromFile(namespace)`: open handle read-only, read key index string, iterate and read each key back into `m_entries`
- Key truncation: `STORE_MAX_KEY` is 16; NVS allows 15; truncate silently on write and document the limit
- Tables: serialize to flat JSON string blob (same logic as existing JSON writer, adapted for `char[]`)
- Confidence: MEDIUM — NVS API is well-documented; integration with existing LuaStore slots needs careful type mapping

**What would be surprising if missing:** Tomodachi device loses all game state on every restart. Silent no-op is worse than a boot-time error because it's invisible to Lua scripts.

---

### Tween-Await Coroutine Integration

**What:** `engine.tween.await(target, props, duration, easing)` — starts a tween and suspends the calling coroutine until it completes.

**Expected behavior:**
```lua
engine.async.start(function()
    -- animate health bar from current to 0 over 0.5s
    engine.tween.await(player, {hp = 0}, 0.5, "easeOut")
    -- execution resumes here after tween finishes
    engine.scene.switch("game_over")
end)
```

**Standard behavior (how tween+await works in game engines):**
- In Godot: `await tween.finished` suspends the coroutine at that line
- In Unity/BeauRoutine: `yield return Routine.WaitForSeconds(0.5f)` inside a coroutine
- The implementation requirement: the tween's on-complete callback must resume the waiting coroutine thread
- If `engine.tween.await()` is called outside a coroutine: raise `luaL_error` (same pattern as `engine.async.wait`)

**Implementation approach:**
- Add `engine.tween.await` binding in `bindings_tween.cpp`
- `lua_engine_tween_await`: check `lua_isyieldable(L)`; if not, raise error. Call `lua_engine_tween_to` internally to start the tween. Store `luaL_ref(L, LUA_REGISTRYINDEX)` of the current thread as `coroutineRef` in the TweenSlot (alongside existing `doneCbRef`).
- In `tickTweens()` when `t >= 1.0`: if `slot.coroutineRef != LUA_NOREF`, retrieve thread and call `lua_resume(co, L, 0, &nres)` before clearing the slot.
- If tween also has `doneCbRef`: fire `doneCbRef` first, then resume coroutine.
- Key invariant: the coroutine is suspended (LUA_YIELD state) while tween runs — no manual resume needed per-frame.
- Confidence: HIGH — Exactly mirrors the existing `engine.async.wait` pattern; same `lua_resume`/`lua_yield` mechanism.

**What would be surprising if missing:** Developers who want sequential animations either nest callbacks (ugly) or poll a flag every frame in `update()` (error-prone). Tween-await is the clean solution.

---

### wait_frames Helper

**What:** A helper that suspends the current coroutine for `n` engine update ticks (frames).

**Expected behavior:**
```lua
engine.async.start(function()
    -- skip 1 frame to let init settle
    engine.async.wait_frames(1)
    -- now do something that requires the previous frame to have processed
    local pos = engine.scene.find("player"):getPosition()
end)
```

**Standard behavior:**
- PICO-8: `yield()` — suspends for exactly one frame, called in a loop for multi-frame waits
- Roblox: `task.wait()` — suspends for one frame minimum
- Defold: `coroutine.yield()` in `update()` body

**Implementation approach (Lua-level, no new C binding needed):**
```lua
-- Can be exposed as a built-in Lua function in the engine's standard preamble
function engine.async.wait_frames(n)
    n = n or 1
    for i = 1, n do
        engine.async.wait(0)  -- wait(0) yields for one tick (0 seconds)
    end
end
```
- `engine.async.wait(0)` with `waitRemaining = 0` means: slot is immediately eligible for resume next tick. This is confirmed by `tickCoroutines()` — epsilon check `0.001f` only gates positive wait values; `waitRemaining = 0` passes through on next frame.
- Alternative: a dedicated C binding in `bindings_async.cpp` that stores a frame counter in the `CoroutineSlot`. The Lua-level approach is simpler and avoids new C++ structure fields.
- Confidence: HIGH — `engine.async.wait(0)` behavior is confirmed by reading `bindings_async.cpp`.

**What would be surprising if missing:** Frame-exact delays require reimplementing a frame counter in Lua (standard boilerplate). wait_frames is a one-liner that eliminates that pattern.

---

### Camera Dead Zone

**What:** A rectangular region centered on the camera's current target within which camera follow does not activate. Player can move freely inside the zone without the camera tracking.

**Expected behavior:**
```lua
engine.camera.follow("player", 0.08)           -- smooth follow
engine.camera.setDeadZone(16, 8)               -- 16px wide, 8px tall dead zone
engine.camera.clearDeadZone()                  -- remove dead zone; follow always active
```

**Standard behavior (how dead zones work in 2D engines):**
- Dead zone = rectangular region centered at the camera's current position
- While the follow target is within the dead zone, the camera does NOT call `lookAt()` — no movement
- When target exits the dead zone boundary, camera resumes tracking
- Commonly: half-width on each side (so `setDeadZone(16, 8)` = ±8px X, ±4px Y)
- Camera damping still applies to the movement once it resumes — no jarring snap
- Edge cases: dead zone larger than viewport clips to viewport size

**Implementation approach:**
- Add `float m_deadZoneW{0.f}`, `float m_deadZoneH{0.f}` to `C_Camera` private section
- In `tickCameraFollow()` (SDL runner / engine update):
  ```cpp
  float dx = abs(targetX - (m_pos.x + canvasW/2));
  float dy = abs(targetY - (m_pos.y + canvasH/2));
  if (dx > m_deadZoneW/2 || dy > m_deadZoneH/2) {
      camera->lookAt(targetX - canvasW/2, targetY - canvasH/2, lerpSpeed);
  }
  ```
- `engine.camera.setDeadZone(w, h)`: calls `camera->setDeadZone(w, h)` — new public method
- `engine.camera.clearDeadZone()`: calls `camera->clearDeadZone()` (sets both to 0)
- Lua bindings: 2 new entries in `bindings_engine.cpp` camera subtable
- Confidence: HIGH — well-understood pattern; C_Camera already has all needed infrastructure

**What would be surprising if missing:** Camera jitter on small player movements. Any pixel-art game with a following camera needs this to prevent the scene from feeling "loose".

---

### Dev Environment Setup Script

**What:** A shell script that installs Emscripten and ESP-IDF on Arch Linux, validates each is functional, and outputs a build-ready environment.

**Expected behavior:**
```bash
./setup-dev.sh           # installs everything, validates
./setup-dev.sh --check   # just verify tools are present
```

**Standard behavior for dev setup scripts:**
- Idempotent: running twice does not break a working install
- Exits with clear error and instruction if a step fails
- Validates success: confirms `emcc --version`, `idf.py --version`, `cmake --version`
- Does not require root for the Emscripten step (emsdk user-local install is standard)
- Does require `sudo` for `pacman -S` or AUR helpers
- Documents what it changes (PATH additions, shell function suggestions)

**Platform-specific notes (Arch Linux):**
- Emscripten: `sudo pacman -S emscripten` installs the Arch-maintained package (v5.0.2-1 in `extra`). Does not require emsdk manual install — the Arch package is pre-configured.
- ESP-IDF: `yay -S esp-idf` (AUR) or manual clone. AUR path puts ESP-IDF in `/opt/esp-idf`. Requires sourcing `/opt/esp-idf/export.sh` per-session.
- Arch-specific: `ncurses5-compat-libs` (AUR) needed for `xtensa-esp32-elf-gdb` debugger. Not needed for builds.
- CMake: available via `pacman -S cmake`; likely already installed.
- Lua (for SDL3 builds): `pacman -S lua` (likely Lua 5.4); or use the vendored `luajit` already in the repo

**Confidence:** HIGH — Arch package availability confirmed (emscripten in `extra` repo). ESP-IDF AUR package is standard practice on Arch.

---

### Docusaurus Tutorials

**What:** New guide pages that walk a developer through using enjin2 from installation to a working Lua script.

**Expected structure:**
- `docs/src/getting-started.md` — Updated: SDL3 runner setup; correct API; Lua script entry point; not C++.
- `docs/src/tutorials/first-script.md` — Tamagotchi walkthrough: covers `update(dt)`, `draw()`, `engine.input.*`, `engine.state.*`, drawing primitives, UI bars.
- `docs/src/tutorials/async-coroutines.md` — Covers `engine.async.start`, `engine.async.wait`, `engine.tween.await`, `wait_frames`.

**Standard behavior for embedded SDK tutorials:**
- Tutorial is task-oriented ("build X"), not reference-oriented ("here is API Y")
- Each step has working code that can be copy-pasted and run
- Annotates the demo scripts already in `scripts/` — don't invent new examples
- Links to API reference pages for depth
- Getting Started covers: install, clone, first build, running the SDL3 runner with a script

**What would be surprising if missing:** A developer clones the repo, sees 125 source files, no tutorial, and gives up. The existing `getting-started.md` stub references `Canvas8_128x64` (stale API from enjin1 era) and has no Lua example. This is actively misleading.

**Confidence:** HIGH — Docusaurus Guides section infrastructure already works; tamagotchi.lua and arkanoid.lua are complete and runnable demo scripts.

---

## Tech Debt Cleanup Details

### m_followTargetProxy (MEDIUM priority, LOW effort)

**What goes wrong:** When `engine.camera.follow()` is called with an ObjectProxy target, the proxy reference (`m_followTargetProxy`) is stored but not cleared during `registerAll()` (hot-reload) or `setActiveScene()` (scene transition). This leaves a dangling reference that is currently safe only because the `lua_ok` gate prevents coroutine/tween/store operations during the error-reset window.

**Fix:** In `LuaBindings::registerAll()` and `LuaBindings::setActiveScene()` (wherever camera follow state is reset): clear `m_followTargetProxy` to `LUA_NOREF` and zero `m_followTargetName`. This is a 2-line addition per reset site.

**Complexity:** LOW. No design change needed — just apply the same pattern used for `clearCoroutines()` and `clearTweens()`.

---

### PERSIST Standalone Gap (LOW-MEDIUM priority, LOW-MEDIUM effort)

**What goes wrong:** `engine.scene.persist(name)` and `engine.scene.unpersist(name)` call into `PersistentObjectRegistry`, which is owned by `SceneStateMachine`. In the SDL standalone runner (which does not use SceneStateMachine), these are silent no-ops. A script that uses persist/unpersist in SDL standalone mode gets no error and no behavior.

**Options:**
1. **Wire it:** Make the SDL standalone runner hold a `SceneStateMachine` internally. Adds overhead but makes PERSIST work everywhere.
2. **Document and warn:** Emit `lua_warning(L, "engine.scene.persist() requires SceneStateMachine; ignored in standalone mode")` when called without SSM context. Update docs.

**Recommendation:** Option 2 (warn + document). Standalone mode is the SDL runner for scripting-only use; the SSM is for multi-scene games. PERSIST is a multi-scene feature by design. Emitting a Lua warning is the honest behavior.

**Complexity:** LOW (option 2 — add one null check + `lua_warning()`).

---

## Complexity Summary

| Feature | Phase Estimate | Risk |
|---------|----------------|------|
| Dev environment setup script | 1 phase | LOW — bash + known packages |
| WASM build verification | 1-2 phases | MEDIUM — unknown gap count from v1.7 additions |
| ESP32 build verification | 1-2 phases | MEDIUM — PSRAM unknowns; coroutine library compat |
| WASM localStorage bridge | 1 phase | LOW-MEDIUM — EM_JS wiring is standard |
| ESP32 NVS storage | 1 phase | MEDIUM — NVS API + serialization + key truncation |
| Tech debt: m_followTargetProxy | <0.5 phase | LOW — 2-line fix |
| Tech debt: PERSIST standalone | <0.5 phase | LOW — warn + document |
| Tween-await integration | 1 phase | LOW-MEDIUM — same mechanism as async.wait |
| wait_frames helper | 0.5 phase | LOW — Lua-level wrapper |
| Camera dead zone | 1 phase | LOW — 2 C_Camera fields + follow gate |
| Docusaurus tutorials | 1-2 phases | LOW — content work; infrastructure already done |
| Build helper scripts | 0.5 phase | LOW — thin bash wrappers |

---

## Sources

- Codebase: `/home/unwn/dev/enjin/src/scripting/bindings_store.cpp` — LuaStore stubs for ESP32/WASM confirmed (`saveToFile`/`loadFromFile` return false); SDL3 JSON I/O path confirmed working
- Codebase: `/home/unwn/dev/enjin/src/scripting/bindings_async.cpp` — `engine.async.wait(0)` tick behavior confirmed; `waitRemaining` epsilon check is `0.001f`
- Codebase: `/home/unwn/dev/enjin/src/scripting/bindings_tween.cpp` — `done_cb` (doneCbRef) fires on slot completion; extension point for coroutine resume confirmed
- Codebase: `/home/unwn/dev/enjin/src/components/camera.cpp` — C_Camera fields and `lookAt()` confirmed; no dead zone fields exist yet
- Codebase: `/home/unwn/dev/enjin/src/bindings/emscripten_bindings.cpp` — WASM bindings structure confirmed; Lua guard pattern confirmed
- Codebase: `/home/unwn/dev/enjin/examples/esp32_idf_example/CMakeLists.txt` — ESP32 example structure confirmed; NVS not yet wired
- Codebase: `/home/unwn/dev/enjin/build_wasm.sh` — WASM build entry point confirmed; emsdk expected at `../emsdk`
- Codebase: `/home/unwn/dev/enjin/docs/src/getting-started.md` — Stale stub confirmed (Canvas8_128x64 reference is v1 era); needs full rewrite
- Codebase: `/home/unwn/dev/enjin/scripts/tamagotchi.lua` — Demo script confirmed functional; good tutorial basis
- [Emscripten localStorage — EM_JS / EM_ASM patterns](https://emscripten.org/docs/api_reference/emscripten.h.html)
- [Emscripten File System API — IDBFS vs localStorage](https://emscripten.org/docs/api_reference/Filesystem-API.html)
- [ESP32 NVS Programming Guide — Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html)
- [Arch Linux emscripten package](https://archlinux.org/packages/extra/x86_64/emscripten/) — v5.0.2-1 in `extra`
- [Arch Linux ESP32 wiki](https://wiki.archlinux.org/title/ESP32) — `ncurses5-compat-libs` requirement confirmed
- [Camera dead zone patterns — GMTK: How to Make a Good 2D Camera](https://gmtk.substack.com/p/how-to-make-a-good-2d-camera)
- [Coroutine + tween await in Godot](https://uhiyama-lab.com/en/notes/godot/await-coroutine-basics/) — `await tween.finished` pattern
- [wait_frames pattern — PICO-8 and Lua coroutines](https://www.lexaloffle.com/bbs/?tid=3458)
- [BeauRoutine — coroutine+tween framework pattern](https://github.com/BeauPrime/BeauRoutine)
- [Docusaurus documentation best practices 2025](https://nerdleveltech.com/building-documentation-that-scales-best-practices-for-2025/)

---
*Feature research for: enjin2 v1.8 — platform hardening, WASM/ESP32 storage, tween-await, wait_frames, camera dead zone, dev setup, tutorials*
*Researched: 2026-03-02*
