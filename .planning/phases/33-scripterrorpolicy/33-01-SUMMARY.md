---
phase: 33-scripterrorpolicy
plan: 01
subsystem: scripting/components
tags: [lua, cmake, compilation, structural-fix]
dependency_graph:
  requires: []
  provides: [lua_script.cpp compiled into enjin2_lua, C_LuaScript linkable]
  affects: [enjin2_lua, CMakeLists.txt, include/enjin2/components/lua_script.hpp, include/enjin2/scripting/bindings.hpp]
tech_stack:
  added: []
  patterns: [LuaScriptSystem/LuaCanvas replace IScriptInterpreter/IScriptGraphics, ICanvas<Pixel4>* constructor on LuaCanvas for abstract canvas access]
key_files:
  created: []
  modified:
    - include/enjin2/components/lua_script.hpp
    - include/enjin2/scripting/bindings.hpp
    - src/components/lua_script.cpp
    - CMakeLists.txt
decisions:
  - "lua_script.hpp private members reconciled to LuaScriptSystem/LuaCanvas to match .cpp — IScriptInterpreter/IScriptGraphics removed entirely"
  - "LuaCanvas gains ICanvas<Pixel4>* constructor — C_LuaScript::draw() receives abstract interface; template constructors cannot match it"
  - "GetWidth()/GetHeight() (PascalCase) corrected from getWidth()/getHeight() — C_Drawable follows PascalCase accessor convention"
metrics:
  duration: "2 minutes"
  completed_date: "2026-02-27"
  tasks_completed: 2
  files_modified: 4
---

# Phase 33 Plan 01: Structural Fix — lua_script.cpp Compilation Summary

lua_script.cpp added to enjin2_lua with header/impl type mismatch fixed and two compilation bugs corrected; all 8 existing Lua tests pass.

## Objective

Fix the structural blockers preventing lua_script.cpp from compiling: the file was orphaned from CMake targets, and lua_script.hpp declared fields using IScriptInterpreter/IScriptGraphics while the .cpp used LuaScriptSystem/LuaCanvas.

## Tasks Completed

| # | Task | Commit | Files Changed |
|---|------|--------|---------------|
| 1 | Reconcile lua_script.hpp to match lua_script.cpp | 9189c23 | include/enjin2/components/lua_script.hpp |
| 2 | Add lua_script.cpp to enjin2_lua + verify clean build | d49211e | CMakeLists.txt, bindings.hpp, lua_script.cpp |

## Decisions Made

1. **LuaScriptSystem/LuaCanvas replace IScriptInterpreter/IScriptGraphics** — lua_script.hpp private section fully aligned to lua_script.cpp actual usage. Second constructor (ScriptFactory::InterpreterType), getInterpreter(), getGraphics(), getInterpreterType(), and initializeInterpreter() all removed. handleScriptError parameter corrected from ScriptResult to LuaResult.

2. **LuaCanvas(ICanvas<Pixel4>*) constructor added** — C_LuaScript::draw() receives ICanvas<Pixel4>& (abstract interface), which the template Canvas4<W,H>* constructors cannot match. The new constructor reads width/height from the interface's virtual getters and stores the void* — consistent with how bindings.cpp already casts canvasPtr to ICanvas<Pixel4>* when is4Bit is true.

3. **GetWidth()/GetHeight() corrected** — C_Drawable uses PascalCase accessors. lua_script.cpp called getWidth()/getHeight() (camelCase) which do not exist as members — would compile only if ICanvas methods were visible, but in initializeScriptSystem() there is no canvas argument, so this was always calling into thin air.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed getWidth()/getHeight() casing mismatch in lua_script.cpp**
- Found during: Task 2 (first build attempt)
- Issue: lua_script.cpp:47-48 called `getWidth()`/`getHeight()` but C_Drawable exposes `GetWidth()`/`GetHeight()` (PascalCase)
- Fix: Corrected to `GetWidth()`/`GetHeight()` in initializeScriptSystem()
- Files modified: src/components/lua_script.cpp
- Commit: d49211e

**2. [Rule 1 - Bug] Fixed setupLuaCanvas() type mismatch — LuaCanvas has no ICanvas<Pixel4>* constructor**
- Found during: Task 2 (first build attempt)
- Issue: C_LuaScript::draw() receives ICanvas<Pixel4>&; setupLuaCanvas() template tried new LuaCanvas(&canvas) but LuaCanvas only had Canvas4<W,H>* and Canvas8<W,H>* template constructors
- Fix: Added explicit LuaCanvas(ICanvas<Pixel4>*) constructor to bindings.hpp; updated setupLuaCanvas() to cast through ICanvas<Pixel4>*
- Files modified: include/enjin2/scripting/bindings.hpp, src/components/lua_script.cpp
- Commit: d49211e

## Verification Results

```
grep "lua_script.cpp" CMakeLists.txt        -> src/components/lua_script.cpp  PASS
grep IScriptInterpreter lua_script.hpp      -> (no output)                     PASS
cmake --build build | grep "error:"        -> (no output)                     PASS
ctest -R "layer_binding|hot_reload|..."    -> 8/8 passed                      PASS
```

## Self-Check: PASSED

- [x] include/enjin2/components/lua_script.hpp — exists, LuaScriptSystem/LuaCanvas present
- [x] include/enjin2/scripting/bindings.hpp — ICanvas<Pixel4>* constructor added
- [x] src/components/lua_script.cpp — GetWidth/GetHeight corrected
- [x] CMakeLists.txt — src/components/lua_script.cpp in enjin2_lua target_sources
- [x] Commit 9189c23 — reconcile lua_script.hpp
- [x] Commit d49211e — CMakeLists + bug fixes
