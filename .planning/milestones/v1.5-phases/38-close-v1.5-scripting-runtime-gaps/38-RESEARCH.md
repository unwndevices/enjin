# Phase 38: Close v1.5 Scripting Runtime Gaps - Research

**Researched:** 2026-02-27
**Domain:** C++ / Lua runtime integration — wiring fixes, documentation debt, SDL runner gaps
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ENG-01 | Lua scripts access `engine.scene.switch(id)` to request scene transitions | Registry-order fix: update `enjin_ssm` at call time via pointer-to-pointer or deferred re-registration after `setSceneStateMachine()` |
| ENG-02 | Lua scripts access `engine.scene.find(name)` to locate named objects (returns proxy or nil) | Same fix as ENG-01 — `enjin_active_scene` snapshotted as nullptr; needs live pointer access |
| PROXY-01 | Every Lua callback receives `self` as the first argument: `init(self)`, `update(self, dt)`, `draw(self)` | `loadScriptFile()` calls `callScriptFunctionSafe(INIT_FUNCTION)` with 0 args; must be replaced with `callWithProxy(INIT_FUNCTION, 0.0f, false)` |
</phase_requirements>

---

## Summary

Phase 38 is a gap-closure phase — not a new-feature phase. All three requirements (ENG-01, ENG-02, PROXY-01) involve specific, localised bugs in existing code paths. The code for every binding function is correct; what is broken is the wiring between C++ host code and the Lua registry at initialization time.

The root cause of ENG-01 and ENG-02 is that `LuaBindings::registerAll()` snapshots `m_ssm` and `m_activeScene` as lightuserdata into the Lua registry. Both are `nullptr` at `registerAll()` time in every host (SDL runner calls `initialize()` before scene construction; C_LuaScript never calls `setSceneStateMachine()` at all). The existing `EngineTimeState` pointer works correctly because it stores a pointer to a stable `LuaBindings` member (`m_timeState`), not a pointer that changes after construction. The fix is to apply the same pattern: store a pointer to the member pointers so the closures always see the current value. The PROXY-01 fix is a single-line change in `loadScriptFile()`.

Beyond the three requirement bugs, the phase closes two further categories: (1) the SDL runner never calls `C_LuaScript::setInput()`, so `on_button_pressed`/`on_button_released` never fire in production — this is wiring-only tech debt; (2) two REQUIREMENTS.md checkboxes (DT-01, DT-02) remain unchecked despite full implementation, and Phase 35 has no VERIFICATION.md. These are documentation-only items.

**Primary recommendation:** Fix the three code bugs first (ENG-01/02 registry order, PROXY-01 loadScriptFile), then wire setInput() in the SDL runner, then close the documentation debt. Structure as two or three plans matching this priority order. All ctests must remain green after each plan.

---

## Standard Stack

### Core
| Component | Location | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| Lua registry (`lua_pushlightuserdata` + `lua_setfield` / `lua_getfield`) | `src/scripting/bindings.cpp`, `bindings_engine.cpp` | Store host C++ pointers for retrieval in closures | Established pattern; `enjin_time` already uses this correctly |
| `LuaBindings::registerAll()` | `src/scripting/bindings.cpp:371` | One-shot registration of all bindings and registry keys | Called after every `initialize()` including hot-reload |
| `C_LuaScript::callWithProxy()` | `src/components/lua_script.cpp:288` | Push proxy as first arg, call Lua function via `lua_pcall` | All lifecycle callbacks (`update`, `draw`) already use this |
| `C_LuaScript::callScriptFunctionSafe()` | `src/components/lua_script.cpp:265` | Call Lua function with 0 args | Only remaining user is `loadScriptFile()` init call |
| `C_LuaScript::setInput()` | `src/components/lua_script.cpp:211` | Wire `InputState*` to `LuaBindings` | Necessary for `dispatchInputCallbacks()` to fire in SDL runner |

### SDL Runner Integration
| Component | Location | Purpose | Current Status |
|-----------|----------|---------|----------------|
| `performReload()` | `src/platform/sdl/sdl_main.cpp:107` | Full Lua teardown + reload; calls `setLayers()` and `setInput()` | Already calls `setInput()` on `g_lua` |
| Per-frame `g_lua.getBindings().setInput(&g_input)` | `sdl_main.cpp:269` | Re-wires input each frame | Already present for `engine.input.*` polling |

