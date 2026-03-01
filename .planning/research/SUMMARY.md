# Project Research Summary

**Project:** enjin2 v1.7 — Developer Experience & New Capability
**Domain:** Zero-alloc 2D game engine, Lua scripting layer expansion (embedded + SDL3 + WASM)
**Researched:** 2026-03-01
**Confidence:** HIGH

## Executive Summary

enjin2 v1.7 is a developer experience milestone that adds seven new Lua API surfaces to an existing, well-validated engine: debug draw (`engine.debug.*`), camera follow helpers (`engine.camera.follow`), save/load serialization hardening, persistent objects across scenes, coroutine/async scheduling (`engine.async.*`), tween helpers (`engine.tween.*`), and UI component bindings (`engine.ui.*`). The milestone is also paired with a structural refactoring of the 1390-line `bindings.cpp` monolith and a systematic null safety pass across all binding chains. Critically, every v1.7 feature is achievable with the existing C++ codebase — no new external libraries are required and all constraints (zero dynamic allocation, ESP32/WASM portability, LuaJIT 5.1 API compatibility) are already solved by patterns established in v1.0–v1.6.

The recommended approach is to build in dependency order: stabilize the binding structure first (split `bindings.cpp`, establish `bindings_internal.hpp`), deliver quick-win low-complexity features (debug draw, camera follow, save/load SDL3 path), then layer on the medium-complexity systems requiring new pooled state (coroutines, tweens), and finish with the architecturally invasive persistent objects system. UI component bindings are stateless draw calls and can be delivered in any later phase. All scheduler state (tweens, coroutines) must live in fixed C++ arrays inside `LuaBindings` — never in Lua tables — because Lua tables do not survive hot reload.

The primary risks are concentrated in three areas: (1) coroutine lifecycle management — specifically yielding across the `lua_pcall` boundary raised by `callWithProxy()`, and ensuring all thread refs are `luaL_unref`'d before `lua_close()` on hot reload; (2) the persistent objects architectural change — `PersistentObjectRegistry` requires modifying `ObjectCollection`, `SceneStateMachine`, and `engine.scene` Lua bindings simultaneously; and (3) the bindings split — `static` linkage of existing helpers in `bindings.cpp` must be audited before any files are extracted to avoid linker errors. All other features are low-risk and directly grounded in confirmed codebase capabilities.

---

## Key Findings

### Recommended Stack

All v1.7 work is pure C++ + LuaJIT with no new external dependencies. The existing `Primitives<Pixel4>` shape set, `LuaStore` JSON persistence, `C_Camera::lookAt()`, and `lua_newthread`/`lua_resume`/`lua_yield` from the bundled LuaJIT are sufficient for every planned feature. Two new C++ constructs are needed: a `TweenSlot[8]` fixed array in `LuaBindings` for the tween pool, and a `PersistentObjectRegistry` class owned by `SceneStateMachine` for persistent objects. Everything else is binding code.

**Core technologies:**
- LuaJIT 2.1 (Lua 5.1 API, bundled): coroutine scheduling, tween callbacks — native `lua_newthread`/`lua_resume`/`lua_yield` confirmed present; `coroutine` library must be explicitly opened in the ESP32 `openEmbeddedLibraries()` path (currently absent)
- `Primitives<Pixel4>` (existing): debug draw shapes — `drawRect`, `drawCircle`, `drawLine`, `fillRect` already implemented; zero new drawing code needed
- `LuaStore` (existing): save/load and persistent object data — only the `#ifdef VCV_RACK` platform guard needs replacing with proper SDL/ESP32/WASM branching
- `C_Camera::lookAt()` (existing): camera follow — the helper is 15-20 lines of binding code wrapping the already-implemented lerp function
- `bind_helpers.hpp` + `LuaFuncDef`/`luaBindFunctions()` (existing): the registration pattern all new `bindings_*.cpp` files follow

**What NOT to add:**
- No external tween libraries (EnTT tweeny, cpptween) — breaks zero-alloc and ESP32 constraints
- No external JSON library — LuaStore's handwritten writer is sufficient
- No Lua async frameworks (copas, luasocket) — require sockets, incompatible with embedded targets
- No LVGL or layout engine — grossly over-engineered for a 128x128 pixel display
- No `DontDestroyOnLoad` heap mechanism — use `PersistentObjectRegistry` with fixed-size ownership

