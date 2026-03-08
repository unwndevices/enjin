---
phase: 65
slug: allocation-verification
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 65 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Self-validating binary (`bench_alloc`) — exits 0 on pass, 1 on first detected allocation |
| **Config file** | `benchmarks/CMakeLists.txt` (target: `bench_alloc`) |
| **Quick run command** | `"${BUILD_DIR}/benchmarks/bench_alloc"` |
| **Full suite command** | `bash scripts/build-bench.sh` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build "${BUILD_DIR}" --target bench_alloc && "${BUILD_DIR}/benchmarks/bench_alloc"`
- **After every plan wave:** Run `bash scripts/build-bench.sh`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 65-01-01 | 01 | 1 | ALLOC-01 | unit | `./bench_alloc` | ❌ W0 | ⬜ pending |
| 65-01-02 | 01 | 1 | ALLOC-03 | integration | `./bench_alloc` | ❌ W0 | ⬜ pending |
| 65-01-03 | 01 | 1 | ALLOC-02 | smoke (CI) | `bash scripts/build-bench.sh` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `include/enjin2/instrumentation/alloc_guard.hpp` — AllocGuard RAII class + extern declarations (ALLOC-01)
- [ ] `benchmarks/bench_alloc.cpp` — operator new override + hot-path guard sections (ALLOC-01, ALLOC-03)
- [ ] `benchmarks/CMakeLists.txt` — bench_alloc target with ENJIN2_ALLOC_VERIFICATION=1 (ALLOC-01)
- [ ] `scripts/build-bench.sh` — add bench_alloc to build targets and run step (ALLOC-02)
- [ ] `.github/workflows/benchmarks.yml` — explicit Allocation Verification step (ALLOC-02)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| CI job fails on hot-path allocation | ALLOC-02 | Requires pushing a commit to GitHub Actions | Push a branch with intentional alloc inside guard; verify Actions fails |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
