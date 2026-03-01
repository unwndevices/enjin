# Phase 49: Coroutine/Async Scheduler - Research

**Researched:** 2026-03-01
**Domain:** Lua coroutine API, fixed-slot C scheduler, per-frame dt accumulation, ESP32 library registration
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Pool exhaustion policy:**
- 8 fixed coroutine slots
- When all slots are full, `engine.async.start()` returns nil instead of an ID
- Scripts that care can check: `local id = engine.async.start(fn); if not id then ... end`
- No Lua error raised — consistent with sprite pool, event bus, and LuaStore overflow behavior
- No introspection API (no `slots()` query) — keep API minimal

**Scene transition behavior:**
- Cancel all coroutines on scene transition (`clearCoroutines()`)
- Same cleanup on hot-reload (F5)
- Persistent objects keep running but their async tasks do NOT survive transitions
- Clean slate per scene — prevents stale callback refs to destroyed objects

### Claude's Discretion

- CoroutineSlot struct layout and state machine (waiting, running, dead)
- Where in the SDL main loop to call `tickCoroutines(dt)` — before or after scene update
- `wait()` precision model (frame-accumulated dt)
- ESP32 `luaopen_coroutine` integration details
- Thread ref lifecycle (`luaL_ref`/`luaL_unref` management)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ASYNC-01 | `engine.async.start(fn)` registers coroutine in 8-slot scheduler | Fixed-slot pool pattern from sprite pool / event bus; `lua_newthread` + `luaL_ref` for thread anchor |
| ASYNC-02 | `engine.async.wait(seconds)` yields coroutine and resumes after delay | `lua_yield` from C binding; dt accumulation in `CoroutineSlot.waitRemaining`; `lua_resume` outside pcall |
| ASYNC-03 | `engine.async.cancel(id)` and `engine.async.cancelAll()` cleanup | `luaL_unref(threadRef)` pattern from C_Timer; `lua_closethread` (Lua 5.4) or just abandon |
| ASYNC-04 | Coroutine library opened on ESP32 in `openEmbeddedLibraries()` | `luaopen_coroutine` exists in standard Lua 5.x; LuaJIT bundles it in `luaopen_base` |
</phase_requirements>

---

## Summary

This phase adds a cooperative coroutine scheduler to enjin2 as `engine.async.*` Lua bindings backed by an 8-slot fixed array in `LuaBindings`. The implementation follows the same zero-allocation, fixed-pool pattern already established by the sprite pool (16 slots), event bus (16 channels / 8 subscribers), LuaStore (16 keys), and C_Timer (8 timers per component).

The critical architectural decision — confirmed in STATE.md — is that coroutines are resumed via `lua_resume` from C **outside any `lua_pcall` scope**. This avoids the yield-across-pcall boundary limitation present in standard Lua 5.1/LuaJIT-without-CoCo. The SDL runner's current game loop calls `lua_pcall` for `update()` and `draw()`; `tickCoroutines()` must be added as a **separate C call** after those pcall scopes close, not inside them.

The second critical fact: the desktop/SDL build links system **Lua 5.4** (confirmed: `find_package(Lua)` finds Lua 5.4.8), while WASM links LuaJIT 2.x. The `lua_resume` signature differs between them — `lua_resume(L, from, narg, &nresults)` in Lua 5.4 vs `lua_resume(L, narg)` in LuaJIT. A `LUA_VERSION_NUM` guard is required, following the existing `lua_pcallk` compatibility pattern already in `lua_platform.hpp`.

