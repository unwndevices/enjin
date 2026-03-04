---
phase: quick-9
plan: 9
type: execute
wave: 1
depends_on: []
files_modified:
  - examples/arduino_lcd_demo/lib/lua54/library.json
  - examples/arduino_lcd_demo/lib/lua51/ (deleted)
  - examples/esp32_idf_example/CMakeLists.txt
  - examples/esp32_lcd_demo/CMakeLists.txt
autonomous: true
requirements: [QUICK-9]

must_haves:
  truths:
    - "arduino_lcd_demo has lib/lua54/ containing Lua 5.4.8 .c/.h sources"
    - "lib/lua51/ no longer exists in arduino_lcd_demo"
    - "library.json in lib/lua54/ names lua54 at version 5.4.8"
    - "esp32_idf_example CMakeLists.txt references lua54_esp32 not lua51_esp32"
    - "esp32_lcd_demo CMakeLists.txt references lua54_esp32 not lua51_esp32"
  artifacts:
    - path: "examples/arduino_lcd_demo/lib/lua54/library.json"
      provides: "PlatformIO library descriptor for Lua 5.4.8"
      contains: "lua54"
    - path: "examples/arduino_lcd_demo/lib/lua54/lapi.c"
      provides: "Lua 5.4.8 core source"
    - path: "examples/arduino_lcd_demo/lib/lua54/lcorolib.c"
      provides: "Lua 5.4 coroutine library (new in 5.4)"
    - path: "examples/arduino_lcd_demo/lib/lua54/lutf8lib.c"
      provides: "Lua 5.4 UTF-8 library (new in 5.4)"
  key_links:
    - from: "examples/arduino_lcd_demo/lib/lua54/library.json"
      to: "PlatformIO build system"
      via: "srcFilter +<l*.c> -<lua.c> -<luac.c>"
    - from: "examples/esp32_idf_example/CMakeLists.txt"
      to: "lua54_esp32 target"
      via: "if(TARGET lua54_esp32)"
    - from: "examples/esp32_lcd_demo/CMakeLists.txt"
      to: "lua54_esp32 target"
      via: "if(TARGET lua54_esp32)"
---

<objective>
Update both example categories to use Lua 5.4 instead of Lua 5.1:
1. Replace the bundled lua51/ sources in arduino_lcd_demo with Lua 5.4.8 sources in lib/lua54/.
2. Fix the two IDF example CMakeLists.txt files that still reference the old lua51_esp32 CMake target.

Purpose: Quick task 8 upgraded the main CMakeLists.txt to Lua 5.4 (target renamed lua54_esp32). Examples must be consistent.
Output: lib/lua54/ with full Lua 5.4.8 sources, lib/lua51/ removed, IDF CMakeLists.txt corrected.
</objective>

<execution_context>
@/home/unwn/.claude/get-shit-done/workflows/execute-plan.md
@/home/unwn/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/quick/9-update-examples-arduino-to-lua54-lib-idf/9-PLAN.md
</context>

<tasks>

