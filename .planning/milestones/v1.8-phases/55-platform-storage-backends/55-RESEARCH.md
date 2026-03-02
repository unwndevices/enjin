# Phase 55: Platform Storage Backends - Research

**Researched:** 2026-03-02
**Domain:** Emscripten localStorage (EM_JS) + ESP-IDF NVS (nvs_flash component)
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Write frequency:**
- `save()`, `delete()`, `clear()` update in-memory state only on both WASM and ESP32
- `flush()` is the commit point — it writes the full store blob to localStorage (WASM) or NVS (ESP32)
- Add a brief inline comment on `flush()` in the Lua binding explaining it is the commit point for WASM/ESP32

**load() behavior:**
- `load()` reads from in-memory state only on all platforms — no per-call NVS/localStorage read
- Initial backing store read happens once at startup

**WASM interop:**
- Use **EM_JS** (not EM_ASM)
- localStorage write/read functions live in a new **`src/scripting/wasm_storage.cpp`**
- localStorage key is fixed: `"enjin2_store"`
- `engine.store.path()` is irrelevant on WASM (no filesystem path concept)

**ESP32 NVS key limit:**
- Keys longer than 15 characters passed to `engine.store.save()` on ESP32 return a Lua error (not silent truncation)
- Validation happens in the C++ binding layer

### Claude's Discretion

- ESP32 NVS implementation file location (e.g. `src/scripting/esp32_storage.cpp` by analogy with wasm_storage.cpp)
- NVS namespace name
- Whether `engine.store.path()` is a no-op or prints a warning on WASM/ESP32
- Error message wording for key-too-long rejection

### Deferred Ideas (OUT OF SCOPE)

- `engine.store.reload()` — force re-read from localStorage/NVS into memory. Not in Phase 55 scope; add to backlog.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| STORE-02 | `engine.store.save/flush/load` works on WASM via localStorage with flush-only persistence pattern | EM_JS API verified; localStorage is synchronous and suitable for the store's 4KB blob |
| STORE-03 | `engine.store.save/flush/load` works on ESP32 via NVS with single JSON blob and `nvs_commit()` | ESP-IDF NVS blob API verified; single-blob approach is the correct pattern for a JSON string |
| STORE-04 | NVS backend validates and rejects keys longer than 15 characters | NVS 15-char key limit is a hard ESP-IDF constraint; validation in C++ binding layer is the correct approach |
</phase_requirements>

---

## Summary

Phase 55 implements two platform-specific storage backends that replace the stub `saveToFile`/`loadFromFile` in `bindings_store.cpp` (lines 411–412) on WASM and ESP32. On WASM, the approach uses Emscripten's `EM_JS` macro to declare C-callable JavaScript functions that read/write a single `localStorage` item keyed `"enjin2_store"`. On ESP32, the approach uses the ESP-IDF NVS (Non-Volatile Storage) component's blob API to store and retrieve a single JSON string under a fixed namespace and key.

Both backends rely on `writeStoreToBuffer()` (implemented in Phase 54) to serialize the in-memory store to compact JSON before writing. For loading, both backends retrieve the raw JSON string and hand it to the existing JSON parser already present in `bindings_store.cpp` (the `readJsonValue`/`readJsonObject` functions). The key design invariant is that all Lua-facing mutations (`save`, `delete`, `clear`) are in-memory only; `flush()` is the single commit point that triggers the actual I/O.

The primary risk on WASM is correctly bridging the C/JS boundary with EM_JS — the macro syntax is sensitive and must return values by pointer, not by value. The primary risk on ESP32 is NVS initialization: `nvs_flash_init()` must be called once (typically in `app_main`) before any NVS handle open, and the CMakeLists must link `idf::nvs_flash`.

**Primary recommendation:** Add `wasm_storage.cpp` (EM_JS read/write) and `esp32_storage.cpp` (NVS blob read/write) under `src/scripting/`, update the platform stubs in `bindings_store.cpp`, update `lua_engine_store_flush()` and `lua_engine_store_save()` to call the new backends, and add NVS key-length validation in `lua_engine_store_save()` under `#ifdef ESP32`.

