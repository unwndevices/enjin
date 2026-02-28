# Phase 37: Address Prominent Codebase Concerns - Research

**Researched:** 2026-02-27
**Domain:** C++ game engine hardening — Lua proxy safety, ScriptProxy error semantics, ObjectProxy, clang-tidy CI, build hygiene, fixed-buffer conversion, component assertion, Lua tag bindings
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Scope & prioritization:**
- Primary focus: all 10 "Looks Done But Isn't" checklist items from CONCERNS.md
- Every item addressed — not triaged. Each gets either a verification test or a code fix, not just documentation.
- Infrastructure items (clang-tidy CI integration, build directory cleanup): actually wire up / clean up — not just documented.
- ESP32-specific audits: skip. Desktop-first only. Zero-alloc integrity concern is noted in CONCERNS.md; no embedded profiling in this phase.

**ScriptProxy error experience:**
- Stale proxy access (after scene destruction) must raise a Lua error — not return nil silently.
- Error message: `"object has been destroyed"` — short and direct. No object name context needed.
- Both `__index` (reads) and `__newindex` (writes) on a stale proxy raise this error.
- Test: store `self` in a Lua global during `init()`, transition scenes, access the stored proxy — assert that a Lua error is raised (not nil, not crash).

**Component limit assertion:**
- `MAX_COMPONENTS = 16`: add `assert(componentCount < MAX_COMPONENTS)` in debug builds; `fprintf(stderr, ...)` + `return nullptr` in release builds.
- No silent nullptr return — overflow is always visible.
- No size increase (16 stays as-is).

**std::string → fixed buffer:**
- Convert `C_LuaScript::errorMessage` from `std::string` to `char errorMessage[256]`.
- `scriptCode` and `scriptPath` remain as `std::string` (acceptable for desktop, loaded once at startup).

