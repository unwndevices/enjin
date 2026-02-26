# Stack Research

**Domain:** Embedded 2D game engine — Lua scripting foundation + C++ engine enhancements (v1.5)
**Researched:** 2026-02-26
**Confidence:** HIGH (all conclusions drawn from live codebase + verified Lua C API headers)

---

## Scope

This document covers **only stack additions and API-level decisions for v1.5**. It does not re-research validated v1.4 capabilities (LuaJIT, SDL3, Canvas4, InputState, LayerCompositor, SpriteSheet, hot reload, etc.).

---

## Recommended Stack

### Core Technologies (unchanged, verified present)

| Technology | Version | Role |
|------------|---------|------|
| LuaJIT | 2.1.1753364724 (API: Lua 5.1.4) | Scripting runtime — static pool allocator, custom panic handler |
| C++17 | `CMAKE_CXX_STANDARD 17` in CMakeLists.txt | `if constexpr`, `std::string_view`, `std::array`, template SFINAE |
| SDL3 | Present (enjin2_sdl CMake target) | Desktop runner — event loop, input, frame timing |

**No new external dependencies are introduced by v1.5.** Every feature is implemented via:
- Lua C API (already linked via LuaJIT)
- C++17 stdlib (`<array>`, `<string_view>`, `<type_traits>`, `<cstring>`)
- Existing enjin2 headers

---

### Feature-by-Feature Stack Decisions

#### 1. float dt everywhere

**What changes:** `uint16_t deltaTime` (milliseconds) → `float dt` (seconds) in all virtual methods.

Files affected:
- `include/enjin2/core/component.hpp` — `update(uint16_t)`, `lateUpdate(uint16_t)`
- `include/enjin2/core/object.hpp` — `update(uint16_t)`, `lateUpdate(uint16_t)`
- `include/enjin2/core/scene.hpp` — `update(uint16_t)`, `onUpdate(uint16_t)`
- `include/enjin2/core/object_collection.hpp` — `update(uint16_t)`, `lateUpdate(uint16_t)`
- `include/enjin2/core/scene_state_machine.hpp` — `update(uint16_t)`, `updateTransition(uint16_t)`
- All component subclasses that override `update()`

**Type choice: `float` (not `double`).**
- ESP32-S3 has a hardware single-precision FPU. `double` operations are software-emulated — 3-10x slower.
- `float` provides ~7 significant digits. At 120fps, dt ≈ 0.00833s — fully representable without accumulator rounding.
- Unity, LOVE2D, Defold all use `float` seconds. The SDL runner already computes `float dt` correctly.
- The SDL runner already passes `dt` as `float` to `g_lua.callFunction("update", dt)` (line 265 in sdl_main.cpp). C++ side is the only inconsistency.

**Lua side:** `update(self, dt)` — `dt` is a `lua_Number` (double in LuaJIT). C++ passes `static_cast<lua_Number>(dt)` via `lua_pushnumber`. No precision concern for Lua scripts.

**No new library needed.**

---

#### 2. Named Objects + Tags

**`Object::name` field type: `std::string`** — not `std::string_view`.
- `std::string_view` cannot own data. Names are set once at construction, must outlive the object.
- ESP32 concern: `std::string` SSO (Small String Optimization) in libstdc++/libc++ stores strings ≤15 chars inline — typical names ("player", "enemy_01", "hud") fit without heap allocation.
- The object's name is set once and never changed — no hot-path allocation.

**`ObjectCollection` name map type:**

Two valid approaches depending on target:

| Approach | When | Implementation |
|----------|------|---------------|
| `std::unordered_map<std::string, Object*>` | SDL3 / WASM | O(1) lookup, heap-allocated buckets — fine on desktop |
| Linear scan of existing `objects[]` array | ESP32 / zero-alloc constraint | O(n) over max 128 objects, no additional memory |

**Recommendation: linear scan with a `findByName(std::string_view)` method on `ObjectCollection`.**
- The existing `forEach` pattern is already the collection traversal idiom.
- Name lookups happen on scene setup or Lua `engine.scene.find()` — never every frame.
- Avoids adding a heap-allocated map to a type that currently has zero heap allocation.
- If profiling reveals real bottleneck on desktop, add the unordered_map as an optional build-time flag.