---

## Standard Stack

### Core

| API / Component | Version | Purpose | Why Standard |
|----------------|---------|---------|--------------|
| `EM_JS` macro | Emscripten 3.1.73 (pinned) | Declare named C-callable JS functions with inline bodies | Official Emscripten mechanism for synchronous JS interop from C; preferred over EM_ASM per CONTEXT.md |
| `localStorage` (Web API) | Living standard | Persist store blob across page reloads | Synchronous, available in all browsers, supports strings up to 5–10 MB — far exceeds the 4KB store blob maximum |
| `nvs_flash` component | ESP-IDF v5.5 (pinned) | ESP32 non-volatile key-value storage | Official ESP-IDF persistent storage API; hardware-managed wear leveling; handles power-loss safety |
| `nvs_set_blob` / `nvs_get_blob` | ESP-IDF v5.5 | Store/retrieve arbitrary byte blobs (used here for the JSON string) | Correct NVS API for variable-length data; avoids the 4000-byte NVS string value limit |
| `nvs_commit` | ESP-IDF v5.5 | Flush NVS partition write to flash | Required after writes; without it, data is not guaranteed to survive a power cycle |

### Supporting

| API | Version | Purpose | When to Use |
|-----|---------|---------|-------------|
| `emscripten/em_js.h` | Emscripten 3.1.73 | Header that defines `EM_JS` macro | Include in `wasm_storage.cpp`; not needed in other files |
| `nvs_flash.h` | ESP-IDF v5.5 | NVS init (`nvs_flash_init`) and handle API | Include in `esp32_storage.cpp` |
| `esp_nvs.h` (via `nvs_flash.h`) | ESP-IDF v5.5 | `nvs_open`, `nvs_set_blob`, `nvs_get_blob`, `nvs_commit`, `nvs_close` | The core NVS handle operations |

---

## Architecture Patterns

### Recommended File Structure

```
src/scripting/
├── wasm_storage.cpp        # NEW — WASM localStorage backend (EM_JS)
├── esp32_storage.cpp       # NEW — ESP32 NVS backend (nvs_flash)
└── bindings_store.cpp      # MODIFY — replace stubs, add key-length validation
```

No new headers are needed. The two new files expose functions that are called only from within `bindings_store.cpp` via forward declarations or a small inline header — consistent with how the project currently structures platform-specific helpers.

### Pattern 1: EM_JS WASM Storage

**What:** Declare two C functions with inline JavaScript bodies using `EM_JS`. One writes a string to `localStorage`, one reads a string from `localStorage`.

**When to use:** Any time C code in a WASM build needs to call synchronous JavaScript storage operations.

**EM_JS syntax rules (critical):**
- The macro signature is `EM_JS(return_type, function_name, (args), { js_body; })`.
- The JS body must end with a semicolon inside the closing brace.
- String data is exchanged through WASM memory using `UTF8ToString` (JS reading from C pointer) and `stringToUTF8` (JS writing into a C-allocated buffer).
- The function is compiled as a regular C function callable anywhere in the translation unit that includes its definition.

```c
// Source: Emscripten official docs — https://emscripten.org/docs/api_reference/emscripten.h.html#c.EM_JS
#include <emscripten/em_js.h>

// Write the JSON blob to localStorage
EM_JS(void, wasm_storage_write, (const char* json_ptr), {
    var json = UTF8ToString(json_ptr);
    localStorage.setItem('enjin2_store', json);
});

// Read the JSON blob from localStorage into a caller-supplied buffer.
// Returns 1 on success (key found), 0 if the key does not exist.
EM_JS(int, wasm_storage_read, (char* out_ptr, int out_cap), {
    var val = localStorage.getItem('enjin2_store');
    if (val === null) { return 0; }
    var len = lengthBytesUTF8(val);
    if (len + 1 > out_cap) { return 0; }   // would overflow
    stringToUTF8(val, out_ptr, out_cap);
    return 1;
});
```

