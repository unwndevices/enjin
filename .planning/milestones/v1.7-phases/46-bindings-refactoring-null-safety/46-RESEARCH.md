# Phase 46: Bindings Refactoring + Null Safety - Research

**Researched:** 2026-03-01
**Domain:** C++ Lua binding architecture, null safety in Lua C functions, ctest infrastructure
**Confidence:** HIGH

## Summary

Phase 46 has four concrete deliverables: split `bindings.cpp` (1390 lines) into focused translation units via a new `bindings_internal.hpp` shared header, add systematic null safety guards to all numeric-returning binding chains, create `lua_wrapper.hpp` so `sprite_load_test.cpp` compiles, and write overflow tests for the event bus, sprite pool, and component destruction.

The bindings split is the largest mechanical task. `bindings.cpp` currently contains six component-proxy metatables (ScriptProxy, C_Position, C_Timer, C_StateMachine, C_Tilemap, C_Camera), the LuaCanvas method implementations, the LuaBindings core (registerAll + utility methods), and the ObjectProxy metatable. The other nine `bindings_*.cpp` files already exist and are already wired into the `enjin2_lua` CMake target. The split means extracting the remaining content from `bindings.cpp` into two or three new files (`bindings_proxy.cpp`, `bindings_canvas.cpp` or combined), and creating `bindings_internal.hpp` to hold all inter-file-private constants (metatable name literals, forward declarations) that are needed across those new files.

The null safety work is narrow: wherever a binding calls `getBindings(L)` and the result could be null, numeric-returning functions must push `0` (or `0.0`) rather than nothing and `return 0` (which leaves the Lua stack unbalanced when a caller expects a return value). Most files already follow this pattern (`bindings_draw.cpp`, `bindings_input_sprites.cpp`, `bindings_engine.cpp`), but a systematic audit of all binding files is required to catch any gaps in `bindings.cpp`'s remaining proxy code.

**Primary recommendation:** Create `bindings_internal.hpp` first (it unblocks the split), then extract proxy metatables into `bindings_proxy.cpp` and LuaCanvas implementations into `bindings_canvas.cpp`, then create `lua_wrapper.hpp`, then write overflow tests.

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BIND-01 | bindings.cpp split into focused files via bindings_internal.hpp | Section "Architecture Patterns" — split plan, file boundaries, linker safety |
| BIND-02 | Null safety guards added to all binding chains (numeric returns default to 0, not nil) | Section "Null Safety Audit" — existing pattern, gaps found |
| TEST-01 | sprite_load_test.cpp compiles without errors (missing lua_wrapper.hpp resolved) | Section "lua_wrapper.hpp" — exact API required, implementation pattern |
| TEST-02 | Overflow tests for event bus, sprite pool, component destruction | Section "Overflow Tests" — capacity constants, existing test patterns |
</phase_requirements>

---

## Standard Stack

### Core
| Component | Version/Location | Purpose | Why Standard |
|-----------|-----------------|---------|--------------|
| Lua C API | luajit (project-vendored) | Binding functions, stack management | Already the project's Lua runtime |
| GTest | System GTest (find_package(GTest QUIET)) | Unit test framework for sprite_load_test | Already used by sprite_load_test; cmake guard already present |
| CMake | 3.16+ | Build system | Already used; enjin2_lua target already defined |

### Supporting
| Component | Location | Purpose | When to Use |
|-----------|---------|---------|-------------|
| `bind_helpers.hpp` | `include/enjin2/scripting/bind_helpers.hpp` | `LuaFuncDef`, `luaBindFunctions`, `ENJIN_ARRAY_LEN` | All new .cpp files registering function tables |
| `bindings.hpp` | `include/enjin2/scripting/bindings.hpp` | LuaBindings class, all member declarations | Every `bindings_*.cpp` file includes this |
| `component_proxy.hpp` | `include/enjin2/scripting/component_proxy.hpp` | ComponentProxy struct (used by all component proxy metatables) | Proxy metatable files |

