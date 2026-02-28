# Phase 32: ScriptProxy Userdata - Research

**Researched:** 2026-02-27
**Domain:** Lua 5.1 full userdata, metamethods (__index/__newindex), generation-token validity, C++ property binding via raw C API
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PROXY-01 | Every Lua callback receives `self` as the first argument: `init(self)`, `update(self, dt)`, `draw(self)` | `LuaEngine::callFunction()` uses variadic templates — add a `ScriptProxy*` userdata as the first pushed arg before dt; requires push helper for userdata; three call sites in `LuaScriptSystem` |
| PROXY-02 | Scripts can read/write `self.x`, `self.y`, `self.visible`, `self.layer`, `self.name`, `self.active` mapped to C++ component properties | `__index` and `__newindex` metamethods on the `ScriptProxy` metatable dispatch property name to the correct C++ getter/setter via `strcmp`; x/y → `C_Position`; visible/layer → `C_Drawable` (buffer_index); active → `Object::active`; name → `Object::_name` (Phase 29) |
| PROXY-03 | ScriptProxy uses a validity mechanism (generation token or flag) to prevent dangling pointer access after Object destruction | STATE.md blocker: "Decide validity mechanism (generation token vs valid flag) before writing any proxy code — cannot retrofit safely." Research recommendation: `bool valid` flag on the `ScriptProxy` struct itself — simpler than generation token, sufficient for Phase 32 scope where proxy lifetime is controlled by `C_LuaScript` |
| PROXY-04 | All existing Lua scripts migrated to new `(self, ...)` callback signature atomically | Four scripts: `reload_test.lua`, `layer_demo.lua`, `pikachu_demo.lua`, `e2e_parity.lua` — all use `function update(dt)` and `function draw()` today; migration is `function update(self, dt)` and `function draw(self)`; `init` functions do not exist yet, so only `update` and `draw` need updating |
</phase_requirements>

---

## Summary

Phase 32 introduces a `ScriptProxy` full userdata that wraps a C++ `C_LuaScript*` (and its owner `Object*`) and is pushed as the first argument to every Lua callback. The proxy bridges Lua property access (`self.x = 10`) to C++ component setters (`C_Position::setPosition()`, `C_Drawable::SetVisibility()`, etc.) via `__index` and `__newindex` metamethods registered in the Lua state.

The key architectural decision — flagged as a blocker in STATE.md — is the validity mechanism. A `bool valid` flag stored directly in the `ScriptProxy` struct is the correct choice for this phase: the proxy struct lives in Lua-managed memory (full userdata), and `C_LuaScript` clears the flag when the component is destroyed. This is simpler and sufficient compared to a generation token, which would require an extra level of indirection and an external generation counter array. Generation tokens become worth the complexity only when multiple independent proxy instances must coexist for the same object across GC cycles — not a requirement here.

The current `LuaEngine::callFunction()` template pushes arguments via `pushArgs()` specializations. Pushing the proxy userdata as the first argument requires either a new `pushArg<ScriptProxy*>` specialization that creates/reuses the userdata, or a dedicated helper that pushes the userdata before calling `lua_pcall`. Because the proxy must be a stable Lua value (a reference to the same userdata across frames, not a new allocation per call), the correct pattern is to create the userdata once at script load time, store a reference in the Lua registry, and retrieve it for each callback invocation.

PROXY-04 requires migrating all four existing scripts from `update(dt)` / `draw()` to `update(self, dt)` / `draw(self)`. This is the smallest Lua change — it must be done atomically with the C++ changes so the existing scripts do not break at the boundary between the old and new call conventions.

**Primary recommendation:** Implement `ScriptProxy` as a full userdata with a single `bool valid` flag + raw `C_LuaScript*` pointer; register the metatable once at `LuaScriptSystem::initialize()` time; create one proxy userdata per `C_LuaScript`, store it in the Lua registry, and push it as the first argument before each callback call.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua 5.1 / LuaJIT raw C API | Project standard | Full userdata, `luaL_newmetatable`, `lua_newuserdata`, `lua_setfield`, `lua_getfield` | All existing bindings use raw C API; `lua_platform.hpp` includes `lua.h`, `lauxlib.h`, `lualib.h` directly; confirmed by `bindings.cpp` |
| C++17 | Project standard | `strcmp`, `static_cast`, template specializations | `CMAKE_CXX_STANDARD 17`; no new compile requirements |

### Supporting

