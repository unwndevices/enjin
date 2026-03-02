# Phase 54: JSON Serializer Refactor - Research

**Researched:** 2026-03-02
**Domain:** C++ buffer-based JSON serialization, embedded-safe allocation-free I/O
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**JSON formatting**
- `writeStoreToBuffer()` produces compact JSON (no whitespace, no indentation)
- Example: `{"hp":100,"name":"hero"}` not the multi-line format of `saveToFile()`
- "Same JSON as saveToFile" in the success criteria means logically equivalent (same data), not byte-for-byte identical
- `loadFromFile()` must parse compact output correctly — both formats are valid JSON

**Helper refactor scope**
- Rewrite `writeJsonEscaped()` and `writeSlotValue()` to work with char buffers, alongside the existing ostream versions
- No `std::ostringstream` wrapper — keep buffer writes allocation-free for embedded targets
- Ostream-based helpers remain for `saveToFile()` (SDL3 path is unchanged)

### Claude's Discretion
- Whether to define a `STORE_BUFFER_MAX` constant or let callers derive the limit from existing `STORE_MAX_*` constants
- Buffer overflow return convention (snprintf-style bytes-needed, -1, or size_t with truncation flag)
- Internal organization of buffer-writing helpers (private methods vs static functions)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| STORE-01 | `writeStoreToBuffer(char* out, size_t cap)` helper extracted from existing `saveToFile` for shared JSON serialization | Buffer-writing pattern from snprintf discipline; existing ostream helpers in `bindings_store.cpp` lines 113-163 provide exact logic to port |
</phase_requirements>

---

## Summary

Phase 54 is a pure internal refactor: extract a `writeStoreToBuffer(char* out, size_t cap)` function from the existing SDL3-only `saveToFile()` implementation in `/home/unwn/git/enjin/src/scripting/bindings_store.cpp`. The function must write compact JSON into a caller-supplied buffer without any heap allocation, making it usable from WASM and ESP32 backends (Phase 55) where `std::ofstream` is unavailable.

The scope is tightly bounded. All serialization logic already exists in three static helpers: `writeJsonEscaped()` (lines 113-126), `writeSlotValue()` (lines 128-163), and `saveToFile()` (lines 165-181). The task is to create buffer-writing counterparts to the ostream helpers, wire them into `writeStoreToBuffer()`, and declare `writeStoreToBuffer()` as a `public` method on `LuaStore`. The existing ostream helpers and `saveToFile()` are left untouched.

The `loadFromFile()` parser is already whitespace-tolerant (it calls `skipWhitespace()` throughout), so compact JSON output from `writeStoreToBuffer()` will round-trip correctly through `loadFromFile()` without any changes to the reader. The test infrastructure (`tests/store_test.cpp`) exists and uses the `StoreFixture` pattern — new tests for `writeStoreToBuffer()` can follow the same structure, particularly the `test_store_file_persistence()` pattern which exercises the full write/read cycle at the C++ level.

**Primary recommendation:** Port `writeJsonEscaped` and `writeSlotValue` to buffer-writing versions using `snprintf`-style cursor tracking (a `pos` offset into `out`, bounded by `cap`), then implement `writeStoreToBuffer` outside the `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` guard so all three platforms share it.

---

## Standard Stack

This phase uses no new libraries. It is pure C++17 with standard `<cstdio>` (`snprintf`) and `<cstring>` (`strncpy`, `strcmp`).

### Core

| Function | Header | Purpose | Why Used |
|----------|--------|---------|----------|
| `snprintf` | `<cstdio>` | Write formatted number/bool into buffer with bounds | Standard C; already included in `bindings_store.cpp`; works on ESP32 and WASM |
| `strlen`/`strncpy` | `<cstring>` | String operations for key/value copying | Already in use throughout `LuaStore` |

### No External Dependencies

No new CMake targets, no new header includes beyond what is already in `bindings_store.cpp` and `bindings.hpp`.

---

## Architecture Patterns

### Recommended Project Structure

The change is entirely self-contained within two files:

