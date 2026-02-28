# Phase 33: ScriptErrorPolicy - Research

**Researched:** 2026-02-27
**Domain:** C++ error-handling policy enum on C_LuaScript; Lua 5.1 pcall error recovery; platform panic dispatch; hot-reload state clearing
**Confidence:** HIGH

## Summary

Phase 33 adds a `ScriptErrorPolicy` enum to `C_LuaScript` with three values — Disable, Log, and Panic — controlling what happens when `lua_pcall` returns an error code in `callWithProxy()`. The current implementation already uses `lua_pcall` everywhere for safe error capture; what's missing is the _response_ logic: right now errors set `scriptError = true` via the comment "Do NOT set scriptError here — callWithProxy is the low-level call path" and callers decide nothing. The phase closes this gap by making the dispatch decision in `callWithProxy()` (or its callers) based on the policy field.

There is a critical pre-existing structural issue discovered in Phase 32 verification: **`src/components/lua_script.cpp` is not compiled into any CMake target**. The `enjin2_lua` library only compiles `lua_engine.cpp`, `lua_platform.cpp`, and the `bindings_*.cpp` files. This means `C_LuaScript::callWithProxy()`, `executeScript()`, the destructor, and all policy logic would be dead code unless Phase 33 also adds `lua_script.cpp` to the `enjin2_lua` target (or a new component target). This is the single most important blocker for this phase. Additionally, `lua_script.hpp` declares `IScriptInterpreter*` but `lua_script.cpp` uses `LuaScriptSystem*` — these two are incompatible and must be reconciled before the file can compile at all.

The SDL runner (the live test vehicle) drives Lua callbacks directly via `lua_pcall` without going through `C_LuaScript`. The runner already implements Disable-equivalent behaviour (`lua_ok = false` on error). Phase 33 targets the `C_LuaScript` component path, not the SDL runner path. Hot-reload on F5 calls `performReload()` which does a full Lua state shutdown and re-init, which naturally clears error state — the planner only needs to ensure the `scriptError` bool and policy state are reset on `reloadScript()`.

**Primary recommendation:** Fix the CMake + header mismatch as Wave 0 before adding any policy logic. Once `lua_script.cpp` compiles, add the `ScriptErrorPolicy` enum and dispatch in `callWithProxy()`. Three implementation patterns exist (shown in Code Examples below).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ERR-01 | `C_LuaScript` has a `ScriptErrorPolicy` field with values Disable, Log, Panic | Enum placement in `lua_script.hpp`; field added as `policy_` or `errorPolicy`; default = Disable |
| ERR-02 | Default Disable policy: on error, script is disabled, logs once, engine continues | `callWithProxy()` sets `scriptError = true` after first error; subsequent frames short-circuit in `update()`/`draw()` via `if (!hasScript || scriptError)` guard already present |
| ERR-03 | Log policy: on error, logs every frame, script keeps running on subsequent frames | `callWithProxy()` logs error but returns without setting `scriptError`; script runs again next frame |
| ERR-04 | Panic policy: on error, calls platform panic handler | Desktop: `assert(false)` or `std::abort()`; ESP32: `esp_restart()` via `#ifdef ESP32` — same pattern as existing `LuaEngine::luaPanic()` |
| ERR-05 | F5 hot-reload clears error state and re-enables disabled scripts | `reloadScript()` / `executeScript()` already sets `scriptError = false`; verify `hasScript` is also reset; SDL runner `performReload()` does full lua close+init which naturally resets all state |
</phase_requirements>

## Standard Stack

### Core
| Component | Location | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| `lua_pcall` | Lua C API | Protected call — catches errors without crashing the process | Already used in all `callWithProxy()` call sites; Lua 5.1 standard |
| `ScriptErrorPolicy` enum | `lua_script.hpp` | Three-value dispatch selector | C++ enum class, zero allocation, fits in a byte |
| `printf` | `<cstdio>` | Logging on all platforms | Project convention: printf-only, no std::cout, no std::string (see Phase 31 decisions) |
| `std::abort()` | `<cstdlib>` | Desktop panic | Existing pattern in `LuaEngine::luaPanic()` |