<task type="auto">
  <name>Task 1: Replace lib/lua51 with lib/lua54 (Lua 5.4.8 sources)</name>
  <files>
    examples/arduino_lcd_demo/lib/lua54/library.json
    examples/arduino_lcd_demo/lib/lua54/*.c
    examples/arduino_lcd_demo/lib/lua54/*.h
    examples/arduino_lcd_demo/lib/lua51/ (deleted)
  </files>
  <action>
Download Lua 5.4.8 and replace the bundled 5.1.5 sources:

```bash
# Download and extract Lua 5.4.8
cd /tmp
curl -LO https://www.lua.org/ftp/lua-5.4.8.tar.gz
tar xzf lua-5.4.8.tar.gz

# Create lib/lua54 directory
mkdir -p /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54

# Copy all .c and .h files from Lua 5.4.8 src/
cp /tmp/lua-5.4.8/src/*.c /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/
cp /tmp/lua-5.4.8/src/*.h /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/

# Remove the old lua51 directory
rm -rf /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua51
```

Then create library.json in examples/arduino_lcd_demo/lib/lua54/:

```json
{
  "name": "lua54",
  "version": "5.4.8",
  "description": "Lua 5.4.8 scripting language",
  "license": "MIT",
  "build": {
    "srcFilter": [
      "+<l*.c>",
      "-<lua.c>",
      "-<luac.c>"
    ]
  }
}
```

The srcFilter `+<l*.c>` picks up all lapi.c, lcorolib.c, lutf8lib.c etc. and excludes lua.c (REPL) and luac.c (compiler) — same filter as 5.1 but correctly captures the new 5.4-only files (lcorolib.c, lutf8lib.c, lctype.c).
  </action>
  <verify>
    <automated>ls /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/lcorolib.c /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/lutf8lib.c /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/library.json &amp;&amp; ! ls /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua51 2>/dev/null &amp;&amp; grep -q '"5.4.8"' /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/library.json</automated>
  </verify>
  <done>lib/lua54/ contains Lua 5.4.8 .c/.h files including lcorolib.c and lutf8lib.c; library.json names lua54 at 5.4.8; lib/lua51/ is gone.</done>
</task>

<task type="auto">
  <name>Task 2: Update IDF example CMakeLists.txt — lua51_esp32 to lua54_esp32</name>
  <files>
    examples/esp32_idf_example/CMakeLists.txt
    examples/esp32_lcd_demo/CMakeLists.txt
  </files>
  <action>
In both CMakeLists.txt files, replace every occurrence of `lua51_esp32` with `lua54_esp32`.

For examples/esp32_idf_example/CMakeLists.txt:
- Line 37 comment: "lua51_esp32 uses longjmp..." -> "lua54_esp32 uses longjmp..."
- Line 39: `if(TARGET lua51_esp32)` -> `if(TARGET lua54_esp32)`
- Line 40: `target_link_libraries(lua51_esp32 PRIVATE idf::freertos)` -> `target_link_libraries(lua54_esp32 PRIVATE idf::freertos)`

For examples/esp32_lcd_demo/CMakeLists.txt:
- Line 40 comment: "lua51_esp32 uses longjmp..." -> "lua54_esp32 uses longjmp..."
- Line 41: `if(TARGET lua51_esp32)` -> `if(TARGET lua54_esp32)`
- Line 42: `target_link_libraries(lua51_esp32 PRIVATE idf::freertos)` -> `target_link_libraries(lua54_esp32 PRIVATE idf::freertos)`
  </action>
  <verify>
    <automated>! grep -r 'lua51_esp32' /home/unwn/git/enjin/examples/ &amp;&amp; grep -l 'lua54_esp32' /home/unwn/git/enjin/examples/esp32_idf_example/CMakeLists.txt /home/unwn/git/enjin/examples/esp32_lcd_demo/CMakeLists.txt</automated>
  </verify>
  <done>Both IDF example CMakeLists.txt files reference lua54_esp32; no remaining lua51_esp32 references in examples/.</done>
</task>

</tasks>

<verification>
After both tasks:

```bash
# No lua51 references remain in examples
grep -r 'lua51' /home/unwn/git/enjin/examples/ || echo "PASS: no lua51 references"

# lua54 dir has correct files
ls /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/ | grep -E 'lcorolib|lutf8lib|library.json'

# library.json version correct
grep '"5.4.8"' /home/unwn/git/enjin/examples/arduino_lcd_demo/lib/lua54/library.json
```
</verification>

<success_criteria>
- examples/arduino_lcd_demo/lib/lua54/ exists with Lua 5.4.8 .c and .h files
- examples/arduino_lcd_demo/lib/lua51/ does not exist
- library.json in lua54/ has name "lua54", version "5.4.8", same srcFilter as before
- examples/esp32_idf_example/CMakeLists.txt uses lua54_esp32 exclusively
- examples/esp32_lcd_demo/CMakeLists.txt uses lua54_esp32 exclusively
- No remaining lua51_esp32 references in examples/ directory
</success_criteria>

<output>
After completion, create `.planning/quick/9-update-examples-arduino-to-lua54-lib-idf/9-SUMMARY.md` with what was done, files changed, and any notes.
</output>
