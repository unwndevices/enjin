# Phase 56: Tech Debt Cleanup - Research

**Researched:** 2026-03-02
**Domain:** C++ Lua bindings cleanup — dangling pointer elimination and diagnostic warning
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

No user decisions required. This phase has precise success criteria that fully define both fixes. Implementation follows directly from the codebase patterns found during scouting. Claude has full discretion on approach.

### Claude's Discretion

- Camera fix: Clear `m_followTargetProxy = nullptr` in `setActiveScene()` alongside the existing `m_activeCamera = nullptr`, `clearCoroutines()`, `clearTweens()` pattern (bindings.cpp:717). Also check if hot reload goes through a separate path that needs the same clear.
- Warning: Use `lua_warning()` (Lua 5.4 C API) per success criteria, or fall back to the existing `printf()` pattern if the Lua version doesn't support it — planner decides after checking the Lua version in use.
- Warning message wording for the persist() no-SSM case.

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DEBT-01 | `m_followTargetProxy` cleared on scene transition and hot reload (no dangling reference) | Two call sites confirmed: `setActiveScene()` (bindings.cpp:708-720) and `registerAll()` (bindings.cpp:435-482). Both need one line added. |
| DEBT-02 | `engine.scene.persist()` emits `lua_warning()` when called without SceneStateMachine context | Silent `lua_pushnil` at bindings_engine.cpp:420-422 confirmed. Project uses Lua 5.1 — `lua_warning()` does not exist; `printf()` is the established pattern. |
</phase_requirements>

## Summary

Phase 56 addresses two latent correctness bugs in the scripting bindings layer. Both fixes are surgical one-to-three-line changes in files that are already well-understood.

**DEBT-01 (camera proxy leak):** `m_followTargetProxy` (an `ObjectProxy*` declared in `bindings.hpp:450`) is set by `engine.camera.follow()` and cleared by `engine.camera.stopFollow()` and `tickCameraFollow()` on proxy destruction. However, neither `setActiveScene()` (the scene-change path) nor `registerAll()` (the hot-reload path) clear this pointer. Both functions already contain a well-established cleanup block that clears `m_activeCamera`, coroutines, and tweens — `m_followTargetProxy = nullptr` belongs in both blocks, following the exact same pattern.

**DEBT-02 (silent persist no-op):** `lua_engine_scene_persist()` returns `lua_pushnil` silently at line 420-422 when `b->m_ssm` is null (no SceneStateMachine context). The fix is to emit a diagnostic message before the silent return. The project uses Lua 5.1 (LuaJIT on desktop, Lua 5.1.5 on WASM and ESP32) — `lua_warning()` is a Lua 5.4 API and does not exist in this codebase. The established pattern across the entire codebase is `printf()`. The warning should use `printf()` with a clear, actionable message.

**Primary recommendation:** Two isolated edits in two files. Add `m_followTargetProxy = nullptr` in `setActiveScene()` (bindings.cpp:717) and `registerAll()` (bindings.cpp:482), and add a `printf()` warning before the no-SSM nil return in `lua_engine_scene_persist()` (bindings_engine.cpp:420).

## Standard Stack

### Core

| Component | Version | Purpose | Notes |
|-----------|---------|---------|-------|
| Lua 5.1 (LuaJIT) | LuaJIT 2.1 | Desktop scripting runtime | API-compatible with Lua 5.1 |
| Lua 5.1.5 | 5.1.5 | WASM and ESP32 runtime | Built from source via FetchContent |
| C++ (bindings layer) | C++17 | Host-side Lua binding implementation | No new libraries needed |

No new dependencies. This phase uses only code already present in the project.

### Lua Version Reality Check

All three build targets run the Lua 5.1 C API:

| Target | Lua Runtime | `lua_warning()` available? |
|--------|------------|---------------------------|
| Desktop (sdl3) | LuaJIT 2.1 (Lua 5.1 API) | NO — Lua 5.4 only |
| WASM | Lua 5.1.5 | NO — Lua 5.4 only |
| ESP32 | Lua 5.1.5 | NO — Lua 5.4 only |

**Conclusion:** `lua_warning()` MUST NOT be used. Use `printf()` — the project-wide established pattern for all diagnostic output.

## Architecture Patterns