### Platform Panic Dispatch
| Platform | Panic Implementation | Header Required |
|----------|---------------------|----------------|
| Desktop (VCV_RACK define) | `std::abort()` or `assert(false)` | `<cassert>` or `<cstdlib>` |
| ESP32 | `esp_restart()` or `abort()` | `"esp_system.h"` (already in `lua_platform.cpp`) |
| Pattern | `#ifdef ESP32 / #else` switch | Same pattern as `lua_platform.cpp` throughout |

### No External Libraries Needed
This phase is pure C++ enum + control flow. No new packages. No new Lua APIs. No new cmake `find_package`.

## Architecture Patterns

### Recommended File Structure
```
include/enjin2/components/lua_script.hpp   — add ScriptErrorPolicy enum + policy_ member + setter
src/components/lua_script.cpp              — add to CMakeLists enjin2_lua target; fix header mismatch
```

**CRITICAL: Wave 0 structural work (must precede any policy code):**
```
1. Reconcile lua_script.hpp vs lua_script.cpp — align on LuaScriptSystem* (not IScriptInterpreter*)
2. Add src/components/lua_script.cpp to enjin2_lua STATIC target in CMakeLists.txt
3. Verify cmake build is clean before proceeding to policy implementation
```

### Pattern 1: Enum Declaration in Header
**What:** A C++ enum class at namespace or class scope in `lua_script.hpp`
**When to use:** Always — single definition location
```cpp
// Source: project convention — zero-heap, no std::string enum
namespace enjin2 {

enum class ScriptErrorPolicy : uint8_t {
    Disable = 0,  ///< On error: disable script, log once, engine continues (default)
    Log     = 1,  ///< On error: log every frame, script keeps running
    Panic   = 2   ///< On error: invoke platform panic handler (halts ESP32, aborts desktop)
};

class C_LuaScript : public C_Drawable {
    // ...
    ScriptErrorPolicy errorPolicy{ScriptErrorPolicy::Disable};
public:
    void setErrorPolicy(ScriptErrorPolicy policy) { errorPolicy = policy; }
    ScriptErrorPolicy getErrorPolicy() const { return errorPolicy; }
    // ...
};
```

### Pattern 2: Policy Dispatch in callWithProxy()
**What:** After `lua_pcall` returns non-LUA_OK, switch on `errorPolicy`
**When to use:** callWithProxy() is the single error capture point — dispatch here, not in callers
```cpp
// Source: extends existing callWithProxy() in src/components/lua_script.cpp
// Based on the comment on line 306: "The callers (update, draw) decide whether to set scriptError"
// — Phase 33 moves this decision INTO callWithProxy() via the policy field

int result = lua_pcall(L, nargs, 0, 0);
if (result != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    errorMessage = err ? err : "unknown Lua error";
    lua_pop(L, 1);

    switch (errorPolicy) {
        case ScriptErrorPolicy::Disable:
            // Log once, disable script permanently (until hot-reload)
            if (!scriptError) {
                printf("[lua] script error (%s): %s\n", funcName, errorMessage.c_str());
            }
            scriptError = true;
            break;

        case ScriptErrorPolicy::Log:
            // Log every frame, do not disable (debug mode)
            printf("[lua] script error (%s): %s\n", funcName, errorMessage.c_str());
            // scriptError intentionally NOT set — script runs again next frame
            break;

        case ScriptErrorPolicy::Panic:
            // Invoke platform panic handler — no return
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
```

### Pattern 3: Hot-Reload Clears Error State
**What:** `executeScript()` / `reloadScript()` already resets `scriptError = false` — confirm it also clears `errorMessage`
**When to use:** ERR-05 — no new code needed if existing reset is complete
```cpp
// Source: lua_script.cpp line 126-128 — already present
hasScript = true;
scriptError = false;
errorMessage.clear();
// The policy field itself does NOT need resetting — it's a configuration, not runtime state
// SDL runner: performReload() calls lua.shutdown() + lua.initialize() which destroys the LuaScriptSystem
// C_LuaScript: reloadScript() -> loadScript()/loadScriptFile() -> executeScript() which resets the flags
```

