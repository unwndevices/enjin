# Phase 26: Lua Hot Reload - Research

**Researched:** 2026-02-26
**Domain:** C++ Lua state lifecycle, SDL3 event handling, game-loop state machines
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Error display**
- Errors print to console/stdout only — no in-window overlay or text rendering
- Format: file + line + message (e.g. `[reload error] game.lua:42: attempt to call nil value`)
- No timestamp or reload attempt number — keep it minimal
- Syntax vs runtime error distinction: Claude's discretion

**State reset scope**
- Full clear: all canvas/layer contents wiped on reload — clean slate
- Layer count resets to default — destroy all layers, script re-creates what it needs
- Window size stays as-is — no resolution reset on reload
- Game-loop state fully resets — tick count back to 0, delta time accumulator reset

**Reload feedback**
- Successful reload prints: `[reload] script.lua` — simple one-liner, no timing info
- No visual feedback in the window — canvas clears and script starts drawing immediately
- Always reload on F5 regardless of whether file changed — no file-modification-time checking
- No debounce — each F5 triggers a full reload (idempotent by design)

**Error recovery flow**
- After failed reload: canvas is blank (cleared), not frozen on last frame
- Game loop pauses on error — stop calling update/draw, just poll for input (F5 to retry)
- Recovery via F5 is identical to normal reload — no special recovery path or messages
- Initial startup failure behaves the same as reload failure — window opens, error prints, loop pauses, F5 to retry

### Claude's Discretion
- Whether to label syntax vs runtime errors differently in console output
- Internal implementation of loop pause (SDL event polling details)

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| HOT-01 | F5 key in SDL3 runner triggers Lua script reload from disk | SDL3 `SDL_EVENT_KEY_DOWN` + `SDLK_F5` in existing event loop; script path must be stored |
| HOT-02 | Reload performs full reset (Lua state destroyed and recreated, all bindings re-registered) | `LuaEngine::shutdown()` + `LuaEngine::initialize()` already exist; `LuaBindings::registerAll()` + `setLayers()` re-wire everything |
| HOT-03 | Reload error (syntax/runtime) displays error message without crashing the runner | `LuaResult.success + .error` already captures errors; loop pause state machine keeps window alive |
</phase_requirements>

## Summary

Phase 26 implements manual F5 hot-reload for the SDL3 Lua runner. The implementation is entirely self-contained in `src/platform/sdl/sdl_main.cpp` (and no other platform file). The Lua lifecycle APIs already exist — `LuaEngine::shutdown()` tears down the Lua state and `LuaEngine::initialize()` creates a fresh one; `LuaBindings::registerAll()` and `setLayers()` re-wire bindings to the same static canvas objects, which remain valid across reloads. The hard technical work is two design problems: (1) the `LuaCallback` (std::function) dangling-pointer bug documented in STATE.md must be fixed before reload is layered on top, and (2) the game loop needs a simple paused/running state machine to implement "pause on error, resume on F5".

The WASM and ESP32 constraint is automatically satisfied because hot-reload lives entirely inside `#ifdef ENJIN2_BUILD_SDL` and the existing `#ifdef ENJIN2_BUILD_LUA` guards. No new preprocessor symbols are needed and no platform code is touched.

**Primary recommendation:** Add a `performReload()` free function in sdl_main.cpp that encapsulates shutdown → initialize → registerAll → setLayers → loadScript. Drive it from the SDL event pump on `SDLK_F5`. Add a `bool g_lua_ok` flag to gate `update`/`draw` calls. Fix the `registerFunction(LuaCallback)` dangling pointer before anything else.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua (system) | 5.x (already linked) | Scripting runtime | Already in use via enjin2_lua |
| SDL3 | release-3.4.2 (FetchContent) | Event detection for F5 | Already in use |

### Supporting

No new libraries are needed. This phase is purely a restructuring of existing code.

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Full state teardown/recreate | Partial state reset (clear globals) | Full teardown is simpler, safer, and already decided by user |
| F5 via SDL_GetKeyboardState | F5 via SDL_EVENT_KEY_DOWN | KEY_DOWN is correct here — it fires once per press; GetKeyboardState fires every frame and would require its own edge-detect logic |

## Architecture Patterns

### Recommended Project Structure

Only `src/platform/sdl/sdl_main.cpp` changes. All other files are untouched.

```
src/platform/sdl/sdl_main.cpp   ← all changes live here
scripts/                        ← script path captured from argv[1] at startup
```

### Pattern 1: Script Path from argv