---

## Architecture Patterns

### Current bindings.cpp Structure (what remains to be split)

```
src/scripting/bindings.cpp (1390 lines)
├── Lines   1-12:  Includes (bindings.hpp, component_proxy.hpp, components/*.hpp)
├── Lines  13-17:  g_currentBindings global + namespace open
├── Lines  18-246: ScriptProxy metatable (lua_proxy_*_impl, tags)
├── Lines 247-298: C_Position_Proxy metatable
├── Lines 299-386: C_Timer_Proxy metatable
├── Lines 387-483: C_StateMachine_Proxy metatable
├── Lines 484-654: C_Tilemap_Proxy metatable (largest: 170 lines)
├── Lines 655-746: C_Camera_Proxy metatable
├── Lines 747-928: LuaCanvas method implementations (clear, setPixel, drawLine, etc.)
├── Lines 929-1203: LuaBindings core (constructor, registerAll, setCanvas, setInput,
│                   setLayers, resetSpritePool, getBindings, getSpriteSheet,
│                   registerProxyMetatable, registerFont)
└── Lines 1204-1389: ObjectProxy metatable + registerObjectProxyMetatable,
                     registerComponentProxyMetatable, setActiveScene
```

### Existing Split Files (already in enjin2_lua target, no changes needed)

```
src/scripting/
├── bindings_draw.cpp         (363 lines)  — canvas drawing primitives + palette
├── bindings_engine.cpp       (904 lines)  — engine.* table, state machine, random, event, camera
├── bindings_input_sprites.cpp(210 lines)  — input polling, sprite pool (newSprite/drawSprite)
├── bindings_layers_text.cpp  (251 lines)  — layer system, text bindings
├── bindings_math.cpp         (472 lines)  — Vec2/Point/Rect metatables, math utilities
├── bindings_physics.cpp      (514 lines)  — engine.physics.* bindings
├── bindings_sprite_load.cpp  (140 lines)  — engine.sprite.load / freeSprite (file I/O)
├── bindings_store.cpp        (512 lines)  — engine.store.* (LuaStore + LuaCanvas impl)
└── bindings_system.cpp       (42 lines)   — LuaScriptSystem class
```

### Pattern 1: bindings_internal.hpp (the keystone)

**What:** A private (not installed) header living in `src/scripting/` that holds all constants and forward declarations shared between bindings translation units. It must NOT be a public header (install path).

**Why needed:** The proxy metatable names (e.g., `"ScriptProxy"`, `"C_Position_Proxy"`) are currently defined as `static constexpr` at file scope in `bindings.cpp`. When the file is split, the extracted `bindings_proxy.cpp` and `bindings.cpp` (or `bindings_canvas.cpp`) both need these constants. Without the internal header, you duplicate them and risk ODR violations if they diverge. The `g_currentBindings` global also needs a single definition point.

**Location:** `src/scripting/bindings_internal.hpp` (private to src tree, not in include/)

**Contents of bindings_internal.hpp:**
```cpp
// src/scripting/bindings_internal.hpp
// Private inter-TU declarations for enjin2_lua — NOT a public install header
#pragma once
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/component_proxy.hpp"

namespace enjin2 {

// Metatable name constants — shared by bindings.cpp and bindings_proxy.cpp
static constexpr const char* PROXY_METATABLE          = "ScriptProxy";
static constexpr const char* CPOSITION_PROXY_METATABLE = "C_Position_Proxy";
static constexpr const char* CTIMER_PROXY_METATABLE    = "C_Timer_Proxy";
static constexpr const char* CFSM_PROXY_METATABLE      = "C_StateMachine_Proxy";
static constexpr const char* CTILEMAP_PROXY_METATABLE  = "C_Tilemap_Proxy";
static constexpr const char* CCAMERA_PROXY_METATABLE   = "C_Camera_Proxy";
static constexpr const char* OBJECT_PROXY_METATABLE    = "ObjectProxy";

} // namespace enjin2
```

