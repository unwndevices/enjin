# Phase 49: Coroutine/Async Scheduler - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Add engine.async.* Lua API backed by an 8-slot fixed coroutine pool in LuaBindings — supporting start/cancel/cancelAll and per-frame wait() yield — with the scheduler resumed from C each frame outside any pcall scope, and the coroutine library explicitly opened on ESP32.

</domain>

<decisions>
## Implementation Decisions

### Pool exhaustion policy
- 8 fixed coroutine slots
- When all slots are full, engine.async.start() returns nil instead of an ID
- Scripts that care can check: `local id = engine.async.start(fn); if not id then ... end`
- No Lua error raised — consistent with sprite pool, event bus, and LuaStore overflow behavior
- No introspection API (no slots() query) — keep API minimal

### Scene transition behavior
- Cancel all coroutines on scene transition (clearCoroutines())
- Same cleanup on hot-reload (F5)
- Persistent objects keep running but their async tasks do NOT survive transitions
- Clean slate per scene — prevents stale callback refs to destroyed objects

### Claude's Discretion
- CoroutineSlot struct layout and state machine (waiting, running, dead)
- Where in the SDL main loop to call tickCoroutines(dt) — before or after scene update
- wait() precision model (frame-accumulated dt)
- ESP32 luaopen_coroutine integration details
- Thread ref lifecycle (luaL_ref/luaL_unref management)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- setTimeState(dt, totalTime, frameCount) in bindings.hpp — provides dt for wait accumulation
- C_Timer pattern (include/enjin2/components/timer.hpp): Uses luaL_ref for callback storage — same pattern for coroutine thread refs
- LuaEventBus::clearHandlers() pattern — model for clearCoroutines() cleanup

### Established Patterns
- Fixed-slot arrays: sprite pool (16), event bus (16 channels / 8 subs), LuaStore (16 keys), ObjectCollection (128)
- Hot-reload cleanup: lua.shutdown() + lua.initialize() in sdl_main.cpp:100-128
- Component cleanup on reload: C_Timer::clearTimers(), C_StateMachine::clearStates(), m_eventBus.clearHandlers()

### Integration Points
- SDL runner main loop (sdl_main.cpp:251-271): dt computation and time state injection — tickCoroutines goes here
- performReload() in sdl_main.cpp:100-128: clearCoroutines() goes alongside existing cleanup
- LuaBindings class (bindings.hpp:357-800): owns the coroutine pool as member data
- lua_State* accessible via LuaBindings::getBindings(L) — needed for lua_newthread()

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 49-coroutine-async-scheduler*
*Context gathered: 2026-03-01*