Note: The SDL runner (`sdl_main.cpp`) operates as a standalone Lua host — it has no `SceneStateMachine`, no named `Scene` objects, and no `C_LuaScript` components. The `engine.scene.switch()` and `engine.scene.find()` ENG-01/02 fixes are needed for the **C_LuaScript component path** (which is the primary game-logic path), not the SDL runner. The SDL runner uses raw `lua_pcall` for `update` and `draw` without a proxy, by design.

---

## Architecture Patterns

### Pattern 1: The Registration-Order Bug (ENG-01 / ENG-02)

**What:** `registerAll()` snapshots `m_ssm` and `m_activeScene` as lightuserdata into the Lua registry. These are `nullptr` at `registerAll()` time. Later calls to `setSceneStateMachine(ssm)` and `setActiveScene(scene)` update the C++ members but NOT the Lua registry entries. Closures always read `nullptr` from the registry.

**Root cause (confirmed by code inspection):**

```cpp
// bindings.cpp:387 — BROKEN: snapshots nullptr at registerAll() time
lua_pushlightuserdata(L, m_ssm);          // m_ssm is nullptr here
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
lua_pushlightuserdata(L, m_activeScene);  // m_activeScene is nullptr here
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");

// bindings.cpp:391 — CORRECT: stores stable member address
lua_pushlightuserdata(L, &m_timeState);   // always valid (member of LuaBindings)
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_time");
```

**Fix: apply pointer-to-pointer pattern (same as `enjin_time`):**

```cpp
// In registerAll() — store address of m_ssm and m_activeScene members:
lua_pushlightuserdata(L, &m_ssm);          // pointer to the pointer member
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
lua_pushlightuserdata(L, &m_activeScene);  // pointer to the pointer member
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");

// In lua_engine_scene_switch() — dereference the double pointer:
lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
auto** ssmPtr = static_cast<SceneStateMachine**>(lua_touserdata(L, -1));
lua_pop(L, 1);
if (!ssmPtr || !*ssmPtr) return 0;
(*ssmPtr)->switchTo(id);

// In lua_engine_scene_find() — dereference the double pointer:
lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
auto** scenePtr = static_cast<Scene**>(lua_touserdata(L, -1));
lua_pop(L, 1);
if (!scenePtr || !*scenePtr) { lua_pushnil(L); return 1; }
Scene* scene = *scenePtr;
```

This pattern is correct because `m_ssm` and `m_activeScene` are non-static members of `LuaBindings`. Their addresses are stable for the lifetime of the `LuaBindings` object (which is the same lifetime as the Lua state). `setSceneStateMachine()` and `setActiveScene()` update the value at those stable addresses, so closures reading via the pointer-to-pointer will always see the current value.

**Confidence: HIGH** — Verified by code inspection. `EngineTimeState` uses this same pattern correctly.

### Pattern 2: loadScriptFile() init Call (PROXY-01)

**What:** `loadScriptFile()` (file load path) calls `callScriptFunctionSafe(INIT_FUNCTION)` which pushes 0 arguments. The `init(self)` function receives `nil` as `self`. The `loadScript()` (string load path) calls `executeScript()` which calls `callWithProxy(INIT_FUNCTION, 0.0f, false)` — correct.

**Root cause (confirmed by code inspection):**

```cpp
// lua_script.cpp:93 — BROKEN loadScriptFile() path:
callScriptFunctionSafe(INIT_FUNCTION);  // 0 args — self is nil

// lua_script.cpp:167 — CORRECT executeScript() path:
callWithProxy(INIT_FUNCTION, 0.0f, false);  // proxy pushed as first arg
```

**Fix: one-line change in `loadScriptFile()`:**

```cpp
// lua_script.cpp:93 — replace:
callScriptFunctionSafe(INIT_FUNCTION);
// with:
callWithProxy(INIT_FUNCTION, 0.0f, false);
```