**Tag storage on `Object`: `std::array<const char*, 8>`**
- Tags are `const char*` string literals defined at compile time (e.g., `"enemy"`, `"collidable"`).
- 8-slot fixed capacity — zero allocation, zero heap, embedded-safe.
- `const char*` comparison uses `strcmp` not pointer equality — different string literals with same content must match.
- 8 tags per object is sufficient for the engine's complexity level. Increase via `ENJIN2_MAX_TAGS` compile constant if needed.

**No new library.** `<array>`, `<string_view>`, `<cstring>` are C++17 stdlib, already included.

---

#### 3. Scene Self-Transitions

**Injection pattern: store `SceneStateMachine*` (non-owning raw pointer) in `Scene`.**

**Include guard for circular dependency:**
- `scene_state_machine.hpp` currently includes `scene.hpp`.
- Adding `SceneStateMachine*` to `Scene` would create a circular include.
- **Fix:** forward-declare `class SceneStateMachine;` in `scene.hpp`, add `#include "scene_state_machine.hpp"` only in `scene.cpp`.
- This is the standard C++ forward-declaration technique for circular class relationships.

**Activation injection pattern:**
```cpp
// In SceneStateMachine::completeTransition() or activate():
currentScene->setSM(this);  // inject before calling activate()
currentScene->activate();
```

The `Scene` base class gains:
```cpp
private:
    SceneStateMachine* sm_ = nullptr;  // non-owning, valid during active lifetime only

public:
    void setSM(SceneStateMachine* sm) { sm_ = sm; }
```

**Alternative: `std::function<void(uint32_t)>` callback.** Also valid — avoids the forward-declaration requirement. Downside: `std::function` may heap-allocate for captures. For zero-alloc constraint, raw pointer is cleaner.

**`SceneStateMachine::changeScene(uint32_t)` already exists** — the target method. No new SSM API needed for the Lua binding `engine.scene.switch(id)`.

---

#### 4. engine.* Global Table

**Registration pattern: manual `lua_newtable` + `lua_pushcfunction` + `lua_setfield` chain.**

This is the pattern already used in `bindings.cpp` for `love.graphics`. The codebase has a working precedent.

**Critical constraint: `luaL_newlib` (Lua 5.2+ API) is NOT available in LuaJIT 2.1's Lua 5.1 API.** Do not use it. HIGH confidence — verified against `luajit/src/lua.h` (`LUA_VERSION_NUM 501`).

Full pattern:
```cpp
// In LuaBindings::registerAll():
lua_State* L = engine->getState();

// Create engine table
lua_newtable(L);                              // stack: [engine_tbl]

  // engine.scene subtable
  lua_newtable(L);                            // stack: [engine_tbl, scene_tbl]
    lua_pushcfunction(L, lua_engine_scene_switch);
    lua_setfield(L, -2, "switch");
    lua_pushcfunction(L, lua_engine_scene_id);
    lua_setfield(L, -2, "id");
    lua_pushcfunction(L, lua_engine_scene_find);
    lua_setfield(L, -2, "find");
  lua_setfield(L, -2, "scene");              // engine.scene = scene_tbl

  // engine.input subtable (namespace existing polling API)
  lua_newtable(L);
    lua_pushcfunction(L, lua_engine_input_held);
    lua_setfield(L, -2, "held");
    lua_pushcfunction(L, lua_engine_input_just_pressed);
    lua_setfield(L, -2, "just_pressed");
    lua_pushcfunction(L, lua_engine_input_just_released);
    lua_setfield(L, -2, "just_released");
    lua_pushcfunction(L, lua_engine_input_axis);
    lua_setfield(L, -2, "axis");
  lua_setfield(L, -2, "input");

  // engine.time subtable
  lua_newtable(L);
    lua_pushcfunction(L, lua_engine_time_now);
    lua_setfield(L, -2, "now");
    lua_pushcfunction(L, lua_engine_time_frame);
    lua_setfield(L, -2, "frame");
  lua_setfield(L, -2, "time");

  // engine.lua subtable
  lua_newtable(L);
    lua_pushcfunction(L, lua_engine_lua_collect);
    lua_setfield(L, -2, "collect");
    lua_pushcfunction(L, lua_engine_lua_memory);
    lua_setfield(L, -2, "memory");
  lua_setfield(L, -2, "lua");

  // engine.log (direct function, not subtable)
  lua_pushcfunction(L, lua_engine_log);
  lua_setfield(L, -2, "log");

lua_setglobal(L, "engine");                  // _G["engine"] = engine_tbl
```