**Buffer sizing:** `LuaStore::STORE_BUFFER_MAX` (4096) is the correct buffer size. Pass this value as `out_cap`.

**Startup loading pattern in `loadFromFile` WASM branch:**
```cpp
#ifdef __EMSCRIPTEN__
bool LuaStore::loadFromFile(const char*) {
    char buf[STORE_BUFFER_MAX];
    if (!wasm_storage_read(buf, static_cast<int>(STORE_BUFFER_MAX))) {
        return true;  // no saved data — not an error
    }
    clear();
    const char* p = buf;
    // ... reuse existing JSON parser (skipWhitespace / readJsonValue)
    return true;
}
#endif
```

**Flush pattern in `saveToFile` WASM branch:**
```cpp
#ifdef __EMSCRIPTEN__
bool LuaStore::saveToFile(const char*) const {
    char buf[STORE_BUFFER_MAX];
    if (!writeStoreToBuffer(buf, STORE_BUFFER_MAX)) return false;
    wasm_storage_write(buf);
    return true;
}
#endif
```

### Pattern 2: ESP32 NVS Blob Storage

**What:** Use the NVS blob API to store the entire JSON string as a single blob under a fixed namespace and key.

**NVS key constraint (STORE-04):** NVS keys are limited to 15 characters (plus null terminator = 16 bytes). This applies to both the namespace name and the value key. The JSON blob is stored under a *value key* inside the NVS namespace, not the Lua-level store key, so there is no conflict with arbitrary Lua key names. Lua-level key validation (15-char limit) is required so that if game code ever stores per-key entries directly in NVS (not the blob pattern), they would not be silently truncated.

**Why blob over per-key NVS storage:** The requirements document explicitly lists "Per-key NVS storage" as out of scope ("NVS key enumeration is diagnostic-only; single JSON blob is correct"). NVS blob avoids 15-char key length restrictions on Lua store keys and avoids the overhead of NVS iteration.

**NVS namespace:** Choose `"enjin2"` (≤ 15 chars). Value key: `"store"` (≤ 15 chars).

**NVS initialization:** `nvs_flash_init()` must be called once per power cycle before any NVS handle is opened. This is typically done in `app_main()`. The ESP32 example's `main.cpp` does not call it yet — this is a required integration step. If `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, the partition must be erased first.

```c
// Source: ESP-IDF v5.5 NVS documentation
// https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/storage/nvs_flash.html
#include "nvs_flash.h"

// Call once in app_main before opening any NVS handle:
esp_err_t ret = nvs_flash_init();
if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
}
```

**Write pattern (flush):**
```cpp
bool LuaStore::saveToFile(const char*) const {
    char buf[STORE_BUFFER_MAX];
    if (!writeStoreToBuffer(buf, STORE_BUFFER_MAX)) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("enjin2", NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(handle, "store", buf, strlen(buf) + 1);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return (err == ESP_OK);
}
```

**Read pattern (startup load):**
```cpp
bool LuaStore::loadFromFile(const char*) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("enjin2", NVS_READONLY, &handle);
    if (err != ESP_OK) return true;  // no saved data — not an error

    size_t required = 0;
    err = nvs_get_blob(handle, "store", nullptr, &required);
    if (err != ESP_OK || required == 0 || required > STORE_BUFFER_MAX) {
        nvs_close(handle);
        return true;
    }

    char buf[STORE_BUFFER_MAX];
    err = nvs_get_blob(handle, "store", buf, &required);
    nvs_close(handle);
    if (err != ESP_OK) return false;

    clear();
    const char* p = buf;
    // ... reuse existing JSON parser
    return true;
}
```

**Pattern 3: ESP32 Key-Length Validation (STORE-04)**

Validation goes in `lua_engine_store_save()` in `bindings_store.cpp`, inside a `#ifdef ESP32` block, before any store mutation:

```cpp
#ifdef ESP32
    // NVS key length limit: 15 characters max
    if (strlen(key) > 15) {
        return luaL_error(L, "engine.store.save: key '%s' exceeds 15-character limit on ESP32", key);
    }
#endif
```

This is consistent with how other input validation is done in the binding layer (returning a Lua error rather than silently truncating).

### Anti-Patterns to Avoid

- **Using EM_ASM instead of EM_JS:** EM_ASM inlines JavaScript into the calling C function and cannot easily return strings or accept string parameters. EM_JS is the correct approach when named, reusable JS functions are needed. The CONTEXT.md locks this decision.
- **Calling localStorage per-key (on save/delete):** The design decision is flush-only persistence. Auto-persist calls in `lua_engine_store_save`, `lua_engine_store_delete`, and `lua_engine_store_clear` must NOT call storage backends on WASM/ESP32 — only `flush()` triggers writes.
- **Opening NVS handle per-call:** NVS handles should be opened, used, and closed within the same function call. Holding an open handle across calls is an error-prone pattern in ESP-IDF.
- **Using `nvs_set_str` instead of `nvs_set_blob`:** `nvs_set_str` has a 4000-byte value limit in ESP-IDF v5.x. `nvs_set_blob` does not have this restriction. Our blob can be up to 4096 bytes (`STORE_BUFFER_MAX`), which safely fits within both APIs, but `nvs_set_blob` is the more correct API for arbitrary binary/string data that includes the null terminator.
- **Silently ignoring `nvs_commit` failure:** If `nvs_commit` fails, the write is not durable. The `saveToFile` return value must reflect commit success.
- **Not calling `nvs_flash_init()`:** The NVS subsystem must be initialized before use. Forgetting this in `app_main` will cause `nvs_open` to fail with `ESP_ERR_NVS_NOT_INITIALIZED`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON serialization | Custom JSON writer | `writeStoreToBuffer()` from Phase 54 | Already implemented, tested, and allocation-free |
| JSON parsing | Custom JSON reader | Existing `readJsonValue`/`readJsonObject` static functions in `bindings_store.cpp` | Already handles all store types with escape sequences |
| Wear leveling | Flash page rotation | NVS component | NVS handles wear leveling automatically across the NVS partition |
| Power-loss safety | Journaling logic | `nvs_commit()` | NVS provides atomic commit semantics |
| JS string↔C bridge | Custom memory copy | `UTF8ToString` / `stringToUTF8` | Emscripten standard helpers for WASM memory access from JS |

**Key insight:** The serialization and parsing layers are fully reusable from Phase 54. This phase is purely about plugging in platform I/O at the `saveToFile`/`loadFromFile` boundaries.

---

## Common Pitfalls

### Pitfall 1: EM_JS Compilation Guard Missing

**What goes wrong:** `wasm_storage.cpp` is compiled on all platforms. `EM_JS` is only defined when compiling with Emscripten. If the file is added to `enjin2_lua` without guards, desktop builds fail to compile.

**Why it happens:** The new file is added to `enjin2_lua` sources in `CMakeLists.txt`, which is shared across SDL3, WASM, and ESP32.

**How to avoid:** Wrap the entire `wasm_storage.cpp` body in `#ifdef __EMSCRIPTEN__` and add an empty compilation unit fallback, OR list it conditionally in `CMakeLists.txt` using generator expressions or `if(EMSCRIPTEN)`.

**Warning signs:** Desktop SDL3 or test build fails with `EM_JS undefined`.

**Recommendation:** Use `#ifdef __EMSCRIPTEN__` guards in the source file — consistent with how the existing SDL3 block guard `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` already works in `bindings_store.cpp`.

### Pitfall 2: Auto-Persist Still Active on WASM/ESP32

**What goes wrong:** On WASM or ESP32, the existing `lua_engine_store_save` binding calls `b->m_store.saveToFile(b->m_storePath)` after each mutation. On desktop this is correct. On WASM/ESP32 the stub returns `false` today — but after this phase, the WASM stub will call `wasm_storage_write` on every `save()` call, causing expensive localStorage writes on every mutation instead of only on `flush()`.