```
src/scripting/bindings_store.cpp   — add buffer-writing helpers + writeStoreToBuffer()
include/enjin2/scripting/bindings.hpp — add writeStoreToBuffer() declaration to LuaStore
```

### Pattern 1: snprintf-style Cursor Buffer Writing

**What:** Track a `size_t pos` cursor. Each write operation advances `pos` by the number of bytes written. Writes beyond `cap - 1` are silently skipped (truncated). Return `pos` (bytes that WOULD have been written, snprintf-style), or return `false`/`-1` on overflow.

**When to use:** Any time a function needs to write incrementally to a fixed-size buffer without allocation. This is the universal pattern for embedded JSON writers.

**Example (pattern for this phase):**
```cpp
// Buffer-writing helper (inside bindings_store.cpp, no platform guard needed)
static size_t bufWriteChar(char* out, size_t cap, size_t pos, char c) {
    if (pos + 1 < cap) out[pos] = c;
    return pos + 1;
}

static size_t bufWriteStr(char* out, size_t cap, size_t pos, const char* s) {
    while (*s) {
        if (pos + 1 < cap) out[pos] = *s;
        ++pos; ++s;
    }
    return pos;
}

static size_t bufWriteJsonEscaped(char* out, size_t cap, size_t pos, const char* s) {
    pos = bufWriteChar(out, cap, pos, '"');
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '"':  pos = bufWriteStr(out, cap, pos, "\\\""); break;
            case '\\': pos = bufWriteStr(out, cap, pos, "\\\\"); break;
            case '\n': pos = bufWriteStr(out, cap, pos, "\\n");  break;
            case '\r': pos = bufWriteStr(out, cap, pos, "\\r");  break;
            case '\t': pos = bufWriteStr(out, cap, pos, "\\t");  break;
            default:   pos = bufWriteChar(out, cap, pos, *p);   break;
        }
    }
    return bufWriteChar(out, cap, pos, '"');
}

// Returns bytes written (excluding null terminator). Caller checks pos < cap.
bool LuaStore::writeStoreToBuffer(char* out, size_t cap) const {
    size_t pos = 0;
    pos = bufWriteChar(out, cap, pos, '{');
    for (int i = 0; i < m_count; ++i) {
        if (i > 0) pos = bufWriteChar(out, cap, pos, ',');
        pos = bufWriteJsonEscaped(out, cap, pos, m_entries[i].key);
        pos = bufWriteChar(out, cap, pos, ':');
        pos = bufWriteSlotValue(out, cap, pos, m_entries[i]);
    }
    pos = bufWriteChar(out, cap, pos, '}');
    if (pos < cap) { out[pos] = '\0'; return true; }
    if (cap > 0) out[cap - 1] = '\0';
    return false;  // truncation occurred
}
```

**Source:** Standard C embedded JSON serialization pattern. snprintf convention is documented in ISO C99 §7.19.6.5.

### Pattern 2: No Platform Guard on writeStoreToBuffer

**What:** The new `writeStoreToBuffer()` must NOT be wrapped in `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)`. It is the shared implementation that all three platforms will use.

**Why:** The existing `#if` guard applies only to ostream-based `saveToFile`/`loadFromFile` (which use `<fstream>` and `<sstream>`). `writeStoreToBuffer()` uses only `<cstdio>` and `<cstring>`, which are available everywhere. Placing it outside the guard is what enables Phase 55.

**Example placement in bindings_store.cpp:**
```cpp
// BEFORE the platform guard — shared by all platforms
bool LuaStore::writeStoreToBuffer(char* out, size_t cap) const { ... }

#if !defined(ESP32) && !defined(__EMSCRIPTEN__)
// Existing ostream helpers and saveToFile/loadFromFile stay here, unchanged
static void writeJsonEscaped(std::ofstream& out, const char* s) { ... }
...
bool LuaStore::saveToFile(const char* path) const { ... }
bool LuaStore::loadFromFile(const char* path) { ... }
#else
bool LuaStore::saveToFile(const char*) const { return false; }
bool LuaStore::loadFromFile(const char*) { return false; }
#endif
```

### Pattern 3: Number Serialization with snprintf

