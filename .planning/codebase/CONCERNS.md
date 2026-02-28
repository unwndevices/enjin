# Codebase Concerns

**Analysis Date:** 2026-02-27

## Tech Debt

### ScriptProxy Validity Check — Dangling Pointer on Scene Destruction

**Issue:** `ScriptProxy` stores a raw `C_LuaScript* component` pointer alongside a simple `bool valid` flag. The `valid` flag is set to false on `C_LuaScript` destructor. However, this design only prevents immediate crashes — it does not address the underlying lifetime issue.

**Files:**
- `include/enjin2/scripting/bindings.hpp` (lines 36-39) — ScriptProxy struct definition with `valid` flag
- `src/scripting/bindings.cpp` (lines 27, 79) — Validity checks in `__index` and `__newindex` metamethods
- `include/enjin2/components/lua_script.hpp` (lines 67-69) — Destructor comment promises proxy invalidation

**Impact:** If a Lua script stores `self` in a table, coroutine upvalue, or callback across a scene transition, that proxy becomes invalid. Accessing it returns `nil` (safe) but represents lost functionality — code written in idiomatic Lua (e.g., `local obj = self` at module scope) will silently break.

**Current Mitigation:** The `valid` flag prevents reads/writes to dead pointers, but does not recover stored proxies or provide error context.

**What Should Happen:**
- Scripts storing `self` should receive a clear Lua error on access after invalidation: `error: object has been destroyed (scene transitioned)` instead of silent `nil` returns.
- Or: Implement a generation counter on `ObjectCollection`/`Scene` so proxies can validate that the component's generation matches the current scene — stale proxies error immediately with context.

**Recommendation:** Add generation token to `ScriptProxy` alongside `valid` flag. Document in Lua API that `self` is valid only within a callback frame, not across scene boundaries.

---

### Zero-Dynamic-Allocation Constraint vs. std::string Usage

**Issue:** Several sources use `std::string` in performance-sensitive or embedded contexts:
- `include/enjin2/components/lua_script.hpp` (lines 41, 42, 45) — `scriptCode`, `scriptPath`, `errorMessage` stored as `std::string`
- `include/enjin2/core/object.hpp` (line 320) — Object name stored as `const char*` (correct) but tag/name APIs use string comparisons

**Files:**
- `include/enjin2/components/lua_script.hpp` (C_LuaScript fields)
- `src/components/lua_script.cpp` (string assignments, concatenations)

**Impact:** On ESP32, `std::string` allocations consume heap memory managed by the global allocator, not the Lua pool. For `scriptCode` in particular, loading a large script creates a heap allocation outside the Lua memory budget. This can cause fragmentation on embedded systems with tight memory constraints (254 KB Lua pool on ESP32).

**Current Mitigation:** SSO (small string optimization) avoids allocation for short strings (<~15 chars), but `errorMessage` and `scriptPath` can exceed SSO threshold.

**What Should Happen:**
- Use fixed-size arrays for error messages: `char errorMessage[256]` instead of `std::string`
- For script code, keep as `std::string` on desktop; consider a memory-view or arena-allocated approach on ESP32 if this becomes a bottleneck

**Recommendation:** Profile heap usage on target ESP32. If fragmentation is observed, convert error messages and paths to fixed-size buffers. Script code can remain as `std::string` for now (loaded once at startup).

---

### Multiple Build Directories Indicate Cleanup Needed

**Issue:** Repository contains 15 leftover build directories from development/testing:
- `build_21_off`, `build_21_on`, `build_22_*`, `build_24_*`, `build_25_*`, `build_off`, `build_sdl_test`, `build_test_*`, `build_wasm`

**Files:** Directories at project root (not tracked in git per `.gitignore`)

**Impact:** Clutters repository, wastes disk space (~200 MB aggregate), can confuse developers about which build is "current" (symlinked `compile_commands.json` points to `build/`, not others).

**Recommendation:** Remove all non-`build/` directories. Keep only the main `build/` directory. Document any platform-specific build procedures in `DESIGN.md` or a separate `BUILD.md`.

---

## Known Bugs

### Scene Self-Transitions Skip Object Re-initialization

**Issue:** When a scene calls `engine.scene.switch(engine.scene.current())` (transition to itself), the `Scene::initialized` flag prevents `Scene::initialize()` from running again on re-entry. Objects added in `onCreate()` are thus not properly initialized.

