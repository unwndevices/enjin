# Phase 34: Input Event Callbacks - Research

**Researched:** 2026-02-27
**Domain:** Lua callback dispatch from C++ InputState edge detection
**Confidence:** HIGH — all findings verified from project source code

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- All active C_LuaScript components in the scene receive input callbacks — `visible = false` does NOT suppress input events (matches how `update()` works)
- Only active Objects (enabled) receive callbacks; disabled Objects are skipped, consistent with existing lifecycle
- `LuaScriptSystem` (bindings_system.cpp) owns the dispatch loop — it iterates all registered scripts, checks input edge state, and fires input callbacks before calling `update()` on each script, satisfying INPUT-03's ordering requirement
- Per-button iteration: `on_button_pressed(self, btn)` fires once per button edge per active script — if 3 buttons pressed in one frame, each script's callback fires 3 times with the respective `btn` value (mirrors LÖVE2D's `keypressed(k)` model)
- `btn` argument is an integer (matching existing `isButtonHeld(btn)` polling API)
- Callback signature: `self` (ScriptProxy userdata) as first arg, `btn` (integer) as second — matching `on_button_pressed(self, btn)`
- Both callbacks are optional — a script without them defined silently skips (all lifecycle callbacks are optional per design doc)
- Error handling follows `ScriptErrorPolicy` on `C_LuaScript` (same policy as `update`/`draw`)

### Claude's Discretion

- Exact method of iterating buttons (loop over enum range vs. cached edge list)
- How LuaScriptSystem receives or queries input state (direct `InputState*` ref vs. querying through engine bindings)
- Whether the button iteration happens inside `LuaScriptSystem::update()` or in a dedicated `dispatchInputEvents()` method

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INPUT-01 | Lua scripts can define `on_button_pressed(btn)` callback, fired on button press edge | `InputState::justPressed(btn)` already implements the edge test; `callWithProxy()` pattern from `lua_script.cpp` already dispatches with self+arg; just need `callWithProxy` variant that pushes an integer second arg |
| INPUT-02 | Lua scripts can define `on_button_released(btn)` callback, fired on button release edge | Same as INPUT-01 but using `InputState::justReleased(btn)` |
| INPUT-03 | Input event callbacks fire after input polling, before `update()` each frame | `C_LuaScript::update()` is the natural insertion point — fire callbacks at top of `update()` before the existing `callWithProxy(UPDATE_FUNCTION, ...)` call |
</phase_requirements>

---

## Summary

Phase 34 adds `on_button_pressed(self, btn)` and `on_button_released(self, btn)` Lua callbacks to `C_LuaScript`. The entire implementation lives within the Lua scripting subsystem — no new C++ types are needed. The phase is purely additive.

The critical infrastructure is already complete: `InputState` (Phase 22/31) has `justPressed(btn)` and `justReleased(btn)` edge-detection methods; `callWithProxy()` (Phase 32/33) already dispatches Lua callbacks with `self` as the first argument; and `ScriptErrorPolicy` (Phase 33) already governs error behavior. This phase wires them together with a button iteration loop inside `C_LuaScript::update()`.

