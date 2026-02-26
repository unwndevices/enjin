# Project Research Summary

**Project:** enjin2 v1.5 — Lua Scripting Foundation
**Domain:** Embedded/WASM 2D game engine — Lua scripting power + C++ engine foundations
**Researched:** 2026-02-26
**Confidence:** HIGH

## Executive Summary

enjin2 v1.5 is a well-scoped, internally consistent milestone that transforms the engine from a rendering toolkit into a scriptable game runtime. The ten features divide cleanly into two categories: C++ foundations (float dt, named objects, scene self-transitions) that must ship first because they are prerequisites, and Lua-surface features (engine.* table, ScriptProxy, input callbacks, GC control, error policy, component assertions) that depend on those foundations. This dependency structure is clear and the implementation path is fully verified against the live codebase. No new external dependencies are introduced — every feature is built from the existing LuaJIT C API, C++17 stdlib, and enjin2 headers.

The recommended approach follows a "foundations before surface" ordering: fix the onRender Pixel4 correctness bug first (zero risk, unblocks rendering tests), then complete the pervasive float dt signature migration all at once (the widest change, must be atomic), then add the C++ object/scene foundations, then wire the Lua-visible engine.* table, and finally implement ScriptProxy (the highest-complexity and highest-value single item). The pattern choices — `std::string` names (SSO-safe for typical names), `const char*` tag arrays over dynamic collections, full userdata over lightuserdata, `strcmp` dispatch over hash maps — are all driven by the zero-dynamic-allocation constraint that must hold across SDL3, WASM, and ESP32 targets simultaneously.

The top risks are all known and preventable. The most dangerous is `ScriptProxy` dangling pointer on scene transition — this must be addressed in the initial proxy design with a generation token or valid flag, not retrofitted. The second highest risk is the float dt signature migration producing silent behavioral regressions if any override detaches without `-Woverride` enabled. Both pitfalls have clear, low-cost prevention strategies. The onRender Pixel4 bug must be fixed before any phase that depends on rendering to work correctly.

---

## Key Findings

### Recommended Stack

v1.5 introduces zero new external dependencies. Every feature is implemented using the stack already present: LuaJIT 2.1.1753364724 (Lua 5.1 API), C++17, SDL3, and the existing enjin2 headers. The critical API constraint is that `luaL_newlib` (Lua 5.2+) is unavailable — all table registration must use the `lua_newtable` + `lua_pushcfunction` + `lua_setfield` chain already established in `bindings.cpp`. LuaJIT full userdata (not lightuserdata) is required for ScriptProxy because lightuserdata has no metatable support in Lua 5.1. The ESP32-S3 primary target has a hardware single-precision FPU — `float` dt is the correct choice; `double` would be soft-float and 3–10x slower.

**Core technologies:**
- **LuaJIT 2.1 (Lua 5.1 API):** Scripting runtime — use raw C API exclusively; no binding libraries (sol2/luabridge would bypass the static pool allocator); verified against `luajit/src/lua.h` (`LUA_VERSION_NUM 501`)
- **C++17:** `std::array`, `std::string_view`, `if constexpr`, `static_assert` — all available; `std::string` for `Object::name` (SSO keeps typical names heap-free); `assertRequires<T>()` not `requires<T>()` (C++20 keyword collision risk)
- **SDL3:** Desktop runner only — input polling, frame timing, hot-reload (F5); WASM and ESP32 runners unaffected by v1.5 additions; runner already computes `float dt` correctly

See `.planning/research/STACK.md` for full pattern implementations, API compatibility tables, and per-platform notes.

### Expected Features

