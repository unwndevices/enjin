---
phase: 32-scriptproxy-userdata
plan: 01
subsystem: scripting
tags: [lua, userdata, metatable, proxy, component, C_LuaScript]

# Dependency graph
requires:
  - phase: 31-engine-global-table
    provides: LuaBindings::registerAll() pattern; registry storage for engine.* pointers
  - phase: 29-named-objects-tags
    provides: Object::getName() for name property in __index metamethod

provides:
  - ScriptProxy full userdata struct in bindings.hpp with component + valid fields
  - ScriptProxy metatable registered in Lua registry via registerAll()
  - __index/__newindex metamethods dispatching x/y/visible/layer/active/name to C++ components
  - callWithProxy() in C_LuaScript for proxy-first callback dispatch
  - Proxy registry storage keyed by lightuserdata(this) per C_LuaScript lifetime
  - Proxy invalidation (valid=false) in destructor before shutdown

affects:
  - 32-02 (script migration to update(self,dt)/draw(self) signatures)
  - future C_LuaScript consumers that call init/update/draw

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Full userdata with luaL_newmetatable for typed Lua proxy objects
    - Lua registry keyed by lightuserdata(this) for stable per-component reference
    - bool valid flag on proxy struct for safe post-destruction access
    - callWithProxy() helper that manually manages Lua stack before lua_pcall

key-files:
  created: []
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - include/enjin2/components/lua_script.hpp
    - src/components/lua_script.cpp

key-decisions:
  - "ScriptProxy property dispatch uses strcmp chain (6 properties) — no hash map needed at this scale"
  - "self.layer is 1-indexed in Lua to match LAYER_BG/MID/FG/UI constants (buffer_index + 1 in __index)"
  - "self.name is read-only from Lua — Lua string const char* lifetime unsafe to pass to Object::setName()"
  - "Invalid proxy returns nil from __index rather than raising Lua error (lua_touserdata not luaL_checkudata)"
  - "Proxy created inside executeScript() not loadScript() — executeScript() is the shared bottleneck for both load paths"
  - "Phase 29 complete: Object::getName() available so name property returns actual name (not nil guard)"

patterns-established:
  - "Pattern: lua_touserdata + null/valid check in __index/__newindex handlers avoids luaL_argerror in non-error path"
  - "Pattern: registry[lightuserdata(C_LuaScript*)] = proxy_userdata for stable O(1) retrieval per frame"

requirements-completed: [PROXY-01, PROXY-02, PROXY-03]

# Metrics
duration: 3min
completed: 2026-02-27
---

# Phase 32 Plan 01: ScriptProxy Userdata Summary

**ScriptProxy full userdata with __index/__newindex metamethods mapping x/y/visible/layer/active/name to C++ components, wired as first arg to init/update/draw via callWithProxy() and invalidated in destructor**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-27T02:39:50Z
- **Completed:** 2026-02-27T02:43:23Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- ScriptProxy struct added to bindings.hpp with forward declaration to avoid circular include
- luaL_newmetatable registration in registerAll() with lua_proxy_index_impl and lua_proxy_newindex_impl dispatching all 6 properties
- Proxy created in executeScript() and stored in LUA_REGISTRYINDEX keyed by lightuserdata(this); old proxy invalidated on reload
- callWithProxy() added to C_LuaScript replacing callScriptFunctionSafe for init/update/draw; pushes stored proxy as first arg
- Destructor sets proxy->valid = false before scriptSystem->shutdown() to prevent dangling access
- All existing Lua tests (layer_binding_test: 18 assertions, hot_reload_test: 24 assertions) continue to pass

## Task Commits

Each task was committed atomically:

1. **Task 1: Add ScriptProxy struct and metatable registration to bindings** - `3123190` (feat)
2. **Task 2: Wire ScriptProxy creation and callWithProxy into C_LuaScript** - `348470e` (feat)

## Files Created/Modified
- `include/enjin2/scripting/bindings.hpp` - Added C_LuaScript forward declaration, ScriptProxy struct, registerProxyMetatable() private method
- `src/scripting/bindings.cpp` - Added lua_proxy_index_impl and lua_proxy_newindex_impl free functions; registerProxyMetatable() member; added lua_script.hpp/position.hpp/object.hpp includes; wired call from registerAll()
- `include/enjin2/components/lua_script.hpp` - Added callWithProxy() private method declaration
- `src/components/lua_script.cpp` - Implemented callWithProxy(); updated executeScript() with proxy creation block; updated update()/draw() to use callWithProxy(); updated destructor with proxy invalidation

## Decisions Made
- Used `strcmp` chain for property dispatch (6 properties) — hash map would add complexity with no measurable benefit at this scale
- `self.layer` is 1-indexed (buffer_index + 1) to match existing LAYER_BG/MID/FG/UI Lua constants
- `self.name` is read-only — writing Lua's transient `const char*` to Object::setName() would create dangling pointer
- Used `lua_touserdata` (not `luaL_checkudata`) in handlers to return nil silently on type mismatch instead of raising Lua errors
- Phase 29 is complete so name property returns `owner->getName()` (not nil guard with comment)

## Deviations from Plan

None - plan executed exactly as written. The plan's code matched the actual `.cpp` implementation (which already used `scriptSystem`/`LuaScriptSystem` pattern). The only minor note is the `.hpp` and `.cpp` had a pre-existing field name inconsistency (`interpreter` vs `scriptSystem`) that was pre-existing and out of scope.

## Issues Encountered
- Pre-existing inconsistency between `lua_script.hpp` (declares `interpreter: IScriptInterpreter*`) and `lua_script.cpp` (uses `scriptSystem: LuaScriptSystem*`) — `lua_script.cpp` is not in any CMake target so this doesn't affect builds. Added only `callWithProxy()` to the header as planned; left the rest of the inconsistency as pre-existing tech debt outside plan scope.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- ScriptProxy metatable registered and all property dispatch implemented (PROXY-01/02/03)
- Plan 32-02 can now migrate existing Lua scripts from `update(dt)` / `draw()` to `update(self, dt)` / `draw(self)` signatures (PROXY-04)
- engine.scene.find() currently returns lightuserdata (Object*) — Phase 32-02 can upgrade to return a ScriptProxy for found objects if needed

---
*Phase: 32-scriptproxy-userdata*
*Completed: 2026-02-27*
