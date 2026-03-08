---
phase: 64-ci-regression-pipeline
plan: 02
subsystem: infra
tags: [github-actions, benchmark, ci, bench-data, orphan-branch, git, cmake, lua, objectproxy]

# Dependency graph
requires:
  - phase: 64-ci-regression-pipeline
    plan: 01
    provides: "scripts/convert-bench.py and .github/workflows/benchmarks.yml pushed to main"
provides:
  - "(remote branch) bench-data: orphan branch initialized with first baseline data"
  - "dev/bench/index.html on bench-data: auto-generated performance dashboard (live)"
  - "dev/bench/data.js on bench-data: benchmark history time-series data (live)"
  - "Verified end-to-end CI regression pipeline — all 3 benchmark targets run clean on Ubuntu 24.04"
affects: [CI-pipeline-operation, bench-data-branch, first-baseline-recording, ObjectProxy-GC-safety]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - orphan-branch-initialization
    - git-checkout-orphan-workflow
    - ObjectProxy-__gc-metamethod-for-safe-GC
    - CMake-FindLua-include-dir-normalisation

key-files:
  created: []
  modified:
    - CMakeLists.txt
    - src/scripting/bindings_proxy.cpp

key-decisions:
  - "bench-data branch pushed with single empty commit — no files from main, clean orphan root"
  - "main branch pushed to origin simultaneously — 45 previously unpushed commits now live on remote"
  - "First CI run triggered manually via workflow_dispatch — push only touched .planning/ and scripts/, not src/** or include/**"
  - "FindLua sets LUA_INCLUDE_DIR (singular); CMakeLists used LUA_INCLUDE_DIRS (plural) — fixed by normalisation after find_package"
  - "ObjectProxy missing __gc metamethod — Lua GC freed proxy userdata while Object::m_luaProxy still held raw pointer; fixed by adding __gc to clear back-pointer before free"
  - "ObjectProxy __gc fix verified clean under ASAN (detect_leaks=0) before pushing to CI"

patterns-established:
  - "Orphan branch created locally then pushed — git checkout --orphan, git rm -rf ., git commit --allow-empty, git push"
  - "CMake FindLua: always normalise LUA_INCLUDE_DIR -> LUA_INCLUDE_DIRS after find_package(Lua) in desktop else branch"
  - "Lua userdata back-pointer pattern: ObjectProxy __gc must clear Object::m_luaProxy to prevent UAF on GC"

requirements-completed: [CI-03, CI-04, CI-05]

# Metrics
duration: ~2h (including two auto-fix bug hunts)
completed: 2026-03-08
---

# Phase 64 Plan 02: CI Regression Pipeline - bench-data Orphan Branch Summary

**bench-data orphan branch created and first baseline recorded — CI pipeline fully verified end-to-end with two auto-fixed bugs found during first CI run**

## Performance

- **Duration:** ~2 hours (Task 1 in 1 min; Task 2 continuation required two auto-fix cycles)
- **Started:** 2026-03-08T08:16:17Z
- **Completed:** 2026-03-08T10:05:00Z (approx)
- **Tasks:** 2 of 2 complete
- **Files modified:** 2 (CMakeLists.txt, src/scripting/bindings_proxy.cpp)

## Accomplishments

- Created `bench-data` orphan branch with single empty root commit (`6e1a779`)
- Pushed bench-data to `github.com:unwndevices/enjin.git` — branch now exists on remote
- Pushed main to origin (45 commits ahead, now synchronized) — includes `benchmarks.yml` and `convert-bench.py`
- Fixed CMakeLists.txt: `LUA_INCLUDE_DIR` -> `LUA_INCLUDE_DIRS` normalisation so `bench_lua` builds on CI (commit `4ea7950`)
- Fixed `bindings_proxy.cpp`: added `__gc` metamethod to ObjectProxy metatable to prevent heap-use-after-free when Lua GC frees proxy before `Object::~Object()` runs (commit `8b5ef35`)
- Triggered `workflow_dispatch` on GitHub Actions — CI run `22818886231` completed successfully (green)
- Verified: `git fetch origin bench-data && git log origin/bench-data --oneline` shows 3 commits (initial + 2 benchmark result pushes from github-action-benchmark)
- Verified: `dev/bench/data.js` and `dev/bench/index.html` both exist on bench-data branch — dashboard is live

## Task Commits

| Task | Description | Commit |
|------|-------------|--------|
| Task 1 | Create bench-data orphan branch on remote | `6e1a779` (on bench-data branch) |
| Auto-fix 1 | Fix LUA_INCLUDE_DIRS on desktop CI | `4ea7950` (on main) |
| Auto-fix 2 | Add ObjectProxy __gc metamethod (ASAN UAF fix) | `8b5ef35` (on main) |

## Files Created/Modified

