# Phase 35: GC Control + Component Assertions - Research

**Researched:** 2026-02-27
**Domain:** Lua 5.4 GC API, C++ debug/release assertion patterns, embedded frame-budget memory management
**Confidence:** HIGH

## Summary

Phase 35 has two fully independent deliverables that happen to share a phase. The GC side (GC-01, GC-02) wires two Lua functions into the pre-existing `engine.lua` sub-table stub that Phase 31 created intentionally empty for this phase. The component-assertions side (DEP-01, DEP-02, DEP-03) adds a protected `assertRequires<T>()` template method to the `Component` base class that behaves differently under `NDEBUG`.

Both halves are small and well-bounded. The Lua GC half is a two-function table fill using the already-understood `LuaFuncDef` + `luaBindFunctions` pattern. The C++ assertion half is a pure header addition to `component.hpp` with no new source files needed. Testing mirrors the Phase 33 `error_policy_test` pattern (fixture + `Object::addComponent<>()` + named asserts).

The critical constraint for the GC side is embedded-target safety: `engine.lua.collect()` must use `LUA_GCSTEP` (one incremental step), not `LUA_GCCOLLECT` (full stop-the-world collection). The critical constraint for the assert side is the `assertRequires<T>` naming — chosen in Phase 26 to avoid the C++20 `requires` keyword collision — and using `NDEBUG` to gate behavior, which is the standard CMake mechanism already established in this build.

**Primary recommendation:** Implement both halves in a single plan. GC half: fill `engine.lua` table in `bindings_engine.cpp`. Assert half: add protected template to `component.hpp`, gated on `NDEBUG`. Wire one new test `gc_assert_test.cpp` covering all five requirements.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| GC-01 | Lua scripts access `engine.lua.collect()` for explicit GC step | `engine.lua` table stub already exists in `bindings_engine.cpp`; add `lua_gc(L, LUA_GCSTEP, 0)` C function to fill it using `LuaFuncDef` pattern |
| GC-02 | Lua scripts access `engine.lua.memory()` to query current memory usage in bytes | `LuaPlatform::getMemoryUsage()` already implements `lua_gc(L, LUA_GCCOUNT, 0)*1024 + lua_gc(L, LUA_GCCOUNTB, 0)`; Lua function delegates there or replicates the one-liner |
| DEP-01 | Component base class provides `assertRequires<T>()` protected template method | `Component` base class is in `include/enjin2/core/component.hpp`; owner is accessible via `protected Object* owner`; `owner->getComponent<T>()` retrieves sibling |
| DEP-02 | In debug builds, missing dependency triggers an assertion with clear error message naming both components | Gate on `#ifndef NDEBUG`; use `assert()` with a string literal that names both types via `typeid().name()` or static message; current build type is Debug |
| DEP-03 | In release builds, missing dependency logs once and disables the component (no abort on ESP32) | Under `NDEBUG`: check `owner->getComponent<T>() == nullptr`, if so call `printf(...)` once and `this->setEnabled(false)` — mirrors Phase 33 Disable policy pattern |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua 5.4 (system) | 5.4.8 | GC control API | Project already uses `liblua5.4.so`; detected in `CMakeCache.txt` |
| C++ standard `<cassert>` | C++17 | `assert()` macro in debug builds | Standard, zero-overhead in release when `NDEBUG` defined |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `LuaFuncDef` + `luaBindFunctions` | project-internal | Zero-overhead table registration | All new Lua C functions follow this pattern from `bind_helpers.hpp` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `LUA_GCSTEP` (incremental) | `LUA_GCCOLLECT` (full) | `LUA_GCCOLLECT` causes a stop-the-world pause which will spike frame budget on ESP32; `LUA_GCSTEP` is the correct choice |
| `assert()` macro | custom abort handler | `assert()` already calls `abort()` in debug; custom handler adds complexity for no embedded benefit here since DEP-03 handles the release path separately |
| `typeid(T).name()` in message | hard-coded string | `typeid().name()` is available everywhere in this project (not stripped) but returns mangled names on GCC/Clang without demangling; plain string literal in the assert message is simpler |

## Architecture Patterns

### Recommended Project Structure

No new files needed. All changes fit existing files:

```
include/enjin2/core/component.hpp     — add assertRequires<T>() protected method
src/scripting/bindings_engine.cpp     — fill engine.lua stub with collect/memory functions
include/enjin2/scripting/bindings.hpp — declare lua_engine_lua_collect/memory private static methods
tests/gc_assert_test.cpp              — new test file (GC-01, GC-02, DEP-01, DEP-02, DEP-03)
tests/CMakeLists.txt                  — add gc_assert_test inside if(ENJIN2_BUILD_LUA) block
```

