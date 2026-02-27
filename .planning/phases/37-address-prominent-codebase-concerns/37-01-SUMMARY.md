---
phase: 37-address-prominent-codebase-concerns
plan: 01
subsystem: scripting
tags: [lua, scriptproxy, bindings, hardening, tags, error-handling]

# Dependency graph
requires:
  - phase: 32-scriptproxy-userdata
    provides: "ScriptProxy userdata + metatable; callWithProxy() pattern"
  - phase: 29-named-objects-tags
    provides: "Object::addTag/hasTag/clearTags/getTagCount API"
  - phase: 33-scripterrorpolicy
    provides: "ScriptErrorPolicy; char errorMessage field target"
provides:
  - "ScriptProxy __index/__newindex raise luaL_error on stale proxy (not silent nil)"
  - "Tag method bindings: addTag/hasTag/clearTags callable via self:method() from Lua"
  - "C_LuaScript::errorMessage as char[256] zero-heap-allocation fixed buffer"
  - "Object::addComponent overflow asserts in debug, fprintf(stderr) in release"
  - "script_proxy_lifetime_test: 20 assertions covering stale proxy + tag round-trips"
affects: [38-address-prominent-codebase-concerns-02, future-lua-phases]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Split proxy guard: !proxy -> nil (C++ caller error); stale proxy -> luaL_error (Lua caller error)"
    - "Tag method dispatch via strcmp chain in __index returning lua_pushcfunction"
    - "Fixed char[256] buffer with snprintf replaces std::string errorMessage"
    - "addComponent overflow: NDEBUG-gated assert vs fprintf(stderr) — no abort on embedded"

key-files:
  created:
    - tests/script_proxy_lifetime_test.cpp
  modified:
    - src/scripting/bindings.cpp
    - include/enjin2/components/lua_script.hpp
    - src/components/lua_script.cpp
    - include/enjin2/core/object.hpp
    - tests/CMakeLists.txt

key-decisions:
  - "Stale proxy guard split into two checks: !proxy returns nil (no metatable object — C++ error), stale valid/component calls luaL_error (Lua script accessing destroyed object)"
  - "Tag impl functions use luaL_checkudata (not lua_touserdata) for type-safe argument coercion — consistent with Lua convention for method dispatch"
  - "Forward declarations placed before __index so tag dispatch references are resolved at link time"
  - "char[256]{} zero-initializes via C++17 value-initialization — no explicit memset needed"
  - "addComponent overflow: assert(false&&'message') in debug (kills build/test fast); fprintf+return nullptr in release (no esp_restart abort on embedded targets)"

patterns-established:
  - "luaL_error for Lua-visible errors from Lua code; lua_pushnil for C++-internal guard failures"
  - "Method dispatch in __index: compare key string, push lua_pushcfunction, return 1"
  - "Fixed-size error buffer: snprintf for all assignments, [0]='\\0' for clear"

requirements-completed: []

# Metrics
duration: 4min
completed: 2026-02-27
---

# Phase 37 Plan 01: Address Prominent Codebase Concerns Summary

**ScriptProxy hardened with stale luaL_error + tag method Lua bindings; errorMessage converted to char[256] zero-allocation buffer; addComponent overflow assertion added**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-02-27T18:18:14Z
- **Completed:** 2026-02-27T18:21:34Z
- **Tasks:** 3 completed
- **Files modified:** 5 modified, 1 created

## Accomplishments

- ScriptProxy stale access now raises `luaL_error(L, "object has been destroyed")` in both `__index` and `__newindex` instead of silently returning nil
- Tag methods `addTag`, `hasTag`, `clearTags` are now fully callable from Lua via `self:addTag("tag")` syntax — completing Phase 29 Lua API surface
- `C_LuaScript::errorMessage` converted from `std::string` to `char errorMessage[256]{}` eliminating heap allocation on error paths; `getErrorMessage()` returns `const char*`
- `Object::addComponent` overflow path now asserts in debug builds and prints to stderr in release builds (was silent nullptr)
- `script_proxy_lifetime_test` added with 20 assertions: stale proxy reload cycle + TAG-01/02/03 tag binding round-trips — all 17 ctests pass

## Task Commits

Each task was committed atomically:

1. **Task 1: ScriptProxy stale error + tag method bindings** - `22c85c0` (feat)
2. **Task 2: errorMessage fixed buffer + component limit assertion** - `84d7129` (feat)
3. **Task 3: script_proxy_lifetime_test + CMakeLists registration** - `68af546` (test)

## Files Created/Modified

- `src/scripting/bindings.cpp` - Split proxy guard; add lua_proxy_addTag/hasTag/clearTags_impl; tag dispatch in __index
- `include/enjin2/components/lua_script.hpp` - errorMessage: char[256]{}; getErrorMessage(): const char*
- `src/components/lua_script.cpp` - All errorMessage assignments to snprintf; .c_str() removed; add #include <cstdio>
- `include/enjin2/core/object.hpp` - addComponent overflow: assert/fprintf; add #include <cassert> <cstdio>
- `tests/script_proxy_lifetime_test.cpp` - New: PROXY-STALE + TAG-01/02/03, 20 assertions
- `tests/CMakeLists.txt` - Register script_proxy_lifetime_test in ENJIN2_BUILD_LUA block

## Decisions Made

- Split proxy null check: `!proxy` returns nil (defensive C++ guard — userdata was never set); `!proxy->valid || !proxy->component` calls `luaL_error` (Lua script bug — object was destroyed). Two distinct failure modes warrant separate handling.
- Tag dispatch methods use `luaL_checkudata` not `lua_touserdata` for type safety in the method body (method is invoked by Lua, not internally, so Lua-level type checking is appropriate).
- Forward declarations inserted before `__index` rather than reordering functions — minimizes diff, follows C idiom for mutual-referencing static functions.
- `char[256]{}` zero-initializes in C++17 value-initialization — no explicit `memset` needed; the `{}` suffix is sufficient.
- Release-mode overflow uses `fprintf(stderr, ...)` + `return nullptr` — does NOT abort or restart. Embedded targets (ESP32) have no recovery from `assert(false)` in release; returning nullptr lets the caller handle gracefully.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 37-02 is unblocked: stale proxy error + tag bindings are verified working
- All 17 ctests pass confirming no regressions
- The four "Looks Done But Isn't" checklist items from CONCERNS.md are now closed

## Self-Check: PASSED

- tests/script_proxy_lifetime_test.cpp: FOUND
- src/scripting/bindings.cpp: FOUND
- include/enjin2/components/lua_script.hpp: FOUND
- src/components/lua_script.cpp: FOUND
- include/enjin2/core/object.hpp: FOUND
- Commit 22c85c0: FOUND
- Commit 84d7129: FOUND
- Commit 68af546: FOUND

---
*Phase: 37-address-prominent-codebase-concerns*
*Completed: 2026-02-27*