None. Phase 32 introduces no new external dependencies. All changes are in-tree modifications to the scripting subsystem.

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `bool valid` flag on proxy | Generation token (uint32_t generation) | Generation token is safer when multiple proxy copies can be made for the same object; overkill for Phase 32 where one proxy per `C_LuaScript` is sufficient and proxy lifetime matches script state lifecycle |
| Store proxy in Lua registry | Create new userdata on every callback call | Creating userdata per call wastes memory and creates GC pressure; a stored registry reference is stable and zero-allocation per frame |
| Full userdata with metatable | Lightuserdata with no metatable | STATE.md Phase 26 decision locked: "ScriptProxy uses full userdata (not lightuserdata) — lightuserdata has no metatable in Lua 5.1" — this is a locked constraint from the project history |
| `strcmp` dispatch in `__index` | Table of function pointers / hash map | At 6 properties, `strcmp` chain is fast and readable with zero extra data structures; matches the project's established `strcmp` pattern (palette.cpp:51, named_objects) |

**No installation required.** Pure C++ in-tree changes.

---

## Architecture Patterns

### Files to Modify/Create

```
include/enjin2/scripting/
├── bindings.hpp        — add ScriptProxy struct; add proxy registration to LuaBindings;
│                         add lua_register_proxy(), __index, __newindex C functions
└── lua_engine.hpp      — add pushArg<void*> or proxy push helper (optional — may use direct lua_State* approach)

src/scripting/
└── bindings.cpp        — implement ScriptProxy metatable registration, __index/__newindex dispatch,
                          proxy creation, callback push helpers

include/enjin2/components/
└── lua_script.hpp      — add ScriptProxy* proxyRef tracking; invalidate proxy on destruction;
                          change callScriptFunctionSafe to push proxy as first arg

src/components/
└── lua_script.cpp      — update update(), draw(), and init call sites to push proxy first

scripts/
├── reload_test.lua     — migrate: update(dt) → update(self, dt); draw() → draw(self)
├── layer_demo.lua      — migrate: update(dt) → update(self, dt); draw() → draw(self)
├── pikachu_demo.lua    — migrate: update(dt) → update(self, dt); draw() → draw(self)
└── e2e_parity.lua      — migrate: update(dt) → update(self, dt); draw() → draw(self)
```

### Pattern 1: ScriptProxy Struct (C++ side)

**What:** A plain struct stored as Lua full userdata. Contains a pointer back to `C_LuaScript` and a validity flag. When `C_LuaScript` is destroyed, it writes `false` to the proxy's `valid` field through the Lua registry reference.

**When to use:** Created once per `C_LuaScript` at `loadScript()` / `reloadScript()` time. Reused across all callback invocations within the same script's lifetime. Invalidated on destruction.

**Example:**
```cpp
// In include/enjin2/scripting/bindings.hpp — new struct
struct ScriptProxy {
    C_LuaScript* component;   // non-owning; may dangle after invalidation
    bool valid;               // set to false when component is destroyed
};
```

**Note:** `C_LuaScript` lives in the `include/enjin2/components/` subtree, so `bindings.hpp` will need a forward declaration of `C_LuaScript` to avoid a circular include. `C_LuaScript` can include `bindings.hpp` safely since `bindings.hpp` currently includes `lua_engine.hpp` and `LuaScriptSystem` which `lua_script.cpp` already uses.

**Forward declaration to add in bindings.hpp:**
```cpp
// Before ScriptProxy struct
namespace enjin2 { class C_LuaScript; }
```

### Pattern 2: Metatable Registration

**What:** `LuaBindings::registerAll()` creates the `ScriptProxy` metatable in the Lua registry using `luaL_newmetatable`. The metatable has `__index` and `__newindex` set to static C functions. This registration happens once at `initialize()` time (inside `registerAll()`), before any script is loaded.

**When to use:** Called from `LuaScriptSystem::initialize()` → `bindings.registerAll()`.

**Example:**
```cpp
// In LuaBindings::registerAll() — added at end, after existing registrations
static const char* PROXY_METATABLE = "ScriptProxy";

// Register ScriptProxy metatable
if (luaL_newmetatable(L, PROXY_METATABLE)) {   // pushes new table; returns 1 if new, 0 if exists
    lua_pushcfunction(L, lua_proxy_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_proxy_newindex);
    lua_setfield(L, -2, "__newindex");
}
lua_pop(L, 1);  // pop the metatable
```

**Why `luaL_newmetatable`:** It registers the metatable in the Lua registry under the name `"ScriptProxy"`. Subsequent calls to `luaL_checkudata(L, n, "ScriptProxy")` validate that a userdata is indeed a ScriptProxy and not some other userdata type. This is the standard Lua C API pattern for typed userdata.

### Pattern 3: Proxy Creation and Registry Storage

**What:** When `C_LuaScript::loadScript()` / `loadScriptFile()` succeeds, it creates a `ScriptProxy` userdata, sets its metatable, and stores it in the Lua registry for reuse. The registry key is a light userdata address of the `C_LuaScript*` itself (unique per component).