**Why it happens:** The auto-persist call at line 536–538 of `bindings_store.cpp` is unconditional (guarded only by `m_storePath[0] != '\0'`).

**How to avoid:** On WASM and ESP32, `m_storePath` remains empty (no `engine.store.path()` concept). The auto-persist block `if (ok && b->m_storePath[0] != '\0')` will therefore never fire on WASM/ESP32 as long as game code does not call `engine.store.path()`. Add a `#if !defined(__EMSCRIPTEN__) && !defined(ESP32)` guard around the auto-persist block, or ensure the WASM/ESP32 `saveToFile` stubs do nothing when called without `flush()` triggering. The flush path in `lua_engine_store_flush()` (lines 607–614) must be extended with WASM/ESP32 branches.

**Warning signs:** localStorage is written on every `engine.store.save()` call rather than only on `engine.store.flush()`.

### Pitfall 3: NVS `nvs_flash_init()` Not Called

**What goes wrong:** `nvs_open()` returns `ESP_ERR_NVS_NOT_INITIALIZED`, `loadFromFile` silently returns early, and game code sees an empty store after every power cycle.

**Why it happens:** The current `examples/esp32_idf_example/main/main.cpp` does not call `nvs_flash_init()`. This initialization step must be added to `app_main` before any script loads.

**How to avoid:** Add `nvs_flash_init()` (with the erase fallback) to `app_main`, and link `idf::nvs_flash` in the ESP32 CMakeLists.

**Warning signs:** All `nvs_open` calls return `ESP_ERR_NVS_NOT_INITIALIZED`; store loads always return empty.

### Pitfall 4: nvs_flash Not Linked

**What goes wrong:** ESP32 build succeeds but links fail with undefined references to `nvs_open`, `nvs_set_blob`, etc.

**Why it happens:** NVS functions are in the `nvs_flash` ESP-IDF component, which must be explicitly listed in `REQUIRES` in the `idf_component_register` call.

**How to avoid:** Add `nvs_flash` to the `REQUIRES` list in `examples/esp32_idf_example/main/CMakeLists.txt` and add `target_link_libraries(enjin2_lua PRIVATE idf::nvs_flash)` in the ESP32 `CMakeLists.txt`.

**Warning signs:** `idf.py build` fails with `undefined reference to nvs_open`.

### Pitfall 5: EM_JS Return Value for Read Function

**What goes wrong:** The `wasm_storage_read` function needs to signal "no saved data" vs "data written to buffer". Using a global or an out-parameter in EM_JS is error-prone.

**Why it happens:** JavaScript `localStorage.getItem()` returns `null` when the key does not exist — this must be distinguished from an empty string.

**How to avoid:** Return an int from `EM_JS` (0 = not found, 1 = success). The JS body returns the integer via `return 1;` / `return 0;`. This is fully supported by EM_JS.

**Warning signs:** `loadFromFile` always parses the stored value even on first boot (treating null as empty JSON).

### Pitfall 6: STORE_BUFFER_MAX Too Small

**What goes wrong:** `writeStoreToBuffer()` returns `false` (truncated), and the truncated JSON is written to localStorage/NVS. On next load, the JSON parser fails partway through, leaving a partial store.

**Why it happens:** With all 16 keys at STORE_MAX_KEY (64 chars) and STORE_MAX_STRING (128 chars) values, the worst-case blob can exceed 4KB. `STORE_BUFFER_MAX = 4096` is sized for typical game saves, not the absolute worst case.

**How to avoid:** Document the sizing limitation in the flush path. If `writeStoreToBuffer` returns `false`, return `false` from `saveToFile` and do NOT write the truncated buffer. The check is already built into the pattern shown in Code Examples above.

---

## Code Examples

Verified patterns from official sources:

### EM_JS: Reading String from localStorage