**Binding instance access in subtable functions:** same `LUA_REGISTRYINDEX` pattern already used. `getBindings(L)` retrieves `LuaBindings*` from registry. All new `lua_engine_*` static functions are members of `LuaBindings`.

**`engine.scene` needs `Scene*`:** add `Scene* currentScene_` to `LuaBindings`. Set by the SDL runner or SSM after each scene activation. `engine.scene.id()` returns `currentScene_->getId()`. `engine.scene.find(name)` calls through to `currentScene_->getObjects().findByName(name)`.

**`engine.time.now()`:** accumulate `float elapsedSecs` in SDL runner game loop, expose via a `float*` pointer into `LuaBindings`, updated each frame before Lua calls. Or expose a `uint64_t startTick` and compute `(SDL_GetTicks() - startTick) / 1000.0f` directly in the binding function.

**`engine.log(...)`:** variadic Lua function — iterate `lua_gettop(L)` args, call `lua_tostring(L, i)` on each (Lua 5.1 `lua_tostring` coerces numbers to strings), print to `std::cerr`.

---

#### 5. ScriptProxy Userdata (self)

**Mechanism: full userdata via `lua_newuserdata`.** NOT lightuserdata.

Lightuserdata in Lua 5.1 / LuaJIT cannot have metatables — it is just a raw C pointer, period. Full userdata supports `__index`, `__newindex`, `__gc` metatables. HIGH confidence — this is a fundamental Lua 5.1 API distinction.

**Struct layout:**
```cpp
struct ScriptProxy {
    Object*      object;   // non-owning pointer — may become dangling
    bool         valid;    // set false by __gc or explicit invalidation
};
```

`C_LuaScript*` is not needed in the proxy — the static binding functions retrieve `LuaBindings*` from the registry, and the proxy is only live during a callback.

**Metatable registration (once, in `registerAll()`):**
```cpp
luaL_newmetatable(L, "enjin.ScriptProxy");  // creates or retrieves from registry
  lua_pushcfunction(L, proxy_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, proxy_newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pushcfunction(L, proxy_gc);
  lua_setfield(L, -2, "__gc");
lua_pop(L, 1);
```

`luaL_newmetatable` is available in Lua 5.1 / LuaJIT 2.1 — verified against `luajit/src/lauxlib.h`.

**`__index` dispatch pattern:**
```cpp
static int proxy_index(lua_State* L) {
    auto* proxy = static_cast<ScriptProxy*>(luaL_checkudata(L, 1, "enjin.ScriptProxy"));
    if (!proxy->valid || !proxy->object) { lua_pushnil(L); return 1; }
    const char* key = luaL_checkstring(L, 2);
    // strcmp dispatch over fixed field set:
    if (strcmp(key, "x") == 0) {
        auto* pos = proxy->object->getComponent<C_Position>();
        lua_pushnumber(L, pos ? pos->x : 0.0f);
        return 1;
    }
    // ... "y", "visible", "active", "layer", "sort_order"
    lua_pushnil(L); return 1;
}
```

Use `strcmp` over `std::unordered_map<std::string, ...>` — the field set is small and fixed (~6-8 fields), `strcmp` on 6 entries is faster than hash-table lookup with `std::string` construction.

**`luaL_checkudata` is available in Lua 5.1.** Verifies the userdata metatable matches the named type, returns the userdata pointer. Use this, not `lua_touserdata`, to get type-safe access.