**Example:**
```cpp
// In C_LuaScript::executeScript() — after executeString succeeds, before callScriptFunctionSafe(INIT_FUNCTION)

// Create proxy userdata
ScriptProxy* proxy = static_cast<ScriptProxy*>(
    lua_newuserdata(L, sizeof(ScriptProxy)));
proxy->component = this;
proxy->valid = true;

// Set metatable
luaL_getmetatable(L, "ScriptProxy");
lua_setmetatable(L, -2);

// Store in registry with this pointer as key
lua_pushlightuserdata(L, this);   // key: C_LuaScript* as lightuserdata
lua_insert(L, -2);                // move key below userdata value
lua_settable(L, LUA_REGISTRYINDEX);  // registry[this] = proxy_userdata
```

**Retrieving the proxy for each callback:**
```cpp
// Before calling lua_pcall for update/draw/init:
lua_pushlightuserdata(L, this);        // key
lua_gettable(L, LUA_REGISTRYINDEX);   // push proxy userdata onto stack
// Stack: [function, proxy]
// Then push additional args (dt for update), then lua_pcall(L, nargs+1, ...)
```

### Pattern 4: `__index` and `__newindex` Property Dispatch

**What:** The `__index` metamethod is called when Lua accesses `self.property`. The `__newindex` metamethod is called when Lua assigns `self.property = value`. Both validate the proxy, then dispatch on the property name using `strcmp`.

**Property-to-C++ mapping (PROXY-02):**

| Lua property | C++ component | C++ accessor |
|-------------|---------------|-------------|
| `self.x` | `C_Position` via `owner->getPosition()` | `getPosition().x` / `setPosition(x, y)` |
| `self.y` | `C_Position` via `owner->getPosition()` | `getPosition().y` / `setPosition(x, y)` |
| `self.visible` | `C_Drawable` (the `C_LuaScript` itself, since it inherits from `C_Drawable`) | `isVisible()` / `SetVisibility()` |
| `self.layer` | `C_Drawable` (`buffer_index`) | `GetBufferIndex()` / `SetBufferIndex()` |
| `self.name` | `Object` | `owner->getName()` (Phase 29) |
| `self.active` | `Object` | `owner->isActive()` / `owner->setActive()` |

**Critical observation:** `C_LuaScript` inherits from `C_Drawable` which inherits from `Component`. `Component` holds `Object* owner`. So from `proxy->component` (a `C_LuaScript*`):
- `proxy->component->getOwner()` → `Object*` for name, active
- `proxy->component->getOwner()->getPosition()` → `C_Position*` for x, y (may be `nullptr` if no position component exists)
- `proxy->component` directly → `C_Drawable*` for visible, layer (since `C_LuaScript` IS a `C_Drawable`)

**`__index` example:**
```cpp
// In bindings.cpp
static int lua_proxy_index(lua_State* L) {
    // stack: [userdata, key_string]
    ScriptProxy* proxy = static_cast<ScriptProxy*>(
        luaL_checkudata(L, 1, "ScriptProxy"));

    if (!proxy->valid || !proxy->component) {
        lua_pushnil(L);
        return 1;
    }

    const char* key = luaL_checkstring(L, 2);
    C_LuaScript* comp = proxy->component;
    Object* owner = comp->getOwner();

    if (strcmp(key, "x") == 0) {
        C_Position* pos = owner->getPosition();
        lua_pushinteger(L, pos ? pos->getPosition().x : 0);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        C_Position* pos = owner->getPosition();
        lua_pushinteger(L, pos ? pos->getPosition().y : 0);
        return 1;
    } else if (strcmp(key, "visible") == 0) {
        lua_pushboolean(L, comp->isVisible() ? 1 : 0);
        return 1;
    } else if (strcmp(key, "layer") == 0) {
        lua_pushinteger(L, comp->GetBufferIndex());
        return 1;
    } else if (strcmp(key, "name") == 0) {
        const char* name = owner ? owner->getName() : nullptr;
        if (name) lua_pushstring(L, name);
        else lua_pushnil(L);
        return 1;
    } else if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, owner && owner->isActive() ? 1 : 0);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}
```