**What:** sdl_main currently hardcodes `"scripts/e2e_parity.lua"`. Hot-reload requires knowing the path at reload time. Store it in a `static std::string g_script_path`.

**When to use:** Always — this is required for HOT-01.

**Example:**

```cpp
// In main(), after arg parsing:
static std::string g_script_path = "scripts/layer_demo.lua";  // default
for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--script") == 0) {
        g_script_path = argv[i + 1];
    }
}
```

### Pattern 2: performReload() encapsulates the full lifecycle

**What:** A single function that runs the full reload sequence. Called at startup and on every F5.

**When to use:** Any time a reload is triggered — initial load and F5 are identical paths (user decision).

**Example:**

```cpp
// Returns true on success, false on load/syntax error.
// Prints [reload] or [reload error] to stderr.
static bool performReload(enjin2::LuaScriptSystem& lua,
                          enjin2::LuaCanvas** layers, uint8_t count,
                          bool* visible, enjin2::InputState* input,
                          const std::string& path)
{
    lua.shutdown();
    lua.initialize();                          // fresh lua_State
    lua.getBindings().registerAll();           // re-register all C functions
    lua.getBindings().setLayers(layers, count, visible);  // re-wire layer canvases
    lua.getBindings().setInput(input);
    enjin2::LuaResult r = lua.loadScript(path);
    if (r.success) {
        std::cerr << "[reload] " << path << "\n";
        return true;
    } else {
        std::cerr << "[reload error] " << r.error << "\n";
        return false;
    }
}
```

**Critical detail:** `layerCanvases` (the four `static enjin2::LuaCanvas` objects and the pointer array) are `static` locals in `main()` and stay valid across `lua.shutdown()`/`lua.initialize()`. The `LuaCanvas` objects hold raw pointers into `g_compositor.layers[]` which is also a static global — no lifetime issue.

### Pattern 3: g_lua_ok state flag drives loop behavior

**What:** A boolean flag `g_lua_ok` (or equivalent) that the loop checks before calling `update`/`draw`. When false, the loop only polls events and renders the blank canvas.

**When to use:** Whenever `performReload()` returns false.

**Example:**

```cpp
static bool g_lua_ok = false;  // set by performReload() result

// In game loop:
if (g_lua_ok) {
    enjin2::LuaResult r = g_lua.callFunction("update", dt);
    if (!r.success) {
        std::cerr << "[lua error] " << r.error << "\n";
        g_lua_ok = false;  // enter paused state
    }
    // draw() call similar
}
```

**Loop behavior in paused state:** Still calls `g_compositor.clearAll()`, `g_compositor.composite()`, and `SDL_RenderPresent` — the window stays open and blank. Only skips the Lua `update`/`draw` calls.

### Pattern 4: F5 detection in SDL event pump

**What:** Check for `SDLK_F5` in the `SDL_EVENT_KEY_DOWN` branch of the existing event loop.

**Why `KEY_DOWN` not `GetKeyboardState`:** `KEY_DOWN` fires once per keypress. `GetKeyboardState` is polled every frame and would require manual edge-detection to avoid re-triggering. `KEY_DOWN` is the correct mechanism for a "press F5 to reload" action.

**Example:**

```cpp
} else if (event.type == SDL_EVENT_KEY_DOWN) {
    if (event.key.key == SDLK_ESCAPE) {
        running = false;
    } else if (event.key.key == SDLK_F5) {
        g_compositor.clearAll();   // blank canvas before reload
        g_lua_ok = performReload(g_lua, g_lua_layers, enjin2::ENJIN_LAYER_COUNT,
                                 g_compositor.visible, &g_input, g_script_path);
        // Also reset dt accumulator and tick count here
        prev_ticks = SDL_GetTicks();  // prevent dt spike on first frame post-reload
    }
}
```

**No repeat key event issue:** SDL3's default behavior for `SDL_EVENT_KEY_DOWN` includes key repeat events (`event.key.repeat` is nonzero). A rapid F5 hold would trigger multiple reloads. Guard with `if (!event.key.repeat)` to fire only on the initial press.

### Pattern 5: LuaCallback dangling-pointer fix (prerequisite)

**What:** `LuaEngine::registerFunction(const std::string& name, LuaCallback callback)` pushes a `lightuserdata` pointing to a local `LuaCallback` copy on the C++ stack. When the stack frame exits, the pointer dangles. Every call to that Lua function via that path is undefined behavior.