**Self injection before each callback (per-call, NOT cached):**
```cpp
// Before calling init/update/draw:
lua_getglobal(L, callback_name);
if (lua_isfunction(L, -1)) {
    auto* proxy = static_cast<ScriptProxy*>(lua_newuserdata(L, sizeof(ScriptProxy)));
    proxy->object = owner_;
    proxy->valid = true;
    luaL_getmetatable(L, "enjin.ScriptProxy");
    lua_setmetatable(L, -2);
    // for update: also push dt
    lua_pushnumber(L, dt);
    lua_pcall(L, 2, 0, 0);  // update(self, dt)
}
```

**Per-call allocation:** each callback creates a new `ScriptProxy` userdata on the Lua heap (pool-allocated). The `__gc` finalizer runs when Lua GCs it — sets `valid = false`, does nothing else. This prevents stale access if a script stores `self` in a global variable between frames.

**Do NOT create a single long-lived ScriptProxy.** If the Object is destroyed between frames, the stored proxy would contain a dangling `object` pointer. Per-call proxies expire naturally with the GC.

---

#### 6. ScriptErrorPolicy

**Enum definition in `components/lua_script.hpp`:**
```cpp
enum class ScriptErrorPolicy : uint8_t {
    Disable,   // disable component, log once, continue (default)
    Log,       // log every error frame, keep running (debug mode)
    Panic      // call lua_atpanic handler (embedded recovery)
};
```

**`uint8_t` underlying type:** minimal storage, embedded-safe. No include beyond `<cstdint>` which is already transitively available.

**Integration in `C_LuaScript`:** replace the current `scriptError` bool with `ScriptErrorPolicy policy_` member (default `Disable`) and `bool disabled_by_error_` flag. On error:
- `Disable`: set `disabled_by_error_ = true`, log once, skip future calls
- `Log`: log error string every frame, continue calling
- `Panic`: call `lua_error` or platform panic — use sparingly, only for fatal-error scenarios

**Default `Disable`** matches the SDL runner's existing `lua_ok` gate pattern. Consistent semantics at both component level and runner level.

---

#### 7. Input Event Callbacks

**Mechanism:** after `input_advance_frame` + `input_platform_poll`, iterate buttons 0–15, check `justPressed` / `justReleased` on `InputState`, call Lua global functions if they exist.

**Lua function existence check (Lua 5.1 pattern):**
```cpp
lua_getglobal(L, "on_button_pressed");
if (lua_isfunction(L, -1)) {
    lua_pushinteger(L, btn_index);
    lua_pcall(L, 1, 0, 0);
} else {
    lua_pop(L, 1);
}
```

**Dispatch site: `LuaBindings::dispatchInputEvents(const InputState&)`** — a new method called from the SDL runner after input poll, before `callFunction("update", dt)`.

```cpp
// In LuaBindings:
void dispatchInputEvents(const InputState& input) {
    lua_State* L = engine_->getState();
    for (int btn = 0; btn < 16; ++btn) {
        if (input.justPressed(btn)) {
            lua_getglobal(L, "on_button_pressed");
            if (lua_isfunction(L, -1)) {
                lua_pushinteger(L, btn);
                if (lua_pcall(L, 1, 0, 0) != 0) {
                    // handle error per ScriptErrorPolicy
                    lua_pop(L, 1);
                }
            } else { lua_pop(L, 1); }
        }
        if (input.justReleased(btn)) {
            lua_getglobal(L, "on_button_released");
            // ... same pattern
        }
    }
}
```

**No new library.** Uses existing `InputState::justPressed()` / `justReleased()`.

**Why not per-`C_LuaScript`:** the current SDL runner has one Lua state, one script. Dispatching from `LuaBindings` is simpler and consistent with the existing `update`/`draw` dispatch model. Per-component dispatch is a v1.6 concern when multiple scripts coexist.

---

#### 8. GC Control

**Lua C API (Lua 5.1 / LuaJIT 2.1) — all three constants verified in `luajit/src/lua.h`:**

```c
// From lua.h (LUA_VERSION_NUM 501):
#define LUA_GCSTOP       0
#define LUA_GCRESTART    1
#define LUA_GCCOLLECT    2
#define LUA_GCCOUNT      3
#define LUA_GCCOUNTB     4
#define LUA_GCSTEP       5
#define LUA_GCSETPAUSE   6
#define LUA_GCSETSTEPMUL 7
```