### Expected Features

**Must have (table stakes) — v1.7 scope:**
- `engine.debug.*` bindings (rect, circle, line, cross, text, enabled flag) — every game engine exposes debug primitives; wiring-only, all shapes already exist in codebase
- `engine.camera.follow(proxy, speed)` / `engine.camera.stopFollow()` — obvious DX improvement over calling `lookAt()` manually every frame; single function addition
- Save/load serialization on SDL3 path — `LuaStore` JSON I/O is already written behind `#ifdef VCV_RACK`; change to SDL/ESP32/WASM preprocessor branching
- `engine.async.*` coroutine scheduler — loading screens and cutscenes require waiting across frames; 8-slot fixed pool with `engine.async.wait(seconds)` yield
- `engine.tween.*` tween helpers — 8-slot pool with easing (linear, easeIn, easeOut, easeInOut); animates Lua table fields via `lua_setfield` dispatch
- `engine.ui.*` UI draw bindings (progressBar, statBar, panel, label) — stateless immediate-mode draw calls; existing FillUpGauge/Label C++ components cannot be wrapped due to `std::string`/`std::vector` incompatibility with the zero-alloc Pixel4 pipeline
- Persistent objects across scenes — fixed-size `PersistentObjectRegistry`; `engine.scene.persist(proxy)` / `engine.scene.unpersist(proxy)`

**Should have (differentiators):**
- `engine.debug.enabled` boolean global toggle (zero cost when false) — enables/disables all debug draws without removing calls from scripts
- Coroutine-aware tween await (`engine.tween.await()` that yields inside a coroutine until tween completes) — requires both systems stable first; eliminates callback nesting
- Tween chaining (`.after(fn)`) — `on_complete` triggers next tween; cutscene-quality sequences

**Defer to v1.7.x / v2+:**
- Camera dead zone (C_Camera extension — deferred until follow helper is validated)
- ESP32 NVS save path (requires NVS wear analysis; deferred, stubs return false)
- WASM localStorage bridge (requires JS interop/emscripten val layer)
- UI layout engine (not appropriate for pixel art embedded target)

### Architecture Approach

The architecture is layered: Lua script → `LuaBindings` (split across `bindings_*.cpp`) → component/scene layer → graphics/canvas layer. All new v1.7 state lives in `LuaBindings` fixed arrays (`TweenSlot m_tweens[8]`, `CoroutineSlot m_coroutines[8]`), registered via `engine.*` sub-tables using the established `lua_newtable` + `luaBindFunctions()` pattern. The one exception is `PersistentObjectRegistry`, which is a value member of `SceneStateMachine` (not `LuaBindings`), because object ownership must survive at the C++ level independently of the Lua state. Frame update order is: `tickTweens(dt)` → `tickCoroutines(dt)` → `ssm.update(dt)` → clear debug canvas → `ssm.render()` → `LayerCompositor.composite()`.

**Major components:**
1. `LuaBindings` (modified) — gains `m_debugCanvas*`, `m_tweens[8]`, `m_coroutines[8]`; split from 1390-line monolith into focused `bindings_*.cpp` files via `bindings_internal.hpp`
2. `PersistentObjectRegistry` (new) — SSM-owned `unique_ptr<Object>[16]` array; injects/withdraws from `ObjectCollection::m_external[]` on scene transitions
3. `LuaCoroutineScheduler` (embedded in `LuaBindings`) — 8-slot `CoroutineSlot` array; `tickCoroutines()` resumes live coroutines each frame; `clearCoroutines()` on hot reload
4. Debug canvas — dedicated highest-index layer; cleared automatically each frame before render; all `engine.debug.*` calls route there; null on release/ESP32 builds
5. `C_Camera` (modified) — gains `setFollowTarget(Object*, float)` / `clearFollowTarget()`; follow resolves per-frame via `C_Position`; stale proxy detection via `valid` flag

### Critical Pitfalls

1. **Coroutine yield across pcall boundary** — `engine.async.wait()` called from inside `update(self, dt)` (invoked via `callWithProxy()` → `lua_pcall()`) raises "attempt to yield across C-call boundary" in Lua 5.1. Prevention: design the scheduler so `engine.async.start(fn)` registers the coroutine in the C-side scheduler; the scheduler resumes it via `lua_resume()` from outside any pcall scope. Coroutines must be resumed from C each frame, not yielded from within a pcall context.

