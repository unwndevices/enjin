---
phase: quick-05
plan: 01
subsystem: scripting/bindings
tags: [conformity, zero-alloc, printf-policy, lua-bindings]
dependency_graph:
  requires: []
  provides: [LuaBindings-zero-std-string, printf-only-lua-print, float-distance-global]
  affects: [bindings.hpp, bindings.cpp]
tech_stack:
  added: []
  patterns: [printf-only-output, char-fixed-arrays, strncpy-for-string-copy, inline-lua-table-construction]
key_files:
  created: []
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
decisions:
  - "FontEntry.name and currentFontName use char[32] fixed arrays — sufficient for font names like 'default8', zero heap cost"
  - "registerTable helper removed — its single call-site replaced with inline lua_newtable/setfield pattern; eliminates std::vector and std::string from the private API"
  - "lua_math_distance uses std::sqrt with float inputs — consistent with Vec2::distance (types.hpp line 118) and the float-first engine policy"
  - "lua_time removed entirely — engine.time.now() (totalTime) and engine.time.delta() are the canonical API; std::chrono problematic on ESP32"
metrics:
  duration: "~5 minutes"
  completed: "2026-02-27"
  tasks_completed: 2
  files_changed: 2
---

# Quick Task 5: Conformity Fix — bindings.hpp and bindings.cpp Summary

Restored alignment with enjin2's zero-heap-allocation constraint and Phase 31-02 printf-only decision by removing all std::string/std::vector from LuaBindings class members, replacing std::cout with printf in lua_print, removing the std::chrono-based lua_time, and fixing lua_math_distance to use float coordinates consistent with Vec2.

## What Was Fixed and Why

### Fix 1: std::string members in LuaBindings — zero-alloc constraint

**File:** `include/enjin2/scripting/bindings.hpp`

The text-rendering commit (0470058) introduced three heap-allocating fields into `LuaBindings`:
- `std::string currentFontName{"default"}` — replaced with `char currentFontName[32]{"default"}`
- `FontEntry::name` as `std::string` — replaced with `char name[32]`
- `registerFont(const std::string& name, ...)` — changed to `registerFont(const char* name, ...)`

The `registerTable` private helper's signature used `const std::string&` and `const std::vector<std::pair<std::string, lua_CFunction>>&`. The helper's single call-site (the "love" stub table) was replaced with an inline lua_newtable/setfield block, and the helper itself was deleted. This eliminates the need for `#include <string>` and `#include <vector>` in the header.

The header now includes `<cstring>` for strncpy/strcmp used in the implementation.

**Constraint restored:** No dynamic allocation in the LuaBindings hot path. Font names up to 31 chars are sufficient (longest registered font name in codebase: "default8" = 8 chars).

### Fix 2: std::cout in lua_print — Phase 31-02 printf-only decision

**File:** `src/scripting/bindings.cpp`

The old `lua_print` called `std::cout` for strings/numbers/booleans. This violated the explicit Phase 31-02 decision: "engine.log uses printf exclusively (not std::cout) — embedded target compatibility with ESP32/Emscripten."

The new implementation:
```cpp
int LuaBindings::lua_print(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) { printf("%s", s); }
        else    { printf("(%s)", lua_typename(L, lua_type(L, i))); }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}
```
This also improves multi-argument printing (matching Lua 5.x standard print behaviour with tab separators) and uses the lua_typename fallback pattern already established in lua_engine_log.

`#include <iostream>` removed.

### Fix 3: lua_time using std::chrono — removed entirely

**File:** `src/scripting/bindings.cpp`

The global `time()` Lua function used `std::chrono::steady_clock`, which is incompatible with ESP32 and was already superseded by `engine.time.now()` (totalTime) and `engine.time.delta()` (dt). The function and its registration line were removed.

`#include <chrono>` removed.

### Fix 4: lua_math_distance using int16_t — float consistency

**File:** `src/scripting/bindings.cpp`

The math binding commit (b412e3e) registered a global `distance(x1, y1, x2, y2)` using `luaL_checkinteger` and `int16_t` coordinates, then delegated to `math::distance` which returns `uint16_t`. This was inconsistent with `Vec2::distance` (types.hpp line 118: `static float distance(const Vec2&, const Vec2&)`) and the float-first engine policy established in Phase 28.

New implementation uses `luaL_checknumber` + `float` inputs + `std::sqrt` inline, returning a float to Lua via `lua_pushnumber`. This aligns with Vec2 and accepts fractional coordinates.

## Files Changed

| File | Change |
|------|--------|
| `include/enjin2/scripting/bindings.hpp` | Removed #include string; changed currentFontName to char[32]; changed FontEntry::name to char[32]; changed registerFont param to const char*; removed lua_time declaration; removed registerTable declaration |
| `src/scripting/bindings.cpp` | Removed #include iostream and chrono; rewrote lua_print with printf; deleted lua_time body and registration; updated registerFont/setFont/getFont/resetSpritePool to use strcmp/strncpy; deleted registerTable body; inlined love table construction; fixed lua_math_distance to use float |

## Verification Results

```
grep -rn "std::string|std::vector" bindings.hpp bindings.cpp (LuaBindings class only)
# Result: 0 matches in LuaBindings members

grep -n "std::cout|iostream" bindings.cpp
# Result: 0 matches (only a comment mentioning printf vs std::cout)

cmake --build build --target enjin2_sdl
# Result: 0 errors, 0 relevant warnings

ctest -R "engine_table|math_binding|collision_test"
# Result: 3/3 PASSED
```

## Commits

| Hash | Description |
|------|-------------|
| 130ad80 | fix(quick-05): replace std::string members in LuaBindings with char[32]/const char* |
| 9e859fd | fix(quick-05): restore conformity in bindings.cpp — printf, float distance, no chrono |

## Out of Scope (Pre-existing — Not Fixed)

`LuaScriptSystem::executeScript`, `LuaScriptSystem::loadScript`, and `LuaScriptSystem::callFunction` still use `std::string` parameters. The plan explicitly notes these are "higher-level glue, not in the zero-alloc hot path" and are not in scope for this fix.

## Self-Check: PASSED

- FOUND: include/enjin2/scripting/bindings.hpp
- FOUND: src/scripting/bindings.cpp
- FOUND: .planning/quick/5-check-conformity-to-plan-direction-and-s/5-SUMMARY.md
- FOUND commit: 130ad80
- FOUND commit: 9e859fd