### Pattern 1: Filling the engine.lua stub (GC-01, GC-02)

**What:** The `registerEngineTable()` in `bindings_engine.cpp` already creates an empty `engine.lua` table at line 62. Phase 35 replaces the empty table with two C functions wired in with `luaBindFunctions`.

**When to use:** Mirrors how every other engine sub-table is filled.

**Current stub (line 61-63 of bindings_engine.cpp):**
```cpp
// --- engine.lua sub-table (ENG-06 compat; Phase 35 adds collect/memory) ---
lua_newtable(L);
lua_setfield(L, -2, "lua");
```

**Phase 35 replacement:**
```cpp
// --- engine.lua sub-table (GC-01, GC-02) ---
static const LuaFuncDef kLuaFuncs[] = {
    {"collect", lua_engine_lua_collect},
    {"memory",  lua_engine_lua_memory},
};
lua_newtable(L);
luaBindFunctions(L, -1, kLuaFuncs, ENJIN_ARRAY_LEN(kLuaFuncs));
lua_setfield(L, -2, "lua");
```

**Implementation of the two C functions:**
```cpp
// GC-01: engine.lua.collect() — one incremental GC step
// Uses LUA_GCSTEP (not LUA_GCCOLLECT) to avoid mid-frame spike on embedded targets.
int LuaBindings::lua_engine_lua_collect(lua_State* L) {
    lua_gc(L, LUA_GCSTEP, 0);
    return 0;
}

// GC-02: engine.lua.memory() — Lua heap bytes
// Combines LUA_GCCOUNT (KB) + LUA_GCCOUNTB (remainder bytes) for exact byte count.
int LuaBindings::lua_engine_lua_memory(lua_State* L) {
    int kb   = lua_gc(L, LUA_GCCOUNT,  0);
    int rem  = lua_gc(L, LUA_GCCOUNTB, 0);
    lua_pushnumber(L, static_cast<lua_Number>(kb * 1024 + rem));
    return 1;
}
```

Source verification: `lua_gc` constants confirmed in `/usr/include/lua.h` lines 330-341. `LuaPlatform::getMemoryUsage()` uses identical `LUA_GCCOUNT + LUA_GCCOUNTB` pattern in `src/scripting/lua_platform.cpp`.

### Pattern 2: assertRequires<T>() on Component base class (DEP-01, DEP-02, DEP-03)

**What:** A protected template method on `Component` that calls `owner->getComponent<T>()` and either asserts (debug) or disables + logs (release).

**When to use:** Called from derived component `awake()` overrides, following the "awake is where you check dependencies" lifecycle convention from `component.hpp` doc comments.

**Implementation in component.hpp:**
```cpp
#include <cassert>
// ... (inside Component class, protected section)

/**
 * @brief Assert that a sibling component of type T exists on the same Object.
 *
 * In debug builds (NDEBUG not defined): triggers assert with message naming both types.
 * In release builds (NDEBUG defined): logs once via printf and disables this component.
 *
 * Call from awake() to declare component dependencies loudly.
 * @tparam T Required component type (must derive from Component)
 */
template<typename T>
void assertRequires() {
    static_assert(std::is_base_of<Component, T>::value,
                  "T must derive from Component");
    if (owner->getComponent<T>() == nullptr) {
#ifndef NDEBUG
        assert(false && "assertRequires<T> failed: required component not present");
#else
        // Release: log once and disable. No abort on ESP32.
        printf("[enjin2] Component dependency missing — disabling component\n");
        setEnabled(false);
#endif
    }
}
```

**Key design choices:**
- `assert(false && "message")` — standard idiom; the string literal appears in the assert failure message on all platforms
- `printf()` in release path — consistent with `engine.log` and all other logging in this codebase (`lua_engine_log` uses printf; `std::cout` is avoided per Phase 31 decisions)
- `setEnabled(false)` — mirrors Phase 33 `ScriptErrorPolicy::Disable` pattern exactly
- `owner->getComponent<T>()` — `Object::getComponent<T>()` already exists and does `dynamic_cast<T*>`

### Anti-Patterns to Avoid