**Must have (table stakes) — required for Lua scripts to do basic game logic:**
- **Fix onRender Pixel4 bug** — correctness regression; `if constexpr` guard in `Scene::render()` silently skips `onRender(ICanvas<Pixel4>&)`; 3-line fix, must be first
- **float dt everywhere** — `update(uint16_t ms)` → `update(float dt)`; expected by LOVE2d, Defold, Unity conventions; ESP32 variable timing makes this especially important
- **engine.* namespaced global table** — replaces flat globals with `engine.scene`, `engine.input`, `engine.time`, `engine.lua`, `engine.log`; required for script discoverability
- **Self proxy injection (ScriptProxy)** — `self` as first argument to every callback; required for scripts to access and modify their own object's properties
- **Named object registry + tags** — `engine.scene.find("name")` and 8-slot `const char*` tag array; required for scripts to locate other objects
- **Scene self-transitions** — `engine.scene.switch(id)` from Lua; required for scripts to drive scene flow
- **ScriptErrorPolicy** — `Disable`/`Log`/`Panic` enum on `C_LuaScript`; required for embedded robustness (reboot-on-error is unacceptable on ESP32)

**Should have (differentiators) — adds meaningful value without deep complexity:**
- **Input event callbacks** — `on_button_pressed(btn)` / `on_button_released(btn)`; wraps existing `InputState` edge detection; no new C++ state required
- **GC control** — `engine.lua.collect()` + `engine.lua.memory()`; prevents mid-frame GC spikes on ESP32; two `lua_gc` wrappers
- **Component dependency assertions** — `assertRequires<T>()` template on `Component`; catches missing dependencies at startup; debug asserts loudly, release disables gracefully

**Defer (v1.6 scope per PROJECT.md):**
- ComponentProxy / `self:get(typename)` — heaviest C++ work; needs type registry and safe proxy lifetime management
- Persistent objects across scene transitions — requires root-level collection outside Scene
- Event bus (engine.emit / engine.on) — separate communication subsystem
- C_Timer, C_StateMachine — standalone new components

See `.planning/research/FEATURES.md` for full prioritization matrix, anti-feature rationale, and feature dependency graph.

### Architecture Approach

The architecture follows a strict layered model: SDL3 runner → Scripting layer (LuaScriptSystem + LuaBindings) → Graphics layer → Core layer. v1.5 additions stay within this existing structure. The only new files are `ScriptProxy` (new header + source in `enjin2_lua`), requiring a single CMake source addition. All other changes are modifications to existing files. The `engine.*` table uses the pattern already established for `love.graphics` in `bindings.cpp`. Scene self-transition uses forward declaration to break the circular header dependency between `scene.hpp` and `scene_state_machine.hpp`. Input event callbacks dispatch globally from `LuaBindings::dispatchInputEvents()` — consistent with the existing decoupled architecture where Lua bindings do not iterate ECS components directly.

**Major components after v1.5:**
1. **LuaBindings** (`src/scripting/bindings.cpp`) — gains `engine.*` table registration, `SceneStateMachine*` pointer, `setTime()`, `dispatchInputEvents()`; central wiring point for all new Lua-visible features
2. **ScriptProxy** (`include/enjin2/scripting/script_proxy.hpp` + `src/scripting/script_proxy.cpp`) — new full userdata with `__index`/`__newindex`/`__gc`; maps `self.x`, `self.y`, `self.visible`, `self.layer`, `self.name`, `self.active` to C++ component reads/writes
3. **C_LuaScript** (`src/components/lua_script.cpp`) — gains `ScriptErrorPolicy` enum, float dt signature, ScriptProxy injection before each callback invocation
4. **Core ECS types** — `Object` gains `std::string name` and `std::array<const char*, 8>` tags; `Scene` gains `SceneStateMachine*` injection; all gain `float dt` signatures
5. **sdl_main.cpp** — gains `setSceneStateMachine()` in `performReload()`, `dispatchInputEvents()` before `callFunction("update")`, `setTime(dt)` accumulation each frame

See `.planning/research/ARCHITECTURE.md` for full data flow diagrams, build order dependency graph, memory budget, and anti-patterns to avoid.

### Critical Pitfalls

1. **Dangling `Object*` in ScriptProxy after scene destruction** — Lua scripts can store `self` in upvalues across frame boundaries; when the scene is destroyed, the stored `Object*` becomes a dangling pointer causing hard faults on ESP32. Prevention: add a generation token or `valid` flag to `ScriptProxy` in the initial design; invalidate on scene deactivation. Cannot be retrofitted safely.

