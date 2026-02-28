# Pitfalls Research

**Domain:** Adding Lua scripting power + C++ engine foundations to enjin2 (v1.5 Lua Scripting Foundation)
**Researched:** 2026-02-26
**Confidence:** HIGH (based on direct codebase analysis of all relevant source files, prior project design documents, and cross-platform embedded constraints)

---

## Critical Pitfalls

### Pitfall 1: Dangling Object* in ScriptProxy After Scene Destruction

**What goes wrong:**
`ScriptProxy` userdata holds a raw `Object*` pointing into the scene's `ObjectCollection`. When a scene transition occurs, the old scene's objects are destroyed (the `unique_ptr` array in `ObjectCollection` releases them). Any Lua code that captured `self` across a frame boundary — for example, a coroutine, a callback stored in a table, or a delayed event handler — will dereference a freed `Object*` on its next access. The symptom is a hard fault or silent memory corruption on ESP32, and an ASAN/UBSan hit on SDL3 desktop.

This is especially treacherous because the hot path (normal script callbacks) is safe — `self` is injected fresh each call from the current scene's live objects. The bug only surfaces when Lua code stores `self` across a lifetime boundary, which is idiomatic Lua (assigning `local obj = self` at module level is common).

**Why it happens:**
C++ object ownership is clear at the C++ level but invisible from Lua. Lua programmers cannot see that `self` is a pointer wrapper, not a value. Storing `self` in an upvalue is the natural Lua pattern for objects.

**How to avoid:**
- `ScriptProxy` userdata must contain a "generation" token alongside `Object*`. Implement a `generation_id` counter on `ObjectCollection` (or Scene). When a scene is destroyed, increment the generation. Every `ScriptProxy` stores the generation it was created in. On `__index`/`__newindex`, compare the stored generation against the current collection generation — if they differ, the proxy is stale; return nil or raise a Lua error instead of dereferencing.
- Alternatively, use a boolean `valid` flag on the proxy and explicitly invalidate all live proxies when the scene deactivates. The scene would need to maintain a list of issued proxies.
- Document clearly: `self` must not be stored across frame callbacks. The proxy is valid only for the duration of the callback that received it.
- Emit a clear Lua error (not a C++ crash) on stale access: `"error: self is no longer valid (object was destroyed)"`.

**Warning signs:**
- `ScriptProxy` stores `Object*` with no validity check
- No generation/epoch counter on `ObjectCollection` or `Scene`
- Lua scripts that use `local my_self = self` at module scope (top of script, outside any function)
- Scene transition triggered while a stored proxy is still referenced

**Phase to address:**
Self proxy implementation phase — validity check must be part of the initial proxy design, not added retroactively.

---

### Pitfall 2: Pervasive float dt Signature Change — Missed Call Sites Produce Silent Wrong Behavior

**What goes wrong:**
Changing `update(uint16_t deltaTime)` to `update(float dt)` touches every virtual override site across Object, Component, all Component subclasses (animation.hpp, sprite.hpp, canvas.hpp, satellite.hpp, planet.hpp, probe.hpp, effects/postfx.hpp, image_cache.hpp, lua_script.hpp), Scene, ObjectCollection, and SceneStateMachine. Missing a single override produces silent behavioral corruption: the old `uint16_t` override no longer matches the new virtual signature, so the override silently becomes a new (non-virtual) method on the derived class. The base class `update(float dt)` is called instead, which does nothing. The derived class behavior disappears with no compile error.

This is the worst kind of C++ mistake: it compiles cleanly with no warning unless `-Wshadow` or `-Woverride` is enabled, and the failure mode is "feature stops working" not "crash."

**Why it happens:**
C++ does not require `override` keyword. Every existing component that does not use `override` (or uses it on the old `uint16_t` signature) will silently detach from the virtual chain when the base class signature changes. ESP32-IDF toolchains often compile without `-Wall -Wextra` by default.

**How to avoid:**
- Add `-Woverride` (or equivalent) to all three platform builds (SDL3, WASM Emscripten, ESP32-IDF) before beginning the signature change. This makes detached overrides a compile error.
- Change the base class first. Then compile. Every file that fails to compile is a call site that needs updating. Fix each one individually.
- Use the full list: `Object::update`, `Object::lateUpdate`, `Component::update`, `Component::lateUpdate`, `Scene::update`, `Scene::onUpdate`, `ObjectCollection::update`, `ObjectCollection::lateUpdate`, `SceneStateMachine::update`, `SceneStateMachine::updateTransition`, and every concrete component subclass.
- Search for `uint16_t deltaTime` and `uint16_t deltaMs` across all headers and source files; also search for the informal variants `ms`, `msec`, `millis`.
- After the change, any remaining `/ 1000` or `/ 1000.0f` in game update code is a candidate for removal — flag them as leftover from the old millisecond convention.

**Warning signs:**
- A component's animation or movement stops working after the signature change
- Builds pass clean but runtime behavior changes — specific feature stops updating
- Any `update()` method without `override` keyword
- `deltaTime` parameter names still present in any override (naming convention helps catch stragglers)

