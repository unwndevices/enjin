# Project Research Summary

**Project:** enjin2 v1.6 Game Ready
**Domain:** Embedded/WASM zero-alloc 2D game engine — Lua scripting layer completion
**Researched:** 2026-02-28
**Confidence:** HIGH

## Executive Summary

enjin2 v1.6 is an incremental feature milestone on an existing, well-structured C++ game engine targeting ESP32 embedded hardware and WASM/SDL3 desktop. The v1.5 baseline is solid: Lua scripting, ScriptProxy/ObjectProxy userdata, scene state machine with deferred transitions, sprite animation, AABB/circle collision, LuaStore persistence, and hot-reload all work. v1.6 adds five Lua-facing capabilities — `C_Timer`, `C_StateMachine`, `ComponentProxy/self:get()`, `engine.events` event bus, and persistent objects — that are strictly necessary to build complete small games (Arkanoid, physics sandbox, tamagotchi) entirely from Lua. Every pattern needed already has a direct precedent in the codebase: `luaL_ref` integer handles for Lua callbacks, `char[N]` buffers for copied strings, fixed-slot arrays for collections, and the `valid` bool + non-owning raw pointer pattern for Lua userdata proxy safety.

The recommended approach is to build these five features in a dependency-ordered sequence. `ComponentProxy/self:get()` is the access mechanism that all component-facing Lua surfaces depend on and should be built first. `C_Timer` and `C_StateMachine` are independent of each other and can follow in either order. The `EventBus` is fully independent of the component system and can be built at any point. Persistent objects are architecturally the most invasive — they require modifying `ObjectCollection`, `SceneStateMachine`, and the Lua scene API — and should come last to isolate regressions. For persistent object state between scenes, the existing `LuaStore` (already in v1.5) is likely sufficient for all three target games; the full cross-scene object registry should be validated against game requirements before investing implementation effort.

The primary risks are all in the Lua/C++ memory safety boundary: stale `luaL_ref` handles after hot-reload or scene transitions, dangling `ComponentProxy` raw pointers when components are removed, and the single-proxy-per-object constraint that silently fails when multiple scripts obtain proxies to the same object. All three risks have established mitigation patterns in the existing codebase and must be addressed at component design time, not retrofitted. A secondary risk is memory budget on ESP32: every new static array contributes to heap and SRAM consumption, and all new component sizes must be verified with `static_assert` before shipping.

---

## Key Findings

### Recommended Stack

All v1.6 features require zero new external dependencies. The stack is unchanged from v1.5: C++17, LuaJIT 2.1 (Lua 5.1 ABI), CMake multi-target. No new `find_package` calls, no vendored headers. All new components use stdlib headers already compiled into the project (`<array>`, `<cstring>`, `<type_traits>`, `<cassert>`).

New components borrow `lua_State*` from the owning `C_LuaScript` component — never create a second Lua VM. Lua callbacks are stored as `luaL_ref` integer handles (zero allocation, GC-safe, stable across GC cycles), not `std::function` or `lua_CFunction`. Channel and state names that may come from transient Lua strings are copied into `char[N]` fixed buffers, following the established `LuaStore::StoreSlot::key[64]` precedent.

**Core technologies:**
- C++17: all new components — already required, no change
- LuaJIT 2.1 / Lua 5.1: scripting callbacks via `luaL_ref` integer handles — borrow `lua_State*` from `C_LuaScript`, never a second VM
- CMake multi-target: new `.cpp` files join `enjin2_core` and `enjin2_lua` — no new targets
- `std::array<T, N>` + fixed counts: timer slots, FSM state slots, event bus channels — zero heap per-frame

**Memory budget (ESP32):**

| Addition | Size | Count | Total |
|----------|------|-------|-------|
| `C_Timer` (8 slots) | ~128 bytes | 1-4 per scene | 128-512 bytes |
| `C_StateMachine` (8 states) | ~200 bytes | 1-4 per scene | 200-800 bytes |
| `EventBus` (16 channels x 8 listeners) | ~1.6 KB | 1 per active scene | 1.6 KB |
| SSM persistent collection | ~1 KB | 1 global | 1 KB |

Total C++ addition: ~3-4 KB. Within ESP32 PSRAM budget.