**Primary recommendation:** Add `LuaCoroutineScheduler` as member data in `LuaBindings` (same class that owns m_eventBus, m_store, sprite pool). Tick it from `sdl_main.cpp` using an inline `tickCoroutines(dt)` call between `update()` and `draw()` — not inside either pcall scope.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua 5.4 C API | 5.4.8 (desktop) | `lua_newthread`, `lua_resume`, `lua_yield`, `luaL_ref` | Already linked as enjin2_lua dependency |
| LuaJIT 2.1 | bundled (`luajit/src`) | WASM target; same API for coroutines as Lua 5.1 | Already bundled in project |
| Lua 5.4 coroutine lib | 5.4.8 | Desktop: opened by `luaL_openlibs()`; ESP32: must open explicitly | Needed for Lua-side `coroutine.*` access (not required if only using C API) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `luaopen_coroutine` (Lua 5.x) | standard lib | Makes `coroutine.*` table available to Lua scripts | Required on ESP32 where `luaL_openlibs` is not called |
| `LUA_COLIBNAME` constant | lualib.h | Identifies the coroutine library name `"coroutine"` | Used with `luaL_requiref` in `openEmbeddedLibraries()` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `lua_newthread` per slot | `lua_State* lua_newstate()` | `lua_newthread` creates a child thread sharing the parent state; `lua_newstate` is a fully independent state — wrong for coroutines |
| `luaL_ref` for thread anchor | Raw `lua_State*` storage | `lua_State*` from `lua_newthread` is NOT GC-rooted unless anchored via registry ref; GC would collect it |
| `lua_resume` outside pcall | `coroutine.resume()` from Lua | C-side resume allows precise per-frame control; Lua-side resume would require the scheduler to be scripted |

**Installation:** No new libraries — uses existing Lua dependency already linked as `enjin2_lua`.

---

## Architecture Patterns

### Recommended Project Structure

New file: `src/scripting/bindings_async.cpp`
Modified: `include/enjin2/scripting/bindings.hpp` (add pool member + public methods)
Modified: `src/scripting/bindings_engine.cpp` (`registerEngineTable` adds `engine.async` subtable)
Modified: `src/platform/sdl/sdl_main.cpp` (`tickCoroutines` call in game loop)
Modified: `src/scripting/lua_platform.cpp` (`openEmbeddedLibraries` adds coroutine lib)
Modified: `src/scripting/bindings.cpp` (`registerAll` clears pool; `clearCoroutines` called from `setActiveScene`)
New test: `tests/coroutine_async_test.cpp`

### Pattern 1: CoroutineSlot Struct (Claude's Discretion)

**What:** Each of the 8 slots holds a thread ref (anchored in Lua registry), wait timer, slot ID, and state.
**When to use:** Always — this is the core data structure.

```cpp
// Source: modeled after C_Timer::TimerEntry (include/enjin2/components/timer.hpp)
struct CoroutineSlot {
    int     threadRef{LUA_NOREF};  // luaL_ref handle anchoring the thread; LUA_NOREF = inactive
    float   waitRemaining{0.0f};   // seconds until next resume (0 = ready to resume now)
    int     id{0};                 // monotonically increasing cancel ID returned to Lua
    bool    active{false};         // slot in use
};
```

### Pattern 2: `lua_newthread` + `luaL_ref` for GC Anchoring

**What:** `lua_newthread(L)` creates a new coroutine thread on the main state's stack. Without anchoring, the GC can collect it. `luaL_ref(L, LUA_REGISTRYINDEX)` pops it off the stack and stores it in the registry, preventing GC.
**When to use:** In `engine.async.start()` binding.

```cpp
// Source: Lua 5.4 Reference Manual §4.6 + C_Timer::scheduleAfter pattern
// Stack on entry to lua_engine_async_start: [1]=function
lua_State* L = /* main state */;
luaL_checktype(L, 1, LUA_TFUNCTION);

// Find a free slot
int slotIdx = /* linear scan for first !active slot */;
if (slotIdx < 0) { lua_pushnil(L); return 1; }  // pool full -> nil

// Create a new coroutine thread
lua_State* co = lua_newthread(L);         // pushes thread onto L's stack
// Push the function onto the coroutine's stack
lua_pushvalue(L, 1);                      // copy function to top of L
lua_xmove(L, co, 1);                     // move function from L to co's stack

// Anchor the thread in registry so GC won't collect it
int threadRef = luaL_ref(L, LUA_REGISTRYINDEX);  // pops thread from L; stores in registry

CoroutineSlot& slot = m_coroutinePool[slotIdx];
slot.threadRef    = threadRef;
slot.waitRemaining = 0.0f;
slot.id           = ++m_nextCoroutineId;
slot.active       = true;

// The coroutine starts on the NEXT tickCoroutines call (not immediately)
// This matches the per-frame tick model
lua_pushinteger(L, static_cast<lua_Integer>(slot.id));
return 1;
```