**What:** The `writeSlotValue` ostream version uses `out << slot.numVal` for numbers. The buffer version must use `snprintf`.

**Example:**
```cpp
// Number serialization in buffer version
case LuaStore::StoreType::Number: {
    char numBuf[64];
    int n = snprintf(numBuf, sizeof(numBuf), "%g", slot.numVal);
    for (int j = 0; j < n; ++j)
        pos = bufWriteChar(out, cap, pos, numBuf[j]);
    break;
}
```

Note: `%g` format trims trailing zeros (e.g., `100` not `100.000000`). The existing `out << slot.numVal` uses C++ stream default which also trims trailing zeros. Both produce equivalent output that `strtod` (used in `readJsonValue`) can parse back correctly.

### Anti-Patterns to Avoid

- **Using `std::ostringstream` as intermediate:** Explicitly forbidden by locked decisions. Adds heap allocation and defeats the embedded-safe requirement.
- **Modifying `saveToFile` to call `writeStoreToBuffer` then write to file:** Not required by success criteria. `saveToFile` should remain independent. Phase 55 callers will use `writeStoreToBuffer` directly.
- **Wrapping in the platform guard:** The buffer function must NOT be inside `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)`.
- **Forgetting null terminator on truncation:** When `pos >= cap`, write `out[cap-1] = '\0'` before returning false.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Number formatting | Custom float-to-string | `snprintf(buf, N, "%g", val)` | `%g` handles int-like doubles cleanly (100 not 100.000000); already available everywhere |
| JSON string escaping | Ad-hoc per-char switch | Port existing `writeJsonEscaped` switch verbatim | The logic is already correct and tested via `saveToFile` round-trips |

**Key insight:** The ostream helpers are the reference implementation. The buffer helpers are a mechanical translation of the same logic from `out <<` to `bufWriteChar/bufWriteStr`.

---

## Common Pitfalls

### Pitfall 1: Buffer Size — The Absolute Worst Case Is Large

**What goes wrong:** A 4KB buffer (`4096` bytes) is safe for typical game saves, but the absolute theoretical maximum (all 16 keys are tables, all 16 table entries have 64-char max-escaped keys and 128-char max-escaped string values) is ~100KB.

**Why it happens:** `STORE_MAX_KEYS=16`, `STORE_MAX_TABLE_ENTRIES=16`, `STORE_MAX_STRING=128`, all with worst-case JSON escaping (every character escaped = 2x expansion).

**How to avoid:** The buffer size is the CALLER's responsibility (Phase 55 decides the size). `writeStoreToBuffer` returns `false` on truncation, which is the correct response. The planner should document the realistic bound: with typical 8-16 char keys and 8-32 char string values, output is under 1KB. A 4096-byte buffer covers practical use; 16384 covers near-absolute-worst.

**Warning signs:** Tests that use small buffers (e.g., 256 bytes) with maximum-length string values will trigger the overflow return path.

### Pitfall 2: `%g` vs Stream Number Formatting

**What goes wrong:** `snprintf(buf, N, "%g", 100.0)` produces `"100"` (correct). `snprintf(buf, N, "%g", 3.14159)` produces `"3.14159"` (correct). But `snprintf(buf, N, "%g", 1e-10)` produces `"1e-10"` (scientific notation). The existing stream `out << slot.numVal` also uses C++ default which uses scientific notation for very large/small values.

**Why it happens:** `%g` uses scientific notation when the exponent is < -4 or >= precision (default 6). This is the same behavior as C++ stream output.

**How to avoid:** This is not a bug — the existing `loadFromFile` parser uses `strtod` which correctly parses scientific notation. No special handling needed.

### Pitfall 3: Boolean Serialization

**What goes wrong:** The ostream version writes `"true"` or `"false"` via `out << (slot.boolVal ? "true" : "false")`. The buffer version must do the same with `bufWriteStr`.

**How to avoid:** Direct port of the switch case. No pitfall if ported faithfully.

### Pitfall 4: `saveToFile` Compact vs Pretty Discrepancy

**What goes wrong:** The existing `saveToFile()` writes pretty JSON with `"\n  "` before each key and `" "` after the colon. `writeStoreToBuffer()` writes compact JSON. The success criterion says "logically equivalent" not byte-for-byte identical.