Note: The proxy must already be stored in the registry before `callWithProxy()` is invoked. In `loadScriptFile()`, `scriptSystem->loadScript(filename)` runs the script chunk (which defines functions but does not call them). The proxy creation happens in `executeScript()`. Since `loadScriptFile()` uses a different code path than `executeScript()`, the proxy registration block from `executeScript()` (lines 131-163) must be duplicated or factored out before calling `callWithProxy()`.

**The full fix for `loadScriptFile()` requires:**
1. After `hasScript = true; scriptError = false;` (line 89), run the proxy creation block (same as `executeScript()` lines 131-163).
2. Replace `callScriptFunctionSafe(INIT_FUNCTION)` with `callWithProxy(INIT_FUNCTION, 0.0f, false)`.

**Confidence: HIGH** — Verified by direct code inspection.

### Pattern 3: SDL Runner setInput() Wiring for Production Input Callbacks

**What:** `C_LuaScript::dispatchInputCallbacks()` reads input from `scriptSystem->getBindings().getInput()`. `C_LuaScript::update()` calls `dispatchInputCallbacks()`. For this to work in production, the host must call `component->setInput(&input)` each frame before `update()`.

The SDL runner (`sdl_main.cpp`) does NOT instantiate `C_LuaScript` components — it directly calls `lua_pcall` for `update`/`draw`. The SDL runner uses `g_lua.getBindings().setInput(&g_input)` which wires input to the global `LuaBindings`, making `engine.input.*` polling work. But the `on_button_pressed`/`on_button_released` callbacks in the SDL runner path are dispatched by `dispatchInputCallbacks()` in `C_LuaScript::update()` — which is NOT called by the SDL runner.

**The SDL runner is a standalone script host.** It calls `update` and `draw` as plain global Lua functions (no component). The input callbacks (`on_button_pressed`, `on_button_released`) as defined in `C_LuaScript::dispatchInputCallbacks()` are a component-level concept. In the SDL runner, they must be dispatched separately.

**Fix for SDL runner:** Add an explicit input callback dispatch loop in `sdl_main.cpp`, mirroring `dispatchInputCallbacks()`:

```cpp
// After setInput(&g_input), before calling update — dispatch input edge callbacks:
for (int btn = 0; btn < 16; ++btn) {
    if (g_input.justPressed(btn)) {
        lua_State* lua_L = g_lua.getEngine().getState();
        lua_getglobal(lua_L, "on_button_pressed");
        if (lua_isfunction(lua_L, -1)) {
            lua_pushnil(lua_L);   // self = nil (SDL runner has no proxy)
            lua_pushinteger(lua_L, static_cast<lua_Integer>(btn));
            if (lua_pcall(lua_L, 2, 0, 0) != LUA_OK) {
                // handle error
                lua_pop(lua_L, 1);
            }
        } else {
            lua_pop(lua_L, 1);
        }
    }
    // same for justReleased -> on_button_released
}
```

**Confidence: HIGH** — Derived from code inspection of both `dispatchInputCallbacks()` and `sdl_main.cpp`. The SDL runner already polls input correctly; it just lacks the edge-callback dispatch loop.

### Pattern 4: Documentation Debt Closure

**DT-01 and DT-02 checkboxes in REQUIREMENTS.md:**

```markdown
# Current state (REQUIREMENTS.md lines 16-17):
- [x] **DT-01**: ...  ← ALREADY CORRECT per REQUIREMENTS.md line 16
- [x] **DT-02**: ...  ← ALREADY CORRECT per REQUIREMENTS.md line 17
```

Wait — on re-reading REQUIREMENTS.md lines 16-17:
```
- [x] **DT-01**: ...
- [x] **DT-02**: ...
```

Both checkboxes ARE already `[x]` in REQUIREMENTS.md as read. The audit report says they are unchecked (`[ ]`), but the REQUIREMENTS.md file as read shows `[x]`. This requires verification at plan time — if they are already checked, only Phase 35 VERIFICATION.md needs to be written.

**Phase 35 VERIFICATION.md:** Missing. A VERIFICATION.md must be written at `.planning/phases/35-gc-control-component-assertions/35-VERIFICATION.md`. It should document the verified behaviors of GC-01, GC-02, DEP-01, DEP-02, DEP-03 based on code inspection and the gc_assert_test ctest results.

### Anti-Patterns to Avoid

