---
phase: quick
plan: 8
type: execute
wave: 1
depends_on: []
files_modified:
  - CMakeLists.txt
  - include/enjin2/scripting/lua_platform.hpp
  - src/scripting/lua_platform.cpp
  - src/scripting/bindings_async.cpp
  - src/scripting/bindings_tween.cpp
autonomous: true
requirements: []
must_haves:
  truths:
    - "WASM and ESP32 FetchContent targets download Lua 5.4.8 (not 5.1.5)"
    - "Lua 5.1 compat shims are gone from lua_platform.hpp"
    - "bindings_async and bindings_tween use unconditional Lua 5.4 lua_resume signature"
    - "All 44 desktop tests pass after changes"
  artifacts:
    - path: "CMakeLists.txt"
      provides: "Lua 5.4.8 FetchContent for WASM and ESP32"
      contains: "lua-5.4.8.tar.gz"
    - path: "include/enjin2/scripting/lua_platform.hpp"
      provides: "Clean Lua 5.4 platform header with no 5.1 compat block"
    - path: "src/scripting/bindings_async.cpp"
      provides: "Unconditional 5.4 lua_resume(co, L, 0, &nres) call"
    - path: "src/scripting/bindings_tween.cpp"
      provides: "Unconditional 5.4 lua_resume(co, L, 0, &nres) call"
  key_links:
    - from: "CMakeLists.txt EMSCRIPTEN/ESP32 blocks"
      to: "Lua 5.4.8 source tarball"
      via: "FetchContent URL"
    - from: "lua_platform.hpp"
      to: "lua.h / lauxlib.h / lualib.h"
      via: "extern C include (no compat shims needed)"
---

<objective>
Upgrade the embedded Lua version (WASM and ESP32 FetchContent targets) from 5.1.5 to 5.4.8, remove all now-dead Lua 5.1 compatibility shims from the codebase, and collapse conditional API guards to the unconditional Lua 5.4 form.

Purpose: The desktop build already uses system Lua 5.4.8 (all 44 tests pass). WASM and ESP32 still fetch Lua 5.1.5 — they need to match. Compat shims and version guards are now dead code that adds noise and maintenance risk.

Output: Unified Lua 5.4.8 across all three targets; clean source with no 5.1 vestiges.
</objective>

<execution_context>
@/home/unwn/.claude/get-shit-done/workflows/execute-plan.md
@/home/unwn/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md

Current Lua situation (verified by investigation):
- Desktop: `find_package(Lua QUIET)` finds system Lua 5.4.8 — builds clean, 44/44 tests pass already
- WASM: FetchContent fetches `lua-5.1.5.tar.gz`, builds `lua51_wasm` static lib
- ESP32: FetchContent fetches `lua-5.1.5.tar.gz`, builds `lua51_esp32` static lib
- Lua 5.4 API differences already handled via `#if LUA_VERSION_NUM >= 504` guards in bindings_async.cpp and bindings_tween.cpp
- `lua_platform.hpp` has a compat block for `LUA_OK`, `lua_pcallk`, `luaL_testudata` guarded by `LUA_VERSION_NUM < 502` — all now native in 5.4
- GC API: `LUA_GCSETPAUSE` and `LUA_GCSETSTEPMUL` still exist in Lua 5.4 (incremental mode) — no changes needed there
- `lua_resume` signature in Lua 5.4: `lua_resume(L, from, narg, nres)` — 4th arg is `int*` for result count

Lua 5.4 breaking changes relevant to this codebase:
1. `lua_resume` has 4th parameter `int *nresults` — already guarded with `#if LUA_VERSION_NUM >= 504` in both async and tween bindings
2. `luaL_testudata` — native in 5.4, was emulated in compat block
3. `LUA_OK` — native in 5.4, was emulated in compat block
4. `lua_pcallk` — native in 5.4, was emulated in compat block
5. Integer subtype (`lua_Integer`) — not used directly, no impact
6. `string.gmatch`/`unpack` — not used in C bindings, no impact
</context>

<tasks>

<task type="auto">
  <name>Task 1: Upgrade WASM and ESP32 FetchContent from Lua 5.1.5 to 5.4.8</name>
  <files>CMakeLists.txt</files>
  <action>
In CMakeLists.txt, update both the EMSCRIPTEN and ESP32 branches of the Lua FetchContent block:

1. EMSCRIPTEN branch (lines ~122-141):
   - Change `URL https://www.lua.org/ftp/lua-5.1.5.tar.gz` to `URL https://www.lua.org/ftp/lua-5.4.8.tar.gz`
   - Rename FetchContent name from `lua51` to `lua54`
   - Rename `lua51_wasm` target to `lua54_wasm`
   - Update `lua51_SOURCE_DIR` to `lua54_SOURCE_DIR`
   - Update GLOB from `"${lua54_SOURCE_DIR}/src/l*.c"` — same pattern works for 5.4
   - Update `LUA51_SOURCES` variable name to `LUA54_SOURCES`
   - Update comment: "Building Lua 5.4.8 from source for WebAssembly (LuaJIT is x86/ARM only)"

2. ESP32 branch (lines ~143-159):
   - Same renames: lua51 -> lua54, lua51_esp32 -> lua54_esp32, lua51_SOURCE_DIR -> lua54_SOURCE_DIR
   - Change URL to `https://www.lua.org/ftp/lua-5.4.8.tar.gz`
   - Update comment: "Building Lua 5.4.8 from source for ESP32"

