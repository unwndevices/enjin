# Phase 31: engine.* Global Table - Research

**Researched:** 2026-02-27
**Domain:** Lua C API — nested table construction, pointer injection, raw-API binding patterns
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ENG-01 | Lua scripts access `engine.scene.switch(id)` to request scene transitions | `switchTo(uint32_t)` added in Phase 30 to `SceneStateMachine`; `engine.scene` sub-table holds a C closure that captures `SceneStateMachine*` stored in the Lua registry; registration must happen after Phase 30 SSM pointer is known |
| ENG-02 | Lua scripts access `engine.scene.find(name)` to locate named objects (returns proxy or nil) | `findByName(const char*)` added in Phase 29 to `Scene`/`ObjectCollection`; `engine.scene.find` closure captures current-scene pointer or SSM pointer to reach the active scene; returns nil when not found (push `lua_pushnil`) |
| ENG-03 | Lua scripts access `engine.input.held(btn)`, `engine.input.just_pressed(btn)`, `engine.input.just_released(btn)`, `engine.input.axis(n)` for polling | `InputState` already has `held()`, `justPressed()`, `justReleased()`, `axes[]`; existing flat bindings (`isButtonHeld`, `isButtonJustPressed`, etc.) provide the implementation blueprint; new `engine.input` sub-table wraps same `InputState*` pointer |
| ENG-04 | Lua scripts access `engine.time.delta()`, `engine.time.now()`, `engine.time.frame()` for timing | No C++ side state exists yet; must add a small `EngineTimeState` struct to hold `float dt`, `float totalTime`, `uint32_t frameCount`; these are updated by the host (SDL main loop or game runner) before each frame's Lua calls |
| ENG-05 | Lua scripts access `engine.log(...)` for platform-safe logging | No dedicated `engine_log` function exists; `lua_print` in `bindings.cpp` uses `std::cout` — must write a new binding that works on ESP32 (no `std::cout`); implementation calls `printf` or a platform-log wrapper; varargs via `lua_tostring` + `luaL_tolstring` |
| ENG-06 | `engine.*` table is registered before any script loads (module-level access works) | `LuaBindings::registerAll()` is called during `lua.initialize()` in `performReload()`; the new `registerEngineTable()` call must be added to `registerAll()` so it fires before any `executeString`/`executeFile` call |
</phase_requirements>

---

## Summary

Phase 31 adds a single global `engine` table to the Lua state, populated with sub-tables `engine.scene`, `engine.input`, `engine.time`, and the top-level `engine.log(...)` function. Each sub-table entry is a C closure (a `lua_CFunction` with upvalues) that reads from C++ pointers stored in the Lua registry.

The critical constraint (ENG-06) is that the entire `engine.*` namespace must be populated during `LuaBindings::registerAll()`, which is called from `LuaScriptSystem::initialize()` before any script is loaded. This means no host data (dt, frame counter, SSM pointer) can be missing at registration time — but these pointers must exist in the registry so closures can reach them at call time. The pattern used throughout the existing codebase is: store a pointer as a `lua_pushlightuserdata` / `lua_setfield(L, LUA_REGISTRYINDEX, ...)` during registration, and fetch it in each C function via `lua_getfield(L, LUA_REGISTRYINDEX, ...)`. This pattern is already established in `LuaBindings::registerAll()` (`"enjin_bindings"` key).

The implementation requires: (1) new fields on `LuaBindings` for `SceneStateMachine*`, `Scene*` (or a getter for the active scene), and a small `EngineTimeState`; (2) a `registerEngineTable()` method on `LuaBindings` that builds the nested `engine.*` table structure; (3) setters so the host can inject updated pointers each frame (`setSceneStateMachine()`, `setTimeState()`); (4) a new `engine_table_test.cpp` test that calls module-level access before any function definition. Note that the STATE.md flags this explicitly: "Must verify with module-level access test script before Phase 32 begins."

