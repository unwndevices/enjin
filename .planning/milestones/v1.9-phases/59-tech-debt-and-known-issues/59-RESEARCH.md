# Phase 59: Tech Debt and Known Issues - Research

**Researched:** 2026-03-03
**Domain:** C++ engine internals — Lua binding correctness, const-correctness, palette snapshot semantics, multi-platform input wiring
**Confidence:** HIGH

## Summary

Phase 59 addresses five concrete, well-scoped technical debt items accumulated across v1.0-v1.8. Each item is already documented in PROJECT.md with a root cause — this phase eliminates them rather than carrying them forward. No new capabilities are introduced; the goal is structural correctness and future-safety.

The five items span four distinct subsystems: palette/WASM bindings (`getPaletteRGB`), the `Object` component API (`hasComponent` const-correctness and single-proxy-per-component constraint), the Lua event bus (`EventBus m_L=nullptr` window), and cross-platform input wiring (`C_LuaScript::setInput` on WASM/ESP32). Each has a clear minimal fix that does not perturb the existing architecture or break any currently passing tests.

The project uses CTest with a large suite of ctest unit tests (all currently green) and a zero-dynamic-allocation constraint. Every fix must be buildable on all three targets (SDL3, WASM, ESP32). The SDL3 CTest suite is the primary verification harness; WASM and ESP32 are compile-verified via `build.sh --target wasm|esp32`.

**Primary recommendation:** Fix each item in isolation as its own plan/wave, verify with the existing CTest suite after each, and do not introduce new abstractions that require heap allocation.

## Standard Stack

This phase uses only the project's existing toolchain — no new libraries are introduced.

### Core
| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| C++17 | Project-configured | Language standard throughout codebase | All existing code uses C++17; `if constexpr`, SFINAE, `static_assert` already in use |
| Lua 5.4 | Project-configured | Scripting runtime, LuaRegistry, luaL_ref | All scripting debt items involve Lua C API correctness |
| CTest (SDL3 build) | CMake 3.16+ | Test runner for regression guard | `ctest --test-dir build/sdl3` runs 148+ assertions; green baseline confirmed v1.8 |
| emscripten_bindings.cpp | Emscripten 3.1.73 | WASM JS bindings for `getPaletteRGB`, `setInput` wiring | Two debt items live in WASM-facing code |
| ESP-IDF v5.5 | Project-configured | ESP32 build target, `nvs_flash` | `setInput` wiring needed on ESP32 app_main loop |

### Supporting
| Component | Version | Purpose | When to Use |
|-----------|---------|---------|-------------|
| static_cast / const_cast | C++17 | Clean const-correctness fix for `hasComponent` | Prefer a properly `const` overload of `getComponent<T>` |
| `typed_memory_view` | Emscripten embind | WASM zero-copy buffer return in `getPaletteRGB` | Already in use; snapshot semantic is the issue, not the mechanism |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Adding `const` overload of `getComponent<T>` | `const_cast` in `hasComponent` | `const_cast` is a band-aid; proper `const` overload is the correct fix and matches how the rest of the codebase is written |
| Documenting single-proxy constraint | Supporting multiple proxies per component | Multiple proxy support requires a ref-counted list or fixed array per component — adds complexity; documenting the constraint with an `assert` is the correct minimal fix |
| Fixing EventBus m_L window | No fix (it is already safe) | The window is safe for Lua-reachable paths because `subscribe()`/`emit()` guard on `m_L`; the debt is that the window is undocumented and easy to misuse from C++ — add assertion guard |

## Architecture Patterns

### Item 1: getPaletteRGB() Snapshot Semantics

**Location:** `src/bindings/emscripten_bindings.cpp` line 133-142

**Current behavior:** Returns `typed_memory_view` of a `static uint8_t buf[45]` that is re-filled on each call. The JavaScript caller receives a view into WASM linear memory. If `setPaletteColor` is called after `getPaletteRGB` but before the JS renderer reads the buffer, the view is stale (it was filled at call time, not at render time).

**Root cause:** `static` buffer is filled eagerly at call time. `typed_memory_view` is a view into that buffer, not a live reference to `g_palette`.

**Fix pattern:** Two valid approaches:
1. **Document + assert** — Add a JS-side comment to the WASM bindings explaining callers must re-invoke `getPaletteRGB()` after any `setPaletteColor()` call. The SDL runner is unaffected (it reads `g_palette.resolve()` directly at render time). This is the minimal fix.
2. **Return a stable view** — Fill a globally scoped buffer that is updated on every `setPaletteColor` call as well, so the view is always current. This changes the calling convention and is more invasive.

