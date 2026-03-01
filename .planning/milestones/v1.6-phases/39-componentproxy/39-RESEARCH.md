# Phase 39: ComponentProxy - Research

**Researched:** 2026-02-28
**Domain:** Lua userdata proxies, C++ component lifetime management, ScriptProxy.__index dispatch
**Confidence:** HIGH

## Summary

Phase 39 adds `self:get("TypeName")` to the `ScriptProxy` interface so Lua scripts can retrieve typed proxies to sibling components on the same object. The proxy must expose component-specific methods (e.g., `timer:after()` for C_Timer, `fsm:setState()` for C_StateMachine) and must raise a `luaL_error` when accessed after the component is destroyed.

The entire pattern is already established in this codebase: `ObjectProxy` (Phase 37) is the direct model. A `ComponentProxy` struct mirrors `ObjectProxy` exactly — a raw pointer to the component plus a `bool valid` flag. The `Component` base class gains a `m_luaProxy` field and destructor logic identical to `Object`. The `ScriptProxy.__index` metamethod gains a `"get"` branch checked before all other properties to prevent name collision.

The most important architectural decision to resolve during planning is where the per-component-type method dispatch lives: each component type will need its own Lua metatable (e.g., `"C_Timer_ComponentProxy"`, `"C_StateMachine_ComponentProxy"`) registered during `LuaBindings::registerAll()`. Since Phases 40 and 41 have not been implemented yet, Phase 39 must build the infrastructure (ComponentProxy struct, Component base class integration, `self:get()` dispatch in ScriptProxy) without any concrete component proxy implementations — or it can include one stub/demo proxy to prove the system works end-to-end.

**Primary recommendation:** Mirror the `ObjectProxy` pattern exactly. Add `ComponentProxy` struct to a new standalone header, add `m_luaProxy` + destructor invalidation to `Component`, add `"get"` branch at the top of `lua_proxy_index_impl`, register a `"ComponentProxy"` base metatable, and write the test with a concrete component (the simplest option is using an existing component such as `C_Position` to prove the machinery works before C_Timer exists).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PROXY-01 | Lua script can access sibling components via `self:get("TypeName")` | ScriptProxy.__index gains `"get"` branch that calls owner->getComponent<T>() by type name string, wraps result in ComponentProxy userdata |
| PROXY-02 | Returned proxy is full userdata with typed method table (e.g., `timer:after()`, `fsm:setState()`) | Each component type registers a named metatable (e.g., `"C_Timer_Proxy"`) with its own `__index` method table; ComponentProxy struct holds `Component*` + `bool valid` + `const char* typeName` |
| PROXY-03 | Component destruction invalidates all outstanding proxies (valid flag pattern) | Component base class gains `ComponentProxy* m_luaProxy = nullptr` and destructor sets `m_luaProxy->valid = false` — identical to Object::~Object() invalidating ObjectProxy |
| PROXY-04 | Stale ComponentProxy access raises `luaL_error` (not silent nil) | ComponentProxy metatable `__index` checks `valid` first and calls `luaL_error(L, "component has been destroyed")` — identical to ObjectProxy stale-error pattern |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Lua (LuaJIT) | Bundled in `/luajit/` | Full userdata + metatables for ComponentProxy | Already in use for ScriptProxy, ObjectProxy |
| C++ `dynamic_cast` | C++11 | Type-safe component lookup by runtime type | Already used in `Object::getComponent<T>()` |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `luaL_newmetatable` / `luaL_checkudata` | Lua 5.1 API | Typed userdata metatables with safety check | Used in all existing proxy implementations |
| `luaL_testudata` | Lua 5.1 API | Non-throwing userdata type check | Used in `lua_engine_scene_destroy` for graceful nil-check |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Per-type metatables (`"C_Timer_Proxy"`) | Single `"ComponentProxy"` metatable with method dispatch per `typeName` string | Single table is simpler but prevents type-safe method lookup; per-type is the correct pattern — matches how `ObjectProxy` vs `ScriptProxy` are separate metatables |
| Runtime type dispatch via string `typeName` in `self:get()` | Template-based `self:get<C_Timer>()` | Lua cannot pass C++ template types; string dispatch is required and is the only option |

