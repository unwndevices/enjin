---
phase: quick
plan: 8
subsystem: scripting/lua
tags: [lua, upgrade, cleanup, wasm, esp32, compat]
dependency_graph:
  requires: []
  provides: [lua-5.4.8-unified-across-all-targets]
  affects: [CMakeLists.txt, lua_platform.hpp, bindings_async.cpp, bindings_tween.cpp]
tech_stack:
  added: []
  patterns: [lua-5.4-unconditional-api]
key_files:
  created: []
  modified:
    - CMakeLists.txt
    - include/enjin2/scripting/lua_platform.hpp
    - src/scripting/lua_platform.cpp
    - src/scripting/bindings_async.cpp
    - src/scripting/bindings_tween.cpp
decisions:
  - "Use Lua 5.4.8 (not 5.3.x) to match desktop system Lua version already in use"
  - "Remove compat shims entirely rather than leaving dead code guarded by version checks"
metrics:
  duration: "1 minute"
  completed_date: "2026-03-04"
  tasks_completed: 2
  files_modified: 5
---

# Quick Task 8: Move to Lua 5.4 — SUMMARY

**One-liner:** Unified Lua 5.4.8 across all three targets (desktop/WASM/ESP32) with all 5.1 compat dead code removed and lua_resume collapsed to unconditional 5.4 form.

## What Was Done

WASM and ESP32 FetchContent targets previously fetched Lua 5.1.5; desktop already used system Lua 5.4.8 with all 44 tests passing. This task aligned all targets to 5.4.8 and removed the now-dead compat code.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Upgrade WASM and ESP32 FetchContent from Lua 5.1.5 to 5.4.8 | 64e3894 | CMakeLists.txt |
| 2 | Remove Lua 5.1 compat shims and collapse version guards | 7b347a5 | lua_platform.hpp, lua_platform.cpp, bindings_async.cpp, bindings_tween.cpp |

## Changes Made

### Task 1 — CMakeLists.txt

- EMSCRIPTEN branch: `lua51` -> `lua54`, URL `lua-5.1.5.tar.gz` -> `lua-5.4.8.tar.gz`, lib `lua51_wasm` -> `lua54_wasm`, variable `LUA51_SOURCES` -> `LUA54_SOURCES`
- ESP32 branch: same renames, `lua51_esp32` -> `lua54_esp32`
- Desktop error message: `liblua5.1-dev` -> `liblua5.4-dev`
- WASM comment reference updated: `lua51_wasm` -> `lua54_wasm`

### Task 2 — Compat cleanup

**lua_platform.hpp:** Removed 24-line `#if LUA_VERSION_NUM < 502` block that provided `LUA_OK`, `lua_pcallk`, and `luaL_testudata` — all native in Lua 5.4. Updated include comments to say "Lua 5.4 for desktop" and "Lua 5.4.8 for ESP32".

**lua_platform.cpp:** Replaced multi-line Lua 5.1 rationale comment in `openEmbeddedLibraries` with a single accurate comment.

**bindings_async.cpp:** Removed 15-line `lua_isyieldable` compat block (Lua 5.3+ / LuaJIT compat). Collapsed 7-line `#if LUA_VERSION_NUM >= 504` lua_resume guard to 3-line unconditional form.

**bindings_tween.cpp:** Same removals as bindings_async.cpp.

## Verification

```
grep -c "lua-5\.1\|lua51" CMakeLists.txt          => 0  (pass)
grep -c "LUA_VERSION_NUM < 502" lua_platform.hpp  => 0  (pass)
grep -c "LUA_VERSION_NUM >= 504" bindings_async.cpp bindings_tween.cpp => 0 0  (pass)
ctest: 100% tests passed, 0 tests failed out of 44  (pass)
```

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check

### Files exist:
- [x] CMakeLists.txt — modified
- [x] include/enjin2/scripting/lua_platform.hpp — modified
- [x] src/scripting/lua_platform.cpp — modified
- [x] src/scripting/bindings_async.cpp — modified
- [x] src/scripting/bindings_tween.cpp — modified

### Commits exist:
- [x] 64e3894 — Task 1 (CMakeLists.txt FetchContent upgrade)
- [x] 7b347a5 — Task 2 (compat shim removal)

## Self-Check: PASSED