### Pattern 1: Scene-Change Cleanup Block (DEBT-01 fix)

**What:** `setActiveScene()` in bindings.cpp already contains a cleanup block for state that must not survive scene transitions. The `m_followTargetProxy` clear goes in this block.

**Current state (bindings.cpp:708-720):**
```cpp
void LuaBindings::setActiveScene(Scene* scene) {
    if (scene != m_activeScene) {
        // EVENT-04: Scene is changing -- clear event bus for the outgoing scene.
        m_eventBus.clearHandlers();
        // CAM-08: Clear cached camera pointer on scene change (Phase 44).
        m_activeCamera = nullptr;
        // ASYNC-03: clear coroutines on scene transition (prevent stale refs)
        clearCoroutines();
        clearTweens();     // TWEEN-02: clean slate on scene transition
    }
    m_activeScene = scene;
}
```

**After fix — add after `clearTweens()`:**
```cpp
        m_followTargetProxy = nullptr;  // DEBT-01: clear follow target on scene change
```

### Pattern 2: Hot-Reload Cleanup Block (DEBT-01 fix — second call site)

**What:** `registerAll()` in bindings.cpp contains the hot-reload cleanup. Lines 477-482 already clear event bus, coroutines, and tweens. The follow proxy clear goes in the same block.

**Current state (bindings.cpp:477-482):**
```cpp
    // EVENT-05: clear event bus handlers from previous load (hot-reload cleanup)
    m_eventBus.clearHandlers();
    m_eventBus.setLuaState(L);
    // ASYNC-03: clear coroutine pool on every hot-reload (clean slate)
    clearCoroutines();
    clearTweens();     // TWEEN-02: clean slate on every hot-reload
```

**After fix — add after `clearTweens()`:**
```cpp
    m_followTargetProxy = nullptr;  // DEBT-01: clear follow target on hot reload
```

### Pattern 3: Diagnostic printf Before Silent Return (DEBT-02 fix)

**What:** `lua_engine_scene_persist()` in bindings_engine.cpp silently returns nil when called without SSM context. The fix adds a `printf()` before the nil return, following the project's established diagnostic pattern (`printf()` is used in `lua_print`, engine.log, and throughout the codebase).

**Current state (bindings_engine.cpp:419-422):**
```cpp
    LuaBindings* b = getBindings(L);
    if (!b || !b->m_ssm) {
        lua_pushnil(L);
        return 1;
    }
```

**After fix:**
```cpp
    LuaBindings* b = getBindings(L);
    if (!b || !b->m_ssm) {
        printf("[enjin] WARNING: engine.scene.persist() called without SceneStateMachine context — no-op\n");
        lua_pushnil(L);
        return 1;
    }
```

**Note:** The `!b` branch (null bindings) is an engine misconfiguration, not a user script error. The `!b->m_ssm` branch is the actionable user-facing case. If desired, a planner could print warning only for `!b->m_ssm` and leave `!b` as a silent error. However, a single combined printf is simpler and both cases indicate the same outcome from the script's perspective.

### Anti-Patterns to Avoid

- **Do not use `lua_warning()`:** It is a Lua 5.4 API. The project uses Lua 5.1. Using it will cause a compile error.
- **Do not add `m_followTargetProxy = nullptr` inside the `tickCameraFollow()` "invalid proxy" branch only:** That branch only triggers when the proxy's Object is destroyed at runtime. Scene change and hot reload bypass it entirely.
- **Do not touch `bindings_engine.cpp` line 415 (the invalid-proxy nil return):** That guard (lines 412-417) handles a destroyed or invalid proxy — it is correct behavior, not a bug.
- **Do not add the proxy clear outside the `if (scene != m_activeScene)` guard:** The guard exists intentionally; a no-op scene set (same scene) should not clear anything.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Warning output | Custom warn infrastructure | `printf()` | Already the project standard; consistent across all 3 platforms |
| Proxy lifecycle management | Custom weak-ref scheme | Existing `valid` flag pattern | ObjectProxy.valid already handles in-frame destruction; only scene boundary is missing |

**Key insight:** Both fixes are one-line additions following patterns already established in the same functions. No new abstractions needed.

## Common Pitfalls

### Pitfall 1: Only Fixing One of the Two Call Sites for DEBT-01