**Installation:** No new dependencies. All required infrastructure already present.

## Architecture Patterns

### Recommended Project Structure

New files for Phase 39:
```
include/enjin2/scripting/component_proxy.hpp   # ComponentProxy struct (mirrors object_proxy.hpp)
tests/component_proxy_test.cpp                  # PROXY-01..PROXY-04 test suite
```

Modified files:
```
include/enjin2/core/component.hpp              # Add m_luaProxy field + setLuaProxy() + destructor invalidation
src/scripting/bindings.cpp                     # Add "get" branch in lua_proxy_index_impl; registerComponentProxyMetatable()
include/enjin2/scripting/bindings.hpp          # Declare registerComponentProxyMetatable() private method
tests/CMakeLists.txt                            # Register component_proxy_test (under ENJIN2_BUILD_LUA guard)
```

### Pattern 1: ComponentProxy Struct (mirrors ObjectProxy exactly)

**What:** A standalone header with a plain struct holding a non-owning pointer plus validity flag. Placed in its own header to avoid circular includes.

**When to use:** Any time a Lua script holds a handle to a C++ component instance.

**Example:**
```cpp
// include/enjin2/scripting/component_proxy.hpp
#pragma once
namespace enjin2 {
class Component;  // Forward declaration only

/**
 * @brief Lua proxy userdata wrapping a raw Component* from self:get().
 *
 * Placed in a standalone header to avoid circular includes between component.hpp
 * and bindings.hpp. Component::~Component() sets valid = false before the component
 * is freed, preventing dangling-pointer access from stale Lua proxy references.
 *
 * Only one ComponentProxy should be active per Component at a time.
 */
struct ComponentProxy {
    Component* component;   ///< Non-owning. Do NOT dereference if valid == false.
    bool valid;             ///< Set false by Component::~Component() when component is destroyed.
};
} // namespace enjin2
```

### Pattern 2: Component Base Class Extension

**What:** Add `m_luaProxy` back-pointer + `setLuaProxy()` + destructor invalidation to the `Component` base class. Mirrors what `Object` does for `ObjectProxy`.

**When to use:** Any component that may be retrieved via `self:get()` will be registered automatically via this mechanism.

**Example:**
```cpp
// In component.hpp — additions only:

#include "../scripting/component_proxy.hpp"  // forward-declared header

class Component {
    // ... existing fields ...
private:
    ComponentProxy* m_luaProxy = nullptr;  ///< Non-owning; invalidated in ~Component()

public:
    // Existing virtual ~Component() = default; MUST become non-default:
    virtual ~Component() {
        if (m_luaProxy) {
            m_luaProxy->valid = false;
            m_luaProxy = nullptr;
        }
    }

    void setLuaProxy(ComponentProxy* proxy) { m_luaProxy = proxy; }
};
```

**Critical note:** `Component` currently declares `virtual ~Component() = default;`. This must change to a defined destructor body. Any existing subclass that explicitly defines its own destructor (e.g., `C_LuaScript`) will still call the base destructor automatically — the invalidation is correct and does not require changes to subclasses.

### Pattern 3: ScriptProxy.__index "get" Branch (PROXY-04 collision prevention)

**What:** Add a `"get"` key check at the very top of `lua_proxy_index_impl` — before `"x"`, `"y"`, `"visible"`, etc. — so the method name `get` cannot shadow a component property.

**When to use:** Required. The success criteria explicitly mandates that `"get"` is checked first.

**Example:**
```cpp
// In src/scripting/bindings.cpp — lua_proxy_index_impl additions:

static int lua_proxy_index_impl(lua_State* L) {
    // ... existing validity checks ...

    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    // PROXY-04: Check "get" FIRST before any property/method lookup
    if (strcmp(key, "get") == 0) {
        lua_pushcfunction(L, lua_proxy_get_component_impl);
        return 1;
    }

    // ... existing "x", "y", "visible", "layer", "active", "name", tag methods ...
}
```

### Pattern 4: self:get("TypeName") Implementation