```c
// Source: Emscripten docs — https://emscripten.org/docs/api_reference/emscripten.h.html#c.EM_JS
#ifdef __EMSCRIPTEN__
#include <emscripten/em_js.h>

EM_JS(int, wasm_storage_read, (char* out_ptr, int out_cap), {
    var val = localStorage.getItem('enjin2_store');
    if (val === null) { return 0; }
    var encoded_len = lengthBytesUTF8(val);
    if (encoded_len + 1 > out_cap) { return 0; }
    stringToUTF8(val, out_ptr, out_cap);
    return 1;
});

EM_JS(void, wasm_storage_write, (const char* json_ptr), {
    localStorage.setItem('enjin2_store', UTF8ToString(json_ptr));
});
#endif
```

### NVS: Storing a JSON blob on ESP32

```cpp
// Source: ESP-IDF v5.5 NVS Programming Guide
// https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/storage/nvs_flash.html
#ifdef ESP32
#include "nvs_flash.h"

bool esp32_storage_write(const char* json, size_t len_including_null) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("enjin2", NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;
    err = nvs_set_blob(handle, "store", json, len_including_null);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return (err == ESP_OK);
}

bool esp32_storage_read(char* out, size_t cap) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("enjin2", NVS_READONLY, &handle);
    if (err != ESP_OK) return false;
    size_t required = 0;
    err = nvs_get_blob(handle, "store", nullptr, &required);
    if (err != ESP_OK || required == 0 || required > cap) {
        nvs_close(handle);
        return false;
    }
    err = nvs_get_blob(handle, "store", out, &required);
    nvs_close(handle);
    return (err == ESP_OK);
}
#endif
```

### Binding Layer: flush() with platform branches

```cpp
// In bindings_store.cpp — lua_engine_store_flush()
int LuaBindings::lua_engine_store_flush(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushboolean(L, 0); return 1; }

#if defined(__EMSCRIPTEN__) || defined(ESP32)
    // flush() is the commit point for WASM/ESP32 — writes full store blob
    bool ok = b->m_store.saveToFile(nullptr);  // path arg unused on these platforms
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
#else
    // Desktop: requires a path to be set
    if (b->m_storePath[0] == '\0') { lua_pushboolean(L, 0); return 1; }
    bool ok = b->m_store.saveToFile(b->m_storePath);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
#endif
}
```

### Binding Layer: key-length validation on ESP32

```cpp
// In lua_engine_store_save(), before the switch(vtype):
#ifdef ESP32
    if (strlen(key) > 15) {
        return luaL_error(L, "engine.store.save: key '%s' exceeds 15-character NVS limit on ESP32", key);
    }
#endif
```

---

## Integration Points (File-by-File)

### `src/scripting/bindings_store.cpp`

1. **Lines 409–413 (platform stubs):** Replace `return false` stubs with calls to the new backend functions:
   ```cpp
   #elif defined(__EMSCRIPTEN__)
   bool LuaStore::saveToFile(const char*) const { /* wasm impl */ }
   bool LuaStore::loadFromFile(const char*)     { /* wasm impl */ }
   #elif defined(ESP32)
   bool LuaStore::saveToFile(const char*) const { /* esp32 impl */ }
   bool LuaStore::loadFromFile(const char*)     { /* esp32 impl */ }
   #endif
   ```
   The current `#else` block covers both with the same stubs. The new structure splits them.

2. **Lines 607–614 (`lua_engine_store_flush`):** Add WASM/ESP32 branch (see Code Examples above).

3. **Lines 503–542 (`lua_engine_store_save`):** Add ESP32 key-length validation before the `switch(vtype)`.

4. **Comment on `flush()`:** Add inline comment: "flush() is the commit point — calls saveToFile which writes to localStorage (WASM) or NVS (ESP32)".

### `src/scripting/wasm_storage.cpp` (NEW)