See `.planning/research/STACK.md` for full implementation patterns, avoid-list rationale, and ESP32 tuning constants.

---

### Expected Features

Features validated against three concrete target games: Arkanoid, physics sandbox, tamagotchi.

**Must have (table stakes — v1.6):**
- `C_Timer` (one-shot + repeating) — every game needs "do X after N seconds"; tamagotchi needs periodic decay every 30-45s; Arkanoid needs restart delay after ball loss. Lowest complexity new component, proves the `luaL_ref` pattern for subsequent components. P1.
- `engine.events` subscribe/emit/unsubscribe — Arkanoid: brick-to-score decoupled communication; tamagotchi: feed/play action dispatch. Independent of component system. Can be built at any phase. P1.
- `C_StateMachine` (flat FSM, enter/update/exit) — tamagotchi is a textbook FSM (idle/hungry/sleeping/happy/dead); Arkanoid needs game-state management (attract/play/paused/game_over/victory). P1.
- `ComponentProxy / self:get("Type")` — scripts cannot read position, toggle sprite frames, or query sibling state without this. Highest complexity. Keystone of the milestone. P1.
- Persistent objects — validate via LuaStore serialize/restore first. If that covers all three games (likely), defer full cross-scene object registry to v1.7. P1 (validate) or P2 (registry).

**Should have (v1.6 or v1.7):**
- `C_Timer` repeat count (fire N times then stop) — useful for "flash brick 3 times"; workaround is a counter in callback. P2.
- `C_StateMachine` transition guards — prevents invalid transitions; only needed when invalid transitions appear in practice. P2.
- `engine.events` auto-cleanup on component destroy — correctness at scale; lower priority while scene count is small. P2.

**Defer (v2+):**
- Coroutine-based state machines — C_StateMachine + C_Timer covers the same patterns without coroutine lifetime complexity
- Hierarchical state machines — no evidence of need from three target games
- Point-to-point event routing (msg.post style) — global broadcast is sufficient at enjin2 scale
- Full cross-scene persistent object registry — only if LuaStore + re-create proves insufficient

**Physics sandbox note:** Simplest of the three target games for v1.6. The v1.5 baseline is nearly sufficient. `ComponentProxy` (for sprite frame toggling) is the only strictly new v1.6 feature it needs. No timers, FSM, or events required.

See `.planning/research/FEATURES.md` for full prioritization matrix, anti-feature rationale, and per-game feature requirement tables.

---

### Architecture Approach

All five features integrate cleanly into the existing Component/Object/Scene/SceneStateMachine hierarchy without architectural disruption. The build order is dependency-driven: `ComponentProxy` first (access mechanism for all other Lua-facing components), then `C_Timer` and `C_StateMachine` (independently), then `EventBus` (independent of components), then persistent objects (most structurally invasive — touches ObjectCollection and SSM).

**Major components and responsibilities:**

1. `C_Timer` (`include/enjin2/components/timer.hpp`) — delayed/repeating Lua callbacks; 8-slot `TimerSlot` fixed array; `luaL_ref` for callbacks; float accumulator mirrors `SpriteState::accumSec`; borrowed `lua_State*`; destructor calls `luaL_unref` on all active refs
2. `C_StateMachine` (`include/enjin2/components/state_machine.hpp`) — per-object flat FSM; 8-slot `StateSlot` fixed array; deferred transition via `pending_` field (mirrors `SceneStateMachine::switchTo()`); `const char*` state names pointing to Lua-interned strings for stable lifetime
3. `ComponentProxy` (`include/enjin2/core/component_proxy.hpp`) — full userdata (not lightuserdata — LuaJIT has no metatable support for lightuserdata) with `Component*` + `bool valid`; per-type metatable (`"C_Timer"`, `"C_StateMachine"`); registered with owning `Object` for invalidation on destruction; accessed via `self:get("TypeName")` added to `ScriptProxy.__index`
4. `EventBus` (`include/enjin2/core/event_bus.hpp`) — scene-scoped, not global; 16 channels x 8 listeners; `char[32]` channel names (copied from transient Lua strings); `luaRef` integer listeners; `clear()` called in `Scene::deactivate()` to prevent cross-scene stale refs; exposed as `engine.events.*` sub-table
5. `PersistentObjectRegistry` (`include/enjin2/core/persistent_registry.hpp`) — SSM-owned; 16-slot `unique_ptr<Object>` array; scenes hold non-owning `Object*` in `ObjectCollection::m_external[]`; objects withdrawn from departing scene and injected into arriving scene during `applyDeferredTransition()`