**What:** `lua_proxy_get_component_impl` receives `self` (ScriptProxy userdata) and a type name string. It calls `owner->getComponent<T>()` via a type-name registry (a static dispatch table mapping strings to component-finding functions), then wraps the result in a `ComponentProxy` userdata with the per-type metatable.

**The type registry problem:** C++ templates cannot be instantiated from Lua-side strings at runtime. The solution is a static table of `{type_name_string, finder_function}` entries. Each concrete component type that should be proxied registers an entry. In Phase 39, this can be a simple `if/else if` chain since there are only 2-3 component types initially (C_Timer, C_StateMachine). A table-driven approach is cleaner but both work.

**Example:**
```cpp
static int lua_proxy_get_component_impl(lua_State* L) {
    // Stack: [1]=ScriptProxy userdata (self), [2]=type_name_string
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }

    const char* typeName = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    if (!owner) { lua_pushnil(L); return 1; }

    // Type dispatch — only registered component types are proxied
    enjin2::Component* comp = nullptr;
    const char* metaName = nullptr;

    // Phase 39 stub: no concrete component types yet
    // Phase 40 will add: if (strcmp(typeName, "C_Timer") == 0) { comp = owner->getComponent<C_Timer>(); metaName = "C_Timer_Proxy"; }
    // Phase 41 will add: if (strcmp(typeName, "C_StateMachine") == 0) { ... }

    if (!comp) { lua_pushnil(L); return 1; }

    // Allocate ComponentProxy userdata and set the component-specific metatable
    auto* cproxy = static_cast<enjin2::ComponentProxy*>(
        lua_newuserdata(L, sizeof(enjin2::ComponentProxy)));
    cproxy->component = comp;
    cproxy->valid = true;
    luaL_getmetatable(L, metaName);
    lua_setmetatable(L, -2);

    // Register proxy with component for destructor invalidation
    comp->setLuaProxy(cproxy);

    return 1;
}
```

### Pattern 5: Per-Type ComponentProxy Metatable Registration

**What:** Each component type that participates in the proxy system registers its own named metatable in `registerAll()`. The metatable defines `__index` with component-specific methods.

**Example (for Phase 39 base infrastructure):**
```cpp
// "ComponentProxy" base metatable — used for generic stale-error checking
// Per-type metatables (e.g., "C_Timer_Proxy") will be registered by Phase 40+

void LuaBindings::registerComponentProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;
    if (luaL_newmetatable(L, "ComponentProxy")) {
        lua_pushcfunction(L, lua_component_proxy_index_impl);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);
}

// Base __index for ComponentProxy — handles stale check only
static int lua_component_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, "ComponentProxy"));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    lua_pushnil(L);
    return 1;
}
```

### Anti-Patterns to Avoid

- **Storing ComponentProxy in the Lua registry keyed by `this` pointer:** This is how ScriptProxy works (one-per-script). ComponentProxy is different: multiple scripts can call `self:get("C_Timer")` and each needs an independent proxy. Do NOT key by `comp` pointer in the registry the same way — multiple outstanding proxies per component are valid. The component's `m_luaProxy` field is a limitation (single proxy back-pointer for invalidation) documented in STATE.md as the "single-proxy-per-component constraint." **Resolution:** Accept the single-proxy-per-component limitation and emit a dev-mode warning on `setLuaProxy()` overwrite, as noted in STATE.md v1.6 OPEN item.

- **Using lightuserdata for ComponentProxy:** ScriptProxy uses full userdata (Phase 32 decision: lightuserdata cannot have per-object metatables). ComponentProxy must also be full userdata.

- **Checking `"get"` after other properties:** The success criteria and ROADMAP explicitly require `"get"` is checked first to prevent name collision with future component properties named `get`.

- **Not invalidating in `Component::~Component()`:** The destructor is currently `= default`. If the base class destructor is not given a body, C++ will never run invalidation code. The destructor must be changed to a defined virtual destructor body.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Typed userdata type safety | Custom tag field on struct | `luaL_newmetatable` + `luaL_checkudata` | Already handles type checking with named metatables |
| String-to-type dispatch table | Complex registry with function pointers and heap allocation | Inline `if/else if` chain in `lua_proxy_get_component_impl` | Only 2 types in v1.6; table-driven is premature; the `if/else if` is O(n) but n=2 |
| Proxy invalidation on destruction | Reference counting or weak pointers | Valid flag pattern (mirroring ObjectProxy exactly) | Zero allocation, O(1), already proven |

