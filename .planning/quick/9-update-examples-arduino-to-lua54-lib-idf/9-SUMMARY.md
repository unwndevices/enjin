---
phase: quick-9
plan: 9
subsystem: examples
tags: [lua, lua54, arduino, esp32-idf, platformio]
dependency_graph:
  requires: [quick-8]
  provides: [consistent-lua54-examples]
  affects: [examples/arduino_lcd_demo, examples/esp32_idf_example, examples/esp32_lcd_demo, examples/esp32_sdcard_demo, examples/esp32_spiffs_demo]
tech_stack:
  added: []
  patterns: [lua-5.4.8-platformio-srcFilter]
key_files:
  created:
    - examples/arduino_lcd_demo/lib/lua54/library.json
    - examples/arduino_lcd_demo/lib/lua54/*.c (56 files, Lua 5.4.8 sources)
    - examples/arduino_lcd_demo/lib/lua54/*.h (25 files, Lua 5.4.8 headers)
  modified:
    - examples/esp32_idf_example/CMakeLists.txt
    - examples/esp32_lcd_demo/CMakeLists.txt
    - examples/esp32_sdcard_demo/CMakeLists.txt
    - examples/esp32_spiffs_demo/CMakeLists.txt
  deleted:
    - examples/arduino_lcd_demo/lib/lua51/ (all source files)
decisions:
  - "srcFilter +<l*.c> -<lua.c> -<luac.c> unchanged from lua51 — same pattern correctly captures lcorolib.c, lutf8lib.c, lctype.c in 5.4"
metrics:
  duration: "~2.5 minutes"
  completed_date: "2026-03-04"
  tasks_completed: 2
  files_changed: 85
---

# Quick Task 9: Update Examples Arduino to Lua 5.4 and IDF CMakeLists.txt Summary

**One-liner:** Replaced bundled Lua 5.1.5 sources with Lua 5.4.8 in arduino_lcd_demo and updated all four IDF example CMakeLists.txt files from lua51_esp32 to lua54_esp32.

## What Was Done

### Task 1: Replace lib/lua51 with lib/lua54 (Lua 5.4.8 sources)

Downloaded Lua 5.4.8 from lua.org/ftp/, created `examples/arduino_lcd_demo/lib/lua54/` with the full source distribution (all .c and .h files from the src/ directory), and wrote a `library.json` PlatformIO descriptor. The old `lib/lua51/` directory was removed entirely.

The srcFilter `+<l*.c> -<lua.c> -<luac.c>` from the lua51 library.json was preserved unchanged — it correctly picks up the new 5.4-only files (lcorolib.c, lutf8lib.c, lctype.c) while continuing to exclude the standalone interpreter (lua.c) and compiler (luac.c).

**Commit:** `321e572`

### Task 2: Update IDF example CMakeLists.txt — lua51_esp32 to lua54_esp32

Updated the FreeRTOS longjmp workaround block in both target CMakeLists.txt files identified in the plan:

- `examples/esp32_idf_example/CMakeLists.txt` — comment + `if(TARGET ...)` + `target_link_libraries(...)` updated
- `examples/esp32_lcd_demo/CMakeLists.txt` — same pattern updated

**Commit:** `1771c94`

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing coverage] Also updated esp32_sdcard_demo and esp32_spiffs_demo CMakeLists.txt**

- **Found during:** Task 2 verification
- **Issue:** The plan listed only `esp32_idf_example` and `esp32_lcd_demo`, but `esp32_sdcard_demo/CMakeLists.txt` and `esp32_spiffs_demo/CMakeLists.txt` also had `lua51_esp32` references. The plan's success criterion "No remaining lua51_esp32 references in examples/" could not be met without fixing these two.
- **Fix:** Applied the same lua51_esp32 -> lua54_esp32 replacement to both files.
- **Files modified:** `examples/esp32_sdcard_demo/CMakeLists.txt`, `examples/esp32_spiffs_demo/CMakeLists.txt`
- **Commit:** `0ea4d52`

## Self-Check: PASSED

| Check | Result |
|-------|--------|
| examples/arduino_lcd_demo/lib/lua54/library.json exists | FOUND |
| examples/arduino_lcd_demo/lib/lua54/lcorolib.c exists | FOUND |
| examples/arduino_lcd_demo/lib/lua54/lutf8lib.c exists | FOUND |
| examples/arduino_lcd_demo/lib/lua51/ does not exist | CONFIRMED |
| Commit 321e572 (Task 1) | FOUND |
| Commit 1771c94 (Task 2) | FOUND |
| Commit 0ea4d52 (deviation fix) | FOUND |
