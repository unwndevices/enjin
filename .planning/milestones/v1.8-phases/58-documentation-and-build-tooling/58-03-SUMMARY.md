---
phase: 58-documentation-and-build-tooling
plan: 03
subsystem: infra
tags: [docusaurus, lua, tutorial, async, coroutines, tween]

requires:
  - phase: 58-01
    provides: Tutorials sidebar category pre-wired with tutorials/async-coroutines entry
provides:
  - "Async Coroutines" tutorial covering all three yield primitives and the coroutine pool model
affects: []

tech-stack:
  added: []
  patterns: [One-page async tutorial covering all three primitives with concrete game scenario examples]

key-files:
  created:
    - docs/src/tutorials/async-coroutines.md
  modified: []

key-decisions:
  - "Covered all three primitives (wait, wait_frames, tween.await) plus cancelAll and coroutine pool info on a single page"
  - "Used concrete game scenarios (invincibility frames, message flash, slide animation) rather than abstract doSomething() placeholders"
  - "Combining Primitives section shows all three composing in a single coroutine to illustrate the real power"
  - "Correct API names used throughout: engine.async.start, engine.async.wait, engine.async.wait_frames, engine.tween.await"

patterns-established:
  - "Async tutorial pattern: explain semantics in 1-2 sentences, then a self-contained game-scenario code example"

requirements-completed: [DOC-03]

duration: 5min
completed: 2026-03-03
---

# Plan 58-03: "Async Coroutines" Tutorial Summary

**Single-page async tutorial covering engine.async.start, engine.async.wait, engine.async.wait_frames, and engine.tween.await with concrete game examples and a composition section**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-03T00:05:00Z
- **Completed:** 2026-03-03T00:10:00Z
- **Tasks:** 1
- **Files modified:** 1 (created)

## Accomplishments
- Created `docs/src/tutorials/async-coroutines.md` with correct Docusaurus frontmatter and doc ID `tutorials/async-coroutines`
- Covered all three async yield primitives: `engine.async.wait`, `engine.async.wait_frames`, `engine.tween.await`
- 7 Lua code fences — every primitive has its own working example plus a Combining Primitives composition section
- Zero wrong API names (no `coroutine.yield`, `engine.wait`, `tween.wait`, etc.)
- Docusaurus build passes with no broken links

## Task Commits

1. **Task 1: Write tutorial (combined with Plan 58-02)** — `d9e147a` — feat(58-02, 58-03): add Your First Script and Async Coroutines tutorials

## Files Created/Modified
- `docs/src/tutorials/async-coroutines.md` — full tutorial page

## Decisions Made
- Combined commit with Plan 58-02 since both were created in the same execution pass
- Added "Combining Primitives" section (beyond plan spec) to show all three yield primitives composing in a single coroutine — illustrates the real-world use case

## Deviations from Plan

None - plan executed as specified. The "Combining Primitives" section is within plan scope (the plan spec shows a composition example in the required structure).

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Tutorial exists at the sidebar-registered path — Docusaurus build confirms no broken link
- Both Wave 2 tutorials complete; all four DOC requirements implemented

---
*Phase: 58-documentation-and-build-tooling*
*Completed: 2026-03-03*