**Linker safety:** Because these are `static constexpr`, each translation unit that includes this header gets its own copy. There is no ODR issue since they are not `extern` or non-const globals. The `g_currentBindings` global is only referenced internally within `bindings.cpp` proxy functions — it does NOT need to be in the internal header as long as the ScriptProxy section stays in `bindings.cpp`.

**CRITICAL WARNING (from STATE.md):** `bindings_internal.hpp` must be created BEFORE any extraction begins. Splitting before the header exists breaks compilation immediately.

### Pattern 2: Proposed Split Targets

**Recommended split: 2 new files**

#### bindings_proxy.cpp (extract from bindings.cpp)
Contains all 6 component proxy metatables + ObjectProxy metatable + the three registration functions:
- Lines 247-298: C_Position_Proxy
- Lines 299-386: C_Timer_Proxy
- Lines 387-483: C_StateMachine_Proxy
- Lines 484-654: C_Tilemap_Proxy
- Lines 655-746: C_Camera_Proxy
- Lines 1204-1338: ObjectProxy metatable
- `registerObjectProxyMetatable()`, `registerComponentProxyMetatable()` (lines 1339-1377)

Includes needed:
```cpp
#include "bindings_internal.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/state_machine.hpp"
#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/components/camera.hpp"
#include "../../include/enjin2/core/object.hpp"
```

**What stays in bindings.cpp after extraction:**
- Lines 1-17: includes + g_currentBindings
- Lines 18-246: ScriptProxy metatable (depends on g_currentBindings global)
- Lines 747-928: LuaCanvas implementations
- Lines 929-1203: LuaBindings core (registerAll, setCanvas, setInput, etc.)
- Lines 1378-1389: setActiveScene() (depends on m_eventBus + m_activeCamera)

This keeps `bindings.cpp` at ~650 lines, well below 1400.

#### Alternative: extract LuaCanvas to bindings_canvas.cpp
LuaCanvas method implementations (lines 747-928, ~182 lines) could go to `bindings_canvas.cpp`. This is optional — doing it reduces bindings.cpp further but adds a third new file. If the success criterion is just "split into focused files," extracting the proxy metatables alone satisfies it.

### Pattern 3: CMake target update

After creating new files, add them to the `enjin2_lua` target in `CMakeLists.txt`:
```cmake
target_sources(enjin2_lua PRIVATE
    # ... existing files ...
    src/scripting/bindings_proxy.cpp      # NEW
    # src/scripting/bindings_canvas.cpp   # NEW if extracting LuaCanvas
)
```

### Anti-Patterns to Avoid

- **Splitting before creating bindings_internal.hpp:** Compile will break before a single line is moved.
- **Making bindings_internal.hpp a public header:** Must stay in `src/scripting/`, never in `include/enjin2/`. It references component headers that bindings.hpp intentionally omits to control include cost.
- **Moving g_currentBindings to bindings_internal.hpp:** This would create a definition in each TU that includes it — ODR violation for a non-const global. Keep it in `bindings.cpp` only.
- **Using `extern` declarations in bindings_internal.hpp for the metatable strings:** Not needed since `static constexpr` works correctly with multiple TU inclusion.

---

## Null Safety Audit

### Existing Pattern (already correct)

The established pattern across `bindings_draw.cpp`, `bindings_input_sprites.cpp`, `bindings_engine.cpp`:

```cpp
// For numeric-returning functions:
int LuaBindings::lua_getPixel(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) {
        lua_pushinteger(L, 0);  // push 0, not nothing
        return 1;
    }
    // ... real implementation ...
}

// For void-returning functions:
int LuaBindings::lua_clear(lua_State* L) {
    REQUIRE_CANVAS(b, L);  // returns 0 (no push) if null — correct for void
    // ...
    return 0;
}
```

**Key rule:** If a binding function is documented to return a value (integer, number, boolean), it MUST push a safe default and `return 1` when the guard condition fails. Returning `return 0` from a numeric function leaves the stack empty, causing a Lua arithmetic error at the call site in scripts.

