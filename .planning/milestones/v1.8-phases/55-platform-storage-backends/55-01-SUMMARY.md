---
phase: 55-platform-storage-backends
plan: 01
subsystem: scripting
tags: [wasm, emscripten, localStorage, lua, storage, cpp]

requires:
  - phase: 54-json-serializer-refactor
    provides: writeStoreToBuffer — shared allocation-free JSON serializer used by WASM saveToFile

provides:
  - wasm_storage.cpp with EM_JS localStorage bridge (wasm_storage_write / wasm_storage_read)
  - WASM saveToFile/loadFromFile in bindings_store.cpp (extern "C" forward-declared, #elif __EMSCRIPTEN__ block)
  - flush() platform branch: WASM/ESP32 bypass empty-path guard, desktop retains path-check
  - auto-persist guards: save/delete/clear no longer auto-flush on WASM or ESP32
affects: [55-02-esp32-nvs-backend, wasm-build, engine-store]

tech-stack:
  added: []
  patterns:
    - EM_JS bridge: JS functions declared in wasm_storage.cpp, forward-declared as extern "C" in bindings_store.cpp
    - Platform guard pattern: #if !defined(ESP32) && !defined(__EMSCRIPTEN__) / #elif __EMSCRIPTEN__ / #else (ESP32)
    - WASM/ESP32 flush-only persistence: auto-persist disabled; only explicit flush() writes storage

key-files:
  created:
    - src/scripting/wasm_storage.cpp
  modified:
    - src/scripting/bindings_store.cpp
    - CMakeLists.txt

key-decisions:
  - "wasm_storage.cpp only holds EM_JS declarations — loadFromFile lives in bindings_store.cpp same TU to access static JSON parser functions"
  - "loadFromFile returns true (not false) when wasm_storage_read returns 0 — absence of saved data is not an error"
  - "flush() platform branch added for WASM/ESP32 — calls saveToFile(nullptr) bypassing empty m_storePath guard"
  - "auto-persist in save/delete/clear guarded with !EMSCRIPTEN && !ESP32 — WASM/ESP32 must call flush() explicitly"

patterns-established:
  - "Platform guard pattern: #if !defined(ESP32) && !defined(__EMSCRIPTEN__) (desktop) / #elif defined(__EMSCRIPTEN__) (WASM) / #else (ESP32)"
  - "EM_JS bridge: declarations in wasm_storage.cpp, extern C forward-decls in bindings_store.cpp"

requirements-completed: [STORE-02]

duration: 12min
completed: 2026-03-02
---

# Phase 55-01: WASM localStorage Backend Summary

**WASM localStorage backend for engine.store: EM_JS bridge (wasm_storage.cpp) + saveToFile/loadFromFile + flush() platform branch**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-03-02T20:30:00Z
- **Completed:** 2026-03-02T20:42:00Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- Created `wasm_storage.cpp` with `EM_JS`-based `wasm_storage_write` and `wasm_storage_read` — entire file guarded with `#ifdef __EMSCRIPTEN__`, zero symbol emission on non-WASM builds
- Added `#elif defined(__EMSCRIPTEN__)` block in `bindings_store.cpp` with `saveToFile` (calls `writeStoreToBuffer` then `wasm_storage_write`) and `loadFromFile` (calls `wasm_storage_read` then parses JSON using existing static parser in same TU)
- Updated `lua_engine_store_flush()` with platform branch — WASM/ESP32 now call `saveToFile(nullptr)` bypassing the desktop empty-path guard; auto-persist in save/delete/clear is guarded with `#if !defined(__EMSCRIPTEN__) && !defined(ESP32)`

## Task Commits

1. **Task 1: Create wasm_storage.cpp with EM_JS localStorage bridge** — `59995ac` (feat)
2. **Task 2: Implement WASM saveToFile/loadFromFile and update flush()** — `2726f7b` (feat)

## Files Created/Modified

- `src/scripting/wasm_storage.cpp` — EM_JS bridge: `wasm_storage_write` writes JSON blob to `localStorage['enjin2_store']`; `wasm_storage_read` reads it back into a caller-supplied buffer
- `src/scripting/bindings_store.cpp` — Added WASM `#elif` block with `saveToFile`/`loadFromFile`; updated `flush()` with platform branch; guarded auto-persist blocks
- `CMakeLists.txt` — Added `wasm_storage.cpp` to `enjin2_lua` `target_sources` PRIVATE list

## Decisions Made

- `loadFromFile` returns `true` (not `false`) when `wasm_storage_read` returns 0 — absence of saved data in localStorage is not an error condition
- The `loadFromFile` WASM implementation lives in `bindings_store.cpp` (same TU as the SDL3 implementation) because the static JSON parser functions (`skipWhitespace`, `readJsonString`, `readJsonValue`) have internal linkage and are not accessible from `wasm_storage.cpp`
- `flush()` platform branch uses `saveToFile(nullptr)` on WASM/ESP32 — the path argument is unused on these platforms

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None — build succeeded and all 94 tests passed on first attempt.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

Plan 55-02 (ESP32 NVS backend) can proceed. The platform guard structure in `bindings_store.cpp` now has the correct three-way split: desktop / WASM (55-01) / ESP32 (55-02 stub → to be replaced). The `flush()` ESP32 branch is already wired correctly — Plan 55-02 only needs to replace the stub in the `#else` block.

---
*Phase: 55-platform-storage-backends*
*Completed: 2026-03-02*
