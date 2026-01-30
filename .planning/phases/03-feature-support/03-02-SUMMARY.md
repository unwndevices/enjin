---
phase: 03-feature-support
plan: 02
subsystem: abstraction
tags: interfaces, compile-time-polymorphism, c++17, virtual-functions

# Dependency graph
requires:
  - phase: 02-core-migration
    provides: Seam infrastructure (ComponentSeam, SceneSeam), compatibility layer (enjin namespace wrapper)
  - phase: 03-feature-support (plan 01)
    provides: CMake compile-time backend selection (USE_ENJIN1_BACKEND macro)
provides:
  - ICanvas template interface with full rendering API (text, shapes, bitmap drawing)
  - IComponent interface with component lifecycle methods (awake, start, update, lateUpdate, onEnable, onDisable)
  - IScene template interface with scene lifecycle methods (onCreate, onActivate, onDeactivate, onDestroy, onUpdate, onRender)
affects: Future phases implementing enjin1/enjin2 compile-time switching via abstraction layers

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Pure virtual interface pattern (I-prefixed types with = 0 methods)
    - Template-based abstraction for pixel type flexibility
    - Forward declarations to avoid implementation dependencies
    - Virtual destructors for safe polymorphic deletion

key-files:
  created:
    - enjin2/include/enjin2/abstract/icanvas.hpp
    - enjin2/include/enjin2/abstract/icomponent.hpp
    - enjin2/include/enjin2/abstract/iscene.hpp
  modified: []

key-decisions:
  - "Template ICanvas and IScene on pixel type to match enjin2's existing pattern"
  - "Non-templated IComponent (doesn't depend on pixel type)"
  - "Minimal interface scope - only methods that exist in enjin2 implementations"

patterns-established:
  - "Pure virtual interface pattern: All methods are = 0, virtual destructors = default"
  - "Template abstraction for pixel-dependent types (ICanvas, IScene)"
  - "Forward declarations maintain compilation independence (no enjin2/core/* includes in abstract headers)"

# Metrics
duration: 2 min
completed: 2026-01-30
---

# Phase 3 Plan 2: Abstraction Interfaces Summary

**ICanvas, IComponent, and IScene pure virtual interfaces enabling compile-time polymorphism between enjin1 and enjin2 implementations**

## Performance

- **Duration:** 2 min
- **Started:** 2026-01-30T20:54:11Z
- **Completed:** 2026-01-30T20:56:15Z
- **Tasks:** 3
- **Files created:** 3

## Accomplishments

- Created ICanvas template interface with extended rendering API (text, shapes, bitmap drawing)
- Created IComponent interface with full component lifecycle methods and state queries
- Created IScene template interface with scene lifecycle methods and rendering
- All interfaces use pure virtual methods (= 0) for compile-time polymorphism
- All interfaces have virtual destructors for safe polymorphic deletion
- Used forward declarations to maintain compilation independence
- Followed enjin2's existing ICanvas template pattern

## Task Commits

Each task was committed atomically:

1. **Task 1: Create extended ICanvas abstraction interface** - `7618dee` (feat)
2. **Task 2: Create IComponent abstraction interface** - `56c463a` (feat)
3. **Task 3: Create IScene abstraction interface** - `357d573` (feat)

**Plan metadata:** TBD (docs: complete plan)

## Files Created/Modified

- `enjin2/include/enjin2/abstract/icanvas.hpp` - Canvas abstraction interface with full rendering API (text, shapes, bitmaps)
- `enjin2/include/enjin2/abstract/icomponent.hpp` - Component abstraction interface with lifecycle methods (awake, start, update, lateUpdate, onEnable, onDisable) and state queries (getOwner, isEnabled, setEnabled)
- `enjin2/include/enjin2/abstract/iscene.hpp` - Scene abstraction interface with lifecycle methods (onCreate, onActivate, onDeactivate, onDestroy, onUpdate, onRender) and state queries (getId, isActive, isInitialized)

## Decisions Made

- Template ICanvas and IScene on pixel type to match enjin2's existing ICanvas pattern
- IComponent is non-templated - doesn't depend on pixel type
- Minimal interface scope - only methods that exist in enjin2 implementations (awake, start, update, etc.)
- Forward declarations used throughout - abstract headers don't include implementation headers (enjin2/core/*)
- All methods are pure virtual (= 0) - enforces implementation requirements
- All interfaces have virtual destructors - enables safe deletion through base pointers

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - all abstraction interfaces compiled successfully with g++ -std=c++17.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Abstraction interfaces are ready for enjin1 and enjin2 implementations
- Interfaces maintain compilation independence (no implementation includes)
- Ready for Plan 03-03: Update seams for compile-time routing with interface implementation
- No blockers identified

---
*Phase: 03-feature-support*
*Completed: 2026-01-30*