**Why it happens:** The CONTEXT.md explicitly resolved this: compact output is valid input to `loadFromFile()` because `skipWhitespace()` handles both forms.

**Warning signs:** A test that does `strcmp(fileContents, bufferContents)` byte-for-byte will fail by design. Tests should compare round-trip deserialized values, not raw strings.

---

## Code Examples

### writeStoreToBuffer Declaration (bindings.hpp)

Add to `LuaStore` public interface, after `loadFromFile`:

```cpp
/** @brief Serialise the store to a caller-supplied buffer as compact JSON.
 *  No heap allocation — safe for WASM and ESP32 callers.
 *  @param out  Buffer to write into (null-terminated on success or truncation)
 *  @param cap  Buffer capacity in bytes (including null terminator)
 *  @return true if the entire store fit in the buffer, false if truncated */
bool writeStoreToBuffer(char* out, size_t cap) const;
```

### Test Pattern (extending store_test.cpp)

Follow the `test_store_file_persistence` pattern:

```cpp
static void test_write_store_to_buffer_basic() {
    printf("--- writeStoreToBuffer basic round-trip ---\n");

    LuaStore store;
    store.setNumber("score", 1000);
    store.setString("player", "Alice");
    store.setBool("done", true);

    char buf[4096];
    bool ok = store.writeStoreToBuffer(buf, sizeof(buf));
    ASSERT(ok, "writeStoreToBuffer should return true when buffer is large enough");

    // Round-trip: load the compact JSON back
    LuaStore store2;
    bool loaded = store2.loadFromBuffer(buf);  // reuses existing loadFromFile parser path
    ASSERT(loaded, "loadFromFile should parse compact JSON output");
    // ... verify values
}
```

Note: `loadFromFile` reads from disk. To test the round-trip without disk I/O, create a thin `loadFromBuffer(const char*)` or directly call `loadFromFile` with a temp file. The existing `test_store_file_persistence` pattern (write to `/tmp/enjin_store_test.json`) is the simplest approach and matches project conventions.

### Compact JSON Output Examples

```
Empty store:      {}
Number:           {"score":1000}
String:           {"player":"Alice"}
Bool:             {"done":true}
Table:            {"stats":{"kills":42,"deaths":3}}
Mixed:            {"score":1000,"player":"Alice","done":true}
Escaped string:   {"msg":"line1\nline2"}
```

---

## State of the Art

This phase uses no external libraries, so there is no "state of the art" in the library ecosystem sense. The embedded-safe buffer-writing pattern used here is the standard approach in embedded C/C++ JSON serializers (JSMN, ArduinoJson static buffers, etc.).

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `std::ofstream`-only JSON serialization | `writeStoreToBuffer` + `saveToFile` coexist | Phase 54 | Phase 55 can implement WASM localStorage and ESP32 NVS backends |

---

## Open Questions

1. **Should `writeStoreToBuffer` be declared in the header outside any platform guard?**
   - What we know: `bindings.hpp` currently has no platform guards. `LuaStore` is declared unconditionally. The platform guards are only in `bindings_store.cpp`.
   - What's unclear: Whether Phase 55's WASM/ESP32 platform code needs to call `writeStoreToBuffer` via the same `LuaStore` object, or via a separate interface.
   - Recommendation: Declare in the header unconditionally (no guard). The implementation in `.cpp` also has no guard. Phase 55 callers include the same header.

2. **Buffer overflow return convention: `bool` vs `int` (bytes needed)?**
   - What we know: CONTEXT.md marks this as Claude's discretion.
   - Recommendation: Use `bool` return (true = success, false = truncation). Reasons: (1) existing `saveToFile` returns `bool`; (2) ESP32/WASM callers just need to know if the write succeeded; (3) the "bytes needed" snprintf convention is most useful when callers can retry with a larger buffer — but for embedded targets, the buffer size is fixed at compile time. Keep it consistent with existing API.