2. **Stale Lua refs after hot reload** — Tweens and coroutines hold `luaL_ref` handles into a `lua_State` destroyed on F5. Prevention: implement `clearTweens()` and `clearCoroutines()` symmetric to `C_Timer::clearTimers()`; call both from `registerAll()` (the hot-reload entrypoint). Store `lua_State*` alongside each `int callbackRef` in tween slots; compare at fire time. Store coroutine threads as `int threadRef` (registry ref), never as raw `lua_State*`.

3. **Debug draw on wrong canvas** — Calling `engine.debug.*` inside `update()` draws onto stale pixel data; drawing to `currentCanvas` (game layer) buries debug shapes under game content. Prevention: route all `engine.debug.*` to `m_debugCanvas` (highest-index dedicated layer); clear it automatically at frame start in the SDL runner, not in Lua scripts.

4. **bindings.cpp split breaks static linkage** — `static` helpers in `bindings.cpp` (including `g_currentBindings`) are TU-local; splitting to `bindings_debug.cpp` etc. makes them invisible to new TUs. Prevention: create `bindings_internal.hpp` for shared non-public declarations before any file is split; confirm `getBindings(L)` is a `public static` member of `LuaBindings` (already accessible from all TUs); audit all `static` functions before extracting.

5. **Persistent objects — undefined Lua script lifecycle** — Scripts on persistent objects may call `engine.store.save()` on every `update()` frame (causing NVS wear or I/O stalls), or try to access scene-specific objects during the transition window when `m_activeScene` is null. Prevention: document that persistent object Lua scripts are NOT reloaded on scene transition; provide an optional `on_scene_change(new_id)` callback; prohibit store writes inside `update()`.

---

## Implications for Roadmap

Based on research, seven phases are appropriate for v1.7. The order is dependency-driven: bindings structure first, quick wins second, complex schedulers third, invasive architectural change fourth, and stateless UI last.

### Phase 1: Bindings Refactoring + Null Safety Foundation

**Rationale:** All subsequent binding work is easier with a smaller `bindings.cpp` and a `bindings_internal.hpp` that prevents static-linkage breakage across split files. Null safety must be done incrementally with test runs after each guard addition, not batched. These are parallelizable but both must complete before new binding files are added at scale.
**Delivers:** Split `bindings.cpp` → `bindings_proxy.cpp` + reduced coordinator; `bindings_internal.hpp`; null guard additions with zero return values for numeric properties; all 27+ ctests still pass.
**Addresses:** Bindings monolith tech debt (PROJECT.md); null safety pass (PROJECT.md).
**Avoids:** Pitfall 4 (static linkage breakage on split), Pitfall 10 (null guards returning nil instead of 0 breaking script arithmetic).

### Phase 2: Debug Draw Bindings

**Rationale:** Debug draw is wiring only — no new C++ drawing code. It establishes the dedicated debug canvas mechanism and confirms layer routing architecture works before coroutines and tweens are added. Highest developer value at lowest implementation cost.
**Delivers:** `bindings_debug.cpp`; `engine.debug.*` sub-table (rect, circle, line, cross, text, enabled flag); `m_debugCanvas` added to `LuaBindings`; `ENJIN2_DEBUG_DRAW` CMake option; debug canvas auto-cleared in SDL runner before each `draw()` pass.
**Addresses:** Debug draw (table stakes P1); `engine.debug.enabled` toggle (differentiator).
**Avoids:** Pitfall 1 (debug draw on wrong canvas/layer — dedicated layer + auto-clear protocol locked in here).

### Phase 3: Camera Follow Helpers + Save/Load SDL3 Hardening

**Rationale:** Camera follow and save/load are both low-complexity changes — follow is 15-20 lines of binding code; save/load is a preprocessor guard change. They are independent of each other and of the complex scheduler systems. Ship these before coroutines and tweens to reduce milestone scope uncertainty early.
**Delivers:** `engine.camera.follow(proxy, speed)` / `engine.camera.stopFollow()`; `C_Camera::setFollowTarget()`; ObjectProxy validity guard before each follow frame; `#ifdef VCV_RACK` replaced with SDL/ESP32/WASM branching in `bindings_store.cpp`; `engine.store.flush()` and `engine.store.path()`.
**Addresses:** Camera follow helper (table stakes P1); save/load SDL3 path (table stakes P1).
**Avoids:** Pitfall 7 (follow crashes on destroyed target — proxy validity checked before reading position), Pitfall 6 (binary save format non-portability — JSON-only via LuaStore).