2. **float dt signature change — missed override sites produce silent behavioral regression** — C++ silently detaches overrides when base signature changes without `override` keyword; the detached method becomes a new non-virtual function and the base class no-op runs instead. Prevention: enable `-Woverride` across all three platform builds before making the first signature change; change base class first, compile, fix every error, treat as an atomic all-or-nothing migration.

3. **`engine.*` table registered after script load causes nil error** — any script that accesses `engine.*` at module level (outside a function body) will see `nil` if the table is not yet registered. Prevention: register the `engine` global table as the absolute first action in `registerAll()`, before any other registration; test with a module-level access script as the completion gate.

4. **`update(self, dt)` signature change breaks all existing scripts** — every existing Lua script uses `function update(dt)`; after self injection, the `dt` parameter receives the ScriptProxy userdata and all arithmetic on it throws a runtime error. Prevention: migrate all scripts (`reload_test.lua`, `layer_demo.lua`, `pikachu_demo.lua`, `e2e_parity.lua`) atomically in the same commit that introduces ScriptProxy injection.

5. **Scene self-transition skips `onCreate()` due to `initialized` guard** — `Scene::initialize()` has an `if (initialized) return;` early exit that prevents re-initialization on self-transition; also, inline `changeScene()` during `onDeactivate()` causes re-entrant state machine corruption. Prevention: implement a `reset()` method that clears `initialized`; implement deferred (not inline) transition execution.

See `.planning/research/PITFALLS.md` for all 11 critical pitfalls, the "Looks Done But Isn't" checklist, integration gotchas table, and recovery strategies.

---

## Implications for Roadmap

Based on the combined research, the architecture's build-order dependency graph maps directly to a 10-phase roadmap. The ordering is driven by three rules: correctness fixes before new work, C++ foundations before Lua surface, and pervasive changes before targeted additions.

### Phase 1: Fix onRender Pixel4 Bug
**Rationale:** Correctness regression. The `if constexpr` guard in `Scene::render()` silently skips `onRender(ICanvas<Pixel4>&)` on all derived scenes. Any test or demo relying on scene-level rendering is broken. This is a 3-line fix with zero risk that unblocks all subsequent rendering validation.
**Delivers:** Correct scene rendering dispatch for Pixel4 canvas type across all derived scenes.
**Addresses:** Pre-existing correctness failure; unblocks rendering tests before other features land.
**Avoids:** False-negative test results during subsequent phases.

### Phase 2: float dt Migration
**Rationale:** The widest single change in the milestone. Touches every `update(uint16_t)` override across `Component`, `Object`, `ObjectCollection`, `Scene`, `SceneStateMachine`, `C_LuaScript`, and all concrete component subclasses. Must be done atomically and completely before any new components are written (otherwise new components would be written with the wrong signature). The SDL3 runner already passes `float dt` correctly — only the C++ side is inconsistent.
**Delivers:** Consistent `float dt` (seconds) throughout the entire C++ update chain; eliminates all `/ 1000` division patterns.
**Addresses:** float dt table stakes feature; prerequisite for `update(self, dt)` Lua signature.
**Avoids:** Silent override detachment (Pitfall 2) — enable `-Woverride` before starting, treat as atomic; soft-float overhead — use `float` not `double` throughout; verify no `uint16_t.*delta` grep matches remain.

### Phase 3: Named Objects + Tags
**Rationale:** Pure C++ core change with no Lua dependencies. Provides the `Object::name` field and `ObjectCollection::findByName()` / `findAllWithTag()` methods that `engine.scene.find()` will call. Doing this before the Lua surface keeps the C++ and Lua work cleanly separated.
**Delivers:** `std::string name` on `Object` (SSO-safe for typical short names); `std::array<const char*, 8>` tag array; `findByName()` (linear scan over MAX_OBJECTS=128) and `findAllWithTag()` on `ObjectCollection`.
**Addresses:** Named object registry + tags feature; prerequisite for `engine.scene.find()`.
**Avoids:** Heap allocation on ESP32 (Pitfall 3) — `std::string` SSO keeps names under ~15 chars heap-free; `const char*` array for tags is zero allocation.