### Pattern 3: Per-Frame `tickCoroutines(dt)` Resume Loop

**What:** Called once per frame from the SDL game loop, OUTSIDE any pcall scope. Iterates active slots, decrements `waitRemaining`, and calls `lua_resume` when ready.
**When to use:** After `update()` pcall closes, before `draw()` pcall. Or after both — see Pitfall 2 for discussion.

```cpp
// Source: modeled after C_Timer::update() in timer.cpp
void LuaBindings::tickCoroutines(float dt) {
    lua_State* L = engine->getState();
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = m_coroutinePool[i];
        if (!slot.active) continue;

        slot.waitRemaining -= dt;
        if (slot.waitRemaining > 0.0f) continue;

        // Retrieve the thread from the registry
        lua_rawgeti(L, LUA_REGISTRYINDEX, slot.threadRef);
        lua_State* co = lua_tothread(L, -1);
        lua_pop(L, 1);
        if (!co) { /* dead/invalid — clean up slot */ clearSlot(slot); continue; }

        // Resume the coroutine (pass 0 args on first start or after yield)
#if LUA_VERSION_NUM >= 504
        int nres = 0;
        int status = lua_resume(co, L, 0, &nres);
        if (nres > 0) lua_pop(co, nres);  // discard any yielded/returned values
#else
        // LuaJIT / Lua 5.1: lua_resume(L, narg)
        int status = lua_resume(co, 0);
#endif
        if (status == LUA_YIELD) {
            // Coroutine yielded — waitRemaining was set by wait() before yield
            // (wait() binding pushes the seconds value, which tickCoroutines reads back)
            // OR: wait() sets slot.waitRemaining BEFORE calling lua_yield
            // See Pitfall 3 for the wait() design choice
        } else if (status == LUA_OK) {
            // Coroutine returned normally — slot is done
            clearSlot(slot, L);
        } else {
            // Error — print and clean up
            const char* err = lua_tostring(co, -1);
            printf("[async error] %s\n", err ? err : "unknown");
            clearSlot(slot, L);
        }
    }
}
```

### Pattern 4: `wait()` Binding — Setting `waitRemaining` Before Yield

**What:** `engine.async.wait(seconds)` finds the calling slot, sets `waitRemaining`, then calls `lua_yield`.
**Key insight:** The wait() C function runs inside the coroutine's execution context. It can set the slot's `waitRemaining` BEFORE calling `lua_yield`, so `tickCoroutines` sees the updated value on the next frame.

```cpp
// Source: Lua 5.4 Reference Manual §4.6 — lua_yield is called from inside a C function
// running in a coroutine context; this is safe and does NOT cross a pcall boundary
// because tickCoroutines uses lua_resume (not pcall)
int LuaBindings::lua_engine_async_wait(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return lua_yield(L, 0);

    float seconds = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    if (seconds < 0.0f) seconds = 0.0f;

    // Identify which slot is running this coroutine
    // (L here is the coroutine's lua_State, not the main state)
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = b->m_coroutinePool[i];
        if (!slot.active) continue;
        lua_rawgeti(b->engine->getState(), LUA_REGISTRYINDEX, slot.threadRef);
        lua_State* co = lua_tothread(b->engine->getState(), -1);
        lua_pop(b->engine->getState(), 1);
        if (co == L) {
            slot.waitRemaining = seconds;  // set BEFORE yield
            break;
        }
    }
    return lua_yield(L, 0);  // suspend; returns to tickCoroutines
}
```