### Phase 4: Coroutine / Async Scheduler

**Rationale:** Coroutines have the most critical and platform-sensitive pitfall in the milestone (pcall yield boundary). Building them before tweens allows the scheduler to be independently validated across SDL3, WASM, and ESP32 before tween-coroutine co-design work begins. The `clearCoroutines()` teardown protocol must be locked in before writing any resume/yield logic.
**Delivers:** `bindings_async.cpp`; `LuaCoroutineScheduler` embedded in `LuaBindings`; `engine.async.start(co)`, `engine.async.cancel(id)`, `engine.async.wait(seconds)`, `engine.async.wait_frames(n)`; `tickCoroutines()` wired in SDL runner; `clearCoroutines()` in hot-reload path; LuaJIT CoCo availability verified on WASM/ESP32; `luaopen_coroutine` added to ESP32 `openEmbeddedLibraries()`.
**Addresses:** Coroutine/async Lua (table stakes P1).
**Avoids:** Pitfall 2 (coroutine thread dangling after reload — `int threadRef` not raw `lua_State*`; `clearCoroutines()` before `lua_close()`), Pitfall 4 (yield across pcall boundary — scheduler resumes from C outside pcall scope).

### Phase 5: Tween Helpers

**Rationale:** Tweens depend on the ObjectProxy write dispatch path (established by camera follow in Phase 3) and benefit from the coroutine scheduler being stable. The 8-slot `TweenSlot` pool follows the fixed-array pattern proven in prior phases. The coroutine-aware tween await (differentiator) can be added here as Phase 5b once both systems are validated.
**Delivers:** `bindings_tween.cpp`; `TweenSlot m_tweens[8]` in `LuaBindings`; `engine.tween.to(proxy, prop, target, dur, easing)`, `engine.tween.value(from, to, dur, cb)`, `engine.tween.cancel(id)`, `engine.tween.cancelAll()`; 4 inline easing functions (linear, easeIn, easeOut, easeInOut, no external library); `tickTweens(dt)` wired in SDL runner; `clearTweens()` in hot-reload path; proxy validity check before each write.
**Addresses:** Tween helpers (table stakes P1); tween chaining (differentiator via `on_complete`).
**Avoids:** Pitfall 3 (stale callback ref after hot reload — `clearTweens()` in `registerAll()`; `lua_State*` alongside `int callbackRef`), Pitfall 3b (write to destroyed proxy — `valid` flag checked at every tween advance step).

### Phase 6: Persistent Objects Across Scenes

**Rationale:** Most architecturally invasive change — requires simultaneous modification of `ObjectCollection`, `SceneStateMachine`, and `engine.scene` Lua bindings. All prior phases must be stable before this change. The `PersistentObjectRegistry` design (SSM-owned unique_ptr array, inject/withdraw on scene transition) is fully specified and does not conflict with tween or coroutine systems.
**Delivers:** `PersistentObjectRegistry` class in `enjin2_core`; `ObjectCollection::m_external[]` non-owning array; `SceneStateMachine::applyDeferredTransition()` modified; `engine.scene.persist(proxy)`, `engine.scene.unpersist(proxy)`, `engine.scene.is_persistent(name)`; `engine.scene.find()` extended to search persistent registry; optional `on_scene_change(new_id)` Lua callback for persistent objects.
**Addresses:** Persistent objects across scenes (table stakes P2).
**Avoids:** Pitfall 9 (persistent object store data lost on script reload — persistent scripts are NOT reloaded on scene transition; lifecycle documented explicitly), Pitfall 5b (stale scene access during transition window — `on_scene_change` callback fires only after new scene is active).

### Phase 7: UI Component Bindings