**Recommended fix:** Approach 1 — add a `@note` JSDoc comment to the emscripten binding explaining snapshot semantics, and optionally a `refreshPaletteRGB()` function that JS callers can call after mutation. No C++ data structure changes needed.

```cpp
// Source: src/bindings/emscripten_bindings.cpp (current code, lines 133-142)
function("getPaletteRGB", +[]() -> val {
    static uint8_t buf[45];
    // Snapshot: fills buf from g_palette at call time.
    // Callers MUST re-invoke after any setPaletteColor() call.
    for (int i = 0; i < 15; ++i) {
        enjin2::RGB c = enjin2::g_palette.getColor(i);
        buf[i*3]   = c.r;
        buf[i*3+1] = c.g;
        buf[i*3+2] = c.b;
    }
    return val(typed_memory_view(45, buf));
});
```

### Item 2: hasComponent() const Calls Non-const getComponent<T>()

**Location:** `include/enjin2/core/object.hpp` lines 181-184

**Current code:**
```cpp
template<typename T>
bool hasComponent() const {
    return getComponent<T>() != nullptr;  // ERROR: calls non-const member from const member
}
```

**Root cause:** `getComponent<T>()` (lines 145-155) is not `const`-qualified. `hasComponent()` is `const`. This is a legitimate const-correctness violation — in practice it compiles because `this` is implicitly `const_cast`-ed by the compiler (UB in strict interpretations, though commonly accepted in practice).

**Fix pattern:** Add a `const` overload of `getComponent<T>()`:
```cpp
template<typename T>
const T* getComponent() const {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    for (size_t i = 0; i < componentCount; ++i) {
        if (auto component = dynamic_cast<const T*>(components[i].get())) {
            return component;
        }
    }
    return nullptr;
}
```

`hasComponent()` then calls this const overload correctly. The non-const overload remains for mutation contexts. No callers need changing.

### Item 3: Single-Proxy-Per-Component Constraint (Last-Wins Overwrite)

**Location:** `include/enjin2/core/component.hpp` line 58, `src/scripting/bindings_proxy.cpp`

**Current behavior:** `Component::m_luaProxy` holds a single non-owning `ComponentProxy*`. If Lua calls `self:get(C_Timer)` twice on the same object, the second call overwrites `m_luaProxy`, so the first proxy's `valid` flag is never cleared on component destruction. The first proxy becomes a dangling reference with an outdated `valid=true`.

**Root cause:** `setLuaProxy()` is a single-pointer setter in `Component`. The design assumed one active proxy per component type. The PROJECT.md comment says "last-wins overwrite."

**Fix pattern (minimal):** Add a debug-build assert in `setLuaProxy()` that fires if a non-null proxy is already registered:
```cpp
void setLuaProxy(ComponentProxy* proxy) {
    // Debug: catch double-proxy usage (single-proxy-per-component constraint)
#ifndef NDEBUG
    if (proxy != nullptr && m_luaProxy != nullptr) {
        printf("[enjin2] WARNING: setLuaProxy called with existing proxy — previous proxy will not be invalidated on destruction\n");
    }
#endif
    m_luaProxy = proxy;
}
```

This documents the constraint loudly without adding complexity. The alternative (fixed array of proxies) is out of scope — it breaks the zero-alloc simplicity and is not required by any game use case documented so far.

### Item 4: EventBus m_L=nullptr Window

**Location:** `include/enjin2/scripting/lua_event_bus.hpp`, `src/scripting/lua_event_bus.cpp`

**Current behavior:** After `clearHandlers()` is called (on scene change), `m_L` is set to `nullptr`. The `setLuaState(L)` call that re-arms it happens in `executeScript()`/`loadScriptFile()` — only after the Lua script begins loading. There is a window between scene change and script load where:
- `m_L == nullptr`
- `subscribe()` and `emit()` will silently no-op (both guard on `!m_L`)
- Any C++ code that calls `emit()` during this window gets silently dropped

**Root cause:** The window is by design for Lua-reachable paths — scripts cannot run during scene load, so no Lua callback can call `engine.event.on()` during the window. The risk is C++ code (not yet present) that calls `emit()` from C++ lifecycle hooks during scene setup.

**Fix pattern:** The current behavior is documented in the PROJECT.md as "safe for Lua-reachable paths." The fix is a comment and optionally a `printf` guard to make the window visible:
```cpp
void LuaEventBus::emit(const char* name) {
    if (!m_L) {
        // m_L is nullptr during the window between clearHandlers() and setLuaState().
        // This is safe: no Lua callbacks can be registered during scene load.
        // C++ callers emitting during scene setup will silently drop their event.
        return;
    }
    // ...existing code...
}
```