**Key insight:** Every technical pattern needed for Phase 39 is already implemented in this codebase. The implementation is mechanical replication of the ObjectProxy/ScriptProxy patterns applied to the Component layer.

## Common Pitfalls

### Pitfall 1: Component Destructor is Currently Default
**What goes wrong:** `Component`'s destructor is `virtual ~Component() = default;`. Adding `m_luaProxy` and forgetting to change the destructor means the invalidation code never runs.
**Why it happens:** C++ generates a default destructor body that does nothing beyond destroying members. The `m_luaProxy` pointer will not have its target's `valid` flag set to `false`.
**How to avoid:** Change `virtual ~Component() = default;` to an explicit destructor definition in `component.cpp`.
**Warning signs:** The test `PROXY-03` (stale proxy raises luaL_error) will succeed in scenarios where the component outlives the test but fail to invalidate when the component is destroyed.

### Pitfall 2: Circular Include Between component.hpp and component_proxy.hpp
**What goes wrong:** `component_proxy.hpp` needs `Component*` as a forward declaration; `component.hpp` needs `component_proxy.hpp` for `ComponentProxy*`. If done naively with full includes, circular dependency arises.
**Why it happens:** This is the exact same problem that required `object_proxy.hpp` to be a standalone header with only a forward declaration `class Object;`.
**How to avoid:** Follow the existing pattern exactly: `component_proxy.hpp` contains only a forward declaration `class Component;` and the struct definition. `component.hpp` includes `component_proxy.hpp`.

### Pitfall 3: Single-Proxy-Per-Component Constraint (STATE.md OPEN item)
**What goes wrong:** If two scripts on the same object both call `self:get("C_Timer")`, the second call overwrites `comp->m_luaProxy`, and the first proxy will not be invalidated on destruction. The first proxy becomes a ghost that appears valid but is never notified.
**Why it happens:** The back-pointer design from ObjectProxy assumes one proxy per object. For components, multiple scripts on the same object sharing one C_Timer creates the same issue.
**How to avoid:** For v1.6, this is an accepted limitation (STATE.md documents it explicitly). Add a `printf` dev-mode warning when `setLuaProxy()` is called on a component that already has a proxy registered. Document the "cache-in-init" pattern: scripts should call `self:get()` once in `init()` and cache the result, not repeatedly in `update()`.
**Warning signs:** Test with two C_LuaScript components on the same object both calling `self:get("C_Timer")` — the first proxy should still work (it will silently stop receiving invalidation notifications, not crash immediately).

### Pitfall 4: "get" vs Existing Property Name Collision in Test Design
**What goes wrong:** Writing a test that doesn't verify `"get"` is resolved before other keys, so the priority ordering isn't proven by the test suite.
**Why it happens:** The test writer may test `self:get("C_Timer")` without confirming that an object property named `get` would have been blocked.
**How to avoid:** Include a test that adds a key `"get"` to the owner Object (e.g., via a tag named `"get"`) and confirms the method dispatch still fires rather than returning the property value.

### Pitfall 5: No Concrete Component Type in Phase 39 → Empty Dispatch Table
**What goes wrong:** If Phase 39 ships with only the infrastructure and no concrete type registered, there is no end-to-end test proving PROXY-01 and PROXY-02 (the typed method table requirement). The phase cannot be marked complete.
**Why it happens:** The concrete component types (C_Timer, C_StateMachine) are Phase 40/41 work.
**How to avoid:** Use an existing component that already exists — `C_Position` makes a good test subject. Register a `"C_Position_Proxy"` metatable with a `getX()` and `getY()` method, and test that `self:get("C_Position")` returns a proxy on which `pos:getX()` works. This proves PROXY-01 and PROXY-02 without requiring Phase 40 to be complete.

## Code Examples

Verified patterns from existing codebase:

### Object Destructor Invalidation Pattern (source of truth for Component to replicate)
```cpp
// src/core/object.cpp — Object::~Object()
Object::~Object() {
    if (m_luaProxy) {
        m_luaProxy->valid = false;
        m_luaProxy = nullptr;
    }
}
```