**Phase to address:**
Float dt migration phase — must be a complete, all-at-once change with `-Woverride` enabled. Do not do it incrementally.

---

### Pitfall 3: std::string name on Object Violates Zero-Alloc Constraint

**What goes wrong:**
Adding `std::string name` to `Object` introduces a heap allocation on every object creation. The heap allocation comes from `std::string`'s SSO (small string optimization) which avoids allocation for strings under ~15 characters, but the exact threshold is implementation-defined and differs between GCC libstdc++ (desktop), Emscripten libc++ (WASM), and ESP32-IDF's xtensa-gcc libstdc++. A name like `"enemy_01"` (8 chars) fits in SSO on desktop. A name like `"enemy_spawner_grid_tile_17"` (25 chars) allocates on the heap — including the ESP32 global heap, not the Lua pool, which is untracked by the custom Lua allocator. This silently breaks the "zero dynamic allocation" invariant.

Additionally, `std::string` in `ObjectCollection::findByName()` creates temporary strings for comparison, and if name lookup is O(n) linear scan, it runs every frame in scripts that call `engine.scene.find()` repeatedly.

**Why it happens:**
`std::string` is the natural C++ string type. On desktop it works perfectly. The embedded constraint is invisible in normal C++ development. ESP32-IDF does support dynamic allocation (it has a heap) but enjin2's design goal is to avoid it for object management.

**How to avoid:**
- Use `char name[32]` (or a compile-time constant `MAX_NAME_LEN`) as a fixed-size array on `Object`. `std::strncpy` for assignment. `std::strcmp` for lookup. Zero allocation, deterministic size, works on all three platforms.
- For `ObjectCollection` name lookup: store a separate `const char* nameIndex[MAX_OBJECTS]` parallel array pointing into each `Object::name` buffer. Linear scan is O(n) but MAX_OBJECTS is 128 — that is 128 `strcmp` calls, acceptable even on ESP32 for a non-hot-path operation. If O(1) is needed, use a fixed-size hash map with open addressing (no `std::unordered_map`).
- Tag arrays follow the same rule: `std::array<const char*, 8>` of string literals (pointers to compile-time strings), not `std::string` tags.
- If `std::string` is used only on desktop (via `#ifdef`) and `char[]` on embedded, this adds complexity. Prefer the `char[]` approach uniformly — it works everywhere and is simpler to reason about.

**Warning signs:**
- `std::string name` as a field on `Object` or `ObjectCollection`
- `std::unordered_map<std::string, Object*>` in `ObjectCollection`
- `Engine::findByName(std::string)` taking `std::string` by value (creates a copy)
- ESP32 build shows heap fragmentation or `malloc` failures that did not exist before the named-object phase

**Phase to address:**
Named objects phase — decide the string representation before writing any name registration code.

---

### Pitfall 4: engine.* Table Registration Must Complete Before Any Script Loads

**What goes wrong:**
If the `engine` global table (with sub-tables `engine.scene`, `engine.input`, `engine.time`, `engine.lua`, `engine.log`) is registered after `lua.loadScript()`, any script that accesses `engine.*` at module level (outside any function body) will see `nil` for `engine` and throw a Lua error: `attempt to index a nil value (global 'engine')`. This is a sequencing bug — the script is loaded and executed top-to-bottom during `loadScript()`; any top-level code runs at that point.

In the current `performReload()` sequence in `sdl_main.cpp`: `initialize()` → `setLayers()` → `setInput()` → `loadScript()`. The engine table registration happens inside `LuaBindings::registerAll()`, which is called by `initialize()`. If `registerAll()` is restructured or if the `engine.*` sub-tables are added in a separate registration step that happens after `loadScript()`, this silently breaks.

The F5 hot-reload path follows the same sequence. An error in the engine table registration (not exposed if the table was not present before) will cause every reload to fail with a confusing "nil" error in user scripts.

**Why it happens:**
The `engine.*` table is new infrastructure. Developers add it to `LuaBindings::registerAll()` but may add it at the end of `registerAll()`, after existing function registrations. The problem only appears with scripts that use `engine.*` at module level. Scripts that only use `engine.*` inside `update()` or `draw()` will not expose the bug.

**How to avoid:**
- Register the `engine` global table as the absolute first action in `LuaBindings::registerAll()`, before any function or constant registration. Document this with a comment.
- Use `lua_createtable` + `lua_setglobal` for the top-level `engine` table, then use `lua_getglobal(L, "engine")` / `lua_setfield()` for sub-tables. Ensure all sub-tables exist (even if empty) before the first script load.
- Add a test script that accesses `engine.time`, `engine.input`, `engine.scene`, `engine.lua`, `engine.log` at module level (not inside any function). If the test script loads without error, the ordering is correct.
- The `performReload()` function must remain sequenced: initialize → register all (including engine table) → setLayers → setInput → loadScript. Any refactoring that separates registration from initialization must preserve this order.

**Warning signs:**
- Lua error `attempt to index a nil value (global 'engine')` on script load
- `engine.*` registration added at the bottom of `registerAll()` rather than at the top
- Scripts that work when `engine.*` is only used inside functions but fail when used at module level
- `performReload()` calls `loadScript()` before all sub-tables are registered