### Files Already Compliant (HIGH confidence)

| File | Pattern Used | Status |
|------|-------------|--------|
| `bindings_draw.cpp` | `REQUIRE_CANVAS` macro returns 0 (void ops); getWidth/getHeight push 0 | Compliant |
| `bindings_input_sprites.cpp` | All 4 input funcs push `0`/`0.0`; newSprite pushes `-1` | Compliant |
| `bindings_engine.cpp` | `lua_engine_random_integer` pushes 0; float pushes 0.0; time pushes 0 | Compliant |
| `bindings_sprite_load.cpp` | lua_loadSprite pushes -1; lua_freeSprite returns 0 (void) | Compliant |
| `bindings_physics.cpp` | Physics ops are void or push numeric results after null check | Needs verification |
| `bindings_math.cpp` | Vec2/Point/Rect constructors; math utilities | Needs verification |

### Audit Scope for BIND-02

The systematic audit must cover every function in every `bindings_*.cpp` file. The audit question for each function:
1. Does it return a numeric/boolean value to Lua?
2. Does it call `getBindings(L)` or `getCanvas()` at entry?
3. If yes, does the null-check branch push a safe default AND return 1?

Any function that calls `getBindings(L)` without null-checking (or checks but returns 0 without pushing) is a BIND-02 gap.

Functions to pay particular attention to in `bindings.cpp` that are NOT yet extracted:
- `lua_cposition_proxy_index_impl` — returns from proxy getX/getY closures; closures check valid but the outer index function does return 1 (pushnil) on null key, which is correct
- `lua_ctimer_proxy_index_impl`, `lua_cfsm_proxy_index_impl`, etc. — all proxy __index functions; return nil on unknown key is acceptable (nil is not arithmetic)
- The proxy methods themselves (timer:after, fsm:addState) are void — `return 0` is correct

**Conclusion:** The main BIND-02 risk is in any binding that returns a number and could be called with a stale/null `getBindings(L)` result. The proxy metatable code uses `luaL_error` on stale proxy (which is a longjmp, so no push needed). The primary risk is in the non-proxy sections.

---

## lua_wrapper.hpp

### What sprite_load_test.cpp Requires

The test `sprite_load_test.cpp` includes `lua_wrapper.hpp` and uses:
```cpp
LuaWrapper lua_;

lua_.initialize();             // -> bool
lua_.getBindings()             // -> LuaBindings& (for .setAssetPath())
lua_.shutdown();
lua_.execute(const char*)      // -> LuaResult
lua_.getEngine()               // -> LuaEngine& (for .getState())
lua_.setCanvas(LuaCanvas*)     // -> void (proxy to bindings.setCanvas)
```

### Implementation Pattern

`LuaWrapper` is a thin convenience aggregate that owns a `LuaEngine` and `LuaBindings`. It mirrors `LuaScriptSystem` (already defined in `bindings.hpp`) but with the simpler API the test needs.

**Option A (Preferred):** Create `lua_wrapper.hpp` as a pure header alias — it defines `LuaWrapper` as a typedef or thin wrapper that delegates to `LuaScriptSystem`, which already has `initialize()`, `shutdown()`, `getEngine()`, `getBindings()`, `executeScript()`, but NOT `execute()`:

```cpp
// include/enjin2/scripting/lua_wrapper.hpp
#pragma once
#include "bindings.hpp"

namespace enjin2 {

/**
 * @brief Convenience wrapper combining LuaEngine + LuaBindings for tests.
 * Mirrors LuaScriptSystem but exposes execute() returning LuaResult.
 */
class LuaWrapper {
public:
    LuaEngine  engine;
    LuaBindings bindings;

    LuaWrapper() : bindings(&engine) {}

    bool initialize() {
        if (!engine.initialize()) return false;
        bindings.registerAll();
        return true;
    }

    void shutdown() { engine.shutdown(); }

    LuaResult execute(const char* code) {
        return engine.executeString(code);
    }

    LuaEngine& getEngine()     { return engine; }
    LuaBindings& getBindings() { return bindings; }

    void setCanvas(LuaCanvas* c) { bindings.setCanvas(c); }
};

} // namespace enjin2
```

