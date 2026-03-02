---
phase: 48-camera-follow-save-load
plan: 01
subsystem: scripting/camera
tags: [lua-bindings, camera, follow, CAM-01, CAM-02]
dependency_graph:
  requires: []
  provides: [engine.camera.follow, engine.camera.stopFollow, LuaBindings::tickCameraFollow]
  affects: [sdl_main.cpp game loop, LuaBindings update path]
tech_stack:
  added: []
  patterns: [set-once follow pattern, member-function Lua binding for private member access]
key_files:
  created:
    - tests/camera_follow_test.cpp
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings_engine.cpp
    - src/platform/sdl/sdl_main.cpp
    - tests/CMakeLists.txt
decisions:
  - "lua_engine_camera_follow and lua_engine_camera_stopFollow implemented as LuaBindings member functions (not file-scope statics) to access private m_followTargetProxy/m_followSpeed members — same pattern as Phase 47 debug bindings"
  - "setActiveScene() must be called before setActiveCamera() because setActiveScene clears m_activeCamera on scene change — documented in test setup"
  - "Scripts must store proxy in Lua global (not local) for follow to survive across frames; local variables may be GC'd between ticks"
  - "Object::addComponent<C_Position>() should not be called when Object already auto-creates C_Position at construction; use getPosition()->setPosition() instead"
metrics:
  duration: "687 seconds (~11 minutes)"
  completed: "2026-03-01"
  tasks_completed: 1
  files_changed: 5
---

# Phase 48 Plan 01: Camera Follow Bindings Summary

engine.camera.follow/stopFollow Lua bindings with per-frame C++ tickCameraFollow dispatch.

## What Was Built

Added `engine.camera.follow(proxy, speed)` and `engine.camera.stopFollow()` Lua bindings implementing a set-once follow pattern. Scripts declare a follow target once; `tickCameraFollow(dt)` is called automatically each frame from the SDL update loop, moving the camera via `C_Camera::lookAt()`.

## Tasks Completed

| Task | Description | Commit | Files |
|------|-------------|--------|-------|
| 1 | Camera follow members + bindings + test (TDD) | 850d06a | bindings.hpp, bindings_engine.cpp, sdl_main.cpp, camera_follow_test.cpp, CMakeLists.txt |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Object auto-adds C_Position at construction**
- **Found during:** Task 1 (RED → GREEN debugging)
- **Issue:** `targetObj->addComponent<C_Position>(100, 50)` added a second C_Position on top of the auto-created one; `getComponent<C_Position>()` returned the first (at 0,0)
- **Fix:** Changed test to use `targetObj->getPosition()->setPosition(100, 50)` instead
- **Files modified:** tests/camera_follow_test.cpp

**2. [Rule 1 - Bug] setActiveScene() clears m_activeCamera**
- **Found during:** Task 1 (test debugging)
- **Issue:** `setActiveScene()` clears `m_activeCamera` when scene changes; calling `setActiveCamera()` first then `setActiveScene()` silently cleared the camera pointer
- **Fix:** Reversed order in all scene-based tests: setActiveScene first, then setActiveCamera
- **Files modified:** tests/camera_follow_test.cpp

**3. [Rule 1 - Bug] lua_engine_camera_follow needs private member access**
- **Found during:** Task 1 (GREEN compile error)
- **Issue:** File-scope static functions cannot access private LuaBindings members
- **Fix:** Implemented as `LuaBindings::lua_engine_camera_follow` member function, consistent with Phase 47 debug binding pattern
- **Files modified:** src/scripting/bindings_engine.cpp

**4. [Rule 2 - Missing] Lua proxy must be stored in global to survive GC**
- **Found during:** Task 1 (test design)
- **Issue:** Storing `local proxy` in Lua init() function leaves the userdata eligible for GC between frames; raw C++ pointer in m_followTargetProxy would become dangling
- **Fix:** Tests store proxy in Lua globals (`g_target`, `g_movable`, etc.); this is the correct usage pattern for scripts too
- **Files modified:** tests/camera_follow_test.cpp

## Test Results

```
38/38 tests passed (full suite, 0 regressions)
camera_follow_test: 29 assertions, 6 test functions — PASSED
camera_lua_test: PASSED (no regression)
```

## Self-Check: PASSED

- [x] `tests/camera_follow_test.cpp` exists
- [x] `include/enjin2/scripting/bindings.hpp` contains `m_followTargetProxy`
- [x] `src/scripting/bindings_engine.cpp` contains `lua_engine_camera_follow`
- [x] `src/platform/sdl/sdl_main.cpp` contains `tickCameraFollow`
- [x] Commit 850d06a exists: `git log --oneline | grep 850d06a`