**Phase to address:**
engine.* table phase — make sub-table registration the first line of `registerAll()`, tested with a module-level access script before any other engine.* work proceeds.

---

### Pitfall 5: update(self, dt) Signature Change Breaks All Existing Lua Scripts

**What goes wrong:**
The planned design injects `self` as the first parameter to `update`, changing the Lua-visible signature from `update(dt)` to `update(self, dt)`. Every existing Lua script in the repository (reload_test.lua, layer_demo.lua, pikachu_demo.lua, e2e_parity.lua) uses `function update(dt)`. After the change, all existing scripts will silently receive `self` as `dt` and whatever was `dt` as a second ignored argument. `dt` will be a userdata, not a number. Any arithmetic on `dt` (such as animating positions) will throw a Lua runtime error: `attempt to perform arithmetic on a userdata value (local 'dt')`.

This is a mandatory breaking change. The question is not "if" but "how to handle it safely."

**Why it happens:**
This is unavoidable — the new component-style lifecycle requires `self` as the first argument. The existing flat-script style passes only `dt`. These are incompatible conventions.

**How to avoid:**
- Accept the break explicitly. Document it. Update all scripts in the repository atomically in the same phase that introduces `self` injection.
- Consider a transition mechanism: if the engine detects `function update(dt)` (one parameter) vs `function update(self, dt)` (two parameters), it could use `debug.getinfo` to determine arity and adapt the call. However, this adds complexity and the `debug` library may be disabled on ESP32. Prefer explicit migration over detection.
- After the change, add a comment in `sdl_main.cpp` (or a migration note) explicitly documenting the old and new signatures so future script authors understand why the API uses two parameters.
- The top-level `update(dt)` / `draw()` in the SDL3 runner (called from the C++ game loop, not from a C_LuaScript component) does NOT get `self` injection — it is a scene-level callback, not a per-component callback. Ensure these two contexts are clearly distinguished.
- Write the test script for the `self` proxy with the new signature from the start. Never write a test in the old style that will need to be migrated.

**Warning signs:**
- Lua arithmetic error on `dt` after the change (symptom: self received as dt)
- Any existing script using `function update(dt)` without being updated
- C++ call site passing `(dt)` instead of `(self_userdata, dt)` — the component callback path and the top-level runner path must be distinguished
- `draw()` inadvertently receiving `self` as first argument (draw should not receive self unless specifically designed to)

**Phase to address:**
Self proxy phase — migrate all existing scripts in the same commit that introduces self injection.

---

### Pitfall 6: ScriptErrorPolicy and Hot-Reload Error State Interaction

**What goes wrong:**
The current SDL3 runner uses a `lua_ok` boolean flag: if any `callFunction()` returns an error, `lua_ok` is set to `false` and all subsequent update/draw calls are skipped (the engine enters "paused" error state). F5 resets the Lua state and sets `lua_ok` based on whether the script loaded successfully.

With `ScriptErrorPolicy` added to `C_LuaScript`, there are now two error systems: the global `lua_ok` gate in the runner and the per-component `scriptError` flag in `C_LuaScript`. If they are not coordinated, an error in a component script might be swallowed by the `Disable` policy (component silently disables itself) but the runner's `lua_ok` is also set to `false`, which then prevents ALL components from updating — including components that had no error. Alternatively, if `C_LuaScript` catches errors via its own policy and does not propagate them, the runner's `lua_ok` never learns about errors, masking bugs during development.

A second interaction: F5 hot-reload clears the Lua state (the global state). But `C_LuaScript::scriptError = true` (the per-component flag) may still be set on a component that was NOT reloaded. After F5, the component's error flag should be reset along with the Lua state — but the component is recreated only when the scene reinitializes, which may not happen on every reload.

**Why it happens:**
Two independently designed error mechanisms without a defined protocol for how they interact. The global runner mechanism predates the per-component policy.

**How to avoid:**
- Define the protocol explicitly: `ScriptErrorPolicy::Disable` affects only the per-component error state, not the global runner state. The runner's `lua_ok` is set to `false` only by panics or Lua state initialization failure — not by per-component script errors.
- F5 hot-reload performs a full Lua state reset (already implemented). All scenes and components are recreated (or should be) during reload. Ensure `C_LuaScript::scriptError` is reset to `false` in `C_LuaScript`'s constructor, which is called when the component is re-added to the scene during reinitializaiton.
- `ScriptErrorPolicy::Log` mode should log but not set `lua_ok = false`. `ScriptErrorPolicy::Panic` mode should set `lua_ok = false` and show an error.
- Document the policy hierarchy in a comment at the point where the policy is evaluated.

**Warning signs:**
- F5 reload succeeds (Lua state reinitializes) but a component remains visually disabled
- A single component error stops all other components from updating (over-broad error propagation)
- `scriptError` flag on `C_LuaScript` is not reset between hot-reloads
- `lua_ok = false` triggered by a component-level error that should have been swallowed by Disable policy

**Phase to address:**
Error policy phase — define the two-level error protocol (global vs per-component) before implementing either.

---