**`__newindex` example:**
```cpp
static int lua_proxy_newindex(lua_State* L) {
    // stack: [userdata, key_string, value]
    ScriptProxy* proxy = static_cast<ScriptProxy*>(
        luaL_checkudata(L, 1, "ScriptProxy"));

    if (!proxy->valid || !proxy->component) {
        return 0;  // silently ignore writes to invalid proxy
    }

    const char* key = luaL_checkstring(L, 2);
    C_LuaScript* comp = proxy->component;
    Object* owner = comp->getOwner();

    if (strcmp(key, "x") == 0) {
        C_Position* pos = owner->getPosition();
        if (pos) {
            int16_t newX = static_cast<int16_t>(luaL_checkinteger(L, 3));
            pos->setPosition(newX, pos->getPosition().y);
        }
    } else if (strcmp(key, "y") == 0) {
        C_Position* pos = owner->getPosition();
        if (pos) {
            int16_t newY = static_cast<int16_t>(luaL_checkinteger(L, 3));
            pos->setPosition(pos->getPosition().x, newY);
        }
    } else if (strcmp(key, "visible") == 0) {
        bool vis = lua_toboolean(L, 3) != 0;
        comp->SetVisibility(vis);
    } else if (strcmp(key, "layer") == 0) {
        uint8_t layer = static_cast<uint8_t>(luaL_checkinteger(L, 3));
        comp->SetBufferIndex(layer);
    } else if (strcmp(key, "active") == 0) {
        bool active = lua_toboolean(L, 3) != 0;
        if (owner) owner->setActive(active);
    }
    // "name" is read-only — silently ignore writes

    return 0;
}
```

### Pattern 5: Pushing the Proxy as First Callback Argument

**What:** In `C_LuaScript::callScriptFunctionSafe()` (or in the specific `init`, `update`, `draw` call sites), retrieve the stored proxy userdata from the Lua registry and push it onto the stack before pushing any other arguments, then call `lua_pcall` with `nargs + 1`.

**Current call site (update):**
```cpp
// Current: g_lua.callFunction("update", dt)  →  lua_pcall(L, 1, 0, 0)
// New:     push proxy, push dt               →  lua_pcall(L, 2, 0, 0)
```

**Because** `LuaEngine::callFunction()` uses a variadic template that pushes args generically, the cleanest integration is to push the proxy BEFORE calling `callFunction`, or to add a `callFunctionWithProxy(name, proxy_key, args...)` helper. The direct approach is simpler: implement a `callWithProxy` method on `LuaScriptSystem` or `C_LuaScript` that manually handles the stack:

```cpp
// In C_LuaScript — new private helper
LuaResult callWithProxy(const char* funcName, float dt, bool passDt) {
    lua_State* L = scriptSystem->getEngine().getState();

    // Push function
    lua_getglobal(L, funcName);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return LuaResult("Function not found");
    }

    // Push proxy as first argument
    lua_pushlightuserdata(L, this);
    lua_gettable(L, LUA_REGISTRYINDEX);   // push ScriptProxy userdata

    int nargs = 1;  // proxy is arg 1
    if (passDt) {
        lua_pushnumber(L, static_cast<lua_Number>(dt));
        nargs = 2;
    }

    int result = lua_pcall(L, nargs, 0, 0);
    if (result != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::string msg = err ? err : "unknown error";
        lua_pop(L, 1);
        return LuaResult(msg);
    }
    return LuaResult();
}
```

### Pattern 6: Proxy Invalidation on Destruction

**What:** When `C_LuaScript` destructor runs, it must set `proxy->valid = false` so stored Lua upvalues of `self` from previous frames do not crash if accessed after destruction.

**Example:**
```cpp
// In C_LuaScript::~C_LuaScript()
~C_LuaScript() {
    // Invalidate proxy if still alive in Lua registry
    if (scriptSystem && scriptSystem->getEngine().isInitialized()) {
        lua_State* L = scriptSystem->getEngine().getState();
        if (L) {
            lua_pushlightuserdata(L, this);
            lua_gettable(L, LUA_REGISTRYINDEX);
            if (lua_isuserdata(L, -1)) {
                ScriptProxy* proxy = static_cast<ScriptProxy*>(lua_touserdata(L, -1));
                if (proxy) proxy->valid = false;
            }
            lua_pop(L, 1);
        }
    }
    if (scriptSystem) {
        scriptSystem->shutdown();
    }
}
```

### Pattern 7: Lua Script Migration (PROXY-04)

