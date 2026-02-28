---
phase: 44-2d-camera-system
plan: 02
subsystem: scripting
tags: [lua, bindings, camera, C_Camera, ComponentProxy, engine-table, tilemap]

# Dependency graph
requires:
  - phase: 44-01
    provides: C_Camera C++ component with setPosition/lookAt/shake/setBounds/getScreenOffset
  - phase: 43-tilemap-system
    provides: C_Tilemap with draw() and drawWithOffset hook point
  - phase: 39-componentproxy
    provides: ComponentProxy userdata pattern and self:get() dispatch infrastructure

provides:
  - C_Camera_Proxy metatable with setPosition/getPosition/lookAt/shake/setBounds/clearBounds
  - self:get("C_Camera") dispatch in lua_proxy_get_component_impl
  - engine.camera.* global sub-table (setPosition/getPosition/lookAt/shake/setBounds/clearBounds)
  - m_activeCamera member in LuaBindings with setActiveCamera/getActiveCamera
  - C_Tilemap::drawWithOffset override integrating camera and tilemap scroll (CAM-09)
  - camera_lua_test covering CAM-07, CAM-08, CAM-09

affects: [game-scripts, scene-rendering, lua-host-integration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - engine.camera.* global table follows engine.input.* silent-no-op pattern (getActiveCamera returns null -> no-op)
    - C_Camera_Proxy follows CTILEMAP_PROXY_METATABLE pattern with per-method push
    - m_activeCamera cleared in setActiveScene() like m_eventBus.clearHandlers()
    - drawWithOffset save/restore scroll pattern for temporary offset application

key-files:
  created:
    - tests/camera_lua_test.cpp
  modified:
    - src/scripting/bindings.cpp
    - src/scripting/bindings_engine.cpp
    - include/enjin2/scripting/bindings.hpp
    - include/enjin2/components/tilemap.hpp
    - src/components/tilemap.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "engine.camera.* uses getBindings(L)->getActiveCamera() — follows existing input pointer pattern (not registry-stored pointer-to-pointer)"
  - "C_Tilemap::drawWithOffset saves/restores m_scrollX/m_scrollY, subtracts camera offset (negative) to produce additive scroll"
  - "setActiveCamera cleared in setActiveScene() on scene change — same lifecycle as m_eventBus.clearHandlers()"

patterns-established:
  - "C_Camera_Proxy: proxy methods as file-static functions, __index dispatches by string key"
  - "engine.camera.* silent no-op when m_activeCamera==nullptr — consistent with engine.input.* behavior"
  - "TDD: test file committed first (RED), implementation already green from prior tasks"

requirements-completed: [CAM-07, CAM-08, CAM-09]

# Metrics
duration: 15min
completed: 2026-02-28
---

# Phase 44 Plan 02: 2D Camera System Lua Bindings Summary

**C_Camera Lua bindings: C_Camera_Proxy metatable via self:get(), engine.camera.* global sub-table, C_Tilemap drawWithOffset camera integration, and camera_lua_test (10/10 assertions passing)**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-02-28T22:34:00Z
- **Completed:** 2026-02-28T22:46:40Z
- **Tasks:** 3
- **Files modified:** 6

## Accomplishments

- C_Camera_Proxy registered in `registerComponentProxyMetatable()` with 6 methods: setPosition, getPosition, lookAt, shake, setBounds, clearBounds. self:get("C_Camera") dispatch added alongside C_Timer, C_StateMachine, C_Tilemap.
- engine.camera.* sub-table registered in `registerEngineTable()` following engine.event.* pattern. Silent no-op when m_activeCamera is nullptr — safe for scripts that run before a camera is placed. setActiveScene() clears m_activeCamera on scene change.
- C_Tilemap::drawWithOffset override integrates camera offset additively with tilemap scroll: effective_scroll = m_scrollX - offset.x (where offset = getScreenOffset() = -camera_pos). Saves/restores scroll around draw call.
- camera_lua_test.cpp: 10 test functions, all passing (CAM-07a through CAM-07f, CAM-08a through CAM-08e, CAM-09).

## Task Commits

Each task was committed atomically:

1. **Task 1: C_Camera_Proxy metatable and self:get dispatch** - `3500d99` (feat)
2. **Task 2: engine.camera.* sub-table and C_Tilemap drawWithOffset** - `b39418f` (feat)
3. **Task 3: Lua camera integration test suite (CAM-07, CAM-08, CAM-09)** - `7122b80` (test)

_Note: Task 3 used TDD — test committed first (RED), then immediately GREEN because Tasks 1+2 already provided full implementation._

## Files Created/Modified

- `src/scripting/bindings.cpp` - Added C_Camera include, C_Camera dispatch in get, C_Camera_Proxy metatable implementation, m_activeCamera clear in setActiveScene
- `src/scripting/bindings_engine.cpp` - Added camera.hpp include, engine.camera.* sub-table registration, getActiveCamera helper, 6 engine.camera.* function implementations
- `include/enjin2/scripting/bindings.hpp` - Forward declare C_Camera, add m_activeCamera member, add setActiveCamera/getActiveCamera public methods
- `include/enjin2/components/tilemap.hpp` - Declare drawWithOffset override (CAM-09)
- `src/components/tilemap.cpp` - Implement C_Tilemap::drawWithOffset with save/restore scroll pattern
- `tests/camera_lua_test.cpp` - 10-function Lua integration test suite (CAM-07, CAM-08, CAM-09)
- `tests/CMakeLists.txt` - Add camera_lua_test inside ENJIN2_BUILD_LUA block

## Decisions Made

- engine.camera.* uses `getBindings(L)->getActiveCamera()` pattern (not registry pointer-to-pointer). This follows the existing `currentInput` pattern for engine.input.* — simpler, consistent, and the host sets the pointer explicitly via `bindings.setActiveCamera(cam)`.
- C_Tilemap::drawWithOffset subtracts the camera offset from scroll: `m_scrollX -= offset.x`. Since offset = getScreenOffset() = -camera_pos, subtracting a negative offset is additive: effective_scroll = scroll + camera_pos. Verified: camera at (10,0) + tilemap scroll at (5,0) = effective 15px scroll.
- m_activeCamera cleared in setActiveScene() on scene change, parallel to m_eventBus.clearHandlers() — same lifecycle boundary.

## Deviations from Plan

None — plan executed exactly as written. The tilemap_test, tilemap_lua_test, and camera_test regressions were all zero (31/32 pre-existing tests still pass; sprite_load_test failure is pre-existing/unrelated to this plan).

## Issues Encountered

- **Compilation error:** Test file initially used `getLuaEngine()` which doesn't exist on C_LuaScript. Fixed to use `script->getScriptSystem().getBindings()`. [Rule 3 - auto-fixed inline]
- **Compilation error:** Test file used `Canvas<Pixel4, 64, 64>` — correct type is `Canvas4<64, 64>`. Fixed. [Rule 3 - auto-fixed inline]

## Next Phase Readiness

- Phase 44 is complete — all C_Camera C++ (CAM-01..CAM-06) and Lua (CAM-07..CAM-09) requirements implemented and tested.
- Lua game scripts can now control the 2D camera via `self:get("C_Camera")` proxy or `engine.camera.*` global API.
- Host integration: call `bindings.setActiveCamera(cam)` after scene setup to wire engine.camera.* functions.
- C_Tilemap drawWithOffset integration is live — camera scroll and tilemap scroll are additive.

---
*Phase: 44-2d-camera-system*
*Completed: 2026-02-28*