- **Using `LUA_GCCOLLECT` instead of `LUA_GCSTEP`:** `LUA_GCCOLLECT` performs a full GC cycle. On ESP32 with a busy heap this can stall for several frame budgets. `LUA_GCSTEP` performs one incremental step (bounded work).
- **Using `std::cout` or `std::cerr` in the release log path:** The entire codebase uses `printf` exclusively for embedded compatibility (Phase 31-02 decision).
- **Calling `getComponent<T>()` on a null owner:** `owner` is set in `Component::Component(Object* owner)` constructor and is never null by construction contract. No null check needed.
- **Naming the method `requires<T>()`:** Explicitly rejected in Phase 26 — `requires` is a C++20 keyword and causes a compile error. The name `assertRequires<T>()` was chosen then.
- **Registering engine.lua functions globally instead of in the sub-table:** All Phase 35 GC functions must be fields of `engine.lua`, not global Lua functions.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Lua GC byte count | Custom memory counter | `lua_gc(LUA_GCCOUNT)*1024 + lua_gc(LUA_GCCOUNTB)` | Already used in `LuaPlatform::getMemoryUsage()`; exact and correct |
| Debug assertion macro | Custom assert framework | Standard `assert()` from `<cassert>` | Already in the build; gated by `NDEBUG` which CMake manages correctly |
| Component type name in error message | RTTI demangling | Plain string in `assert(false && "message")` | Mangled names are unreadable; a clear static string message is better for embedded targets |

## Common Pitfalls

### Pitfall 1: LUA_GCSTEP vs LUA_GCCOLLECT for embedded targets

**What goes wrong:** Using `lua_gc(L, LUA_GCCOLLECT, 0)` causes a full garbage collection cycle. On embedded targets this is O(live heap) and will cause a visible frame-time spike.

**Why it happens:** `LUA_GCCOLLECT` "performs a full garbage-collection cycle" (Lua 5.4 manual). `LUA_GCSTEP` "performs an incremental step of garbage collection, controlling the amount of work to be done by the argument `data`."

**How to avoid:** Always use `LUA_GCSTEP` with `data=0` (one minimal step) in the `collect()` binding. This gives script authors a way to do light incremental pressure without risking a spike.

**Warning signs:** Scripts calling `engine.lua.collect()` in `update()` causing periodic frame drops.

### Pitfall 2: NDEBUG not set in Debug build type

**What goes wrong:** If the test for DEP-02/DEP-03 runs in a Debug build (current build is `CMAKE_BUILD_TYPE=Debug`), `NDEBUG` is NOT defined, so the `assert()` branch fires and aborts the test process.

**Why it happens:** The test must call `assertRequires<T>()` when the component IS present (to test the happy path), not when it is missing (which would abort). Tests for the "missing dependency" debug path cannot be automated — identical to the Phase 33 ERR-04 Panic decision.

**How to avoid:** Test `assertRequires<T>()` only with a component that IS present in the test object. The assertion-fires-on-missing-component behavior is verified by field value and code inspection, not by a live abort call. For the release path (DEP-03), tests can mock the behavior by testing `isEnabled()` after adding a component that calls `assertRequires` for a missing type — but only when building under a Release configuration with NDEBUG defined.

**Practical test strategy:** Add one `#ifdef NDEBUG` guarded test case that adds a component calling `assertRequires<T>()` for a missing type and checks `isEnabled() == false`. The case is skipped in Debug builds. Add an unconditional happy-path test verifying no-op behavior when the required component is present.

### Pitfall 3: engine.lua functions need the lua_State* not the LuaEngine*

**What goes wrong:** `lua_engine_lua_memory()` just needs `L` to call `lua_gc`. It does NOT need to retrieve a registered C++ pointer from the registry (unlike `engine.time.*` which reads `EngineTimeState*`).

**Why it happens:** Forgetting that `lua_gc` is called directly on the `lua_State*` argument — it's the Lua state's own memory.

**How to avoid:** The function signature is `static int lua_engine_lua_collect(lua_State* L)`. `L` is the Lua state. `lua_gc(L, ...)` is all that is needed.

### Pitfall 4: hasComponent() const but getComponent() non-const

**What goes wrong:** `Component::assertRequires<T>()` calls `owner->getComponent<T>()` which is non-const. Calling it from a const method would be a compile error.

**Why it happens:** `Object::getComponent<T>()` is declared non-const (line 146 of `object.hpp`). `hasComponent<T>() const` calls it via a const_cast loophole (the const method calls the non-const method, which is technically UB but works in practice — it compiles but is a smell).

**How to avoid:** `assertRequires<T>()` should be non-const (no const qualifier on the method) since it may call `setEnabled(false)` in the release branch anyway. This matches the lifecycle — `awake()` is not called on a const object.

