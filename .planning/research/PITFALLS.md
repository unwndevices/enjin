# Pitfalls Research

**Domain:** Adding debug draw, save/load serialization, persistent objects, camera follow helpers, coroutines/async, tweens, UI components, bindings.cpp refactoring, and null safety to existing zero-alloc 2D game engine with Lua scripting (enjin2 v1.7 Developer Experience)
**Researched:** 2026-03-01
**Confidence:** HIGH (direct codebase analysis of all v1.6 shipped headers and bindings, cross-referenced against PROJECT.md constraints, and first-principles reasoning from embedded + Lua VM design)

---

## Critical Pitfalls

### Pitfall 1: Debug Draw Renders Into the Wrong Canvas Layer

**What goes wrong:**
`engine.debug.*` bindings (e.g., `drawBBox`, `drawCircle`) draw directly to the current active canvas. If the developer calls `engine.debug.drawBBox(x, y, w, h)` inside `update()` — before `draw()` is called — the canvas has not been cleared yet (or was cleared at the start of the previous frame). The debug primitive is drawn onto stale pixel data from the previous frame, producing ghost overlaps or invisible shapes depending on what was rendered before.

Even worse: if debug draw targets `layerCanvases[activeLayer]` but the scene renders objects on a different layer, the debug primitive is composited on the wrong layer and either appears behind all game content (drawn on layer 0 with game on layer 2) or is invisible (drawn on layer 3 which is transparent/hidden).

A second ordering failure: debug draw from inside a C++ component's `draw()` method (called by `Scene::renderObjects()`) interleaves with sorted drawable rendering. A debug box drawn mid-render-pass appears under drawables rendered after it in the sort order, even if the developer expects it to be "on top."

**Why it happens:**
Debug draw is typically treated as a pass that happens after all gameplay rendering, in a dedicated debug render pass. enjin2 has no concept of a "post-render pass." The `Scene::renderObjects()` path collects and sorts drawables, then iterates — there is no hook after this loop. The natural place to call debug draw (inside `update()`, inside a Lua callback, inside a component `draw()`) is the wrong place.

**How to avoid:**
- `engine.debug.*` functions must write into a dedicated, always-on-top layer (e.g., `layerCanvases[layerCount - 1]`, or a separate debug overlay canvas). This layer is cleared at the start of each frame by the engine, not by the script.
- The debug layer must be cleared automatically by the SDL runner (or WASM host) each frame, NOT by the Lua script. This guarantees debug primitives are fresh each frame regardless of when they are drawn.
- Document that `engine.debug.*` functions are valid inside `draw(self)` only, not `update(self, dt)`. The debug layer is cleared after `draw()` completes.
- Alternatively, buffer all debug draw calls in a fixed-size command list and flush them as the final rendering step. This avoids layer management entirely at the cost of a fixed command buffer (e.g., `DebugCmd cmds[64]`).

**Warning signs:**
- `engine.debug.drawBBox()` called inside `update()` instead of `draw()`
- Debug primitives appear one frame late (drawn onto current frame, visible next frame)
- No dedicated debug canvas or layer — debug draws on `currentCanvas` which is the same canvas game content uses
- No automatic clear of the debug layer at frame start

**Phase to address:**
Debug draw bindings phase — define the canvas routing and clear protocol before registering any `engine.debug.*` functions.

---

### Pitfall 2: Coroutine Lua State Outlives the Lua Thread It Was Created On