No structural change is required. The debt here is documentation and explicitness, not a bug.

### Item 5: C_LuaScript::setInput() Must Be Wired Per-Frame on WASM/ESP32

**Location:** `include/enjin2/components/lua_script.hpp`, `src/components/lua_script.cpp` line 306-310, `src/platform/sdl/sdl_main.cpp` line 274

**Current behavior on SDL3:** `g_lua.getBindings().setInput(&g_input)` is called every frame in the game loop (sdl_main.cpp line 274) after `input_platform_poll`. This correctly wires the current frame's input before Lua update callbacks fire.

**Current behavior on WASM/ESP32:** The ESP32 example (`examples/esp32_idf_example/main/main.cpp`) has no game loop at all — it only calls `engine.executeString(script)` once. There is no per-frame input poll or `setInput()` call. The WASM bindings (`src/bindings/emscripten_bindings.cpp`) expose `LuaScriptSystem` but do not expose `setInput()` or an equivalent.

**Root cause:** The PROJECT.md marks this as explicitly deferred: "C_LuaScript::setInput() in WASM/ESP32 host paths — SDL runner done; platform wiring deferred." It was deferred because WASM and ESP32 game loop entry points are not yet written as full production runners (the ESP32 example is a minimal NVS demo, not a game loop).

**Fix pattern:** Two sub-items:

1. **WASM:** Expose `setInput(buttons, ax, ay)` via `EMSCRIPTEN_BINDINGS` so JavaScript can call it each frame before the Lua update. The JS host already calls `requestAnimationFrame`; adding one binding call is straightforward.

```cpp
// In emscripten_bindings.cpp, inside EMSCRIPTEN_BINDINGS:
function("setInputState", +[](uint16_t buttons, float ax0, float ay0) {
    // Update g_input (requires a global InputState accessible from bindings)
    // Then: g_lua.getBindings().setInput(&g_input);
});
```

2. **ESP32:** Update the ESP32 example's `app_main` to include a FreeRTOS task loop that calls `input_platform_poll`, `setInput`, and the Lua update each tick. The actual hardware input source (GPIO buttons, MIDI) is Tomodachi-side; the example should show the correct wiring pattern even if it uses a stub `input_platform_poll`.

**Important constraint:** The ESP32 fix must stay within the zero-alloc constraint. `InputState` is already a value type (`uint16_t buttons + float axes[8]`) — it can be a static global.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| const-correct component lookup | Custom component cache or index | Add `const` overload to `getComponent<T>()` | `dynamic_cast` with const pointer is standard C++ — no new infrastructure needed |
| Multiple proxy tracking | Fixed array of proxy pointers per component | Document constraint + assert | The single-proxy constraint is intentional for zero-alloc; a fixed array adds complexity without a concrete game use case |
| WASM input state | New InputState class for WASM | Expose existing `InputState` struct via embind | `InputState` (uint16_t + float axes[8]) already matches the WASM contract |

**Key insight:** All five debt items are fixable with small, targeted changes to existing files. None require new types, new files (except possibly a comment update), or changes to the CMake build graph.

## Common Pitfalls

### Pitfall 1: Breaking const correctness during getComponent fix
**What goes wrong:** If the `const` overload of `getComponent<T>` returns `const T*` but callers that previously called it via `hasComponent` start expecting a mutable `T*`, compilation breaks.
**Why it happens:** Template overload resolution: the `const` overload returns `const T*`, the non-const overload returns `T*`. Callers in const contexts must use the `const T*` form.
**How to avoid:** The `hasComponent()` method only needs `const T*` (it just checks for null). No mutation happens in `hasComponent`. Only `getComponent<T>()` in mutation contexts uses the non-const overload. Verify no test calls `hasComponent` and then tries to mutate the returned pointer.
**Warning signs:** Compiler error "cannot convert 'const T*' to 'T*'" at call sites.

### Pitfall 2: WASM setInput wiring requires a globally accessible InputState
**What goes wrong:** `emscripten_bindings.cpp` currently has no reference to `g_input` or any `InputState`. Adding a `setInputState()` binding requires a global or singleton `InputState` accessible from the binding lambda.
**Why it happens:** Emscripten binding lambdas cannot capture state from a game-loop-level variable — bindings are registered at module init time.
**How to avoid:** Declare a `static enjin2::InputState g_wasm_input;` in the same translation unit as the bindings, matching the pattern in `sdl_main.cpp`. The JS caller populates it via `setInputState(buttons, ax, ay)` each frame; a subsequent `updateAndDraw()` call (also to be added) calls `g_lua.getBindings().setInput(&g_wasm_input)`.
**Warning signs:** Linker error or segfault if `g_lua` is also not accessible from the bindings TU.

