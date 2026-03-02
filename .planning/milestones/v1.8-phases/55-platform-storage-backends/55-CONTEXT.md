# Phase 55: Platform Storage Backends - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Implement `saveToFile()`/`loadFromFile()` for WASM (localStorage) and ESP32 (NVS) so `engine.store` data persists across page reloads and power cycles. SDL3 desktop behavior is unchanged. Key length >15 chars on ESP32 is rejected with a Lua error.

</domain>

<decisions>
## Implementation Decisions

### Write frequency
- `save()`, `delete()`, `clear()` update in-memory state only on both WASM and ESP32
- `flush()` is the commit point — it writes the full store blob to localStorage (WASM) or NVS (ESP32)
- This matches the success criteria wording ("save() + flush() persists") and protects NVS flash endurance on ESP32
- Applies consistently on both platforms so game code behaves the same everywhere
- Add a brief inline comment on `flush()` in the Lua binding explaining it is the commit point for WASM/ESP32

### load() behavior
- `load()` reads from in-memory state only on all platforms — no per-call NVS/localStorage read
- Initial backing store read happens once at startup

### WASM interop
- Use **EM_JS** (C declares named JS functions with inline JS bodies) — not EM_ASM
- localStorage write/read functions live in a new **`src/scripting/wasm_storage.cpp`** (isolated from bindings_store.cpp)
- localStorage key is fixed: `"enjin2_store"` — WASM has no filesystem path concept so engine.store.path() is irrelevant on WASM

### ESP32 NVS key limit
- Keys longer than 15 characters passed to `engine.store.save()` on ESP32 return a Lua error (not silent truncation)
- Validation happens in the C++ binding layer, consistent with how other input validation works

### Claude's Discretion
- ESP32 NVS implementation file location (e.g. `src/scripting/esp32_storage.cpp` by analogy with wasm_storage.cpp)
- NVS namespace name
- Whether `engine.store.path()` is a no-op or prints a warning on WASM/ESP32
- Error message wording for key-too-long rejection

</decisions>

<specifics>
## Specific Ideas

- User noted: would like a dedicated way to reload state from the backing store (re-read localStorage/NVS into memory). Not in Phase 55 scope — noted for backlog.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `LuaStore::saveToFile()` / `loadFromFile()` SDL3 implementation (bindings_store.cpp:165–314): Reference for correctness; platform stubs at lines 316–320 are the replacement targets
- `writeStoreToBuffer()` from Phase 54: The WASM/ESP32 backends call this to serialize before writing
- `src/bindings/emscripten_bindings.cpp`: Existing embind pattern — shows how WASM-specific code is currently structured

### Established Patterns
- Platform guards: `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` wraps SDL3 implementation
- Auto-persist on SDL3: All mutating operations call `saveToFile()` — on WASM/ESP32 these become no-ops (flush() handles writes)
- `STORE_MAX_KEY = 64` in bindings.hpp — NVS allows only 15 chars; validation goes in the ESP32 binding layer

### Integration Points
- `bindings_store.cpp` platform stubs (line 316–320): Replace `return false` stubs with calls to new wasm_storage / esp32_storage implementations
- `lua_engine_store_flush()` (bindings_store.cpp:514–521): Add WASM/ESP32 branch that calls the new commit functions
- CMakeLists.txt: New source files (wasm_storage.cpp, esp32_storage.cpp) need to be added to the appropriate platform build targets

</code_context>

<deferred>
## Deferred Ideas

- `engine.store.reload()` — force re-read from localStorage/NVS into memory. Not in Phase 55 scope; add to backlog.

</deferred>

---

*Phase: 55-platform-storage-backends*
*Context gathered: 2026-03-02*