### ObjectProxy Struct Layout (ComponentProxy must mirror this exactly)
```cpp
// include/enjin2/scripting/object_proxy.hpp
struct ObjectProxy {
    Object* object;   ///< Non-owning. Do NOT dereference if valid == false.
    bool valid;       ///< Set false by Object::~Object() when object is destroyed.
};
```

### Full Userdata Allocation with Metatable (pattern from engine.scene.find())
```cpp
// src/scripting/bindings_engine.cpp — lua_engine_scene_find()
auto* proxy = static_cast<enjin2::ObjectProxy*>(
    lua_newuserdata(L, sizeof(enjin2::ObjectProxy)));
proxy->object = obj;
proxy->valid  = true;
luaL_getmetatable(L, "ObjectProxy");
lua_setmetatable(L, -2);
obj->setLuaProxy(proxy);
return 1;
```

### Stale-Error Check Pattern (ComponentProxy __index must use this)
```cpp
// src/scripting/bindings.cpp — lua_objproxy_index_impl()
if (!proxy->valid || !proxy->object) {
    luaL_error(L, "object has been destroyed");
    return 0;  // unreachable — luaL_error longjmps
}
```

### ScriptProxy __index — "get" goes at the top (current code for reference)
```cpp
// src/scripting/bindings.cpp — lua_proxy_index_impl() (current state, no "get" yet)
static int lua_proxy_index_impl(lua_State* L) {
    // ... validity check ...
    const char* key = lua_tostring(L, 2);
    // "get" MUST be added here, before all existing strcmp() chains
    if (strcmp(key, "x") == 0) { ... }
    else if (strcmp(key, "y") == 0) { ... }
    // etc.
}
```

### Component __index Retrieval Pattern (how self:get() finds the component)
```cpp
// From Object::getComponent<T>() — the underlying mechanism
template<typename T>
T* getComponent() {
    for (size_t i = 0; i < componentCount; ++i) {
        if (auto component = dynamic_cast<T*>(components[i].get())) {
            return component;
        }
    }
    return nullptr;
}
// self:get("C_Position") -> owner->getComponent<C_Position>()
```

