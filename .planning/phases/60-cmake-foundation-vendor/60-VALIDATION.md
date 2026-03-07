---
phase: 60
slug: cmake-foundation-vendor
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-07
---

# Phase 60 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — this phase produces CMake infrastructure, not C++ logic under test |
| **Config file** | N/A |
| **Quick run command** | `cmake -DENJIN2_BUILD_BENCHMARKS=ON -B /tmp/bench_test_build . && cmake --build /tmp/bench_test_build --target bench_smoke` |
| **Full suite command** | Same — single target in this phase |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build /tmp/bench_test_build --target bench_smoke`
- **After every plan wave:** Run full configure + build of both ON and OFF variants
- **Before `/gsd:verify-work`:** All 4 success criteria verified
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 60-01-01 | 01 | 1 | BENCH-01a | smoke/build | `cmake -DENJIN2_BUILD_BENCHMARKS=ON -B /tmp/b .` | ❌ W0 | ⬜ pending |
| 60-01-02 | 01 | 1 | BENCH-01b | smoke/build | `cmake --build /tmp/b --target bench_smoke` | ❌ W0 | ⬜ pending |
| 60-01-03 | 01 | 1 | BENCH-01c | smoke/build | `cmake -DENJIN2_BUILD_BENCHMARKS=OFF -B /tmp/b2 .` | ❌ W0 | ⬜ pending |
| 60-01-04 | 01 | 1 | BENCH-01d | file check | `grep bench-results/ .gitignore` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `vendor/nanobench.h` — download nanobench v4.3.11 single header
- [ ] `benchmarks/CMakeLists.txt` — benchmark subdirectory CMake file
- [ ] `benchmarks/bench_smoke.cpp` — trivial smoke benchmark for compile verification
- [ ] Root `CMakeLists.txt` patch — add `ENJIN2_BUILD_BENCHMARKS` option and guard
- [ ] `.gitignore` patch — add `bench-results/` entry

*All Wave 0 items are created by Phase 60 plan tasks.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|

*All phase behaviors have automated verification.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
