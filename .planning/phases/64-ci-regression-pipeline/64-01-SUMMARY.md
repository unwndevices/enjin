---
phase: 64-ci-regression-pipeline
plan: 01
subsystem: infra
tags: [github-actions, benchmark, ci, nanobench, python, customSmallerIsBetter, bench-data]

# Dependency graph
requires:
  - phase: 61-native-benchmark-suite
    provides: "bench_canvas, bench_ecs, bench_lua binaries writing bench-results/*.json"
  - phase: 63-lua-profiler-headless-runner
    provides: "build-bench.sh running all three benchmark binaries including bench_lua"
provides:
  - "scripts/convert-bench.py: nanobench JSON to customSmallerIsBetter conversion (27 benchmarks)"
  - ".github/workflows/benchmarks.yml: GitHub Actions CI pipeline with push-to-main history storage and PR regression checking"
affects: [64-02-seed-baseline, CI-pipeline-operation, bench-data-branch]

# Tech tracking
tech-stack:
  added: [benchmark-action/github-action-benchmark@v1, python3-stdlib-json-os-sys]
  patterns: [nanobench-to-customSmallerIsBetter-conversion, dual-path-CI-push-vs-PR, bench-data-orphan-branch-strategy]

key-files:
  created:
    - scripts/convert-bench.py
    - .github/workflows/benchmarks.yml
  modified: []

key-decisions:
  - "bench-data orphan branch used (not gh-pages) — docs.yml uses actions/deploy-pages which would wipe benchmark history on gh-pages"
  - "alert-threshold: 150% on PR regression check — shared runner variance too high for tighter threshold; tighten after 30-50 baseline runs"
  - "cancel-in-progress: false on concurrency group — interrupted auto-push leaves bench-data in partial state"
  - "workflow_dispatch trigger added — allows manual seed run since initial commits may not touch src/** or include/**"
  - "auto-push: false + save-data-file: false on PR step — prevents PRs from polluting benchmark history branch"

patterns-established:
  - "Two-conditional-step pattern: store step gated by push||workflow_dispatch, regression step gated by pull_request"
  - "Python stdlib-only conversion script: no external deps, shebang + executable, non-zero exit on missing input"

requirements-completed: [CI-01, CI-02, CI-03, CI-04, CI-05]

# Metrics
duration: 2min
completed: 2026-03-08
---

# Phase 64 Plan 01: CI Regression Pipeline - Conversion + Workflow Summary

**nanobench JSON to customSmallerIsBetter converter (27 benchmarks) + GitHub Actions push-to-main history storage and PR regression alerting at 150% threshold using bench-data branch**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-08T08:11:22Z
- **Completed:** 2026-03-08T08:13:34Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Created `scripts/convert-bench.py` — reads all three nanobench JSON files, converts median(elapsed) seconds to ns/op, maps medianAbsolutePercentError to ± X.XX% range, writes bench-combined.json with all 27 benchmarks
- Created `.github/workflows/benchmarks.yml` — full CI pipeline with liblua5.4-dev install, build-bench.sh, convert-bench.py, and dual-path steps (push-to-main stores history; PRs compare against baseline with fail-on-alert)
- Verified conversion produces exactly 27 entries with correct name/unit/value/range fields matching customSmallerIsBetter format
- All critical constraints met: bench-data branch (not gh-pages), cancel-in-progress:false, 150% threshold, workflow_dispatch trigger

## Task Commits

Each task was committed atomically:

1. **Task 1: Create nanobench-to-customSmallerIsBetter conversion script** - `17fc5c7` (feat)
2. **Task 2: Create GitHub Actions benchmarks workflow** - `25b266c` (feat)

**Plan metadata:** (docs commit — see below)

## Files Created/Modified

- `scripts/convert-bench.py` — reads bench_canvas.json, bench_ecs.json, bench_lua.json; converts to customSmallerIsBetter; exits non-zero if any file missing; produces bench-combined.json with 27 entries
- `.github/workflows/benchmarks.yml` — GitHub Actions workflow with push/PR/workflow_dispatch triggers, single benchmark job, two conditional action steps (store vs check-regression)

## Decisions Made

- Used bench-data branch (not gh-pages) — docs.yml uses actions/deploy-pages which performs full artifact replacement and would wipe nanobench history on gh-pages
- Set alert-threshold to 150% — shared GitHub Actions runners have 10-30% natural jitter; 150% was the project decision from STATE.md; tighten after 30-50 baseline runs
- cancel-in-progress: false — if a push-to-main workflow is cancelled mid-way through the auto-push step, bench-data can be left in a partial/corrupted state
- Added workflow_dispatch trigger — the initial commits that add scripts/convert-bench.py and benchmarks.yml don't touch src/** or include/**, so a manual trigger is needed for the first seed run
- auto-push: false + save-data-file: false on PR step — ensures PRs only read and compare against baseline without polluting the history branch

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- PyYAML not installed on the system (Python 3.14.3 without pip available) — used pattern-matching validation instead of YAML parsing; all 24 structural checks passed. This is a local tooling limitation, not a workflow correctness issue. The YAML was written directly from the plan specification and verified structurally.

## User Setup Required

**One-time manual step required before first CI run:**

The `bench-data` branch must exist as an orphan branch on the remote before the first workflow push run. Without it, the `auto-push` step will fail with a git error.

```bash
# Run once from the enjin repo:
git checkout --orphan bench-data
git rm -rf .
git commit --allow-empty -m "chore: initialize bench-data branch"
git push origin bench-data
git checkout main
```

This is documented in 64-RESEARCH.md Pattern 3 and will be addressed in Plan 02 (seed baseline).

## Next Phase Readiness

- Conversion script and workflow file are committed and ready to push
- Both artifacts will trigger CI immediately on next push to main touching src/** or include/**
- Plan 02 (seed baseline) must create the bench-data orphan branch and run the first baseline recording via workflow_dispatch
- Blockers: bench-data remote branch does not yet exist (one-time orphan branch creation needed)

---
*Phase: 64-ci-regression-pipeline*
*Completed: 2026-03-08*

## Self-Check: PASSED

- FOUND: scripts/convert-bench.py
- FOUND: .github/workflows/benchmarks.yml
- FOUND: 64-01-SUMMARY.md
- FOUND commit: 17fc5c7 (feat(64-01): create nanobench-to-customSmallerIsBetter conversion script)
- FOUND commit: 25b266c (feat(64-01): add GitHub Actions benchmark CI workflow)
