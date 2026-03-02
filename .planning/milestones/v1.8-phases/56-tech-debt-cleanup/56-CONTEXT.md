# Phase 56: Tech Debt Cleanup - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Two targeted correctness fixes: (1) clear the camera follow proxy on scene change and hot reload so no stale reference remains, (2) emit a Lua warning when `engine.scene.persist()` is called outside SceneStateMachine context instead of silently returning nil.

</domain>

<decisions>
## Implementation Decisions

### No user decisions required
This phase has precise success criteria that fully define both fixes. Implementation follows directly from the codebase patterns found during scouting. Claude has full discretion on approach.

### Claude's Discretion
- Camera fix: Clear `m_followTargetProxy = nullptr` in `setActiveScene()` alongside the existing `m_activeCamera = nullptr`, `clearCoroutines()`, `clearTweens()` pattern (bindings.cpp:717). Also check if hot reload goes through a separate path that needs the same clear.
- Warning: Use `lua_warning()` (Lua 5.4 C API) per success criteria, or fall back to the existing `printf()` pattern if the Lua version doesn't support it — planner decides after checking the Lua version in use.
- Warning message wording for the persist() no-SSM case.

</decisions>

<specifics>
## Specific Ideas

No specific requirements — success criteria are the full spec.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `setActiveScene()` (bindings.cpp:708–720): Already clears `m_activeCamera`, coroutines, tweens on scene change — `m_followTargetProxy = nullptr` goes in the same block
- `tickCameraFollow()` (bindings_engine.cpp:1021–1039): Already handles destroyed proxies at runtime (`valid` check) — complements the scene-change clear
- `lua_engine_scene_persist()` (bindings_engine.cpp:407–432): The silent `lua_pushnil` at line 420–422 is where the warning should be added before returning

### Established Patterns
- Scene-change cleanup block in `setActiveScene()`: Pattern is clear — add the proxy clear alongside existing clears
- Existing logging: `printf()` used throughout; `lua_warning()` is the Lua 5.4 API function — check Lua version before choosing

### Integration Points
- `bindings.cpp:717`: Camera proxy clear (one line)
- `bindings_engine.cpp:420–422`: Replace silent `lua_pushnil` with warning + nil return for the no-SSM path

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 56-tech-debt-cleanup*
*Context gathered: 2026-03-02*
