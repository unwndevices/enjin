---
phase: 57-qol-features
plan: 01
subsystem: scripting
tags: [lua, coroutines, tweens, async, bindings]

requires:
  - phase: 49-async-coroutines
    provides: CoroutineSlot pool, tickCoroutines, engine.async.wait/start/cancel
  - phase: 50-tween-animation
    provides: TweenSlot pool, tickTweens, engine.tween.to/cancel

provides:
  - CoroutineSlot with waitFrames and waitTweenId fields
  - engine.async.wait_frames(n) — suspends coroutine for exactly n frames
  - engine.tween.await(id) — suspends coroutine until tween completes
  - tickCoroutines: frame-first dual-mode check (frame before time), waitTweenId gate
  - tickTweens: coroutine resume scan before done_cb pcall

affects: [57-03-qol-test, any future coroutine-tween integration]

tech-stack:
  added: []
  patterns:
    - Frame-based coroutine suspension via waitFrames counter (decremented each tick)
    - Tween-awaiting coroutines gated by waitTweenId (resumed by tickTweens on completion)
    - Mutual exclusion between waitFrames and waitRemaining (setting one clears the other)
    - Inline coroutine slot clear in bindings_tween.cpp (clearSlot not cross-TU visible)

key-files:
  created: []
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings_async.cpp
    - src/scripting/bindings_tween.cpp

key-decisions:
  - "waitTweenId gate in tickCoroutines: skip frame/time check entirely for tween-awaiting slots — resume fires from tickTweens instead"
  - "Frame check precedes time check in tickCoroutines for mutual exclusion correctness"
  - "Coroutine resume in tickTweens before done_cb pcall to avoid yield-across-pcall boundary error"
  - "Inline slot clear in bindings_tween.cpp since clearSlot template is file-static to bindings_async.cpp"

requirements-completed: [QOL-01, QOL-02]

duration: 18min
completed: 2026-03-02
---

# Phase 57 Plan 01: Coroutine QoL APIs Summary

**engine.async.wait_frames(n) and engine.tween.await(id) — two new Lua APIs wired into CoroutineSlot pool and tickTweens coroutine resume**

## Performance

- **Duration:** 18 min
- **Started:** 2026-03-02T22:40:00Z
- **Completed:** 2026-03-02T22:58:00Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Extended `CoroutineSlot` with `waitFrames` (int) and `waitTweenId` (int) fields — zero alloc
- `engine.async.wait_frames(n)` suspends coroutine for exactly n frames; 0 or negative returns immediately
- `engine.tween.await(id)` suspends coroutine until tween completes; expired/invalid ID resumes immediately
- `tickCoroutines`: frame-first dual-mode check; `waitTweenId != 0` gate skips tween-awaiting slots
- `tickTweens`: coroutine resume scan fires before `done_cb` pcall, outside any pcall scope
- `engine.async.wait()` clears `waitFrames` for mutual exclusion; `wait_frames()` clears `waitRemaining`

## Task Commits

1. **Task 1: Extend CoroutineSlot and wire wait_frames into tickCoroutines** - `81cbe2e` (feat)
2. **Task 2: Implement tween.await binding and coroutine resume in tickTweens** - `aec1463` (feat)

## Files Created/Modified
- `include/enjin2/scripting/bindings.hpp` — CoroutineSlot +2 fields; +2 static method declarations
- `src/scripting/bindings_async.cpp` — clearSlot resets new fields; tickCoroutines dual-mode; wait_frames impl + registration
- `src/scripting/bindings_tween.cpp` — tween.await impl; tickTweens coroutine resume block; await registration

## Decisions Made
- `waitTweenId` gate in `tickCoroutines` skips the slot entirely — resume responsibility belongs to `tickTweens` to avoid double-resume risk
- Coroutine resume in `tickTweens` placed before `done_cb` pcall to avoid the "yield-across-pcall" boundary error (lua_resume cannot be called from within a pcall)
- Inline slot clear in `bindings_tween.cpp` (6 fields) because `clearSlot` template is `static` to `bindings_async.cpp` and not visible across translation units
- `waitFrames` decremented to 0 then resume fires on the same tick (not the next tick) — this matches "exactly n frames" semantics per QOL-02 spec

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness
- Plan 57-02 (camera dead zone) can proceed — no dependency on this plan's APIs
- Plan 57-03 (qol_test suite) depends on both Plans 01 and 02 being complete

---
*Phase: 57-qol-features*
*Completed: 2026-03-02*

## Self-Check: PASSED
- key-files modified exist on disk: bindings.hpp ✓, bindings_async.cpp ✓, bindings_tween.cpp ✓
- git commits present: 81cbe2e, aec1463 ✓
- No Self-Check: FAILED marker
