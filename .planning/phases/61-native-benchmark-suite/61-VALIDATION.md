---
phase: 61
slug: native-benchmark-suite
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-07
---

# Phase 61 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Build verification — benchmark binaries are measurement tools, not pass/fail tests |
| **Config file** | benchmarks/CMakeLists.txt |
| **Quick run command** | `cmake -DENJIN2_BUILD_BENCHMARKS=ON -DENJIN2_BUILD_TESTS=OFF -DENJIN2_BUILD_EXAMPLES=OFF -B /tmp/b61 . && cmake --build /tmp/b61 --target bench_canvas bench_ecs bench_lua` |
| **Full suite command** | `bash scripts/build-bench.sh` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Build the relevant benchmark target and run it to verify JSON output
- **After every plan wave:** Run `bash scripts/build-bench.sh`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 61-01-01 | 01 | 1 | BENCH-02 | smoke/build | `cmake --build /tmp/b61 --target bench_canvas && /tmp/b61/benchmarks/bench_canvas` | ❌ W0 | ⬜ pending |
| 61-01-02 | 01 | 1 | BENCH-03 | smoke/build | `cmake --build /tmp/b61 --target bench_ecs && /tmp/b61/benchmarks/bench_ecs` | ❌ W0 | ⬜ pending |
| 61-01-03 | 01 | 1 | BENCH-04 | smoke/build | `cmake --build /tmp/b61 --target bench_lua && /tmp/b61/benchmarks/bench_lua` | ❌ W0 | ⬜ pending |
| 61-01-04 | 01 | 1 | BENCH-05 | file check | `test -f bench-results/bench_canvas.json && python3 -c "import json,sys; json.load(open('bench-results/bench_canvas.json'))"` | ❌ W0 | ⬜ pending |
| 61-01-05 | 01 | 1 | BENCH-06 | integration | `bash scripts/build-bench.sh && test -f bench-results/bench_ecs.json` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `benchmarks/bench_canvas.cpp` — Canvas4/Canvas8/LayerCompositor benchmark (BENCH-02)
- [ ] `benchmarks/bench_ecs.cpp` — Object/Scene/Component benchmark (BENCH-03)
- [ ] `benchmarks/bench_lua.cpp` — LuaEngine headless benchmark (BENCH-04)
- [ ] `benchmarks/CMakeLists.txt` — add three new executable targets
- [ ] `scripts/build-bench.sh` — orchestration script (BENCH-06)

*All source files created as part of Wave 1 execution.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Canvas timings non-trivially short at -O2 | BENCH-02 SC3 | Dead-code elimination is compiler-dependent; need human review of timing magnitudes | Run bench_canvas at -O2, verify pixel op timings > 1ns (not optimized away) |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