**Where the bug is:** `src/scripting/lua_engine.cpp` lines 96–101:
```cpp
void LuaEngine::registerFunction(const std::string& name, LuaCallback callback) {
    // callback is a local copy — its address is invalid after this function returns
    lua_pushlightuserdata(L, reinterpret_cast<void*>(&callback));  // BUG: &local
    lua_pushcclosure(L, [](lua_State* L) -> int {
        LuaCallback* cb = static_cast<LuaCallback*>(lua_touserdata(L, lua_upvalueindex(1)));
        return (*cb)(L);  // UB: cb points to dead stack memory
    }, 1);
    lua_setglobal(L, name.c_str());
}
```

**Fix options:**
1. Store callbacks in a container (`std::vector<LuaCallback>` member on `LuaEngine`) and push the address of the stored copy. The container is cleared in `shutdown()`.
2. Allocate via `lua_newuserdata` so Lua GC owns the memory.
3. Since all new bindings already use `lua_CFunction` exclusively (STATE.md decision from Phase 22), simply delete or `[[deprecated]]` the `LuaCallback` overload entirely if it is unused.

**Recommended fix:** Verify that `LuaBindings::registerAll()` uses only `lua_CFunction` paths (it does — bindings.cpp shows `engine->registerFunction("name", lua_cfunc)`). The `LuaCallback` overload is vestigial. Mark it deprecated or remove its body, leaving only the `lua_CFunction` overload. This eliminates the bug with zero risk.

### Anti-Patterns to Avoid

- **Don't call `lua_close(L)` and then dereference `LuaBindings*` from the registry:** After `shutdown()`, the old Lua state is gone. `registerAll()` and `setLayers()` must always follow a fresh `initialize()` call. The `performReload()` pattern enforces this ordering.
- **Don't reset `LuaCanvas` objects or their underlying `Canvas4` pointers on reload:** The canvas pixels are cleared via `g_compositor.clearAll()`, but the objects themselves remain. `setLayers()` simply re-stores the same pointers — it does not move or re-create canvases.
- **Don't use `delta_time` from the frame before reload on the first post-reload frame:** The dt accumulator (`prev_ticks`) must be reset to the current tick count immediately after reload to prevent a large dt spike.
- **Don't add new `#ifdef` guards for hot-reload:** All reload code goes inside the existing `#ifdef ENJIN2_BUILD_LUA` + `#ifdef ENJIN2_BUILD_SDL` scope. No new preprocessor symbols.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Error message formatting | Custom error parser | `LuaResult.error` as-is | Lua already includes `filename:line: message` in error strings from `luaL_loadstring`/`lua_pcall` |
| Key-repeat filtering | Custom timer debounce | `event.key.repeat` field | SDL3 provides this field directly on `SDL_KeyboardEvent` |
| Script file reading | Custom file I/O | Existing `LuaFileSystem::readScriptFile` via `LuaEngine::executeFile` | Already tested and cross-platform |

**Key insight:** The entire feature is a restructuring of already-working code. There is nothing to hand-roll.

## Common Pitfalls

### Pitfall 1: LuaCallback Dangling Pointer
**What goes wrong:** After a reload, calling any function registered via the `LuaCallback` overload corrupts the stack or crashes with a segfault because the upvalue points to a dead stack variable.
**Why it happens:** `registerFunction(name, callback)` takes `callback` by value (local copy) and stores `&callback` as a lightuserdata upvalue. The local copy is destroyed when the function returns.
**How to avoid:** Fix before Phase 26 work begins. The `LuaCallback` overload is unused by all current bindings (they all use `lua_CFunction`). Either delete the body or mark deprecated.
**Warning signs:** Intermittent crashes on the first Lua callback invocation after any `initialize()` call.

### Pitfall 2: Key Repeat Firing Multiple Reloads
**What goes wrong:** Holding F5 for even a quarter second fires dozens of reloads in succession; the second reload starts while the first `lua.shutdown()` is still executing (single-threaded, so they serialize, but the console floods and can be misleading).
**Why it happens:** SDL3 fires `SDL_EVENT_KEY_DOWN` with `event.key.repeat = 1` for auto-repeated keys.
**How to avoid:** `if (!event.key.repeat)` guard on the F5 handler. This is one line.
**Warning signs:** Console floods with `[reload] script.lua` on a single F5 press.

### Pitfall 3: dt Spike on First Frame After Reload
**What goes wrong:** The first `update(dt)` call after reload receives a large `dt` (the time spent in `performReload()`), causing animations to jump, physics to explode, or other time-dependent effects to glitch.
**Why it happens:** `prev_ticks` was set to a frame before the reload. If reload takes 50ms and the frame target is 33ms, the first dt is 50ms instead of 33ms.
**How to avoid:** Reset `prev_ticks = SDL_GetTicks()` immediately after `performReload()` returns.
**Warning signs:** Visual glitch or skip on the first frame after every reload.

