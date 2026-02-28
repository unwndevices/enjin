---
phase: 30-scene-self-transitions
plan: 01
subsystem: scene
tags: [scene, state-machine, deferred-transition, tdd, cpp-headers]

# Dependency graph
requires:
  - phase: 29-named-objects-tags
    provides: ObjectCollection name/tag search proxies on Scene; established Scene subclassing patterns

provides:
  - Scene::m_ssm protected back-pointer to owning SceneStateMachine
  - Scene::setStateMachine() injection method
  - Scene::resetInitialized() to bypass initialize guard for self-transitions
  - SceneStateMachine::switchTo() deferred transition queuing (last-wins, safe from onUpdate)
  - SceneStateMachine::applyDeferredTransition() — self-reset and cross-scene paths
  - scene_transition_test covering SCENE-01, SCENE-02, SCENE-03

affects:
  - 31-engine-global-table
  - 32-scriptproxy-userdata
  - any phase that extends SceneStateMachine or derives from Scene

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "TDD RED/GREEN cycle: test written against not-yet-existing API, compile error confirms RED, implementation achieves GREEN"
    - "Deferred dispatch pattern: queue ID + bool flag, clear flag BEFORE dispatch to allow re-queuing from callbacks"
    - "Forward declaration in header to break circular include between Scene and SceneStateMachine"
    - "Non-owning back-pointer injection: setStateMachine() called before activate() in all activation paths"

key-files:
  created:
    - tests/scene_transition_test.cpp
  modified:
    - include/enjin2/core/scene.hpp
    - include/enjin2/core/scene_state_machine.hpp
    - tests/CMakeLists.txt

key-decisions:
  - "hasPendingTransition cleared to false BEFORE applyDeferredTransition() call — ensures switchTo() from onDeactivate() queues for next frame, not re-enters current frame"
  - "Forward declaration only in scene.hpp — no reverse include to avoid circular include between scene.hpp and scene_state_machine.hpp"
  - "m_ssm field placed in protected section (not private) — allows TestScene subclass to expose via getSSM() and derived scenes to call m_ssm->switchTo()"
  - "switchTo() uses last-wins semantics — multiple calls in same frame overwrite pendingSceneId"
  - "Self-transition via applyDeferredTransition: deactivate -> resetInitialized -> setStateMachine -> activate (not a separate code path from normal activation)"

patterns-established:
  - "Deferred transition: bool flag + ID stored in SSM; cleared atomically before apply to allow safe re-queuing"
  - "SSM injection: setStateMachine(this) must be called in all activation paths (completeTransition, startTransition SLIDE, applyDeferredTransition)"
  - "Self-transition reset: deactivate() + resetInitialized() + activate() restores full lifecycle cycle for current scene"

requirements-completed: [SCENE-01, SCENE-02, SCENE-03]

# Metrics
duration: 2min
completed: 2026-02-27
---

# Phase 30 Plan 01: Scene Self-Transitions Summary

**Scene back-pointer injection (m_ssm) and deferred switchTo() mechanism enabling safe self-transitions with full onCreate/onActivate reset cycle**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-27T02:13:53Z
- **Completed:** 2026-02-27T02:16:04Z
- **Tasks:** 2 (RED + GREEN)
- **Files modified:** 4

## Accomplishments

- Added `SceneStateMachine* m_ssm` back-pointer to Scene (protected, forward-declared only)
- Implemented `switchTo()` deferred transition queue on SceneStateMachine — safe to call from `onUpdate()`
- Self-transition triggers full reset: `onDeactivate`, `onCreate`, `onActivate` all fire; `createCount == 2` after one self-switch
- All 17 test assertions pass across 4 test functions covering SCENE-01, SCENE-02, SCENE-03
- Zero dynamic allocation — no std::queue, single bool+uint32_t pending fields

## Task Commits

Each task was committed atomically:

1. **Task RED: Write failing scene_transition_test** - `7f50d7a` (test)
2. **Task GREEN: Implement m_ssm injection + deferred switchTo** - `1dfb102` (feat)

**Plan metadata:** (docs commit — see final_commit below)

_Note: TDD plan — RED commit had 4 confirmed compile errors; GREEN commit passes all 17 assertions_

## Files Created/Modified

- `tests/scene_transition_test.cpp` — TestScene struct with lifecycle counters; 4 test functions covering SCENE-01/02/03
- `tests/CMakeLists.txt` — Added scene_transition_test CTest target registration
- `include/enjin2/core/scene.hpp` — Forward declaration; protected m_ssm field; setStateMachine(); resetInitialized()
- `include/enjin2/core/scene_state_machine.hpp` — pendingSceneId/hasPendingTransition fields; switchTo(); updated update(); applyDeferredTransition(); SSM injection in completeTransition() and startTransition() SLIDE branch

## Decisions Made

- `hasPendingTransition` cleared to `false` BEFORE calling `applyDeferredTransition()` — if `onDeactivate()` calls `switchTo()`, it re-sets the flag for the NEXT frame rather than re-entering the current dispatch. This is the safe-queue guarantee.
- Forward declaration only (`class SceneStateMachine;`) in scene.hpp — no `#include "scene_state_machine.hpp"` which would create a circular include. scene_state_machine.hpp already includes scene.hpp.
- `m_ssm` is `protected` (not `private`) — matches `m_` prefix convention from component.hpp; enables derived TestScene to expose via `getSSM()` for test assertions.
- `switchTo()` is last-wins — multiple calls per frame overwrite `pendingSceneId`. Simplest correct policy with no allocation.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Scene back-pointer and deferred transition mechanism complete
- SceneStateMachine now has `getScene()` and `getCurrentScene()` alongside `switchTo()`
- Phase 31 (engine.* global table) and Phase 32 (ScriptProxy userdata) can proceed
- No circular include between scene.hpp and scene_state_machine.hpp — verified by clean cmake configure

---
*Phase: 30-scene-self-transitions*
*Completed: 2026-02-27*

## Self-Check: PASSED

- tests/scene_transition_test.cpp: FOUND
- include/enjin2/core/scene.hpp: FOUND
- include/enjin2/core/scene_state_machine.hpp: FOUND
- .planning/phases/30-scene-self-transitions/30-01-SUMMARY.md: FOUND
- Commit 7f50d7a (RED test): FOUND
- Commit 1dfb102 (GREEN impl): FOUND
