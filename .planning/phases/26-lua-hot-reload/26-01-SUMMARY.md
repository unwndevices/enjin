---
phase: 26-lua-hot-reload
plan: 01
subsystem: scripting
tags: [lua, sdl3, hot-reload, game-loop, state-machine]

# Dependency graph
requires:
  - phase: 25-multi-layer-canvas-composition
    provides: LuaCanvas layer wrappers + setLayers() API in sdl_main.cpp
provides:
  - F5 hot-reload for SDL3 Lua runner with full Lua state reset
  - LuaCallback dangling-pointer bug eliminated
  - resetSpritePool() method on LuaBindings with auto-call on registerAll()
  - performReload() function encapsulating full Lua lifecycle
  - --script CLI flag for selecting Lua script at startup
  - lua_ok gate pattern for error-recovery in the game loop
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "performReload() encapsulates full Lua lifecycle: shutdown + initialize + setLayers + setInput + loadScript"
    - "lua_ok boolean gate: false = paused state (error recovery), true = normal update/draw"
    - "!event.key.repeat guard on SDL_EVENT_KEY_DOWN to filter key auto-repeat"
    - "Initial startup failure identical to reload failure: window stays open, loop pauses, F5 retries"

key-files:
  created: []
  modified:
    - src/scripting/lua_engine.cpp
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - src/platform/sdl/sdl_main.cpp

key-decisions:
  - "LuaCallback overload neutered to no-op (not removed) to preserve ABI; all bindings use lua_CFunction exclusively"
  - "resetSpritePool() called from registerAll() so every reload starts with a clean pool"
  - "Drawing state (currentColor=15, lineWidth=1) also reset in registerAll() to prevent leakage"
  - "performReload() does NOT call registerAll() explicitly — initialize() already calls it via LuaScriptSystem"
  - "Single [reload error] prefix for both syntax and runtime errors; Lua error string already contains file:line:message detail"
  - "Script path stored as std::string local in main() scope (accessible throughout, inside no extra guard)"

patterns-established:
  - "performReload pattern: shutdown then initialize then wire bindings then loadScript — always the same path"
  - "lua_ok gate pattern: Lua update/draw wrapped in if(lua_ok) block; errors set lua_ok=false"

requirements-completed: [HOT-01, HOT-02, HOT-03]

# Metrics
duration: 3min
completed: 2026-02-26
---

# Phase 26 Plan 01: Lua Hot-Reload Summary

**F5 hot-reload in SDL3 runner with LuaCallback dangling-pointer fix, sprite pool reset, and paused-loop error recovery**

## Performance

- **Duration:** ~3 min
- **Started:** 2026-02-26T11:43:32Z
- **Completed:** 2026-02-26T11:46:52Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Eliminated LuaCallback dangling-pointer UB by neutering the overload body to a no-op
- Added `resetSpritePool()` to `LuaBindings` and called it (plus drawing state reset) at the top of `registerAll()` so every reload starts clean
- Implemented `performReload()` as a single function encapsulating the full Lua lifecycle (shutdown + initialize + setLayers + setInput + loadScript)
- Added F5 detection in SDL event pump with `!event.key.repeat` guard; initial startup and F5 reload share identical code path
- Game loop `lua_ok` flag gates `update`/`draw` calls; any error (reload or runtime) transitions to paused state; F5 retries
- Added `--script path` CLI flag (default: `scripts/layer_demo.lua`)

## Task Commits

Each task was committed atomically:

1. **Task 1: Fix LuaCallback dangling pointer + add sprite pool reset** - `e536cfb` (fix)
2. **Task 2: Implement F5 hot-reload in SDL3 runner** - `9c428cc` (feat)

**Plan metadata:** (docs commit below)

## Files Created/Modified
- `src/scripting/lua_engine.cpp` - LuaCallback overload neutered to no-op with explanatory comment
- `include/enjin2/scripting/bindings.hpp` - Added public `resetSpritePool()` declaration
- `src/scripting/bindings.cpp` - Implemented `resetSpritePool()`, added reset calls at top of `registerAll()`
- `src/platform/sdl/sdl_main.cpp` - `performReload()`, `lua_ok` flag, F5 handler, `--script` arg, gated update/draw, `<string>` include

## Decisions Made
- LuaCallback overload neutered to no-op (not removed): keeps ABI compatible while eliminating UB. All bindings confirmed to use `lua_CFunction` exclusively.
- `resetSpritePool()` placed in `registerAll()` rather than a separate `reset()` call: lowest-friction approach that keeps `performReload()` simple.
- `performReload()` does not call `registerAll()` explicitly: `LuaScriptSystem::initialize()` already calls `bindings.registerAll()` internally.
- `lua_ok` is a local variable in `main()` inside `#ifdef ENJIN2_BUILD_LUA` (not a static global): keeps it scoped naturally to the Lua build path.
- Single `[reload error]` prefix for both syntax and runtime errors on reload: Lua error strings already contain `file:line:message` detail that distinguishes them.

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None — the target `layer_binding_test` was named differently in the CMake build system (`cmake --build build --target layer_binding_test` required `cmake --build build` to discover the target, then a direct `./build/tests/layer_binding_test` run). No functional issue; all 18 tests passed on first run.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness
- Phase 26 Plan 01 is the only plan in the phase; phase is now complete.
- Hot-reload is fully operational: `./build/bin/enjin2_sdl --script scripts/layer_demo.lua` then F5 to reload.
- All v1.4 milestone work is now complete (Phase 24, 25, 26 all done).

---
*Phase: 26-lua-hot-reload*
*Completed: 2026-02-26*

## Self-Check: PASSED

- FOUND: .planning/phases/26-lua-hot-reload/26-01-SUMMARY.md
- FOUND: src/platform/sdl/sdl_main.cpp
- FOUND: src/scripting/bindings.cpp
- FOUND commit: e536cfb (fix LuaCallback dangling pointer + add sprite pool reset)
- FOUND commit: 9c428cc (implement F5 hot-reload in SDL3 runner)
