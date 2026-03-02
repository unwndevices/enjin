---
phase: 44-2d-camera-system
plan: 01
subsystem: camera
tags: [c++, camera, drawable, scene, lerp, shake, bounds, rendering]

requires:
  - phase: 43-tilemap-system
    provides: Tilemap component pattern (C_Tilemap), proxy pattern (CTILEMAP_PROXY_CHECK)
  - phase: 36-drawable-decoupling
    provides: C_Drawable base class with getComponents<C_Drawable> and buffer_index sort

provides:
  - C_Camera component with setPosition, lookAt (lerp follow), shake (sin decay), setBounds/clearBounds, getScreenOffset
  - C_Drawable::drawWithOffset(canvas, offset) virtual method for camera-aware rendering
  - C_Drawable::m_screenSpace flag — HUD/UI elements opt out of camera offset
  - Scene::renderObjects() camera-aware render pipeline using drawWithOffset
  - camera_test.cpp with 11 assertions covering CAM-01 through CAM-06

affects: [44-2d-camera-system-plan-02, lua-bindings, rendering-pipeline]

tech-stack:
  added: []
  patterns:
    - "Camera offset convention: camera pos = top-left world point; screenOffset = -(pos + shakeOffset)"
    - "Screen-space opt-out: m_screenSpace flag on C_Drawable; drawWithOffset skips offset for UI"
    - "Shake: sin oscillation with linear decay; elapsed incremented first so sin(0) is avoided"
    - "Lerp: factor = min(lerpSpeed * dt * 10, 1.0); lerpSpeed>=1.0 snaps via lookAt"
    - "Scene camera detection: objects.forEach finds first active C_Camera; Point(0,0) fallback"

key-files:
  created:
    - include/enjin2/components/camera.hpp
    - src/components/camera.cpp
    - tests/camera_test.cpp
  modified:
    - include/enjin2/components/drawable.hpp
    - include/enjin2/core/scene.hpp
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Camera position convention: top-left screen corner in world space (screen_pos = world_pos - cam_pos), matching tilemap scroll semantics"
  - "Shake elapsed incremented before computing sin offset — avoids sin(0)=0 on first frame bug"
  - "drawWithOffset saves/restores anchor_offset rather than computing a separate draw position — no interface change to draw() subclasses"
  - "camera.cpp added to enjin2_lua STATIC target (same as timer.cpp, state_machine.cpp) — Lua Plan 02 requires it in that target"
  - "scene.hpp includes camera.hpp directly — acceptable since scene is already a template-heavy header; camera.hpp is lightweight"

patterns-established:
  - "Camera offset sign: getScreenOffset() returns negative camera position — adding to anchor_offset achieves world_pos - cam_pos"
  - "drawWithOffset pattern: save anchor_offset, apply offset, call draw(), restore — zero overhead for screen-space drawables"

requirements-completed: [CAM-01, CAM-02, CAM-03, CAM-04, CAM-05, CAM-06]

duration: 4min
completed: 2026-02-28
---

# Phase 44 Plan 01: 2D Camera System C++ Foundation Summary

**C_Camera with lerp follow, sin-decay shake, and bounds clamping integrated into Scene::renderObjects() via C_Drawable::drawWithOffset() — screen-space drawables skip camera offset**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-28T22:30:11Z
- **Completed:** 2026-02-28T22:34:12Z
- **Tasks:** 2 (Task 1: implementation + Task 2: test suite, per TDD RED/GREEN cycle)
- **Files created/modified:** 7

## Accomplishments

- C_Camera component: setPosition (instant), lookAt (lerp or snap), shake (sin oscillation + decay), setBounds/clearBounds, getScreenOffset
- C_Drawable extended with m_screenSpace flag and drawWithOffset virtual method — backward compatible (existing draw() subclasses unchanged)
- Scene::renderObjects() updated to find first active C_Camera and pass offset to drawWithOffset() — world-space drawables shift automatically, screen-space drawables skip offset
- 11-test camera_test.cpp covering all CAM-01 through CAM-06 requirements — all pass

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): Failing camera tests** - `e988f4e` (test)
2. **Task 2 (GREEN): C_Camera implementation + pipeline** - `273de0e` (feat)

_Note: TDD tasks have two commits (test RED → implementation GREEN)_

## Files Created/Modified

- `include/enjin2/components/camera.hpp` - C_Camera class declaration with all public API
- `src/components/camera.cpp` - Full implementation: lerp, shake with sin decay, bounds clamping
- `include/enjin2/components/drawable.hpp` - Added m_screenSpace, setScreenSpace, isScreenSpace, drawWithOffset virtual
- `include/enjin2/core/scene.hpp` - Added camera.hpp include, renderObjects() uses drawWithOffset with camOffset
- `CMakeLists.txt` - Added src/components/camera.cpp to enjin2_lua STATIC sources
- `tests/camera_test.cpp` - 11 test functions covering CAM-01..CAM-06
- `tests/CMakeLists.txt` - camera_test executable and CTest registration

## Decisions Made

- **Camera convention:** Camera position = top-left screen corner in world space. screenOffset = -cameraPos. This matches tilemap scroll semantics (setScroll stores the world origin of the visible window).
- **Shake fix (Rule 1 - Bug):** Initial implementation computed shake at `elapsed=0` giving `sin(0)=0`. Fixed by incrementing `m_shakeElapsed` before computing shake offset so the first update samples `sin(dt*40)` which is non-zero.
- **drawWithOffset pattern:** Saves/restores `anchor_offset` rather than a separate position parameter — avoids changing the draw() interface in every subclass.
- **camera.cpp in enjin2_lua target:** Plan 02 Lua bindings target enjin2_lua; placing camera.cpp there ensures the object file is available without circular deps.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Shake offset was zero on first frame (sin(0) = 0)**
- **Found during:** Task 1 verification (CAM-05a test failure)
- **Issue:** Update computed shake at `m_shakeElapsed=0` before incrementing, so `sin(0*40) = 0`. The CAM-05a test `shake(3,0.4)` then `update(0.1)` expected non-zero offset.
- **Fix:** Incremented `m_shakeElapsed += dt` first, then computed sin at the post-increment elapsed value. Clamped to duration for the final frame.
- **Files modified:** `src/components/camera.cpp`
- **Verification:** camera_test CAM-05a passes (33 passed, 0 failed)
- **Committed in:** `273de0e` (implementation commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - Bug)
**Impact on plan:** Essential correctness fix. No scope creep.

## Issues Encountered

- Pre-existing `sprite_load_test` failure: `lua_wrapper.hpp` missing. Confirmed pre-existing (fails before our changes). Out of scope — logged to deferred.

## Next Phase Readiness

- C++ foundation complete: C_Camera, modified C_Drawable, camera-aware Scene render pipeline all implemented and tested
- Plan 02 (Lua bindings) can begin immediately — C_Camera is in enjin2_lua target, getScreenOffset/setPosition/lookAt/shake/setBounds/clearBounds all available as C++ API
- No blockers

## Self-Check: PASSED

All created files exist on disk. Both task commits (e988f4e, 273de0e) found in git log.

---
*Phase: 44-2d-camera-system*
*Completed: 2026-02-28*