**Files modified (not new):** `object.hpp` (ComponentProxy registration array), `object_collection.hpp` (external non-owning array), `scene_state_machine.hpp` (persistent registry member + `applyDeferredTransition()` modification), `bindings.hpp` (EventBus member, new sub-table declarations), `bindings_engine.cpp` (scene API extensions, `resolveComponent()` dispatch, `lua_proxy_get_component`), `CMakeLists.txt` (new `.cpp` sources).

See `.planning/research/ARCHITECTURE.md` for full data flow sequences, component interaction diagrams, anti-patterns, and the explicit new/modified file list.

---

### Critical Pitfalls

1. **Timer callback fires into dead Lua state after hot-reload** — `C_Timer` must store `lua_State*` alongside `int ref`. Before calling `lua_rawgeti`, verify the stored `L` matches the current active state. Destructor must call `luaL_unref` on all active refs. `LuaScriptSystem` must call a `cancelLuaCallback()` protocol on live timers before `lua_close`. Design teardown protocol before writing any tick logic. (PITFALLS.md Pitfall 1)

2. **ComponentProxy dangling pointer after component removal** — `Component` base class needs `ComponentProxy* m_luaProxy` field (mirrors `Object::m_luaProxy`). `Component::~Component()` must set `m_luaProxy->valid = false`. Single-proxy-per-component constraint: log a development warning when `setLuaProxy()` would overwrite a non-null registration. Cache `self:get()` result in `init()` — never call per-frame. (PITFALLS.md Pitfall 4)

3. **Event bus holding stale Lua refs across scene transitions** — EventBus must be scene-scoped, not global. `EventBus::clear()` called in `Scene::deactivate()` unrefs all listener handles. A global event bus accumulates stale refs from destroyed Lua contexts across reloads and will fire into freed memory. (PITFALLS.md Pitfall 5)

4. **Mid-loop object spawn from state machine callbacks** — `ObjectCollection::update()` must snapshot `objectCount` at loop entry (`size_t count = objectCount`), not read it fresh each iteration. Objects spawned from enter/exit callbacks during update appear at index `>= count` and receive their first update next frame. `engine.scene.spawn()` from inside a state callback must use the same deferred queue pattern as `engine.scene.switch()`. (PITFALLS.md Pitfall 3)

5. **Static array memory overflow on ESP32** — All new component static arrays live on the heap (components are heap-allocated). Define array sizes as `constexpr`: `MAX_TIMERS = 8`, `MAX_STATES = 8`, `MAX_EVENT_CHANNELS = 16`, `MAX_LISTENERS_PER_CHANNEL = 8` — reduce to 4/4/8/4 if SRAM is tight. Add `static_assert(sizeof(C_Timer) <= 256)` and equivalent for each new component before shipping. Establish memory budget at milestone start. (PITFALLS.md Pitfall 8)

See `.planning/research/PITFALLS.md` for 10 documented pitfalls total, the "Looks Done But Isn't" checklist, integration gotchas table, and recovery cost matrix.

---

## Implications for Roadmap

Based on combined research, five phases are appropriate for v1.6. Each phase is self-contained and testable. Phases 2 and 3 are independent and can proceed in either order or in parallel.

### Phase 1: ComponentProxy Foundation

**Rationale:** `self:get("TypeName")` is the access mechanism that all subsequent component Lua surfaces depend on. Building it first means each later component phase delivers a complete, testable Lua API immediately upon completion rather than waiting for a separate integration step. It also forces the critical design decisions (proxy validity protocol, `ScriptProxy.__index` dispatch hierarchy, full userdata requirement) before any component-specific code is written.