- **Storing pointer value in registry when pointer changes post-init:** The existing bug for ENG-01/02. Always store `&member` (stable address) not `member` (value that changes).
- **Adding a new registerAll() call after setSceneStateMachine():** This would re-register all bindings, losing metatable state and sprite pool reset timing. The pointer-to-pointer fix avoids this.
- **Calling callScriptFunctionSafe() for lifecycle callbacks:** callScriptFunctionSafe() pushes 0 args. Lifecycle callbacks (init, update, draw) require the proxy as first arg — always use callWithProxy().
- **Modifying SDL runner to use C_LuaScript:** The SDL runner is a standalone script host by design. Adding component machinery would violate the zero-dynamic-allocation constraint and the SDL runner's intentional simplicity.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Live pointer access from Lua closures | Custom callback registration mechanism | Pointer-to-pointer pattern (`&m_ssm`) already used by `enjin_time` | One consistent pattern across all engine.* registry entries |
| Input edge detection in SDL runner | New InputState wrapper | Reuse `InputState::justPressed()` / `justReleased()` already called in `dispatchInputCallbacks()` | Same 16-button loop, same API |
| Phase 35 VERIFICATION.md | Rerun all tests | Write document citing gc_assert_test ctest pass | Evidence is already captured in Phase 37 (18 ctests pass including gc_assert_test) |

---

## Common Pitfalls

### Pitfall 1: Proxy Not Created Before callWithProxy() in loadScriptFile()
**What goes wrong:** Calling `callWithProxy(INIT_FUNCTION, 0.0f, false)` in `loadScriptFile()` without first creating the proxy userdata in the Lua registry. `callWithProxy()` retrieves the proxy via `lua_gettable(L, LUA_REGISTRYINDEX)` keyed by `this` — if it was never stored, `lua_gettable` returns nil and `init(self)` still gets nil.
**Why it happens:** `loadScriptFile()` does not go through `executeScript()` which contains the proxy creation block (lines 131-163 of lua_script.cpp).
**How to avoid:** Before calling `callWithProxy()` in `loadScriptFile()`, run the same proxy creation block that `executeScript()` runs. This block already handles the reload case (invalidating old proxy if present).
**Warning signs:** `init(self)` receives nil even after the fix if proxy creation was skipped.

### Pitfall 2: Forgetting the on_button_released Loop in SDL Runner
**What goes wrong:** Adding only `justPressed` dispatch, missing `justReleased`. Tests pass (only pressed is checked) but released callbacks silently never fire.
**Why it happens:** Easy to copy only the pressed half.
**How to avoid:** Mirror `dispatchInputCallbacks()` exactly — both pressed and released, same 16-button range.

### Pitfall 3: Breaking Hot-Reload for ENG-01/02 Fix
**What goes wrong:** `performReload()` calls `lua.shutdown()` then `lua.initialize()` then `bindings.registerAll()`. After reload, a new Lua state exists. The pointer-to-pointer fix stores `&m_ssm` in the NEW state's registry. If `setSceneStateMachine()` was called before the first `registerAll()` but `registerAll()` on reload writes `&m_ssm` correctly, everything works. Verify that `m_ssm` and `m_activeScene` are NOT reset during `shutdown()` / `initialize()` / `registerAll()` — they should be host-injected and persist across reloads.
**Warning signs:** `engine.scene.switch()` works before F5, silent no-op after F5.

### Pitfall 4: Proxy Creation Block Assumes metatable is Registered
**What goes wrong:** In `loadScriptFile()`, if `registerAll()` has not been called yet (unlikely but defensive), `luaL_getmetatable(L, "ScriptProxy")` returns nil and the proxy creation block bails out. Then `callWithProxy()` gets nil from the registry.
**Why it happens:** Out-of-order initialization.
**How to avoid:** `loadScriptFile()` is only called after `initialize()` which calls `registerAll()`. This is not a real risk in normal operation, but the existing nil-check in `executeScript()` (lines 152-153) provides the guard.

