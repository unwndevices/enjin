---
phase: 64-ci-regression-pipeline
verified: 2026-03-08T10:15:17Z
status: passed
score: 9/9 must-haves verified
re_verification: false
---

# Phase 64: CI Regression Pipeline Verification Report

**Phase Goal:** Every push to main automatically benchmarks the codebase, records results, and a public dashboard shows performance history; PRs are alerted when a regression threshold is crossed
**Verified:** 2026-03-08T10:15:17Z
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| #  | Truth                                                                                              | Status     | Evidence                                                                                               |
|----|----------------------------------------------------------------------------------------------------|------------|--------------------------------------------------------------------------------------------------------|
| 1  | convert-bench.py reads all three bench-results/*.json files and produces bench-combined.json       | VERIFIED   | Script runs, writes 27 entries; `python3 scripts/convert-bench.py` confirmed live against real data   |
| 2  | bench-combined.json contains exactly 27 entries in customSmallerIsBetter format                    | VERIFIED   | Runtime validation: `len(d)==27`, all entries have name/unit/value/range, unit is "ns/op"             |
| 3  | benchmarks.yml triggers on push to main and PRs when src/** or include/** files change             | VERIFIED   | Lines 4–12 of benchmarks.yml: push.branches=[main] paths=[src/**,include/**]; pull_request same paths |
| 4  | Push-to-main path stores results to bench-data branch with auto-push                               | VERIFIED   | Lines 40–51: `if: push\|\|workflow_dispatch`, `auto-push: true`, `gh-pages-branch: bench-data`       |
| 5  | PR path compares against baseline with 150% threshold and fail-on-alert                            | VERIFIED   | Lines 53–67: `if: pull_request`, `alert-threshold: '150%'`, `fail-on-alert: true`                    |
| 6  | bench-data orphan branch exists on remote with initial commit                                      | VERIFIED   | `git log origin/bench-data` shows `6e1a779 chore: initialize bench-data branch for benchmark history` |
| 7  | GitHub Actions workflow completed successfully on push to main                                     | VERIFIED   | CI run 22818886231 completed (green) after two auto-fixed bugs; bench-data populated with 2 data runs  |
| 8  | Benchmark history accumulates on bench-data after CI run                                           | VERIFIED   | `origin/bench-data` has 3 commits; data.js has 2 run entries each with 27 benchmark points            |
| 9  | Dashboard index.html is generated in dev/bench/ on bench-data branch                              | VERIFIED   | `git show origin/bench-data:dev/bench/index.html` returns valid HTML (DOCTYPE present)                |

**Score:** 9/9 truths verified

---

### Required Artifacts

| Artifact                               | Expected                                             | Status      | Details                                             |
|----------------------------------------|------------------------------------------------------|-------------|-----------------------------------------------------|
| `scripts/convert-bench.py`             | nanobench JSON to customSmallerIsBetter conversion   | VERIFIED    | 59 lines, executable (-rwxr-xr-x), shebang present  |
| `.github/workflows/benchmarks.yml`     | GitHub Actions CI workflow                           | VERIFIED    | 67 lines, valid YAML, all required sections present  |
| `(remote) bench-data`                  | Orphan branch for benchmark history storage          | VERIFIED    | 3 commits on origin/bench-data                       |
| `(remote) bench-data:dev/bench/index.html` | Auto-generated performance dashboard            | VERIFIED    | HTML dashboard file confirmed present on branch      |
| `(remote) bench-data:dev/bench/data.js`    | Benchmark history time-series data              | VERIFIED    | `window.BENCHMARK_DATA` with 2 runs of 27 points each |

---

### Key Link Verification

| From                            | To                              | Via                                   | Status   | Details                                                   |
|---------------------------------|---------------------------------|---------------------------------------|----------|-----------------------------------------------------------|
| `.github/workflows/benchmarks.yml` | `scripts/convert-bench.py`   | `python3 scripts/convert-bench.py`    | WIRED    | Line 38: `run: python3 scripts/convert-bench.py`          |
| `.github/workflows/benchmarks.yml` | `scripts/build-bench.sh`     | `bash scripts/build-bench.sh`         | WIRED    | Line 35: `run: bash scripts/build-bench.sh`               |
| `.github/workflows/benchmarks.yml` | `bench-results/bench-combined.json` | `output-file-path` input        | WIRED    | Lines 46+59: `output-file-path: bench-results/bench-combined.json` |
| `scripts/convert-bench.py`      | `bench-results/bench_canvas.json`   | `json.load` file read            | WIRED    | Line 20: `INPUT_FILES = ['bench_canvas.json', ...]`       |
| `.github/workflows/benchmarks.yml` | `(remote) bench-data`        | `github-action-benchmark auto-push`   | WIRED    | Lines 47+60: `gh-pages-branch: bench-data` (both steps)   |

---

### Requirements Coverage

| Requirement | Source Plan | Description                                                                          | Status    | Evidence                                                                      |
|-------------|-------------|--------------------------------------------------------------------------------------|-----------|-------------------------------------------------------------------------------|
| CI-01       | 64-01       | Workflow triggers on push to main and PRs touching src/** or include/**              | SATISFIED | benchmarks.yml lines 4–13: push + pull_request triggers with correct paths    |
| CI-02       | 64-01       | JSON conversion script combines results into customSmallerIsBetter format            | SATISFIED | convert-bench.py confirmed: 27 entries, name/unit/value/range, unit="ns/op"  |
| CI-03       | 64-01, 64-02| Benchmark history stored on dedicated branch (isolated from Docusaurus deployment)   | SATISFIED | bench-data orphan branch used; docs.yml uses gh-pages, no conflict possible   |
| CI-04       | 64-02       | Performance dashboard auto-generated from benchmark history                          | SATISFIED | dev/bench/index.html live on bench-data; dev/bench/data.js has 2 data runs   |
| CI-05       | 64-01       | Regression threshold with fail-on-alert for PRs                                     | SATISFIED | benchmarks.yml lines 65–67: alert-threshold: '150%', fail-on-alert: true      |

**Note on CI-03 wording:** REQUIREMENTS.md text says "gh-pages branch" but implementation correctly uses the dedicated `bench-data` orphan branch. This is a REQUIREMENTS.md documentation artifact — the intent (isolation from Docusaurus deployment) is fully satisfied and the `bench-data` strategy was an explicitly logged project decision (STATE.md) that supersedes the initial requirement wording.

---

### Anti-Patterns Found

No anti-patterns detected in any phase-64 modified files:
- `scripts/convert-bench.py` — no TODOs, no stubs, full implementation
- `.github/workflows/benchmarks.yml` — no TODOs, no placeholder steps
- `CMakeLists.txt` (auto-fix) — normalisation applied correctly
- `src/scripting/bindings_proxy.cpp` (auto-fix) — `__gc` metamethod fully implemented

---

### Human Verification Required

The following items were verified programmatically via `git show origin/bench-data` and CI run records documented in 64-02-SUMMARY.md. No additional human verification is needed, but one item is noted for informational completeness:

#### 1. PR Regression Alert Visibility

**Test:** Open a PR that touches `src/**` and artificially slows a benchmark by 2x, then check GitHub Actions output.
**Expected:** PR check fails with an alert comment on the PR indicating which benchmark regressed and by how much.
**Why human:** Requires an actual regressing PR against a live baseline. CI run 22818886231 verified the green (no-regression) path; the alert path requires a deliberate regression trigger.

This is informational only — the configuration (`fail-on-alert: true`, `comment-on-alert: true`, `alert-threshold: '150%'`) is present and wired. The tool (github-action-benchmark@v1) handles the alert logic.

---

### Auto-Fixed Bugs (Recorded for Completeness)

Two bugs were discovered and fixed during the first CI run as part of Plan 02 execution:

| Fix | File | Commit | Issue |
|-----|------|--------|-------|
| CMake FindLua singular/plural mismatch | `CMakeLists.txt` | `4ea7950` | `LUA_INCLUDE_DIRS` was empty on desktop CI; fixed by `set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})` after `find_package(Lua)` |
| ObjectProxy heap-use-after-free | `src/scripting/bindings_proxy.cpp` | `8b5ef35` | Missing `__gc` metamethod caused UAF when Lua GC freed proxy before `Object::~Object()` ran; fixed by `lua_objproxy_gc_impl` clearing `m_luaProxy` back-pointer |

Both fixes are verified in the codebase — commits exist on main and CI run 22818886231 passed after both were applied.

---

### Gaps Summary

None. All 9 observable truths verified, all 5 artifacts confirmed substantive and wired, all 4 key links confirmed, all 5 requirement IDs satisfied. Phase goal is fully achieved.

---

_Verified: 2026-03-08T10:15:17Z_
_Verifier: Claude (gsd-verifier)_