### Phase 4: Scene Self-Transitions
**Rationale:** Pure C++ core change. Injects `SceneStateMachine*` into `Scene` at activation time via a forward-declared pointer (breaking the circular header dependency). Required before `engine.scene.switch()` can be implemented.
**Delivers:** `SceneStateMachine*` in `Scene`; `reset()` method to clear `initialized` flag; deferred transition flag in `SceneStateMachine`; correct self-transition support.
**Addresses:** Scene self-transitions feature; prerequisite for `engine.scene.switch(id)`.
**Avoids:** Circular destruction / re-entrancy (Pitfall 11) — implement deferred transition, not inline execution during callbacks; `initialized` guard issue — add `reset()` path for the self-transition code.

### Phase 5: engine.* Global Table
**Rationale:** The Lua-side payoff for Phases 3 and 4. With C++ foundations in place, this phase wires the complete `engine.scene`, `engine.input`, `engine.time`, `engine.lua`, and `engine.log` sub-tables into `LuaBindings::registerAll()`. `engine.scene` requires the `SceneStateMachine*` from Phase 4; `engine.scene.find()` requires `findByName()` from Phase 3.
**Delivers:** Complete `engine.*` global table with all five sub-tables; `SceneStateMachine*` wired in SDL runner's `performReload()`; `setTime(dt)` accumulation; `dispatchInputEvents()` declared (implemented in Phase 8).
**Addresses:** engine.* namespaced global table; input polling re-namespaced under `engine.input`; time API; GC API stubs.
**Avoids:** Registration ordering bug (Pitfall 4) — register `engine` table as the first action in `registerAll()`; verify with a module-level access test script before proceeding.

### Phase 6: ScriptProxy Userdata (Self Proxy)
**Rationale:** The highest-complexity and highest-value feature. Requires `Object::name` from Phase 3 (for `self.name`) and the float dt signature from Phase 2 (for the `update(self, dt)` call site). Creates the `ScriptProxy` full userdata with `__index`/`__newindex`/`__gc` metatables and injects `self` as the first argument to every `C_LuaScript` callback.
**Delivers:** `ScriptProxy` userdata (new header + source); `self.x`, `self.y`, `self.visible`, `self.layer`, `self.name`, `self.active` read/write; `update(self, dt)`, `draw(self)`, `init(self)` callback signatures; all existing scripts migrated to new signature in the same commit.
**Addresses:** Self proxy injection (highest-priority Lua feature); script callback signature change.
**Avoids:** Dangling Object* crash (Pitfall 1) — generation token or valid flag must be in initial ScriptProxy design; existing script breakage (Pitfall 5) — migrate all scripts atomically in the same commit.

### Phase 7: ScriptErrorPolicy
**Rationale:** Independent of all ScriptProxy work once float dt lands (Phase 2). Touches only `C_LuaScript` and the `lua_ok` gate interaction. Can slot in here without risk.
**Delivers:** `ScriptErrorPolicy` enum (`Disable`/`Log`/`Panic`) on `C_LuaScript`; two-level error protocol (global `lua_ok` vs per-component policy); hot-reload error state reset verified.
**Addresses:** Script error policy feature; embedded robustness requirement.
**Avoids:** lua_ok / ScriptErrorPolicy interaction bug (Pitfall 6) — `Disable` policy must not set global `lua_ok = false`; define the two-level protocol before implementing either.

### Phase 8: Input Event Callbacks
**Rationale:** Wraps the existing `InputState` edge detection that already works correctly. Requires the `engine.*` table from Phase 5 for namespace context. Implementation is a small loop in `LuaBindings::dispatchInputEvents()` plus a call site in `sdl_main.cpp`.
**Delivers:** `on_button_pressed(btn)` and `on_button_released(btn)` global Lua callbacks; dispatched after `input_platform_poll`, before `callFunction("update")`.
**Addresses:** Input event callbacks feature.
**Avoids:** Callback ordering / stale edge detection (Pitfall 7) — must fire after `input_advance_frame` + `input_platform_poll` + `setInput()`, never from the SDL event pump.

