---
phase: 57-qol-features
plan: 03
subsystem: tests
tags: [lua, coroutines, tweens, camera, integration-tests, qol]

requires:
  - phase: 57-01
    provides: engine.async.wait_frames, engine.tween.await, CoroutineSlot waitFrames/waitTweenId
  - phase: 57-02
    provides: engine.camera.setDeadZone, tickCameraFollow dead zone gate

provides:
  - tests/qol_test.cpp — 9 integration tests for QOL-01, QOL-02, QOL-03
  - qol_test registered in tests/CMakeLists.txt inside ENJIN2_BUILD_LUA guard

affects: [CI, ctest]

tech-stack:
  added: []
  patterns:
    - QoLFixture struct for coroutine/tween tests (LuaEngine + LuaBindings, tickBoth helper)
    - CamFixture struct for dead zone tests (Scene + Object + C_Camera + C_Position)
    - Lua globals as integers (0/1) not booleans — getGlobalNumber() returns 0 for booleans
    - tickBoth order: tickCoroutines THEN tickTweens (tween.await resume fires in tickTweens)
    - lerpSpeed=1.0 in camera tests for immediate snap (avoids needing camera->update(dt))

key-files:
  created:
    - tests/qol_test.cpp
  modified:
    - tests/CMakeLists.txt
    - src/scripting/bindings_async.cpp

key-decisions:
  - "Lua globals use integers (g_resumed = 0/1) not booleans — lua_isnumber returns false for booleans, making getGlobalNumber useless for boolean flags"
  - "camera dead zone tests use lerpSpeed=1.0 so lookAt snaps camera immediately without calling camera->update(dt)"
  - "wait_frames n-1 fix: slot.waitFrames = n-1 so the tick where wait_frames() is called counts as frame 1"
  - "bindings_async.cpp off-by-one bug found and fixed during test authoring: original slot.waitFrames = n gave n+1 ticks"

requirements-completed: [QOL-01, QOL-02, QOL-03]

duration: 25min
completed: 2026-03-02
---

# Phase 57 Plan 03: QoL Integration Test Suite Summary

**9 integration tests covering tween.await, wait_frames, and camera dead zone — 27 assertions, 0 failures**

## Performance

- **Duration:** 25 min
- **Started:** 2026-03-02T23:10:00Z
- **Completed:** 2026-03-02T23:35:00Z
- **Tasks:** 2
- **Files created:** 1
- **Files modified:** 2

## Accomplishments
- Created `tests/qol_test.cpp` with 9 test functions and 27 assertions covering all three QoL features
- Fixed `wait_frames` off-by-one bug in `bindings_async.cpp` (`n` → `n-1`) — found during test authoring
- Registered `qol_test` in `tests/CMakeLists.txt` inside `ENJIN2_BUILD_LUA` guard after `tween_test`
- All 9 tests pass; no regressions in coroutine_async_test (28), tween_test (53), camera_follow_test (40)

## Tests Written

### QOL-01: engine.tween.await(id)
1. **test_tween_await_suspends_and_resumes** — coroutine suspended mid-tween; resumes exactly when tween completes
2. **test_tween_await_invalid_id_resumes_immediately** — await(9999) with no matching tween returns immediately (no permanent suspension)
3. **test_tween_await_resumes_exactly_once** — two coroutines await the same tween; both resume on completion

### QOL-02: engine.async.wait_frames(n)
4. **test_wait_frames_yields_exactly_n** — wait_frames(3): false after tick 1, false after tick 2, true after tick 3
5. **test_wait_frames_zero_resumes_immediately** — wait_frames(0): no yield, resumes on first tick
6. **test_wait_frames_negative_resumes_immediately** — wait_frames(-5): no yield, resumes on first tick

### QOL-03: engine.camera.setDeadZone(w, h)
7. **test_dead_zone_freezes_camera** — target inside 20×20 dead zone (dx=5, dy=5 ≤ 10): camera frozen
8. **test_dead_zone_resumes_on_exit** — target outside dead zone (dx=50, dy=50 > 10): camera moves
9. **test_dead_zone_zero_disables** — setDeadZone(0,0): no freeze even with target at (5,5)

## Task Commits

1. **Task 1: Write qol_test.cpp with 9 integration tests** - `0fd7262` (test)
2. **Task 2: Register qol_test in CMakeLists and fix wait_frames n-1** - `cafa725` (build)

## Files Created/Modified
- `tests/qol_test.cpp` — 9 test functions, QoLFixture + CamFixture structs, 27 assertions
- `tests/CMakeLists.txt` — qol_test added inside ENJIN2_BUILD_LUA guard after tween_test
- `src/scripting/bindings_async.cpp` — wait_frames off-by-one fix (n → n-1) + comment explaining semantics

## Decisions Made
- Lua globals as integers (`g_resumed = 0`/`g_resumed = 1`) because `lua_isnumber` returns false for Lua booleans; `getGlobalNumber()` would always return 0.0 for a `true`/`false` value
- Camera dead zone tests use `lerpSpeed=1.0` so `lookAt()` snaps camera to position immediately — avoids needing the `camera->update(dt)` lerp path which would require additional ticks
- `tickBoth()` order is coroutines-first, tweens-second — tween.await resume fires inside `tickTweens`; coroutine must have already yielded (from a prior tickCoroutines call or the current tick's initial run)

## Deviations from Plan

- **wait_frames bug fix committed as part of Task 2** rather than a separate bugfix commit. The bug (off-by-one: `slot.waitFrames = n` gave n+1 total ticks) was discovered during test execution and fixed in the same session.

## Issues Encountered

1. **Lua boolean globals return 0 from getGlobalNumber()** — `getGlobalNumber("g_resumed")` uses `lua_isnumber` which returns false for Lua booleans. All test globals changed to integer flags (0/1).
2. **Camera doesn't move from lookAt() with speed < 1.0** — `C_Camera::lookAt(x, y, speed)` with speed < 1.0 only stores the target; actual lerp runs in `camera->update(dt)` which tests never call. Fix: use `lerpSpeed=1.0` for immediate snap.
3. **wait_frames off-by-one** — `slot.waitFrames = n` required n+1 ticks (1 initial + n decrements). Fix: `slot.waitFrames = n - 1` so the calling tick counts as frame 1, giving exactly n total ticks.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness
- Phase 57 is complete — all three plans done, all requirements satisfied
- All 148 assertions across 4 test suites pass

---
*Phase: 57-qol-features*
*Completed: 2026-03-02*

## Self-Check: PASSED
- key-files created/modified exist on disk: qol_test.cpp ✓, CMakeLists.txt ✓, bindings_async.cpp ✓
- git commits present: 0fd7262, cafa725 ✓
- 27 passed, 0 failed from qol_test binary ✓
- No Self-Check: FAILED marker