**Files:**
- `include/enjin2/core/scene.hpp` — Scene::initialize() has early-return guard on `initialized == true`
- `include/enjin2/core/scene_state_machine.hpp` — changeScene() does not detect self-transitions

**Trigger:** Call `engine.scene.switch(current_scene_id)` or execute scene transition during scene's `onUpdate()`

**Workaround:** Manually call `scene->reset()` or reinitialize objects explicitly in `onCreate()` using a flag. No user-facing API currently exists to reset a scene safely.

**Fix Approach:** Detect self-transitions in `SceneStateMachine::changeScene()` and either:
1. Call a `reset()` method that clears `initialized = false`, then proceed with normal transition
2. Defer the transition to the next frame (deferred-transition pattern) instead of executing inline

**Priority:** Medium — self-transitions are uncommon but idiomatic in some game patterns (reset level, reload scene).

---

### clang-tidy Configuration Present But Not Enforced in CI

**Issue:** `.clang-tidy` file exists and configures comprehensive checks (bugprone-*, cppcoreguidelines-*, modernize-*, performance-*), but it is not run as part of the build or CI pipeline.

**Files:**
- `.clang-tidy` — Configuration file at project root
- `CMakeLists.txt` — No clang-tidy integration via `set(CMAKE_CXX_CLANG_TIDY ...)` or custom target

**Impact:** Code quality issues caught by clang-tidy are never reported until developers manually run `clang-tidy -p build src/...`. New code may silently introduce warnings that accumulate.

**Recommendation:** Add a CMake check-target that runs clang-tidy:
```cmake
if(CLANG_TIDY)
    add_custom_target(lint
        COMMAND clang-tidy -p ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src/**/*.cpp
        COMMENT "Running clang-tidy checks..."
    )
endif()
```
Or integrate into CI/CD pipeline to fail builds on new warnings.

---

## Security Considerations

### Lua Bindings Expose Raw Object Pointers Without Validation

**Issue:** Several bindings return `Object*` as lightuserdata without lifetime checks:
- `engine.scene.find(name)` returns `lightuserdata` (Object pointer) per `bindings_engine.cpp:100`
- These pointers are dereferenceable immediately but can become dangling after a scene transition

**Files:**
- `src/scripting/bindings_engine.cpp` (lines 91-103) — `lua_engine_scene_find()` pushes Object* as lightuserdata

**Risk:** Lua scripts can call `engine.scene.find("enemy")`, receive a pointer, store it, then on scene transition dereference it leading to use-after-free in Lua-called C++ code.

**Current Mitigation:** Phase 32 design intended to wrap lightuserdata in ScriptProxy with metatable validation, but the comment at line 92 indicates this is "planned" — verify Phase 32 completion status.

**Verification Needed:** Confirm Phase 32 (scriptproxy-userdata) is fully implemented and lightuserdata->ScriptProxy migration is complete. If not, this is HIGH-risk UB waiting to happen.

---

## Performance Bottlenecks

### Linear Name Lookup in ObjectCollection

**Issue:** `ObjectCollection::findByName(const char* name)` performs O(n) linear scan across all objects every call. If Lua scripts repeatedly call `engine.scene.find("player")` in `update()`, this is O(128) strcmp operations per frame.

**Files:**
- `src/core/object_collection.cpp` or `include/enjin2/core/object_collection.hpp` — No indexed name storage

**Impact:** Measurable on ESP32 at 128 objects * 60 Hz: ~7680 strcmp calls/sec. Desktop is unaffected, but embedded frame budgets are tight.

**Prevention:** Add a parallel `const char* nameIndex[MAX_OBJECTS]` lookup table or fixed-size hash map (open-addressed, no std::unordered_map) to enable O(1) name lookup.

**Recommendation:** Document in Lua: cache the result `local player = engine.scene.find("player")` at `init()` time, reuse in `update()`. Add a simple name-indexed lookup on the C++ side if profiling shows frame-time impact.

---

### GC Full Collection Mid-Frame Risk

**Issue:** `engine.lua.collect()` binding calls `lua_gc(L, LUA_GCSTEP, n)` which is safe (incremental). However, if script calls it with a large step count or if a script mistakenly calls an internal `engine.lua.fullCollect()` (if exposed in future), this could spike frame time by 1–5 ms on ESP32 at 254 KB pool size.

**Files:**
- `src/scripting/bindings_engine.cpp` — `lua_engine_lua_collect` implementation