**What:** All four existing scripts use the old no-self signatures. The migration is:
- `function update(dt)` → `function update(self, dt)` (the body does NOT use `self` currently — just changing the signature is sufficient)
- `function draw()` → `function draw(self)` (same — body doesn't use `self` currently)
- `function init()` → not present in any of the four scripts yet; new scripts write `function init(self)`

**pikachu_demo.lua special case:** Line 12 calls `updateSprite(sprite, dt * 1000)` — note this passes `dt * 1000` (milliseconds). STATE.md records "Lua updateSprite API now expects dt in seconds (accumSec replaces accumMs)" from Phase 28. This call should already have been fixed in Phase 28 but was flagged as needing verification. The signature change to `update(self, dt)` does not affect this line.

**Migration table:**
| Script | Change |
|--------|--------|
| `reload_test.lua` | Line 20: `function update(dt)` → `function update(self, dt)` |
| `layer_demo.lua` | Lines 4, 8: `function update(dt)` → `function update(self, dt)`; `function draw()` → `function draw(self)` |
| `pikachu_demo.lua` | Lines 10, 16: same pattern |
| `e2e_parity.lua` | Lines 45, 50: same pattern |

### Anti-Patterns to Avoid

- **Using lightuserdata for the proxy:** Explicitly ruled out by Phase 26 project decision. Lightuserdata has no metatable in Lua 5.1 — `self.x` would be a compile-time error in Lua.
- **Creating a new proxy userdata on every callback call:** Creates GC pressure; breaks the "same object across frames" semantic needed for PROXY-03.
- **Storing the raw `C_LuaScript*` as the Lua registry key AS an integer:** Use `lua_pushlightuserdata(L, this)` as the key — it is unique per component instance and requires no additional tracking.
- **Registering the metatable every `callWithProxy` call:** Register once in `registerAll()`, retrieve with `luaL_getmetatable()` elsewhere.
- **Using `lua_rawget`/`lua_rawset` in `__index`/`__newindex`:** Don't call the metatable's own metamethods recursively. The handlers dispatch to C++ directly, not through the Lua table.
- **Forgetting to `lua_pop` after retrieving the metatable in `registerAll()`:** `luaL_newmetatable` pushes the table; it must be popped after setup.
- **Not null-checking `C_Position`:** `Object::getPosition()` returns `nullptr` if no `C_Position` component has been added. `self.x` / `self.y` must handle `nullptr` gracefully (return 0 / silently ignore writes).
- **Mutating `name` via `__newindex`:** `Object::setName()` (Phase 29) stores a `const char*` pointer — Lua strings are interned but calling `setName` with a transient Lua string pointer is unsafe because the Lua GC may collect the string. The `name` property should be read-only from Lua (silently ignore `__newindex` on "name").
- **Accessing the proxy after `scriptSystem->shutdown()`:** The Lua state is closed in `shutdown()`; the registry no longer exists. The invalidation must happen BEFORE shutdown.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Typed userdata safety | Manual `lua_type` check + cast | `luaL_checkudata(L, n, "ScriptProxy")` | Validates type AND name, raises a Lua error with correct message if type doesn't match |
| Metatable lookup | Manually re-push metatable each time | `luaL_getmetatable(L, "ScriptProxy")` | Retrieves from registry by name; idiomatic C API pattern |
| Userdata allocation | `malloc` / `new` with lifetime tracking | `lua_newuserdata(L, sizeof(ScriptProxy))` | Lua GC owns the memory; no separate deletion needed; lifetime tied to Lua state |

**Key insight:** The Lua C API provides exactly the tools needed for typed userdata — `luaL_newmetatable`, `luaL_checkudata`, `lua_newuserdata`. Using them correctly gives type safety, GC integration, and metatable dispatch without any hand-rolled infrastructure.

---

## Common Pitfalls

### Pitfall 1: `C_LuaScript` / `LuaBindings` Circular Include

**What goes wrong:** `ScriptProxy` in `bindings.hpp` needs to reference `C_LuaScript`. But `lua_script.hpp` includes `bindings.hpp` (via `lua_engine.hpp`). Adding a full `#include "lua_script.hpp"` to `bindings.hpp` creates a circular include.

**Why it happens:** `lua_script.hpp` already includes `lua_interpreter.hpp` which includes `lua_engine.hpp` which `bindings.hpp` depends on.

**How to avoid:** Forward-declare `C_LuaScript` in `bindings.hpp`:
```cpp
namespace enjin2 { class C_LuaScript; }
```
The `ScriptProxy` struct only needs a pointer to `C_LuaScript`, not the full type. The `__index`/`__newindex` implementations in `bindings.cpp` include both `bindings.hpp` and `lua_script.hpp` (and `position.hpp`, `drawable.hpp`) where the full types are needed.

**Warning signs:** Compiler error about incomplete type when trying to call `proxy->component->getOwner()` in the static handler functions.

### Pitfall 2: Proxy Registry Entry Not Present on Callback

**What goes wrong:** `callWithProxy()` does `lua_pushlightuserdata(L, this); lua_gettable(L, LUA_REGISTRYINDEX)` but the proxy was never stored — this happens if the proxy creation path (`executeScript()`) was not updated, or if the proxy is not created on `loadScriptFile()` but only on `loadScript()` (or vice versa).

**Why it happens:** `C_LuaScript` has two load paths: `loadScript(code)` → `executeScript(code)` and `loadScriptFile(filename)` → `loadScript()` (via `engine.executeFile`). The proxy must be created on BOTH paths.

**How to avoid:** Create the proxy inside `executeScript()` (which is the shared bottleneck called by both `loadScript()` and `loadScriptFile()`). Verify that `lua_gettable` returns a non-nil value before calling `lua_pcall`.

**Warning signs:** `lua_isuserdata` returns false on the result of the registry lookup; `init(self)` receives `nil` as first argument.

### Pitfall 3: `luaL_checkudata` Error in Non-Error Path

**What goes wrong:** `luaL_checkudata` raises a Lua error (via `luaL_argerror`) if the value at stack position 1 is not a userdata with the expected metatable. This error propagates as a Lua runtime error, defeating `lua_pcall`'s error capture.

**Why it happens:** If a script is called with the wrong argument type (or if the proxy was replaced with nil), the `__index`/`__newindex` handler throws before the nil/invalid check.

**How to avoid:** In the `__index`/`__newindex` handlers, use `lua_touserdata` + manual null/type check instead of `luaL_checkudata`, and return nil/0 on mismatch rather than raising:
```cpp
if (!lua_isuserdata(L, 1)) { lua_pushnil(L); return 1; }
ScriptProxy* proxy = static_cast<ScriptProxy*>(lua_touserdata(L, 1));
if (!proxy || !proxy->valid) { lua_pushnil(L); return 1; }
```
Or, use `lua_pcall` error handling correctly and ensure the metatable assignment is always correct so `luaL_checkudata` always succeeds for valid proxies.

**Warning signs:** Script exits immediately on `init(self)` with a "bad argument #1" Lua error.

### Pitfall 4: `self.name` Write Unsafe (Lua String Lifetime)

**What goes wrong:** `__newindex` for "name" calls `owner->setName(lua_tostring(L, 3))`. `lua_tostring` returns a `const char*` into the Lua string intern table. After `lua_pop`, the reference count may drop to zero and the GC may collect the string. `Object::name` now holds a dangling pointer.

**Why it happens:** Lua strings are GC-managed. `const char*` pointers obtained from `lua_tostring` are only valid while the Lua string object is on the stack or otherwise referenced.

**How to avoid:** Make "name" a read-only property from Lua (`__newindex` for "name" is a no-op). If writable name-setting from Lua is needed in the future, copy the string to a static buffer or use a fixed char array — but that requires heap or static storage that contradicts the project's zero-allocation constraint. For Phase 32, read-only is correct.

**Warning signs:** `findByName()` returns wrong objects after `self.name = "something"` is called from Lua; crashes with use-after-free when iterating named objects.

### Pitfall 5: Invalid Proxy After Hot-Reload (F5)

**What goes wrong:** F5 hot-reload calls `g_lua.shutdown()` → `lua_close(L)`, destroying the old Lua state and all its userdata. If a stale C++ reference to the old `LuaEngine::L` or a raw proxy pointer is held anywhere, it will dangle.

**Why it happens:** The SDL runner calls `performReload()` which calls `lua.shutdown()` then `lua.initialize()`. This creates a brand-new `lua_State*`. The old userdata memory is gone.

**How to avoid:** The proxy is owned by the Lua state — when the Lua state is destroyed, the userdata is destroyed. `C_LuaScript::~C_LuaScript()` invalidates the proxy, but during hot-reload the `C_LuaScript` component itself may not be destroyed (the reload happens at the SDL runner level, not the component level). The invalidation must happen in `C_LuaScript::reloadScript()` (which calls `shutdown()`) before the new proxy is created.

**The correct sequence for reload:**
1. `shutdown()` — old Lua state closes; old proxy userdata is gone
2. `initialize()` — new Lua state created
3. `executeScript()` — new proxy created and stored in new registry

No explicit proxy pointer tracking is needed in the C++ side as long as proxy invalidation happens inside `shutdown()` before the state is closed (so we write `valid = false` before `lua_close`).

**Warning signs:** Crash on first callback after F5 reload when the frame before the reload stored `self` in a Lua upvalue.

### Pitfall 6: Upvalue Capture of `self` Across Frames

**What goes wrong:** A script does `local saved = self` at module level (outside any function). `saved` becomes an upvalue in a closure. If the object is destroyed and the next frame's script (different `C_LuaScript`) is loaded, `saved` refers to a proxy with `valid = false`. Accessing `saved.x` should return nil, not crash.

**Why it happens:** The proxy is a full userdata stored in Lua heap memory. After invalidation (`valid = false`), the userdata still exists in Lua; it just has `valid = false`. The `__index` check returns nil safely.

**How to avoid:** The `valid` flag check in `__index` / `__newindex` handles this correctly — check at the top of every handler before any property access. This is the core of PROXY-03.

**Warning signs:** Crash on property read after object destruction; absence of this crash after correct `valid` flag implementation.

---

## Code Examples

### Full Userdata Creation with Metatable

```c
// Source: Lua 5.1 Reference Manual — "full userdata" pattern
// https://www.lua.org/manual/5.1/manual.html#lua_newuserdata

// Create new userdata of size sizeof(ScriptProxy)
ScriptProxy* proxy = (ScriptProxy*)lua_newuserdata(L, sizeof(ScriptProxy));
proxy->component = comp;
proxy->valid = true;

// Assign metatable (created by luaL_newmetatable earlier)
luaL_getmetatable(L, "ScriptProxy");
lua_setmetatable(L, -2);
// Stack: [proxy_userdata] (metatable popped by setmetatable)
```

### Storing and Retrieving from Lua Registry

```c
// Source: Lua 5.1 Reference Manual — LUA_REGISTRYINDEX pattern
// Store: registry[lightuserdata_key] = proxy_userdata
lua_pushlightuserdata(L, (void*)this_ptr);  // key
// proxy_userdata must already be on the stack:
lua_insert(L, -2);                          // swap so key is below value
lua_settable(L, LUA_REGISTRYINDEX);

// Retrieve: push registry[lightuserdata_key]
lua_pushlightuserdata(L, (void*)this_ptr);  // key
lua_gettable(L, LUA_REGISTRYINDEX);         // pushes value (proxy_userdata or nil)
```

### Metatable `__index` Dispatch Pattern

```c
// Source: Lua 5.1 Reference Manual — metamethods
// Typical __index handler for property dispatch:
static int lua_proxy_index(lua_State* L) {
    // arg 1 = userdata (self)
    // arg 2 = key (string)
    ScriptProxy* p = (ScriptProxy*)lua_touserdata(L, 1);
    if (!p || !p->valid) { lua_pushnil(L); return 1; }

    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    // Dispatch on key...
    if (strcmp(key, "x") == 0) {
        // read C++ property, push Lua value
        return 1;
    }
    lua_pushnil(L);
    return 1;
}
```

### `luaL_newmetatable` Registration (Once at Init)

```c
// Source: lauxlib.h — luaL_newmetatable
// Returns 1 if new (table created), 0 if existing (no-op)
if (luaL_newmetatable(L, "ScriptProxy")) {
    lua_pushcfunction(L, lua_proxy_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_proxy_newindex);
    lua_setfield(L, -2, "__newindex");
}
lua_pop(L, 1);  // always pop — both new and existing cases leave table on stack
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Callbacks: `update(dt)`, `draw()` (no self) | Callbacks: `update(self, dt)`, `draw(self)`, `init(self)` | Phase 32 | Scripts receive a typed proxy for property access |
| Component state exposed via global variables (`setScriptVar("dt", ...)`) | Component state via `self.x`, `self.y`, etc. via proxy metamethods | Phase 32 | Cleaner, object-oriented API; avoids global namespace pollution |
| No validity protection | `bool valid` flag on proxy; nil return on invalid access | Phase 32 | Prevents crashes when `self` is captured across frames and object is destroyed |

**Deprecated after Phase 32:**
- The `setScriptVar("dt", ...)` / `setScriptVar("time", ...)` pattern in `C_LuaScript::update()` can remain as-is (dt is still passed as a Lua callback argument), but the global variable injection is now redundant for dt since it is a callback parameter.

---

## Open Questions

1. **Should `self.layer` be 1-indexed (Lua convention) or 0-indexed (C++ `buffer_index`)?**
   - What we know: The existing layer system uses 1-indexed values in Lua (`LAYER_BG = 1`) but `C_Drawable::buffer_index` is 0-indexed. The `setLayer(n)` binding does `cpp_idx = lua_idx - 1`.
   - What's unclear: Whether `self.layer` should follow the existing 1-indexed convention or expose the raw 0-indexed C++ value.
   - Recommendation: Use 1-indexed in Lua for `self.layer` to match `LAYER_BG`, `LAYER_MID`, `LAYER_FG`, `LAYER_UI` constants. Convert in `__index` (`return buffer_index + 1`) and `__newindex` (`SetBufferIndex(lua_val - 1)`).

2. **What should `self.x` / `self.y` return if no `C_Position` component exists on the Object?**
   - What we know: `Object::getPosition()` returns `nullptr` if no `C_Position` was added. The proxy `__index` must null-check before dereferencing.
   - What's unclear: Whether returning 0 or raising a Lua error is more useful.
   - Recommendation: Return 0 for reads (safe, non-crashing). Silently ignore writes. Add a comment in the handler noting this behavior.

3. **Should the current SDL runner's `callFunction("update", dt)` / `callFunction("draw")` be changed to use the proxy push path?**
   - What we know: The SDL runner calls `g_lua.callFunction("update", dt)` and `g_lua.callFunction("draw")` directly from `sdl_main.cpp`. These are at the runner level, not the `C_LuaScript` component level. The runner does not have a `C_LuaScript` component — it manages the `LuaScriptSystem` directly.
   - What's unclear: The phase goal says "every Lua callback receives self" but the current SDL runner architecture has no `Object` or `C_LuaScript` — the scripts are run directly by the SDL loop. Phase 32 appears to be designed for the `C_LuaScript` component path, not the SDL runner path.
   - Recommendation: Phase 32 should implement the proxy for the `C_LuaScript` component path. The existing SDL runner scripts (which do not use `C_LuaScript`) can be migrated (PROXY-04) to accept `self` as a parameter that receives a proxy created at the runner level with a dummy/scene-level proxy — OR the migration can simply add `self` as the first parameter in the script function signatures without the runner pushing an actual proxy, relying on Lua's tolerance for extra unused parameters. The planner must decide: (a) implement a runner-level proxy or (b) migrate scripts to accept `self` while the SDL runner pushes a lightweight sentinel. Option (b) is lower risk for Phase 32 scope.

---

## Validation Architecture

`workflow.nyquist_validation` is not present in `.planning/config.json` — this section is omitted per instructions.

---

## Sources

### Primary (HIGH confidence)

- Live codebase inspection (2026-02-27):
  - `include/enjin2/scripting/bindings.hpp` — `LuaBindings`, `LuaCanvas`, `LuaScriptSystem` confirmed; `g_currentBindings` pattern; registry key `"enjin_bindings"` via lightuserdata confirmed
  - `src/scripting/bindings.cpp` — full implementation including registry storage pattern, `getBindings(L)` retrieval, `registerAll()` flow confirmed
  - `src/scripting/lua_engine.cpp` — `callFunction()` template, `pushArg` specializations for float/double/int/bool confirmed; `LuaEngine::getState()` returns `lua_State*`
  - `include/enjin2/components/lua_script.hpp` — `C_LuaScript : C_Drawable`; `callScriptFunctionSafe()`, `update(float dt)`, `draw(ICanvas<Pixel4>&)` confirmed
  - `src/components/lua_script.cpp` — existing callback dispatch flow; `callScriptFunctionSafe(INIT_FUNCTION)` pattern confirmed
  - `include/enjin2/components/drawable.hpp` — `C_Drawable::is_visible`, `buffer_index`, `SetVisibility()`, `GetBufferIndex()`, `SetBufferIndex()`, `isVisible()` confirmed
  - `include/enjin2/components/position.hpp` — `C_Position::getPosition()` returns `const Point&`; `setPosition(int16_t x, int16_t y)` confirmed
  - `include/enjin2/core/object.hpp` — `Object::active`, `isActive()`, `setActive()`, `getPosition()` confirmed; Phase 29 adds `getName()`/`setName()`
  - `include/enjin2/core/component.hpp` — `Component::owner` (protected `Object*`), `getOwner()` confirmed
  - `src/platform/sdl/sdl_main.cpp` — `g_lua.callFunction("update", dt)` and `g_lua.callFunction("draw")` call sites confirmed; `performReload()` flow confirmed
  - `scripts/reload_test.lua`, `scripts/layer_demo.lua`, `scripts/pikachu_demo.lua`, `scripts/e2e_parity.lua` — current `function update(dt)` / `function draw()` signatures confirmed
  - `.planning/STATE.md` — "ScriptProxy uses full userdata (not lightuserdata)" locked decision; validity mechanism blocker confirmed
  - Phase 29 RESEARCH.md — `Object::getName()` / `Object::setName()` design confirmed
  - `CMakeLists.txt` — `VCV_RACK` define used for both core lib and SDL runner (SDL runner uses `VCV_RACK` transitively through `enjin2_core`); `ENJIN2_BUILD_LUA` confirmed

### Secondary (MEDIUM confidence)

- Lua 5.1 Reference Manual metamethod patterns (training knowledge, cross-verified against existing project code that uses identical patterns for registry storage in `bindings.cpp`)

### Tertiary (LOW confidence)

None — all critical claims verified from live codebase or confirmed against Lua 5.1 reference.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; Lua C API patterns confirmed from existing `bindings.cpp` code
- Architecture: HIGH — all C++ types, method names, and call sites verified from direct file inspection
- Pitfalls: HIGH — all pitfalls traced to specific existing patterns (circular include from Phase 30 research, registry storage from `bindings.cpp`, string lifetime from Phase 29 research, hot-reload from `sdl_main.cpp`)

**Research date:** 2026-02-27
**Valid until:** 90 days (stable C++ codebase; Lua 5.1 API is frozen)