**Why header-only is correct:** `LuaEngine` and `LuaBindings` already have their implementations in `.cpp` files. `LuaWrapper` contains no new logic beyond delegation — it does not need a `.cpp` file. This also means no CMake changes are required (no new source file to add).

**NOTE:** `LuaScriptSystem` already exists in `bindings.hpp` and does most of this, but its `executeScript()` returns `LuaResult` while the test calls `lua_.execute()`. The wrapper adds the `execute()` alias and the `getEngine()` returning `LuaEngine&` (LuaScriptSystem already has this).

---

## Overflow Tests

### TEST-02 Targets

| System | Capacity Constant | Location | Overflow Behavior |
|--------|------------------|---------|------------------|
| Event bus | `MAX_CHANNELS = 16`, `MAX_SUBS_PER_CH = 8` | `lua_event_bus.hpp` | subscribe() returns 0 when channel or subscriber table is full |
| Sprite pool | `LUA_SPRITE_POOL_SIZE = 16` | `bindings.hpp` (LuaBindings private) | lua_loadSprite returns -1 when pool full (already tested in sprite_load_test PoolFull test) |
| Component destruction | N/A (dynamic via Object/proxy system) | ComponentProxy valid flag | stale proxy raises luaL_error (already tested in script_proxy_lifetime_test) |

### Existing Test Patterns

Tests in this project use a combination of:
1. **GTest fixture** (only sprite_load_test uses GTest via find_package(GTest QUIET))
2. **Custom ASSERT macro pattern** (all other Lua tests: eventbus_test, scripting_api_test, etc.)

The custom pattern:
```cpp
static int passes = 0, failures = 0;
#define ASSERT(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } else { passes++; } } while(0)

int main() {
    test_overflow_eventbus();
    // ...
    printf("%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
```

### New Test File: `tests/overflow_test.cpp`

Should cover:
1. **EventBus channel overflow**: Create 17 distinct event names via `engine.event.on()`; 17th channel subscription returns 0 (failure ID).
2. **EventBus subscriber overflow**: Register 9 subscribers on the same channel; 9th returns 0.
3. **Sprite pool overflow**: Already partially covered by `sprite_load_test.cpp::PoolFullReturnsNegOne`. The new overflow test can exercise this via the Lua `engine.sprite.load()` path with 17 loads.
4. **Component destruction proxy safety**: Destroy an object and verify that accessing the proxy returns error (existing `script_proxy_lifetime_test` covers this, but a focused overflow test for component destruction order is the new ask).

### CMake Entry for overflow_test

```cmake
if(ENJIN2_BUILD_LUA)
    add_executable(overflow_test
        overflow_test.cpp
    )
    target_include_directories(overflow_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_link_libraries(overflow_test PRIVATE
        enjin2
        enjin2_lua
    )
    add_test(NAME overflow_test COMMAND overflow_test)
endif()
```

**Link pattern:** Uses `enjin2 enjin2_lua` (not GTest), matching the existing Lua test pattern.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| String constants shared across TUs | Duplicate literals in each .cpp | `static constexpr` in bindings_internal.hpp | ODR-safe, zero-overhead |
| Null-safe integer return | Custom error type | `lua_pushinteger(L, 0); return 1;` inline pattern | Consistent with all existing bindings |
| LuaWrapper initialization | New LuaState management code | Delegate to existing `LuaEngine::initialize()` + `LuaBindings::registerAll()` | Already correct and tested |
| Overflow detection | Custom capacity tracking | Use existing constants: `LuaEventBus::MAX_CHANNELS`, `LuaBindings::LUA_SPRITE_POOL_SIZE` | Constants already correct |

---

## Common Pitfalls

