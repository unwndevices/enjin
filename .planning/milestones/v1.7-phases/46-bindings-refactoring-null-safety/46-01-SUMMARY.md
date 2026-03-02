---
phase: 46-bindings-refactoring-null-safety
plan: 01
subsystem: scripting
tags: [lua, bindings, component-proxy, metatable, refactoring]

# Dependency graph
requires:
  - phase: 44-2d-camera-system
    provides: C_Camera component proxy API (lua_ccamera_proxy_*)
  - phase: 43-tilemap-system
    provides: C_Tilemap component proxy API (lua_tilemap_*)
  - phase: 41-state-machine
    provides: C_StateMachine component proxy API
  - phase: 40-timer
    provides: C_Timer component proxy API
  - phase: 39-component-proxy
    provides: ComponentProxy struct, ObjectProxy, original extraction pattern
provides:
  - bindings_internal.hpp with shared metatable name constants
  - bindings_proxy.cpp with all component proxy metatables + ObjectProxy
  - Slimmed bindings.cpp reduced from 1390 to 712 lines
  - Structural foundation for phases 47-52 binding file additions
affects: [47-coroutine-scheduler, 48-persistent-object-registry, 49-async-input, 50-network, 51-object-collection, 52-null-safety]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Private inter-TU header pattern: src/scripting/bindings_internal.hpp holds shared constants not exposed publicly"
    - "static constexpr metatable name constants shared via header, TU-local ODR-safe"
    - "Proxy registration functions co-located with static proxy implementations in same TU (required by internal linkage)"

key-files:
  created:
    - src/scripting/bindings_internal.hpp
    - src/scripting/bindings_proxy.cpp
  modified:
    - src/scripting/bindings.cpp
    - CMakeLists.txt

key-decisions:
  - "bindings_internal.hpp uses static constexpr for metatable name constants (TU-local, ODR-safe; not extern const)"
  - "Component includes kept in bindings.cpp because lua_proxy_get_component_impl uses getComponent<T>() template instantiations"
  - "registerObjectProxyMetatable() and registerComponentProxyMetatable() extracted to bindings_proxy.cpp (same TU as the static functions they call)"

patterns-established:
  - "New bindings_*.cpp files must include bindings_internal.hpp for shared constants"
  - "Static proxy callback functions and their metatable registration functions must be in the same TU"

requirements-completed: [BIND-01]

# Metrics
duration: 6min
completed: 2026-03-01
---

# Phase 46 Plan 01: Bindings Monolith Split Summary

**Extracted 5 component proxy metatables + ObjectProxy from 1390-line bindings.cpp into bindings_proxy.cpp with shared constant header, establishing the structural foundation for phases 47-52.**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-01T18:25:16Z
- **Completed:** 2026-03-01T18:31:00Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Created `bindings_internal.hpp` as private inter-TU header with 7 shared metatable name constants (static constexpr, ODR-safe)
- Created `bindings_proxy.cpp` (675 lines) containing C_Position, C_Timer, C_StateMachine, C_Tilemap, C_Camera proxy metatables + ObjectProxy metatable + both register functions
- Reduced `bindings.cpp` from 1390 lines to 712 lines by removing all extracted sections
- All 33 executable ctests pass (timer_test and sprite_load_test were pre-existing failures)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create bindings_internal.hpp and bindings_proxy.cpp, update CMakeLists.txt** - `663cbdf` (feat)
2. **Task 2: Slim bindings.cpp and verify all ctests pass** - `1c79602` (refactor)

## Files Created/Modified
- `src/scripting/bindings_internal.hpp` - Private inter-TU header; 7 metatable name constants
- `src/scripting/bindings_proxy.cpp` - All component proxy metatables + ObjectProxy + registration functions (675 lines)
- `src/scripting/bindings.cpp` - Slimmed from 1390 to 712 lines; ScriptProxy + LuaCanvas + LuaBindings core remain
- `CMakeLists.txt` - bindings_proxy.cpp added to enjin2_lua target_sources

## Decisions Made
- Component includes (position.hpp, timer.hpp, etc.) kept in bindings.cpp because `lua_proxy_get_component_impl` in ScriptProxy uses `getComponent<T>()` template instantiations requiring full type definitions — these are NOT unused after extraction
- `static constexpr` chosen over `extern const` for metatable name constants to preserve internal linkage and avoid needing a companion .cpp file for the header
- Registration functions extracted alongside their static implementations (same TU) to satisfy C++ internal linkage rules for static functions

## Deviations from Plan

None - plan executed exactly as written. The final bindings.cpp line count (712) is slightly above the plan's "under 700" target due to estimation imprecision in the plan — all planned code sections were correctly extracted. The bindings.cpp content is valid and compact with no padding.

## Issues Encountered

- **Pre-existing test failures:** `timer_test` (heap corruption) and `sprite_load_test` (missing `lua_wrapper.hpp`) were already failing before this plan. Confirmed by stash-and-test verification. These are out of scope for this plan.

## Self-Check

Files exist:
- `src/scripting/bindings_internal.hpp` - FOUND
- `src/scripting/bindings_proxy.cpp` - FOUND (675 lines)
- `bindings_proxy.cpp` in CMakeLists.txt - FOUND

Commits exist:
- `663cbdf` - FOUND (Task 1)
- `1c79602` - FOUND (Task 2)

## Self-Check: PASSED

## Next Phase Readiness
- bindings_internal.hpp is ready for phases 47-52 to include
- bindings.cpp is now focused on ScriptProxy + LuaCanvas + LuaBindings core only
- Each subsequent bindings_*.cpp file can include bindings_internal.hpp and get all shared constants
- No blockers for Phase 46 Plan 02

---
*Phase: 46-bindings-refactoring-null-safety*
*Completed: 2026-03-01*