### Pattern 4: printf + char array (NOT std::string for log messages)
**What:** Error messages in callWithProxy use `errorMessage` (std::string member already present in header)
**When to use:** The errorMessage field already exists as `std::string` — keep it. But log lines use `printf`, not `std::cout`.
```cpp
// Correct pattern (project conformity — Phase 31 decision):
printf("[lua] script error (%s): %s\n", funcName, errorMessage.c_str());

// Incorrect (violates project convention):
// std::cerr << "[lua] " << errorMessage << std::endl;
```

### Anti-Patterns to Avoid
- **Setting scriptError in update()/draw() callers directly:** The existing comment in callWithProxy() line 306 says "callers decide" but Phase 33 moves this to callWithProxy() dispatch — don't leave partial logic in both.
- **Using std::string for font names / policy labels:** printf with c_str() is the project convention.
- **Calling esp_restart() without `#ifdef ESP32`:** This will fail to compile on desktop. Always gate platform APIs.
- **Resetting errorPolicy on reload:** errorPolicy is configuration, not runtime state. Don't clear it on F5 reload.
- **Adding lua_script.cpp to enjin2_ui or a new enjin2_components target:** Add it to `enjin2_lua` — that's where all the Lua C API dependencies are linked.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Error capture | Try/catch around Lua calls | `lua_pcall` (already used) | Lua errors unwind the C stack; pcall is the correct protection mechanism in Lua 5.1 |
| Platform panic | Custom reset/halt logic | `std::abort()` / `esp_restart()` via existing `#ifdef ESP32` guard pattern | Already established in lua_platform.cpp; consistent with `LuaEngine::luaPanic` |
| Error logging | Custom log buffer | `printf` to stderr/stdout | Project convention; embedded-safe; no allocation |

**Key insight:** The error infrastructure (pcall, error message capture, scriptError flag) is already 90% present in `lua_script.cpp`. Phase 33 is primarily a dispatch routing change plus a CMake build fix.

## Common Pitfalls

### Pitfall 1: lua_script.cpp Not Compiled
**What goes wrong:** All policy code is dead code; phase appears to work in isolation but C_LuaScript is never used in any test
**Why it happens:** `enjin2_lua` STATIC target in CMakeLists.txt does not list `src/components/lua_script.cpp`
**How to avoid:** Wave 0 task: add `src/components/lua_script.cpp` to `target_sources(enjin2_lua PRIVATE ...)` in root CMakeLists.txt around line 140
**Warning signs:** `grep "lua_script" CMakeLists.txt` in the project root returns zero hits

### Pitfall 2: Header vs Implementation Type Mismatch
**What goes wrong:** `lua_script.hpp` declares `interpreter: std::unique_ptr<IScriptInterpreter>` but `lua_script.cpp` uses `scriptSystem: std::unique_ptr<LuaScriptSystem>` — these are different types, compilation fails
**Why it happens:** The header was written for a generic interpreter abstraction; the implementation was written for LuaScriptSystem directly
**How to avoid:** Wave 0 task: align the header to match the .cpp — change the `interpreter` field to `scriptSystem: std::unique_ptr<LuaScriptSystem>` and remove the unused `IScriptInterpreter*` declarations; OR rewrite the .cpp to use the interface (more work, less correct for this project)
**Warning signs:** `src/components/lua_script.cpp` fails to compile with "no member named 'scriptSystem' in 'enjin2::C_LuaScript'"

### Pitfall 3: Setting scriptError in callWithProxy Breaks Log Policy
**What goes wrong:** If you set `scriptError = true` before the switch, Log policy still disables the script
**Why it happens:** `update()` and `draw()` both guard on `if (!hasScript || scriptError)` — if scriptError is true, script is skipped
**How to avoid:** Only set `scriptError = true` inside the Disable case of the switch

### Pitfall 4: Panic on Desktop Halts Test Suite
**What goes wrong:** A unit test that exercises Panic policy will abort the entire test process
**Why it happens:** `std::abort()` kills the process
**How to avoid:** The unit test for ERR-04 must use a separate process or a signal handler, OR skip the live Panic invocation and just verify the policy value is set correctly; test the panic path via manual inspection only