**Alternative (simpler):** Pass seconds as a yield value and read it back in tickCoroutines. However, the slot-scanning approach above is self-contained and avoids complex value passing between `co` and `L` stacks.

### Pattern 5: `clearCoroutines()` — Matching `clearHandlers()` Pattern

**What:** Unref all active thread refs, deactivate all slots. Called on scene transition and hot-reload.
**When to use:** In `setActiveScene()` (already calls `m_eventBus.clearHandlers()`) and in `registerAll()` (already called from `performReload`).

```cpp
// Source: modeled after LuaEventBus::clearHandlers() in lua_event_bus.cpp
void LuaBindings::clearCoroutines() {
    lua_State* L = engine ? engine->getState() : nullptr;
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = m_coroutinePool[i];
        if (!slot.active) continue;
        if (L && slot.threadRef != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, slot.threadRef);
        }
        slot = CoroutineSlot{};  // zero-reset
    }
    m_nextCoroutineId = 1;
}
```

### Pattern 6: ESP32 `luaopen_coroutine`

**What:** On ESP32, `openEmbeddedLibraries()` calls `luaL_requiref` for each library instead of `luaL_openlibs`. The coroutine library must be added here.
**Key finding:** `luaopen_coroutine` is declared in `lualib.h` for standard Lua 5.x. On LuaJIT, coroutines are included in `luaopen_base`. Since ESP32 uses standard Lua (not LuaJIT per the CMakeLists.txt comments), `luaopen_coroutine` is available.

```cpp
// Source: existing openEmbeddedLibraries pattern in lua_platform.cpp:166-194
// Add after the table library:
luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
lua_pop(L, 1);
```

### Pattern 7: `lua_resume` API Compatibility Guard

**What:** Lua 5.4 changed `lua_resume` signature — added `from` and `*nresults` parameters. LuaJIT (Lua 5.1) uses the old two-argument form.

```cpp
// Source: confirmed from /usr/include/lua.h (5.4) vs luajit/src/lua.h (5.1)
// Existing precedent: lua_platform.hpp already has lua_pcallk compat guard
#if LUA_VERSION_NUM >= 504
    int nres = 0;
    int status = lua_resume(co, L, 0, &nres);
    if (nres > 0) lua_pop(co, nres);
#else
    // LuaJIT / Lua 5.1
    int status = lua_resume(co, 0);
    // nres not available; check co stack with lua_gettop(co) if needed
    if (lua_gettop(co) > 0) lua_settop(co, 0);
#endif
```

### Anti-Patterns to Avoid

- **Resuming inside `lua_pcall`:** Calling `lua_resume` while a `lua_pcall` is on the C call stack causes "attempt to yield across C-call boundary" in Lua 5.1/LuaJIT without CoCo. Always call `tickCoroutines` as a bare C call, not nested inside pcall.
- **Not anchoring the thread:** Storing only the raw `lua_State*` pointer from `lua_newthread` without a `luaL_ref` allows GC to collect the thread, causing a dangling pointer crash.
- **Running coroutines after `lua_close`:** The `clearCoroutines()` call in `registerAll()` handles the hot-reload case, but must use the old `lua_State*` before `engine->shutdown()` closes it. Follow the `clearHandlers()` pattern: call `clearCoroutines()` at the top of `registerAll()` BEFORE `engine->getState()` might return a new state.
- **Calling `luaopen_coroutine` on desktop:** On desktop, `luaL_openlibs()` already opens all libraries including coroutine. Only add `luaopen_coroutine` in the `#ifdef ESP32` path.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Coroutine state machine | Custom fiber/continuation system | Lua's built-in coroutine API (`lua_newthread`, `lua_resume`, `lua_yield`) | Lua VM handles stack save/restore; coroutines are a core Lua feature |
| Cross-frame waiting | Polling in update() | `lua_yield` + dt accumulation in slot | yield suspends execution at exact call site; no polling needed |
| Thread anchoring | Raw `lua_State*` storage | `luaL_ref` + registry | GC will collect unanchored threads; registry ref prevents collection |