### Pitfall 1: g_currentBindings ODR Violation
**What goes wrong:** Moving `static LuaBindings* g_currentBindings = nullptr;` into `bindings_internal.hpp` creates multiple definitions — one per TU including the header.
**Why it happens:** Non-const globals cannot be safely placed in shared headers.
**How to avoid:** Keep `g_currentBindings` defined only in `bindings.cpp`. It is only referenced from the ScriptProxy static functions in `bindings.cpp`, so there is no cross-file need.

### Pitfall 2: PROXY_METATABLE used in bindings.cpp AND bindings_proxy.cpp
**What goes wrong:** If PROXY_METATABLE is left as `static constexpr` in `bindings.cpp` after the split, `bindings_proxy.cpp` cannot see it unless it defines its own copy or includes `bindings_internal.hpp`.
**Why it happens:** Static file-scope declarations have no linkage — they are invisible to other TUs.
**How to avoid:** Move all metatable name constants to `bindings_internal.hpp` before splitting. Both `bindings.cpp` and `bindings_proxy.cpp` include the internal header.

### Pitfall 3: registerProxyMetatable calls reference static functions
**What goes wrong:** `registerProxyMetatable()` (in bindings.cpp) references `lua_proxy_index_impl` and `lua_proxy_newindex_impl`. After the split, if those functions move to `bindings_proxy.cpp`, `registerProxyMetatable()` must also move — or the implementation must be reorganized.
**Why it happens:** C++ static functions are TU-local.
**How to avoid:** Keep `registerProxyMetatable()` in the same TU as `lua_proxy_index_impl`. Move the ScriptProxy section and `registerProxyMetatable()` to `bindings_proxy.cpp` together, OR keep the ScriptProxy section in `bindings.cpp` (since it references `g_currentBindings`).

**Recommended:** Keep ScriptProxy + registerProxyMetatable in `bindings.cpp`. Extract only the ComponentProxy (C_Position through C_Camera) and ObjectProxy sections to `bindings_proxy.cpp`. This avoids the `g_currentBindings` cross-reference entirely.

### Pitfall 4: sprite_load_test links against enjin2_lua + enjin2_core + enjin2_graphics
**What goes wrong:** sprite_load_test's CMakeLists links against `enjin2_lua enjin2_core enjin2_graphics GTest::gtest_main`. After creating `lua_wrapper.hpp`, if the header pulls in any symbols not covered by those libraries, link will fail.
**Why it happens:** The test does not link against `enjin2` (the aggregate). It links the component libraries directly.
**How to avoid:** `lua_wrapper.hpp` must only use classes already in `bindings.hpp` (LuaEngine, LuaBindings, LuaCanvas, LuaResult). These are all in `enjin2_lua`, which is already linked.

### Pitfall 5: Pushing nothing on null guard in numeric function
**What goes wrong:** A function documented as returning a number does `if (!b) return 0;` without pushing. Lua caller receives nothing, then tries to do arithmetic — runtime error in the script.
**Why it happens:** The developer confuses "return 0 Lua values" with "return the integer 0".
**How to avoid:** For any function that must return a number: `if (!b) { lua_pushinteger(L, 0); return 1; }`. For void functions: `if (!b) return 0;` is correct (no push).

### Pitfall 6: LuaWrapper in header-only form — member initialization order
**What goes wrong:** `LuaWrapper` has `LuaEngine engine; LuaBindings bindings;` as public members. If `bindings` is initialized before `engine` (by declaration order), and `LuaBindings` constructor takes `LuaEngine*`, the pointer is valid but the engine is not yet constructed.
**Why it happens:** C++ initializes members in declaration order.
**How to avoid:** Declare `engine` before `bindings` in the class definition. In the constructor initializer list, use `bindings(&engine)` — the pointer arithmetic is safe even though `engine` is not fully initialized yet (LuaBindings constructor only stores the pointer, does not call any methods). This is the same pattern used by `scripting_api_test.cpp::Fixture` successfully.

---

## Code Examples