The ordering guarantee in INPUT-03 is satisfied naturally: the input callbacks fire at the top of `C_LuaScript::update()`, before the existing `callWithProxy(UPDATE_FUNCTION, ...)` call. Since `update()` is called after input polling each frame (this is the host's responsibility, already established by Phase 31/22 plumbing), the "after polling, before update" ordering falls out for free.

**Primary recommendation:** Add a private `dispatchInputCallbacks(const InputState& input)` method to `C_LuaScript`, called at the top of `update()`, that loops over all 16 possible button indices and calls `callWithProxy("on_button_pressed", btn)` / `callWithProxy("on_button_released", btn)` for each edge. The `callWithProxy()` function must be extended slightly to support pushing an integer second argument (currently it only handles float `dt`).

---

## Standard Stack

### Core

| Component | Version/Location | Purpose | Why Standard |
|-----------|-----------------|---------|--------------|
| `InputState` | `include/enjin2/input/input_state.hpp` | Holds `buttons`/`prev_buttons` bitmasks; `justPressed(btn)`, `justReleased(btn)` inline methods | Already used by engine.input.* Lua bindings (Phase 31) |
| `C_LuaScript` | `include/enjin2/components/lua_script.hpp` / `src/components/lua_script.cpp` | Component that owns the Lua state and all lifecycle dispatching | All existing callbacks (`init`, `update`, `draw`) live here |
| `callWithProxy()` | Private method on `C_LuaScript` | Retrieves ScriptProxy from Lua registry, pushes it, calls Lua function via `lua_pcall`, applies `ScriptErrorPolicy` | Every lifecycle callback already uses this pattern |
| `ScriptErrorPolicy` | `include/enjin2/components/lua_script.hpp` | Governs what happens on Lua errors | Phase 33 complete; input callbacks get same behavior |

### Supporting

| Component | Version/Location | Purpose | When to Use |
|-----------|-----------------|---------|-------------|
| `LuaBindings::currentInput` | `bindings.hpp` private field | Holds injected `InputState*` for current frame | Already populated by host via `setInput()`; option for querying input state from within C_LuaScript without duplication |
| `lua_pushinteger(L, btn)` | Lua C API | Push button index as integer onto stack | Used for the `btn` argument in callbacks |

---

## Architecture Patterns

### Recommended Project Structure

No new files needed. Changes go into:

```
src/components/lua_script.cpp          — dispatchInputCallbacks() + callWithProxy() extension
include/enjin2/components/lua_script.hpp  — declaration of dispatchInputCallbacks()
tests/input_event_callback_test.cpp    — new test file for INPUT-01, INPUT-02, INPUT-03
tests/CMakeLists.txt                   — register new test
```

### Pattern 1: callWithProxy() — Existing Dispatch Pattern

**What:** Every lifecycle callback pushes the stored `ScriptProxy` userdata from the Lua registry as `self`, then calls the Lua function via `lua_pcall`.

**Current signature:**
```cpp
// In lua_script.cpp
bool C_LuaScript::callWithProxy(const char* funcName, float dt, bool passDt) {
    // 1. lua_getglobal(L, funcName) — push function
    // 2. if not function: pop, return false (silent skip)
    // 3. lua_pushlightuserdata(L, this) + lua_gettable(LUA_REGISTRYINDEX) — push proxy
    // 4. if passDt: lua_pushnumber(L, dt) — push dt
    // 5. lua_pcall(L, nargs, 0, 0)
    // 6. on error: apply ScriptErrorPolicy
}
```

**Extension needed for Phase 34:** The function needs a way to push an integer `btn` as second argument instead of `float dt`. Options (Claude's discretion):

Option A — Extend signature with `int btn` and a separate flag:
```cpp
bool callWithProxy(const char* funcName, float dt, bool passDt,
                   int btn = -1, bool passBtn = false);
```

Option B — Dedicated method `callWithProxyAndBtn(const char* funcName, int btn)`:
```cpp
bool callWithProxyAndBtn(const char* funcName, int btn);
```

**Recommendation:** Option B. The `callWithProxy` signature is already growing; a dedicated method is cleaner and avoids adding another orthogonal flag to the existing method. It also makes call sites at the dispatch loop very readable.

### Pattern 2: Button Iteration Loop

**What:** Iterate over all 16 possible button indices, check edge state, fire callbacks.

```cpp
void C_LuaScript::dispatchInputCallbacks(const InputState& input) {
    if (!hasScript || scriptError || !scriptSystem) return;
    for (int btn = 0; btn < 16; ++btn) {
        if (input.justPressed(btn)) {
            callWithProxyAndBtn("on_button_pressed", btn);
        }
        if (input.justReleased(btn)) {
            callWithProxyAndBtn("on_button_released", btn);
        }
    }
}
```

**Placement:** Called at the top of `C_LuaScript::update()`, before the existing `callWithProxy(UPDATE_FUNCTION, ...)` — satisfying INPUT-03.

```cpp
void C_LuaScript::update(float dt) {
    Component::update(dt);
    if (!hasScript || scriptError || !scriptSystem) return;

    // INPUT-03: fire input callbacks BEFORE update()
    if (currentInputForCallbacks) {
        dispatchInputCallbacks(*currentInputForCallbacks);
    }

    lastUpdateTime += dt;
    setScriptVar("dt", ...);
    callWithProxy(UPDATE_FUNCTION, dt, true);
}
```

### Pattern 3: How C_LuaScript Gets the InputState

`C_LuaScript` currently does NOT hold an `InputState*` — input access is only available through `LuaBindings::currentInput`, which is populated by the host per-frame via `bindings.setInput()`.

For `dispatchInputCallbacks` to work, `C_LuaScript` needs access to the current frame's `InputState`. Options (Claude's discretion):

**Option A — Store InputState* on LuaBindings, expose via accessor:**
`LuaBindings` already has `currentInput`. Add `InputState* getInput() const { return currentInput; }`. `C_LuaScript::update()` calls `scriptSystem->getBindings().getInput()`.

Pros: No new injection plumbing. The `InputState*` is already being updated each frame. Zero redundancy.
Cons: `C_LuaScript` depends on `LuaBindings`'s internal state ordering — `setInput()` must be called before `update()`.

**Option B — Add `setInputForCallbacks(InputState*)` on C_LuaScript directly:**
A separate injection point on `C_LuaScript` analogous to `LuaBindings::setInput()`.

Pros: `C_LuaScript` owns its input explicitly.
Cons: Two injection points for the same data (one on `LuaBindings`, one on `C_LuaScript`); host must call both.

**Recommendation:** Option A. `LuaBindings::getInput()` is a one-liner accessor, and `scriptSystem->getBindings().getInput()` is already the established pattern for accessing bindings-owned state from `C_LuaScript`. No new host-side injection is required.

### Pattern 4: Optional Callback — Silent Skip

Consistent with all existing lifecycle callbacks, if `on_button_pressed` or `on_button_released` is not defined in the Lua script, `callWithProxyAndBtn()` should check `lua_isfunction` after `lua_getglobal` and return false silently (no error, no log). This is already how `callWithProxy()` behaves:

```cpp
lua_getglobal(L, funcName);
if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    return false;  // optional — not defined, not an error
}
```

### Anti-Patterns to Avoid

- **Do not pass the `InputState*` through Lua:** Edge detection is C++-side logic. The callbacks receive `btn` (an integer); they can call `engine.input.held(btn)` for additional state if needed.
- **Do not call input callbacks during `draw()`:** `draw()` is render-only. Input dispatch belongs in `update()` (INPUT-03 is explicit about this).
- **Do not fire callbacks when `scriptError == true`:** The early-return guard `if (!hasScript || scriptError || ...)` must precede the dispatch loop, consistent with `update()` and `draw()`.
- **Do not fire callbacks when Object is disabled:** `Component::update(dt)` (the base class call at the top of `C_LuaScript::update()`) already handles the disabled Object check — the guard cascade is already correct.
- **Do not use 32 as the button count ceiling:** `InputState` documents 16 buttons (indices 0-15); `buttons` is `uint16_t`. Loop must use `< 16`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Edge detection | Custom prev/curr state tracking | `InputState::justPressed(btn)` / `justReleased(btn)` | Already in `input_state.hpp`; Phase 22 complete |
| Self argument dispatch | Re-implement registry lookup | `callWithProxy()` + extension | Pattern is complete and tested in Phase 32/33 |
| Error handling | Per-callback error policy | `ScriptErrorPolicy` in `callWithProxy` | Phase 33 complete; identical behavior needed |

**Key insight:** This phase has no new infrastructure to build — it wires three complete, tested components together. The entire implementation is estimated at ~60 lines of C++ across two methods.

---

## Common Pitfalls

### Pitfall 1: Calling Callbacks When scriptError Is True (Disable Policy)

**What goes wrong:** With `ScriptErrorPolicy::Disable`, the first error sets `scriptError = true`. If `dispatchInputCallbacks()` does not respect this flag, it calls `callWithProxyAndBtn()` which immediately calls `callWithProxy()` which hits the `if (!hasScript || scriptError || ...)` guard inside... but wait — `callWithProxyAndBtn` bypasses that guard if it's implemented naively.

**How to avoid:** The `dispatchInputCallbacks()` method must check `if (!hasScript || scriptError || !scriptSystem) return;` at the top, matching the same guard in `update()`. Do not rely on `callWithProxyAndBtn` to handle this — it's a lower-level function.

### Pitfall 2: Firing Callbacks from a Missing InputState

**What goes wrong:** If the host does not call `LuaBindings::setInput()` before calling `update()`, `getInput()` returns `nullptr`. Dereferencing it in `dispatchInputCallbacks()` causes UB.

**How to avoid:** Guard `dispatchInputCallbacks()` with a null check: `if (!currentInputForCallbacks) return;`. The `engine.input.*` Lua bindings already do this same guard pattern.

### Pitfall 3: Ordering — Callbacks Fire After Update

**What goes wrong:** If `dispatchInputCallbacks()` is called AFTER `callWithProxy(UPDATE_FUNCTION, ...)`, INPUT-03 is violated. On the same frame a button is pressed, `update()` runs before the callback fires — stale behavior.

**How to avoid:** `dispatchInputCallbacks()` call goes BEFORE `callWithProxy(UPDATE_FUNCTION, ...)` in `C_LuaScript::update()`. The ordering is explicit in the code.

### Pitfall 4: lua_pcall Leaves Stack in Unknown State on Error

**What goes wrong:** If `lua_pcall` fails, it leaves the error string on the stack. If `callWithProxyAndBtn` doesn't pop it correctly, subsequent calls in the same button loop have a polluted stack.

**How to avoid:** `callWithProxy()` already does `lua_pop(L, 1)` after error handling. The new `callWithProxyAndBtn()` must do the same. Mirror the existing error path exactly.

### Pitfall 5: Double-Firing on ScriptErrorPolicy::Log

**What goes wrong:** With `ScriptErrorPolicy::Log`, errors are logged but `scriptError` stays false, so the script keeps running. If `on_button_pressed` errors every frame, it will fire every frame the button is held (not just on the press edge), because the per-frame button loop runs regardless.

**How to avoid:** This is actually correct behavior by design — `justPressed` returns true only on the one transition frame. On subsequent held frames, `justPressed` returns false, so the callback does not fire. No special handling needed.

---

## Code Examples

### callWithProxyAndBtn Pattern

```cpp
// Source: derived from existing callWithProxy() in src/components/lua_script.cpp

bool C_LuaScript::callWithProxyAndBtn(const char* funcName, int btn) {
    if (!scriptSystem) return false;
    lua_State* L = scriptSystem->getEngine().getState();
    if (!L) return false;

    lua_getglobal(L, funcName);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return false;  // optional callback — not defined is not an error
    }

    // Retrieve stored proxy userdata from Lua registry
    lua_pushlightuserdata(L, this);
    lua_gettable(L, LUA_REGISTRYINDEX);

    // Push btn as integer second arg
    lua_pushinteger(L, static_cast<lua_Integer>(btn));

    // call: funcName(self, btn)
    int result = lua_pcall(L, 2, 0, 0);
    if (result != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        errorMessage = err ? err : "unknown Lua error";
        lua_pop(L, 1);

        switch (errorPolicy) {
            case ScriptErrorPolicy::Disable:
                if (!scriptError) {
                    printf("[lua] script error (%s): %s\n", funcName, errorMessage.c_str());
                }
                scriptError = true;
                break;
            case ScriptErrorPolicy::Log:
                printf("[lua] script error (%s): %s\n", funcName, errorMessage.c_str());
                break;
            case ScriptErrorPolicy::Panic:
                printf("[lua] PANIC (%s): %s\n", funcName, errorMessage.c_str());
#ifdef ESP32
                esp_restart();
#else
                std::abort();
#endif
                break;
        }
        return false;
    }
    return true;
}
```

### dispatchInputCallbacks Pattern

```cpp
// Source: derived from project design doc (lua-embedding-design.md §4.1)

void C_LuaScript::dispatchInputCallbacks(const InputState& input) {
    if (!hasScript || scriptError || !scriptSystem) return;
    for (int btn = 0; btn < 16; ++btn) {
        if (input.justPressed(btn)) {
            callWithProxyAndBtn("on_button_pressed", btn);
        }
        if (input.justReleased(btn)) {
            callWithProxyAndBtn("on_button_released", btn);
        }
    }
}
```

### Insertion Point in C_LuaScript::update()

```cpp
// BEFORE (current):
void C_LuaScript::update(float dt) {
    Component::update(dt);
    if (!hasScript || scriptError || !scriptSystem) return;
    ...
    callWithProxy(UPDATE_FUNCTION, dt, true);
}

// AFTER (Phase 34):
void C_LuaScript::update(float dt) {
    Component::update(dt);
    if (!hasScript || scriptError || !scriptSystem) return;

    // INPUT-03: fire input edge callbacks before update()
    InputState* input = scriptSystem->getBindings().getInput();
    if (input) {
        dispatchInputCallbacks(*input);
    }

    lastUpdateTime += dt;
    setScriptVar("dt", static_cast<double>(dt));
    setScriptVar("time", static_cast<double>(lastUpdateTime));
    callWithProxy(UPDATE_FUNCTION, dt, true);
}
```

### Example Lua Script (Verification Reference)

```lua
-- on_button_pressed fires exactly once on the frame the button transitions to pressed
function on_button_pressed(self, btn)
    if btn == 0 then
        pressed_count = (pressed_count or 0) + 1
    end
end

-- on_button_released fires exactly once on the frame the button transitions to released
function on_button_released(self, btn)
    if btn == 0 then
        released_count = (released_count or 0) + 1
    end
end

function update(self, dt)
    -- update runs after both callbacks
end
```

### Test Pattern (input_event_callback_test.cpp)

```cpp
// Pattern modeled on error_policy_test.cpp and engine_table_test.cpp
// Uses InputState directly (no platform poll needed)

Object obj;
C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
script->loadScript(
    "pressed_count = 0\n"
    "released_count = 0\n"
    "function on_button_pressed(self, btn)\n"
    "    if btn == 0 then pressed_count = pressed_count + 1 end\n"
    "end\n"
    "function on_button_released(self, btn)\n"
    "    if btn == 0 then released_count = released_count + 1 end\n"
    "end\n"
);

// Create InputState — set button 0 as just-pressed (prev=0, curr=1)
InputState input{};
input.prev_buttons = 0;
input.buttons = static_cast<uint16_t>(1u << 0);

// Inject input into LuaBindings before calling update()
LuaEngine* engine = &script->...; // access via LuaScriptSystem
// NOTE: C_LuaScript owns its LuaScriptSystem privately — test must call update()
// which internally accesses LuaBindings::currentInput via the accessor

// Simpler approach: wire input through LuaScriptSystem::getBindings().setInput()
// after constructing the script — then call update(dt)
```

**Important test architecture note:** `C_LuaScript` owns its `LuaScriptSystem` privately (no public accessor). The test needs to either:
1. Add a `getScriptSystem()` accessor (not recommended — breaks encapsulation), OR
2. Use `getScriptNumber("pressed_count")` to read Lua globals after `update()` — this is the established pattern from `error_policy_test.cpp`.

The second approach is correct and consistent. The test injects `InputState` by calling `script->getScriptSystem()` indirectly, OR by having the test fixture manually call `LuaBindings::setInput()` on the internally-owned system. Since `C_LuaScript` does not currently expose `scriptSystem`, the test must call `update()` and read results via `getScriptNumber()`.

However, to inject `InputState`, the test needs a way to pass it in. The cleanest approach: add a `setInput(InputState*)` method to `C_LuaScript` as a thin wrapper over `scriptSystem->getBindings().setInput()`. This is a clean, minimal addition that follows the same pattern as `LuaBindings::setInput()`.

---

## Open Questions

1. **Does `C_LuaScript` need a public `setInput()` method?**
   - What we know: Tests need to inject `InputState` to verify callback firing. The current class has no such method. `LuaBindings::setInput()` is what the host calls.
   - What's unclear: Whether the test should drive through `C_LuaScript` directly or via its internal `LuaScriptSystem`.
   - Recommendation: Add `void C_LuaScript::setInput(InputState* input)` as a thin wrapper over `scriptSystem->getBindings().setInput(input)`. This is consistent with how `C_LuaScript::setCanvas()`/etc. work (they delegate to scriptSystem). The host may also want this in production game loops.

2. **Should `callWithProxyAndBtn` be a new private method or extend `callWithProxy`?**
   - What we know: Both approaches work. CONTEXT.md leaves this to Claude's discretion.
   - What's unclear: Nothing — both are clear.
   - Recommendation: Separate method `callWithProxyAndBtn` (Option B) as documented in Architecture Patterns above. Cleaner call sites, no new flag on `callWithProxy`.

3. **Button count ceiling — should it be `16` or a constant?**
   - What we know: `InputState::buttons` is `uint16_t`, documented as 16 buttons (indices 0-15).
   - Recommendation: Loop with `btn < 16`. Alternatively extract `static constexpr int kMaxButtons = 16` in `lua_script.cpp` if consistency with future changes is a concern. Either is fine; the literal is acceptable given InputState's documented constraint.

---

## Sources

### Primary (HIGH confidence)

- `/home/unwn/dev/enjin/include/enjin2/input/input_state.hpp` — `InputState` struct with `justPressed()`, `justReleased()`, `held()` inline methods; documents 16-button limit, `uint16_t` bitmask
- `/home/unwn/dev/enjin/src/components/lua_script.cpp` — `callWithProxy()` implementation; `update()` structure; `ScriptErrorPolicy` error handling path
- `/home/unwn/dev/enjin/include/enjin2/components/lua_script.hpp` — `C_LuaScript` class declaration; `ScriptErrorPolicy` enum; `callWithProxy()` docstring
- `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — `LuaBindings` class; `currentInput` field; `setInput()` method
- `/home/unwn/dev/enjin/tests/error_policy_test.cpp` — test pattern for `C_LuaScript` callbacks via `loadScript` + `update()` + `getScriptNumber()`; CMakeLists registration pattern
- `/home/unwn/dev/enjin/tests/CMakeLists.txt` — test registration pattern for `ENJIN2_BUILD_LUA`-gated tests
- `/home/unwn/dev/enjin/project/lua-embedding-design.md` — §4.1 defines `on_button_pressed(btn)` / `on_button_released(btn)` API design; §P6 rationale for event callbacks alongside polling

### Secondary (MEDIUM confidence)

- `/home/unwn/dev/enjin/.planning/phases/34-input-event-callbacks/34-CONTEXT.md` — user decisions locking dispatch scope, ordering, `btn` type, error policy inheritance

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — verified against source code; all infrastructure complete
- Architecture: HIGH — `callWithProxy()` pattern is established and tested; extension is mechanical
- Pitfalls: HIGH — derived from reading existing error handling and guard patterns in `lua_script.cpp`

**Research date:** 2026-02-27
**Valid until:** 60 days — this research is entirely based on project source code, not external libraries