### Pitfall 5: DT-01/DT-02 Checkboxes Already Checked
**What goes wrong:** Plan documents work to "check" the DT-01/DT-02 boxes when they are already `[x]`.
**Why it happens:** The audit report says they are `[ ]` but the current REQUIREMENTS.md shows `[x]` on lines 16-17. The audit may have been run against a state where they were not yet updated.
**How to avoid:** At plan time, read REQUIREMENTS.md lines 16-17 verbatim. If already `[x]`, no REQUIREMENTS.md edit is needed. If still `[ ]`, change to `[x]`.

---

## Code Examples

Verified patterns from source inspection:

### Pointer-to-Pointer Registry Pattern (registerAll fix)
```cpp
// Source: src/scripting/bindings.cpp:391 (enjin_time — the working example)
lua_pushlightuserdata(L, &m_timeState);   // stores address of member
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_time");

// Retrieval in closure (bindings_engine.cpp:160):
lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
// ts is &m_timeState — always valid, sees latest value written by setTimeState()
```

Apply same pattern for SSM and activeScene:
```cpp
// In registerAll():
lua_pushlightuserdata(L, &m_ssm);
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
lua_pushlightuserdata(L, &m_activeScene);
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");

// In lua_engine_scene_switch():
lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
auto** ssmPP = static_cast<SceneStateMachine**>(lua_touserdata(L, -1));
lua_pop(L, 1);
if (!ssmPP || !*ssmPP) return 0;
uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
(*ssmPP)->switchTo(id);
return 0;

// In lua_engine_scene_find():
lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
auto** scenePP = static_cast<Scene**>(lua_touserdata(L, -1));
lua_pop(L, 1);
if (!scenePP || !*scenePP) { lua_pushnil(L); return 1; }
Scene* scene = *scenePP;
// ... rest unchanged
```

### loadScriptFile() Proxy Creation + Init Call Fix
```cpp
// Source: src/components/lua_script.cpp (patch area: lines 88-95)

// After:
hasScript = true;
scriptError = false;
errorMessage[0] = '\0';

// ADD: proxy creation block (mirrors executeScript() lines 131-163)
{
    lua_State* L = scriptSystem->getEngine().getState();
    if (L) {
        // Invalidate old proxy if present (handles reload via loadScriptFile)
        lua_pushlightuserdata(L, this);
        lua_gettable(L, LUA_REGISTRYINDEX);
        if (lua_isuserdata(L, -1)) {
            ScriptProxy* oldProxy = static_cast<ScriptProxy*>(lua_touserdata(L, -1));
            if (oldProxy) oldProxy->valid = false;
        }
        lua_pop(L, 1);

        // Create new proxy userdata
        ScriptProxy* proxy = static_cast<ScriptProxy*>(
            lua_newuserdata(L, sizeof(ScriptProxy)));
        proxy->component = this;
        proxy->valid = true;

        luaL_getmetatable(L, "ScriptProxy");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 2);
        } else {
            lua_setmetatable(L, -2);
            lua_pushlightuserdata(L, this);
            lua_insert(L, -2);
            lua_settable(L, LUA_REGISTRYINDEX);
        }
    }
}

// REPLACE:
callScriptFunctionSafe(INIT_FUNCTION);
// WITH:
callWithProxy(INIT_FUNCTION, 0.0f, false);
```

### SDL Runner Input Callback Dispatch
```cpp
// Source: src/platform/sdl/sdl_main.cpp (add after setInput() call, before update lua_pcall)
// Add inside #ifdef ENJIN2_BUILD_LUA block, per-frame:

// Dispatch input edge callbacks (mirrors C_LuaScript::dispatchInputCallbacks)
for (int btn = 0; btn < 16; ++btn) {
    if (g_input.justPressed(btn)) {
        lua_State* lua_L = g_lua.getEngine().getState();
        lua_getglobal(lua_L, "on_button_pressed");
        if (lua_isfunction(lua_L, -1)) {
            lua_pushnil(lua_L);               // self = nil (SDL runner has no proxy)
            lua_pushinteger(lua_L, static_cast<lua_Integer>(btn));
            if (lua_pcall(lua_L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(lua_L, -1);
                std::cerr << "[lua error] on_button_pressed: "
                          << (err ? err : "unknown") << "\n";
                lua_pop(lua_L, 1);
                lua_ok = false;
            }
        } else {
            lua_pop(lua_L, 1);
        }
    }
    if (g_input.justReleased(btn)) {
        lua_State* lua_L = g_lua.getEngine().getState();
        lua_getglobal(lua_L, "on_button_released");
        if (lua_isfunction(lua_L, -1)) {
            lua_pushnil(lua_L);
            lua_pushinteger(lua_L, static_cast<lua_Integer>(btn));
            if (lua_pcall(lua_L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(lua_L, -1);
                std::cerr << "[lua error] on_button_released: "
                          << (err ? err : "unknown") << "\n";
                lua_pop(lua_L, 1);
                lua_ok = false;
            }
        } else {
            lua_pop(lua_L, 1);
        }
    }
}
```