**What goes wrong:**
Lua coroutines are created via `coroutine.create(fn)`. Each coroutine is a `lua_State*` (a thread, sharing the parent state's registry). When a Lua script uses a coroutine for a loading screen or animation, the coroutine's `lua_State` is alive as long as something holds a reference to the coroutine thread.

The failure: hot reload (F5) calls `lua_close()` on the parent `lua_State`. Any coroutine thread derived from that state is implicitly invalid after `lua_close()`. If the C++ side holds a `lua_State*` pointer to a coroutine thread (e.g., stored in a `C_Coroutine` component or in the `LuaBindings` coroutine registry), it now holds a dangling pointer. Calling `lua_resume()` on a closed thread's state is undefined behavior — it may crash, corrupt memory, or silently succeed (reading garbage).

The second failure: if the `engine.coroutine.*` API stores active coroutine thread pointers in a fixed array (`lua_State* m_threads[MAX_COROUTINES]`), and hot reload closes the parent state without iterating that array to null the entries, each thread pointer is dangling. Any future `resume()` call fires into freed memory.

**Why it happens:**
Coroutines are Lua-level objects. Developers model them as "lightweight threads" and store the thread pointer in C++ as if it were a persistent handle. But the thread's validity is coupled to the parent `lua_State` lifecycle. After `lua_close()`, the parent and all its threads are gone. The C++ `lua_State*` pointers in the coroutine array are not automatically nulled — the developer must explicitly clear them on teardown.

**How to avoid:**
- The `engine.coroutine.*` implementation must expose a `clearCoroutines()` teardown function, symmetric with `C_Timer::clearTimers()` and `LuaEventBus::clearHandlers()`. This function is called by the script system before `lua_close()`.
- Do NOT store raw `lua_State*` thread pointers in C++ after `lua_close()`. The teardown protocol is: (1) iterate all active coroutine slots, (2) set each slot's state pointer to `nullptr`, (3) call `lua_close()` on parent.
- Coroutine threads should be tracked using `int` Lua registry refs (`luaL_ref(L, LUA_REGISTRYINDEX)` on the coroutine thread value), NOT raw `lua_State*` pointers. This way the GC controls thread lifetime, and `luaL_unref` on hot reload correctly frees the thread.
- The fixed coroutine array: `CoroutineSlot slots[MAX_COROUTINES]` where each slot holds `int threadRef` (not `lua_State*`). To resume: `lua_rawgeti(L, LUA_REGISTRYINDEX, slot.threadRef)` → `lua_tothread(L, -1)` → `lua_resume(thread, ...)`.

**Warning signs:**
- `lua_State* m_coroutineThreads[N]` in C++ without a nulling teardown before `lua_close()`
- `lua_resume()` called after F5 reload on a thread ref from the previous Lua state
- Coroutine array not cleared in the hot-reload path (`performReload()` → `registerAll()`)
- `clearCoroutines()` not called before `lua_close()` in the shutdown sequence

**Phase to address:**
Coroutine/async phase — design the teardown protocol before implementing any resume/yield mechanism.

---

### Pitfall 3: Tween Holds a Lua Callback Reference That Becomes Stale After Hot Reload

**What goes wrong:**
A tween helper (e.g., `engine.tween.to(target, "x", 100, 0.5)`) animates a value over time and fires an `onComplete` callback when done. The `onComplete` callback is stored as a `luaL_ref` handle in the tween slot.

After F5 hot reload, the parent `lua_State` is destroyed and a new one is created. Any `luaL_ref` from the old state is invalid in the new state. If the tween system's fixed-size slot array (`TweenSlot slots[MAX_TWEENS]`) is stored in `LuaBindings` (which survives across `registerAll()` calls on the same `LuaBindings` instance), the stale `callbackRef` values remain in the slot array. On the next frame, the tween continues advancing (if `elapsed < duration`), fires the callback, calls `lua_rawgeti(newL, LUA_REGISTRYINDEX, staleRef)`, and gets a wrong function or nil.

The second failure: tweens that target Lua userdata properties (e.g., "animate `self.x` from 0 to 100") store a reference to the `ScriptProxy` userdata and a property string. After hot reload, the `ScriptProxy` is invalidated (its `valid = false` is set by `C_LuaScript`'s destructor). A tween that resumes and tries to write to the proxy via `__newindex` gets a "object has been destroyed" error — at best. At worst, if the validity check is absent in the tween write path, it writes to a dangling `C_LuaScript*`.

**Why it happens:**
Tweens combine two hot-reload hazards in one: stale `luaL_ref` callback handles (same as C_Timer) and stale ScriptProxy targets (same as ObjectProxy). The compound nature means developers who handle one hazard correctly may miss the other.

**How to avoid:**
- Tween teardown must be symmetric with C_Timer teardown. Add `clearTweens()` called before `lua_close()`, which calls `luaL_unref(L, LUA_REGISTRYINDEX, slot.callbackRef)` for all active slots, then sets all slot state to inactive.
- Tween callbacks must store `lua_State*` alongside `int callbackRef` (same discipline as C_Timer). Compare `L == m_L` before `lua_rawgeti` at fire time.
- Tween targets should use `int tweenTargetRef` (a registry ref to the target userdata) rather than raw `ScriptProxy*`. At advance time, check the userdata's `valid` flag before writing. If invalid, silently cancel the tween.
- `registerAll()` must call `clearTweens()` before rebuilding the registry. Do not rely on slot reuse to overwrite stale refs.

**Warning signs:**
- `TweenSlot` holds `int callbackRef` without `lua_State*`
- `clearTweens()` not called in hot-reload path
- Tween target stored as raw `ScriptProxy*` instead of registry ref with validity check
- No nil check after `lua_rawgeti` for tween callback

**Phase to address:**
Tween helpers phase — design teardown and target validity before implementing any advance/fire logic.

---

### Pitfall 4: Coroutines Yield Inside a pcall — Lua 5.1 Cannot Resume Across a pcall Boundary

**What goes wrong:**
enjin2 uses standard Lua 5.1 (likely LuaJIT based on `luajit/` directory). In Lua 5.1, a coroutine cannot yield across a C call stack boundary. If a coroutine yields inside a function that was called via `lua_pcall()` (which is how `callWithProxy()` calls all Lua functions), the yield raises an error: "attempt to yield across metamethod/C-call boundary."

This is the most common coroutine pitfall in embedded Lua. The enjin2 binding layer calls all Lua functions through `callWithProxy()` which uses `lua_pcall()` internally. A script that does:
```lua
function update(self, dt)
    coroutine.wrap(function()
        coroutine.yield()  -- ERROR: yields across C pcall boundary
    end)()
end
```
will raise an error on the first yield.

**Why it happens:**
Lua 5.2+ and LuaJIT have continuations that allow yielding across C call boundaries. Lua 5.1 does not. Developers who know LuaJIT's C API (`lua_yieldk`, coroutines work with LuaJIT's special handling) or who learned coroutines on Lua 5.2+ make assumptions that do not hold in standard 5.1.

The project uses LuaJIT (the `luajit/` directory is present). LuaJIT DOES support yielding across `lua_pcall` in some cases (via its CoCo coroutine implementation, which patches the C stack). However, this requires LuaJIT to be compiled with CoCo support (default on most platforms) and the yield cannot cross a non-yieldable C call. The behavior is platform-dependent — CoCo works on x86/ARM but may not work on ESP32 RISC-V or Emscripten.

**How to avoid:**
- Do not yield inside `update(self, dt)` or `draw(self)` directly. These are called via `callWithProxy()` → `lua_pcall()`. A yield attempt raises an error.
- The correct pattern: create a coroutine in `init(self)`, store it in a Lua local. In `update(self, dt)`, call `coroutine.resume(co)` explicitly. The coroutine body yields normally; the `resume()` call in `update()` is a Lua→Lua call, not a C→Lua pcall boundary.
- For LuaJIT on ESP32: verify CoCo support at build time. If CoCo is absent (e.g., WASM), coroutine yield across any C boundary fails. Document the restriction: "coroutines must be resumed from Lua, not from C-called Lua functions."
- Provide a `Coroutine` helper table in Lua (not a C binding) that wraps `coroutine.wrap()` with the correct resume-from-update pattern. This keeps the implementation in Lua where yield semantics are clear.

**Warning signs:**
- `coroutine.yield()` called directly inside `update(self, dt)` or `draw(self)` without checking call origin
- No documentation on the pcall boundary restriction
- `engine.coroutine.create(fn)` C binding that calls `lua_resume()` from C inside a `lua_pcall()` scope
- No cross-platform test for LuaJIT CoCo availability (especially WASM and ESP32 targets)

**Phase to address:**
Coroutine/async phase — design the coroutine resume pattern around pcall boundaries before writing any C bindings.

---

### Pitfall 5: bindings.cpp Split Breaks Static Forward Declarations Between Translation Units

**What goes wrong:**
`bindings.cpp` is 1390 lines with extensive forward declarations at the top (`static int lua_proxy_index_impl`, `static int lua_proxy_get_component_impl`, etc.). These static functions are used across the file. When the file is split into `bindings_debug.cpp`, `bindings_ui.cpp`, `bindings_tween.cpp`, etc., each new TU cannot see the `static` functions from `bindings.cpp`. `static` linkage is TU-local.

The failure mode: `bindings_ui.cpp` needs `getBindings(L)` (defined in `bindings.cpp`). If `getBindings` is declared `static`, it is invisible to `bindings_ui.cpp`. The new file will not compile (undefined reference), OR the developer copies `getBindings` into the new file, creating a second definition — which is fine for `static` (each TU gets its own copy) but produces duplicated logic that diverges over time.

A second failure: the `g_currentBindings` global (line 16 of `bindings.cpp`: `static LuaBindings* g_currentBindings = nullptr`) is TU-local by `static` linkage. If any new `.cpp` file tries to access or set `g_currentBindings`, it accesses a different (uninitialized) copy of the variable, not the one set during initialization.

**Why it happens:**
`static` at file scope means "internal linkage." This is correct for file-private helpers but prevents sharing across TUs. When a monolithic file is split, all the TU-private helpers that were implicitly shared within the file become invisible across the split.

The correct pattern for the split: shared functions become `extern` (declared in a private header, defined in exactly one `.cpp`). File-private helpers remain `static` but only in the one `.cpp` that owns them.

**How to avoid:**
- Before splitting, audit `bindings.cpp` for functions that are referenced by more than one future TU. These must be converted to non-`static` member functions of `LuaBindings` (already accessible via the header) or to non-`static` free functions declared in `bindings_internal.hpp` (not installed as a public header).
- `getBindings(L)` is already a `public static` member of `LuaBindings` — it is accessible from all TUs that include `bindings.hpp`. This is the correct model for shared binding utilities.
- `g_currentBindings` — if it is used only in `bindings.cpp` for the ScriptProxy metatable, keep it `static` in that file and do not reference it from new TUs. New TUs use `getBindings(L)` instead.
- Add a `bindings_internal.hpp` for non-public shared declarations (e.g., `REQUIRE_CANVAS` macro, `LuaFuncDef` helper). This header is included by all `bindings_*.cpp` files but not exported as a public API.
- Compile-test each new split TU in isolation before committing. Use `nm` or `readelf` to verify no undefined symbols remain in the split files.

**Warning signs:**
- `static int someHelper(lua_State* L)` function in `bindings.cpp` referenced by a new `bindings_*.cpp` file (linker error)
- `g_currentBindings` accessed from more than one TU (reads zero instead of initialized pointer)
- New `.cpp` file re-defines `getBindings` as a local copy (divergence risk)
- No internal header for shared split-file declarations

**Phase to address:**
Bindings refactoring phase — audit `static` linkage before splitting; establish `bindings_internal.hpp` as the first step.

---

### Pitfall 6: Save/Load Serialization Writes Non-Portable Binary Data on ESP32

**What goes wrong:**
The existing `LuaStore::saveToFile()` uses JSON (conditional on `VCV_RACK`). A new save/load serialization helper that goes beyond `LuaStore`'s 16-key limit may be tempted to write binary data (direct `fwrite` of structs) for speed and simplicity on embedded targets. This fails for two reasons:

1. **ESP32 endianness vs WASM/SDL3 endianness**: ESP32 (Xtensa) is little-endian; x86/WASM is also little-endian. So endianness is not a problem here — but struct padding is. A struct written on ESP32 with a Xtensa-GCC layout may have different padding than the same struct compiled on x86-GCC. A save file written on SDL3 desktop may be unreadable on ESP32 if the struct layout differs.

2. **ESP32 NVS limitations**: ESP32 Non-Volatile Storage (NVS) has key-length limits (15 chars), value-size limits (per-namespace, ~4 KB per key for blob type, total NVS partition size of 16 KB typical), and write cycle limits (100K writes per key before flash wear). Binary blob NVS writes that serialize an entire `GameState` struct on every frame tick will exhaust flash in hours.

**Why it happens:**
Desktop developers reach for `fwrite(struct)` as the fastest path. Embedded developers know NVS but underestimate its limits. Neither group accounts for the multi-platform constraint that enjin2 explicitly carries.

**How to avoid:**
- Use JSON-based save format only, matching the existing `LuaStore` pattern. JSON is human-readable, portable, and the `LuaStore` save/load infrastructure already exists — extend it rather than building a parallel system.
- For the serialization helper, define max key/value limits that work across platforms: key length ≤ 15 chars (NVS limit), total keys ≤ 32 (fits in NVS partition), value strings ≤ 256 bytes.
- Save only on explicit `engine.store.save()` call or scene transition, never inside `update()`. The existing `LuaStore::saveToFile()` guards this via the store path mechanism.
- If binary format is required for ESP32 performance, use a fixed-layout struct with explicit `uint8_t` padding bytes (portable layout), document the format version, and add a magic number at the start for format validation.
- NVS write path is deferred — mark it as WASM/SDL3 only for v1.7; ESP32 NVS integration requires a separate phase with wear-leveling analysis.

**Warning signs:**
- `fwrite(&gameState, sizeof(GameState), 1, fp)` in any platform-shared code path
- Save/load functions inside `update()` rather than in explicit API calls
- NVS key names longer than 15 characters
- No file format version or magic number in binary save files
- Binary save written on SDL3 and tested only on SDL3 (endianness/padding differences with ESP32 not tested)

**Phase to address:**
Save/load serialization phase — define the format constraints and platform scope before writing any serialization code.

---

### Pitfall 7: Camera Follow Sets Position Every Frame, Fighting Manual Override

**What goes wrong:**
`engine.camera.follow(target)` is intended to call `C_Camera::lookAt(target.x, target.y, lerpSpeed)` every frame. If this is implemented as a "set it and forget it" registration (the camera stores a target reference and follows automatically in `C_Camera::update()`), a script that calls `engine.camera.shake(5, 0.3)` and also has a follow target active will have the shake cancelled: `lookAt()` sets the target position, and on the next frame `update()` lerps toward the target, overwriting the shake offset.

The existing `C_Camera::update()` applies lerp toward `m_target` and advances `m_shakeElapsed`. If `lookAt()` is called every frame from Lua (in the script's `update()`) AND shake is active, the lerp destination changes every frame — but the shake still accumulates correctly in `m_shakeOffset`. This is actually fine: shake is additive on top of the lerped position. The problem is ONLY if the follow implementation calls `setPosition()` (which resets lerp residual) instead of `lookAt()`.

A second failure: `engine.camera.follow(target)` in the Lua script holds an `ObjectProxy` to the target object. If the target is destroyed (`engine.scene.destroy(target)`), the ObjectProxy's `valid` flag is `false`. The next frame's follow call reads `target.x` through the stale proxy — which raises a Lua error ("object has been destroyed"), crashing the script.

**Why it happens:**
Camera follow is often a "fire and forget" feature in game engines (Unity's `Camera.Follow(target)`). enjin2's design of explicitly calling `lookAt()` each frame from Lua is correct but requires the script to handle target lifetime. The ObjectProxy validity check is the correct protection, but scripts that trust `engine.camera.follow(target)` to be safe "forever" will not add the guard.

**How to avoid:**
- `engine.camera.follow(target)` is a Lua convenience that the script must call each frame inside `update()`. It is NOT a registration. Document this explicitly: "call `engine.camera.follow(target)` every frame in `update()` — it is not an automatic subscription."
- Do NOT implement follow as a C_Camera internal feature with `m_followTarget`. This creates a target lifetime coupling that is hard to clean up correctly.
- In the Lua API: `engine.camera.follow(target)` should check if the passed `ObjectProxy` is valid (non-nil, `proxy.valid`) before reading its position. Provide a null guard in the C binding function that checks the ObjectProxy validity and silently returns if the target is destroyed.
- Use `lookAt()` not `setPosition()` inside the follow implementation. `lookAt()` respects lerp and shake additive semantics.

**Warning signs:**
- `C_Camera` stores `ObjectProxy*` or `Object*` as `m_followTarget` (lifetime coupling)
- `engine.camera.follow(target)` reads `target.x` without checking `proxy.valid`
- Follow implementation calls `setPosition()` instead of `lookAt()`, cancelling lerp residual
- No documentation that follow is per-frame, not a subscription

**Phase to address:**
Camera follow helpers phase — define the per-frame pattern and validity guard before binding `engine.camera.follow()`.

---

### Pitfall 8: UI Component Bindings Allocate State in LuaBindings That Is Never Reset on Hot Reload

**What goes wrong:**
`engine.ui.*` bindings (progress bars, stat bars, etc.) maintain rendering state in `LuaBindings` member variables. The pattern in existing bindings: add members to `LuaBindings`, initialize in `registerAll()`, read/write from `static int lua_engine_ui_*` C functions. If UI state (e.g., a `UIBar` array of active bar descriptors) is added as a member of `LuaBindings`, it must be reset in `registerAll()` — the same function used by hot reload.

The failure: if `LuaBindings::registerAll()` does not explicitly reset the UI member arrays (`for(auto& bar : m_uiBars) bar = {};`), the bars retain state from the previous script on F5 reload. Scripts that do not call `engine.ui.createBar()` on reload see bars from the previous session rendered on screen. The bars are drawn from C++ during the draw pass — they do not depend on Lua calling any function to render.

A second failure: if UI component state includes `int luaCallbackRef` values (e.g., an `onChange` callback for a stat bar), these refs are invalid after hot reload. Same hazard as C_Timer and tweens, same fix: call `luaL_unref` before `lua_close`, null the refs in reset.

**Why it happens:**
The existing `resetSpritePool()` function in `LuaBindings` is the correct pattern — it clears all sprite pool state explicitly. The hot-reload path calls `resetSpritePool()` from `registerAll()`. New subsystems added to `LuaBindings` must follow the same discipline: each subsystem needs a `resetXxx()` function called from `registerAll()`.

**How to avoid:**
- Add `resetUIState()` (called from `registerAll()`) that zeros all UI member arrays: `for(auto& bar : m_uiBars) bar = {}`.
- Any `int callbackRef` in UI state follows the `clearTimers()`/`clearHandlers()` teardown pattern: call `luaL_unref` for each active ref before zeroing.
- Fixed-size UI arrays: `UIBarSlot m_uiBars[MAX_UI_BARS]` with `active` flag, matching all other fixed arrays in the engine. Do not use `std::vector`.
- Document the reset requirement in a comment above each new member group in `LuaBindings`: `// UI state — reset in resetUIState() called from registerAll()`.

**Warning signs:**
- New `LuaBindings` member arrays without a corresponding `resetXxx()` call in `registerAll()`
- UI bars visible after F5 reload without the script explicitly creating them
- `int callbackRef` in UI state not `luaL_unref`'d before hot reload
- `std::vector` used for UI state list (violates zero-alloc constraint)

**Phase to address:**
UI component bindings phase — add `resetUIState()` before any UI state members are added; include it in `registerAll()` immediately.

---

### Pitfall 9: Persistent Objects Own LuaStore Data That Is Reset When the Store Is Reloaded

**What goes wrong:**
Persistent objects (objects that survive scene transitions) may carry Lua-side state through the `LuaStore` (via `engine.store.save()`/`engine.store.load()`). The store is loaded from disk in `setStorePath()` → `loadFromFile()`. On scene transition, the persistent object's `C_LuaScript` is NOT destroyed — it keeps its Lua state and its script's global variables.

The failure: the script author designs the persistent object to save state to `engine.store` on every scene exit and reload from `engine.store` on every scene enter. On scene transition, the SSM destroys the old scene (destroying its `ObjectCollection`) before `activate()`-ing the new scene. The persistent object's Lua script fires its `on_scene_exit` event handler, calls `engine.store.save()`, and writes state to the in-memory store. Then on new scene init, the persistent script calls `engine.store.load()` and reads state back.

If, however, the persistent object's script is reloaded from disk (via `loadScriptFile()`) during the scene transition — which can happen if the persistent object's script path is relative and the new scene changes the asset path — the Lua state is torn down and `engine.store.load()` is never called. The store's in-memory data from `engine.store.save()` is still there, but the newly-loaded script starts fresh from its `init()` callback and must re-read from the store explicitly. Without a clear protocol for this, the persistent object's state is silently lost.

**Why it happens:**
Persistent object lifecycle has three potentially different events: scene transition (object survives, Lua state survives), script reload (Lua state destroyed, object survives in C++), and session restart (everything destroyed). The interaction between store persistence and Lua state persistence is underspecified.

**How to avoid:**
- Define a strict ordering: persistent object scripts are NOT reloaded on scene transition. They keep their Lua state. Only the scene's scripts are torn down.
- Document: "persistent objects' Lua scripts are not reloaded across scene transitions. The script's `update()` and `draw()` continue running. To save persistent state, call `engine.store.save()` at explicit save points (not in `update()`)."
- If the persistent object's script must access scene-specific resources (sprites, tilemaps from the new scene), it should obtain those via `engine.scene.find()` after the new scene is active — not during the transition window.
- Add a `on_scene_change(new_scene_id)` callback for persistent scripts (optional, only fires if defined). This gives persistent scripts a clear hook for scene-transition-aware logic without conflating it with `update()`.

**Warning signs:**
- Persistent object script calls `loadScriptFile()` during scene transition
- `engine.store.save()` called inside `update()` on every frame for a persistent object
- No documented distinction between "persistent Lua state" and "persisted store data"
- Persistent object tries to access scene-specific objects during the transition window (m_activeScene is null)

**Phase to address:**
Persistent objects phase (v1.7 variant) — define the Lua state persistence model before designing the scene transition hooks.

---

### Pitfall 10: Null Safety Fixes Introduce Behavioral Changes in Existing Binding Chains

**What goes wrong:**
The null safety improvement phase aims to add guards in binding chains where pointer dereferences currently fail silently or crash. The risk: adding a null guard changes a function that previously returned garbage or crashed to now return `nil` or `0`. Scripts that were "working" by relying on the garbage value (or by being tested on platforms where the nullptr dereference happened to read zeros) now get different behavior.

Example: a binding that currently does:
```cpp
C_Position* pos = owner ? owner->getPosition() : nullptr;
lua_pushinteger(L, pos->getPosition().x);  // crashes if pos == nullptr
```
After fix:
```cpp
C_Position* pos = owner ? owner->getPosition() : nullptr;
lua_pushinteger(L, pos ? static_cast<lua_Integer>(pos->getPosition().x) : 0);
```
Scripts on ESP32 that ran on objects that always had a `C_Position` never triggered the crash path. SDL3 desktop tests pass. But a script that intentionally used an object without `C_Position` was getting 0 (from the crash path's undefined behavior coincidentally reading zero) and now gets 0 from the null guard — no change. This is a good outcome.

The bad outcome: a binding that returns `nil` instead of a default value after null-guard addition causes Lua code that does `local x = self.x + 10` to get `nil + 10` → Lua runtime error. Previously, the same code got `0 + 10 = 10` (from the undefined-behavior-that-happened-to-work path). The null guard "fixes" the crash but "breaks" the script's arithmetic.

**Why it happens:**
Null safety is retroactive. The existing script API has implicit contracts ("this always returns a number") that were enforced by the implementation coincidentally, not by design. When the contract is made explicit with a null guard, the implicit contract may change.

**How to avoid:**
- For every null guard added, document the new return value: "returns 0 if C_Position is absent" vs "returns nil if C_Position is absent." Prefer returning 0 (not nil) for numeric properties — this preserves arithmetic operability.
- Run the existing test suite (all 27+ ctests) after each null guard addition. Do not batch null guard changes — add them one function at a time to isolate regressions.
- Run example scripts (`scripts/arkanoid.lua`, `scripts/tamagotchi.lua`) under SDL3 before and after. Behavioral differences are regressions.
- Where a null guard changes return type (crashes → nil), add an assertion in debug mode: "if owner is null here, this is a programming error — the script is accessing a property on an object that doesn't exist." Use `luaL_error` in that case rather than returning nil silently.

**Warning signs:**
- Null guards added in bulk across many functions in one commit
- No test run between null guard additions
- Null guard returns `nil` for properties previously guaranteed to return numbers (breaks script arithmetic)
- No distinction between "expected null" (feature: C_Position is optional) and "unexpected null" (bug: owner destroyed, proxy should be invalid)

**Phase to address:**
Null safety phase — add guards one function at a time, run ctests after each, document the return value contract for each guarded function.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Debug draw writes to `currentCanvas` instead of dedicated layer | No layer setup needed | Debug primitives are destroyed by next `clear()` call; cannot survive across update/draw | Never — debug layer must be separate and auto-cleared |
| Coroutine stores raw `lua_State*` thread pointer in C++ | Simple pointer equality check for resume | Dangling after `lua_close()`; undefined behavior on resume | Never — use `int` registry ref + `lua_tothread()` pattern |
| Tween target as raw `ScriptProxy*` instead of registry ref | Avoids extra Lua stack operations | Write to freed memory if component destroyed mid-tween | Never — proxy validity must be checked at advance time |
| bindings.cpp split without `bindings_internal.hpp` | Faster split (copy-paste helpers) | Duplicated `getBindings()` and macro logic diverges over time | Never — internal header is a one-time cost |
| Binary save format for ESP32 speed | Fast serialization | Non-portable across compilers, struct padding differences, no NVS wear analysis | Only with explicit padding bytes, format version, and NVS write analysis |
| Follow target stored in `C_Camera` as `Object*` | "Fire and forget" API | Dangling pointer when target destroyed; no proxy validity check | Never — per-frame `lookAt()` call from Lua is the correct pattern |
| UI state not reset in `registerAll()` | Skip reset = faster reload | Stale UI bars visible after F5 hot reload | Never — `resetUIState()` in `registerAll()` is a one-liner |
| `coroutine.yield()` from inside `update()` | Intuitive API | Fails on Lua 5.1 pcall boundary; unreliable on LuaJIT + WASM/ESP32 | Never — yield must be from a coroutine resumed in Lua, not from a C-called function |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Debug draw + camera offset | Debug boxes drawn in world space appear offset from intended position when camera is active | Debug draw functions accept screen-space coordinates (skipping camera offset), not world-space |
| Coroutines + pcall boundary | `coroutine.yield()` inside `update(self, dt)` raises "attempt to yield across C boundary" | Create coroutine in `init()`, resume with `coroutine.resume(co)` from `update()` Lua code |
| Tweens + hot reload | Tween's `callbackRef` is stale after F5; fires wrong function | `clearTweens()` before `lua_close()`; compare `L` at fire time |
| Tweens + invalid target | Tween writes to destroyed ScriptProxy via stale raw pointer | Target stored as registry ref; `valid` flag checked before each write |
| Persistent objects + scene.find() | `engine.scene.find()` returns nil for persistent objects not in current scene | Extend `find()` to search SSM-level persistent collection after scene search |
| Persistent objects + store | Store reloaded from disk overwrites in-memory state written by persistent script | Persistent scripts save/load explicitly at defined save points, not in `update()` |
| bindings.cpp split + `static` helpers | New TU cannot see `static int someHelper()` from `bindings.cpp` | Shared helpers → `LuaBindings` static member or `bindings_internal.hpp` extern |
| Save/load + ESP32 NVS | Long key names (> 15 chars) silently fail NVS writes | Key names ≤ 15 chars enforced by LuaStore; NVS path documented as platform limit |
| Camera follow + destroyed target | `engine.camera.follow(deadProxy)` reads invalid proxy, raises Lua error | Follow binding checks `proxy.valid` before reading `x`/`y`; returns silently if stale |
| UI callbacks + hot reload | `int onChangeRef` in UIBarSlot is stale after F5 | `resetUIState()` calls `luaL_unref` for all active callback refs before zeroing slots |
| Null safety + script arithmetic | Guard returns `nil` for numeric property; `nil + 10` is a Lua runtime error | Null guards return `0` for numeric properties, not `nil`; use `luaL_error` for unexpected null |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Debug draw called every frame from `update()` drawing hundreds of bounding boxes | Frame time spikes; visible on ESP32 as lag | Guard with `if DEBUG_DRAW_ENABLED` compile flag; disable for release builds | ESP32 with > 32 objects under debug draw at 128x128 pixel budget |
| Tween advances checked every frame for all MAX_TWEENS slots | O(MAX_TWEENS) scan per frame even for 1 active tween | Early-out counter: `m_activeTweenCount`; skip full scan if 0 | Any platform with MAX_TWEENS > 16 and sparse usage |
| `coroutine.resume(co, dt)` called every frame even when coroutine is dead | `lua_resume()` on a dead coroutine returns `LUA_ERRRUN`; each failure pays pcall overhead | Check `coroutine.status(co) ~= "dead"` before resume; remove dead coroutines from active list | ESP32 with > 8 concurrent coroutines all finishing at different times |
| Save file written on every `update()` loop via persistent object's Lua code | NVS write cycle exhaustion on ESP32; slow I/O on SDL3 | Document: save only on scene transitions or explicit user action | ESP32 after ~100K total writes to the same NVS key |
| UI progress bars redrawn every frame with `fillRect()` covering full bar width | Pixel-perfect animation at cost of full bar repaint each frame | Bar rendering is cheap at 128x128; only a problem with > 32 bars simultaneously | ESP32 at very high frame rates with many UI elements; acceptable for Tomodachi use |
| `engine.camera.follow(target)` calls `lookAt()` every frame with new target position | Lerp speed set to 1.0 (snap) negates smooth follow | Document lerpSpeed parameter; ensure script uses lerpSpeed < 1.0 for smooth follow | Any platform — incorrect lerpSpeed causes jitter or instant snap |

---

## "Looks Done But Isn't" Checklist

- [ ] **Debug draw:** Debug layer cleared at start of each frame automatically — verify the SDL runner and WASM host clear the debug canvas before `draw()` is called
- [ ] **Debug draw:** `engine.debug.*` functions accept screen-space coordinates (not affected by camera offset) — verify coordinate system documented and consistent
- [ ] **Coroutines:** `clearCoroutines()` called before `lua_close()` in the teardown sequence — verify coroutine registry refs are `luaL_unref`'d
- [ ] **Coroutines:** Coroutine resume pattern documented as "resume from Lua `update()`, not from C pcall context" — check for yield-inside-pcall in example scripts
- [ ] **Coroutines:** LuaJIT CoCo availability verified on WASM and ESP32 targets — check `LUAJIT_ENABLE_LUA52COMPAT` and `LUAJIT_USE_COCO` build flags
- [ ] **Tweens:** `clearTweens()` called in `registerAll()` hot-reload path — verify in `performReload()` sequence
- [ ] **Tweens:** Tween `callbackRef` stores `lua_State*` alongside `int` — verify struct has both fields
- [ ] **Tweens:** Target validity check before write — verify tween advance reads target proxy's `valid` flag
- [ ] **Bindings split:** `bindings_internal.hpp` exists and is included by all split files — verify no `static` helpers copied between TUs
- [ ] **Bindings split:** `g_currentBindings` or equivalent global is in exactly ONE TU — verify via `nm` output before committing
- [ ] **Bindings split:** All `bindings_*.cpp` files add to the same CMake target — verify CMakeLists.txt lists all split files
- [ ] **Save/load:** Key names ≤ 15 chars enforced — verify LuaStore truncates or rejects longer keys
- [ ] **Save/load:** No binary `fwrite` of structs in any shared code path — grep for `fwrite` usage
- [ ] **Camera follow:** `engine.camera.follow()` binding checks ObjectProxy validity before reading position — verify null guard in C function
- [ ] **Camera follow:** `lookAt()` used (not `setPosition()`) in follow implementation — verify lerp and shake are preserved
- [ ] **UI state:** `resetUIState()` called from `registerAll()` — verify UI bars are cleared on F5 hot reload
- [ ] **UI callbacks:** `luaL_unref` called for all active UI `callbackRef` values in `resetUIState()` — verify unref before zeroing
- [ ] **Null safety:** All null-guarded numeric properties return `0` not `nil` — grep for guards that push nil for numeric accessors
- [ ] **Null safety:** ctests pass after each null guard addition — verify test run as part of null safety phase process

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Debug draw on wrong layer/canvas | LOW | Define dedicated debug canvas; update `engine.debug.*` binding to route there; add auto-clear in frame start |
| Coroutine raw `lua_State*` dangling after reload | MEDIUM | Add `int threadRef` to slot struct; replace all `lua_State*` slot fields; add `clearCoroutines()` call before `lua_close()` |
| Tween stale `callbackRef` after reload | LOW | Same pattern as C_Timer: add `lua_State*` to tween slot; add `clearTweens()` to hot-reload path |
| bindings.cpp split breaks compilation | MEDIUM | Identify undefined symbols via linker output; move each to `bindings_internal.hpp` as `extern`; remove `static` keyword from affected helpers |
| Binary save format non-portable on ESP32 | HIGH | Rewrite serializer in JSON (extend LuaStore); migrate existing save files or version-gate with magic number |
| Camera follow crashes on destroyed target | LOW | Add `proxy.valid` check in `engine.camera.follow()` binding; return 0 silently if stale |
| UI state survives hot reload | LOW | Add `resetUIState()` called from `registerAll()`; implement `luaL_unref` loop for callback refs |
| Null guard returns nil for numeric property, breaks script arithmetic | LOW | Change guard to return `0`; rerun ctests; check affected scripts |
| Persistent object store data lost on script reload | MEDIUM | Define persistent script lifecycle: scripts not reloaded on scene transition; add `on_scene_change` callback hook |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Debug draw on wrong canvas/layer | Debug draw bindings phase | Debug box visible at correct position after `clear()` + `draw()` cycle; not present next frame after engine clears debug layer |
| Coroutine thread dangling after reload | Coroutine/async phase | F5 reload with active coroutine: no crash; new script starts with clean coroutine state |
| Coroutine yield across pcall boundary | Coroutine/async phase | `coroutine.resume(co)` from Lua `update()` succeeds on all three platforms (SDL3, WASM, ESP32) |
| Tween stale callback after reload | Tween helpers phase | F5 reload with active tween: no wrong-callback fire; tween state clean in new session |
| Tween writes to destroyed proxy | Tween helpers phase | Destroy tween target mid-tween: no crash; tween silently cancelled |
| bindings.cpp split breaks compilation | Bindings refactoring phase | Full clean build after split; `nm` output shows no duplicated symbols |
| Binary save non-portable | Save/load serialization phase | Save on SDL3, load on ESP32 (or vice versa): data round-trips correctly |
| Camera follow crashes on stale proxy | Camera follow phase | `engine.scene.destroy(target)` while follow active: no crash; camera stops following |
| UI state survives hot reload | UI component bindings phase | F5 reload with active UI bars: bars absent after reload without script re-creating them |
| Null guard returns nil for numeric | Null safety phase | All 27+ ctests pass; `scripts/arkanoid.lua` runs without arithmetic errors |
| Persistent object store data lost | Persistent objects phase | Scene transition with persistent object: script's in-Lua state preserved; `engine.store` data accessible |

---

## Sources

- Codebase analysis: `include/enjin2/scripting/bindings.hpp` — `LuaBindings` member layout, `LuaStore` fixed-capacity arrays, `resetSpritePool()` pattern, `registerAll()` as hot-reload entrypoint (2026-03-01)
- Codebase analysis: `src/scripting/bindings.cpp` — `g_currentBindings` static global, `static int lua_proxy_*` forward declarations, 1390-line monolith (2026-03-01)
- Codebase analysis: `include/enjin2/components/camera.hpp` — `C_Camera::lookAt()` vs `setPosition()` semantics, `getScreenOffset()`, `m_shakeOffset` additive (2026-03-01)
- Codebase analysis: `include/enjin2/components/timer.hpp` — `clearTimers()` teardown pattern, `lua_State*` + `int callbackRef` slot design (2026-03-01)
- Codebase analysis: `include/enjin2/scripting/lua_event_bus.hpp` — `clearHandlers()` pattern, `int threadRef` approach for Lua object lifetime (2026-03-01)
- Codebase analysis: `include/enjin2/core/scene.hpp` — `Scene::renderObjects()` post-sort rendering, no post-render hook (2026-03-01)
- Codebase analysis: `include/enjin2/components/drawable.hpp` — `m_screenSpace` flag, `drawWithOffset()` camera-offset routing (2026-03-01)
- Codebase analysis: `src/scripting/bindings_draw.cpp` — `REQUIRE_CANVAS` macro, null-guard pattern for canvas access (2026-03-01)
- Project context: `PROJECT.md` — zero-alloc constraint, multi-platform (ESP32/WASM/SDL3), 1390-line bindings.cpp documented as debt, hot-reload full-state-destroy semantics, `LuaStore` existing fixed-capacity implementation (2026-03-01)
- Lua 5.1 reference: coroutine yield semantics; `coroutine.yield()` cannot cross C call boundary (`lua_pcall` scope); `lua_State*` thread validity coupled to parent state lifecycle
- LuaJIT documentation: CoCo coroutine continuations (platform-dependent); `lua_yieldk` not available in 5.1 compatibility mode; yield behavior on WASM/Emscripten requires explicit verification
- ESP32 NVS documentation: 15-char key limit; ~4 KB per blob key; 100K write cycle limit per flash sector; total NVS partition typically 16 KB (Espressif NVS API reference, verified against ESP-IDF docs)

---
*Pitfalls research for: Adding debug draw, save/load serialization, persistent objects, camera follow helpers, coroutines/async, tweens, UI components, bindings refactoring, and null safety to enjin2 v1.7*
*Researched: 2026-03-01*
