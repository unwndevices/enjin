# Phase 54: JSON Serializer Refactor - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Extract a shared `writeStoreToBuffer(char* out, size_t cap)` helper from the existing SDL3 `saveToFile()` implementation. This unlocks WASM and ESP32 storage backends (Phase 55) which cannot use `std::ofstream`. No runtime behavior changes — SDL3 `engine.store.flush/load` behavior must remain identical after the refactor.

</domain>

<decisions>
## Implementation Decisions

### JSON formatting
- `writeStoreToBuffer()` produces **compact JSON** (no whitespace, no indentation)
- Example: `{"hp":100,"name":"hero"}` not the multi-line format of `saveToFile()`
- "Same JSON as saveToFile" in the success criteria means logically equivalent (same data), not byte-for-byte identical
- `loadFromFile()` must parse compact output correctly — both formats are valid JSON

### Helper refactor scope
- Rewrite `writeJsonEscaped()` and `writeSlotValue()` to work with char buffers, alongside the existing ostream versions
- No `std::ostringstream` wrapper — keep buffer writes allocation-free for embedded targets
- Ostream-based helpers remain for `saveToFile()` (SDL3 path is unchanged)

### Claude's Discretion
- Whether to define a `STORE_BUFFER_MAX` constant or let callers derive the limit from existing `STORE_MAX_*` constants
- Buffer overflow return convention (snprintf-style bytes-needed, -1, or size_t with truncation flag)
- Internal organization of buffer-writing helpers (private methods vs static functions)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `writeJsonEscaped(std::ostream&, const char*)` (bindings_store.cpp:113–126): Handles `"`, `\`, `\n`, `\r`, `\t` — logic can be ported to buffer version
- `writeSlotValue(std::ostream&, const StoreSlot&)` (bindings_store.cpp:128–163): Recursive slot serializer for Number/String/Bool/Table — same logic needed for buffer version
- `saveToFile()` (bindings_store.cpp:165–181): Current SDL3 stream-based serializer — reference implementation for correctness

### Established Patterns
- Platform conditionals via `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` — buffer function goes in the shared (non-platform-specific) section
- Fixed-capacity arrays throughout (`STORE_MAX_KEYS=16`, `STORE_MAX_STRING=128`, `STORE_MAX_TABLE_ENTRIES=16`) — max JSON output is bounded and predictable (~4KB)

### Integration Points
- `bindings_store.cpp`: Where the new buffer helpers and `writeStoreToBuffer()` live
- `saveToFile()` refactor target: should call `writeStoreToBuffer()` internally or remain independent (planner decides)
- Phase 55 will call `writeStoreToBuffer()` from WASM and ESP32 platform implementations

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 54-json-serializer-refactor*
*Context gathered: 2026-03-02*