### Pitfall 5: Log Policy Fires on init() Errors Too
**What goes wrong:** Log policy logs on every frame — if init() errors, subsequent update()/draw() calls error too, creating a log flood
**Why it happens:** With Log policy, scriptError is never set, so the is-it-disabled guard never triggers
**How to avoid:** This is intentional per ERR-03 ("debug mode") — document it, don't "fix" it; the Disable policy is the sane default

### Pitfall 6: errorMessage Uses std::string But Log Must Use printf
**What goes wrong:** Using `std::cerr <<` instead of `printf` violates project convention
**Why it happens:** `std::string errorMessage` tempts use of `<<` operator
**How to avoid:** Always access via `errorMessage.c_str()` with `printf`

### Pitfall 7: Hot-Reload and C_LuaScript vs SDL Runner Are Two Separate Paths
**What goes wrong:** Thinking that the SDL runner's `lua_ok = false` is the same as the C_LuaScript Disable policy
**Why it happens:** Both implement "stop running script on error" but they're independent code paths
**How to avoid:** Phase 33 only modifies `C_LuaScript`. The SDL runner already has its own policy (implicit Disable); don't change it unless there's an explicit requirement.

## Code Examples

Verified patterns from existing codebase:

### Existing callWithProxy Error Path (lua_script.cpp lines 301-308)
```cpp
// Source: src/components/lua_script.cpp
int result = lua_pcall(L, nargs, 0, 0);
if (result != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    errorMessage = err ? err : "unknown Lua error";
    lua_pop(L, 1);
    // Do NOT set scriptError here — callWithProxy is the low-level call path.
    // The callers (update, draw) decide whether to set scriptError.
    return false;
}
return true;
```
Phase 33 replaces the "Do NOT set scriptError" comment block with the policy switch.

### Existing update() Guard (lua_script.cpp lines 214-218)
```cpp
// Source: src/components/lua_script.cpp
void C_LuaScript::update(float dt) {
    Component::update(dt);

    if (!hasScript || scriptError || !scriptSystem) {
        return;  // <-- Disable policy works via this guard
    }
    // ...
    callWithProxy(UPDATE_FUNCTION, dt, true);
}
```

### Existing Platform Panic Pattern (lua_engine.cpp line 226-230)
```cpp
// Source: src/scripting/lua_engine.cpp — existing luaPanic handler
int LuaEngine::luaPanic(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    std::cerr << "Lua panic: " << (msg ? msg : "unknown error") << std::endl;
    std::abort();
}
// Note: lua_engine.cpp uses std::cerr for the panic message — this is acceptable for a panic-level
// event. Normal error logging must use printf per project convention.
```

### CMakeLists Addition (root CMakeLists.txt ~line 140)
```cmake
# Source: CMakeLists.txt enjin2_lua STATIC target definition
add_library(enjin2_lua STATIC)
target_sources(enjin2_lua PRIVATE
    src/scripting/lua_engine.cpp
    src/scripting/lua_platform.cpp
    src/scripting/bindings.cpp
    src/scripting/bindings_draw.cpp
    src/scripting/bindings_input_sprites.cpp
    src/scripting/bindings_layers_text.cpp
    src/scripting/bindings_engine.cpp
    src/scripting/bindings_math.cpp
    src/scripting/bindings_system.cpp
    src/components/lua_script.cpp    # <-- ADD THIS LINE (Phase 33 Wave 0)
)
```

### Header Reconciliation (lua_script.hpp)
```cpp
// Current broken state (lua_script.hpp line 29):
//     std::unique_ptr<IScriptInterpreter> interpreter;

// Phase 33 Wave 0 fix — change to match lua_script.cpp actual usage:
private:
    std::unique_ptr<LuaScriptSystem> scriptSystem;
    std::unique_ptr<LuaCanvas>       luaCanvas;
    bool hasScript{false};
    bool scriptError{false};
    std::string errorMessage;
    ScriptErrorPolicy errorPolicy{ScriptErrorPolicy::Disable};
    // ...
```

### Test Pattern: Error Policy Unit Test
```cpp
// Pattern from existing hot_reload_test.cpp and engine_table_test.cpp fixtures
// Uses LuaEngine + LuaBindings directly (same as all existing Lua unit tests)
// No external framework needed — project uses hand-rolled ASSERT macros

struct ErrorPolicyFixture {
    LuaEngine  engine;
    LuaBindings bindings;
    // ...
};

// ERR-02 test: Disable policy stops script after one error
static void test_disable_policy_stops_after_error() {
    // Load a script with a buggy update(), verify it stops calling after error
    // Can be tested via LuaEngine + executeString (without C_LuaScript if needed)
}
```