**Delivers:** `self:get("C_Timer")`, `self:get("C_StateMachine")`, `self:get("C_Position")` working from Lua; `Component` base class extended with `m_luaProxy` field; `Object::~Object()` invalidating all registered ComponentProxy instances; `ScriptProxy.__index` dispatch hierarchy documented and locked (reserve `"get"` as first check, before component property dispatch).

**Addresses:** Physics sandbox sprite frame switching; foundation for Phases 2 and 3 Lua surfaces.

**Avoids:** Pitfall 4 (ComponentProxy dangling pointer — design validity protocol before any `__index` implementation); Pitfall 7 (ScriptProxy `"get"` name collision — check `"get"` as first case in `__index`).

**Research flag:** Standard pattern — direct extension of existing ObjectProxy and ScriptProxy precedent. Skip research-phase.

---

### Phase 2: C_Timer

**Rationale:** Lowest complexity new component. Unlocks Arkanoid restart delay and tamagotchi periodic decay. Establishes the `luaL_ref` callback management pattern and teardown protocol that `C_StateMachine` will reuse. Building this before the FSM means the pattern is tested and stable when the more complex state machine uses it.

**Delivers:** `timer:after(delay, fn)`, `timer:every(interval, fn)`, `timer:cancel(id)`, `timer:cancelAll()`; repeating and one-shot modes; `luaL_unref` in destructor; hot-reload teardown protocol integrated with LuaScriptSystem.

**Uses:** `luaL_ref`/`lua_rawgeti` pattern from `bindings_engine.cpp`; float accumulator from `SpriteState::accumSec`; borrowed `lua_State*` — no second Lua VM.

**Avoids:** Pitfall 1 (timer callback into dead Lua state — design teardown protocol before tick logic; store `lua_State*` alongside `int ref`); Pitfall 2 (accumulator drift — use `>=` not `==` comparison; use subtraction-based reset for repeating timers: `remaining -= interval`, not `remaining = 0`).

**Research flag:** Standard pattern — well-established in existing codebase. Skip research-phase.

---

### Phase 3: C_StateMachine

**Rationale:** Independent of C_Timer but benefits from the established `luaL_ref` callback management pattern. Deferred transition design mirrors the existing `SceneStateMachine::switchTo()` pattern exactly. Unlocks tamagotchi pet FSM and Arkanoid game-state management.

**Delivers:** `fsm:addState(name, enterFn, updateFn, exitFn)`, `fsm:transition(name)`, `fsm:current()`; deferred transition via `pending_` field; enter/exit/update Lua callbacks passing `self` and `dt`; `luaL_unref` in destructor for all state refs.

**Uses:** `luaL_ref` pattern established in Phase 2; `const char*` state names (Lua-interned, stable lifetime); deferred transition from `SceneStateMachine` precedent.

**Avoids:** Pitfall 3 (enter/exit during object construction — document that `setState()` in `init()` is safe because all components have started by then; prohibit `engine.scene.spawn()` from callbacks without deferred queue); FSM re-entrancy (deferred `pending_` field prevents same-frame re-entry via the same mechanism as `SceneStateMachine`).

**Research flag:** Standard ECS/component FSM pattern. Deferred transition is established engine idiom. Skip research-phase.

---

### Phase 4: Event Bus

**Rationale:** Cross-object communication. Fully independent of Phases 1-3. Placed after the three component phases so integration testing benefits from all components being functional. Unlocks Arkanoid brick-to-score signaling and tamagotchi action dispatch.

**Delivers:** `engine.events.subscribe(name, fn)`, `engine.events.emit(name, ...)`, `engine.events.unsubscribe(subId)`; scene-scoped bus (not global); `char[32]` copied channel names (transient-safe); variadic emit forwarding all Lua stack args to subscribers synchronously; `EventBus::clear()` on scene deactivation.

**Uses:** `luaL_ref` for listener storage; pointer-to-pointer Lua registry injection pattern (same as `m_ssm` and `m_activeScene`); `LuaStore::StoreSlot::key[64]` as precedent for copied channel name buffers.

**Avoids:** Pitfall 5 (stale signal callbacks across scene transitions — scene-scoped bus with `clear()` on deactivation eliminates entire class of cross-scene dangling ref bugs); do NOT use existing `Signal<T>` (heap-allocated `std::function`, compile-time typed — wrong for string-keyed Lua event bus with runtime-unknown channel names).

