---
phase: 62
slug: frame-timing-instrumentation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 62 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Project-own assertion macros (ASSERT macro, fprintf stderr) — same as debug_draw_test.cpp |
| **Config file** | tests/CMakeLists.txt — add `frame_timing_test` entry |
| **Quick run command** | `cmake --build build --target frame_timing_test && ./build/tests/frame_timing_test` |
| **Full suite command** | `cmake --build build && ctest --output-on-failure` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build/sdl3 --target enjin2_sdl && cmake --build build --target frame_timing_test && ./build/tests/frame_timing_test`
- **After every plan wave:** Run `cmake --build build && ctest --output-on-failure`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 62-01-01 | 01 | 1 | FRAME-01 | unit | `./build/tests/frame_timing_test` | ❌ W0 | ⬜ pending |
| 62-01-02 | 01 | 1 | FRAME-01 | compile-check | `cmake --build build` | ❌ W0 | ⬜ pending |
| 62-01-03 | 01 | 1 | FRAME-02 | unit | `cmake --build build/sdl3 --target enjin2_sdl` | ✅ | ⬜ pending |
| 62-01-04 | 01 | 1 | FRAME-03 | unit | `./build/tests/frame_timing_test` | ❌ W0 | ⬜ pending |
| 62-01-05 | 01 | 1 | FRAME-03 | compile-check | `cmake --build build` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/frame_timing_test.cpp` — stubs for FRAME-01 and FRAME-03 unit assertions
- [ ] Entry in `tests/CMakeLists.txt` for `frame_timing_test` executable (links `enjin2_core` only)
- [ ] `include/enjin2/instrumentation/frame_timing.hpp` — the new header

*Note: Wave 0 tasks create these files as part of the first plan.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| `--show-timing` displays overlay with 4 timing values | FRAME-02 | Requires visual confirmation of on-screen text | Run `./build/sdl3/enjin2_sdl --show-timing --script scripts/layer_demo.lua`, verify 4 lines of timing data visible |
| Timing values are non-zero and update each frame | FRAME-02 | Requires observing dynamic values across frames | Watch overlay for ~2 seconds, values should vary per frame |
| WASM build compiles with new header | FRAME-03 | Cross-compilation environment required | Run WASM build toolchain, verify clean compile |
| ESP32 build compiles with new header | FRAME-03 | Cross-compilation environment required | Run ESP32 build toolchain, verify clean compile |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