### Phase 9: GC Control
**Rationale:** Two `lua_gc` wrappers registered under `engine.lua` (subtable already created in Phase 5). Zero C++ complexity. Must be documented with clear usage constraints for embedded targets.
**Delivers:** `engine.lua.collect()` and `engine.lua.memory()`; API comments specifying collect() is for scene transitions, not per-frame use.
**Addresses:** GC control feature; ESP32 mid-frame frame-spike prevention.
**Avoids:** GC spike during frame (Pitfall 8) — use `LUA_GCSTEP` (incremental) not `LUA_GCCOLLECT` (full) for `collect()`, or document that full collect must only be called at scene boundaries.

### Phase 10: Component Dependency Assertions
**Rationale:** Pure `enjin2_core` change. Header-only template method on `Component` base. Fully independent once float dt lands (Phase 2). Low risk, low complexity.
**Delivers:** `assertRequires<T>()` protected template on `Component`; debug builds assert loudly; release builds log once and call `setEnabled(false)` (no abort on ESP32).
**Addresses:** Component dependency assertions feature; developer ergonomics.
**Avoids:** Release-build crash with assert stripped (Pitfall 10) — `#ifdef NDEBUG` dual-path is mandatory; use `assertRequires<T>()` not `requires<T>()` (C++20 keyword collision).

### Phase Ordering Rationale

- **Correctness before new work:** Phase 1 (Pixel4 bug fix) must precede everything so rendering tests are valid during subsequent phases.
- **Pervasive changes before targeted additions:** Phase 2 (float dt) touches every component and must be complete before any new component is written with a signature. Deferring it means a larger, riskier migration later.
- **C++ foundations before Lua surface:** Phases 3–4 establish the C++ types that Phases 5–6 expose to Lua. The dependency is hard: `engine.scene.find()` cannot be implemented without `findByName()`; `engine.scene.switch()` cannot work without the SSM pointer injection.
- **engine.* table before ScriptProxy:** Phase 5 must precede Phase 6 because the SDL runner wiring it establishes is needed for ScriptProxy's callback injection path. Also, testing the engine.* table in isolation before adding ScriptProxy complexity reduces debugging surface.
- **Independent features grouped at end:** Phases 7–10 are independent of each other and of ScriptProxy. They can be reordered freely if implementation constraints shift.

### Research Flags

Phases requiring careful attention during execution (not additional research, but execution decisions that must be made before coding begins):

- **Phase 2 (float dt):** High blast radius. Enable `-Woverride` first. Treat as a single atomic commit. Use the PITFALLS.md "Looks Done But Isn't" checklist: zero `uint16_t.*delta` grep matches, zero `/ 1000` vestiges.
- **Phase 6 (ScriptProxy):** Highest complexity. The generation/validity check for dangling pointers must be designed before any code is written. The script migration must happen atomically with the C++ change.
- **Phase 5 (engine.* table):** Verify with a module-level access test script before any subsequent phase begins. This is the canary for the registration ordering pitfall.
- **Phase 4 (Scene self-transitions):** Decide deferred-vs-inline transition semantics before writing any `changeScene()` injection code. Inline causes re-entrancy during `onDeactivate()`.

Phases with standard, well-documented patterns (low execution risk):
- **Phase 1 (Pixel4 bug fix):** 3-line change, zero new concepts.
- **Phase 7 (ScriptErrorPolicy):** Enum + switch statement; isolated to one file.
- **Phase 8 (Input event callbacks):** Simple loop wrapping existing `InputState` API.
- **Phase 9 (GC control):** Two `lua_gc` wrappers; identical pattern already in `LuaEngine::getMemoryUsage()`.
- **Phase 10 (Component assertions):** Header-only template; `#ifdef NDEBUG` dual-path is established C++ idiom.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All claims verified against live codebase: `luajit/src/lua.h` (Lua 5.1 API constants), `CMakeLists.txt` (C++17 standard), `sdl_main.cpp` (float dt already computed correctly), `bindings.cpp` (existing table registration pattern confirmed) |
| Features | HIGH | All ten features verified against current code state (`include/enjin2/core/` headers); reference engine comparisons grounded in Defold, LOVE2d, Solarus documentation; PROJECT.md authoritative for scope boundaries |
| Architecture | HIGH | All integration points verified against live source; new file count (2: `script_proxy.hpp` + `script_proxy.cpp`) and CMake change confirmed; build order derived from actual header dependency analysis |
| Pitfalls | HIGH | All 11 pitfalls derived from actual codebase analysis — `initialized` guard in `scene.hpp`, inline `changeScene()` in `scene_state_machine.hpp`, `lua_ok` gate in `sdl_main.cpp`, frame sequence in `sdl_main.cpp`; not speculative |

