---
phase: 39-componentproxy
plan: 01
subsystem: scripting
tags: [lua, component-proxy, c-position, userdata, metatable, destructor-invalidation]

# Dependency graph
requires:
  - phase: 37-objectproxy
    provides: "ObjectProxy pattern (valid flag + destructor invalidation) replicated exactly"
  - phase: 32-scriptproxy
    provides: "ScriptProxy full userdata decision (not lightuserdata)"
provides:
  - "ComponentProxy struct in include/enjin2/scripting/component_proxy.hpp"
  - "Component base class destructor invalidation via m_luaProxy field"
  - "self:get('TypeName') dispatch in ScriptProxy.__index (lua_proxy_get_component_impl)"
  - "C_Position_Proxy metatable with getX()/getY() methods and stale-check"
  - "PROXY-01..PROXY-04 test suite in tests/component_proxy_test.cpp"
affects:
  - "40-c-timer (add C_Timer_Proxy in lua_proxy_get_component_impl type dispatch)"
  - "41-c-statemachine (add C_StateMachine_Proxy)"

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ComponentProxy: standalone header with forward declaration to avoid circular includes"
    - "Component destructor invalidation: m_luaProxy->valid = false on destruction"
    - "Per-type ComponentProxy metatable: C_Position_Proxy registered in registerComponentProxyMetatable()"
    - "Type dispatch in lua_proxy_get_component_impl: extensible else-if chain for future phases"
    - "get key checked FIRST in ScriptProxy.__index before all other properties (collision prevention)"
    - "Non-capturing lambdas as getX/getY method implementations in C_Position_Proxy __index"

key-files:
  created:
    - include/enjin2/scripting/component_proxy.hpp
    - tests/component_proxy_test.cpp
  modified:
    - include/enjin2/core/component.hpp
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "ComponentProxy placed in standalone header (mirrors ObjectProxy isolation pattern) to avoid circular includes between component.hpp and bindings.hpp"
  - "Component destructor is inline defined body (not = default) mirroring Object::~Object() pattern exactly"
  - "self:get() check inserted FIRST in lua_proxy_index_impl before x/y/visible/active/name/addTag to prevent name collision (PROXY-04b)"
  - "Single-proxy-per-component constraint accepted: multiple self:get() calls overwrite m_luaProxy; last call wins (v1.6 limitation, carried from STATE.md)"
  - "PROXY-04 Lua-level stale test uses standalone LuaScriptSystem (not C_LuaScript) to decouple Lua state lifetime from C_Position owner lifetime"

patterns-established:
  - "Phase 40/41 pattern: add else-if in lua_proxy_get_component_impl type dispatch + register new metatable in registerComponentProxyMetatable()"

requirements-completed: [PROXY-01, PROXY-02, PROXY-03, PROXY-04]

# Metrics
duration: 35min
completed: 2026-02-28
---

# Phase 39 Plan 01: ComponentProxy Summary

**ComponentProxy infrastructure: self:get('C_Position') returns typed proxy userdata with getX()/getY(), stale-safe via Component::~Component() valid-flag invalidation**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-02-28T00:00:00Z
- **Completed:** 2026-02-28T00:35:00Z
- **Tasks:** 2
- **Files modified:** 6 (2 created, 4 modified)

## Accomplishments
- ComponentProxy struct in standalone header mirrors ObjectProxy exactly (forward decl only, no circular includes)
- Component base class extended with private m_luaProxy field, setLuaProxy() method, and defined destructor body that invalidates outstanding Lua proxies
- self:get("TypeName") dispatch added to ScriptProxy.__index FIRST (before all other properties) via lua_proxy_get_component_impl
- C_Position_Proxy metatable registered with getX()/getY() methods and stale check raising "component has been destroyed"
- Test suite: PROXY-01..PROXY-04b all passing (26 assertions), zero regressions in existing 23-test suite

## Task Commits

Each task was committed atomically:

1. **Task 1: Create ComponentProxy struct and extend Component base class** - `0d422fe` (feat)
2. **Task 2: Wire self:get() dispatch, C_Position_Proxy metatable, and test suite** - `91891e9` (feat)

**Plan metadata:** (final commit hash — see below)

## Files Created/Modified
- `include/enjin2/scripting/component_proxy.hpp` - ComponentProxy struct with forward-decl-only Component reference
- `include/enjin2/core/component.hpp` - Added m_luaProxy field, setLuaProxy(), defined destructor body
- `include/enjin2/scripting/bindings.hpp` - Added registerComponentProxyMetatable() declaration
- `src/scripting/bindings.cpp` - lua_proxy_get_component_impl + C_Position_Proxy metatable + registerComponentProxyMetatable()
- `tests/component_proxy_test.cpp` - PROXY-01..PROXY-04b test suite
- `tests/CMakeLists.txt` - component_proxy_test registration under ENJIN2_BUILD_LUA guard

## Decisions Made
- PROXY-04 Lua-level stale test uses standalone LuaScriptSystem (instead of C_LuaScript) because C_LuaScript owns the Lua state — destroying the Object would close the Lua state before we could observe the stale error. Standalone LuaScriptSystem decouples Lua lifetime from C_Position owner lifetime.
- "get" key checked FIRST in ScriptProxy.__index (lines 51-55) before C_LuaScript* comp assignment to avoid any chance of collision with future property names.

## Deviations from Plan

None - plan executed exactly as written.

The PROXY-04 test approach differed from the plan's suggested "destroy Object then run Lua snippet" (which can't work since C_LuaScript owns the Lua state), but the actual mechanism tested is identical: ComponentProxy.valid goes false via Component::~Component(), and __index raises luaL_error. The standalone LuaScriptSystem approach correctly exercises the full code path.

## Issues Encountered
- PROXY-04 test implementation required rethinking: the plan suggested destroying the Object and then running Lua code, but since C_LuaScript is on the same Object as C_Position, both die together. Solved by using a standalone LuaScriptSystem with direct Lua C API calls (lua_newuserdata + lua_setglobal) to create the proxy, then destroying the Object while the Lua state stays alive.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 40 (C_Timer): add `else if (strcmp(typeName, "C_Timer") == 0)` in lua_proxy_get_component_impl, add C_Timer_Proxy metatable to registerComponentProxyMetatable()
- Phase 41 (C_StateMachine): same pattern
- The extensible type dispatch (commented placeholders already in place) makes Phase 40/41 implementation straightforward
- Zero regressions: full 24-test suite passes

---
*Phase: 39-componentproxy*
*Completed: 2026-02-28*