**Rationale:** Stateless draw functions — no new C++ components, no pooled state, no allocation. Can be delivered at any point after Phase 1 (bindings structure stable) but is appropriately last because it benefits from the debug draw canvas patterns (Phase 2), and requires `resetUIState()` in `registerAll()` written carefully after the hot-reload discipline is fully established across all prior phases.
**Delivers:** `bindings_ui.cpp`; `engine.ui.progressBar(x,y,w,h,value,fgColor,bgColor)`, `engine.ui.statBar(x,y,w,h,cur,max,fg,bg)`, `engine.ui.panel(x,y,w,h,bg,border)`, `engine.ui.label(x,y,w,h,text,fg,bg)`; all implemented as stateless `LuaCanvas` draw calls (fillRect + drawRect + text); `resetUIState()` called from `registerAll()`.
**Addresses:** UI component bindings (table stakes P2).
**Avoids:** Pitfall 8 (UI state survives hot reload — `resetUIState()` in `registerAll()` zeroes all UI member arrays before rebuild).

### Phase Ordering Rationale

- **Foundation before features:** Splitting `bindings.cpp` and fixing null safety first eliminates the static-linkage landmine that would break every subsequent bindings file. Non-negotiable.
- **Quick wins second:** Debug draw, camera follow, and save/load are each low-complexity. Delivering them early builds confidence and de-risks the milestone before complex systems are added.
- **Coroutines before tweens:** The pcall yield boundary pitfall is the most platform-sensitive risk in the milestone. Isolating and validating coroutines on all three targets before adding tween-coroutine interaction prevents compounding failures.
- **Persistent objects last among core features:** The only change touching `SceneStateMachine` core logic. Doing it last means all other systems are stable and tested before the most invasive surgery.
- **UI last:** Zero architectural dependency, P2 priority. Position at the end reflects priority, not risk.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 4 (Coroutines):** LuaJIT CoCo availability on WASM (Emscripten) and ESP32 RISC-V must be verified at build time before writing the resume path. The behavior of `lua_resume()` when CoCo is absent differs from documented behavior on other platforms. Needs a targeted test binary built for each target before coroutine API design is finalized.
- **Phase 6 (Persistent Objects):** The `ObjectCollection::m_external[]` non-owning array modification touches the core object update/render loop. The explicit ordering of `withdrawFrom` vs `deactivate` vs `injectInto` vs `activate` within `applyDeferredTransition()` requires a design review session before implementation begins to avoid one-frame-late update bugs or double-update during transition.

Phases with standard, well-documented patterns (skip research-phase):
- **Phase 1 (Bindings Refactoring):** Standard C++ TU splitting; `bindings_internal.hpp` is a known pattern.
- **Phase 2 (Debug Draw):** Pure wiring onto confirmed `Primitives<Pixel4>`. No research needed.
- **Phase 3 (Camera Follow + Save/Load):** Minimal changes to well-understood existing systems.
- **Phase 5 (Tweens):** Fixed-pool tween math is elementary; easing functions are four inline equations; pool pattern established in prior phases.
- **Phase 7 (UI Bindings):** Stateless draw calls using confirmed `LuaCanvas` methods.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Full codebase read; all technologies confirmed in live source files; no new external dependencies required; every feature is purely additive binding work on confirmed primitives |
| Features | HIGH | All features are standard 2D engine patterns; each is mapped to confirmed existing C++ infrastructure; complexity estimates grounded in direct code analysis, not speculation |
| Architecture | HIGH | Architecture research reads directly from live headers (`bindings.hpp`, `camera.hpp`, `scene_state_machine.hpp`, `object_collection.hpp`); all patterns are established and in use in v1.0–v1.6 |
| Pitfalls | HIGH | Pitfalls derived from direct codebase constraints: Lua 5.1 API, `#ifdef VCV_RACK`, `static` linkage in `bindings.cpp`, NVS key limits, `callWithProxy()` pcall scope — not speculative; each has a confirmed source |

**Overall confidence:** HIGH

### Gaps to Address

- **LuaJIT CoCo on WASM/ESP32:** Whether LuaJIT is compiled with CoCo coroutine continuations on Emscripten and ESP32 RISC-V is not confirmed by research. This directly affects whether `engine.async.wait()` (which calls `lua_yield`) will work on those platforms when the coroutine is resumed from C via `lua_resume`. Resolution: add a compile-time CoCo availability check; document yield behavior per platform; implement a C_Timer-based fallback if CoCo is absent on a given target.

