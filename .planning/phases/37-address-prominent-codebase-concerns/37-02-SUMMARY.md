---
phase: 37-address-prominent-codebase-concerns
plan: 02
subsystem: scripting
tags: [lua, objectproxy, metatable, userdata, safety, invalidation, scene-find]

# Dependency graph
requires:
  - phase: 37-01
    provides: ScriptProxy stale luaL_error, tag bindings, char[256] errorMessage, addComponent overflow assert
  - phase: 32-scriptproxy-userdata
    provides: ScriptProxy userdata pattern (full userdata vs lightuserdata precedent)
  - phase: 31-engine-global-table
    provides: engine.scene.find() lightuserdata implementation to replace

provides:
  - ObjectProxy struct (include/enjin2/scripting/object_proxy.hpp) — standalone header, no Lua includes
  - engine.scene.find() returns ObjectProxy userdata with 'ObjectProxy' metatable, not lightuserdata
  - ObjectProxy __index: name (read-only string), hasTag(tag) method, position table {x,y}, enable boolean
  - ObjectProxy __newindex: position write -> C_Position::setPosition(); enable write -> C_LuaScript::setEnabled()
  - Object::~Object() destructor body sets m_luaProxy->valid = false before memory freed
  - object_proxy_test.cpp: 6 tests, 24 assertions (OBJ-PROXY-01..06)
affects: [engine-scene-api, lua-scripting, object-lifecycle, component-control]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ObjectProxy standalone header pattern: struct with Object* and bool valid in scripting/object_proxy.hpp
      avoids circular include between object.hpp and bindings.hpp"
    - "Destructor invalidation hook: Object::~Object() sets m_luaProxy->valid=false before freeing;
      mirrors ScriptProxy pattern from Phase 32 applied to Object-level proxy"
    - "Non-capturing lambda as lua_pushcfunction argument: [](lua_State* L2) -> int {...} converts
      to plain function pointer (C++11); used for hasTag method returned by __index"

key-files:
  created:
    - include/enjin2/scripting/object_proxy.hpp
    - tests/object_proxy_test.cpp
  modified:
    - include/enjin2/core/object.hpp
    - src/core/object.cpp
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - src/scripting/bindings_engine.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "ObjectProxy impl functions (lua_objproxy_index_impl, lua_objproxy_newindex_impl) are
    file-scope static in bindings.cpp, NOT static member declarations in bindings.hpp.
    Declaring them as class members caused linker undefined-reference when the impl was a free
    static; removed member declarations and kept impl as file-scope statics (simpler, no ABI overhead)"
  - "proxy.enable reads/writes C_LuaScript::isEnabled/setEnabled via obj->getComponent<C_LuaScript>().
    Returns nil (not error) when object has no C_LuaScript — enables proxy on non-scripted objects
    without Lua errors"
  - "position read returns snapshot table {x, y}; position write reads .x and .y from the table arg.
    This is a value-semantics copy (not a live reference) — consistent with embedded target constraints
    where no garbage-collected table proxy is appropriate"
  - "Object constructor already auto-adds C_Position; OBJ-PROXY-03 uses obj.getPosition() directly
    rather than addComponent<C_Position>() to avoid double-adding"

patterns-established:
  - "Standalone proxy header pattern: struct {T* ptr; bool valid} in its own header with only a
    forward-declaration of T avoids circular includes in component/scripting layering"
  - "Lua metatable for engine-returned objects: lua_newuserdata + luaL_getmetatable + lua_setmetatable
    then setLuaProxy() back-pointer so destructor can invalidate"

requirements-completed: []

# Metrics
duration: 5min
completed: 2026-02-27
---

# Phase 37 Plan 02: ObjectProxy Safety Summary

**ObjectProxy userdata with metatable replaces lightuserdata in engine.scene.find(), with Object destructor hook invalidating stale proxy references via proxy.valid = false**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-02-27T18:24:48Z
- **Completed:** 2026-02-27T18:29:34Z
- **Tasks:** 2
- **Files modified:** 8 (5 modified + 2 created + 1 CMakeLists)

## Accomplishments

