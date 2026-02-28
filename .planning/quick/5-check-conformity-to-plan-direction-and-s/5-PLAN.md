---
phase: quick-05
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - include/enjin2/scripting/bindings.hpp
  - src/scripting/bindings.cpp
autonomous: true
requirements: []

must_haves:
  truths:
    - "No std::string or std::vector appears in the LuaBindings class definition or any binding function in bindings.hpp"
    - "lua_print uses printf, not std::cout — consistent with the Phase 31-02 printf-only policy"
    - "FontEntry.name and currentFontName use const char* or fixed-size char arrays, not std::string"
    - "The global Lua distance() function accepts float coordinates, consistent with Vec2::distance"
    - "The old global time() Lua function (std::chrono-based) is removed — engine.time.now() is the canonical API"
  artifacts:
    - path: "include/enjin2/scripting/bindings.hpp"
      provides: "LuaBindings class with zero std::string members"
      contains: "FontEntry"
    - path: "src/scripting/bindings.cpp"
      provides: "printf-only output, no std::cout, no std::chrono in lua_time"
  key_links:
    - from: "LuaBindings::registerFont"
      to: "fontRegistry"
      via: "const char* name parameter + strncpy into fixed char array"
      pattern: "const char\\* name"
    - from: "lua_print"
      to: "stdout"
      via: "printf (not std::cout)"
      pattern: "printf"
---

<objective>
Fix three conformity violations introduced in the text-rendering and math binding commits (0470058 and b412e3e), restoring alignment with the zero-heap-allocation constraint and the Phase 31-02 printf-only decision.

Purpose: enjin2's core constraint is zero dynamic allocation. The text-rendering commit introduced `std::string` fields inside `LuaBindings` (FontEntry.name, currentFontName, registerFont parameter). The pre-existing `lua_print` uses `std::cout`, which violates the explicit Phase 31-02 decision to use printf exclusively for embedded-target compatibility. The global `distance()` Lua function uses int16_t coordinates — inconsistent with the float-based Vec2 API added in the same commit. The redundant global `time()` using std::chrono is superseded by `engine.time.now()`.

Output: Corrected `bindings.hpp` and `bindings.cpp` with no std::string in LuaBindings members, printf-only output, and a float-based global distance.
</objective>

<execution_context>
@/home/unwn/.claude/get-shit-done/workflows/execute-plan.md
@/home/unwn/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/STATE.md

Key decisions to enforce:
- Phase 31-02: "engine.log uses printf exclusively (not std::cout) — embedded target compatibility with ESP32/Emscripten"
- Core constraint: "No dynamic allocation (static arrays, no heap)" — std::string triggers heap allocation beyond SSO threshold
- Phase 28: float dt — the engine uses float throughout; the distance global should be consistent
</context>

<tasks>

<task type="auto">
  <name>Task 1: Replace std::string members in LuaBindings with const char* / fixed char arrays</name>
  <files>include/enjin2/scripting/bindings.hpp</files>
  <action>
In `include/enjin2/scripting/bindings.hpp`:

1. Remove `#include <string>` from the header (it was added by the text commit).

2. In the `FontEntry` struct, change `std::string name` to `char name[32]` (32 chars is sufficient for font names like "default8").

3. Change `std::string currentFontName{"default"}` to `char currentFontName[32]{"default"}`.

4. Change `registerFont(const std::string& name, const GFXfont* font)` signature to `registerFont(const char* name, const GFXfont* font)`.

5. The `registerTable` private helper takes `const std::string& tableName` and `const std::vector<std::pair<std::string, lua_CFunction>>& functions` — this method is pre-existing and used only once (for the stub "love" table). Change it to a direct inline table-building pattern in bindings.cpp (remove the method declaration from the header and the implementation from bindings.cpp, replacing the single call-site with direct lua_newtable/lua_pushcfunction/lua_setfield sequence). This eliminates the `#include <vector>` and the `std::string` in the helper signature. Note: `#include <vector>` may not be explicitly present in the header because `<string>` was pulling it in transitively — after removing `<string>`, verify the header compiles clean.

These changes have no impact on the Lua API surface. `FontEntry.name` needs `<cstring>` for `strncpy` — this is already included via `types.hpp` chain or add directly.
  </action>
  <verify>
grep -n "std::string\|std::vector\|#include <string>\|#include <vector>" include/enjin2/scripting/bindings.hpp
# Should produce zero output (no matches)
  </verify>
  <done>bindings.hpp contains no std::string, no std::vector, no #include string/vector. FontEntry.name is char[32]. registerFont takes const char*. registerTable method is removed.</done>
</task>

<task type="auto">
  <name>Task 2: Fix bindings.cpp — printf for lua_print, const char* for registerFont, remove lua_time, float for lua_math_distance</name>
  <files>src/scripting/bindings.cpp</files>
  <action>
In `src/scripting/bindings.cpp`:

**2a. Remove #include <iostream> and fix lua_print (lines ~758-773):**
Remove `#include <iostream>` from the top of the file.
Replace the `lua_print` body with printf equivalents:
```cpp
int LuaBindings::lua_print(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            printf("%s", s);
        } else {
            printf("(%s)", lua_typename(L, lua_type(L, i)));
        }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}
```
This is now consistent with `lua_engine_log` and the printf-only policy (Phase 31-02 decision).