**`engine.lua.collect()` implementation:**
```cpp
static int lua_engine_lua_collect(lua_State* L) {
    lua_gc(L, LUA_GCCOLLECT, 0);  // full collection cycle
    return 0;
}
```

**`engine.lua.memory()` implementation:**
```cpp
static int lua_engine_lua_memory(lua_State* L) {
    int kb  = lua_gc(L, LUA_GCCOUNT,  0);  // kilobytes
    int rem = lua_gc(L, LUA_GCCOUNTB, 0);  // bytes remainder
    lua_pushnumber(L, static_cast<lua_Number>(kb * 1024 + rem));
    return 1;                                // returns bytes as number
}
```

This is consistent with the existing `LuaEngine::getMemoryUsage()` implementation (it also calls `lua_gc(L, LUA_GCCOUNT, 0)`). HIGH confidence — verified against codebase.

---

#### 9. Component Dependency Assertions

**Protected template method on `Component` base class:**

```cpp
// In include/enjin2/core/component.hpp:
protected:
    template<typename T>
    void assertRequires() {
        static_assert(std::is_base_of<Component, T>::value, "T must be a Component");
        if (owner && owner->getComponent<T>() == nullptr) {
#ifdef NDEBUG
            // Release / embedded: self-disable, log once
            setEnabled(false);
            // Log via platform mechanism if available
#else
            // Debug builds: hard assert
            // typeid available if RTTI is enabled
            assert(false);  // "Missing required component"
#endif
        }
    }
```

**Method name: `assertRequires<T>()` — NOT `requires<T>()`.** `requires` is a C++20 keyword used in concept syntax. While it is valid as a member function name in C++17 (it becomes a keyword only in C++20 context), using it in a codebase that may be compiled with `-std=c++20` later is a maintenance hazard. `assertRequires<T>()` is unambiguous and descriptive.

**`typeid(T).name()` for diagnostics:** available in C++17 with RTTI. On ESP32 with `-fno-rtti` (common for ROM size reduction), guard behind:
```cpp
#if __has_feature(cxx_rtti) || (defined(__GXX_RTTI) && __GXX_RTTI)
    // typeid-based name
#else
    // static constexpr char name[] approach per component
#endif
```

**No new library.** `<type_traits>` already transitively included via `object.hpp`. `<cassert>` for `assert()`.

---

## What NOT to Add

| Avoid | Why | Instead |
|-------|-----|---------|
| `sol2`, `selene`, `luabridge` binding libraries | LuaJIT 2.1 has a custom static-pool allocator. Any Lua binding library that calls `lua_newuserdata`, `lua_newstate`, or registers its own metatables may bypass or corrupt the pool. The raw Lua C API is already well-used in this codebase. | Raw Lua C API — `lua_newtable`, `lua_pushcfunction`, `lua_setfield`, `luaL_newmetatable` |
| `luaL_newlib` | Lua 5.2+ API. Not present in LuaJIT 2.1 / Lua 5.1. Will fail to compile. | `lua_newtable` + `lua_pushcfunction` + `lua_setfield` — exactly what `love.graphics` already uses |
| `lua_rawlen` | Lua 5.2+ API. Use `lua_objlen` in Lua 5.1. | `lua_objlen(L, idx)` |
| `std::unordered_map` on `Object` for tags | Dynamic heap allocation on object construction — breaks zero-alloc constraint | `std::array<const char*, 8>` of string literals |
| LuaJIT FFI for ScriptProxy | FFI cdata allocations go to the system heap, bypassing the static memory pool | Full userdata with metatable — allocated through the custom pool allocator |
| Global `self` variable | Cross-script contamination when multiple `C_LuaScript` components exist | Per-call userdata pushed as first argument to each callback |
| Caching ScriptProxy across frames | If the Object is destroyed between frames, cached proxy holds dangling `object` pointer | Create new `ScriptProxy` userdata per callback invocation |
| `requires<T>()` member name | Becomes reserved in C++20 concept contexts; collision risk during future standard upgrade | `assertRequires<T>()` — unambiguous in all C++ standard versions |
| `double` for float dt | No hardware double FPU on ESP32; 3-10x slower | `float` seconds — same type used by LOVE2D, Defold, Unity |
| `std::string_view` for Object::name | Cannot own data; view becomes dangling after the source string is destroyed | `std::string` — SSO keeps short names on stack |

