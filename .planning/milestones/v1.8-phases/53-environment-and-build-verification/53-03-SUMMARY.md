---
phase: 53-environment-and-build-verification
plan: "03"
status: complete
completed: "2026-03-02"
---

# Plan 53-03 Summary: Platform Build Verification

## Outcome

All three platform targets verified building successfully.

## Fixes Applied

### WASM
- Replaced LuaJIT (`ljamalg.c`) with Lua 5.1.5 via CMake FetchContent — LuaJIT has no WASM architecture backend
- Added Lua 5.1 compat shims (`LUA_OK`, `lua_pcallk`, `luaL_testudata`) in `lua_platform.hpp`
- Propagated `LUA_INCLUDE_DIRS` as INTERFACE on `enjin2_lua` so consumers find `lua.h`
- Fixed relative `luajit/src` paths → absolute in CMakeLists.txt

### ESP32
- Replaced LuaJIT with Lua 5.1.5 via FetchContent for ESP32 target
- Made `VCV_RACK` define conditional on `NOT ESP32` — was unconditionally PUBLIC, hiding ESP32-specific declarations
- Moved Lua 5.1 compat shims outside the `#ifdef VCV_RACK` block so ESP32 path receives them
- Fixed `#ifndef VCV_RACK` → `#ifdef ARDUINO` in `canvas.hpp` (Arduino.h unavailable in ESP-IDF)
- Added `#include <esp_system.h>` in `lua_script.cpp` for `esp_restart()`
- Replaced Lua 5.2+ `luaL_requiref` calls in `openEmbeddedLibraries` with `luaL_openlibs(L)`
- Restructured `esp32_idf_example/` into proper ESP-IDF project (`main/CMakeLists.txt` with `idf_component_register`)
- Linked `lua51_esp32` to `idf::freertos` for `__wrap_longjmp` resolution
- Changed `memoryPool` from static BSS array to PSRAM-allocated pointer (`heap_caps_malloc(MALLOC_CAP_SPIRAM)`) with 2MB limit — keeps DRAM free on ESP32-S3 with 8MB PSRAM

## Build Outputs

| Target | Output | Size |
|--------|--------|------|
| SDL3 | `build/sdl3/enjin2_sdl` | 265KB |
| WASM | `build/wasm/enjin2.js` + `enjin2.wasm` | 141KB + 428KB |
| ESP32 | `build/esp32/enjin2_esp32_lua.bin` | 537KB (48% flash free) |