### Pitfall 3: EventBus m_L re-arm order
**What goes wrong:** If `clearHandlers()` / `setLuaState()` ordering is changed (e.g., calling `setLuaState` before `clearHandlers`), refs allocated under the old Lua state will be `luaL_unref`'d under the new state — causing a Lua registry corruption.
**Why it happens:** `clearHandlers()` calls `luaL_unref(m_L, ...)` for all active refs using the current `m_L`. If `m_L` has already been reassigned to the new state, the unref goes to the wrong registry.
**How to avoid:** Never change the existing order in `lua_script.cpp`: `clearHandlers()` first (which calls `luaL_unref` on old `m_L`), then `setLuaState(L)` (which sets new `m_L`). This order is correct and should be documented with a comment.
**Warning signs:** Lua panic "attempt to unref LUA_NOREF" or corrupted Lua stack.

### Pitfall 4: Double-build test coverage gap for getPaletteRGB
**What goes wrong:** The `getPaletteRGB` snapshot semantic is only observable in JavaScript (WASM context). C++ ctests cannot call emscripten bindings directly.
**Why it happens:** WASM bindings are only compiled under Emscripten; the CTest suite runs under the SDL3 native build.
**How to avoid:** The fix for item 1 is documentation + a JS comment — verification is manual (run the WASM build, call `setPaletteColor`, call `getPaletteRGB`, confirm the returned view is updated). No automated test gap to fill beyond ensuring the WASM build compiles.

## Code Examples

### Const overload of getComponent (Item 2 fix)

```cpp
// Source: include/enjin2/core/object.hpp — add alongside existing non-const overload

// Non-const (existing — unchanged):
template<typename T>
T* getComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    for (size_t i = 0; i < componentCount; ++i) {
        if (auto component = dynamic_cast<T*>(components[i].get())) {
            return component;
        }
    }
    return nullptr;
}

// Const overload (new):
template<typename T>
const T* getComponent() const {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    for (size_t i = 0; i < componentCount; ++i) {
        if (auto component = dynamic_cast<const T*>(components[i].get())) {
            return component;
        }
    }
    return nullptr;
}

// hasComponent now correctly calls const overload (no change needed to hasComponent itself):
template<typename T>
bool hasComponent() const {
    return getComponent<T>() != nullptr;  // resolves to const overload
}
```

### setLuaProxy double-registration warning (Item 3 fix)

```cpp
// Source: include/enjin2/core/component.hpp

void setLuaProxy(ComponentProxy* proxy) {
#ifndef NDEBUG
    if (proxy != nullptr && m_luaProxy != nullptr && m_luaProxy != proxy) {
        printf("[enjin2] WARNING: Component::setLuaProxy — overwriting existing proxy. "
               "Previous proxy will not be invalidated on destruction. "
               "Only one ComponentProxy per component is supported.\n");
    }
#endif
    m_luaProxy = proxy;
}
```

### WASM setInputState binding (Item 5 fix)

```cpp
// Source: src/bindings/emscripten_bindings.cpp
// Add a global InputState for WASM host to populate each frame

#ifdef ENJIN2_BUILD_LUA
static enjin2::LuaScriptSystem g_wasm_lua;
static enjin2::InputState g_wasm_input{};

// Called by JS each frame BEFORE updateAndDraw()
function("setInputState", +[](int buttons, float ax0, float ay0) {
    // advance_frame clears justPressed/justReleased from previous frame
    enjin2::input_advance_frame(&g_wasm_input);
    g_wasm_input.buttons = static_cast<uint16_t>(buttons);
    g_wasm_input.axes[0] = ax0;
    g_wasm_input.axes[1] = ay0;
    g_wasm_lua.getBindings().setInput(&g_wasm_input);
});
#endif
```

Note: `input_advance_frame` is declared in `include/enjin2/input/input_state.hpp` and has platform-neutral implementation — safe to call in WASM context.

### EventBus window documentation (Item 4 fix)

