---
phase: 55-platform-storage-backends
plan: 02
subsystem: scripting
tags: [esp32, nvs, nvs_flash, lua, storage, cpp, idf]

requires:
  - phase: 55-01-wasm-storage
    provides: platform guard structure — #elif __EMSCRIPTEN__ block, flush() platform branch, auto-persist guards
  - phase: 54-json-serializer-refactor
    provides: writeStoreToBuffer — shared allocation-free JSON serializer used by ESP32 saveToFile

provides:
  - esp32_storage.cpp with NVS blob read/write helpers (esp32_storage_write / esp32_storage_read)
  - ESP32 saveToFile/loadFromFile in bindings_store.cpp (#else block replacing stubs)
  - ESP32 key-length validation in lua_engine_store_save (> 15 chars → Lua error, STORE-04)
  - nvs_flash_init() with erase fallback in app_main before engine.initialize()
  - nvs_flash linked in examples/esp32_idf_example CMakeLists files
affects: [esp32-build, engine-store, examples]

tech-stack:
  added: []
  patterns:
    - NVS blob pattern: two-phase nvs_get_blob (size query then fill); nvs_open failure = not-found (not error)
    - ESP32 key validation: inline strlen > 15 check with luaL_error before switch(vtype)
    - NVS init pattern: nvs_flash_init() → if NO_FREE_PAGES or NEW_VERSION_FOUND → erase + reinit

key-files:
  created:
    - src/scripting/esp32_storage.cpp
  modified:
    - src/scripting/bindings_store.cpp
    - CMakeLists.txt
    - examples/esp32_idf_example/CMakeLists.txt
    - examples/esp32_idf_example/main/CMakeLists.txt
    - examples/esp32_idf_example/main/main.cpp

key-decisions:
  - "esp32_storage_read treats nvs_open failure as not-found — returns false, caller treats as no saved data (same semantics as WASM)"
  - "loadFromFile returns true when esp32_storage_read returns false — absence of NVS data is not an error"
  - "Key validation uses strlen(key) > 15 with luaL_error — not silent truncation — ESP32 NVS enforces 15-char key limit"
  - "esp32_storage_write takes len_including_null (strlen+1) so NVS stores the null terminator in the blob"
  - "nvs_flash_init() placed in app_main not engine.initialize() — NVS init is a platform concern, not engine concern"

patterns-established:
  - "NVS init pattern: nvs_flash_init() with erase fallback in app_main before any engine init"
  - "ESP32 key validation: luaL_error not silent truncation — matches NVS constraint exactly"

requirements-completed: [STORE-03, STORE-04]

duration: 10min
completed: 2026-03-02
---

# Phase 55-02: ESP32 NVS Backend Summary

**ESP32 NVS persistence for engine.store: NVS blob helpers (esp32_storage.cpp) + saveToFile/loadFromFile + 15-char key validation + nvs_flash_init in app_main**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-03-02T20:45:00Z
- **Completed:** 2026-03-02T20:55:00Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Created `esp32_storage.cpp` with `esp32_storage_write` (NVS `nvs_set_blob` + `nvs_commit`) and `esp32_storage_read` (two-phase `nvs_get_blob`: size query then fill) — entire file guarded with `#ifdef ESP32`
- Replaced ESP32 stub in `bindings_store.cpp` with full `#else` NVS implementation: `extern "C"` forward declarations, `saveToFile` (calls `writeStoreToBuffer` → `esp32_storage_write`), `loadFromFile` (calls `esp32_storage_read` then parses JSON using existing static functions)
- Added ESP32 key-length validation in `lua_engine_store_save`: keys > 15 chars return `luaL_error` with descriptive message (STORE-04)
- Updated three CMakeLists files with `nvs_flash` references; added `nvs_flash_init()` with erase fallback in `app_main` before `engine.initialize()`

## Task Commits

1. **Task 1: Create esp32_storage.cpp and update CMakeLists files** — `85436a8` (feat)
2. **Task 2: Implement ESP32 saveToFile/loadFromFile, key validation, and nvs_flash_init** — `2a8b533` (feat)

## Files Created/Modified

- `src/scripting/esp32_storage.cpp` — NVS blob helpers: `esp32_storage_write` (READWRITE open → set_blob → commit → close) and `esp32_storage_read` (READONLY open → two-phase get_blob → close)
- `src/scripting/bindings_store.cpp` — ESP32 `#else` block: `extern "C"` forward decls + `saveToFile`/`loadFromFile` implementations + key-length validation in `lua_engine_store_save`
- `CMakeLists.txt` — Added `esp32_storage.cpp` to `enjin2_lua` `target_sources` PRIVATE list
- `examples/esp32_idf_example/CMakeLists.txt` — Added `idf::nvs_flash` to `enjin2_lua` PRIVATE link libraries
- `examples/esp32_idf_example/main/CMakeLists.txt` — Added `nvs_flash` to component REQUIRES
- `examples/esp32_idf_example/main/main.cpp` — Added `#include "nvs_flash.h"` and `nvs_flash_init()` with erase fallback before `engine.initialize()`

## Decisions Made

- `esp32_storage_write` takes `len_including_null = strlen(buf) + 1` — NVS blob stores null terminator for clean string extraction on read
- `esp32_storage_read` returns false on `nvs_open` failure (not-found condition) — caller (`loadFromFile`) converts this to `return true` (no saved data, not an error), matching WASM semantics
- Key validation raises `luaL_error` (not silent truncation) — ESP32 NVS strictly enforces 15-char key limit; silent truncation would cause surprising behavior

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None — build succeeded and all 94 tests passed on first attempt.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

Phase 55 complete. REQUIREMENTS STORE-02, STORE-03, STORE-04 all satisfied:
- STORE-02: WASM localStorage backend (55-01)
- STORE-03: ESP32 NVS backend (55-02)
- STORE-04: NVS key-length validation (55-02)

---
*Phase: 55-platform-storage-backends*
*Completed: 2026-03-02*