**Impact:** At 30 Hz (33 ms budget), a 5 ms GC spike is 15% of frame budget, visible as frame drops.

**Current Mitigation:** Only `LUA_GCSTEP` is exposed (safe), documentation recommends calling only on scene transitions.

**Recommendation:** Document clearly that `engine.lua.collect()` should not be called from `update()` or `draw()`. Consider adding a release-mode check that warns if called mid-frame (compare frame counter before/after).

---

## Fragile Areas

### Lua 5.1 GC Interaction with Bump Allocator

**Issue:** LuaJIT's GC threshold logic is calibrated for malloc/free patterns. The enjin2 bump allocator does not call the system allocator — Lua's GC tracking via `lua_gc(LUA_GCCOUNT)` may not trigger at expected times.

**Files:**
- `include/enjin2/scripting/lua_engine.hpp` — Custom memory pool and allocator
- `include/enjin2/scripting/lua_platform.hpp` (line 42) — `tuneGarbageCollector()` called during `initialize()`

**Why Fragile:** If the bump allocator is changed (e.g., to a pool-based scheme), GC triggering behavior may shift silently. Scripts may accumulate dead objects longer than expected before collection, eventually hitting the pool limit and failing allocations.

**Safe Modification:**
1. Add explicit GC step calls at deterministic points: after script reload, on scene transition
2. Test GC behavior by logging `lua_gc(LUA_GCCOUNT)` before/after heavy allocation phases
3. On ESP32, use a debug flag to dump GC stats periodically

**Test Coverage:** `gc_assert_test.cpp` verifies `collect()` and `memory()` APIs but not the automatic GC threshold interaction.

---

### ScriptErrorPolicy State Machine — Two-Level Error Handling Without Explicit Protocol

**Issue:** There are now two error systems: the global `lua_ok` gate in (presumably) the host runner, and the per-component `scriptError` flag in `C_LuaScript` with `ScriptErrorPolicy` (Disable/Log/Panic). The interaction between these two is not formally documented.

**Files:**
- `include/enjin2/components/lua_script.hpp` (lines 23-27, 44) — ScriptErrorPolicy enum and scriptError flag
- Implicit assumption: host runner sets `lua_ok = false` on error, blocking all subsequent updates

**Risk:** If a component uses `ScriptErrorPolicy::Disable` (swallow error locally, disable only that component) but the runner's `lua_ok` is also set to false, all OTHER components are also blocked — over-broad failure.

**Safe Modification:**
1. Document the protocol: `ScriptErrorPolicy::Disable` affects ONLY the component's `enabled` flag, not the global runner gate
2. Global `lua_ok` is set false only by catastrophic failures (Lua state init, Panic policy on a component)
3. F5 hot-reload clears both `lua_ok` and all component `scriptError` flags atomically

**Current State:** Phase 33 (scripterrorpolicy) completed; verify implementation matches design intent.

---

### Float dt Precision Loss in Long-Running Sessions

**Issue:** `dt` (delta time) is stored and computed as `float` throughout the engine. Lua's `lua_Number` is `double` by default. When dt is passed from C++ to Lua, it is converted `static_cast<float>(lua_tonumber(...))` or similar.

**Files:**
- `include/enjin2/core/object.hpp` (line 83) — `update(float dt)` signature
- `src/scripting/lua_engine.cpp` or bindings — Float→Lua conversion points

**Impact:** At 60 Hz for 1 hour = 216,000 frames. Accumulating float-precision dt over this many frames leads to visible drift in animation or timing. Most games don't run that long, but a 24/7 kiosk or game jam game might.

**Mitigation:** Document that `dt` in Lua is float-precision, not double. Avoid accumulating dt in Lua scripts over thousands of frames. Use integer frame counts instead.

**Prevention:** No code change needed — this is a documentation issue.

---

## Scaling Limits

### Object Component Limit — MAX_COMPONENTS = 16

**Issue:** `Object` hardcodes `MAX_COMPONENTS = 16` as a static const. Adding >16 components silently fails (returns nullptr).

**Files:**
- `include/enjin2/core/object.hpp` (line 39) — `static constexpr size_t MAX_COMPONENTS = 16`
- `include/enjin2/core/object.hpp` (line 108) — Early-exit if `componentCount >= MAX_COMPONENTS`

**Scaling Limit:** 16 components per object. A complex character with Position, Sprite, Animation, Collider, AI, Audio, ParticleEmitter, etc. can approach this limit. A 17th component addition silently fails.