**Key insight:** The Lua coroutine API is the complete solution for cooperative scheduling. The C layer only needs to manage which slot to resume and when — the VM handles everything else.

---

## Common Pitfalls

### Pitfall 1: Yield-Across-C-Boundary (CRITICAL)
**What goes wrong:** `lua_yield` called from a C function that was itself called via `lua_pcall` (not `lua_resume`) raises "attempt to yield across metamethod/C-call boundary" in Lua 5.1.
**Why it happens:** `lua_pcall` installs a C error handler; yielding through it is unsupported in Lua 5.1/LuaJIT without CoCo.
**How to avoid:** Call `tickCoroutines(dt)` from `sdl_main.cpp` as a plain C call — NOT inside the `lua_pcall(update, ...)` or `lua_pcall(draw, ...)` scopes. The current SDL runner structure makes this straightforward: add `tickCoroutines` after the update pcall block closes.
**Warning signs:** Error message "attempt to yield across C-call boundary" or "attempt to yield from outside a coroutine".

### Pitfall 2: Coroutine Tick Ordering
**What goes wrong:** If `tickCoroutines` fires before `update()`, coroutines that call engine functions (draw, set positions) may run before the frame's state is set up. If it fires after `draw()`, graphical updates from coroutines miss the current frame.
**Recommendation (Claude's Discretion):** Call `tickCoroutines` **between `update()` and `draw()`** — after game logic runs, before rendering. This matches the natural intuition: coroutine side-effects are visible in the same frame's draw pass.

### Pitfall 3: wait() Slot Identification
**What goes wrong:** `engine.async.wait()` receives a `lua_State* L` that is the coroutine's state (not the main state). Finding which `CoroutineSlot` owns `L` requires iterating the pool.
**How to avoid:** Iterate the slot array comparing `lua_tothread` result against `L`. With only 8 slots, this is O(8) — acceptable. Alternative: push slot index as an upvalue into the wait function at start time, but this complicates binding registration.
**Warning signs:** wait() silently no-ops if `L` doesn't match any active slot (e.g., called from non-coroutine context). A guard check — `lua_isyieldable(L)` returning false — can be used to print a warning.

### Pitfall 4: Double-Unref on Hot-Reload
**What goes wrong:** If `clearCoroutines()` is called after `engine->shutdown()` (which calls `lua_close`), `luaL_unref` operates on a closed `lua_State` — undefined behavior / crash.
**How to avoid:** Call `clearCoroutines()` BEFORE `lua.shutdown()` in `performReload`. The existing pattern in `sdl_main.cpp` calls `lua.shutdown()` which calls `engine.shutdown()`. Add `clearCoroutines()` to `LuaBindings::registerAll()` at the top — this runs early in the reload sequence while the old state is still open. Alternatively, add it to `performReload` before `lua.shutdown()` by exposing a public `clearCoroutines()` method.
**Warning signs:** ASAN use-after-free or crash on F5 reload.

### Pitfall 5: ESP32 LuaJIT vs Standard Lua
**What goes wrong:** Assuming ESP32 uses LuaJIT and searching for `luaopen_coroutine` in LuaJIT headers — it's not there as a standalone function (coroutines bundled in `luaopen_base` in LuaJIT).
**Why it matters:** The CMakeLists.txt ESP32 path uses user-provided `LUA_LIBRARIES` — typically standard Lua (not LuaJIT) on ESP32. Standard Lua 5.x exports `luaopen_coroutine` separately.
**How to avoid:** Use `luaopen_coroutine` in the `#ifdef ESP32` path. On LuaJIT (WASM path), `luaL_openlibs` or `luaopen_base` covers it. The existing `lua_platform.cpp` ESP32 section opens base/math/string/table — add coroutine after table.

### Pitfall 6: `lua_resume` API Difference (Lua 5.4 vs LuaJIT)
**What goes wrong:** Using `lua_resume(co, 0)` compiles fine against LuaJIT headers (Lua 5.1) but fails against Lua 5.4 headers (needs `from` and `*nresults` params).
**How to avoid:** Use `#if LUA_VERSION_NUM >= 504` guard. The existing `lua_platform.hpp` already does this for `lua_pcallk` — follow the same pattern in `bindings_async.cpp`.

### Pitfall 7: coroutine.yield() from Non-Coroutine Context
**What goes wrong:** A Lua script calls `engine.async.wait()` from outside a coroutine (e.g., in `update()` directly). `lua_isyieldable(L)` returns false; calling `lua_yield` anyway causes an error.
**How to avoid:** Guard `lua_engine_async_wait` with `lua_isyieldable(L)` check. If not yieldable, push a Lua error message and return 0. Document in API: `wait()` must be called from inside a coroutine started with `engine.async.start()`.

---

## Code Examples

Verified patterns from codebase + Lua reference:

### Complete `engine.async.start()` binding (simplified)
```cpp
// File: src/scripting/bindings_async.cpp
// Pattern from: LuaEventBus::subscribe + C_Timer::scheduleAfter
int LuaBindings::lua_engine_async_start(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { lua_pushnil(L); return 1; }
    luaL_checktype(L, 1, LUA_TFUNCTION);

    // Find free slot
    int slotIdx = -1;
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        if (!b->m_coroutinePool[i].active) { slotIdx = i; break; }
    }
    if (slotIdx < 0) { lua_pushnil(L); return 1; }  // pool full

    // Create coroutine thread, push function onto it
    lua_State* co = lua_newthread(L);   // [... thread]
    lua_pushvalue(L, 1);                // [... thread fn]
    lua_xmove(L, co, 1);               // fn is now on co's stack

    // Anchor thread in registry
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);  // pops thread

    CoroutineSlot& slot = b->m_coroutinePool[slotIdx];
    slot.threadRef     = ref;
    slot.waitRemaining = 0.0f;   // start immediately (first tick)
    slot.id            = ++b->m_nextCoroutineId;
    slot.active        = true;

    lua_pushinteger(L, static_cast<lua_Integer>(slot.id));
    return 1;
}
```

### `engine.async.cancel(id)` binding
```cpp
int LuaBindings::lua_engine_async_cancel(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;
    int cancelId = static_cast<int>(luaL_checkinteger(L, 1));
    for (int i = 0; i < COROUTINE_POOL_SIZE; ++i) {
        CoroutineSlot& slot = b->m_coroutinePool[i];
        if (slot.active && slot.id == cancelId) {
            luaL_unref(L, LUA_REGISTRYINDEX, slot.threadRef);
            slot = CoroutineSlot{};
            break;
        }
    }
    return 0;
}
```

### `registerAll()` additions (initialization and cleanup)
```cpp
// In LuaBindings::registerAll() — add alongside m_eventBus.clearHandlers()
clearCoroutines();  // ASYNC-03: clean slate on every hot-reload
// (m_L is still valid here — clearCoroutines uses engine->getState() before any reload)
```

### `sdl_main.cpp` game loop additions (between update and draw)
```cpp
// In the lua_ok section, between update pcall and draw pcall:
// Tick camera follow after Lua update (Phase 48: CAM-01)
g_lua.getBindings().tickCameraFollow(dt);
// Tick coroutine scheduler (Phase 49: ASYNC-01..ASYNC-04)
g_lua.getBindings().tickCoroutines(dt);
```

### Registration in `registerEngineTable()` (bindings_engine.cpp)
```cpp
// --- engine.async sub-table (Phase 49: ASYNC-01..ASYNC-04) ---
static const LuaFuncDef kAsyncFuncs[] = {
    {"start",     lua_engine_async_start},
    {"cancel",    lua_engine_async_cancel},
    {"cancelAll", lua_engine_async_cancelAll},
    {"wait",      lua_engine_async_wait},
};
lua_newtable(L);
luaBindFunctions(L, -1, kAsyncFuncs, ENJIN_ARRAY_LEN(kAsyncFuncs));
lua_setfield(L, -2, "async");
```

### Sample Lua usage (for integration test)
```lua
-- Test: wait() pauses and resumes correctly
local resumed = false
engine.async.start(function()
    engine.async.wait(0.5)
    resumed = true
end)

-- After 0.4s (4 frames at 0.1s dt): resumed should still be false
-- After 0.5s (5 frames): resumed should be true
```

### `openEmbeddedLibraries` ESP32 addition
```cpp
// In lua_platform.cpp openEmbeddedLibraries(), after table library:
luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
lua_pop(L, 1);
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Lua 5.4 `lua_resume(L, from, narg, &nresults)` requires from+nresults | `#if LUA_VERSION_NUM >= 504` guard needed | Lua 5.4 (2020) | Must use compat guard; LuaJIT still uses old 2-arg form |
| LuaJIT requires CoCo extension for yield-from-pcall | Resume outside pcall avoids this entirely | Design decision | Scheduler must call `lua_resume` bare (not inside pcall) |

**Deprecated/outdated:**
- `coroutine.running()` pattern for self-identification: fragile, not needed — slot scanning by `lua_State*` comparison is reliable with 8 slots.

---

## Open Questions

1. **`tickCoroutines` before or after `draw()`?**
   - What we know: calling before `draw()` means coroutine state changes are visible to the draw call in the same frame; calling after means one-frame lag.
   - What's unclear: whether game scripts ever rely on synchronous ordering between coroutine effects and draw.
   - Recommendation: place between `update()` and `draw()` (before draw) — gives maximum frame coherence. Planner should make this explicit in the plan.

2. **`wait()` with dt=0 (first frame start)**
   - What we know: `waitRemaining` initializes to `0.0f`, so `tickCoroutines` fires the coroutine on the SAME frame as `start()` if dt check is `<= 0`.
   - What's unclear: user expectation — does `start(fn)` run `fn` immediately in the same frame, or next frame?
   - Recommendation: run on the SAME frame as start (consistent with "start immediately"). Document this. A `waitRemaining = 0.0f` slot passes the `<= 0` threshold immediately.

3. **`engine.async.wait(0)` behavior**
   - What we know: wait(0) should yield for at least one frame (give control back even with 0 delay).
   - Recommendation: `if (seconds <= 0.0f) seconds = 0.0f; slot.waitRemaining = seconds;` — a 0-second wait yields once and resumes next frame when `waitRemaining` is re-decremented to <= 0 by dt (which is always > 0 for a real frame).

---

## Validation Architecture

`workflow.nyquist_validation` is not present in `.planning/config.json` (no such key). Skipping Validation Architecture section per instructions.

Test infrastructure: existing pattern — standalone C++ executables in `tests/`, linked against `enjin2_lua`, added to `tests/CMakeLists.txt`, registered via `add_test`.

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Test File | Notes |
|--------|----------|-----------|-----------|-------|
| ASYNC-01 | `start(fn)` returns integer ID; coroutine executes next frame | integration | `coroutine_async_test.cpp` | Use `C_LuaScript` fixture or `LuaScriptSystem` |
| ASYNC-01 | Pool full: 8 active coroutines, 9th `start()` returns nil | unit | `coroutine_async_test.cpp` | Same file, separate test function |
| ASYNC-02 | `wait(seconds)` pauses coroutine for specified duration | integration | `coroutine_async_test.cpp` | Tick multiple frames, check resume timing |
| ASYNC-03 | `cancel(id)` stops coroutine; `cancelAll()` stops all | unit | `coroutine_async_test.cpp` | Verify slot cleared, no double-unref |
| ASYNC-04 | Coroutine library available on ESP32 | compile-time | ESP32 build / manual check | `luaopen_coroutine` guard in `openEmbeddedLibraries` |

### Test File Structure (for `coroutine_async_test.cpp`)

```
tests/coroutine_async_test.cpp
  test_async01_start_returns_id          — start() returns integer
  test_async01_coroutine_runs_next_frame — body executes on tick
  test_async01_pool_overflow_returns_nil — 9th start() returns nil
  test_async02_wait_resumes_after_delay  — wait(0.5) pauses 5 frames at dt=0.1
  test_async02_multiple_waits           — coroutine with multiple sequential waits
  test_async03_cancel_by_id             — cancel(id) prevents execution
  test_async03_cancel_all               — cancelAll() clears all slots
  test_async03_clear_on_hot_reload      — clearCoroutines via registerAll
```

---

## Sources

### Primary (HIGH confidence)
- Source inspection: `src/scripting/bindings_engine.cpp` — engine subtable registration pattern (`lua_newtable`, `luaBindFunctions`, `lua_setfield`)
- Source inspection: `include/enjin2/components/timer.hpp` — `TimerEntry` struct layout, `luaL_ref` pattern, `clearTimers()` pattern
- Source inspection: `include/enjin2/scripting/lua_event_bus.hpp` — `clearHandlers()` pattern for hot-reload cleanup
- Source inspection: `src/scripting/lua_event_bus.cpp` — `luaL_unref` cleanup in `clearHandlers()`
- Source inspection: `src/platform/sdl/sdl_main.cpp` — game loop structure; `tickCameraFollow` placement as reference for `tickCoroutines`
- Source inspection: `src/scripting/lua_platform.cpp:166-194` — `openEmbeddedLibraries` pattern on ESP32
- Source inspection: `include/enjin2/scripting/lua_platform.hpp` — `lua_pcallk` compat guard pattern (`#if LUA_VERSION_NUM < 502`)
- API verification: `/usr/include/lua.h` (Lua 5.4.8) — `lua_resume(L, from, narg, *nresults)` signature
- API verification: `/home/unwn/dev/enjin/luajit/src/lua.h` (LuaJIT 2.1/Lua 5.1) — `lua_resume(L, narg)` signature
- API verification: `/usr/include/lualib.h` — `luaopen_coroutine` declared for standard Lua 5.x
- API verification: `/home/unwn/dev/enjin/luajit/src/lualib.h` — `luaopen_coroutine` NOT declared; bundled in `luaopen_base`
- Live test: Lua 5.4 coroutine yield-from-C-inside-pcall behavior verified in shell

### Secondary (MEDIUM confidence)
- Source inspection: `src/scripting/bindings.cpp:435-646` (`registerAll`) — initialization sequence, `m_eventBus.clearHandlers()` placement
- Source inspection: `src/scripting/bindings.cpp:705-715` (`setActiveScene`) — where clearHandlers is called for scene transitions
- CMakeLists.txt analysis: Desktop SDL build links system Lua 5.4 (not LuaJIT); WASM links LuaJIT 2.1

### Tertiary (LOW confidence)
- None — all claims verified against source code or live system.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — project already links Lua; no new dependencies
- Architecture: HIGH — direct analogy to existing C_Timer, LuaEventBus, sprite pool patterns
- Pitfalls: HIGH — yield-across-pcall verified by existing STATE.md flag; lua_resume signature verified against actual headers
- ESP32 integration: MEDIUM — `luaopen_coroutine` pattern confirmed in lualib.h, but ESP32 build not physically tested

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable APIs; project pattern is stable)