**Research flag:** Standard pattern for embedded-safe event bus. Skip research-phase.

---

### Phase 5: Persistent Objects

**Rationale:** Most architecturally invasive phase. Modifies `ObjectCollection` (add external non-owning array), `SceneStateMachine` (add `PersistentObjectRegistry` member, modify `applyDeferredTransition()`), and the Lua scene API (`engine.scene.persist()`, `engine.scene.unpersist()`). Also extends `engine.scene.find()` to search the persistent collection before the active scene. All previous phases should be green and stable before making these structural changes, to isolate any regressions.

**Delivers (if full registry needed):** `engine.scene.persist(proxy)`, `engine.scene.unpersist(proxy)`, extended `engine.scene.find()`; `PersistentObjectRegistry` (SSM-owned, 16-slot `unique_ptr<Object>` array); persistent objects injected/withdrawn during scene transitions; persistent object `update()` called by SSM after scene update.

**Prerequisite decision:** Run tamagotchi and Arkanoid prototypes with LuaStore serialize/restore on scene transitions. If that covers persistence needs without friction, implement only `engine.scene.persist()` backed by LuaStore convention — skip PersistentObjectRegistry. Defer the full registry to v1.7.

**Avoids:** Pitfall 6 (persistent objects receive `dt` from wrong source — SSM-level collection updated by SSM, not by any individual scene); Pitfall 9 (LuaStore persistence confusion — document three survival scopes: file-persistent, scene-persistent, frame-local; add `engine.store.reset()` API); shared_ptr for persistent ownership (violates zero-alloc constraint — use `unique_ptr` in SSM, raw non-owning ptr in scenes).

**Research flag:** Moderately complex structural change. During planning: verify `ObjectCollection` update loop snapshots `objectCount` before the loop (`size_t count = objectCount` — grep target: `for(size_t i = 0; i < objectCount`). Confirm null-currentScene guard in `engine.scene.find()` handles the transition window correctly. No external research needed — all patterns are intra-codebase.

---

### Phase Ordering Rationale

- **ComponentProxy before C_Timer/C_StateMachine:** The Lua surface of both components depends on ComponentProxy. Building the access mechanism first means each component phase delivers a complete, testable Lua API immediately.
- **C_Timer before C_StateMachine:** Establishes the `luaL_ref` teardown protocol that C_StateMachine reuses. Lower complexity, safer first step. Both are independent and can be swapped if needed.
- **EventBus after components:** No dependency on the component system. Placed after Phases 1-3 so integration testing benefits from all components being functional. Could move earlier if event-driven testing of C_Timer or C_StateMachine is desired before Phase 4.
- **Persistent objects last:** Most invasive structural change. Placing it last isolates regressions. The LuaStore validation (does it cover game persistence requirements?) happens naturally during Phases 2-4 game prototyping.

### Research Flags

**Needs additional research during planning:** None. All five phases use patterns that exist verbatim in the v1.5 codebase. Critical design decisions are documented in STACK.md and ARCHITECTURE.md with working code examples.

**Validation needed before committing to Phase 5 scope:** Prototype tamagotchi and Arkanoid with LuaStore for persistence across scene transitions. If this works without friction, defer full `PersistentObjectRegistry` to v1.7.

**Standard patterns — skip research-phase for all phases:**
- Phase 1 (ComponentProxy): direct extension of ObjectProxy/ScriptProxy — identical `valid` flag protocol
- Phase 2 (C_Timer): `luaL_ref` accumulator pattern already proven in `bindings_engine.cpp`
- Phase 3 (C_StateMachine): deferred transition copied verbatim from `SceneStateMachine`
- Phase 4 (EventBus): char-keyed fixed-slot bus follows LuaStore precedent for buffer sizes
- Phase 5 (Persistent objects): `unique_ptr` ownership transfer follows existing `ObjectCollection` model

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All decisions derived from direct codebase analysis. Zero new dependencies. All patterns verified against live headers and source files at HEAD. |
| Features | HIGH | Validated against three concrete target games (Arkanoid, physics sandbox, tamagotchi). Feature acceptance bar is objective: can the game be built from Lua? |
| Architecture | HIGH | All integration points verified by reading live headers. Component/Object/Scene hierarchy is stable. Proposed modifications follow established patterns exactly. File-level changes listed explicitly in ARCHITECTURE.md. |
| Pitfalls | HIGH | All 10 pitfalls derived from direct codebase analysis of proxy invalidation patterns, Lua ref lifecycle, ObjectCollection update loop, and SceneStateMachine transition logic. No speculation. |