- Replaced raw lightuserdata in `engine.scene.find()` with safe ObjectProxy full userdata carrying a `valid` flag and `Object*` pointer
- Implemented `__index` metamethod exposing `name`, `hasTag(tag)`, `position` (table snapshot), `enable` (C_LuaScript state)
- Implemented `__newindex` metamethod supporting `proxy.position = {x=N, y=M}` dispatching to `C_Position::setPosition()` and `proxy.enable = bool` dispatching to `C_LuaScript::setEnabled()`
- Added `Object::~Object()` destructor body that sets `m_luaProxy->valid = false` before Object memory is freed, closing the dangling-pointer risk from lightuserdata
- Created `object_proxy_test.cpp` with 6 tests (24 assertions) covering nil-path, C++ API delegation, stale invalidation, and enable/disable control
- All 18 ctests pass (17 pre-existing + 1 new object_proxy_test)

## Task Commits

Each task was committed atomically:

1. **Task 1: ObjectProxy struct + registration + Object destructor hook** - `dd01c2b` (feat)
2. **Task 2: lua_engine_scene_find returns ObjectProxy + object_proxy_test** - `70ddd3b` (feat)

**Plan metadata:** (docs commit to follow)

## Files Created/Modified

- `include/enjin2/scripting/object_proxy.hpp` - Standalone ObjectProxy struct (Object* + bool valid); no Lua includes
- `include/enjin2/core/object.hpp` - Added ObjectProxy* m_luaProxy field; setLuaProxy() setter; changed ~Object() from =default to explicit declaration
- `src/core/object.cpp` - Added Object::~Object() body: sets m_luaProxy->valid=false before freed
- `include/enjin2/scripting/bindings.hpp` - Added #include object_proxy.hpp; declared registerObjectProxyMetatable()
- `src/scripting/bindings.cpp` - Added OBJECT_PROXY_METATABLE constant, lua_objproxy_index_impl, lua_objproxy_newindex_impl, registerObjectProxyMetatable(); called from registerAll()
- `src/scripting/bindings_engine.cpp` - lua_engine_scene_find upgraded: lua_newuserdata ObjectProxy + metatable + setLuaProxy() back-pointer
- `tests/object_proxy_test.cpp` - 6 tests across OBJ-PROXY-01..06
- `tests/CMakeLists.txt` - Added object_proxy_test target inside ENJIN2_BUILD_LUA block

## Decisions Made

- ObjectProxy metamethod impl functions are file-scope statics in bindings.cpp (not static class members in bindings.hpp). Declaring them as class members caused linker undefined-reference when impl was a free static; removed member declarations, kept impl as file-scope statics.
- `proxy.enable` returns nil (not error) when object has no C_LuaScript component — enables proxy usage on non-scripted objects without throwing Lua errors.
- `proxy.position` read returns a snapshot table `{x, y}` (value semantics, not live reference) — appropriate for embedded targets where a live-reference proxy table would require GC overhead.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed static member declarations for lua_objproxy_index_impl and lua_objproxy_newindex_impl from bindings.hpp**
- **Found during:** Task 1 (build verification)
- **Issue:** Plan specified declaring static impl functions as class members in bindings.hpp AND defining them as file-scope statics in bindings.cpp. Linker error: undefined reference to `enjin2::LuaBindings::lua_objproxy_index_impl(lua_State*)` — class members and file-scope statics are different symbols.
- **Fix:** Removed the two static member declarations from bindings.hpp. The impls remain file-scope statics in bindings.cpp, consistent with the ScriptProxy metatable pattern already established.
- **Files modified:** include/enjin2/scripting/bindings.hpp
- **Verification:** Build succeeds, all 17+1 ctests pass
- **Committed in:** dd01c2b (Task 1 commit, fix applied inline)

---

**Total deviations:** 1 auto-fixed (Rule 1 — build-blocking linker error from conflicting declarations)
**Impact on plan:** Fix was necessary for correct compilation. No scope change. Pattern now consistent with existing ScriptProxy implementation.

## Issues Encountered

None beyond the single linker error documented above.

## Next Phase Readiness

- Phase 37-02 complete: ObjectProxy fully implemented and tested
- Phase 37 may be complete (plan 02 was the last listed plan for this phase)
- `engine.scene.find()` now returns safe proxy with stale-invalidation — dangling-pointer concern closed
- Checklist item #2 ("Phase 32 completion: engine.scene.find() fully upgraded") is resolved

---
*Phase: 37-address-prominent-codebase-concerns*
*Completed: 2026-02-27*
