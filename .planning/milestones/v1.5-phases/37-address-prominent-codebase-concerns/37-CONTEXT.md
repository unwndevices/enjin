# Phase 37: Address Prominent Codebase Concerns - Context

**Gathered:** 2026-02-27
**Status:** Ready for planning

<domain>
## Phase Boundary

Address the prominent concerns identified in `.planning/codebase/CONCERNS.md` — verifying completed work, fixing silent failures, and closing Lua API gaps. The scope is defined by the "Looks Done But Isn't" checklist (10 items) plus four concrete fixes identified during discussion. No new capabilities; this phase hardens what's already built.

</domain>

<decisions>
## Implementation Decisions

### Scope & prioritization
- Primary focus: all 10 "Looks Done But Isn't" checklist items from CONCERNS.md
- Every item addressed — not triaged. Each gets either a verification test or a code fix, not just documentation.
- Infrastructure items (clang-tidy CI integration, build directory cleanup): actually wire up / clean up — not just documented.
- ESP32-specific audits: skip. Desktop-first only. Zero-alloc integrity concern is noted in CONCERNS.md; no embedded profiling in this phase.

### ScriptProxy error experience
- Stale proxy access (after scene destruction) must raise a Lua error — not return nil silently.
- Error message: `"object has been destroyed"` — short and direct. No object name context needed.
- Both `__index` (reads) and `__newindex` (writes) on a stale proxy raise this error.
- Test: store `self` in a Lua global during `init()`, transition scenes, access the stored proxy — assert that a Lua error is raised (not nil, not crash).

### Component limit assertion
- `MAX_COMPONENTS = 16`: add `assert(componentCount < MAX_COMPONENTS)` in debug builds; `fprintf(stderr, ...)` + `return nullptr` in release builds.
- No silent nullptr return — overflow is always visible.
- No size increase (16 stays as-is).

### std::string → fixed buffer
- Convert `C_LuaScript::errorMessage` from `std::string` to `char errorMessage[256]`.
- `scriptCode` and `scriptPath` remain as `std::string` (acceptable for desktop, loaded once at startup).

### Lua tag bindings (completing Phase 29)
- Expose `self:addTag(tag)`, `self:hasTag(tag)`, `self:clearTags()` as ScriptProxy metamethods.
- C++ implementation exists; this is a bindings gap only.
- `self.name` remains read-only from Lua (Claude's discretion — mutating names that C++ lookup may rely on is risky).

### ObjectProxy for engine.scene.find()
- `engine.scene.find(name)` currently returns a raw `Object*` as lightuserdata — dangling pointer risk after scene transition.
- Wrap in a new `ObjectProxy` userdata with a `valid` flag (same pattern as ScriptProxy).
- ObjectProxy exposes full access: `name` (read), `hasTag(tag)`, `position` (read/write), component enable/disable control.
- Accessing a stale ObjectProxy raises the same `"object has been destroyed"` Lua error.

### clang-tidy CI enforcement
- Add a CMake `lint` target that runs clang-tidy against `src/**/*.cpp`.
- Integrate into CI so new warnings fail the build.
- Actually wired up and working — not just a TODO comment.

### Build directory cleanup
- Remove all non-`build/` build directories from the repository root (`build_21_off`, `build_21_on`, `build_22_*`, etc.).
- Keep only the main `build/` directory.
- Document any platform-specific build procedures if relevant.

### Claude's Discretion
- Whether `self.name` becomes writable from Lua (decision: keep read-only, rationale above).
- Exact implementation of clang-tidy CMake target and CI step format.
- ObjectProxy struct layout and GC finalization strategy.
- How to handle `engine.scene.findAllWithTag()` — if it returns a list of ObjectProxies, the approach should be consistent with `engine.scene.find()`.

</decisions>

<specifics>
## Specific Ideas

- ScriptProxy and ObjectProxy should share the same error message (`"object has been destroyed"`) for consistency.
- The "Looks Done But Isn't" checklist in CONCERNS.md is the literal task list — each checkbox should be closeable after this phase.
- clang-tidy lint target: prefer a CMake option flag (`-DCLANG_TIDY=ON`) so it doesn't run by default but can be wired into CI explicitly.

</specifics>

<deferred>
## Deferred Ideas

- None — discussion stayed within phase scope.

</deferred>

---

*Phase: 37-address-prominent-codebase-concerns*
*Context gathered: 2026-02-27*