**What goes wrong:** `m_followTargetProxy` is only cleared in `setActiveScene()` but not in `registerAll()`, leaving hot reload as a lingering stale-proxy path.
**Why it happens:** The two cleanup blocks look similar but serve different events. `setActiveScene()` handles scene transitions; `registerAll()` handles hot reload (F5/script reload). They are independent code paths.
**How to avoid:** The fix must appear in BOTH functions. Verify by checking both call sites: `setActiveScene()` at bindings.cpp:708 and `registerAll()` at bindings.cpp:435.
**Warning signs:** Success criterion 1 says "Switching scenes OR triggering hot reload" — both paths must be covered.

### Pitfall 2: Using `lua_warning()` (Lua 5.4 API on Lua 5.1)

**What goes wrong:** Compile failure on all three targets.
**Why it happens:** CONTEXT.md mentions `lua_warning()` as a candidate but flags it as version-dependent. The project uses Lua 5.1 universally. `lua_warning()` was introduced in Lua 5.4.
**How to avoid:** Use `printf()`. It is already the diagnostic output mechanism across the entire codebase (see bindings_engine.cpp:587-602, and engine.log binding).
**Warning signs:** If you see `lua_warning` in a diff targeting this project, flag it immediately.

### Pitfall 3: Touching the Wrong nil Return in lua_engine_scene_persist

**What goes wrong:** Adding the warning to the wrong early-return branch (invalid proxy check at lines 412-417 instead of no-SSM check at lines 419-422).
**Why it happens:** The function has three nil-return paths. The success criteria specifically calls out "called without SceneStateMachine context."
**How to avoid:** The no-SSM check is the second guard: `if (!b || !b->m_ssm)`. Only this path (or specifically the `!b->m_ssm` branch) should get the warning. The proxy-invalid path (lines 412-417) is expected behavior and should remain silent.

### Pitfall 4: Forgetting `m_followSpeed` (it doesn't need a reset)

**What goes wrong:** Unnecessary change that adds noise to the diff.
**Why it happens:** `m_followTargetProxy = nullptr` makes `tickCameraFollow()` return immediately at line 1022, so `m_followSpeed` is never read until a new `follow()` call sets it again. No reset needed.
**How to avoid:** Only clear `m_followTargetProxy`. Leave `m_followSpeed` alone.

## Code Examples

### Complete setActiveScene() After Fix

```cpp
// Source: src/scripting/bindings.cpp — setActiveScene()
void LuaBindings::setActiveScene(Scene* scene) {
    if (scene != m_activeScene) {
        // EVENT-04: Scene is changing -- clear event bus for the outgoing scene.
        m_eventBus.clearHandlers();
        // CAM-08: Clear cached camera pointer on scene change (Phase 44).
        m_activeCamera = nullptr;
        // ASYNC-03: clear coroutines on scene transition (prevent stale refs)
        clearCoroutines();
        clearTweens();     // TWEEN-02: clean slate on scene transition
        m_followTargetProxy = nullptr;  // DEBT-01: clear follow target on scene change
    }
    m_activeScene = scene;
}
```

### registerAll() Hot-Reload Cleanup Block After Fix

```cpp
// Source: src/scripting/bindings.cpp — registerAll() cleanup block ~line 477
    // EVENT-05: clear event bus handlers from previous load (hot-reload cleanup)
    m_eventBus.clearHandlers();
    m_eventBus.setLuaState(L);
    // ASYNC-03: clear coroutine pool on every hot-reload (clean slate)
    clearCoroutines();
    clearTweens();     // TWEEN-02: clean slate on every hot-reload
    m_followTargetProxy = nullptr;  // DEBT-01: clear follow target on hot reload
```

### lua_engine_scene_persist() No-SSM Guard After Fix

```cpp
// Source: src/scripting/bindings_engine.cpp — lua_engine_scene_persist() ~line 419
    LuaBindings* b = getBindings(L);
    if (!b || !b->m_ssm) {
        printf("[enjin] WARNING: engine.scene.persist() called without SceneStateMachine context — no-op\n");
        lua_pushnil(L);
        return 1;
    }
```

## State of the Art