### Test Pattern (mirrors object_proxy_test.cpp structure)
```cpp
// tests/component_proxy_test.cpp — PROXY-03 stale test
static void test_proxy03_stale_raises_error() {
    ComponentProxy proxy;
    proxy.component = nullptr;
    proxy.valid = true;

    {
        Object tempObj;
        C_Position* pos = tempObj.getPosition();
        proxy.component = pos;
        proxy.valid = true;
        pos->setLuaProxy(&proxy);
        // pos goes out of scope when tempObj is destroyed
    }
    ASSERT(proxy.valid == false,
           "PROXY-03: Component destructor must set proxy.valid = false");
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| lightuserdata for proxy | Full userdata with metatables | Phase 32 decision | Required for metamethod dispatch |
| No proxy invalidation | Valid flag set in destructor | Phase 37 (ObjectProxy) | Stale access raises luaL_error |
| No component access from Lua | self:get("TypeName") (Phase 39) | This phase | Scripts can access sibling components |

## Open Questions

1. **Should Phase 39 include a C_Position proxy as the proof-of-concept implementation?**
   - What we know: Phase 40 (C_Timer) and 41 (C_StateMachine) don't exist yet. The phase success criteria mention `timer:after()` but Phase 39 cannot depend on Phase 40.
   - What's unclear: Whether the planner should scope Phase 39 to infrastructure-only (with a C_Position proxy for testing) or wait for Phase 40 to provide the first real proxy type.
   - Recommendation: Include a `C_Position_Proxy` metatable registration in Phase 39 with `getX()`/`getY()` methods. This satisfies PROXY-01 and PROXY-02 end-to-end without Phase 40 dependency. Phase 40 simply adds another entry to the dispatch table.

2. **Should the type dispatch in `self:get()` use a static array of structs or an if/else chain?**
   - What we know: v1.6 has at most 2 proxied component types (C_Timer, C_StateMachine). Both are added in Phase 40 and 41 respectively.
   - What's unclear: Whether a more extensible dispatch table is worth the complexity.
   - Recommendation: Use an if/else chain in Phase 39. Keep the comment noting where Phase 40 adds its entry. Refactor to a table-driven approach only if a third component type is needed.

3. **Does `Component::~Component()` need to be defined in a .cpp file, or can it be inline in the header?**
   - What we know: `Object::~Object()` is defined in `object.cpp`. The codebase pattern is to keep destructor bodies in .cpp files.
   - What's unclear: Whether an inline definition would cause ODR violations.
   - Recommendation: Define `Component::~Component()` in a new `component.cpp` (or inline with `= {}` plus body in the existing `.cpp`). Check if `src/ui/component.cpp` already defines a Component — it likely defines `enjin2::ui::Component`, not `enjin2::Component`. The enjin2 core `Component` has no existing `.cpp` file; the destructor body should be added inline in `component.hpp` marked with an explicit body, or a new `src/core/component.cpp` can be created and registered in the CMakeLists.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Hand-rolled (ASSERT macro + pass/failure counters) — matches all existing tests |
| Config file | none — vanilla ctest |
| Quick run command | `ctest -R component_proxy_test --output-on-failure` (from build dir) |
| Full suite command | `ctest --output-on-failure` (from build dir) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PROXY-01 | `self:get("C_Position")` in `init()` returns non-nil proxy | unit (Lua) | `ctest -R component_proxy_test` | No — Wave 0 |
| PROXY-02 | Proxy has typed method table (`pos:getX()` returns correct value) | unit (Lua) | `ctest -R component_proxy_test` | No — Wave 0 |
| PROXY-03 | Component destruction sets proxy.valid = false | unit (C++) | `ctest -R component_proxy_test` | No — Wave 0 |
| PROXY-04 | Stale proxy raises `luaL_error` (not nil, not crash) | unit (Lua) | `ctest -R component_proxy_test` | No — Wave 0 |

### Sampling Rate
- **Per task commit:** `ctest -R component_proxy_test --output-on-failure`
- **Per wave merge:** `ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/component_proxy_test.cpp` — covers PROXY-01, PROXY-02, PROXY-03, PROXY-04
- [ ] `tests/CMakeLists.txt` addition — `component_proxy_test` under `ENJIN2_BUILD_LUA` guard
- [ ] `include/enjin2/scripting/component_proxy.hpp` — new file (ComponentProxy struct)
- [ ] `include/enjin2/core/component.hpp` — modified to add `m_luaProxy` field and destructor body
- [ ] `src/scripting/bindings.cpp` — modified to add `"get"` branch in `lua_proxy_index_impl` and `registerComponentProxyMetatable()` call

## Sources

### Primary (HIGH confidence)
- Codebase: `include/enjin2/scripting/object_proxy.hpp` — ComponentProxy struct design
- Codebase: `src/core/object.cpp` — destructor invalidation pattern
- Codebase: `src/scripting/bindings.cpp` — lua_proxy_index_impl, ScriptProxy metatable registration, ObjectProxy metatable registration
- Codebase: `src/scripting/bindings_engine.cpp` — full userdata allocation + metatable assignment pattern
- Codebase: `include/enjin2/core/component.hpp` — current Component base class (no m_luaProxy, no destructor body)
- Codebase: `include/enjin2/core/object.hpp` — m_luaProxy field + setLuaProxy() model
- Codebase: `.planning/STATE.md` — single-proxy-per-component documented as OPEN concern

### Secondary (MEDIUM confidence)
- `.planning/ROADMAP.md` Phase 39 description — explicit requirement that `"get"` is checked first in ScriptProxy.__index
- `.planning/REQUIREMENTS.md` PROXY-01..PROXY-04 — requirement definitions

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all technology already in use in the codebase
- Architecture: HIGH — ObjectProxy/ScriptProxy patterns are direct, verified models; no external dependencies
- Pitfalls: HIGH — pitfalls identified directly from code inspection and STATE.md documented concerns

**Research date:** 2026-02-28
**Valid until:** 2026-03-30 (stable C++ codebase, 30-day window appropriate)