**Current Capacity:** 16 is reasonable for most game objects but not future-proof for complex entities.

**Scaling Path:** Increase `MAX_COMPONENTS` to 32 or implement dynamic allocation (violates zero-alloc constraint). Trade-off: memory per object vs. flexibility.

**Recommendation:** Document the limit. If exceeded, add a hard assertion (`assert(componentCount < MAX_COMPONENTS)` in debug) instead of silent failure. Monitor real projects to see if this becomes a bottleneck.

---

### ObjectCollection Capacity — Implicit from MAX_OBJECTS

**Issue:** `ObjectCollection` has an implicit maximum object count (likely 128 per enjin1 design, but not explicit in enjin2 code inspected). Adding >128 objects silently fails.

**Files:** Unknown — examine `ObjectCollection::addObject()` implementation

**Scaling Limit:** If max is 128, a dense game world (many small entities) or multiplayer level hits this ceiling.

**Recommendation:** Define `MAX_OBJECTS` as an explicit constant in `ObjectCollection`, document it, and provide an assertion on overflow.

---

## Dependencies at Risk

### LuaJIT vs. Lua 5.1 API — Long-Term Compatibility

**Issue:** The codebase uses Lua 5.1 API (`lua.h`, `lauxlib.h` from embedded `luajit/src/`), which is stable but no longer actively maintained (Lua 5.4 is current).

**Files:**
- `luajit/` — LuaJIT fork embedded in repository
- Various `.cpp` files include `lua.h`, `lauxlib.h`

**Risk:**
- LuaJIT development is sporadic; any critical security bug fix must be cherry-picked manually
- Future C++ compilers may drop support for Lua 5.1 C++ binding patterns (unlikely but possible)
- LuaJIT bytecode is not stable across versions — serialization/deserialization is fragile

**Mitigation:** LuaJIT is embedded, so no external dependency risk. However, if abandoning LuaJIT, migrating to Lua 5.4 would require rewriting all bindings.

**Recommendation:** Monitor LuaJIT releases. For ESP32 embedded use, LuaJIT is the best choice (JIT not available, but bytecode is compact). Document this decision.

---

## Missing Critical Features

### Scene Transition Deferred vs. Immediate — No Documented Semantics

**Issue:** The `SceneStateMachine::changeScene()` method executes immediately during a callback. If a scene calls `changeScene()` during `onUpdate()`, the transition happens inline, potentially during a complex callback chain. This is not documented.

**Files:**
- `include/enjin2/core/scene_state_machine.hpp` — `changeScene()` signature and semantics

**Missing Feature:** No "deferred transition" mechanism. If deferred transitions are needed (avoid re-entrancy), there is no API for it.

**Recommendation:** Document current behavior: "Transition executes immediately; use deferred calling patterns if re-entrancy is a concern." If deferred transitions are a priority, implement as a separate phase.

---

### named-objects / tags API — No Lua Bindings Yet

**Issue:** Phase 29 (named-objects-tags) completed on C++ side: `Object::setName()`, `Object::getName()`, `Object::addTag()`, `Object::hasTag()`. But Lua API for adding/querying tags is not implemented.

**Files:**
- `include/enjin2/core/object.hpp` (lines 257-300) — Name and tag methods
- `src/scripting/bindings.cpp` (line 59-67) — `self.name` read-only in Lua

**Impact:** Lua scripts cannot set names or tags at runtime. Game logic that relies on dynamic tagging (e.g., "mark enemy as dead") cannot use the tag system from Lua.

**Recommendation:** Expose in Lua via `self:addTag(tag)`, `self:hasTag(tag)`, `self:clearTags()` as metamethods on ScriptProxy.

---

## Test Coverage Gaps

### ScriptProxy Validity After Scene Destruction — Not Tested

**Issue:** Phase 32 (scriptproxy-userdata) completed and `valid` flag was added. But there is no test that stores `self` in a Lua table, transitions scenes, then accesses the stored proxy to verify it returns `nil` instead of crashing.

**Files:**
- `tests/` — No test file for proxy lifetime management
- `src/scripting/bindings.cpp` — Validity check exists but not exercised by any test

**Risk:** Silent regression if validity check is accidentally removed or if `C_LuaScript::~destructor()` forgets to set proxy->valid = false.