### Pitfall 4: Sprite Pool Not Zeroed After Reload
**What goes wrong:** `spritePool` entries in `LuaBindings` retain their `active = true` state from the previous script run. New script calling `newSprite()` may find "no free slots" even though the previous script's sprites are dead.
**Why it happens:** `LuaBindings` is constructed once and `registerAll()` does not reset `spritePool`. Shutdown + reinitialize of the `LuaEngine` does not touch the `LuaBindings` C++ object.
**How to avoid:** `LuaBindings::registerAll()` should zero/reset the sprite pool. Either add explicit reset logic, or add a `reset()` method called from `performReload()`.
**Warning signs:** `newSprite()` returns `-1` (pool full) after the second or third reload.

### Pitfall 5: activeLayer Not Reset After Reload
**What goes wrong:** After reload, `bindings.activeLayer` still holds the value from the last script run (e.g., layer 3). The next `setLayer()` call by the new script works correctly, but any drawing between script load and the first `setLayer()` call goes to the wrong layer.
**Why it happens:** `setLayers()` does reset `activeLayer = 0` and `currentCanvas = layerCanvases[0]` — so this is actually handled correctly by `setLayers()`.
**How to avoid:** Confirm that `setLayers()` is always called as part of `performReload()`. It is — no action needed beyond following the pattern.
**Warning signs:** None if `performReload()` calls `setLayers()`. Document for clarity.

### Pitfall 6: loadedScripts Vector Grows Unboundedly
**What goes wrong:** `LuaEngine::loadedScripts` (a `std::vector<std::string>`) appends the script filename on every successful `executeFile()`. After N reloads, it holds N copies of the same filename. Memory impact is small but the API is misleading.
**Why it happens:** `shutdown()` calls `loadedScripts.clear()`, but since `shutdown()` is called every reload this is already handled.
**How to avoid:** Confirmed: `shutdown()` calls `loadedScripts.clear()`. No action needed.

## Code Examples

Verified patterns from codebase inspection:

### performReload() skeleton (sdl_main.cpp)

```cpp
// Source: analysis of existing sdl_main.cpp + bindings.cpp lifecycle
static bool performReload(enjin2::LuaScriptSystem& lua,
                          enjin2::LuaCanvas** layers,
                          uint8_t count,
                          bool* visible,
                          enjin2::InputState* input,
                          const std::string& path)
{
    lua.shutdown();                                        // lua_close(L), clear loadedScripts
    if (!lua.initialize()) {                               // luaL_newstate(), open libs, set panic
        std::cerr << "[reload error] Lua init failed\n";
        return false;
    }
    lua.getBindings().registerAll();                       // re-register all C functions + globals
    lua.getBindings().setLayers(layers, count, visible);   // re-wire layer canvas pointers
    lua.getBindings().setInput(input);
    // Reset sprite pool (see Pitfall 4 — add reset() if needed)
    enjin2::LuaResult r = lua.loadScript(path);            // executeFile -> executeString
    if (r.success) {
        std::cerr << "[reload] " << path << "\n";
        return true;
    }
    std::cerr << "[reload error] " << r.error << "\n";
    return false;
}
```

### F5 detection in SDL event pump (sdl_main.cpp)

```cpp
// Source: analysis of existing event loop in sdl_main.cpp
} else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
    if (event.key.key == SDLK_ESCAPE) {
        running = false;
    } else if (event.key.key == SDLK_F5) {
        g_compositor.clearAll();
        g_lua_ok = performReload(g_lua, g_lua_layers,
                                 enjin2::ENJIN_LAYER_COUNT,
                                 g_compositor.visible,
                                 &g_input, g_script_path);
        prev_ticks = SDL_GetTicks();  // reset dt accumulator
    }
}
```

### Paused game loop (sdl_main.cpp)

```cpp
// Source: analysis of existing game loop structure in sdl_main.cpp
// Only run Lua if last reload succeeded:
#ifdef ENJIN2_BUILD_LUA
if (g_lua_ok) {
    g_lua.getBindings().setInput(&g_input);
    {
        enjin2::LuaResult r = g_lua.callFunction("update", dt);
        if (!r.success) {
            std::cerr << "[lua error] " << r.error << "\n";
            g_lua_ok = false;
        }
    }
    if (g_lua_ok) {
        enjin2::LuaResult r = g_lua.callFunction("draw");
        if (!r.success) {
            std::cerr << "[lua error] " << r.error << "\n";
            g_lua_ok = false;
        }
    }
}
#endif
```