---

## Alternatives Considered

| Decision | Recommendation | Alternative | Why Not |
|----------|---------------|-------------|---------|
| Scene self-transition | `SceneStateMachine*` raw pointer injection | `std::function<void(uint32_t)>` callback | `std::function` may heap-allocate; raw pointer is zero overhead and matches existing ownership model |
| Object name storage | `std::string` | `std::string_view` | `string_view` cannot own; would require names to outlive lookup callers |
| Name lookup | Linear scan (`findByName` on `ObjectCollection`) | `std::unordered_map<std::string, Object*>` | HashMap adds heap buckets; linear over 128 objects is fast enough for non-frame lookups |
| Tag storage | `std::array<const char*, 8>` | `std::bitset<N>` keyed enum | Bitset requires compile-time tag enumeration; `const char*` array enables string-keyed access from Lua |
| ScriptProxy | Full userdata | Lightuserdata | Lightuserdata has no metatable support in Lua 5.1 — cannot implement `__index`/`__newindex` |
| ScriptProxy lifetime | Per-call new userdata | Single cached userdata per component | Cached proxy creates dangling pointer risk when Object is destroyed between frames |
| `__index` dispatch | `strcmp` switch | `std::unordered_map<std::string, handler>` | Small fixed field set (~6-8); `strcmp` over 6 entries costs 0 heap and is faster than hash construction |
| Component dependency check method name | `assertRequires<T>()` | `requires<T>()` | `requires` is a C++20 keyword — naming hazard for future standard upgrades |
| Input event dispatch site | `LuaBindings::dispatchInputEvents()` | Per-`C_LuaScript` dispatch | One Lua state, one script in v1.5 SDL runner — component dispatch is premature |

---

## Integration Points

### Where New Code Lives

| Feature | Header Changes | Source Changes | Wired In |
|---------|---------------|----------------|----------|
| float dt | `core/component.hpp`, `core/object.hpp`, `core/scene.hpp`, `core/object_collection.hpp`, `core/scene_state_machine.hpp` | `core/object.cpp`, `core/scene.cpp` | Pervasive — all overrides |
| Named objects | `core/object.hpp` (+`std::string name`, +`setName`, +`getName`) | `core/object.cpp` | `ObjectCollection::findByName` |
| Tags | `core/object.hpp` (+`std::array<const char*,8>`, +`addTag`, +`hasTag`) | `core/object.cpp` | `ObjectCollection::findAllWithTag` |
| Scene self-transition | `core/scene.hpp` (+`SceneStateMachine* sm_`, +`setSM`) | `core/scene_state_machine.hpp` (`completeTransition` calls `setSM`) | `SceneStateMachine::completeTransition` |
| engine.* table | `scripting/bindings.hpp` (+new static function declarations) | `scripting/bindings.cpp` (`registerAll`) | `LuaBindings::registerAll()` |
| ScriptProxy userdata | `scripting/bindings.hpp` (+`ScriptProxy` struct, +metatable functions) | `scripting/bindings.cpp` (metatable registration + injection helpers) | `LuaBindings::registerAll()` (metatable once), per-callback injection |
| ScriptErrorPolicy | `components/lua_script.hpp` (+enum, +`policy_` member) | `components/lua_script.cpp` (error handling) | Per `C_LuaScript` instance |
| Input event callbacks | `scripting/bindings.hpp` (+`dispatchInputEvents`) | `scripting/bindings.cpp` | SDL runner: after `input_platform_poll`, before `callFunction("update", dt)` |
| GC control | `scripting/bindings.cpp` | `engine.lua.*` subtable in `registerAll()` |
| `assertRequires<T>()` | `core/component.hpp` (+template method, header-only) | none — header-only | Called in `awake()` of component subclasses |

### SDL Runner Integration (`src/platform/sdl/sdl_main.cpp`)

