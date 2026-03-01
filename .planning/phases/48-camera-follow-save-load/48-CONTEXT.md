# Phase 48: Camera Follow + Save/Load - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver two independent features: (1) engine.camera.follow/stopFollow bindings that track a named object per-frame via C_Camera, and (2) LuaStore SDL3 JSON I/O by replacing the VCV_RACK preprocessor guard with correct platform branching — including engine.store.flush() and engine.store.path() for explicit save control.

</domain>

<decisions>
## Implementation Decisions

### Pool/capacity policy
- Not directly pool-based, but follow target uses a single slot (one follow target at a time)
- If follow target proxy becomes invalid (object destroyed), silently stop following — no Lua error
- Consistent with codebase pattern: silent failure on invalid state

### Claude's Discretion
- Camera follow implementation approach (flag on C_Camera vs per-frame lookAt wrapper)
- Default lerp speed for follow
- Store default file path and naming
- Whether flush() changes auto-save behavior or supplements it
- VCV_RACK guard replacement strategy (SDL3 platform detection)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- C_Camera (include/enjin2/components/camera.hpp): Has lookAt(x, y, lerpSpeed) with smooth lerp — follow can wrap this with per-frame target resolution
- LuaStore (include/enjin2/scripting/bindings.hpp:264-349): Full JSON read/write with auto-persist when m_storePath is set
- bindings_engine.cpp: engine.camera.* sub-table already registered with 6 functions — follow/stopFollow extend this
- bindings_store.cpp: engine.store.* sub-table has save/load/exists/delete/clear — flush/path extend this
- getActiveCamera() helper (bindings_engine.cpp:838-841): Already resolves active camera from LuaBindings

### Established Patterns
- Sub-table binding: lua_newtable + luaBindFunctions + lua_setfield pattern
- LuaFuncDef arrays for each sub-table
- VCV_RACK guard at bindings_store.cpp:110-320 controls file I/O

### Integration Points
- engine.camera.* sub-table in bindings_engine.cpp (lines 131-142)
- engine.store.* sub-table in bindings_engine.cpp (lines 101-111)
- C_Camera::update(dt) in camera.cpp — where follow logic would execute per-frame
- Hot-reload path in sdl_main.cpp — stopFollow should be called on reload

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 48-camera-follow-save-load*
*Context gathered: 2026-03-01*