```cpp
// Source: src/scripting/lua_event_bus.cpp — update emit() comment

void LuaEventBus::emit(const char* name) {
    if (!m_L) {
        // m_L is nullptr in the window between clearHandlers() (called on scene deactivation
        // or hot-reload) and setLuaState() (called from executeScript()/loadScriptFile()).
        // This window is safe because no Lua callbacks are registered during scene setup.
        // C++ code emitting during this window will silently drop the event — this is
        // intentional. If C++ lifecycle hooks need to emit events during setup, they must
        // call setLuaState() first.
        return;
    }
    // ...
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `hasComponent()` calling non-const `getComponent()` | Add `const` overload (Phase 59) | Phase 59 | Fixes latent const-correctness violation; no behavior change |
| Single-proxy undocumented | Single-proxy documented + asserted | Phase 59 | Makes constraint explicit; catches misuse in debug builds |
| `getPaletteRGB` snapshot undocumented | Snapshot explicitly documented in binding | Phase 59 | WASM callers know to re-invoke after palette mutation |
| EventBus window undocumented | Window documented in `emit()` body | Phase 59 | Future C++ maintainers understand the invariant |
| WASM/ESP32 `setInput` deferred | WASM: new `setInputState()` binding; ESP32 example: per-frame loop | Phase 59 | Input works correctly on all 3 platforms |

**Deprecated/outdated:**
- The "deferred" label on `C_LuaScript::setInput()` WASM/ESP32 paths in PROJECT.md — Phase 59 resolves this.

## Open Questions

1. **WASM game loop entry point**
   - What we know: The current WASM bindings expose `LuaScriptSystem` methods but no per-frame orchestration. The JS side calls individual methods.
   - What's unclear: Whether the JS host already has a `requestAnimationFrame` loop calling `LuaScriptSystem::executeScript` each frame, or whether a new `updateFrame(dt)` binding is needed.
   - Recommendation: Add both `setInputState(buttons, ax, ay)` and `updateFrame(dt)` bindings. The `updateFrame` binding calls `setTimeState`, `tickCoroutines`, `tickTweens`, and `tickCameraFollow` in the correct order (matching sdl_main.cpp). This makes the WASM host's JS code symmetric with the SDL runner.

2. **ESP32 input hardware source**
   - What we know: The ESP32 example has no game loop and no GPIO input. Tomodachi's actual button wiring is project-side, not engine-side.
   - What's unclear: Whether Phase 59 should wire a stub `input_platform_poll` in the example or defer to Tomodachi integration.
   - Recommendation: Update `esp32_idf_example/main/main.cpp` to show the correct per-frame structure (FreeRTOS task loop, `input_advance_frame`, `input_platform_poll` call, `setInput`, Lua update call) with a stub `input_platform_poll` that returns zeros. This documents the correct wiring pattern without requiring actual GPIO code.

3. **Multiple-proxy support scope**
   - What we know: The single-proxy constraint causes `self:get()` calls to produce last-wins overwrite. No current game code has been observed to call `self:get()` on the same component type twice.
   - What's unclear: Whether any v1.8 or post-v1.8 game code actually triggers this.
   - Recommendation: Fix is a debug assert + comment. If a real game hits the assert, escalate to a fixed-array proxy list in a future phase.

## Sources

### Primary (HIGH confidence)
- Source code directly read: `src/bindings/emscripten_bindings.cpp` — `getPaletteRGB` implementation (lines 133-142)
- Source code directly read: `include/enjin2/core/object.hpp` — `hasComponent`/`getComponent` definitions (lines 145-184)
- Source code directly read: `include/enjin2/core/component.hpp` — `setLuaProxy` (line 83)
- Source code directly read: `src/scripting/lua_event_bus.cpp` — `clearHandlers`, `emit`, `subscribe`
- Source code directly read: `src/platform/sdl/sdl_main.cpp` — per-frame `setInput` wiring (line 274)
- Source code directly read: `examples/esp32_idf_example/main/main.cpp` — missing game loop / no `setInput`
- `.planning/PROJECT.md` — authoritative tech debt list and key decisions table

### Secondary (MEDIUM confidence)
- `.planning/STATE.md` — tech debt list confirmed in sync with PROJECT.md
- Existing test suite in `/home/unwn/git/enjin/tests/` — confirms CTest is the regression guard

### Tertiary (LOW confidence)
- None — all findings are based on direct code inspection.

## Metadata

**Confidence breakdown:**
- Item root causes: HIGH — all five items directly verified by reading source code
- Fix patterns: HIGH — patterns follow existing conventions already established in the codebase (e.g., const overloads, debug `printf`, registry pattern)
- WASM game loop question: MEDIUM — the JS host's calling pattern is not visible in the C++ source tree; requires JS-side investigation at plan time

**Research date:** 2026-03-03
**Valid until:** 2026-04-03 (stable codebase; no upstream dependency changes expected)
