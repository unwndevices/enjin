# Phase 28: float dt Migration - Context

**Gathered:** 2026-02-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Change the delta time parameter from `uint16_t` milliseconds to `float` seconds throughout the entire C++ update chain — Object, Component, Scene, SceneStateMachine, and all subclasses. The conversion from platform ticks to float seconds happens once at the platform edge. Everything downstream receives `float dt` in seconds.

</domain>

<decisions>
## Implementation Decisions

### Parameter naming
- Use `dt` (not `deltaTime`) as the parameter name everywhere
- Applies to: `update(float dt)`, `lateUpdate(float dt)`, and all overrides
- Rename the UI system's existing `float deltaTime` to `float dt` for full consistency
- Migrate ALL engine update signatures — core chain (Object, Component, Scene, SceneStateMachine) plus PostFx, AnimationTrack, and any other engine code using `uint16_t deltaTime`

### Accumulated time variables
- Convert all internal time accumulators to float seconds (elapsed_time, sceneTime, lastUpdateTime, etc.)
- Animation frame durations become float seconds (e.g. 0.1f = 100ms per frame)
- Single conversion point: SDL/platform layer converts ticks to float seconds once. No `/1000` divisions downstream.

### Lua scripting bridge
- Lua API stays the same: `dt` and `time` variables remain in seconds (no breaking change for Lua scripts)
- Remove the `/1000.0` conversion in C_LuaScript::update() — dt is already seconds
- advanceAnimation binding switches from milliseconds to seconds for consistency
- No Lua script audit required — dt was already exposed in seconds on the Lua side

### Claude's Discretion
- Whether to cap dt at the platform edge (e.g. max 0.1f) to prevent physics explosions from frame spikes — currently some examples cap at 33ms
- Float vs double precision when passing dt to Lua (Lua numbers are natively double)

</decisions>

<specifics>
## Specific Ideas

- SDL main loop already computes `float dt = static_cast<float>(frame_start - prev_ticks) / 1000.0f` — this is the conversion source of truth
- UI system already uses `float deltaTime` — prior art in the codebase for float-based timing
- Examples in `examples/` directory are NOT in scope for this phase (user deselected example/demo scope)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 28-float-dt-migration*
*Context gathered: 2026-02-26*