### Pitfall 7: Input Event Callback Ordering — When During the Frame Do on_button_pressed Callbacks Fire?

**What goes wrong:**
Adding `on_button_pressed(btn)` and `on_button_released(btn)` callback dispatch to the frame loop requires a precise insertion point. The current frame sequence in `sdl_main.cpp` is:

1. Event pump (SDL events, F5 detection)
2. `input_advance_frame()` — clears current, snapshots previous
3. `input_platform_poll()` — writes new state
4. `g_compositor.clearAll()` — clears canvases
5. `setInput(&g_input)` — wires input to Lua bindings
6. `callFunction("update", dt)` — Lua update
7. `callFunction("draw")` — Lua draw

If `on_button_pressed` callbacks fire at step 3 (immediately after poll), they run before the canvas is cleared. If they call drawing functions, they draw to the previous frame's canvas. If they fire at step 6 (interleaved with `update`), they modify game state during the update call, creating re-entrancy risk. If they fire after `draw` (step 7), they respond one frame late.

The correct position is between steps 5 and 6: after input is fully polled and wired to Lua, before `update`. This matches the behavior of Defold's `on_input` callback and LOVE2D's `keypressed` callback, both of which fire before `update` in the same frame.

**Why it happens:**
SDL3 provides key events in the event pump (step 1), not after the poll (step 3). It is tempting to fire `on_button_pressed` in the event pump alongside other SDL event handling. But at that point, `input_advance_frame` has not yet run — the edge detection (justPressed) is stale from the previous frame.

**How to avoid:**
- Fire `on_button_pressed` / `on_button_released` callbacks immediately after `setInput(&g_input)` (step 5) and before `callFunction("update")` (step 6).
- The detection of which buttons transitioned uses `input_just_pressed()` and `input_just_released()` on the already-advanced-and-polled `InputState` — consistent with how polling functions work.
- The C++ dispatch is a simple loop over all buttons: if `justPressed(btn)`, call the Lua `on_button_pressed(btn)` global (or per-component callback). This is synchronous, within the same frame, before update.
- Do NOT attempt to dispatch events from the SDL event pump. The input poll and advance must complete first.