| Old Behavior | New Behavior | Requirement | Impact |
|--------------|-------------|-------------|--------|
| `m_followTargetProxy` survives scene change | Cleared in `setActiveScene()` and `registerAll()` | DEBT-01 | Eliminates dangling raw pointer; `tickCameraFollow()` fast-returns instead of dereferencing stale proxy |
| `engine.scene.persist()` without SSM returns nil silently | Returns nil with `printf()` warning | DEBT-02 | Developer can diagnose misconfigured scene setup immediately |

## Existing Test Coverage

The following test files are directly relevant and should receive new test cases as part of this phase:

| Test File | Relevance | New Cases Needed |
|-----------|-----------|-----------------|
| `tests/camera_follow_test.cpp` | CAM-01/02 tests — no scene-change coverage | Add: "follow proxy cleared by setActiveScene"; "follow proxy cleared by registerAll" |
| `tests/hot_reload_test.cpp` | Hot reload cleanup tests — no follow-proxy coverage | Add: "follow proxy cleared on registerAll" (or place in camera_follow_test.cpp) |
| `tests/persistent_lua_test.cpp` | PERSIST-01..03 tests — no no-SSM warning case | Add: "persist() without SSM prints warning and returns nil" |

The test infrastructure (custom `ASSERT` macros, `LuaEngine` + `LuaBindings` fixture pattern) is well-established — new tests follow the same pattern as existing camera and persistent tests.

## Open Questions

1. **Warning message granularity for `!b` vs `!b->m_ssm`**
   - What we know: Both conditions collapse to the same nil return. The `!b` case is an engine bug (null bindings — should never happen in production). The `!b->m_ssm` case is a legitimate developer mistake (using persist() in a standalone SDL scene without SSM).
   - What's unclear: Whether to warn on `!b` (engine bug) or only on `!b->m_ssm` (user error).
   - Recommendation: Print the warning for the combined guard `!b || !b->m_ssm`. Both cases are non-obvious failures. The simpler combined check is consistent with other guards in the file. Splitting would add complexity for minimal benefit.

2. **Printf format — stderr vs stdout**
   - What we know: The existing `lua_print` binding uses `printf()` (stdout). Engine log binding also uses `printf()`.
   - What's unclear: Whether warnings should go to `stderr` to distinguish them from Lua print output.
   - Recommendation: Use `printf()` to `stdout` for consistency with all other diagnostic output in bindings_engine.cpp. If `fprintf(stderr, ...)` is preferred, that is a trivially-adjusted style choice — defer to planner.

## Sources

### Primary (HIGH confidence)

- Direct source inspection: `/home/unwn/git/enjin/src/scripting/bindings.cpp` — `setActiveScene()` (line 708), `registerAll()` (line 435), cleanup patterns
- Direct source inspection: `/home/unwn/git/enjin/src/scripting/bindings_engine.cpp` — `lua_engine_scene_persist()` (line 407), `lua_engine_camera_follow()` (line 990), `tickCameraFollow()` (line 1021)
- Direct source inspection: `/home/unwn/git/enjin/include/enjin2/scripting/bindings.hpp` — `m_followTargetProxy` declaration (line 450), `ObjectProxy*` type
- Direct source inspection: `/home/unwn/git/enjin/build/wasm/_deps/lua51-src/src/lua.h` — confirms `LUA_VERSION "Lua 5.1"`, `LUA_VERSION_NUM 501`
- Direct source inspection: `/home/unwn/git/enjin/CMakeLists.txt` — Lua 5.1.5 for WASM/ESP32, LuaJIT (Lua 5.1 API) for desktop

### Secondary (MEDIUM confidence)

- Test file inventory: `tests/camera_follow_test.cpp`, `tests/hot_reload_test.cpp`, `tests/persistent_lua_test.cpp` — confirms no existing coverage of scene-change proxy clearing or no-SSM warning

## Metadata

**Confidence breakdown:**
- Fix locations: HIGH — verified by direct source reading; exact line numbers confirmed
- Lua version: HIGH — verified from lua.h in build artifacts and CMakeLists.txt
- `lua_warning()` unavailability: HIGH — Lua 5.1 API confirmed; no `lua_warning` symbol anywhere in project
- Test gap identification: HIGH — grepped all relevant test files; no coverage of the specific behaviors being added
- Warning message wording: MEDIUM — open question on stderr vs stdout; both are correct

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable codebase; no external dependencies changing)