## State of the Art

| Old Approach | Current Approach | Relevance |
|--------------|------------------|-----------|
| callWithProxy comment: "callers decide" | Phase 33: policy dispatch in callWithProxy | Phase 33 closes this gap |
| lua_script.cpp orphaned from build | Phase 33 Wave 0: add to CMakeLists | Must be done first |
| Implicit Disable in SDL runner | Explicit ScriptErrorPolicy enum | C_LuaScript gets explicit policy; SDL runner unchanged |

**Pre-existing tech debt entering Phase 33:**
- `lua_script.cpp` not in any CMake target — BLOCKER, must fix in Wave 0
- `lua_script.hpp` field types don't match `lua_script.cpp` usage — BLOCKER, must fix in Wave 0
- `loadScriptFile()` calls `callScriptFunctionSafe(INIT_FUNCTION)` instead of `callWithProxy(INIT_FUNCTION, 0.0f, false)` — PROXY-01 gap; fix this in Wave 0 or as part of error policy plumbing

## Open Questions

1. **Should the test for ERR-04 (Panic) invoke the actual abort?**
   - What we know: `std::abort()` terminates the process; test suites can't recover from it
   - What's unclear: Whether the verifier expects a live abort or just that the code path reaches the right branch
   - Recommendation: Test ERR-04 by verifying the policy field is set to Panic and that the code path reaches the abort branch — use a mock or signal handler only if absolutely required; otherwise mark as manual-verify only

2. **Should lua_script.hpp be cleaned up fully (remove IScriptInterpreter* etc.) or minimally reconciled?**
   - What we know: The header has many methods (callScriptFunction, getInterpreterType, etc.) that may not have matching .cpp implementations
   - What's unclear: How much unused API surface to prune
   - Recommendation: Minimally reconcile — change field types to match .cpp, add ScriptErrorPolicy, don't delete unimplemented methods in Phase 33 (separate cleanup concern)

3. **Should the library.json exclusion of lua_script.cpp be addressed?**
   - What we know: `library.json` has `"-<components/lua_script.cpp>"` (excluded)
   - What's unclear: Whether this affects the CMake build or is a different build system
   - Recommendation: Only add to `CMakeLists.txt` for now; library.json appears to be for an ESP-IDF/Arduino-style build, separate concern

## Validation Architecture

> `workflow.nyquist_validation` is not set in `.planning/config.json` (key absent). Skipping this section.

## Sources

### Primary (HIGH confidence)
- Direct code inspection: `src/components/lua_script.cpp` (lines 274-317, callWithProxy implementation)
- Direct code inspection: `include/enjin2/components/lua_script.hpp` (field declarations, method signatures)
- Direct code inspection: `src/scripting/lua_engine.cpp` (luaPanic pattern, lines 226-231)
- Direct code inspection: `CMakeLists.txt` (enjin2_lua target_sources, lines 139-150)
- Direct code inspection: `.planning/phases/32-scriptproxy-userdata/32-VERIFICATION.md` (blocker documentation)
- Direct code inspection: `.planning/STATE.md` (project decisions, printf-only convention)

### Secondary (MEDIUM confidence)
- `.planning/REQUIREMENTS.md` — ERR-01 through ERR-05 requirements text
- `.planning/ROADMAP.md` Phase 33 success criteria — matches requirements
- `tests/hot_reload_test.cpp` — confirmed test pattern for Lua unit tests (no external framework)
- `src/platform/sdl/sdl_main.cpp` — confirmed hot-reload path and F5 behaviour

### Tertiary (LOW confidence)
- None — all findings verified from direct code inspection

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components are existing project code, no external dependencies
- Architecture: HIGH — callWithProxy is the single error capture point, clearly identified from source
- Pitfalls: HIGH — CMake gap is documented in 32-VERIFICATION.md; header mismatch visible in source

**Research date:** 2026-02-27
**Valid until:** 2026-04-01 (stable codebase; no fast-moving dependencies)