3. Desktop error message (line ~170):
   - Change `apt-get install liblua5.1-dev` to `apt-get install liblua5.4-dev`
   - Change `brew install lua` remains fine (installs latest Lua)

4. Comment at line ~273:
   - Update "PUBLIC on lua51_wasm" to "PUBLIC on lua54_wasm"

Lua 5.4 uses the same source file glob pattern (`l*.c`) so the GLOB line does not need changing beyond the variable rename.
  </action>
  <verify>
    <automated>grep -n "lua-5.4.8\|lua54\|liblua5.4-dev" /home/unwn/git/enjin/CMakeLists.txt | wc -l</automated>
  </verify>
  <done>CMakeLists.txt contains lua-5.4.8.tar.gz in both EMSCRIPTEN and ESP32 branches; no lua51 or lua-5.1.5 references remain in the build definition (only in build/ artifact directories)</done>
</task>

<task type="auto">
  <name>Task 2: Remove Lua 5.1 compat shims and collapse version guards</name>
  <files>
    include/enjin2/scripting/lua_platform.hpp,
    src/scripting/lua_platform.cpp,
    src/scripting/bindings_async.cpp,
    src/scripting/bindings_tween.cpp
  </files>
  <action>
**include/enjin2/scripting/lua_platform.hpp:**

1. Remove the entire compat block (lines 33-56):
   ```
   // Lua 5.1 compat: LUA_OK, lua_pcallk, and luaL_testudata are absent in standard
   // Lua 5.1 ...
   #if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM < 502 && !defined(LUAJIT_VERSION)
   ...
   #endif
   ```
   All three (`LUA_OK`, `lua_pcallk`, `luaL_testudata`) are native in Lua 5.4. Remove the block entirely.

2. Update the platform comments for the VCV_RACK include block:
   - Change `// LuaJIT for VCV Rack (desktop)` to `// Lua 5.4 for desktop (system package)`

3. Update the ESP32 include comment:
   - Change `// Standard Lua 5.1.5 for ESP32 (LuaJIT requires native CPU architecture)` to `// Lua 5.4.8 for ESP32 (built from source via FetchContent)`

**src/scripting/lua_platform.cpp:**

Update comments in `openEmbeddedLibraries`:
- Remove the comment block that says "Lua 5.1 (used on ESP32) does not have luaL_requiref or per-library open functions..." and replace with: `// Open all standard Lua libraries; io/os/debug are restricted by configureSecurityRestrictions() below.`

**src/scripting/bindings_async.cpp:**

1. Remove the lua_isyieldable compat block (lines 20-34):
   ```cpp
   // lua_isyieldable compat guard: available in Lua 5.3+ and LuaJIT 2.1+
   #if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM < 503
       #ifndef lua_isyieldable
       ...
       #endif
   #endif
   ```
   `lua_isyieldable` is native in Lua 5.4.

2. Collapse the `lua_resume` version guard (~lines 203-212). Replace:
   ```cpp
   int status;
   #if LUA_VERSION_NUM >= 504
       int nres = 0;
       status = lua_resume(co, L, 0, &nres);
       if (nres > 0) lua_pop(co, nres);
   #else
       status = lua_resume(co, 0);
       if (lua_gettop(co) > 0) lua_settop(co, 0);
   #endif
   ```
   With:
   ```cpp
   int nres = 0;
   int status = lua_resume(co, L, 0, &nres);
   if (nres > 0) lua_pop(co, nres);
   ```

**src/scripting/bindings_tween.cpp:**

1. Remove the lua_isyieldable compat block (lines 22-33) — same pattern as above.

2. Collapse the `lua_resume` version guard (~lines 259-267). Replace with same unconditional form:
   ```cpp
   int nres = 0;
   int status = lua_resume(co, L, 0, &nres);
   if (nres > 0) lua_pop(co, nres);
   ```
  </action>
  <verify>
    <automated>cd /home/unwn/git/enjin/build/desktop-lua54 && cmake --build . -j$(nproc) 2>&1 | tail -3 && ctest --output-on-failure 2>&1 | tail -5</automated>
  </verify>
  <done>No lua_isyieldable compat, no LUA_VERSION_NUM < 502 compat block, no #if LUA_VERSION_NUM >= 504 guards in async/tween. Build succeeds. 44/44 tests pass.</done>
</task>

</tasks>

<verification>
After both tasks:

1. No Lua 5.1 URL or target name remains in CMakeLists.txt (search lua-5.1, lua51):
   `grep -c "lua-5\.1\|lua51" CMakeLists.txt` should return 0

2. No compat shim block in lua_platform.hpp:
   `grep -c "LUA_VERSION_NUM < 502" include/enjin2/scripting/lua_platform.hpp` should return 0

3. No version-conditional lua_resume in async/tween:
   `grep -c "LUA_VERSION_NUM >= 504" src/scripting/bindings_async.cpp src/scripting/bindings_tween.cpp` should return 0

4. Desktop build + tests:
   `cd build/desktop-lua54 && cmake --build . -j$(nproc) && ctest` — 44/44 pass
</verification>

<success_criteria>
- CMakeLists.txt WASM and ESP32 branches fetch lua-5.4.8.tar.gz
- lua_platform.hpp contains no Lua 5.1 compat block
- bindings_async.cpp and bindings_tween.cpp use unconditional Lua 5.4 lua_resume API
- No lua51 target names remain in CMakeLists.txt
- 44/44 desktop tests pass
</success_criteria>

<output>
After completion, create `.planning/quick/8-move-to-lua-5-4-and-check-all-impacted-a/8-SUMMARY.md`
</output>