---

## Test Architecture

No new test framework needed — existing ctest infrastructure covers all requirements. All Lua-dependent tests use `ENJIN2_BUILD_LUA` CMake guard and link against `enjin2` + `enjin2_lua`.

### Existing Tests (remain green — must not regress)
| Test | Covers | Run Command |
|------|--------|-------------|
| `engine_table_test` | ENG-01..ENG-06 null-guard paths | `ctest -R engine_table_test` |
| `object_proxy_test` | ENG-02 ObjectProxy nil/stale/enable paths | `ctest -R object_proxy_test` |
| `input_event_callback_test` | INPUT-01/02/03 via C_LuaScript path | `ctest -R input_event_callback_test` |
| All 18 ctests | Full regression | `cd build && ctest --output-on-failure` |

### New Tests Needed for Phase 38

**ENG-01 live wiring (in engine_table_test.cpp or new file):**
```cpp
// Test: engine.scene.switch() reaches SceneStateMachine::switchTo()
// Setup: create a MockSSM that counts switchTo() calls
// Register via bindings.setSceneStateMachine(&mockSSM)
// Execute: "engine.scene.switch(2)"
// Assert: mockSSM.switchCount == 1, mockSSM.lastId == 2
```

**ENG-02 live wiring (in engine_table_test.cpp or new file):**
```cpp
// Test: engine.scene.find("hero") returns valid proxy after setActiveScene
// Setup: create Scene with named Object "hero"; call bindings.setActiveScene(&scene)
// Execute: "local p = engine.scene.find('hero'); found = (p ~= nil) and 1 or 0"
// Assert: found == 1.0
```

**PROXY-01 loadScriptFile path (new test or extended script_proxy_lifetime_test):**
```cpp
// Test: script loaded via loadScriptFile() receives valid self in init(self)
// Setup: write a temp .lua file that sets init_self_valid = (self ~= nil)
// Load via loadScriptFile(); call update()
// Assert: getScriptNumber("init_self_valid") == 1.0
```

### Test Structure Pattern (from existing tests)
```cpp
struct EngineTableFixture {
    LuaEngine engine;
    LuaBindings bindings;

    EngineTableFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }
    // ... helpers
};
```

For ENG-01/02 live tests, extend fixture:
```cpp
// Add to EngineTableFixture or create new fixture:
MockSceneStateMachine mockSSM;
bindings.setSceneStateMachine(&mockSSM);
// registerAll() already ran at construction — new value flows through &m_ssm pointer
// No need to call registerAll() again
```

This is the key verification: `setSceneStateMachine()` called AFTER `registerAll()` must be visible to `lua_engine_scene_switch()` via the pointer-to-pointer pattern. Current test `test_engine_scene_null_guards` only verifies the null path.

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `lua_pushlightuserdata(L, m_ssm)` (value snapshot) | `lua_pushlightuserdata(L, &m_ssm)` (address of member) | Phase 38 | ENG-01/02 live dispatch works post-injection |
| `callScriptFunctionSafe(INIT_FUNCTION)` in loadScriptFile() | `callWithProxy(INIT_FUNCTION, 0.0f, false)` (with proxy pre-creation) | Phase 38 | PROXY-01 satisfied for both load paths |
| SDL runner lacks input callback dispatch | Explicit 16-button loop in sdl_main.cpp | Phase 38 | on_button_pressed/released fire in production |

---

## Phase Plan Structure Recommendation