**Primary recommendation:** Add `registerEngineTable()` to `LuaBindings::registerAll()`, store all C++ pointers in the Lua registry, implement each sub-table using nested `lua_newtable` + `lua_pushcfunction` + `lua_setfield` sequences following the existing `registerAll()` pattern in `bindings.cpp`.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua 5.1 C API | liblua5.1 (desktop) | Nested table construction, C closures, registry | Already the project's Lua version; confirmed by `lua_pcallk` compat macro in `lua_platform.hpp` |
| C++17 | project standard | `constexpr`, `if constexpr`, template helpers | `CMAKE_CXX_STANDARD 17` in `CMakeLists.txt` |
| `<cstdio>` | libc | `printf` for platform-safe logging | Available on all targets including ESP32; `std::cout` is NOT available on ESP32 |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `lua_pushlightuserdata` + registry | Lua 5.1 | Storing C++ pointer for retrieval in closures | Used for all injected pointers (SSM, active scene, time state, input state) |
| `luaL_tolstring` | Lua 5.1 | Convert any Lua value to string for `engine.log` varargs | Available in Lua 5.2+; for Lua 5.1 use `lua_tostring` with type checks (same pattern as existing `lua_print`) |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Lua registry for pointer storage | Global lightuserdata `lua_setglobal` | Registry is private (scripts can't overwrite it); globals can be stomped by user scripts |
| C closures per function | Single dispatch table with switch | C closures are simpler and already used in the project; no benefit to dispatch table at this scale |
| `printf` for `engine.log` | `std::cout` | `std::cout` fails on ESP32 (no iostream); `printf` works everywhere |
| `lua_pushlightuserdata` for SSM pointer | `lua_newuserdata` full userdata | Lightuserdata is sufficient for non-owned pointers; full userdata is needed only for objects requiring GC (Phase 32 ScriptProxy scope) |

**No new dependencies.** All work is in-tree modifications to `bindings.hpp`, `bindings.cpp`, plus a new test file.

---

## Architecture Patterns

### Files to Modify / Create

```
include/enjin2/scripting/
└── bindings.hpp                  — add EngineTimeState struct, m_ssm, m_activeScene
                                    setters, registerEngineTable() declaration,
                                    new static lua_* function declarations

src/scripting/
└── bindings.cpp                  — implement registerEngineTable(); implement
                                    engine.scene.switch, engine.scene.find,
                                    engine.input.held/just_pressed/just_released/axis,
                                    engine.time.delta/now/frame, engine.log

tests/
├── engine_table_test.cpp         — new test (Wave 0 gap)
└── CMakeLists.txt                — register engine_table_test under ENJIN2_BUILD_LUA guard
```

No new `.cpp` files beyond the test. All new code is in the existing `bindings.hpp`/`bindings.cpp`.

### Pattern 1: Registry Pointer Storage

**What:** Store C++ pointers in the Lua registry before any script runs. Each C function fetches the pointer at call time. This is the established project pattern (`"enjin_bindings"` key in `registerAll()`).

**When to use:** Any time a C function needs access to C++ state that cannot be passed as a Lua argument.

**Example:**
```cpp
// Source: src/scripting/bindings.cpp (established pattern)

// In registerAll() — stores 'this' pointer in registry:
lua_State* L = engine->getState();
lua_pushlightuserdata(L, this);
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_bindings");

// Retrieval in any C function (established getBindings() pattern):
static LuaBindings* getBindings(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
    LuaBindings* b = static_cast<LuaBindings*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return b;
}
```

For Phase 31, extend this: store `EngineTimeState*` under `"enjin_time"`, store `SceneStateMachine*` under `"enjin_ssm"`, store `Scene*` under `"enjin_active_scene"`. All fetched via identical helper functions.

### Pattern 2: Nested Table Construction (engine.scene sub-table)

**What:** Build a nested table `engine = { scene = { switch = ..., find = ... }, ... }` using the raw Lua C API. The existing `registerAll()` builds the `love.graphics` sub-table using this exact pattern — inspect lines 211–238 of `bindings.cpp`.

**When to use:** Any named sub-namespace under `engine`.

**Example:**
```cpp
// Source pattern: bindings.cpp lines 211-238 (love.graphics construction)
// Applied to engine.scene:

void LuaBindings::registerEngineTable() {
    lua_State* L = engine->getState();

    // Create top-level engine table
    lua_newtable(L);               // stack: [engine_table]

    // --- engine.scene sub-table ---
    lua_newtable(L);               // stack: [engine_table] [scene_table]
    lua_pushcfunction(L, lua_engine_scene_switch);
    lua_setfield(L, -2, "switch"); // scene_table.switch = lua_engine_scene_switch
    lua_pushcfunction(L, lua_engine_scene_find);
    lua_setfield(L, -2, "find");   // scene_table.find   = lua_engine_scene_find
    lua_setfield(L, -2, "scene");  // engine_table.scene = scene_table

    // --- engine.input sub-table ---
    lua_newtable(L);
    lua_pushcfunction(L, lua_engine_input_held);
    lua_setfield(L, -2, "held");
    lua_pushcfunction(L, lua_engine_input_just_pressed);
    lua_setfield(L, -2, "just_pressed");
    lua_pushcfunction(L, lua_engine_input_just_released);
    lua_setfield(L, -2, "just_released");
    lua_pushcfunction(L, lua_engine_input_axis);
    lua_setfield(L, -2, "axis");
    lua_setfield(L, -2, "input");  // engine_table.input = input_table

    // --- engine.time sub-table ---
    lua_newtable(L);
    lua_pushcfunction(L, lua_engine_time_delta);
    lua_setfield(L, -2, "delta");
    lua_pushcfunction(L, lua_engine_time_now);
    lua_setfield(L, -2, "now");
    lua_pushcfunction(L, lua_engine_time_frame);
    lua_setfield(L, -2, "frame");
    lua_setfield(L, -2, "time");

    // --- engine.lua sub-table (GC control — ENG per GC-01/GC-02, Phase 35 scope) ---
    // Stub: create empty table so module-level engine.lua ~= nil (ENG-06)
    lua_newtable(L);
    lua_setfield(L, -2, "lua");

    // --- engine.log (top-level function) ---
    lua_pushcfunction(L, lua_engine_log);
    lua_setfield(L, -2, "log");

    // Assign to global name "engine"
    lua_setglobal(L, "engine");    // stack: [] (engine_table popped)
}
```

**Critical:** Every `lua_newtable` + `lua_setfield` pair must be balanced. The pattern `lua_newtable(L); ...; lua_setfield(L, -2, "name")` pops the inner table and adds it to the outer. Any extra value on the stack is a hard-to-debug bug — balance is the #1 pitfall (see Common Pitfalls).

### Pattern 3: EngineTimeState Struct

**What:** A plain struct stored in `LuaBindings` (not heap-allocated) that holds the current frame's timing data. The host updates it before calling `update()` and `draw()` each frame.

**Example:**
```cpp
// In bindings.hpp — new struct and field

struct EngineTimeState {
    float dt = 0.0f;           // current frame delta in seconds
    float totalTime = 0.0f;    // accumulated total time in seconds
    uint32_t frameCount = 0;   // frame counter, incremented each frame
};

// In LuaBindings private section:
EngineTimeState m_timeState;

// Public setter called by host each frame before update():
void setTimeState(float dt, float totalTime, uint32_t frameCount) {
    m_timeState.dt         = dt;
    m_timeState.totalTime  = totalTime;
    m_timeState.frameCount = frameCount;
}
```

The SDL main loop already computes `float dt` per frame (line 246 of `sdl_main.cpp`). Adding `setTimeState(dt, accum, frame)` before `g_lua.callFunction("update", dt)` is a one-line change.

### Pattern 4: Scene Pointer Injection

**What:** `engine.scene.switch(id)` calls `SceneStateMachine::switchTo(uint32_t)` (added in Phase 30). `engine.scene.find(name)` calls `currentScene->findByName(name)` (added in Phase 29). Both need C++ pointer access.

**Two design options:**

**Option A — SSM pointer only:** Store `SceneStateMachine*` in the registry. `scene.switch` calls `m_ssm->switchTo(id)`. `scene.find` is harder — needs the active scene, which requires `SSM::getCurrentScene()` (must be added to SSM, or a separate `Scene*` pointer stored separately).

**Option B — Both SSM and active Scene pointers:** Store both `SceneStateMachine*` and `Scene*` in the registry. `scene.switch` uses SSM; `scene.find` uses active scene. The active scene pointer is updated by the host at activation time. This is simpler and avoids adding a `getCurrentScene()` accessor to SSM.

**Recommendation (Option B):** Add `SceneStateMachine* m_ssm = nullptr` and `Scene* m_activeScene = nullptr` to `LuaBindings`. Add public setters `setSceneStateMachine()` and `setActiveScene()`. The host (game runner or `C_LuaScript`) calls both at setup. Store pointers in the registry during `registerEngineTable()`. For the SDL main loop (which does not use `SceneStateMachine`), these pointers remain `nullptr` — `engine.scene.switch` and `engine.scene.find` silently return nil/false. This matches the "no crash on any target" constraint in ENG-05.

**Example — engine.scene.switch:**
```cpp
// Source: new code in bindings.cpp

static int lua_engine_scene_switch(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    auto* ssm = static_cast<SceneStateMachine*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!ssm) return 0;  // no SSM installed — silent no-op

    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    ssm->switchTo(id);
    return 0;
}
```

**Forward declaration issue:** `bindings.hpp` includes `input_state.hpp` but does NOT include `scene.hpp` or `scene_state_machine.hpp`. Adding `SceneStateMachine*` and `Scene*` fields requires either:
- A forward declaration `class SceneStateMachine; class Scene;` in `bindings.hpp` (preferred — avoids heavyweight include), OR
- Full includes `#include "../core/scene.hpp"` and `#include "../core/scene_state_machine.hpp"`

Since `bindings.hpp` stores only raw pointers and calls methods only in `bindings.cpp` (not inline in the header), forward declarations in `bindings.hpp` + full includes in `bindings.cpp` is the correct approach. This avoids the circular include issue identified in Phase 30 research.

### Pattern 5: engine.log Varargs Implementation

**What:** `engine.log(msg)` (ENG-05) must work on ESP32 (no `std::cout`). Use `printf`. For multi-argument calls (future), iterate stack and print each.

**Example:**
```cpp
static int lua_engine_log(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            printf("%s", s);
        } else {
            // Non-string value: print type name
            printf("(%s)", lua_typename(L, lua_type(L, i)));
        }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}
```

This is safe on all targets: `printf` is available on ESP32 (maps to `ets_printf` in ESP-IDF), Emscripten (`printf` maps to `console.log`), and desktop. The existing `lua_print` in `bindings.cpp` uses `std::cout` + `std::endl` which is incompatible with ESP32 — the new `lua_engine_log` uses `printf` exclusively.

### Pattern 6: engine.lua Stub for ENG-06 / Phase 35 Compatibility

**What:** ENG-06 requires that `engine.lua` is not nil when accessed at module level. Phase 35 will add `engine.lua.collect()` and `engine.lua.memory()` (GC-01, GC-02). Phase 31 must create an empty `engine.lua` table so that module-level code doing `local gc = engine.lua` does not fail.

**Recommendation:** Create an empty sub-table for `engine.lua` in `registerEngineTable()`. Phase 35 will add `collect` and `memory` to it. No stub functions needed — just `lua_newtable` + `lua_setfield(L, -2, "lua")`.

### Anti-Patterns to Avoid

- **Building `engine.*` outside `registerAll()`:** If `registerEngineTable()` is called after `executeFile()`, module-level script code runs without the `engine` global defined — ENG-06 fails. It MUST be called inside `registerAll()` before any script is loaded.
- **Unbalanced stack in nested table construction:** Every `lua_newtable` must be eventually popped by `lua_setfield` or `lua_setglobal`. An extra value on the stack silently corrupts subsequent operations. Count push/pop pairs manually or use `lua_gettop` asserts during development.
- **Storing `float dt` as a field updated after `lua_setglobal`:** The `engine` table is set as a global once during `registerAll()`. Updating `engine.time.delta()` must go through the live pointer in the registry, not by re-building the table each frame. The C function reads `m_timeState.dt` at call time from the live `LuaBindings` object.
- **Using `std::cout` in `engine.log`:** Incompatible with ESP32. Use `printf`.
- **Calling `SSM::switchTo()` with a signed integer:** `switchTo` takes `uint32_t`. Lua integers are signed. Cast with `static_cast<uint32_t>(luaL_checkinteger(L, 1))` to avoid sign-conversion warnings (`-Wsign-conversion`).
- **Not guarding for null SSM/scene pointer:** The SDL main loop does not use `SceneStateMachine`. `engine.scene.switch` and `engine.scene.find` must return cleanly (not crash) when `m_ssm == nullptr`.
- **Forward declarations missing `namespace enjin2`:** `Scene` and `SceneStateMachine` live in namespace `enjin2`. Forward declarations must be `namespace enjin2 { class Scene; class SceneStateMachine; }`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Nested Lua table construction | Custom wrapper class or macro system | Raw `lua_newtable` + `lua_setfield` sequence | Already the project pattern (see `love.graphics` in `bindings.cpp`); no overhead, zero dependencies |
| SSM pointer access from Lua | Global C variable or function-static | Lua registry (`lua_setfield(L, LUA_REGISTRYINDEX, ...)`) | Registry is private to the Lua state; scripts cannot overwrite it; established project pattern |
| Platform log function | `std::cout` wrapper | `printf` | `printf` available on all targets; `std::cout` absent on ESP32 |
| Time accumulation | Lua-side accumulator script | C++ `EngineTimeState` updated each frame | More accurate (C++ has the authoritative `dt`); avoids floating-point drift from Lua-side accumulation |

**Key insight:** The entire `engine.*` implementation is a thin wrapping layer over C++ state that already exists. Every function body is 2–5 lines. The complexity is in the table construction and pointer injection patterns, not the business logic.

---

## Common Pitfalls

### Pitfall 1: Unbalanced Lua Stack in Table Construction

**What goes wrong:** `lua_newtable` pushes a value; if `lua_setfield` is called with the wrong index, the sub-table is not placed where expected. Subsequent calls to `lua_setfield(L, -2, ...)` target the wrong table.

**Why it happens:** The Lua stack is index-based. Constructing nested tables requires tracking which table is at which index. A missed or extra push silently shifts all subsequent indices.

**How to avoid:** Follow the canonical pattern strictly:
```
lua_newtable(L)                      -- push outer
  lua_newtable(L)                    -- push inner
  lua_pushcfunction(L, f)
  lua_setfield(L, -2, "name")        -- pop f, set inner["name"]
  lua_setfield(L, -2, "sub")         -- pop inner, set outer["sub"]
lua_setglobal(L, "engine")           -- pop outer
```
Each `lua_setfield(L, -2, ...)` pops the top value and assigns to the table now at index -2 (which becomes -1 after the pop). After `lua_setfield`, the stack depth decreases by 1.

**Warning signs:** `engine.scene` is nil despite the C code running; accessing `engine.time.delta` throws a "attempt to index a nil value" error.

### Pitfall 2: registerEngineTable() Called After executeFile()

**What goes wrong:** If `registerEngineTable()` is called after any script file is loaded, module-level code in that script (code outside any function definition) runs before `engine` is defined. `engine` is nil. ENG-06 fails.

**Why it happens:** Lua executes the top-level body of a script at `luaL_loadstring`/`lua_pcall` time, not at function call time. Module-level statements like `local input = engine.input` execute immediately.

**How to avoid:** Call `registerEngineTable()` from within `registerAll()`, which is called before any `executeString` or `executeFile`. The call order in `LuaScriptSystem::initialize()` is: `engine.initialize()` → `bindings.registerAll()` (which includes `registerEngineTable()`) → then (separately) `loadScript()`. This ordering is already correct for the existing bindings.

**Warning signs:** Test scripts with module-level `engine.*` access crash with "attempt to index a nil value (global 'engine')".

### Pitfall 3: SceneStateMachine / Scene Include Circular Dependency

**What goes wrong:** Adding `#include "../core/scene_state_machine.hpp"` to `bindings.hpp` creates a compile error if `scene_state_machine.hpp` transitively includes `bindings.hpp` (or any of its transitive includes).

**Why it happens:** The current include graph for `bindings.hpp` does NOT include core scene headers. Adding them could create new include cycles, similar to the Phase 30 `scene.hpp` ↔ `scene_state_machine.hpp` issue.

**How to avoid:** Use forward declarations in `bindings.hpp`:
```cpp
// In bindings.hpp — before class LuaBindings
namespace enjin2 {
    class Scene;
    class SceneStateMachine;
}
```
The full includes go in `bindings.cpp` only:
```cpp
// In bindings.cpp
#include "../../include/enjin2/core/scene.hpp"
#include "../../include/enjin2/core/scene_state_machine.hpp"
```
Forward declarations are sufficient in the header since `LuaBindings` only stores raw pointers to these types — it does not call any methods inline.

**Warning signs:** Compile errors like "use of incomplete type 'enjin2::SceneStateMachine'" in `bindings.cpp`; or circular include compile failures in the header.

### Pitfall 4: engine.time.delta() Returns Stale Value on First Frame

**What goes wrong:** `EngineTimeState` is default-initialized with `dt = 0.0f`. On the first frame, `setTimeState()` is called before `update()` — if the host forgets to call `setTimeState()` before loading the script, module-level code that calls `engine.time.delta()` gets `0.0f`.

**Why it happens:** The `EngineTimeState` fields default to zero. Module-level calls happen at script-load time. The first `setTimeState()` call happens at the start of the game loop, not during initialization.

**How to avoid:** Document that `engine.time.delta()` returns `0.0f` at module load time — this is correct behavior. The frame delta is meaningless at load time (no frame has elapsed). Scripts should only use `engine.time.delta()` inside the `update(dt)` callback.

**Warning signs:** A script that does `local initial_dt = engine.time.delta()` at module level gets `0.0`, which is correct and expected — not a bug.

### Pitfall 5: engine.scene.find Returns Wrong Type

**What goes wrong:** `engine.scene.find("name")` returns a raw pointer (lightuserdata or integer address). Scripts receive a non-nil, non-table value that they cannot index.

**Why it happens:** Phase 32 (ScriptProxy) is the phase that turns `Object*` pointers into proper Lua proxy objects. Phase 31 should return either `nil` (not found) or a lightuserdata (found) — but Phase 32 may replace this implementation entirely.

**How to avoid:** For Phase 31, return the `Object*` as a lightuserdata when found, or `nil` when not found. This satisfies ENG-02 ("returns proxy or nil"). The lightuserdata is not indexable, but it is non-nil — Phase 32 upgrades it to a full proxy. This is a deliberate two-phase design. Document this clearly in the implementation comment.

**Concrete return code:**
```cpp
static int lua_engine_scene_find(lua_State* L) {
    // ... get active scene pointer from registry ...
    const char* name = luaL_checkstring(L, 1);
    Object* obj = activeScene->findByName(name);
    if (!obj) {
        lua_pushnil(L);
    } else {
        lua_pushlightuserdata(L, obj);  // Phase 32 will upgrade to full proxy
    }
    return 1;
}
```

**Warning signs:** Scripts crash with "attempt to index a nil value" when `find()` returns nil — this is expected behavior (object not found). Scripts crash trying to index the lightuserdata — this is expected until Phase 32.

### Pitfall 6: engine.log Crashes on Non-String Argument

**What goes wrong:** `engine.log(42)` — user passes a number. `lua_tostring` returns a valid string for numbers in Lua 5.1. But `engine.log(true)` or `engine.log(nil)` — `lua_tostring` returns `nullptr` for booleans and nil.

**Why it happens:** `lua_tostring` returns `nullptr` for non-coercible types (boolean, table, function, nil) in Lua 5.1.

**How to avoid:** Check `lua_tostring` return value before calling `printf("%s", s)`. Use `lua_typename(L, lua_type(L, i))` as fallback. See Pattern 5 above for the complete null-safe implementation.

**Warning signs:** Segfault in `printf` when `engine.log(true)` is called; the `nullptr` is passed as the format argument to `printf`.

---

## Code Examples

Verified patterns from direct codebase inspection:

### Existing registry storage pattern (bindings.cpp:140-141)

```cpp
// Source: src/scripting/bindings.cpp lines 140-141
// In registerAll() — stores 'this' in registry
lua_pushlightuserdata(L, this);
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
```

`registerEngineTable()` uses the same pattern for `"enjin_ssm"`, `"enjin_active_scene"`.

### Existing nested table construction (bindings.cpp:211-238)

```cpp
// Source: src/scripting/bindings.cpp lines 205-238
// love.graphics sub-table construction — exact pattern for engine.* sub-tables

lua_getglobal(L, "love");
if (lua_istable(L, -1)) {
    lua_newtable(L);  // Create graphics table

    lua_pushcfunction(L, lua_setColor);
    lua_setfield(L, -2, "setColor");
    // ... more functions ...

    lua_setfield(L, -2, "graphics");  // love.graphics = graphics_table
}
lua_pop(L, 1);  // pop love table
```

`registerEngineTable()` follows this pattern but creates the outer table itself (no pre-existing `engine` table to `getglobal`).

### Existing input polling C function (bindings.cpp:678-683)

```cpp
// Source: src/scripting/bindings.cpp lines 678-683
int LuaBindings::lua_isButtonHeld(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentInput) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, b->currentInput->held(btn) ? 1 : 0);
    return 1;
}
```

`lua_engine_input_held` in Phase 31 is identical but accessed as `engine.input.held(btn)` rather than `isButtonHeld(btn)`. The implementation body is the same — same `getBindings(L)` pattern, same `currentInput` field, same `held()` call.

### Existing time computation in SDL main loop (sdl_main.cpp:246-248)

```cpp
// Source: src/platform/sdl/sdl_main.cpp lines 246-248
float dt = static_cast<float>(frame_start - prev_ticks) / 1000.0f;
if (dt > max_dt) dt = max_dt;
prev_ticks = frame_start;
```

This `dt` value is already passed to `g_lua.callFunction("update", dt)`. Adding `g_lua.getBindings().setTimeState(dt, accum, frame++)` before that line is the only change needed to the SDL main loop.

### Existing registerTable() helper (bindings.cpp:272-282)

```cpp
// Source: src/scripting/bindings.cpp lines 272-282
void LuaBindings::registerTable(const std::string& tableName,
                               const std::vector<std::pair<std::string, lua_CFunction>>& functions) {
    lua_State* L = engine->getState();

    lua_newtable(L);
    for (const auto& func : functions) {
        lua_pushcfunction(L, func.second);
        lua_setfield(L, -2, func.first.c_str());
    }
    lua_setglobal(L, tableName.c_str());
}
```

`registerEngineTable()` is a specialized version that builds nested tables instead of flat ones. The existing `registerTable()` helper is too flat for this use case — write `registerEngineTable()` directly with explicit nested structure.

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Flat global functions (`isButtonHeld`, `getAxis`, `time`) | Namespaced sub-tables (`engine.input.held`, `engine.time.delta`) | Phase 31 | Lua scripts use cleaner namespaced API; old flat globals remain for backwards compat with existing scripts |
| No scene control from Lua | `engine.scene.switch(id)` deferred transition | Phase 31 (requires Phase 30 SSM) | Lua scripts can drive scene transitions without direct C++ coupling |
| No named object lookup from Lua | `engine.scene.find("name")` → lightuserdata or nil | Phase 31 (requires Phase 29 findByName) | Foundation for Phase 32 ScriptProxy |

**Not deprecated:** The flat bindings (`isButtonHeld`, `getAxis`, `time`, `print`) remain registered for backwards compatibility. Existing scripts (`layer_demo.lua`, `reload_test.lua`, `pikachu_demo.lua`) continue to work unchanged. The new `engine.*` API is an addition, not a replacement.

---

## Open Questions

1. **Should `engine.input` replace or supplement the existing flat input functions?**
   - What we know: ENG-03 only requires `engine.input.*` to work. Existing `isButtonHeld`, `isButtonJustPressed`, etc. are registered in `registerAll()` and used in current test scripts.
   - What's unclear: Whether old flat names should be removed, deprecated, or left as aliases.
   - Recommendation: Leave old flat names registered (no breakage, existing tests pass). The new `engine.input.*` API is the v1.5 canonical form. Removal can happen in a future cleanup phase.

2. **Should `engine.time.now()` accumulate from engine start or from script load?**
   - What we know: `totalTime` in `EngineTimeState` must be accumulated by the host. The SDL main loop does not currently track cumulative time.
   - What's unclear: Whether Phase 31 adds cumulative time accumulation to the SDL main loop, or if `engine.time.now()` is only wired for game loop usage (not SDL demo loop).
   - Recommendation: Add `static float s_totalTime = 0.0f; static uint32_t s_frameCount = 0;` to `sdl_main.cpp`'s game loop, accumulated each frame. `setTimeState(dt, s_totalTime, s_frameCount)` before update. This is a 3-line change. If it creates scope concerns, `engine.time.now()` can return `0.0f` with a comment "host must call setTimeState".

3. **Does `engine.scene.find()` need to find objects across all scenes or only the active scene?**
   - What we know: ENG-02 says "locate named objects" without specifying scope. `findByName` is on `ObjectCollection` which is per-scene. The active scene is accessible via `m_activeScene`.
   - What's unclear: Whether global across all scenes or active-scene-only is intended.
   - Recommendation: Active scene only. A script calling `engine.scene.find("name")` from within a scene's `update()` callback is logically in the context of the active scene. Cross-scene find is a future concern.

4. **What does `engine.scene.find()` return before Phase 32 ScriptProxy is complete?**
   - What we know: ENG-02 says "returns proxy or nil." Phase 31 is before Phase 32 (ScriptProxy).
   - What's unclear: Whether Phase 31 should return a lightuserdata placeholder or defer this function to Phase 32.
   - Recommendation: Return lightuserdata (non-nil) when object is found, nil when not found. This satisfies ENG-02's "returns proxy or nil" literally. Phase 32 upgrades the lightuserdata to a proper proxy with a metatable. This is the established pattern used by the existing code for the canvas pointer.

---

## Sources

### Primary (HIGH confidence)

- Live codebase inspection (2026-02-27):
  - `src/scripting/bindings.cpp` — full implementation; registry storage pattern (lines 140-141); nested table construction for `love.graphics` (lines 205-238); `registerTable()` helper (lines 272-282); `getBindings()` retrieval pattern (lines 265-270); all existing input polling C functions (lines 678-709)
  - `include/enjin2/scripting/bindings.hpp` — full class definition; `currentInput` field; `setInput()` setter pattern; `registerAll()` method
  - `include/enjin2/scripting/lua_engine.hpp` — `getState()` accessor; `registerFunction()`; table-creation API
  - `src/scripting/lua_engine.cpp` — `registerFunction()` implementation (lines 99-104); `createTable()` (lines 106-111)
  - `include/enjin2/scripting/lua_platform.hpp` — `VCV_RACK` vs `ESP32` guard; confirms Lua 5.1 compatibility macro (`lua_pcallk` → `lua_pcall`)
  - `include/enjin2/input/input_state.hpp` — `held()`, `justPressed()`, `justReleased()`, `axes[]` confirmed
  - `src/platform/sdl/sdl_main.cpp` — frame loop; `dt` computation (line 246); `setInput()` call (line 263); `callFunction("update", dt)` (line 265)
  - `tests/layer_binding_test.cpp` — Lua test fixture pattern with `LuaEngine + LuaBindings`; ASSERT macro; `engine.initialize()` + `bindings.registerAll()` setup confirmed
  - `tests/CMakeLists.txt` — `if(ENJIN2_BUILD_LUA)` guard pattern for Lua tests; `target_link_libraries(... enjin2_lua)` pattern confirmed
  - Phase 29 RESEARCH.md — `Scene::findByName()` confirmed as available after Phase 29
  - Phase 30 RESEARCH.md — `SceneStateMachine::switchTo(uint32_t)` confirmed as available after Phase 30; forward declaration pattern confirmed; `m_ssm` injection confirmed
  - `.planning/STATE.md` — "Must verify with module-level access test script before Phase 32 begins" blocker noted

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; all C++ state already exists; Lua C API patterns verified from codebase
- Architecture: HIGH — all patterns derived from direct codebase inspection of `bindings.cpp`; no speculation
- Pitfalls: HIGH — all pitfalls traced to specific code patterns, Lua 5.1 API behavior, and existing include structure

**Research date:** 2026-02-27
**Valid until:** 90 days (stable C++ codebase, no fast-moving dependencies)