**Lua tag bindings (completing Phase 29):**
- Expose `self:addTag(tag)`, `self:hasTag(tag)`, `self:clearTags()` as ScriptProxy metamethods.
- C++ implementation exists; this is a bindings gap only.
- `self.name` remains read-only from Lua (Claude's discretion — mutating names that C++ lookup may rely on is risky).

**ObjectProxy for engine.scene.find():**
- `engine.scene.find(name)` currently returns a raw `Object*` as lightuserdata — dangling pointer risk after scene transition.
- Wrap in a new `ObjectProxy` userdata with a `valid` flag (same pattern as ScriptProxy).
- ObjectProxy exposes full access: `name` (read), `hasTag(tag)`, `position` (read/write), component enable/disable control.
- Accessing a stale ObjectProxy raises the same `"object has been destroyed"` Lua error.

**clang-tidy CI enforcement:**
- Add a CMake `lint` target that runs clang-tidy against `src/**/*.cpp`.
- Integrate into CI so new warnings fail the build.
- Actually wired up and working — not just a TODO comment.

**Build directory cleanup:**
- Remove all non-`build/` build directories from the repository root (`build_21_off`, `build_21_on`, `build_22_*`, etc.).
- Keep only the main `build/` directory.
- Document any platform-specific build procedures if relevant.

### Claude's Discretion
- Whether `self.name` becomes writable from Lua (decision: keep read-only, rationale above).
- Exact implementation of clang-tidy CMake target and CI step format.
- ObjectProxy struct layout and GC finalization strategy.
- How to handle `engine.scene.findAllWithTag()` — if it returns a list of ObjectProxies, the approach should be consistent with `engine.scene.find()`.

### Deferred Ideas (OUT OF SCOPE)
- None — discussion stayed within phase scope.

</user_constraints>

## Summary

Phase 37 is a pure hardening and closure phase — no new capabilities. It closes the 10 "Looks Done But Isn't" items from `.planning/codebase/CONCERNS.md`, adds four targeted code fixes, and removes build artifacts. The work divides into five distinct categories: (1) Lua proxy safety (ScriptProxy error-on-stale + ObjectProxy), (2) C++ safety (component limit assertion + fixed error buffer), (3) Lua API completeness (tag bindings), (4) static analysis enforcement (clang-tidy CMake target + CI), and (5) repository hygiene (build directory cleanup).

All patterns needed already exist in the codebase. ScriptProxy with `valid` flag + metatable dispatch is the established model in `bindings.cpp`. The `lua_error()` / `luaL_error()` API is already used in other error paths. The `luaNewUserdata` + `luaL_newmetatable` pattern from Phase 32 is directly reusable for ObjectProxy. The CMake option flag pattern (`option(ENJIN2_BUILD_LUA ...)`) is the existing gating mechanism; the same approach applies for `CLANG_TIDY`.

Research confirms: every locked decision is technically sound and implementable without introducing new dependencies. The main risk area is ObjectProxy GC finalization — specifically ensuring `valid` is set to false when a scene's ObjectCollection destroys objects. The research section on architecture patterns details the recommended invalidation hook.

**Primary recommendation:** Implement in three plans — Plan 01: ScriptProxy error + component assertion + fixed buffer + tag bindings. Plan 02: ObjectProxy. Plan 03: clang-tidy CMake + CI + build cleanup + verification of all 10 checklist items.

## Standard Stack

### Core

This phase uses no new libraries. All needed primitives are already in the codebase.

| Component | Already Present | Purpose in Phase 37 |
|-----------|----------------|----------------------|
| Lua 5.1 C API (LuaJIT) | Yes — `lua.h`, `lauxlib.h` | `luaL_error()` for stale proxy errors; `lua_newuserdata()` + `luaL_newmetatable()` for ObjectProxy |
| `enjin2::ScriptProxy` | Yes — `bindings.hpp:36-39` | Extend with error-on-stale in `__index` / `__newindex` |
| `enjin2::Object` tag API | Yes — `object.hpp:244-271` | Source for ObjectProxy and tag bindings |
| CTest / CMake | Yes | Add `lint` custom target |
| clang-tidy | Yes — `.clang-tidy` config present | Wire into CMake via `add_custom_target` |

### Supporting

| Component | Source | Purpose |
|-----------|--------|---------|
| `luaL_error()` | Lua 5.1 API | Raise a Lua error from C — use in stale-proxy handler |
| `lua_pushlightuserdata` + `lua_newuserdata` | Lua 5.1 API | ObjectProxy allocation |
| `add_custom_target` / `option()` | CMake | clang-tidy lint target |
| `assert()` + `NDEBUG` | Standard C | Component limit debug path |
| `fprintf(stderr, ...)` | Standard C | Component limit release path |
| `snprintf` | Standard C | Safe write into `char errorMessage[256]` |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `luaL_error()` in stale proxy | Return nil | User decision: nil is silent failure. Error required. |
| `lua_newuserdata` for ObjectProxy | `lua_pushlightuserdata` | Lightuserdata has no metatable (established in Phase 26 notes). Full userdata required. |
| `char errorMessage[256]` | `std::string` | std::string allocates heap. char[] is zero-alloc. User decision is fixed. |
| CMake option flag `CLANG_TIDY` | `set(CMAKE_CXX_CLANG_TIDY ...)` | Per-build-system integration; option flag decouples from default build, enables CI opt-in. |

## Architecture Patterns

### Pattern 1: ScriptProxy Stale Error Path

**What:** Replace silent nil-return in `__index`/`__newindex` with `luaL_error()` when `proxy->valid == false`.
**When to use:** Applied only to stale-proxy case. Valid proxies continue to dispatch normally.

**Current code (bindings.cpp:27-30):**
```cpp
if (!proxy || !proxy->valid || !proxy->component) {
    lua_pushnil(L);   // Current: silent nil
    return 1;
}
```

**New code:**
```cpp
if (!proxy) {
    lua_pushnil(L);
    return 1;
}
if (!proxy->valid || !proxy->component) {
    luaL_error(L, "object has been destroyed");  // Raises Lua error — never returns
    return 0;
}
```

**Key: `luaL_error` does a `longjmp` — it never returns normally. Do NOT put cleanup after it.**

Apply identically in both `lua_proxy_index_impl` and `lua_proxy_newindex_impl`.

### Pattern 2: ObjectProxy Struct and Metatable

**What:** New `ObjectProxy` userdata struct placed in `bindings.hpp` alongside `ScriptProxy`. Registered with its own metatable (`"ObjectProxy"`). Uses same `valid` flag pattern.

**ObjectProxy struct (add to bindings.hpp):**
```cpp
/**
 * @brief Lua proxy userdata wrapping a raw Object* from engine.scene.find().
 * Validity flag prevents dangling-pointer access after scene transition.
 * Invalidated by Scene::destroyObject() or scene teardown.
 */
struct ObjectProxy {
    Object* object;   ///< Non-owning pointer. Do NOT dereference if valid == false.
    bool valid;       ///< Set to false when the Object is destroyed/scene transitions.
};
```

**ObjectProxy `__index` fields to expose:**
- `name` (read-only): `object->getName()` — returns string or nil
- `position` (read/write struct-style): `object->getPosition()` — returns `{x, y}` table or reads x/y from proxy
- `hasTag(tag)` (method call via `__index` returning function): calls `object->hasTag(tag)`
- `enable` / `disable` (fields): component enable/disable via `object->getComponent<C_LuaScript>()->setEnabled(bool)`

**Note on method dispatch:** Lua method calls (`proxy:hasTag("enemy")`) require `__index` to return a function when the key is a method name. This is the established pattern from Vec2 methods in `bindings.cpp` (see `lua_Vec2_index`).

**ObjectProxy registration (in registerEngineTable or a dedicated registerObjectProxyMetatable):**
```cpp
// In LuaBindings — called from registerAll()
static constexpr const char* OBJECT_PROXY_METATABLE = "ObjectProxy";

void LuaBindings::registerObjectProxyMetatable() {
    lua_State* L = engine->getState();
    if (luaL_newmetatable(L, OBJECT_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_objproxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_objproxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
        // __gc for safety (optional but recommended — clears valid flag on collection)
        lua_pushcfunction(L, lua_objproxy_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);
}
```

**Allocation in `lua_engine_scene_find` (replacing lightuserdata push):**
```cpp
int LuaBindings::lua_engine_scene_find(lua_State* L) {
    // ... get scene ...
    Object* obj = scene->findByName(name);
    if (!obj) { lua_pushnil(L); return 1; }

    // Allocate ObjectProxy userdata
    auto* proxy = static_cast<ObjectProxy*>(lua_newuserdata(L, sizeof(ObjectProxy)));
    proxy->object = obj;
    proxy->valid  = true;
    luaL_getmetatable(L, OBJECT_PROXY_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}
```

### Pattern 3: ObjectProxy Invalidation

**What:** Scenes/ObjectCollections must set `proxy->valid = false` before destroying the Object.

**Problem:** ObjectProxy is a Lua userdata — C++ has no direct reference to all live proxies. Options:

1. **Invalidate at scene transition** (recommended): When `Scene::deactivate()` is called, all its objects are destroyed. Add a hook in `Scene` that iterates objects and calls `invalidateObjectProxy()` on each before destruction. Each Object stores a `ObjectProxy*` pointer (nullable) set when `lua_engine_scene_find` creates the proxy.

2. **Invalidate via Object destructor**: Object's destructor zeroes a stored `ObjectProxy*` pointer's `valid` flag. This is clean, but requires Object to know about ObjectProxy (creates cross-dependency). Mitigate with a forward declaration + callback approach.

**Recommended approach:** Add `ObjectProxy* m_luaProxy` (nullable) to `Object`. Set in `scene_find`. Zero `m_luaProxy->valid = false` in Object's destructor (Object already knows its destructor is called when scene destroys it). Requires `Object` to include a forward declaration of `ObjectProxy` — acceptable since ObjectProxy is in `bindings.hpp` which is already a separate concern.

**Alternative simpler approach:** Since scenes tear down atomically, add an `invalidateAllObjectProxies()` call at the start of `Scene::deactivate()` or `Scene::onDeactivate()`. This is less precise but works for the scene-transition use case the decision describes.

### Pattern 4: Tag Bindings on ScriptProxy

**What:** `self:addTag("enemy")` / `self:hasTag("enemy")` / `self:clearTags()` called as Lua methods.

**Implementation location:** `lua_proxy_index_impl` in `bindings.cpp` — add method-name dispatch before the `lua_pushnil` fallthrough.

**Pattern (reuses Vec2 method dispatch style):**
```cpp
} else if (strcmp(key, "addTag") == 0) {
    lua_pushcfunction(L, lua_proxy_addTag_impl);
    return 1;  // Returns a function; Lua calls it with (self, tag)
} else if (strcmp(key, "hasTag") == 0) {
    lua_pushcfunction(L, lua_proxy_hasTag_impl);
    return 1;
} else if (strcmp(key, "clearTags") == 0) {
    lua_pushcfunction(L, lua_proxy_clearTags_impl);
    return 1;
}
```

**Method implementations:**
```cpp
// lua_proxy_addTag_impl: stack [1]=proxy, [2]=tag_string
static int lua_proxy_addTag_impl(lua_State* L) {
    auto* proxy = static_cast<ScriptProxy*>(luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* tag = luaL_checkstring(L, 2);
    Object* owner = proxy->component->getOwner();
    if (owner) owner->addTag(tag);
    return 0;
}
```

**Note on tag string lifetime:** `Object::addTag(const char*)` stores a raw pointer. For strings passed from Lua scripts, the Lua state owns the interned string memory — this is safe as long as the Lua state outlives the Object (which it does: Lua state is destroyed after C_LuaScript destructor). Tags must not be concatenated or formatted; only string literals or Lua-interned strings are safe.

### Pattern 5: Component Limit Assertion

**What:** In `object.hpp`, the `addComponent<T>` early-exit is silent:
```cpp
if (componentCount >= MAX_COMPONENTS) {
    return nullptr;  // Current: silent
}
```

**New code:**
```cpp
if (componentCount >= MAX_COMPONENTS) {
#ifndef NDEBUG
    assert(false && "addComponent: MAX_COMPONENTS (16) exceeded — increase limit or reduce components");
#else
    fprintf(stderr, "[enjin2] addComponent: MAX_COMPONENTS (%zu) exceeded\n",
            static_cast<size_t>(MAX_COMPONENTS));
#endif
    return nullptr;
}
```

**Placement:** In the template body in `object.hpp` — this is a header-only template, no .cpp needed.

### Pattern 6: errorMessage Fixed Buffer Conversion

**What:** Change `std::string errorMessage` in `C_LuaScript` to `char errorMessage[256]`.

**Files to touch:**
1. `include/enjin2/components/lua_script.hpp`: Change field declaration; update `getErrorMessage()` return type from `const std::string&` to `const char*`.
2. `src/components/lua_script.cpp`: Replace all `errorMessage = ...` string assignments with `snprintf(errorMessage, sizeof(errorMessage), "%s", ...)`.

**Public API change:** `getErrorMessage()` return type changes from `const std::string&` to `const char*`. Callers use it only in tests (`error_policy_test.cpp`). Check for call sites:
- `error_policy_test.cpp` — uses `getErrorMessage()` to inspect error messages. Update calls to compare with `strcmp` or `std::string(script->getErrorMessage())` for test assertions.

**Header change:**
```cpp
// Before:
std::string errorMessage;
const std::string& getErrorMessage() const { return errorMessage; }

// After:
char errorMessage[256]{};
const char* getErrorMessage() const { return errorMessage; }
```

### Pattern 7: clang-tidy CMake Lint Target

**What:** Add `option(CLANG_TIDY "Run clang-tidy lint checks" OFF)` and `add_custom_target(lint ...)` to root `CMakeLists.txt`.

**Implementation:**
```cmake
# -- Static analysis (clang-tidy) -----------------------------------------
option(CLANG_TIDY "Enable clang-tidy lint target" OFF)
if(CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        # Collect all C++ source files under src/
        file(GLOB_RECURSE ENJIN2_SOURCES CONFIGURE_DEPENDS
            ${CMAKE_SOURCE_DIR}/src/*.cpp)
        add_custom_target(lint
            COMMAND ${CLANG_TIDY_EXE}
                -p ${CMAKE_BINARY_DIR}
                ${ENJIN2_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy on enjin2 sources..."
        )
        message(STATUS "clang-tidy lint target enabled: cmake --build build --target lint")
    else()
        message(WARNING "CLANG_TIDY=ON but clang-tidy not found")
    endif()
endif()
```

**CI integration:** Add a CI step that configures with `-DCLANG_TIDY=ON` and runs `cmake --build build --target lint`. The custom target exits non-zero when clang-tidy finds errors (because clang-tidy returns non-zero on warnings-as-errors). Check `.clang-tidy` — `WarningsAsErrors: ''` means no warnings are promoted to errors by default. For CI enforcement, add `WarningsAsErrors: '*'` or use `--warnings-as-errors='*'` on the command line.

**Note:** The `.clang-tidy` `WarningsAsErrors` field is currently empty string (no warnings-as-errors). For CI to fail on new warnings, use the command-line flag `--warnings-as-errors='*'` in the custom target command, or change the `.clang-tidy` field. Recommend the command-line approach to keep `.clang-tidy` clean.

### Pattern 8: Build Directory Cleanup

**What:** Remove 14 leftover build directories. The directories are NOT tracked by git (per `.gitignore`), so this is a filesystem operation only.

**Directories to remove:**
```
build_21_off/  build_21_on/  build_22_check/  build_22_sdl_lua/
build_22_sdl_nolua/  build_24_check/  build_24_lua/  build_25_check/
build_25_verify/  build_off/  build_sdl_test/  build_test_20/
build_test_sprite/  build_wasm/
```

**Command:**
```bash
cd /path/to/enjin && rm -rf build_21_off build_21_on build_22_check build_22_sdl_lua \
    build_22_sdl_nolua build_24_check build_24_lua build_25_check build_25_verify \
    build_off build_sdl_test build_test_20 build_test_sprite build_wasm
```

**Keep:** `build/` (main active build with `compile_commands.json` symlinked), `build_wasm.sh` (shell script, not a directory).

### Recommended Plan Structure

Based on the 10 checklist items and four code fixes, the work groups naturally into three plans:

**Plan 01 — ScriptProxy + C++ fixes:**
- ScriptProxy: `luaL_error("object has been destroyed")` in `__index` + `__newindex`
- ScriptProxy tag methods: `addTag`, `hasTag`, `clearTags`
- Component limit: assertion in `object.hpp` addComponent template
- Error buffer: `errorMessage` char array conversion in `lua_script.hpp` + `lua_script.cpp`
- Test: `script_proxy_lifetime_test.cpp` — stale proxy raises error

**Plan 02 — ObjectProxy:**
- New `ObjectProxy` struct in `bindings.hpp`
- `registerObjectProxyMetatable()` in `bindings.cpp`
- Update `lua_engine_scene_find` to return ObjectProxy instead of lightuserdata
- ObjectProxy invalidation mechanism (via Object destructor or Scene deactivation)
- ObjectProxy exposes: `name` (read), `hasTag`, `position` (read/write), component enable/disable
- Test: `object_proxy_test.cpp` — stale ObjectProxy raises error

**Plan 03 — CI + verification:**
- clang-tidy CMake `lint` target (with `--warnings-as-errors='*'` for CI)
- Build directory cleanup (rm -rf)
- Verification sweep: run all 16 ctests, verify each of the 10 checklist items is closed

### Anti-Patterns to Avoid

- **Returning nil on stale proxy:** User decision requires `luaL_error`. Do not soft-fail.
- **Using `lua_pushlightuserdata` for ObjectProxy:** Lightuserdata cannot have a metatable (confirmed Phase 26 decision). Use `lua_newuserdata`.
- **Storing Lua-formatted tag strings:** `Object::addTag` stores raw `const char*`. Never pass a Lua `lua_tostring` result that may be gc'd. Pass only interned strings (`luaL_checkstring` interns in Lua 5.1).
- **Putting code after `luaL_error`:** `luaL_error` is a `longjmp` — code after it is unreachable and will trigger compiler warnings.
- **`WarningsAsErrors` in `.clang-tidy`:** Setting this field there affects developer local runs. Prefer the command-line flag in the CI CMake custom target.
- **Increasing `MAX_COMPONENTS`:** User decision: keep at 16. Only add assertion, not increase.
- **Making `self.name` writable:** User decision: keep read-only. Do not add `"name"` to `__newindex` dispatch.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Raising Lua errors from C | Custom return codes or error tables | `luaL_error(L, "message")` (Lua 5.1 API) | Built-in longjmp-based error propagation; any other mechanism bypasses Lua's pcall protection |
| Lua userdata registration | Custom registry scheme | `luaL_newmetatable` + `lua_setmetatable` (already used for ScriptProxy) | Established pattern in this codebase; safe reuse |
| Tag string copying | Manual strdup/strcpy | Direct `Object::addTag(const char*)` — Lua interns strings automatically | `luaL_checkstring` returns interned pointer; safe to store as raw const char* |
| clang-tidy runner | Shell scripts | `add_custom_target` with `find_program(CLANG_TIDY_EXE ...)` | CMake-native; works cross-platform |

**Key insight:** All mechanisms needed for this phase are already present in the codebase. Phase 37 is pattern reuse, not pattern invention.

## Common Pitfalls

### Pitfall 1: `luaL_error` is a noreturn — Missing Compiler Warning
**What goes wrong:** Code placed after `luaL_error(L, ...)` is unreachable. Compilers may warn or the code silently never executes.
**Why it happens:** `luaL_error` calls `lua_error` which is `__attribute__((noreturn))` via longjmp.
**How to avoid:** Write `return 0;` after `luaL_error` only as a style convention to satisfy non-void return type. The return is never reached.
**Warning signs:** Compiler warnings about unreachable code after `luaL_error` are expected and harmless.

### Pitfall 2: ObjectProxy Invalidation Race — Double Valid=False
**What goes wrong:** If `valid = false` is set twice (once in destructor, once in scene deactivation), no crash occurs (idempotent), but if the `Object*` pointer is already freed when valid is set the second time, it's a use-after-free on the proxy struct itself.
**Why it happens:** Object destructor sets `proxy->valid = false` via the stored pointer — but the proxy's `lua_newuserdata` memory is owned by Lua, not C++. The proxy struct lives in Lua heap until GC'd.
**How to avoid:** Only access `proxy->valid` (the bool field), never `proxy->object` after setting valid=false. The bool field is always safe to write since the proxy userdata is GC'd separately by Lua.

### Pitfall 3: Tag String Lifetime After Lua GC
**What goes wrong:** Lua strings are garbage collected. If a tag pointer stored in `Object` points to a Lua string that is later GC'd, the Object holds a dangling pointer.
**Why it happens:** `lua_tostring` returns a pointer to an interned Lua string. In Lua 5.1, strings are interned and persist as long as they are referenced somewhere in the Lua state. If the Lua state is closed, all strings are freed.
**How to avoid:** Tags stored from Lua are safe as long as the Lua state outlives the Object. Since `C_LuaScript` destructs before `lua_close`, this invariant holds. Document this in the binding code. Do NOT use tags across lua_close/reload without re-setting them.

### Pitfall 4: `getErrorMessage()` Return Type Change Breaks Test Assertions
**What goes wrong:** `error_policy_test.cpp` uses `getErrorMessage()`. If the return type changes from `const std::string&` to `const char*`, string comparison in tests may use `==` on pointers (always false in C++).
**Why it happens:** Tests may be written as `script->getErrorMessage() == "some error"` which compares `const char*` pointer identity, not content.
**How to avoid:** After the conversion, grep for all `getErrorMessage()` call sites and ensure they use `strcmp()`, `strstr()`, or wrap with `std::string(script->getErrorMessage())` for assertion.

### Pitfall 5: clang-tidy `WarningsAsErrors` Config vs. CI Flag Mismatch
**What goes wrong:** Setting `WarningsAsErrors: '*'` in `.clang-tidy` makes local developer runs fail on any warning, which may be too strict before codebase is clean.
**Why it happens:** `.clang-tidy` is applied globally to all runs including developer workstations.
**How to avoid:** Keep `.clang-tidy` `WarningsAsErrors: ''` (current state). Use `--warnings-as-errors='*'` only in the CI CMake custom target command string.

### Pitfall 6: `cmake --build . --target lint` Requires Fresh Compile DB
**What goes wrong:** clang-tidy needs `compile_commands.json` from the build directory. If the build directory is stale or the DB is missing, clang-tidy produces incorrect results or fails to find headers.
**Why it happens:** `-p build` flag points to the build directory; the DB must exist before lint runs.
**How to avoid:** Ensure CI runs CMake configure + build before running the lint target. Document the lint invocation as: `cmake -B build -DCLANG_TIDY=ON && cmake --build build --target lint`.

## Code Examples

Verified patterns from codebase inspection:

### ScriptProxy Stale Error (update existing bindings.cpp)
```cpp
// Source: bindings.cpp:27-30 (current), updated for Phase 37
static int lua_proxy_index_impl(lua_State* L) {
    if (!lua_isuserdata(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(lua_touserdata(L, 1));
    if (!proxy) {
        lua_pushnil(L);
        return 1;
    }
    if (!proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;  // unreachable — luaL_error longjmps
    }
    // ... dispatch continues ...
}
```

### ObjectProxy Allocation in scene.find (update bindings_engine.cpp)
```cpp
// Source: bindings_engine.cpp:94-107 (current)
int LuaBindings::lua_engine_scene_find(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto* scene = static_cast<Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!scene) { lua_pushnil(L); return 1; }
    const char* name = luaL_checkstring(L, 1);
    Object* obj = scene->findByName(name);
    if (!obj) { lua_pushnil(L); return 1; }

    // Phase 37: replace lightuserdata with ObjectProxy userdata
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        lua_newuserdata(L, sizeof(enjin2::ObjectProxy)));
    proxy->object = obj;
    proxy->valid  = true;
    luaL_getmetatable(L, "ObjectProxy");
    lua_setmetatable(L, -2);
    return 1;
}
```

### Tag Method Dispatch in ScriptProxy __index (bindings.cpp)
```cpp
// Add after existing property dispatches in lua_proxy_index_impl
} else if (strcmp(key, "addTag") == 0) {
    lua_pushcfunction(L, lua_proxy_addTag_impl);
    return 1;
} else if (strcmp(key, "hasTag") == 0) {
    lua_pushcfunction(L, lua_proxy_hasTag_impl);
    return 1;
} else if (strcmp(key, "clearTags") == 0) {
    lua_pushcfunction(L, lua_proxy_clearTags_impl);
    return 1;
}
```

### Component Limit Assertion (object.hpp addComponent template)
```cpp
// Source: object.hpp:105-107 (current)
if (componentCount >= MAX_COMPONENTS) {
#ifndef NDEBUG
    assert(false && "addComponent: MAX_COMPONENTS exceeded — reduce component count or increase limit");
#else
    fprintf(stderr, "[enjin2] addComponent: MAX_COMPONENTS (%zu) exceeded\n",
            static_cast<size_t>(MAX_COMPONENTS));
#endif
    return nullptr;
}
```

### errorMessage Fixed Buffer (lua_script.hpp and .cpp)
```cpp
// lua_script.hpp — field change
char errorMessage[256]{};

// lua_script.hpp — accessor change
const char* getErrorMessage() const { return errorMessage; }

// lua_script.cpp — assignment change (replace std::string assignment)
snprintf(errorMessage, sizeof(errorMessage), "%s", result.errorMessage.c_str());
```

### clang-tidy CMake Target (CMakeLists.txt)
```cmake
option(CLANG_TIDY "Enable clang-tidy lint target" OFF)
if(CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        file(GLOB_RECURSE ENJIN2_SOURCES CONFIGURE_DEPENDS
            ${CMAKE_SOURCE_DIR}/src/*.cpp)
        add_custom_target(lint
            COMMAND ${CLANG_TIDY_EXE}
                -p ${CMAKE_BINARY_DIR}
                --warnings-as-errors=*
                ${ENJIN2_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy (errors-as-warnings=* mode)..."
        )
    endif()
endif()
```

### Test Pattern for Stale Proxy
```cpp
// script_proxy_lifetime_test.cpp (new file)
// Pattern reuses existing test fixture from error_policy_test.cpp
static void test_proxy_stale_raises_error() {
    printf("--- PROXY-STALE: stale proxy raises Lua error ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);

    // Script stores self in global during init, then tries to read it later
    static const char* k_storeScript =
        "function init(self)\n"
        "    stored = self\n"
        "end\n"
        "function access_stored()\n"
        "    return stored.x\n"  // Access stale proxy
        "end\n";

    script->loadScript(k_storeScript);
    // Manually trigger init (or drive via update)
    // ... then invalidate proxy by destroying obj ...

    // After obj destruction (or proxy invalidation), call access_stored
    // Expect: lua_pcall returns non-zero (error), not nil
    // Test mechanism: check error string or use a separate executeString
}
```

## State of the Art

| Old Approach | Current Approach | Phase | Impact |
|--------------|------------------|-------|--------|
| Lightuserdata for scene.find() | ObjectProxy with metatable | Phase 37 | Dangling pointer risk eliminated |
| Silent nil on stale proxy | `luaL_error("object has been destroyed")` | Phase 37 | Scripts get immediate feedback instead of silent failure |
| Silent nullptr on MAX_COMPONENTS overflow | assert + fprintf | Phase 37 | Developer catches overflow immediately |
| `std::string errorMessage` | `char errorMessage[256]` | Phase 37 | Zero-heap-allocation for error state |
| No Lua tag bindings | `self:addTag/hasTag/clearTags` | Phase 37 | Completes Phase 29 Lua API |
| clang-tidy manual-only | CMake `lint` target + CI | Phase 37 | Static analysis enforced automatically |
| 14 leftover build/ dirs | Single `build/` | Phase 37 | Repository hygiene |

**No deprecated patterns being introduced.** This phase only hardens existing patterns.

## Open Questions

1. **ObjectProxy invalidation hook location**
   - What we know: `Object::~Object()` is called when scene destroys objects. ObjectProxy needs `valid = false` at that point.
   - What's unclear: Whether adding `ObjectProxy* m_luaProxy` to `Object` is acceptable (requires Object to know about the scripting layer), or whether Scene-level mass-invalidation is cleaner.
   - Recommendation: Use Object destructor approach with a `ObjectProxy* m_luaProxy = nullptr` field in Object. Forward-declare ObjectProxy in object.hpp to avoid circular include. This ensures precise invalidation even if objects are removed mid-scene.

2. **`engine.scene.findAllWithTag()` implementation**
   - What we know: CONTEXT.md notes this as Claude's discretion — approach should be consistent with `engine.scene.find()`.
   - What's unclear: Whether to add this in Phase 37 or leave it for v2. `findAllWithTag()` would return a Lua table of ObjectProxies (not a fixed array — Lua tables are dynamic).
   - Recommendation: Defer to v2 unless it's one of the 10 checklist items. It is NOT in the checklist. Do not implement in Phase 37.

3. **Error policy coordination (checklist item 7)**
   - What we know: CONCERNS.md identifies the interaction between per-component `ScriptErrorPolicy::Disable` and a hypothetical global `lua_ok` gate. CONTEXT.md does not prescribe a fix, only says "verify in error_policy_test.cpp".
   - What's unclear: Whether a global `lua_ok` gate exists in the current SDL runner (`src/sdl/...`).
   - Recommendation: Read the SDL runner source and error_policy_test.cpp to confirm: (a) Disable policy affects only the component's enabled flag, not a global gate; (b) add a test that creates two C_LuaScript components, triggers error on one with Disable policy, verifies the other still calls update().

4. **Input callback frame timing test (checklist item 8)**
   - What we know: The test needs to verify `on_button_pressed` fires in the same frame as the button press (same `engine.time.frame()` value).
   - What's unclear: Whether `engine.time.frame()` is accessible from within `on_button_pressed` callback in the current test harness (test may not drive the time state).
   - Recommendation: Add a test helper that increments a C-side frame counter and checks it is equal in both the button callback and the subsequent update callback.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection — `bindings.cpp`, `bindings_engine.cpp`, `bindings.hpp`, `lua_script.hpp`, `object.hpp` — all patterns confirmed from source
- `.planning/codebase/CONCERNS.md` — authoritative list of the 10 checklist items
- `37-CONTEXT.md` — locked implementation decisions
- `.clang-tidy` — existing configuration confirmed; `WarningsAsErrors: ''` confirmed

### Secondary (MEDIUM confidence)
- Lua 5.1 Reference Manual (knowledge of `luaL_error`, `lua_newuserdata`, `luaL_newmetatable`) — well-established API, confirmed from codebase usage patterns
- CMake documentation for `add_custom_target`, `find_program`, `file(GLOB_RECURSE ...)` — standard CMake 3.16+ features, already used in the project's CMakeLists.txt

### Tertiary (LOW confidence)
- None — all claims are sourced from the codebase or Lua 5.1 reference

## Metadata

**Confidence breakdown:**
- ScriptProxy stale error: HIGH — pattern directly from existing code, `luaL_error` is standard
- ObjectProxy struct: HIGH — mirrors ScriptProxy exactly; invalidation mechanism requires design decision (open question 1)
- Tag bindings: HIGH — C++ API exists; pattern mirrors Vec2 method dispatch already in codebase
- Component limit assertion: HIGH — template is in header; pattern is assert+fprintf (same as assertRequires<T>())
- errorMessage buffer: HIGH — mechanical conversion; API change is the only risk (open question handled)
- clang-tidy CMake: HIGH — CMake patterns used throughout; command verified against .clang-tidy config
- Build cleanup: HIGH — pure filesystem operation, directories confirmed from ls output

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable domain — no external dependencies being added)