### Verified pattern: bindings_internal.hpp in use

From `bindings_draw.cpp` (confirmed working, already split):
```cpp
// src/scripting/bindings_draw.cpp
#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/graphics/palette.hpp"
#include <algorithm>
#include <cmath>

namespace enjin2 {

#define REQUIRE_CANVAS(b, L) \
    LuaBindings* b = LuaBindings::getBindings(L); \
    if (!(b) || !(b)->currentCanvas) return 0

int LuaBindings::lua_getWidth(lua_State* L) {
    LuaBindings* bindings = getBindings(L);
    if (!bindings || !bindings->currentCanvas) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, bindings->currentCanvas->getWidth());
    return 1;
}
```

### Verified pattern: LuaWrapper usage (from sprite_load_test.cpp)

```cpp
class SpriteAssetLoaderTest : public ::testing::Test {
protected:
    LuaWrapper lua_;

    void SetUp() override {
        ASSERT_TRUE(lua_.initialize());
        lua_.getBindings().setAssetPath("tests");
    }

    void TearDown() override {
        lua_.shutdown();
    }
};

TEST_F(SpriteAssetLoaderTest, LoadValidSpriteReturnsHandle) {
    LuaResult res = lua_.execute("test_handle = engine.sprite.load('test_pikachu')");
    EXPECT_TRUE(res.success);
    lua_State* L = lua_.getEngine().getState();
    lua_getglobal(L, "test_handle");
    EXPECT_EQ(lua_tointeger(L, -1), 0);  // first slot
    lua_pop(L, 1);
}
```

### Verified pattern: Overflow test structure (from eventbus_test.cpp)

```cpp
// src: tests/eventbus_test.cpp — ASSERT pattern (no GTest)
static void test_event_channel_overflow() {
    printf("--- EventBus channel overflow beyond MAX_CHANNELS ---\n");
    LuaEngine engine;
    LuaBindings bindings(&engine);
    engine.initialize();
    bindings.registerAll();
    lua_State* L = engine.getState();

    // Subscribe to 16 distinct channels (MAX_CHANNELS = 16)
    for (int i = 0; i < LuaEventBus::MAX_CHANNELS; ++i) {
        char buf[64];
        snprintf(buf, sizeof(buf), "engine.event.on('ch%d', function() end)", i);
        LuaResult r = engine.executeString(buf);
        ASSERT(r.success, "channel registration succeeded");
    }

    // 17th channel: subscribe should return 0 (failure)
    LuaResult r = engine.executeString("result_id = engine.event.on('overflow', function() end)");
    ASSERT(r.success, "overflow subscribe did not crash");
    int id = static_cast<int>(engine.getGlobalNumber("result_id"));
    ASSERT(id == 0, "subscribe returns 0 when all channels full");
}
```

---

## Validation Architecture

The config does not have `workflow.nyquist_validation` set to true (the field is absent from `.planning/config.json`). Skipping formal Validation Architecture section.