The SDL runner needs these additions:
1. Wire `Scene*` into `LuaBindings` after scene activation (for `engine.scene.id()` / `engine.scene.find()`)
2. Call `g_lua.getBindings().dispatchInputEvents(g_input)` after `input_platform_poll`, before `callFunction("update", dt)` — HIGH priority for `on_button_pressed` callbacks
3. Accumulate `float g_elapsed_secs += dt` each frame; pass pointer or value into `LuaBindings` for `engine.time.now()`
4. The existing `g_lua.callFunction("update", dt)` already passes float dt correctly — no change needed

---

## Version Compatibility

| Component | Version | Compatibility Notes |
|-----------|---------|---------------------|
| LuaJIT | 2.1.1753364724 (Lua 5.1 API) | Use only Lua 5.1 API: `lua_newtable` (not `luaL_newlib`), `lua_objlen` (not `lua_rawlen`), `luaL_newmetatable` (available), `lua_gc` with `LUA_GCCOLLECT` / `LUA_GCCOUNT` / `LUA_GCCOUNTB` (all available) |
| C++17 | CMake standard 17 | `if constexpr`, `std::string_view`, `std::array`, `static_assert` with message all available |
| ESP32 | RTTI may be disabled | Guard `typeid(T).name()` in `assertRequires<T>()` behind RTTI availability check. Use `__PRETTY_FUNCTION__` as fallback (GCC/Clang, not RTTI-dependent) |
| `luaL_newmetatable` | Lua 5.1 / LuaJIT 2.1 | Available — creates or retrieves named metatable from `LUA_REGISTRYINDEX`. Verified in `luajit/src/lauxlib.h` |
| `luaL_checkudata` | Lua 5.1 / LuaJIT 2.1 | Available — type-safe userdata retrieval with metatable name check |
| `lua_gc(LUA_GCCOLLECT)` | Lua 5.1 / LuaJIT 2.1 | Available — GC constants verified in `luajit/src/lua.h` |

---

## Sources

- Live codebase: `/home/unwn/dev/enjin/luajit/src/lua.h` — `LUA_VERSION "Lua 5.1"`, `LUA_VERSION_NUM 501`, all `LUA_GC*` constants confirmed
- Live codebase: `/home/unwn/dev/enjin/luajit/src/luajit.h` — `LUAJIT_VERSION "LuaJIT 2.1.1753364724"` confirmed
- Live codebase: `include/enjin2/scripting/lua_platform.hpp` — VCV_RACK flag, LuaJIT 5.1 compat macro pattern confirmed
- Live codebase: `src/scripting/bindings.cpp` — `lua_newtable`/`lua_setfield` pattern for `love.graphics` (line 211-237), `LUA_REGISTRYINDEX` pattern for binding retrieval (line 265-270), `lua_pcall` error handling confirmed
- Live codebase: `include/enjin2/core/object.hpp` — `uint16_t deltaTime` confirmed (the type being replaced), `std::array<std::unique_ptr<Component>, 16>`, `dynamic_cast<T*>` confirmed
- Live codebase: `include/enjin2/core/scene_state_machine.hpp` — `changeScene(uint32_t)` API confirmed, `completeTransition()` injection site identified
- Live codebase: `include/enjin2/core/object_collection.hpp` — `forEach` traversal pattern confirmed, no existing name map (linear scan required)
- Live codebase: `src/platform/sdl/sdl_main.cpp` — `float dt` already computed (line 246), `g_lua.callFunction("update", dt)` already correct (line 265), `input_advance_frame` + `input_platform_poll` ordering confirmed (lines 253-254)
- Live codebase: `src/scripting/lua_engine.cpp` — `lua_gc(L, LUA_GCCOUNT, 0)` used in `getMemoryUsage()` — confirms pattern for `engine.lua.memory()`
- Project research: `project/lua-embedding-design.md` — HIGH confidence domain analysis: `luaL_newlib` exclusion, `self` proxy design, `engine.*` namespace structure, GC control rationale
- Project research: `project/cpp-engine-improvements.md` — HIGH confidence: `float dt` rationale, named object / tag design, scene self-transition injection pattern, `requires<T>()` design

---

*Stack research for: enjin2 v1.5 — Lua scripting foundation (engine.* table, ScriptProxy, float dt, named objects, scene transitions, error policy, input events, GC control, dependency assertions)*
*Researched: 2026-02-26*
