---
phase: 50-tween-helpers
plan: 01
subsystem: scripting
tags: [lua, bindings, tween, animation, easing, interpolation]

# Dependency graph
requires:
  - phase: 49-coroutine-async-scheduler
    provides: "CoroutineSlot pool pattern, clearSlot template, registerAsyncSubtable pattern, tickCoroutines/clearCoroutines wiring"
provides:
  - "TweenSlot[8] fixed pool with TweenEasing enum in LuaBindings"
  - "engine.tween.to/cancel/cancelAll Lua bindings"
  - "Four inline easing functions: linear, easeIn, easeOut, easeInOut (multiply/add only)"
  - "tickTweens(dt) per-frame advance integrated into SDL runner"
  - "clearTweens() cleanup on hot-reload and scene transition"
  - "bindings_tween.cpp as standalone TU following bindings_async.cpp pattern"
affects: [51-object-collection, 52-lua-api-polish]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "TweenSlot mirrors CoroutineSlot: fixed pool, LUA_NOREF guards, clearTweenSlot template"
    - "File-scope static template clearTweenSlot(Slot&, lua_State*) for private struct access from separate TU"
    - "TweenEasing enum cast to uint8_t for file-scope tweenEase() to avoid private-enum access error"
    - "lua_next iteration with TWEEN_MAX_PROPS guard: pop key+value before break to maintain stack balance"
    - "done_cb fired via lua_pcall before clearTweenSlot so refs are still valid at call time"

key-files:
  created:
    - src/scripting/bindings_tween.cpp
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings.cpp
    - src/scripting/bindings_engine.cpp
    - src/platform/sdl/sdl_main.cpp
    - CMakeLists.txt

key-decisions:
  - "TweenEasing enum is in private section of LuaBindings; file-scope tweenEase() uses uint8_t cast rather than LuaBindings::TweenEasing to avoid private-access error from separate TU"
  - "clearTweenSlot uses template<typename Slot> following clearSlot pattern from bindings_async.cpp — allows access to private TweenSlot from file-scope static in different TU"
  - "cancel leaves value at current interpolated position, does NOT snap to end, does NOT fire done_cb"
  - "tickTweens called AFTER tickCoroutines in SDL runner — consistent ordering with existing subsystems"

patterns-established:
  - "TweenEasing private enum: cast to underlying integer type (uint8_t) in file-scope helpers to avoid cross-TU access errors"
  - "Per-slot done_cb: check doneCbRef != LUA_NOREF, rawgeti, isfunction guard, pcall, log stderr errors, pop error string"

requirements-completed: [TWEEN-01, TWEEN-02, TWEEN-03]

# Metrics
duration: 4min
completed: 2026-03-01
---

# Phase 50 Plan 01: Tween Helpers Summary

**8-slot fixed tween pool with engine.tween.to/cancel/cancelAll, four multiply-add-only easing functions, SDL runner integration, and hot-reload/scene-change cleanup**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-03-01T22:23:19Z
- **Completed:** 2026-03-01T22:26:40Z
- **Tasks:** 2
- **Files modified:** 5 modified, 1 created

## Accomplishments

- TweenSlot[8] fixed pool with TweenEasing enum declared in LuaBindings private section (mirrors CoroutineSlot pattern exactly)
- bindings_tween.cpp: complete implementation of to/cancel/cancelAll bindings, tickTweens(dt) per-frame advance, clearTweens() cleanup, and registerTweenSubtable
- Four inline easing functions (linear, easeIn, easeOut, easeInOut) using only multiply/add — TWEEN-03 compliance verified
- tickTweens(dt) wired into SDL game loop after tickCoroutines(dt); clearTweens() called in registerAll() and setActiveScene() alongside clearCoroutines()
- engine.tween.* sub-table registered in registerEngineTable() after engine.async
- All 39 ctests pass with no regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Add TweenSlot struct and pool to bindings.hpp, create bindings_tween.cpp** - `ddf025b` (feat)
2. **Task 2: Wire tickTweens into SDL runner, add cleanup hooks, register sub-table** - `f00376e` (feat)

## Files Created/Modified

- `src/scripting/bindings_tween.cpp` - Complete tween pool implementation: clearTweenSlot, tweenEase, lua_engine_tween_to/cancel/cancelAll, tickTweens, clearTweens, registerTweenSubtable
- `include/enjin2/scripting/bindings.hpp` - TweenSlot struct, TWEEN_POOL_SIZE/TWEEN_MAX_PROPS/TWEEN_KEY_MAX, TweenEasing enum, m_tweenPool[]/m_nextTweenId members, tickTweens/clearTweens public methods, lua_engine_tween_* static declarations, registerTweenSubtable private method
- `src/scripting/bindings.cpp` - clearTweens() in registerAll() and setActiveScene()
- `src/scripting/bindings_engine.cpp` - registerTweenSubtable(L) in registerEngineTable()
- `src/platform/sdl/sdl_main.cpp` - tickTweens(dt) after tickCoroutines(dt)
- `CMakeLists.txt` - bindings_tween.cpp added to enjin2_lua sources

## Decisions Made

- **TweenEasing enum access:** The enum is in the private section of LuaBindings. The file-scope `tweenEase()` helper in bindings_tween.cpp cannot directly reference `LuaBindings::TweenEasing` from a separate translation unit. Fix: cast `slot.easing` to `uint8_t` at the call site and match against integer literals (0-3) in the switch. This is safe because enum underlying type is `uint8_t`.
- **cancel semantics:** cancel leaves tween at current interpolated position, does NOT snap to end value, does NOT fire done_cb. Matches documented plan spec.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed private TweenEasing enum access from file-scope helper**
- **Found during:** Task 1 (compile of bindings_tween.cpp)
- **Issue:** `tweenEase(float t, LuaBindings::TweenEasing)` — compiler error: `TweenEasing` is private within `LuaBindings`. File-scope static in a separate TU cannot access private enum types.
- **Fix:** Changed function signature to `tweenEase(float t, uint8_t easingCode)` with integer case labels (0-3) matching enum values; call site casts `slot.easing` to `uint8_t`. Zero behavior change.
- **Files modified:** src/scripting/bindings_tween.cpp
- **Verification:** enjin2_lua builds cleanly; all 39 tests pass
- **Committed in:** ddf025b (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 Rule 1 bug — compile error from private enum access)
**Impact on plan:** Required fix for compilability; zero behavioral impact.

## Issues Encountered

None beyond the auto-fixed deviation above.

## Next Phase Readiness

- engine.tween.* API fully available to Lua scripts: `engine.tween.to(target, {x=100, y=50}, 0.5, "easeOut", done_cb)`
- Pool is 8 slots; sufficient for typical UI and game object animations
- Phase 51 (ObjectCollection) and Phase 52 (Lua API polish) can use tweens immediately

---
*Phase: 50-tween-helpers*
*Completed: 2026-03-01*