## Code Examples

### engine.lua.collect() — GC-01

Lua usage:
```lua
function update(self, dt)
    engine.lua.collect()  -- one incremental GC step
end
```

C++ binding (in `bindings_engine.cpp`):
```cpp
int LuaBindings::lua_engine_lua_collect(lua_State* L) {
    lua_gc(L, LUA_GCSTEP, 0);
    return 0;
}
```

### engine.lua.memory() — GC-02

Lua usage:
```lua
local bytes = engine.lua.memory()
engine.log("Lua heap:", bytes, "bytes")
```

C++ binding (in `bindings_engine.cpp`):
```cpp
int LuaBindings::lua_engine_lua_memory(lua_State* L) {
    int kb  = lua_gc(L, LUA_GCCOUNT,  0);
    int rem = lua_gc(L, LUA_GCCOUNTB, 0);
    lua_pushnumber(L, static_cast<lua_Number>(kb * 1024 + rem));
    return 1;
}
```

Source: `LuaPlatform::getMemoryUsage()` in `/home/unwn/dev/enjin/src/scripting/lua_platform.cpp` uses identical formula (lines 116, 120).

### assertRequires<T>() — DEP-01, DEP-02, DEP-03

Caller side (derived component):
```cpp
#include <enjin2/components/sprite.hpp>   // C_Sprite definition

class C_Animator : public Component {
public:
    C_Animator(Object* owner) : Component(owner) {}

    void awake() override {
        assertRequires<C_Sprite>();   // fails loudly if C_Sprite not on same Object
    }
};
```

Template method in `component.hpp` protected section:
```cpp
template<typename T>
void assertRequires() {
    static_assert(std::is_base_of<Component, T>::value,
                  "T must derive from Component");
    if (owner->getComponent<T>() == nullptr) {
#ifndef NDEBUG
        assert(false && "assertRequires<T> failed: required component not present");
#else
        printf("[enjin2] Component dependency missing — disabling component\n");
        setEnabled(false);
#endif
    }
}
```

### Test fixture pattern (mirrors error_policy_test.cpp)

```cpp
// tests/gc_assert_test.cpp
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/core/component.hpp>

struct GCFixture {
    LuaEngine engine;
    LuaBindings bindings;
    GCFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }
    LuaResult exec(const char* code) { return engine.executeString(code); }
    double getNum(const char* n) { return engine.getGlobalNumber(n); }
};

// GC-01: engine.lua.collect() is a function and does not error
static void test_gc01_collect_no_error() { ... }

// GC-02: engine.lua.memory() returns a non-negative number
static void test_gc02_memory_returns_number() { ... }

// DEP-01: assertRequires<T>() — happy path (component present, no-op)
static void test_dep01_assert_requires_happy_path() { ... }

// DEP-03: release path — missing component disables (only if NDEBUG defined)
#ifdef NDEBUG
static void test_dep03_release_missing_disables() { ... }
#endif
```

### CMakeLists.txt addition (inside if(ENJIN2_BUILD_LUA)):
```cmake
add_executable(gc_assert_test tests/gc_assert_test.cpp)
target_include_directories(gc_assert_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(gc_assert_test PRIVATE enjin2 enjin2_lua)
add_test(NAME gc_assert_test COMMAND gc_assert_test)
```

Note: the DEP tests (component side) only need `enjin2_core` + `enjin2_ui` (same `--start-group/--end-group` pattern as `named_objects_test`). Since GC tests need `enjin2_lua`, linking both covers both halves.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Empty `engine.lua` table (Phase 31 placeholder) | Filled with `collect` and `memory` functions | Phase 35 | Scripts can now manage GC incrementally |
| No component dependency enforcement | `assertRequires<T>()` on Component base | Phase 35 | Dependency bugs caught at startup in debug, gracefully degraded in release |

## Open Questions

1. **LUA_GCSTEP data argument**
   - What we know: `lua_gc(L, LUA_GCSTEP, data)` — `data` controls the "amount of work" in KB units for the Lua incremental GC
   - What's unclear: Whether `data=0` means "minimum step" or "no work at all"
   - Recommendation: Use `data=0` as the default (one minimal step). The Lua 5.4 manual says when `data` is 0 the GC performs one step. Document this in a comment. Game code that needs more aggressive control can call `collect()` multiple times per frame.

