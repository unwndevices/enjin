---
phase: 57-qol-features
plan: 02
subsystem: scripting
tags: [lua, camera, follow, dead-zone, bindings]

requires:
  - phase: 48-camera-follow
    provides: tickCameraFollow, lua_engine_camera_follow, m_followTargetProxy, m_followSpeed
  - phase: 56-tech-debt-cleanup
    provides: m_followTargetProxy = nullptr on scene change and hot reload (DEBT-01 fix)

provides:
  - engine.camera.setDeadZone(w, h) Lua API
  - m_deadZoneW and m_deadZoneH fields on LuaBindings
  - tickCameraFollow: dead zone rectangle check before lookAt()
  - Dead zone cleared to 0 on setActiveScene() and registerAll()

affects: [57-03-qol-test, any future camera-follow features]

tech-stack:
  added: []
  patterns:
    - Dead zone rectangle centered on camera position (not target position)
    - dx/dy compared against half-dimensions (m_deadZoneW * 0.5f) for centered rectangle
    - 0,0 disables dead zone; guard checks > 0.0f before rectangle math

key-files:
  created: []
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings_engine.cpp
    - src/scripting/bindings.cpp

key-decisions:
  - "Dead zone rectangle centered on camera position (not target) — classic platformer feel"
  - "Dead zone check before lookAt() call — early return freezes camera completely"
  - "Cleanup in both setActiveScene() and registerAll() immediately after m_followTargetProxy = nullptr — consistent cleanup group per Phase 56 pattern"

requirements-completed: [QOL-03]

duration: 12min
completed: 2026-03-03
---

# Phase 57 Plan 02: Camera Dead Zone Summary

**engine.camera.setDeadZone(w, h) — rectangular freeze zone centered on camera position wired into tickCameraFollow, cleared on scene change and hot reload**

## Performance

- **Duration:** 12 min
- **Started:** 2026-03-03T00:02:00Z
- **Completed:** 2026-03-03T00:14:00Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- `m_deadZoneW` and `m_deadZoneH` float fields added to `LuaBindings` (zero-initialized, 0 = disabled)
- `engine.camera.setDeadZone(w, h)` binding: clamps negative to 0, registered in camera subtable
- `tickCameraFollow`: dead zone rectangle check before `lookAt()` — target inside → freeze (return early)
- Dead zone cleared alongside `m_followTargetProxy` on both scene change and hot reload

## Task Commits

1. **Task 1: Add dead zone fields to LuaBindings and implement setDeadZone binding** - `021ceeb` (feat)
2. **Task 2: Clear dead zone state on scene change and hot reload** - `6320714` (feat)

## Files Created/Modified
- `include/enjin2/scripting/bindings.hpp` — m_deadZoneW/H fields + lua_engine_camera_setDeadZone declaration
- `src/scripting/bindings_engine.cpp` — setDeadZone impl; kCameraFuncs entry; tickCameraFollow dead zone check
- `src/scripting/bindings.cpp` — m_deadZoneW = m_deadZoneH = 0.0f in setActiveScene() and registerAll()

## Decisions Made
- Rectangle centered on camera position (not target): `dx = |targetX - camPos.x|`, half-dimensions for bounds
- Early return on dead zone freeze (not a flag) — cleaner than conditional around lookAt
- Both cleanup locations immediately follow `m_followTargetProxy = nullptr` per Phase 56 cleanup group convention

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness
- Plan 57-03 (qol_test suite) depends on both Plans 01 and 02 being complete — both are now done
- All three QoL APIs are implemented and ready for integration testing

---
*Phase: 57-qol-features*
*Completed: 2026-03-03*

## Self-Check: PASSED
- key-files modified exist on disk: bindings.hpp ✓, bindings_engine.cpp ✓, bindings.cpp ✓
- git commits present: 021ceeb, 6320714 ✓
- No Self-Check: FAILED marker
