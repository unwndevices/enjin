---
phase: 64
slug: ci-regression-pipeline
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 64 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | No automated test framework for workflow YAML — validation is functional (run workflow) + unit (Python script) |
| **Config file** | `.github/workflows/benchmarks.yml` |
| **Quick run command** | `python3 scripts/convert-bench.py && python3 -c "import json; d=json.load(open('bench-results/bench-combined.json')); assert len(d)==27; print('OK')"` |
| **Full suite command** | Push to main branch; observe GitHub Actions run |
| **Estimated runtime** | ~5 seconds (local script); ~10 minutes (full CI run) |

---

## Sampling Rate

- **After every task commit:** Run `python3 scripts/convert-bench.py && python3 -c "import json; d=json.load(open('bench-results/bench-combined.json')); assert len(d)==27; print('OK')"`
- **After every plan wave:** Full workflow run on push to main
- **Before `/gsd:verify-work`:** All 5 success criteria manually verified
- **Max feedback latency:** 5 seconds (local); 10 minutes (CI)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 64-01-01 | 01 | 1 | CI-02 | unit | `python3 scripts/convert-bench.py && python3 -c "import json; d=json.load(open('bench-results/bench-combined.json')); assert len(d)==27; [assert set(['name','unit','value']).issubset(e) for e in d]; print('PASS')"` | ❌ W0 | ⬜ pending |
| 64-01-02 | 01 | 1 | CI-01 | smoke (manual) | Push commit touching `src/`; verify Actions tab shows run | ❌ W0 | ⬜ pending |
| 64-01-03 | 01 | 1 | CI-03 | manual | `git fetch origin bench-data && git log origin/bench-data` | ❌ W0 | ⬜ pending |
| 64-01-04 | 01 | 1 | CI-04 | manual | `git fetch origin bench-data && git show origin/bench-data:dev/bench/index.html` | ❌ W0 | ⬜ pending |
| 64-01-05 | 01 | 1 | CI-05 | smoke (manual) | Modify benchmark to inflate value; open PR; verify workflow fails | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `scripts/convert-bench.py` — JSON conversion script (CI-02)
- [ ] `.github/workflows/benchmarks.yml` — CI workflow file (CI-01, CI-03, CI-04, CI-05)
- [ ] Orphan branch `bench-data` created on remote (CI-03, CI-04)

*If none: "Existing infrastructure covers all phase requirements."*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Workflow triggers on push to src/** or include/** | CI-01 | Requires actual GitHub Actions runner | Push commit touching `src/`; verify Actions tab shows run |
| bench-data branch accumulates without touching gh-pages | CI-03 | Requires remote git state inspection | `git fetch origin bench-data && git log origin/bench-data` |
| Dashboard accessible at gh-pages URL | CI-04 | Requires GitHub Pages deployment | Navigate to performance dashboard URL |
| PR regression alert visible in Actions log | CI-05 | Requires PR workflow with inflated benchmark | Open PR with artificially slow benchmark; verify workflow failure |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s (local tasks)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