**Recommendation:** Add a test in `input_event_callback_test.cpp` or a new `script_proxy_lifetime_test.cpp`:
```lua
-- Store self in global table
stored_self = self
-- (Later, on scene transition, access it)
-- Expect nil or error, not crash
```

---

### GC Control — Memory() Precision on Embedded Targets

**Issue:** `engine.lua.memory()` uses formula `lua_gc(LUA_GCCOUNT) * 1024 + lua_gc(LUA_GCCOUNTB)` to compute Lua heap size. This formula is only accurate if the Lua allocator is a standard allocator with block-based accounting.

**Files:**
- `src/scripting/bindings_engine.cpp` — `lua_engine_lua_memory()` implementation
- `tests/gc_assert_test.cpp` (lines 1-154) — Tests GC-02 but only verifies "return is non-negative number"

**Risk:** On embedded targets with custom bump allocator, the returned byte count may not reflect actual memory usage patterns. Scripts tuning GC based on `engine.lua.memory()` may be misled.

**Recommendation:** Add a comment documenting the formula. On ESP32, verify the value matches actual pool usage by comparing with `LuaPlatform::getMemoryUsage()` or similar.

---

### Input Event Callback Ordering — Frame Boundary Not Verified

**Issue:** Phase 34 implemented `on_button_pressed` / `on_button_released` callbacks and documents they fire "before update()". But there is no test that verifies they fire in the same frame (before a frame count increment) vs. the next frame.

**Files:**
- `tests/input_event_callback_test.cpp` (lines 1-200+) — Tests button callbacks but may not verify frame-boundary semantics
- `src/components/lua_script.cpp` — `dispatchInputCallbacks()` called from `update()`

**Risk:** If a future refactoring moves the callback dispatch, this regression could pass unnoticed until a game behaves incorrectly.

**Recommendation:** Add a test that calls `engine.time.frameCount()` inside `on_button_pressed` and compares with a baseline frame count to verify they are in the same frame.

---

### assertRequires<T>() Release Path — Only DEP-03 Tested

**Issue:** `assertRequires<T>()` has debug and release behavior. Debug: `assert(false)` with a message. Release: `printf()` and `setEnabled(false)`. The release path (DEP-03) is only tested in a `#ifdef NDEBUG` gated test, which only runs on release builds.

**Files:**
- `include/enjin2/core/component.hpp` (lines 41-54) — `assertRequires<T>()` with debug/release ifdef
- `tests/gc_assert_test.cpp` (lines 1-200+) — DEP-03 gated behind `#ifdef NDEBUG`

**Risk:** If release-mode fallback is broken (wrong printf format, wrong component, setEnabled() not called), the test never catches it on debug builds.

**Recommendation:** Create a separate release-mode only test (or add a conditional compilation step to CI) that verifies missing dependencies disable components without aborting.

---

## "Looks Done But Isn't" Checklist

- [ ] **ScriptProxy validity:** All stored proxies (in Lua tables, coroutines) raise Lua error on access after scene transition — NOT just returning nil silently
- [ ] **Phase 32 completion:** Verify lightuserdata from `engine.scene.find()` has been fully upgraded to ScriptProxy with metatable — comment at line 92 of bindings_engine.cpp says "Phase 32 upgrades" but implementation status unclear
- [ ] **Scene self-transition:** Calling `engine.scene.switch(current_id)` fully re-initializes scene (not skipping onCreate) — test by counting objects after self-transition
- [ ] **GC safety on ESP32:** No calls to `lua_gc(L, LUA_GCCOLLECT)` (full collection) exist in hot-path code — verify all engine code uses LUA_GCSTEP
- [ ] **clang-tidy enforcement:** CI build fails if clang-tidy reports new errors in changed files — currently no CI integration
- [ ] **Component limit assertion:** Adding >16 components logs a clear error (not silent nullptr) — currently fails silently
- [ ] **Error policy coordination:** Single component error (Disable policy) does not block sibling component updates — verify in error_policy_test.cpp
- [ ] **Input callback frame timing:** on_button_pressed fires in same frame as button press, not next frame — verify frame count in test
- [ ] **Zero-alloc integrity:** All Object/Component operations allocate zero heap memory (except std::string fields in C_LuaScript) — audit on ESP32 with malloc instrumentation
- [ ] **Build cleanup:** Only one active `build/` directory exists; build_* dirs removed — currently 15 leftover directories

---

*Codebase concerns audit: 2026-02-27*
