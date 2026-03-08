---
phase: 66
slug: performance-documentation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 66 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Manual verification + shell smoke tests |
| **Config file** | none |
| **Quick run command** | `test -f docs/PERFORMANCE.md && bash scripts/build-bench.sh` |
| **Full suite command** | `bash scripts/build-bench.sh && cmake --build build-bench --target enjin_run && ./build-bench/src/platform/headless/enjin_run --profile --frames 10 scripts/features_demo.lua` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run `test -f docs/PERFORMANCE.md && bash scripts/build-bench.sh`
- **After every plan wave:** Run full suite command
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 66-01-01 | 01 | 1 | DOC-01 | smoke | `test -f docs/PERFORMANCE.md` | ❌ W0 | ⬜ pending |
| 66-01-02 | 01 | 1 | DOC-01 | smoke | `bash scripts/build-bench.sh` | ✅ | ⬜ pending |
| 66-01-03 | 01 | 1 | DOC-02 | manual-only | Read docs/PERFORMANCE.md for 5-step guide | ❌ W0 | ⬜ pending |
| 66-01-04 | 01 | 1 | DOC-03 | manual-only | Read docs/PERFORMANCE.md for frame budget table | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `docs/PERFORMANCE.md` — the entire output of this phase; no prior version exists

*Existing infrastructure covers all automated verification needs (build-bench.sh, enjin_run already functional).*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| All 5 subsystems documented with how-to-first structure | DOC-01 | Content quality cannot be machine-verified | Read each section, verify runnable commands appear before architecture explanation |
| Adding New Benchmarks has 5 concrete steps | DOC-02 | Step completeness is a content review | Read the section, verify Steps 1-5 each reference a real file |
| Frame budget table has ESP32/WASM/SDL3 rows with actual values | DOC-03 | Numbers need human verification against bench-results/ | Compare table values to bench-results/*.json |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