- **`ObjectCollection::m_external[]` update ordering:** The exact update/render ordering for external (persistent) objects injected into a scene's `ObjectCollection` is not yet designed. It must be confirmed that persistent objects get the correct update order relative to owned objects, and that `forEach()` iterates both consistently. Resolution: design the explicit ordering before Phase 6 implementation; add an overflow test for the external array capacity limit.

- **Label component std::string removal scope:** FEATURES.md notes `Label` uses `std::string`/`std::vector`, making it incompatible with the zero-alloc pipeline. The architecture decision for v1.7 is to implement `engine.ui.*` as stateless draw calls (bypassing `Label` entirely). If stateful label UI is needed post-v1.7, adapting the C++ `Label` component (replace `std::string text` with `char text[64]`) is a separate effort. Resolution: document the decision clearly in Phase 7; do not attempt C++ `Label` adaptation in v1.7 scope.

---

## Sources

### Primary (HIGH confidence — direct codebase analysis)
- `/home/unwn/dev/enjin/src/scripting/bindings.cpp` — 1390-line monolith; `static` linkage audit; `g_currentBindings` TU locality confirmed
- `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` — `registerEngineTable()` sub-table registration pattern; `engine.camera.*` baseline confirmed
- `/home/unwn/dev/enjin/src/scripting/bindings_store.cpp` — `LuaStore` full implementation; `#ifdef VCV_RACK` platform guard confirmed
- `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — `LuaBindings` member layout; `LuaStore`, `LuaCanvas`; `resetSpritePool()` hot-reload pattern
- `/home/unwn/dev/enjin/include/enjin2/components/camera.hpp` — `C_Camera` full API (lookAt, shake, setBounds, getScreenOffset) confirmed
- `/home/unwn/dev/enjin/include/enjin2/graphics/primitives.hpp` — full shape set (drawRect, drawCircle, drawLine, fillRect) confirmed for debug draw
- `/home/unwn/dev/enjin/include/enjin2/core/scene_state_machine.hpp` — `applyDeferredTransition()` hook point for persistent objects
- `/home/unwn/dev/enjin/include/enjin2/components/timer.hpp` — `clearTimers()` teardown pattern (template for coroutine/tween teardown)
- `/home/unwn/dev/enjin/include/enjin2/scripting/lua_event_bus.hpp` — `clearHandlers()` pattern; `int threadRef` Lua object lifetime model
- `/home/unwn/dev/enjin/include/enjin2/components/fill_up_gauge.hpp` — confirms Canvas8/`ICanvas<uint8_t>` incompatibility with Pixel4 pipeline
- `/home/unwn/dev/enjin/include/enjin2/components/label.hpp` — confirms `std::string`/`std::vector` usage; not usable for zero-alloc UI bindings
- `/home/unwn/dev/enjin/luajit/src/lua.h` — `lua_newthread`, `lua_resume`, `lua_yield`, `lua_status` confirmed present in bundled LuaJIT
- `/home/unwn/dev/enjin/src/scripting/lua_platform.cpp` — ESP32 library open list; `coroutine` library NOT included; must be added
- `/home/unwn/dev/enjin/.planning/PROJECT.md` — v1.7 feature list, zero-alloc constraint, 1390-line bindings.cpp tech debt, hot-reload semantics

### Secondary (MEDIUM confidence — community consensus, multiple sources agree)
- [Lua coroutines for game scripting — Jonathan Fischer](https://www.jonathanfischer.net/lua-coroutines/) — coroutine resume pattern for game engines
- [flux.lua (rxi)](https://github.com/rxi/flux) — tween API design reference; easing function selection
- [tween.lua (kikito)](https://github.com/kikito/tween.lua) — confirms 4-easing-function minimum for game UX
- [Espressif NVS API reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html) — 15-char key limit, ~4 KB blob value, 100K write cycle limit
- [Improved lerp smoothing — Game Developer](https://www.gamedeveloper.com/programming/improved-lerp-smoothing-) — supports per-frame `lookAt()` pattern over snap follow
- [LuaJIT CoCo documentation](https://coco.luajit.org/) — platform-dependent coroutine continuation support; WASM/ESP32 gap identified as unconfirmed

---

*Research completed: 2026-03-01*
*Ready for roadmap: yes*