**Overall confidence: HIGH**

### Gaps to Address

- **LuaStore vs full PersistentObjectRegistry scope:** Not resolvable from research alone. Requires building a working tamagotchi prototype with LuaStore serialization on scene transitions. Decision gates Phase 5 scope. Resolve during Phases 2-4 development by running game prototypes with real LuaStore usage.

- **`ObjectCollection::update()` loop snapshot:** Research identified the loop may read `objectCount` fresh each iteration (grep target: `for(size_t i = 0; i < objectCount`). Must be verified before C_StateMachine implementation. If unsafe, one-line fix (`size_t count = objectCount` before loop). Low effort, high correctness impact.

- **Single-proxy-per-component constraint at scale:** Multiple scripts calling `self:get("C_Timer")` on the same object will silently overwrite proxy registrations. Mitigate in v1.6 with a development-mode warning on `setLuaProxy()` overwrite, and the documented cache-in-init pattern. Long-term fix (proxy array `m_luaProxies[4]`) is v1.7+ work.

- **EventBus channel name collision:** No namespace on channel names. Two independently-written scripts could use the same event name for different purposes. Document a naming convention before exposing the API (e.g., `enjin_` prefix is reserved for engine-internal events; game scripts use domain-prefixed names like `game_brick_hit`).

---

## Sources

### Primary (HIGH confidence — direct codebase analysis, 2026-02-28)

- `include/enjin2/core/object.hpp` — Object ownership, `getComponent<T>()`, `setLuaProxy()`, single proxy constraint
- `include/enjin2/core/component.hpp` — Component base class, `update(float dt)`, `assertRequires<T>()`, default destructor (no proxy teardown in v1.5)
- `include/enjin2/core/scene_state_machine.hpp` — `switchTo()` deferred transition, `applyDeferredTransition()`, `pendingSceneId` flag
- `include/enjin2/core/object_collection.hpp` — 128-slot `unique_ptr<Object>`, `addObject()`, `update()` loop structure
- `include/enjin2/core/signal.hpp` — `Signal<T>` with `std::function`, `MAX_CONNECTIONS=16`, `SignalConnection` RAII
- `include/enjin2/scripting/bindings.hpp` — `ScriptProxy`, `LuaStore` `char[64]`/`char[128]` patterns, `m_ssm` pointer injection
- `include/enjin2/scripting/object_proxy.hpp` — `ObjectProxy { Object* object; bool valid; }` — direct proxy pattern to replicate
- `src/scripting/bindings_engine.cpp` — `luaL_ref` for spawn string interning, pointer-to-pointer registry pattern, `engine.scene.*` binding structure
- `.planning/PROJECT.md` — v1.6 active requirements, out-of-scope decisions, single-proxy constraint documentation

### Secondary (HIGH confidence — authoritative API references)

- Lua 5.1 reference manual — `luaL_ref`, `lua_rawgeti`, `luaL_unref`, registry lifetime, full vs light userdata metatable rules (lightuserdata has no metatable in Lua 5.1)
- LuaJIT 2.1 documentation — confirmed `luaL_newlib` not available (Lua 5.2+ API); lightuserdata metatable limitation confirmed

### Tertiary (MEDIUM confidence — external game dev patterns)

- Arkanoid physics (GameDev.net, LOVE2D tutorials) — ball/brick collision approach; existing `engine.collision.*` is sufficient without Box2D
- Tamagotchi FSM analysis — flat FSM is the correct model; hierarchical states not needed
- Game Programming Patterns: Event Queue — string-keyed fixed-slot bus is right approach at enjin2 scale
- Unity DontDestroyOnLoad / Persistent Scene — confirmed SSM-owned collection is the correct C++ analogue for embedded targets

---

*Research completed: 2026-02-28*
*Ready for roadmap: yes*