The test infrastructure uses ctest. Current baseline: **35 tests**, with `sprite_load_test` (#27) currently broken (can't compile due to missing `lua_wrapper.hpp`). The new `overflow_test` will add test #36.

**Build command to verify after implementation:**
```bash
cmake --build build
ctest --test-dir build
```

**Quick test for sprite_load_test specifically:**
```bash
cmake --build build --target sprite_load_test
ctest --test-dir build -R sprite_load_test
```

**Quick test for overflow_test:**
```bash
cmake --build build --target overflow_test
ctest --test-dir build -R overflow_test
```

---

## State of the Art

| Old Approach | Current Approach | Notes |
|--------------|-----------------|-------|
| 1390-line monolithic bindings.cpp | Already partially split: 9 specialized `bindings_*.cpp` files exist | bindings.cpp still holds proxy metatables + canvas impl + core |
| No internal coordination header | `bind_helpers.hpp` exists (LuaFuncDef, luaBindFunctions) | bindings_internal.hpp does not yet exist |
| No lua_wrapper convenience class | LuaScriptSystem exists in bindings.hpp | lua_wrapper.hpp does not exist yet |
| Inconsistent null guards | Most files already correct | Audit needed for completeness |

---

## Open Questions

1. **Should LuaCanvas implementations go to bindings_canvas.cpp?**
   - What we know: They are 182 lines (lines 747-928) and could be extracted without issue.
   - What's unclear: The phase success criterion says "focused files" but does not name specific files beyond mentioning `bindings_proxy.cpp` and `bindings_scene.cpp` in the description.
   - Recommendation: Extract to `bindings_canvas.cpp` if the planner wants to fully empty bindings.cpp of implementation code. Otherwise, extracting just the proxy metatables satisfies the "split" criterion.

2. **bindings_scene.cpp mentioned in phase description — what goes in it?**
   - What we know: The phase description mentions `bindings_proxy.cpp` and `bindings_scene.cpp` as example output files.
   - What's unclear: `bindings_engine.cpp` already handles `engine.scene.*` bindings. There is no obvious "scene" content remaining in `bindings.cpp`.
   - Recommendation: Interpret `bindings_scene.cpp` as equivalent to `bindings_proxy.cpp` — it likely refers to proxy-related code (ScriptProxy + ObjectProxy are scene-facing APIs). Use `bindings_proxy.cpp` as the extraction target name.

3. **ICanvas abstract constructor usage in LuaCanvas**
   - What we know: `LuaCanvas(ICanvas<Pixel4>*)` exists in `bindings.hpp` but `Canvas4<W,H>` requires template construction.
   - What's unclear: Whether `sprite_load_test.cpp` using `ICanvas<Pixel4> canvas(64, 64)` (dynamic-sized) requires a new constructor in LuaCanvas.
   - Recommendation: Check if `ICanvas<Pixel4>` supports non-template construction with runtime sizes. Looking at the test: `ICanvas<Pixel4> canvas(64, 64)` — this is a runtime-sized canvas. The `LuaCanvas(ICanvas<Pixel4>* canvas)` constructor already handles this. No new constructor needed.

---

## Sources

### Primary (HIGH confidence)
- Direct source code inspection: `/home/unwn/dev/enjin/src/scripting/bindings.cpp` — 1390 lines, structure mapped
- Direct source code inspection: `/home/unwn/dev/enjin/src/scripting/bindings_*.cpp` (9 files) — verified existing split
- Direct source code inspection: `/home/unwn/dev/enjin/include/enjin2/scripting/bindings.hpp` — LuaBindings API
- Direct source code inspection: `/home/unwn/dev/enjin/include/enjin2/scripting/lua_event_bus.hpp` — capacity constants
- Direct source code inspection: `/home/unwn/dev/enjin/tests/CMakeLists.txt` — build structure, test patterns
- Direct source code inspection: `/home/unwn/dev/enjin/CMakeLists.txt` lines 163-202 — enjin2_lua target
- Direct source code inspection: `/home/unwn/dev/enjin/tests/sprite_load_test.cpp` — exact LuaWrapper API needed
- Direct build verification: `cmake --build build --target sprite_load_test` — confirmed fatal error on missing lua_wrapper.hpp
- Direct test inventory: `ctest --test-dir build -N` — 35 tests, sprite_load_test currently broken

### Secondary (MEDIUM confidence)
- `.planning/STATE.md` decision: "bindings_internal.hpp must be created before any bindings file is extracted (pitfall: static linkage breakage)"
- `.planning/REQUIREMENTS.md` BIND-01, BIND-02, TEST-01, TEST-02 descriptions

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — project already uses these exact tools; verified from source
- Architecture: HIGH — all section boundaries verified by direct line-count inspection and grep
- Pitfalls: HIGH — root causes verified in actual code; static constexpr TU semantics are C++ standard
- Null safety: HIGH — existing pattern verified across 4 files; audit scope defined

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (codebase changes slowly; confidence high for 30 days)