2. **assertRequires<T>() message quality in debug**
   - What we know: `assert(false && "message")` shows the literal string in the assert output
   - What's unclear: Whether to embed type names — `typeid(T).name()` returns mangled names on GCC/Clang without demangling
   - Recommendation: Use a static string: `assert(false && "assertRequires<T> failed: required component not present on owner Object")`. Callers can see which component from the call stack. If type names are desired later, add demangling as a separate improvement.

3. **DEP-03 automated test coverage in Debug builds**
   - What we know: Debug build (`CMAKE_BUILD_TYPE=Debug`) has `NDEBUG` NOT defined, so calling `assertRequires<T>()` with a missing component will `abort()` the test process
   - What's unclear: Whether to add a separate Release-mode CMake test configuration to cover DEP-03 in CI
   - Recommendation: Test DEP-03 only under `#ifdef NDEBUG` guard in the test file (same approach as ERR-04 Panic). Mark it in the test comments. A future CI improvement can add a Release-build test run.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Custom C (hand-rolled ASSERT macro, consistent with all existing tests) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `ctest --test-dir /home/unwn/dev/enjin/build -R gc_assert_test` |
| Full suite command | `ctest --test-dir /home/unwn/dev/enjin/build` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| GC-01 | `engine.lua.collect()` is type `function`, calls without error | unit (Lua) | `ctest --test-dir build -R gc_assert_test` | No — Wave 0 |
| GC-02 | `engine.lua.memory()` returns non-negative number | unit (Lua) | `ctest --test-dir build -R gc_assert_test` | No — Wave 0 |
| DEP-01 | `assertRequires<T>()` method exists on Component; happy path is no-op | unit (C++) | `ctest --test-dir build -R gc_assert_test` | No — Wave 0 |
| DEP-02 | Missing dependency aborts in debug | manual-only (abort kills process; verified by code inspection) | N/A — live abort | No |
| DEP-03 | Missing dependency logs + disables in release | unit (C++, NDEBUG-gated) | `ctest --test-dir build -R gc_assert_test` (Release build only) | No — Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest --test-dir /home/unwn/dev/enjin/build -R gc_assert_test`
- **Per wave merge:** `ctest --test-dir /home/unwn/dev/enjin/build`
- **Phase gate:** Full suite green (13 existing + 1 new = 14 tests) before verify-work

### Wave 0 Gaps
- [ ] `tests/gc_assert_test.cpp` — covers GC-01, GC-02, DEP-01, DEP-03
- [ ] `tests/CMakeLists.txt` entry for `gc_assert_test` inside `if(ENJIN2_BUILD_LUA)`

*(Framework and fixture infrastructure exist — see `engine_table_test.cpp` and `error_policy_test.cpp` for exact patterns to follow.)*

## Sources

### Primary (HIGH confidence)
- `/usr/include/lua.h` lines 330-341 — `LUA_GC*` constants confirmed for Lua 5.4.8
- `/home/unwn/dev/enjin/src/scripting/lua_platform.cpp` lines 116, 120 — `LUA_GCCOUNT + LUA_GCCOUNTB` memory formula
- `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` lines 61-63 — engine.lua stub comment "Phase 35 adds collect/memory"
- `/home/unwn/dev/enjin/include/enjin2/core/component.hpp` — Component base class structure (protected owner, enabled flag, awake() lifecycle)
- `/home/unwn/dev/enjin/include/enjin2/core/object.hpp` — `getComponent<T>()` and `hasComponent<T>()` template methods
- `/home/unwn/dev/enjin/.planning/STATE.md` — Phase 26 decision: `assertRequires<T>()` name; Phase 31 decision: engine.lua registered as empty table for Phase 35
- `/home/unwn/dev/enjin/build/CMakeCache.txt` — `CMAKE_BUILD_TYPE=Debug`, `LUA_LIBRARY=liblua5.4.so`
- `/home/unwn/dev/enjin/include/enjin2/scripting/bind_helpers.hpp` — `LuaFuncDef` + `luaBindFunctions` registration pattern

### Secondary (MEDIUM confidence)
- Lua 5.4 manual (training knowledge + confirmed via header): `LUA_GCSTEP` with `data=0` performs one incremental step; `LUA_GCCOLLECT` performs full cycle

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries and patterns verified in-codebase
- Architecture: HIGH — stub comment in bindings_engine.cpp literally names Phase 35; GC formula already in lua_platform.cpp
- Pitfalls: HIGH — LUA_GCSTEP vs LUA_GCCOLLECT verified from header; NDEBUG behavior verified from CMakeCache

**Research date:** 2026-02-27
**Valid until:** Stable (Lua 5.4 GC API is stable; CMake NDEBUG convention is permanent)