- Contains `EM_JS` declarations for `wasm_storage_write` and `wasm_storage_read`.
- Entire file guarded with `#ifdef __EMSCRIPTEN__`.
- Implements `LuaStore::saveToFile` and `LuaStore::loadFromFile` for WASM.
- Must `#include` the bindings header to access `LuaStore::STORE_BUFFER_MAX` and the JSON parser helpers. Since the JSON parser statics (`skipWhitespace`, `readJsonValue`, etc.) are defined in `bindings_store.cpp` as `static`, they are not accessible from `wasm_storage.cpp`. Options:
  - **Option A (recommended):** Inline the WASM `saveToFile`/`loadFromFile` implementations directly in `bindings_store.cpp` within `#elif defined(__EMSCRIPTEN__)` blocks, and keep `wasm_storage.cpp` only for EM_JS declarations (forward-declared in a minimal header or declared in the .cpp before use).
  - **Option B:** Move the JSON parser helpers to a shared internal header. This is more refactoring than needed.
  - **Recommendation: Option A.** Keep the implementations in `bindings_store.cpp` where the static JSON parser functions are already visible, and let `wasm_storage.cpp` provide only the `EM_JS` function bodies. Include `wasm_storage.cpp`'s declarations via a forward `extern "C"` declaration in `bindings_store.cpp`.

### `src/scripting/esp32_storage.cpp` (NEW)

- Contains `esp32_storage_write` and `esp32_storage_read` helper functions.
- Entire file guarded with `#ifdef ESP32`.
- Implements `LuaStore::saveToFile` and `LuaStore::loadFromFile` for ESP32.
- Same Option A/B consideration as WASM.

### `CMakeLists.txt`

Add the new source files to `enjin2_lua`:
```cmake
target_sources(enjin2_lua PRIVATE
    ...
    src/scripting/wasm_storage.cpp     # safe to add unconditionally — #ifdef __EMSCRIPTEN__ guard inside
    src/scripting/esp32_storage.cpp    # safe to add unconditionally — #ifdef ESP32 guard inside
)
```

### `examples/esp32_idf_example/main/CMakeLists.txt`

Add `nvs_flash` to `REQUIRES`:
```cmake
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "." "../../../include"
    REQUIRES esp_system freertos nvs_flash
)
```

### `examples/esp32_idf_example/CMakeLists.txt`

Add `idf::nvs_flash` link to `enjin2_lua`:
```cmake
if(TARGET enjin2_lua)
    target_link_libraries(enjin2_lua PRIVATE
        idf::esp_system
        idf::heap
        idf::vfs
        idf::spiffs
        idf::newlib
        idf::nvs_flash   # NEW
    )
endif()
```

### `examples/esp32_idf_example/main/main.cpp`

Add NVS initialization in `app_main` before any script execution:
```cpp
#include "nvs_flash.h"

// After ESP_LOGI, before engine.initialize():
esp_err_t nvs_ret = nvs_flash_init();
if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
}
```

---

## Tests

The project uses a hand-rolled `ASSERT` macro framework (no third-party test runner). Tests live in `tests/` as standalone executables added to `CMakeLists.txt`. The existing `store_test.cpp` covers desktop store behavior.

WASM and ESP32 platform-specific tests **cannot run on the desktop test runner** (no `localStorage`, no NVS hardware). The approach used by this project for platform features is:

1. Test the **shared serialization path** (already done for `writeStoreToBuffer` in Phase 54 tests within `store_test.cpp`).
2. For WASM: Manual verification in a browser by loading the test page, calling `engine.store.save(...)` + `engine.store.flush()`, reloading, and checking `engine.store.load(...)`.
3. For ESP32: Manual verification by flashing firmware, running a Lua script that saves + flushes, power cycling, and checking load.

**However**, to give the planner a test target for STORE-04 specifically (key-length rejection), a unit test **can** be written on the desktop because the ESP32 key-length validation path in `lua_engine_store_save` can be exercised by simulating the `ESP32` define — but only if the store test is compiled with that define. The simpler approach: add a `#define` mock or test the behavior as a Lua-error check.

