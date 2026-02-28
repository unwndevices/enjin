# Phase 34: Input Event Callbacks - Context

**Gathered:** 2026-02-27
**Status:** Ready for planning

<domain>
## Phase Boundary

Add `on_button_pressed(self, btn)` and `on_button_released(self, btn)` Lua callbacks that fire on button edge transitions — after input polling, before `update()` each frame. This phase covers the callbacks only; input polling (`engine.input.held`, etc.) already exists from Phase 31.

</domain>

<decisions>
## Implementation Decisions

### Dispatch scope
- All active C_LuaScript components in the scene receive input callbacks — `visible = false` does NOT suppress input events (matches how `update()` works)
- Only active Objects (enabled) receive callbacks; disabled Objects are skipped, consistent with existing lifecycle
- `LuaScriptSystem` (bindings_system.cpp) owns the dispatch loop — it iterates all registered scripts, checks input edge state, and fires input callbacks before calling `update()` on each script, satisfying INPUT-03's ordering requirement
- Per-button iteration: `on_button_pressed(self, btn)` fires once per button edge per active script — if 3 buttons pressed in one frame, each script's callback fires 3 times with the respective `btn` value (mirrors LÖVE2D's `keypressed(k)` model)

### Style and architecture
- Defer to existing codebase patterns and `project/lua-embedding-design.md` for all implementation specifics
- `btn` argument is an integer (matching existing `isButtonHeld(btn)` polling API)
- Callback signature: `self` (ScriptProxy userdata) as first arg, `btn` (integer) as second — matching `on_button_pressed(self, btn)`
- Both callbacks are optional — a script without them defined silently skips (all lifecycle callbacks are optional per design doc)
- Error handling follows `ScriptErrorPolicy` on `C_LuaScript` (same policy as `update`/`draw`)

### Claude's Discretion
- Exact method of iterating buttons (loop over enum range vs. cached edge list)
- How LuaScriptSystem receives or queries input state (direct `InputState*` ref vs. querying through engine bindings)
- Whether the button iteration happens inside `LuaScriptSystem::update()` or in a dedicated `dispatchInputEvents()` method

</decisions>

<specifics>
## Specific Ideas

No specific requirements beyond what's captured above — open to standard approaches consistent with the codebase.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 34-input-event-callbacks*
*Context gathered: 2026-02-27*
