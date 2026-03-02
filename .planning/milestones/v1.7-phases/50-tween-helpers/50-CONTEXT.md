# Phase 50: Tween Helpers - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Add engine.tween.* Lua API backed by an 8-slot TweenSlot fixed array in LuaBindings — animating Lua table fields over time via four inline easing functions — with the pool ticked from C each frame and clearTweens() called on hot reload.

</domain>

<decisions>
## Implementation Decisions

### Pool exhaustion policy
- 8 fixed tween slots
- When all slots are full, engine.tween.to() returns nil instead of an ID
- Scripts that care can check: `local id = engine.tween.to(...); if not id then ... end`
- No Lua error raised — consistent with coroutine pool and all other fixed pools in codebase
- No introspection API — keep API minimal

### Scene transition behavior
- Cancel all tweens on scene transition (clearTweens())
- Same cleanup on hot-reload (F5)
- Clean slate per scene — prevents stale Lua refs to destroyed objects/tables

### Claude's Discretion
- TweenSlot struct layout (target ref, property keys, start/end values, elapsed, duration, easing fn, done_cb ref)
- How to read/write Lua table fields from C (lua_getfield/lua_setfield by string key)
- Easing function implementation (quadratic for easeIn/easeOut, cubic or Hermite for easeInOut)
- done_cb argument contract (what args callback receives)
- cancel-mid-tween behavior (snap to current value vs snap to target)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- math.hpp lerp<T>(a, b, t) (lines 59-69): Template lerp for interpolation
- math.hpp smoothstep(edge0, edge1, x) (lines 96-105): Hermite 3rd-order — could inform easeInOut
- C_Camera lerp pattern (camera.cpp:45-70): Example of dt-scaled interpolation
- SpriteState accumSec pattern (bindings.hpp:383-393): Frame-based accumulation for animation timing

### Established Patterns
- Fixed-slot arrays with silent nil on overflow
- luaL_ref / luaL_unref for Lua callback lifecycle
- dt flow: setTimeState(dt, totalTime, frameCount) each frame
- Hot-reload cleanup pattern: clear all refs, reset all slots

### Integration Points
- SDL runner main loop: tickTweens(dt) goes after scene update, before render
- performReload(): clearTweens() alongside clearCoroutines()
- LuaBindings class: owns tween pool as member data
- engine.tween.* sub-table registered in bindings_engine.cpp

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 50-tween-helpers*
*Context gathered: 2026-03-01*