**Wave 0 gap:** Add test cases to `tests/store_test.cpp` for:
- `test_store_wasm_flush_behavior` — verifies `flush()` returns true/false (platform-independent behavior; existing `test_store_flush_no_path` and `test_store_flush_with_path` already cover this on desktop; no new test needed)
- `test_store_esp32_key_too_long` — requires the binding to be compiled with `#define ESP32`. This cannot run as part of the normal SDL3 test suite without a separate build variant. **Recommendation:** Document as manual-only for the ESP32 flash test, and add a comment in `store_test.cpp` marking the gap.

---

## Open Questions

1. **JSON parser reuse across translation units**
   - What we know: The static JSON parser functions (`skipWhitespace`, `readJsonValue`, `readJsonObject`) are defined as `static` in `bindings_store.cpp` and therefore invisible to other `.cpp` files.
   - What's unclear: Whether the WASM and ESP32 `loadFromFile` implementations should live in `bindings_store.cpp` (accessing the statics) or in separate files (requiring parser duplication or a refactor).
   - Recommendation: Keep all `saveToFile`/`loadFromFile` implementations inside `bindings_store.cpp`, separated by `#elif defined(__EMSCRIPTEN__)` / `#elif defined(ESP32)` blocks. New files (`wasm_storage.cpp`, `esp32_storage.cpp`) provide only the EM_JS/NVS helper functions, called via forward-declared `extern` in `bindings_store.cpp`.

2. **`engine.store.path()` on WASM/ESP32**
   - What we know: The decision is marked "Claude's Discretion".
   - What's unclear: Should it be a no-op (returns silently) or print a warning?
   - Recommendation: Make it a no-op that does NOT call `loadFromFile` on WASM/ESP32 (since `loadFromFile` on these platforms ignores the path argument anyway). Add a `printf("[store] path() ignored on this platform\n")` debug message guarded by a `#if` so it does not pollute production builds.

3. **NVS partition availability on target hardware**
   - What we know: ESP-IDF's default `partitions.csv` includes a NVS partition. The example project uses the default partition table.
   - What's unclear: Whether the target ESP32-S3 hardware in this project has the default partition table or a custom one.
   - Recommendation: Use `nvs_flash_init()` without specifying a partition name (uses the default `nvs` partition). If the default partition is absent, the init will fail with a clear error that can be diagnosed at flash time.

---

## Sources

### Primary (HIGH confidence)

- Emscripten official documentation, `EM_JS` API reference: https://emscripten.org/docs/api_reference/emscripten.h.html#c.EM_JS — verified EM_JS macro syntax, `UTF8ToString`, `stringToUTF8`, and return value handling
- ESP-IDF v5.5 NVS Programming Guide: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/storage/nvs_flash.html — verified `nvs_open`, `nvs_set_blob`, `nvs_get_blob`, `nvs_commit`, `nvs_close`, init pattern
- Project codebase (read directly): `bindings_store.cpp`, `bindings.hpp`, `CMakeLists.txt`, `esp32_idf_example/CMakeLists.txt`, `tests/store_test.cpp` — all integration points and patterns verified from source

### Secondary (MEDIUM confidence)

- Project requirements (`.planning/REQUIREMENTS.md`): "Per-key NVS storage" explicitly listed as out of scope; single JSON blob is correct
- `nvs_set_str` 4000-byte limit: Known ESP-IDF behavior per documentation; `nvs_set_blob` is the correct API for variable-length data including our 4096-byte buffer

### Tertiary (LOW confidence)

- None — all key claims are verified from official sources or direct codebase inspection.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — EM_JS and NVS blob API verified from official Emscripten and ESP-IDF v5.5 docs
- Architecture: HIGH — integration points identified from direct source inspection; platform guard pattern already established in codebase
- Pitfalls: HIGH — NVS init, linking, and EM_JS guard pitfalls verified from official docs; auto-persist concern verified from direct code reading

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable APIs — ESP-IDF v5.5 and Emscripten 3.1.73 are pinned versions)