**Overall confidence:** HIGH

### Gaps to Address

- **ScriptProxy validity mechanism:** Research identifies two valid approaches (generation token on `ObjectCollection` vs explicit `valid` flag invalidated at scene deactivation). The choice affects how `ObjectCollection` tracks issued proxies. Decide during Phase 6 planning before writing the first line of proxy code.
- **`engine.lua.collect()` semantics:** Research recommends `LUA_GCSTEP` (incremental) for `collect()` and reserving `LUA_GCCOLLECT` (full) for a documented separate use case. The API surface (one function vs two) should be decided in Phase 9 planning and captured in the binding comment.
- **`engine.scene.find()` return type:** When a named object is found, does it return a ScriptProxy userdata (giving the caller `self`-like access) or a raw integer handle? The ARCHITECTURE.md shows it returning a `ScriptProxy`, but the lifetime implications (the returned proxy is not a callback-injected proxy — it could outlive the call frame) interact directly with the dangling pointer pitfall from Phase 1. Resolve during Phase 6 planning.
- **ESP32 base (non-S3) float performance:** Research flags that base ESP32 (Xtensa LX6, no FPU) runs float dt via soft-float (~10–20 cycles per operation vs 1–2 for integer). The primary target (ESP32-S3, hardware FPU) is unaffected. If base ESP32 compatibility is tested, establish a frame-time baseline before and after Phase 2.

---

## Sources

### Primary (HIGH confidence — live codebase, verified 2026-02-26)
- `include/enjin2/core/object.hpp` — current `update(uint16_t)` signature, `ObjectCollection` structure, no name/tag fields confirmed
- `include/enjin2/core/scene.hpp` — onRender `if constexpr` bug (lines 116–126), `SceneStateMachine` ownership model
- `include/enjin2/core/scene_state_machine.hpp` — `changeScene()` inline execution, `completeTransition()` injection site identified
- `src/scripting/bindings.cpp` — `lua_newtable`/`lua_setfield` pattern for `love.graphics` (lines 211–237), `LUA_REGISTRYINDEX` binding retrieval pattern
- `src/platform/sdl/sdl_main.cpp` — frame sequence, `float dt` already computed (line 246), `input_advance_frame`/`input_platform_poll` ordering (lines 253–254)
- `luajit/src/lua.h` — `LUA_VERSION_NUM 501`, all `LUA_GC*` constants confirmed
- `luajit/src/luajit.h` — `LUAJIT_VERSION "LuaJIT 2.1.1753364724"` confirmed
- `src/scripting/lua_engine.cpp` — `lua_gc(L, LUA_GCCOUNT, 0)` in `getMemoryUsage()` — confirms GC control pattern
- `project/lua-embedding-design.md` — reference engine survey, ScriptProxy design rationale, engine.* table structure
- `project/cpp-engine-improvements.md` — float dt rationale, named object design, scene self-transition injection pattern

### Secondary (MEDIUM confidence — reference engine patterns and domain knowledge)
- Defold component scripting model — `self` as first argument, `on_input` callback fires before `update` in same frame, deferred message passing
- LOVE2d convention — float seconds for delta time, `keypressed` callback before `update`
- Solarus — `sol.*` table pattern for namespace structure
- Unity — `RequireComponent` attribute pattern (inspires `assertRequires<T>()`)
- Playdate documentation — `collectgarbage()` GC control pattern, frame budget documentation for embedded

### Tertiary (context, no direct verification needed)
- ESP32-S3 technical reference — Xtensa LX7 single-precision FPU confirmed; float dt cost-equivalent to integer on this target
- C++ standard — virtual override detachment when base signature changes without `-Woverride`; `requires` as reserved C++20 keyword

---
*Research completed: 2026-02-26*
*Ready for roadmap: yes*
