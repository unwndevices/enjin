---
phase: 45-optimized-2d-physics-engine
plan: 01
subsystem: physics
tags: [physics, math, triglut, lut, inline, header-only, collision, embedded]

# Dependency graph
requires:
  - phase: 44-2d-camera-system
    provides: Camera Lua bindings and engine.camera.* sub-table (base for engine.physics.* pattern)
  - phase: collision-hpp
    provides: collision::reflect used internally by bounce()
provides:
  - 7 stateless inline physics helper functions in enjin2::physics namespace (physics.hpp)
  - TrigLUT backed by constexpr 256-entry int16_t sine table (no runtime std::sin/cos)
  - physics_test.cpp C++ unit test suite (28 assertions, all pass)
affects: [45-02-physics-lua-bindings, any-phase-using-physics-helpers]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - stateless inline helper functions in header-only physics.hpp (no .cpp needed)
    - constexpr LUT for embedded-friendly trig (avoids runtime std::sin/cos on ESP32)
    - null-safe output pointer pattern (all helpers accept nullptr for unused outputs)
    - cos(x) = sin(x+64) quarter-turn phase offset for 256-entry table

key-files:
  created:
    - include/enjin2/core/physics.hpp
    - tests/physics_test.cpp
  modified:
    - include/enjin2/core/math.hpp
    - src/core/math.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "physics.hpp is fully header-only inline — no .cpp needed; matches collision.hpp pattern"
  - "TrigLUT constexpr sin_table[256] defined in math.hpp header itself; getSineValue() moved inline — math.cpp stub eliminated"
  - "cos() = sin(angle + 64) via quarter-turn phase offset — single table covers both sin and cos"
  - "applyDrag factor clamped to [0,1] — prevents velocity sign flip on overdrag (large drag * large dt)"
  - "attract() uses distSq + 1e-4f epsilon — prevents div-by-zero for coincident points without branching"
  - "springForce operates on 1D scalar — call once per axis; composable with other helpers"

patterns-established:
  - "Null-safe output pointer pattern: all physics helpers accept nullptr for unused outputs, safe to ignore"
  - "Epsilon guard in attract(): distSq += 1e-4f prevents NaN on coincident points"
  - "Drag clamping: factor = clamp(1 - drag*dt, 0, 1) — velocity can stop but never reverse direction from drag"

requirements-completed: [PHYS-01, PHYS-02, PHYS-03, PHYS-04, PHYS-05, PHYS-06, PHYS-07, PHYS-08]

# Metrics
duration: 4min
completed: 2026-03-01
---

# Phase 45 Plan 01: Physics Helper Toolkit Summary

**7 stateless inline physics helpers (applyGravity, bounce, applyDrag, springForce, attract, orbitVelocity, applyVelocity) with TrigLUT backed by constexpr 256-entry int16_t sine table — 28/28 tests pass**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-01T12:31:51Z
- **Completed:** 2026-03-01T12:35:48Z
- **Tasks:** 2
- **Files modified:** 5 (3 modified, 2 created)

## Accomplishments

- Implemented `physics.hpp` with 7 stateless inline functions covering gravity, bounce, drag, springs, attraction, orbit, and velocity integration
- Completed TrigLUT in `math.hpp` with a precomputed constexpr `int16_t[256]` sine table — `sin()/cos()` no longer delegate to `std::sin/std::cos`
- Built 28-test C++ unit suite covering all 7 helpers including edge cases (degenerate positions, overdrag clamping, maxForce cap, NaN guard)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create physics.hpp + complete TrigLUT** - `3dab048` (feat)
2. **Task 2: Create physics_test.cpp C++ unit test suite** - `2958201` (test)

## Files Created/Modified

- `include/enjin2/core/physics.hpp` - 7 stateless inline physics helpers in `enjin2::physics` namespace
- `include/enjin2/core/math.hpp` - TrigLUT completed with constexpr `sin_table[256]`; `getSineValue()` moved inline; `sin()/cos()` use LUT
- `src/core/math.cpp` - Removed `getSineValue()` stub (now inline in header)
- `tests/physics_test.cpp` - 28-assertion test suite for all helpers + TrigLUT
- `tests/CMakeLists.txt` - `physics_test` registered inside `ENJIN2_BUILD_LUA` block

## Decisions Made

- `physics.hpp` is fully header-only inline — matches `collision.hpp` pattern, no `.cpp` needed
- `TrigLUT::getSineValue()` moved inline into `math.hpp` backed by `constexpr sin_table[256]` — eliminates stub in `math.cpp`
- `cos(x) = sin(x + 64)` via quarter-turn phase offset — single table covers both sin and cos with zero extra memory
- `applyDrag` factor clamped to `[0, 1]` — prevents velocity sign flip on overdrag (`large_drag * large_dt`)
- `attract()` uses `distSq += 1e-4f` epsilon — prevents NaN on coincident points without branching

## Deviations from Plan

None — plan executed exactly as written. TDD RED/GREEN flow: tests written first (physics_test.cpp registered, build failed on missing physics.hpp), then physics.hpp implemented to pass all tests.

## Issues Encountered

Pre-existing issue (out of scope): `tests/sprite_load_test.cpp` references a missing `lua_wrapper.hpp`. This was present before Phase 45 and is unrelated to our changes. Logged in `deferred-items.md`.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `physics.hpp` is the C++ layer Plan 02 wraps with Lua bindings (`engine.physics.*`)
- All 7 helpers are header-only inline — Plan 02 only needs to `#include <enjin2/core/physics.hpp>`
- `TrigLUT` is complete and ready for use in orbit calculations in Plan 02 Lua bindings
- No blockers for Plan 02

---
*Phase: 45-optimized-2d-physics-engine*
*Completed: 2026-03-01*
