# Phase 57: QoL Features - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Three additive QoL features: (1) `engine.tween.await(id)` suspends a coroutine until a tween completes, (2) `engine.async.wait_frames(n)` yields a coroutine for exactly N frames, (3) `engine.camera.setDeadZone(w, h)` adds a rectangular dead zone where the camera freezes while the followed target is inside it. No changes to existing tween, async, or camera follow behavior.

</domain>

<decisions>
## Implementation Decisions

### Camera dead zone
- Dead zone is a **rectangle centered on the camera's current position** (not anchored to target)
- While the target is inside the dead zone: camera **freezes completely** — no lerp target update
- When target exits the dead zone: camera **resumes immediately at normal follow speed** — no ease-in
- Shape is rectangle only (width × height), matching the `setDeadZone(w, h)` signature

### Claude's Discretion
- `tween.await()` with invalid/expired ID: Claude decides whether to resume immediately or error — success criteria only specifies the happy path ("suspends until tween completes, then resumes exactly once")
- `wait_frames(n)` edge cases: n=0, n<0 behavior
- Whether dead zone state is stored on `LuaBindings` alongside `m_followTargetProxy`, or on `C_Camera`
- Dead zone persistence across scene changes (whether it's cleared with `m_followTargetProxy`)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `CoroutineSlot` (bindings.hpp:448–457): 8-slot pool with `waitRemaining` float — `wait_frames()` needs a parallel frame-count mechanism (int instead of float, or dual field)
- `TweenSlot.doneCbRef` (bindings.hpp:459–480): Existing done-callback hook — `tween.await()` can piggyback on this completion signal
- `tickCameraFollow()` (bindings_engine.cpp:1021–1039): Single function where the dead zone check naturally goes — before `cam->lookAt()` is called
- `engine.async.wait(seconds)` (bindings_async.cpp:95–126): Existing yield/resume pattern — `wait_frames()` follows the same shape

### Established Patterns
- Fixed 8-slot pools for both tweens and coroutines — new features must work within existing capacity limits
- Monotonically increasing integer IDs for tweens and coroutines
- `clearCoroutines()` / `clearTweens()` called on scene transition — dead zone state should also be cleared if it's stored on `LuaBindings`
- Lua 5.4 vs LuaJIT 5.1 `lua_resume()` compat block already exists in `tickCoroutines()` — reuse for any new resume calls

### Integration Points
- `tickTweens()` (bindings_tween.cpp:196–248): Completion check at `t >= 1.0` is where awaiting coroutines get resumed
- `tickCoroutines()` (bindings_async.cpp:164–217): Frame-based yield tracks a counter here instead of decrementing a float
- `tickCameraFollow()` (bindings_engine.cpp:1021–1039): Dead zone distance check before `cam->lookAt()` call
- `LuaBindings` follow state fields (bindings.hpp:436–438): Dead zone width/height stored here alongside `m_followTargetProxy`

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 57-qol-features*
*Context gathered: 2026-03-02*
