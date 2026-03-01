---
phase: 51-persistent-objects
plan: 01
subsystem: core
tags: [c++, scene-state-machine, object-collection, persistent-objects, ownership-transfer]

# Dependency graph
requires:
  - phase: 50-tween-helpers
    provides: stable SSM + ObjectCollection base that 51-01 extends
provides:
  - PersistentObjectRegistry struct in SceneStateMachine with 4-slot fixed array ownership
  - ObjectCollection::extractObject() returning unique_ptr without destroying object or proxy
  - ObjectCollection m_external[] non-owning injection (injectExternal/clearExternal)
  - Extended update/lateUpdate/findByName/forEach/forEachActive/findAllWithTag iterating externals
  - applyDeferredTransition() preserving persistent objects across self and cross-scene transitions
  - persistObject/unpersistObject/findPersistentByName public SSM API
  - persistent_object_test.cpp with 10-case C++ unit test suite
affects:
  - 51-02 (Lua bindings for engine.persist.add/remove/find will call persistObject/unpersistObject/findPersistentByName)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - extractObject() ownership-transfer pattern (ObjectCollection -> registry) preserves object lifetime without destroying proxy
    - m_external[] non-owning injection: registry owns, scene iterates — no double-ownership
    - injectExternal on persistObject() so object is immediately live in current scene
    - flushPendingRemovals fires BEFORE clearExternal on every transition so marked objects are destroyed cleanly

key-files:
  created:
    - tests/persistent_object_test.cpp
  modified:
    - include/enjin2/core/object_collection.hpp
    - include/enjin2/core/scene_state_machine.hpp
    - tests/CMakeLists.txt

key-decisions:
  - "persistObject() immediately re-injects extracted object as external into current scene so it stays alive and findable during the current scene (not just future scenes)"
  - "PersistentObjectRegistry is a nested public struct (not private inner class) so tests can instantiate it directly without SSM overhead"
  - "flushPendingRemovals called at START of applyDeferredTransition (before clearExternal) so destroyed objects are never re-injected"
  - "clear() on ObjectCollection does NOT clear externals — externals are managed exclusively by registry via clearExternal()"
  - "size() returns objectCount only (externals not 'owned'); consistent with zero-heap-allocation design"

patterns-established:
  - "Ownership transfer: extractObject() -> PersistentObjectRegistry::add() -> injectExternal() is the canonical persist pattern"
  - "Non-owning injection: ObjectCollection m_external[] never holds unique_ptr; registry is sole owner"

requirements-completed: [PERSIST-01, PERSIST-02]

# Metrics
duration: 5min
completed: 2026-03-02
---

# Phase 51 Plan 01: Persistent Objects C++ Infrastructure Summary

**PersistentObjectRegistry (4-slot SSM-owned array) + ObjectCollection::extractObject/injectExternal + applyDeferredTransition persistence — 41/41 tests passing**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-01T23:50:33Z
- **Completed:** 2026-03-02T00:15:07Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- ObjectCollection extended with extractObject() (ownership transfer without destruction), m_external[] non-owning injection array (MAX_EXTERNAL=4), and injectExternal/clearExternal methods
- All existing iteration methods (update/lateUpdate/findByName/forEach/forEachActive/findAllWithTag) now also iterate external objects after owned objects
- SceneStateMachine gained PersistentObjectRegistry nested struct with add/markForRemoval/contains/findByName/flushPendingRemovals, plus persistObject/unpersistObject/findPersistentByName public API
- applyDeferredTransition() updated to flush removals, clear and re-inject externals on both self and cross-scene transitions
- 10-case C++ unit test suite (persistent_object_test.cpp) covering all PERSIST-01..PERSIST-10 requirements, all passing

## Task Commits

Each task was committed atomically:

1. **Task 1: Add extractObject, m_external[], PersistentObjectRegistry to SSM** - `9753bfc` (feat)
2. **Task 2: C++ unit test + fix persistObject() immediate injection** - `11b3aa8` (feat)

## Files Created/Modified
- `include/enjin2/core/object_collection.hpp` - Added extractObject(), m_external[], injectExternal(), clearExternal(); extended update/lateUpdate/findByName/forEach/forEachActive/findAllWithTag to iterate externals
- `include/enjin2/core/scene_state_machine.hpp` - Added PersistentObjectRegistry nested struct, m_persistentRegistry member, persistObject/unpersistObject/findPersistentByName public methods, updated applyDeferredTransition()
- `tests/persistent_object_test.cpp` - 10-case test suite for PERSIST-01..PERSIST-10
- `tests/CMakeLists.txt` - Registered persistent_object_test executable

## Decisions Made
- persistObject() immediately re-injects the extracted object as an external into the current scene so it remains live and findable during the scene in which it was persisted (not just future scenes). This was discovered via TDD: the GREEN fix added the injectExternal() call after add().
- PersistentObjectRegistry defined as a nested public struct so tests can instantiate it standalone for registry-unit tests (Tests 2-5).
- flushPendingRemovals fires at the START of applyDeferredTransition, before clearExternal(), ensuring destroyed objects are never re-injected into the new/reset scene.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] persistObject() did not inject extracted object back into current scene**
- **Found during:** Task 2 TDD RED phase (test_persist06_persist_object_extracts_from_scene)
- **Issue:** persistObject() extracted the object and added it to registry but did not immediately re-inject it as an external into the current scene. This meant the object vanished from the scene's findByName during the current scene.
- **Fix:** After successful registry add, call currentScene->getObjects().injectExternal(rawPtr) so the object is immediately live as an external in the current scene.
- **Files modified:** include/enjin2/core/scene_state_machine.hpp
- **Verification:** PERSIST-06 passes; all 41 tests pass
- **Committed in:** 11b3aa8 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — bug)
**Impact on plan:** Essential for correctness. Without the immediate re-injection, persistent objects would not be findable in the scene where they were persisted, only in subsequent scenes.

## Issues Encountered
None — apart from the auto-fixed bug above, which was caught by TDD in the normal RED/GREEN cycle.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- C++ infrastructure is complete and all 41 tests pass
- Phase 51-02 can now add Lua bindings: engine.persist.add(), engine.persist.remove(), engine.persist.find() by calling persistObject/unpersistObject/findPersistentByName on the SSM
- scene.hpp renderObjects() automatically picks up externals via the extended forEach (no additional changes needed)

## Self-Check: PASSED

---
*Phase: 51-persistent-objects*
*Completed: 2026-03-02*