- **Modified:** `CMakeLists.txt` — added `set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})` after `find_package(Lua)` in desktop branch; tightened `LUA_FOUND` check
- **Modified:** `src/scripting/bindings_proxy.cpp` — added `lua_objproxy_gc_impl` function and `__gc` entry in `registerObjectProxyMetatable()`

## Decisions Made

- main branch was 45 commits behind origin/main; pushed all pending commits as part of this plan so workflow YAML is live on remote
- First CI run used `workflow_dispatch` from the GitHub Actions tab — the initial push only touched `.planning/`, `.github/workflows/`, and `scripts/`, none matching `src/**` or `include/**` trigger paths
- Two blocking bugs found during first CI run attempt, auto-fixed under Rule 1 (broken behavior)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] CMake FindLua include directory mismatch on CI (Ubuntu 24.04)**
- **Found during:** Task 2 — first CI run (run 22817216812) failed with `lua.h: No such file or directory`
- **Issue:** `find_package(Lua QUIET)` sets `LUA_INCLUDE_DIR` (singular), but `target_include_directories(enjin2_lua ...)` consumed `${LUA_INCLUDE_DIRS}` (plural). The plural variable was only set explicitly for WASM and ESP32 FetchContent paths — on desktop it was empty, so Lua headers were never added to the compiler include path.
- **Fix:** Added `set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})` after `find_package(Lua)` in the desktop else branch. Also tightened found-check to `if(NOT LUA_FOUND AND NOT Lua_FOUND ...)`.
- **Files modified:** `CMakeLists.txt`
- **Commit:** `4ea7950`

**2. [Rule 1 - Bug] ObjectProxy heap-use-after-free when Lua GC collects proxy before Object destructs**
- **Found during:** Task 2 — second CI run (run 22818751543) failed with `malloc(): mismatching next->prev_size (unsorted)` after `lua GC: full collect` benchmark
- **Issue:** `registerObjectProxyMetatable()` did not register a `__gc` metamethod. When Lua GC collected the ObjectProxy userdata (after the `lua proxy: find+field round-trip` benchmark lambda returned and GC ran during `lua GC: full collect`), the proxy was freed. `Object::m_luaProxy` still held a raw pointer to this freed memory. `BenchScene::~BenchScene()` -> `Object::~Object()` then wrote `m_luaProxy->valid = false` into freed Lua heap — classic heap-use-after-free.
- **Diagnosis:** Reproduced under AddressSanitizer locally: `WRITE of size 1 at freed address in enjin2::Object::~Object() /home/unwn/git/enjin/src/core/object.cpp:12`.
- **Fix:** Added `lua_objproxy_gc_impl` static function: when Lua GC calls `__gc`, it calls `proxy->object->setLuaProxy(nullptr)` before the proxy memory is freed. `Object::~Object()` now finds `m_luaProxy == nullptr` and skips the write safely.
- **Files modified:** `src/scripting/bindings_proxy.cpp`
- **Commit:** `8b5ef35`
- **ASAN verified:** Re-ran bench_lua under `ASAN_OPTIONS=detect_leaks=0` — clean pass, all 7 benchmarks complete without sanitizer errors.

## CI Run History

| Run | Trigger | Result | Failure |
|-----|---------|--------|---------|
| 22817216812 | push (docs(64-01) commit) | FAIL | `lua.h: No such file or directory` — LUA_INCLUDE_DIRS empty |
| 22818751543 | workflow_dispatch (after LUA fix) | FAIL | `malloc(): mismatching next->prev_size` — ObjectProxy UAF in GC |
| 22818886231 | workflow_dispatch (after __gc fix) | PASS | — bench-data populated |

## Next Phase Readiness

- bench-data branch has 2 benchmark result commits — baseline data established
- Dashboard live at: https://unwndevices.github.io/enjin/dev/bench/ (auto-generated by github-action-benchmark)
- PR regression checking is active for all future PRs touching `src/**` or `include/**` — any 150%+ regression will fail the PR
- ObjectProxy GC safety now guaranteed — fixes a latent bug that affected any code path where a Lua proxy outlives its GC cycle (not just benchmarks)
- Phase 64 complete

---
*Phase: 64-ci-regression-pipeline*
*Completed: 2026-03-08*

## Self-Check: PASSED

- FOUND: bench-data on remote — 3 commits: `6e1a779` (init), `8d0e1ba`, `ae5ab27` (benchmark results)
- FOUND: `dev/bench/data.js` on origin/bench-data (window.BENCHMARK_DATA with entries for enjin2 Benchmarks)
- FOUND: `dev/bench/index.html` on origin/bench-data (auto-generated HTML dashboard)
- FOUND: CI run 22818886231 status: `success` (green)
- FOUND: commit `4ea7950` — fix(64-02): set LUA_INCLUDE_DIRS from FindLua singular LUA_INCLUDE_DIR
- FOUND: commit `8b5ef35` — fix(64-02): add __gc metamethod to ObjectProxy to prevent heap-use-after-free