**2b. Remove lua_time and its registration:**
The global `time()` function uses `std::chrono::steady_clock` which is problematic on ESP32 and is superseded by `engine.time.now()` (totalTime) and `engine.time.delta()`. Remove:
- `#include <chrono>` from the top of the file
- The `lua_time` function body (~lines 775-782)
- The line `engine->registerFunction("time", lua_time);` in `registerAll()` (~line 358)
Note: Also remove the `static int lua_time(lua_State* L);` declaration from bindings.hpp if it is declared there (check — it appears to be inline in the .cpp, so just remove the function and the registration line).

**2c. Fix registerFont to use const char* and strncpy:**
Change `registerFont(const std::string& name, ...)` to `registerFont(const char* name, ...)`.
Replace string comparisons `fontRegistry[i].name == name` with `strcmp(fontRegistry[i].name, name) == 0`.
Replace string assignments `fontRegistry[fontCount].name = name` with `strncpy(fontRegistry[fontCount].name, name, 31); fontRegistry[fontCount].name[31] = '\0';`.
Fix the `registerAll()` call: `registerFont("default8", &defaultFont8pt7b)` — already uses string literal so no change needed.
Fix `currentFontName = "default"` in resetSpritePool() to `strncpy(currentFontName, "default", 31); currentFontName[31] = '\0';`.
Fix `lua_setFont`: the name comparison `fontRegistry[i].name == name` becomes `strcmp(fontRegistry[i].name, b->currentFontName) == 0` (check the actual logic at ~line 1225-1231 and convert all string ops to strcmp/strncpy).
Fix `lua_getFont`: `lua_pushlstring(L, b->currentFontName.data(), b->currentFontName.size())` becomes `lua_pushstring(L, b->currentFontName)`.

**2d. Fix registerTable removal:**
Remove the `LuaBindings::registerTable` function body. Replace the single call-site in `registerAll()`:
```cpp
registerTable("love", {
    {"draw", lua_rectangle},
});
```
With direct table construction:
```cpp
lua_newtable(L);
lua_pushcfunction(L, lua_rectangle);
lua_setfield(L, -2, "draw");
lua_setglobal(L, "love");
```

**2e. Fix lua_math_distance to use float coordinates:**
Change `lua_math_distance` from int16_t to float, using `Vec2::distance` or inline float math:
```cpp
int LuaBindings::lua_math_distance(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float dx = x2 - x1;
    float dy = y2 - y1;
    lua_pushnumber(L, std::sqrt(dx * dx + dy * dy));
    return 1;
}
```
This aligns with the Vec2::distance static method and the float-first engine policy.
  </action>
  <verify>
grep -n "std::cout\|std::endl\|#include <iostream>\|#include <chrono>\|std::string\|std::vector" src/scripting/bindings.cpp
# Should produce zero output
  </verify>
  <done>
- bindings.cpp has no std::cout, no #include iostream, no #include chrono, no std::string, no std::vector
- lua_print uses printf loop
- lua_time and its registration are removed
- registerFont uses const char* with strcmp/strncpy
- registerTable function removed, love table built inline
- lua_math_distance uses float coordinates and returns float
- Build still compiles: cmake --build build 2>&1 | tail -20 shows no errors
  </done>
</task>

</tasks>

<verification>
After both tasks:

1. Zero std:: heap-allocating types in LuaBindings:
   grep -rn "std::string\|std::vector" include/enjin2/scripting/bindings.hpp src/scripting/bindings.cpp
   # Expected: no matches

2. printf-only output policy confirmed:
   grep -n "std::cout\|std::endl\|iostream" src/scripting/bindings.cpp
   # Expected: no matches

3. Build compiles clean:
   cmake --build build --target enjin2_sdl 2>&1 | grep -E "error:|warning:" | grep -v "third.party\|deprecated" | head -20
   # Expected: no errors

4. Existing tests pass:
   cd build && ctest -R "engine_table\|math_binding\|collision_test" --output-on-failure 2>&1 | tail -30
   # Expected: all pass
</verification>

<success_criteria>
- bindings.hpp: no std::string, no std::vector, no #include string/vector; FontEntry.name is char[32]; registerFont takes const char*
- bindings.cpp: no std::cout, no iostream, no chrono; lua_print uses printf; lua_time removed; registerFont uses strcmp/strncpy; registerTable helper removed; lua_math_distance uses float
- All existing tests pass (engine_table_test, math_binding_test, collision_test)
- The Lua API surface is unchanged from a script's perspective (font names still work as strings in Lua)
</success_criteria>

<output>
After completion, create `.planning/quick/5-check-conformity-to-plan-direction-and-s/5-SUMMARY.md` with:
- What was fixed and why (which constraints each fix restores)
- Files changed
- Any remaining pre-existing issues not in scope (e.g. LuaScriptSystem using std::string in executeScript/loadScript — those are higher-level glue, not in the zero-alloc hot path)
</output>