### LuaCallback overload fix (lua_engine.cpp)

```cpp
// Source: analysis of lua_engine.cpp lines 92-101
// BEFORE (dangling pointer bug):
void LuaEngine::registerFunction(const std::string& name, LuaCallback callback) {
    lua_pushlightuserdata(L, reinterpret_cast<void*>(&callback));  // UB: &local
    lua_pushcclosure(L, ..., 1);
    lua_setglobal(L, name.c_str());
}

// AFTER (simplest safe fix — unused overload, just no-op or remove body):
void LuaEngine::registerFunction(const std::string& name, LuaCallback /*callback*/) {
    // LuaCallback overload is vestigial. All bindings use lua_CFunction.
    // Intentionally left as no-op to avoid dangling-pointer UB.
    // Use registerFunction(name, lua_CFunction) instead.
}
```

### Sprite pool reset (bindings.cpp / bindings.hpp)

```cpp
// Add to LuaBindings — called from registerAll() or explicit reset():
void LuaBindings::resetSpritePool() {
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        spritePool[i] = SpriteState{};  // zero all fields, active = false
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Script path hardcoded | Script path from argv[1] or --script flag | Phase 26 | Enables reload to target user-specified script |
| Single game loop state | g_lua_ok gate (running vs paused) | Phase 26 | Pause on error, resume on F5 without crashing |
| LuaCallback dangling pointer bug | LuaCallback overload removed/no-op'd | Phase 26 (prereq) | Eliminates UB that would corrupt reload state |

## Open Questions

1. **Sprite pool reset: in registerAll() or separate call?**
   - What we know: `registerAll()` is called on every reload; sprite pool must be zeroed.
   - What's unclear: Whether `registerAll()` is the right home for state reset (it currently only registers functions), or a separate `reset()` method should be added.
   - Recommendation: Add `resetSpritePool()` called at the top of `registerAll()`. This is the lowest-friction approach and keeps `performReload()` simple.

2. **Script path: --script argv flag or positional arg?**
   - What we know: Current code uses `--fps N` style flags. There is no current script argument.
   - What's unclear: Whether `--script path.lua` flag style or positional `argv[1]` is preferred.
   - Recommendation: Use `--script path.lua` for consistency with `--fps N`. Fall back to `"scripts/layer_demo.lua"` if not provided.

3. **Syntax error vs runtime error label distinction (Claude's discretion)**
   - What we know: `luaL_loadstring` returns `LUA_ERRSYNTAX` for syntax errors; `lua_pcall` returns `LUA_ERRRUN` for runtime errors.
   - What's unclear: User left this to discretion.
   - Recommendation: Use a single `[reload error]` prefix for both. The Lua error string already contains the file:line:message detail that distinguishes them. Adding extra labels ("syntax error" vs "runtime error") adds no useful information since the error message already shows where the failure occurred.

## Sources

### Primary (HIGH confidence)

- Codebase: `/home/unwn/dev/enjin/src/platform/sdl/sdl_main.cpp` — full game loop, event pump, Lua init sequence
- Codebase: `/home/unwn/dev/enjin/src/scripting/lua_engine.cpp` — `initialize()`, `shutdown()`, `registerFunction()` bug
- Codebase: `/home/unwn/dev/enjin/src/scripting/bindings.cpp` — `registerAll()`, `setLayers()`, sprite pool structure
- Codebase: `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — `LuaBindings` and `LuaScriptSystem` public API
- Codebase: `/home/unwn/dev/enjin/tests/layer_binding_test.cpp` — test pattern for reload fixture (shutdown → init → registerAll → setLayers)
- Planning: `.planning/STATE.md` — LuaCallback bug confirmed, all new bindings use `lua_CFunction` only
- SDL3 documentation: `SDL_KeyboardEvent.repeat` field exists and is set to nonzero for auto-repeated key events (HIGH confidence, verified from SDL3 header patterns)

### Secondary (MEDIUM confidence)

- SDL3 API pattern for `SDL_EVENT_KEY_DOWN` / `event.key.repeat` — consistent with SDL2 behavior; field documented in `SDL_events.h`

### Tertiary (LOW confidence)

- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; all existing code inspected directly
- Architecture: HIGH — all code patterns derived from reading the actual files
- Pitfalls: HIGH — bugs identified from direct code inspection (LuaCallback bug documented in STATE.md; others from reading LuaBindings state fields)

**Research date:** 2026-02-26
**Valid until:** 2026-03-28 (stable — no external dependencies change)