**Plan 1: Fix ENG-01, ENG-02, PROXY-01 (code bugs)**
- Fix pointer-to-pointer in `registerAll()` (2 lines changed)
- Fix `lua_engine_scene_switch()` and `lua_engine_scene_find()` dereference (4-6 lines changed)
- Fix `loadScriptFile()` proxy creation + init call (20 lines changed)
- Add live-wiring test cases (new tests in engine_table_test or new file)
- Add PROXY-01 file-load test
- Run full ctest suite (18+ tests green)

**Plan 2: SDL runner input callbacks + documentation debt**
- Add input callback dispatch loop to sdl_main.cpp
- Verify DT-01/DT-02 checkboxes in REQUIREMENTS.md (check or confirm already checked)
- Write Phase 35 VERIFICATION.md
- Run full ctest suite

Alternatively, these can be a single plan if scope is light. The code changes across both plans are localized and non-overlapping.

---

## Open Questions

1. **DT-01/DT-02 checkbox state**
   - What we know: REQUIREMENTS.md as read by research shows `[x]` on lines 16-17. Audit report says `[ ]`.
   - What's unclear: Which is the current file state? The audit was run at `2026-02-27T19:30:00Z`, same day as this research.
   - Recommendation: Plan must read REQUIREMENTS.md verbatim before deciding whether to edit. If `[x]`, only Phase 35 VERIFICATION.md is needed.

2. **MockSceneStateMachine for ENG-01 test**
   - What we know: `SceneStateMachine` is in `include/enjin2/core/scene_state_machine.hpp`. Tests can include it directly.
   - What's unclear: Whether SceneStateMachine is easily instantiable in a test without a full Scene. The audit confirms `switchTo()` just calls `pendingSceneId` assignment — no deep dependency.
   - Recommendation: Subclass or wrap SceneStateMachine in a minimal test stub that counts `switchTo()` calls, or use a real `SceneStateMachine` and verify its `pendingSceneId` / `hasPendingTransition` state after the call.

3. **SDL runner `lua_ok = false` on callback error policy**
   - What we know: The SDL runner sets `lua_ok = false` on any `lua_pcall` error, which disables further script calls until F5. This is the existing error-handling policy for `update`/`draw`.
   - What's unclear: Whether the same policy should apply to input callback errors — it may be overly aggressive (one bad callback disabling all scripts).
   - Recommendation: Use the same `lua_ok = false` policy as update/draw for consistency. Input callbacks are optional; if they crash, pausing until F5 is the correct behavior for SDL dev mode.

---

## Sources

### Primary (HIGH confidence)
- Direct code inspection: `src/scripting/bindings.cpp` — `registerAll()` implementation, lines 371-515
- Direct code inspection: `src/scripting/bindings_engine.cpp` — `lua_engine_scene_switch()`, `lua_engine_scene_find()`, lines 77-119
- Direct code inspection: `src/components/lua_script.cpp` — `loadScriptFile()` line 93, `executeScript()` lines 131-167
- Direct code inspection: `src/platform/sdl/sdl_main.cpp` — per-frame Lua dispatch, lines 267-303
- `.planning/v1.5-MILESTONE-AUDIT.md` — authoritative gap descriptions for ENG-01-LIVE, ENG-02-LIVE, PROXY-01, FLOW-SCENE-SWITCH, FLOW-INIT-FILE
- `.planning/REQUIREMENTS.md` — requirement checkbox states
- `tests/CMakeLists.txt` — test registration patterns
- `tests/engine_table_test.cpp` — existing test fixture and assertion patterns
- `tests/input_event_callback_test.cpp` — existing input test fixture

### Secondary (MEDIUM confidence)
- `.planning/STATE.md` accumulated decisions — pointer-to-pointer pattern noted as standard for `EngineTimeState`

---

## Metadata

**Confidence breakdown:**
- ENG-01/ENG-02 fix: HIGH — root cause and fix verified by direct code inspection; same pattern already works for `enjin_time`
- PROXY-01 fix: HIGH — one-line call change with proxy creation block; both paths verified by reading lua_script.cpp
- SDL runner input callbacks: HIGH — `dispatchInputCallbacks()` code readable; SDL runner loop structure readable
- Documentation debt: MEDIUM — DT-01/DT-02 checkbox state ambiguous between audit and current file read

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable codebase — no external dependencies change)
