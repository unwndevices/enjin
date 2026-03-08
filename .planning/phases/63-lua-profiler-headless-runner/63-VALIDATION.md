---
phase: 63
slug: lua-profiler-headless-runner
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 63 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Project-own ASSERT macros (fprintf stderr) — same pattern as frame_timing_test.cpp, engine_table_test.cpp |
| **Config file** | `tests/CMakeLists.txt` — add `lua_profiler_test` entry |
| **Quick run command** | `cmake --build build --target lua_profiler_test && ./build/tests/lua_profiler_test` |
| **Full suite command** | `cmake --build build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build --target lua_profiler_test && ./build/tests/lua_profiler_test`
- **After every plan wave:** Run `cmake --build build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 63-01-01 | 01 | 1 | PROF-01 | unit | `./build/tests/lua_profiler_test` | ❌ W0 | ⬜ pending |
| 63-01-02 | 01 | 1 | PROF-02 | unit | `./build/tests/lua_profiler_test` | ❌ W0 | ⬜ pending |
| 63-01-03 | 01 | 1 | PROF-03 | unit | `./build/tests/lua_profiler_test` | ❌ W0 | ⬜ pending |
| 63-01-04 | 01 | 1 | PROF-06 | unit | `./build/tests/lua_profiler_test` | ❌ W0 | ⬜ pending |
| 63-02-01 | 02 | 2 | PROF-04 | smoke | `./build/enjin_run --frames 100 scripts/layer_demo.lua` | Manual | ⬜ pending |
| 63-02-02 | 02 | 2 | PROF-05 | smoke | `./build/enjin_run --profile --frames 10 scripts/layer_demo.lua` | Manual | ⬜ pending |
| 63-02-03 | 02 | 2 | PROF-05 | smoke | `./build/enjin_run --profile --output json --frames 10 scripts/layer_demo.lua` | Manual | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/lua_profiler_test.cpp` — unit tests for PROF-01, PROF-02, PROF-03, PROF-06
- [ ] Entry in `tests/CMakeLists.txt` for `lua_profiler_test` (links enjin2_lua)

*Existing infrastructure covers framework and fixture requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| `enjin_run --frames 100 script.lua` exits cleanly | PROF-04 | Requires headless binary built and script available | Build with headless target, run command, verify exit code 0 |
| `enjin_run --profile` prints sorted table | PROF-05 | Output format inspection | Run with `--profile`, verify column headers and sorted rows |
| `enjin_run --profile --output json` writes valid JSON | PROF-05 | JSON structure inspection | Run with `--output json`, pipe through `jq .` to validate |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
