---
phase: 45-optimized-2d-physics-engine
plan: 02
subsystem: physics-lua-bindings
tags: [physics, lua-bindings, engine.physics, raycast, dda, gravity, Vec2]

# Dependency graph
requires:
  - phase: 45-01
    provides: 7 stateless inline physics helpers in enjin2::physics namespace (physics.hpp)
  - phase: 44-2d-camera-system
    provides: engine.camera.* sub-table pattern used for engine.physics.* registration
provides:
  - engine.physics.* Lua sub-table with 10 binding functions
  - Global gravity state (m_gravityX/m_gravityY) in LuaBindings
  - DDA tilemap raycast + linear object proximity scan
  - physics_lua_test.cpp with 22 Lua integration assertions
affects: [any-lua-script-using-physics, game-scripts-update-callbacks]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Vec2/number dual-input pattern via luaL_testudata (matches collision binding pattern)
    - lua_gettop disambiguation for overloaded Lua functions (applyGravity 3-arg vs 5-arg)
    - forEach lambda scan for C_Tilemap (avoids const/non-const getComponent issue)
    - DDA (Amanatides-Woo) tilemap raycast with stack-local state (no heap)
    - Linear object scan via closest-point-on-segment with fixed 8px hit radius

key-files:
  created:
    - src/scripting/bindings_physics.cpp
    - tests/physics_lua_test.cpp
  modified:
    - include/enjin2/scripting/bindings.hpp
    - src/scripting/bindings_engine.cpp
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "forEach lambda scan for tilemap (not findObjectWithComponent<C_Tilemap>) — pre-existing hasComponent() const issue calls non-const getComponent(); workaround avoids touching shared header"
  - "applyGravity uses lua_gettop to disambiguate 3-arg (global gravity) vs 5-arg (override gravity) — matches plan spec"
  - "Vec2 is registered as global constructor function not a table — Vec2(0,0) not Vec2.new(0,0)"
  - "Raycast returns 5 values on hit (bool, hitX, hitY, dist, what) or 1 value (false) on miss — matches plan spec"
  - "DDA tilemap scan runs before linear object scan — tilemap hit takes priority over object hit"

requirements-completed: [PHYS-09, PHYS-10, PHYS-11, PHYS-12, PHYS-13]

# Metrics
duration: 10min
completed: 2026-03-01
---

# Phase 45 Plan 02: Physics Lua Bindings Summary

**10 engine.physics.* Lua binding functions (applyGravity, bounce, applyDrag, springForce, attract, orbitVelocity, applyVelocity, setGravity, getGravity, raycast) with global gravity state in LuaBindings and DDA tilemap raycast — 22/22 Lua integration tests pass**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-01T12:38:34Z
- **Completed:** 2026-03-01T12:49:02Z
- **Tasks:** 2
- **Files modified:** 6 (4 modified, 2 created)

## Accomplishments

- Created `bindings_physics.cpp` with all 10 engine.physics.* static member functions implementing the full physics Lua API
- Added `m_gravityX`/`m_gravityY` to `LuaBindings` private members for scene-level global gravity state
- Registered `engine.physics` sub-table in `registerEngineTable()` in `bindings_engine.cpp` following the existing camera/collision pattern
- All 10 functions accept both plain number pairs and Vec2 userdata via `luaL_testudata` dual-input pattern
- `applyGravity` disambiguates 3-arg (use global gravity) vs 5-arg (use override gravity) via `lua_gettop`
- `raycast` implements two-stage scan: Amanatides-Woo DDA tilemap traversal then linear object proximity scan (8px radius), returning `hit, hitX, hitY, dist, what` on hit
- Created `physics_lua_test.cpp` with 22 Lua integration assertions covering all 10 functions including edge cases

## Task Commits

Each task was committed atomically:

1. **Task 1: Create bindings_physics.cpp + wire gravity state + register engine.physics sub-table** - `192a7b4` (feat)
2. **Task 2: Create physics_lua_test.cpp Lua integration test suite** - `3a19fa5` (test)

