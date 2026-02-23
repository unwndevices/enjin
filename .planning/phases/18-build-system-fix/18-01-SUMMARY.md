---
phase: 18-build-system-fix
plan: 01
subsystem: infra
tags: [cmake, emscripten, wasm, lua, conditional-compilation, generator-expressions]

# Dependency graph
requires: []
provides:
  - CMakeLists.txt WASM block with conditional Lua linking via generator expressions
  - emscripten_bindings.cpp with #ifdef ENJIN2_BUILD_LUA guards on all Lua code
affects: [wasm-build, lua-build]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CMake generator expressions for conditional target linking: $<$<BOOL:${VAR}>:target>"
    - "C++ preprocessor guards matching CMake compile definitions"

key-files:
  created: []
  modified:
    - CMakeLists.txt
    - src/bindings/emscripten_bindings.cpp

key-decisions:
  - "Use CMake generator expressions (not if() blocks) for WASM conditional Lua linking — consistent with line 169 pattern already present in the enjin2 interface target"
  - "Inject ENJIN2_BUILD_LUA=1 as a compile definition from CMake so the C++ source can detect the flag at compile time"
  - "Two #ifdef blocks inside EMSCRIPTEN_BINDINGS: early block for Lua type bindings, late block for Lua factories and helpers — keeps non-Lua Canvas4/Pixel4 bindings cleanly between them"

patterns-established:
  - "CMake conditional linking pattern: $<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua> matches existing enjin2 interface target usage"
  - "C++ guard pattern: #ifdef ENJIN2_BUILD_LUA / #endif wraps Lua headers, function definitions, and EMSCRIPTEN_BINDINGS sections"

requirements-completed: [BLDS-01]

# Metrics
duration: 2min
completed: 2026-02-23
---

# Phase 18 Plan 01: Build System Fix Summary

**WASM build made Lua-optional: CMake generator expressions guard enjin2_lua and luajit/src in WASM block, with matching #ifdef ENJIN2_BUILD_LUA preprocessor guards in emscripten_bindings.cpp**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-23T16:06:45Z
- **Completed:** 2026-02-23T16:08:46Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- CMakeLists.txt WASM block: `luajit/src` include, `ENJIN2_BUILD_LUA=1` compile definition, and `enjin2_lua` link target are all conditional via `$<$<BOOL:${ENJIN2_BUILD_LUA}>:...>` generator expressions
- emscripten_bindings.cpp: 6 `#ifdef ENJIN2_BUILD_LUA` guards wrap all Lua headers, `forceSymbolLinking()`, LuaResult/LuaEngine/LuaCanvas/LuaBindings/LuaScriptSystem bindings, factory functions, and debug helpers
- Non-Lua WASM bindings (Pixel4, Canvas4, testFunction, getCanvasData, setCanvasData, fastFillRect, fastDrawLine, drawPixelsBatch, drawLinesBatch, fillRectsBatch) remain unconditionally active

## Task Commits

Each task was committed atomically:

1. **Task 1: Guard WASM block Lua dependencies in CMakeLists.txt** - `52feba1` (fix)
2. **Task 2: Guard Lua code in emscripten_bindings.cpp with preprocessor directives** - `743efe0` (fix)

**Plan metadata:** (see final commit)

## Files Created/Modified
- `/home/unwn/dev/enjin/CMakeLists.txt` - WASM block lines 189-200: added generator expression guards for luajit/src include, ENJIN2_BUILD_LUA=1 compile definition, and enjin2_lua link target
- `/home/unwn/dev/enjin/src/bindings/emscripten_bindings.cpp` - 6 #ifdef ENJIN2_BUILD_LUA guards added; Lua includes, forceSymbolLinking, type bindings, factories, and helpers all guarded

## Decisions Made
- Used CMake generator expressions rather than `if(ENJIN2_BUILD_LUA)` blocks inside the WASM section — consistent with the existing pattern at line 169 where the `enjin2` interface target already uses `$<$<BOOL:${ENJIN2_BUILD_LUA}>:enjin2_lua>`
- Added `target_compile_definitions` with the `ENJIN2_BUILD_LUA=1` macro so C++ source can detect Lua availability at compile time without a separate header
- Split EMSCRIPTEN_BINDINGS Lua guards into two blocks to keep Canvas4/Pixel4 non-Lua bindings cleanly in the middle section

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Both files are ready for WASM+LUA=OFF builds once the Emscripten toolchain is available
- Full build verification (CMake configure + compile) requires Emscripten; code inspection confirms correctness
- The WASM+LUA=ON path is unchanged in structure

## Self-Check: PASSED

- FOUND: CMakeLists.txt
- FOUND: src/bindings/emscripten_bindings.cpp
- FOUND: .planning/phases/18-build-system-fix/18-01-SUMMARY.md
- FOUND commit: 52feba1
- FOUND commit: 743efe0

---
*Phase: 18-build-system-fix*
*Completed: 2026-02-23*