**Warning signs:**
- `on_button_pressed` callbacks firing with stale edge detection (double-triggering on hold)
- `on_button_pressed` draws to canvas before `clearAll()` has run (drawing to previous frame's buffer)
- Callbacks firing after `draw` (one-frame latency in button response)
- Event dispatch function called from the SDL event pump loop instead of from the main update sequence

**Phase to address:**
Input events phase — define the exact frame position for callback dispatch in the phase plan before writing any dispatch code.

---

### Pitfall 8: GC Step During Frame Causes Frame Time Spike on ESP32

**What goes wrong:**
Lua's garbage collector can run incrementally or in a full collection cycle. On ESP32 with 254 KB Lua memory pool, a full GC at 60 Hz can consume 1–5 ms depending on live heap size. At 30 Hz (33 ms budget), a 5 ms GC spike is a 15% frame overrun — visible as a dropped frame or jerky animation.

The existing `LuaPlatform::tuneGarbageCollector()` configures GC for the platform. But if `engine.lua.collect()` is exposed to scripts and a script calls `engine.lua.collect()` inside `update()`, the GC runs mid-frame. On ESP32 this will cause the frame deadline to be missed.

Additionally, Lua's automatic GC may trigger during `lua_pcall` (during `callFunction("update")`) at any allocation that crosses the GC threshold. The threshold tuning in `tuneGarbageCollector()` controls this, but the interaction with the custom bump allocator is subtle: the bump allocator does not call the system allocator, so Lua's "memory used" tracking may not trigger GC at the expected time if the pool exhaustion behavior differs from the standard allocator's behavior.

**Why it happens:**
GC timing is invisible in SDL3 desktop builds — 5 ms is imperceptible at modern clock speeds. The embedded constraint is only visible on target hardware.

**How to avoid:**
- `engine.lua.collect()` is a valid API but must be documented as "call on scene transitions, not during update or draw." Include a comment in the binding implementation.
- `engine.lua.collect()` should call `lua_gc(L, LUA_GCSTEP, n)` (incremental step), not `lua_gc(L, LUA_GCCOLLECT, 0)` (full collection). Let scripts request a step, not a full GC.
- A separate `engine.lua.full_collect()` (or just document that passing a large step count triggers a full cycle) can be used on scene transitions where a frame drop is acceptable.
- Tune the GC in `tuneGarbageCollector()` for the platform: on ESP32, raise the GC step multiplier to reduce automatic collection frequency. On desktop, lower it for more aggressive incremental GC during development.
- Add a frame-time measurement in the SDL3 debug runner that logs when a frame exceeds the budget. This surfaces GC spikes during development.

**Warning signs:**
- Scripts calling `engine.lua.collect()` inside `update()` or `draw()`
- Frame time spikes of 1–5 ms that are not caused by drawing operations
- `lua_gc(L, LUA_GCCOLLECT, 0)` (full collect) called from a per-frame code path
- `tuneGarbageCollector()` on ESP32 using the same settings as desktop

**Phase to address:**
GC control phase — document the safe call sites for `engine.lua.collect()` in the API comment before exposing the binding.

---

### Pitfall 9: float dt on ESP32 — Soft-Float Overhead on ESP32 Without FPU

**What goes wrong:**
Some ESP32 variants (original ESP32, ESP32-C3 RISC-V) do not have a hardware FPU. Changing `uint16_t deltaTime` to `float dt` means every `update(float dt)` call now involves floating-point arithmetic on a platform that handles floats in software. Each software-float operation is approximately 10–20 CPU cycles instead of 1–2 cycles for integer operations. On an object-heavy scene with 128 objects each with multiple components, the cumulative float overhead across all `update()` calls can consume a measurable fraction of the frame budget.

The ESP32-S3 (the Tomodachi target) DOES have a hardware FPU (Xtensa LX7 with single-precision FPU). So for the primary target, this is not a problem. But the code must run on the base ESP32 as well (per the project constraints), and position updates of the form `x += speed * dt` on the base ESP32 will use soft-float.

**Why it happens:**
Desktop-first development never reveals soft-float overhead. The developer changes `uint16_t` to `float`, everything works correctly at full speed on SDL3, and the ESP32 performance regression is only discovered on device.

**How to avoid:**
- For the Tomodachi primary target (ESP32-S3), the hardware FPU makes `float dt` cost-equivalent to integer operations. Proceed with float dt.
- For base ESP32 compatibility: prefer `float` over `double` everywhere — the soft-float library on Xtensa has hardware acceleration only for single-precision on newer variants. `double` operations are always fully soft-float and twice as slow.
- Avoid `float` in hot tight loops (per-pixel drawing). The `update()` method is called once per object per frame — the overhead is proportional to object count, not pixel count. At 128 objects, this is 128 float multiplications, which is acceptable.
- If ESP32 base performance is a concern, provide a compile-time `ENJIN2_DT_TYPE` option: `float` (default) or `uint16_t` (legacy). This is an escape hatch, not the primary path.
- Measure. Do not optimize speculatively. Establish a frame-time baseline on target hardware before and after the change.

**Warning signs:**
- `double dt` used anywhere in the update chain (use `float` exclusively)
- Per-pixel loops using `float` arithmetic (drawing should remain integer-indexed)
- Frame budget overruns on base ESP32 that did not exist before the float dt change
- `update(double dt)` signatures (Lua's `lua_Number` is `double`; the C++ → Lua bridge should cast to `float` explicitly)

**Phase to address:**
Float dt migration phase — add a note to the phase plan to measure frame timing on ESP32 target before and after if the platform is available.

---

### Pitfall 10: Component Dependency Assertions in Release/Embedded Builds

**What goes wrong:**
The planned `requires<T>()` assertion on `Component::awake()` is designed to fail loudly at construction time when a dependency (e.g., `C_Position`) is missing. In debug builds, this is a `static_assert` or `assert()` that terminates the program with a useful message.

On embedded release builds (ESP32 production firmware), assertions are typically compiled out (`-DNDEBUG`). A missing dependency then becomes the same silent runtime crash that `requires<T>()` was supposed to prevent — the crash just happens later, in `draw()`, with no context.

Furthermore, if `requires<T>()` uses `assert()` and the assertion triggers on ESP32, `abort()` is called. On ESP32, `abort()` by default triggers a watchdog reset — the device reboots silently without any log output unless the serial port is attached. From the user's perspective, the device "crashed" with no information.

**Why it happens:**
Debug-vs-release assertion behavior is a known C++ issue. The embedded release build strips assertions for code size and speed. The consequence of a silent missing dependency is a hard-to-debug runtime crash.

**How to avoid:**
- Implement `requires<T>()` as a two-mode function: in debug builds, `assert(owner->getComponent<T>() != nullptr)` with a log message. In release builds, fall back to graceful disable: if the dependency is absent, log once (via the platform log channel) and call `this->setEnabled(false)`. The component does nothing, which is better than crashing.
- Use a platform-appropriate log channel. On ESP32, `ESP_LOGE(TAG, "...")` writes to serial (if available). On desktop, `std::cerr`. On WASM, `printf` (visible in browser console).
- Do not use `std::string` in the assertion message for embedded builds — use string literals and `typeid(T).name()` (if RTTI is available) or a manually provided `static constexpr const char* TYPE_NAME = "C_Position"` in each component.
- Mark `requires<T>()` failures as non-fatal in release: log + disable, not crash. A disabled but non-crashing component is always safer than a watchdog reset.

**Warning signs:**
- `assert()` inside `requires<T>()` without a release-mode fallback
- `std::string` concatenation in the assertion message (heap allocation in embedded context)
- `abort()` reachable from component initialization on ESP32 release builds
- Missing dependency silently produces no log output in a production build (assertions stripped, no fallback behavior)

**Phase to address:**
Component dependency assertions phase — define the debug vs release behavior explicitly in the implementation plan.

---

### Pitfall 11: Scene Self-Transition via SceneStateMachine Pointer — Circular Destruction Order

**What goes wrong:**
The planned design injects a non-owning `SceneStateMachine*` into each `Scene` at activation: `scene->activate(stateMachine)`. A scene can then call `stateMachine->changeScene(OTHER_ID)` during `onUpdate()`.

The destruction order issue: if a scene calls `stateMachine->changeScene(SAME_SCENE_ID)` (a self-transition), `changeScene()` calls `currentScene->deactivate()` and then `currentScene->activate()` on the same object. If `activate()` calls `onCreate()` which re-creates objects via `objects.addObject<T>()` — but `initialized = true` (set in the first activation) prevents `initialize()` from running again. The result is objects added in `onCreate()` are not initialized on subsequent activations. `Scene::initialize()` has an `if (initialized) return;` early exit.

A second issue: `changeScene()` deactivates the old scene. During deactivation, the scene's destructor or `onDeactivate()` runs. If the C++ destructor of a scene-owned object calls back into the `SceneStateMachine` (e.g., via a signal callback registered against the state machine), the state machine is partially through a transition and the re-entrant call corrupts its state.

**Why it happens:**
The `Scene::initialize()` idempotency guard is correct for the existing use case (a scene is initialized once, activated/deactivated multiple times). But it conflicts with the re-initialization needed for self-transitions. The re-entrant destruction is a signal-safety issue common to event-driven systems.

**How to avoid:**
- For self-transitions, `changeScene()` should detect `targetScene == currentScene` and perform a reset path: call `deactivate()`, then `reset()` (a new method that sets `initialized = false`), then `initialize()`, then `activate()`. This is distinct from a normal scene transition.
- Or: explicitly document that `engine.scene.switch(id)` with the current scene's ID is a full reset. Implement it.
- For re-entrancy: `changeScene()` should set a `transition_pending` flag rather than executing the transition inline. Actual transitions execute at the top of the next `SceneStateMachine::update()` call, not during a nested callback. This is the deferred-transition pattern used by Defold.
- The `SceneStateMachine*` injected into scenes should be a stable, non-owning pointer valid for the entire program lifetime. Do not inject the pointer at activation and revoke it at deactivation — the pointer must remain valid even during deactivation callbacks.

**Warning signs:**
- Self-transition causes scene objects to appear with no `awake()`/`start()` called
- Re-entrant `changeScene()` call during a scene's `onDeactivate()` or destructor
- `transition_pending` not implemented — transitions execute inline during callbacks
- `initialized` flag not reset on self-transition

**Phase to address:**
Scene self-transitions phase — decide deferred-vs-immediate transition semantics before implementing `changeScene()` injection.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| `std::string name` on Object | Works on desktop, no API work | Heap allocation on every object; breaks zero-alloc invariant on ESP32 | Never — use `char name[MAX_NAME_LEN]` |
| ScriptProxy with no validity check | Simple to implement | Use-after-free on scene transition; hard fault on ESP32 | Never — generation check is mandatory |
| `engine.*` sub-tables registered after loadScript | Shorter initialization sequence | Scripts that access engine.* at module level fail with nil error | Never — engine table must precede script load |
| `update(self, dt)` change without migrating existing scripts | Avoid migration work | All existing scripts silently receive wrong argument types | Never — migrate all scripts atomically |
| `assert()` only for requires<T>() with no release fallback | Simpler implementation | Silent crash on ESP32 production builds | Never — always provide a release-mode graceful disable path |
| GC full collect inside update() | Deterministic memory reclaim | 5 ms frame spike on ESP32 at 254 KB pool size | Never during update/draw; only on scene transitions |
| `double dt` in update signature | Matches Lua's lua_Number type | Double soft-float on ESP32 without FPU; twice the cost of float | Never — use float uniformly; cast at Lua boundary |
| Deferred input event dispatch (fire on next frame) | Avoids race condition | 1-frame input latency — noticeable on fast-response interactions | Never — dispatch before update in the same frame |

---

## Integration Gotchas

Common mistakes when connecting the new features to the existing system.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| engine.* table + hot reload | engine.* sub-tables registered once; F5 reinitializes Lua state, which destroys all globals | `registerAll()` re-registers all engine.* sub-tables on every reload; `performReload()` calls `registerAll()` after `initialize()` |
| ScriptProxy + scene destroy | Object* held in proxy becomes dangling when scene destroys ObjectCollection | Generation token or valid flag in proxy; invalidate on scene deactivation |
| float dt + Lua boundary | `lua_pushnumber(L, dt)` pushes as double; C++ receives as float; silent precision loss | Explicit `static_cast<float>(lua_tonumber(L, -1))` on Lua→C++ boundary; document that dt in Lua is a float-precision number |
| on_button_pressed + input advance | Firing callbacks before input_advance_frame runs produces stale edge detection | Dispatch after advance + poll + setInput(), before callFunction("update") |
| ScriptErrorPolicy + lua_ok gate | Per-component Disable policy should not set global lua_ok = false | Global lua_ok = false only for Lua state failure or Panic policy; Disable policy affects only the individual component |
| requires<T>() + NDEBUG | assert() stripped in release builds; missing dependency crashes mid-frame instead of at startup | Provide release-mode fallback: log + setEnabled(false) when dependency is absent |
| GC control + bump allocator | Lua GC threshold triggers don't fire as expected with custom bump allocator | Test GC behavior explicitly; use lua_gc(L, LUA_GCCOUNT, 0) before and after heavy allocation to verify the tracking is correct |
| Scene self-transition + initialized flag | Re-entering a scene after self-transition skips onCreate (initialized guard prevents second init) | Implement reset path that clears initialized flag; or add explicit re-create hook for self-transitions |
| engine.scene.find() + char[] names | String comparison uses strcmp, not == on std::string; char[] name must be null-terminated | Always use strncpy for name assignment; zero-terminate; use strncmp for lookup |
| Component name table + typeid | Using typeid(T).name() for component names produces mangled names on some toolchains | Register a manual static constexpr TYPE_NAME in each component; use that for Lua-visible names |

---

## Performance Traps

Patterns that work at small scale but degrade on ESP32 or under embedded constraints.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| engine.scene.find() called every frame from Lua | O(n) string scan * frame rate; 30 Hz * 128 objects = 3840 comparisons/sec | Cache the result in a Lua local: `local player = engine.scene.find("player")` in init() | ESP32 at ~240 MHz with 128 objects; visible at 60 Hz |
| GC full collect mid-frame | 1–5 ms frame spike on ESP32 at 254 KB pool | Call engine.lua.collect() only in scene transition callbacks | ESP32 with > 32 KB live Lua heap |
| float accumulator in Lua script with double precision | Precision loss accumulates over time (Lua numbers are double; dt comes in as float) | Document that Lua dt is float-precision; avoid accumulating over thousands of frames | Long-running sessions (> 1 hour at 30 Hz = 108K frames) |
| Iterating ObjectCollection in update() via forEach() | std::function callback overhead * 128 objects * 60 Hz = 7680 indirect calls/sec | Use indexed loops in C++; only expose named find functions to Lua (not bulk iteration) | Measurable on ESP32; invisible on desktop |
| Dynamic script proxy creation per callback | Allocating userdata per update/draw call | Reuse a single persistent userdata per C_LuaScript instance; update its Object* pointer each call | Any embedded target — allocation pressure on Lua pool |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **float dt:** Every `update(uint16_t deltaTime)` override site updated — grep `uint16_t.*delta` and `uint16_t.*ms` across all headers and sources; zero matches means migration is complete
- [ ] **float dt:** All `/1000` or `/1000.0f` divisions in update() bodies removed — these are vestiges of the old millisecond convention
- [ ] **Named objects:** Object name stored as `char[N]`, not `std::string` — grep `std::string name` in object.hpp confirms none
- [ ] **Named objects:** `findByName()` uses `strcmp`, not `std::string::operator==` — confirm in objectcollection.cpp
- [ ] **engine.* table:** All five sub-tables (scene, input, time, lua, log) registered before first script load — test with module-level access script
- [ ] **engine.* table:** F5 hot-reload re-registers all engine.* sub-tables — test reload then access engine.time.delta() in update()
- [ ] **ScriptProxy:** Accessing `self` after scene destruction raises a Lua error, not a C++ crash — test by storing self in a global table across a transition
- [ ] **update(self, dt):** All existing scripts (reload_test.lua, layer_demo.lua, pikachu_demo.lua, e2e_parity.lua) updated to new signature — grep `function update(dt)` confirms no old-style signatures remain
- [ ] **on_button_pressed:** Callback fires in the same frame the button is pressed, not the next frame — test by logging frame count in both callback and update
- [ ] **ScriptErrorPolicy:** Disabled component does not prevent other components from updating — test by introducing a syntax error in one C_LuaScript; verify sibling components still run
- [ ] **F5 error clear:** Hot-reload on F5 resets lua_ok to true (if script loads successfully) even after a prior runtime error — test by pressing F5 after intentional error
- [ ] **GC control:** engine.lua.collect() triggers an incremental step, not a full collection — verify via engine.lua.memory() before and after: small decrease for incremental, large decrease for full
- [ ] **requires<T>():** Missing dependency in release build logs a message and disables the component — does NOT crash or abort — test by removing a required component from an object in a release-config build
- [ ] **Scene self-transition:** Re-entering the current scene calls onCreate() again (objects re-created) — test by wiring a self-transition and verifying object count resets

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Dangling Object* in ScriptProxy (crash on scene transition) | HIGH | Add generation counter to ObjectCollection; modify ScriptProxy.__index to check validity; re-test all script-based scene transitions |
| Missed update() override (behavior silently disappears after float dt change) | MEDIUM | Enable -Woverride on all targets; fix each detached override; re-test each component type individually |
| std::string name on Object (heap fragmentation on ESP32) | MEDIUM | Replace std::string with char[MAX_NAME_LEN]; update ObjectCollection::registerByName and findByName; rebuild and measure heap usage |
| engine.* table missing at module-level script access (nil error) | LOW | Move engine.* table registration to top of registerAll(); reload all scripts; verify with module-level test |
| Existing scripts broken by update(self, dt) change | LOW | Script migration is mechanical: change function update(dt) to function update(self, dt); run test suite after each script update |
| GC spike mid-frame (frame budget overrun on ESP32) | LOW | Audit all engine.lua.collect() call sites; move full-collect calls to scene transition callbacks; add frame-time logging |
| requires<T>() crashes on ESP32 release (assert stripped) | MEDIUM | Wrap assert with #ifdef NDEBUG / #else / #endif; add fallback setEnabled(false) path; rebuild release; test on device |
| Self-transition skips onCreate (initialized guard) | LOW | Add reset() method to Scene that clears initialized flag; call it in the self-transition code path; verify object count after transition |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Dangling Object* in ScriptProxy | Self proxy phase — add generation token to initial design | Store self in global table; trigger scene transition; access stored self; expect Lua error, not C++ crash |
| float dt missed override sites | float dt phase — enable -Woverride before first change | Zero new compiler warnings after full migration; all existing component behaviors confirmed working |
| std::string name allocation | Named objects phase — decide char[] vs string before writing Object name field | grep std::string in object.hpp and object_collection.hpp: zero matches |
| engine.* registration ordering | engine.* table phase — register before loadScript in performReload | Module-level access test script loads without nil error |
| update(self, dt) breaks existing scripts | Self proxy phase — migrate all scripts atomically | grep `function update(dt)` in scripts/: zero matches |
| ScriptErrorPolicy + lua_ok interaction | Error policy phase — define two-level error protocol | Introduce error in one component; verify other components still update |
| Input event callback ordering | Input events phase — define frame position before implementation | Log frame count in callback vs update; confirm same frame delivery |
| GC spike on ESP32 | GC control phase — document safe call sites in binding comment | Frame-time log shows no spike when engine.lua.collect() in update(); verify only in scene transition |
| float soft-float on ESP32 | float dt phase — use float not double everywhere | No double-precision temporaries in update chain; review generated assembly if performance regresses |
| requires<T>() release crash | Component assertions phase — define debug/release behavior in plan | Release-config build with missing dependency: component disables, log appears, no abort |
| Scene self-transition initialized guard | Scene self-transitions phase — implement reset path | Self-transition test: object count matches fresh scene creation; no objects skipped |
| Circular destruction on scene transition | Scene self-transitions phase — implement deferred transition | Transition requested during onDeactivate: executes on next update, not inline |

---

## Sources

- Codebase analysis: `include/enjin2/core/object.hpp` — `update(uint16_t deltaTime)` signature, MAX_COMPONENTS=16, no name field (2026-02-26)
- Codebase analysis: `include/enjin2/core/component.hpp` — `update(uint16_t deltaTime)` on all overrides; no requires<T>() (2026-02-26)
- Codebase analysis: `include/enjin2/core/scene.hpp` — `update(uint16_t deltaTime)`, `onUpdate(uint16_t)`, no SceneStateMachine* field (2026-02-26)
- Codebase analysis: `include/enjin2/core/object_collection.hpp` — `update(uint16_t deltaTime)`, no name map, no tag array (2026-02-26)
- Codebase analysis: `include/enjin2/core/scene_state_machine.hpp` — `update(uint16_t deltaTime)`, changeScene() executes inline (no deferred transition) (2026-02-26)
- Codebase analysis: `src/platform/sdl/sdl_main.cpp` — frame sequence: event pump → advance → poll → clearAll → setInput → callFunction("update") → callFunction("draw") (2026-02-26)
- Codebase analysis: `src/scripting/lua_engine.cpp` — bump allocator, memoryPool, pushArg<float> casts to double (2026-02-26)
- Codebase analysis: `src/scripting/bindings.cpp` — registerAll() registration order, lua_pushlightuserdata for bindings instance (2026-02-26)
- Codebase analysis: `include/enjin2/scripting/lua_platform.hpp` — MEMORY_LIMIT=256KB on ESP32, ENABLE_DEBUG=false on ESP32 (2026-02-26)
- Codebase analysis: `include/enjin2/components/lua_script.hpp` — update(uint16_t deltaTime), scriptError flag, no ScriptErrorPolicy enum (2026-02-26)
- Project design: `project/lua-embedding-design.md` — update(self, dt) signature rationale, engine.* table design, ScriptProxy userdata design, GC recommendations (2026-02-25)
- Project design: `project/cpp-engine-improvements.md` — float dt pervasive change rationale, named objects char[] approach, requires<T>() design, SceneStateMachine injection pattern (2026-02-25)
- Defold documentation: on_input callback fires before update in same frame; deferred message passing pattern
- ESP32 technical reference: ESP32-S3 (Xtensa LX7) has single-precision FPU; base ESP32 (Xtensa LX6) no FPU; ESP32-C3 (RISC-V) no FPU
- Lua 5.x reference: lua_gc LUA_GCSTEP vs LUA_GCCOLLECT semantics; lua_Number is double by default
- C++ standard: virtual override detachment when base signature changes without -Woverride flag; longjmp UB with RAII objects across call frames

---
*Pitfalls research for: Lua scripting power + C++ engine foundations — enjin2 v1.5 Lua Scripting Foundation*
*Researched: 2026-02-26*