## Files Created/Modified

- `src/scripting/bindings_physics.cpp` - All 10 engine.physics.* binding implementations (248 lines)
- `tests/physics_lua_test.cpp` - 22-assertion Lua integration test suite
- `include/enjin2/scripting/bindings.hpp` - Added m_gravityX/m_gravityY members + 10 static function declarations
- `src/scripting/bindings_engine.cpp` - Registered engine.physics sub-table with 10 functions
- `CMakeLists.txt` - Added bindings_physics.cpp to enjin2_lua target sources
- `tests/CMakeLists.txt` - Registered physics_lua_test inside ENJIN2_BUILD_LUA block

## Decisions Made

- `forEach` lambda scan for C_Tilemap in raycast — pre-existing `hasComponent() const` calls non-const `getComponent<T>()` would error when instantiated with C_Tilemap; workaround avoids touching shared infrastructure
- `Vec2` constructor is a global function (`Vec2(x, y)`) not a table (`Vec2.new`) — test corrected from `Vec2.new(0,0)` to `Vec2(0,0)` after build feedback
- DDA tilemap scan (Stage 1) runs before linear object scan (Stage 2) — tilemap always takes priority
- No dynamic allocation in raycast — all state is stack-local; `forEach` lambda captures by reference

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Pre-existing const/non-const `hasComponent()` issue triggered by C_Tilemap instantiation**
- **Found during:** Task 1 build (bindings_physics.cpp compilation)
- **Issue:** `ObjectCollection::findObjectWithComponent<C_Tilemap>()` instantiates `Object::hasComponent<C_Tilemap>() const` which internally calls non-const `getComponent<T>()` — compile error `-fpermissive`
- **Fix:** Used `scene->getObjects().forEach(lambda)` with `obj->getComponent<C_Tilemap>()` instead of `findObjectWithComponent<C_Tilemap>()` — avoids the const chain entirely
- **Files modified:** `src/scripting/bindings_physics.cpp`
- **Commit:** `192a7b4`

**2. [Rule 1 - Bug] `LuaResult::errorMessage` does not exist (field is named `error`)**
- **Found during:** Task 2 build (physics_lua_test.cpp compilation)
- **Issue:** Test helper referenced `r.errorMessage.c_str()` but `LuaResult` uses `r.error`
- **Fix:** Changed to `r.error.c_str()` in `runLua()` helper
- **Files modified:** `tests/physics_lua_test.cpp`
- **Commit:** `3a19fa5`

**3. [Rule 1 - Bug] `Vec2.new()` is not valid — Vec2 is a constructor function not a namespace table**
- **Found during:** Task 2 test run (runtime Lua error)
- **Issue:** Test used `Vec2.new(0, 0)` but `Vec2` is registered as a global function — correct call is `Vec2(0, 0)`
- **Fix:** Changed `Vec2.new(...)` to `Vec2(...)` in two test cases
- **Files modified:** `tests/physics_lua_test.cpp`
- **Commit:** `3a19fa5`

## Issues Encountered

None beyond the auto-fixed items above.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- Phase 45 is now complete (Plan 01: C++ helpers, Plan 02: Lua bindings + tests)
- `engine.physics.*` is the final API layer Lua game scripts use in their `update()` callbacks
- All 10 physics functions accessible from Lua with Vec2 dual-input support
- No blockers for any subsequent phase

## Self-Check: PASSED

- FOUND: src/scripting/bindings_physics.cpp
- FOUND: tests/physics_lua_test.cpp
- FOUND: .planning/phases/45-optimized-2d-physics-engine/45-02-SUMMARY.md
- FOUND: commit 192a7b4 (feat: bindings_physics.cpp)
- FOUND: commit 3a19fa5 (test: physics_lua_test.cpp)

---
*Phase: 45-optimized-2d-physics-engine*
*Completed: 2026-03-01*