3. **`STORE_BUFFER_MAX` constant or not?**
   - What we know: CONTEXT.md marks this as Claude's discretion. The absolute worst case is ~100KB (impractical for ESP32 stack). Practical case is under 1KB.
   - Recommendation: Define `STORE_BUFFER_MAX = 4096` as a `static constexpr` in `LuaStore`. Phase 55 callers use it to declare their char arrays. This documents the "safe" size without forcing every caller to re-derive it. Comment it clearly: "sufficient for typical game saves; theoretical maximum with all maximum-length escaped table values is ~100KB."

---

## Validation Architecture

`workflow.nyquist_validation` is not present in `.planning/config.json` (the key does not exist), so this section is included using the test infrastructure that was discovered.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Custom C++ test runner (ASSERT macro, pass/fail counters) |
| Config file | `tests/CMakeLists.txt` — `add_test(NAME store_test COMMAND store_test)` |
| Quick run command | `cd /home/unwn/git/enjin && cmake -B build/tests -DENJIN2_BUILD_TESTS=ON -DENJIN2_BUILD_LUA=ON && cmake --build build/tests --target store_test && ./build/tests/tests/store_test` |
| Full suite command | `cd /home/unwn/git/enjin && cmake --build build/tests && ctest --test-dir build/tests` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | File Exists? |
|--------|----------|-----------|-------------|
| STORE-01 | `writeStoreToBuffer()` exists and produces valid compact JSON | unit (C++) | Partial — `tests/store_test.cpp` exists but has no `writeStoreToBuffer` tests yet |
| STORE-01 | `loadFromFile()` parses compact output (round-trip) | unit (C++) | Partial — `test_store_file_persistence` covers the concept but uses pretty JSON from `saveToFile` |
| STORE-01 | Existing SDL3 `saveToFile`/`loadFromFile` behavior unchanged | regression | `test_store_file_persistence` already covers this |

### Sampling Rate

- **Per task commit:** Run `store_test` executable
- **Per wave merge:** `ctest --test-dir build/tests -R store`
- **Phase gate:** All store tests green

### Wave 0 Gaps

- `tests/store_test.cpp` needs a `test_write_store_to_buffer_*` section — does NOT exist yet
  - `test_write_store_to_buffer_compact_output` — verify compact JSON string
  - `test_write_store_to_buffer_round_trip` — write buffer, parse back, check values
  - `test_write_store_to_buffer_overflow` — buffer too small returns false, null-terminated
  - `test_write_store_to_buffer_empty_store` — `{}` output
  - `test_write_store_to_buffer_table` — table value serialized correctly
- `bindings.hpp` needs `writeStoreToBuffer` declaration — does NOT exist yet
- `bindings_store.cpp` needs buffer-writing helpers and `writeStoreToBuffer` — does NOT exist yet

---

## Sources

### Primary (HIGH confidence)

- Direct source inspection: `/home/unwn/git/enjin/src/scripting/bindings_store.cpp` — full implementation read, lines 110-320
- Direct source inspection: `/home/unwn/git/enjin/include/enjin2/scripting/bindings.hpp` — `LuaStore` class declaration, lines 264-349
- Direct source inspection: `/home/unwn/git/enjin/tests/store_test.cpp` — existing test patterns, all tests read
- Direct source inspection: `/home/unwn/git/enjin/tests/CMakeLists.txt` — test build infrastructure confirmed
- Direct source inspection: `/home/unwn/git/enjin/.planning/phases/54-json-serializer-refactor/54-CONTEXT.md` — locked decisions and phase boundary

### Secondary (MEDIUM confidence)

- ISO C99 §7.19.6.5 (`snprintf` spec): `%g` format behavior for floats — consistent with C++ stream default; used as basis for number serialization pattern

### Tertiary (LOW confidence)

- None

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; only `<cstdio>` snprintf which is universally available
- Architecture: HIGH — the exact logic to port is visible in the source; pattern is mechanical translation from ostream to char buffer
- Pitfalls: HIGH — all derived from reading actual source code (buffer size bounds, pretty vs compact, number formatting)

**Research date:** 2026-03-02
**Valid until:** Stable — no external dependencies; valid indefinitely unless `LuaStore` internals change
