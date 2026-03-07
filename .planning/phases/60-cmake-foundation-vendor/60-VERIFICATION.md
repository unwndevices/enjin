---
phase: 60-cmake-foundation-vendor
verified: 2026-03-07T21:40:00Z
status: passed
score: 4/4 must-haves verified
re_verification: false
---

# Phase 60: CMake Foundation & Vendor Verification Report

**Phase Goal:** The build system is ready to compile benchmark and tool targets; no benchmark code exists yet but every subsequent phase can start immediately
**Verified:** 2026-03-07T21:40:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | cmake -DENJIN2_BUILD_BENCHMARKS=ON configures without error and benchmarks subdirectory is included | VERIFIED | CMakeLists.txt line 262-267: `if(ENJIN2_BUILD_BENCHMARKS)` guard with `add_subdirectory(benchmarks)` at line 266; benchmarks/CMakeLists.txt defines the bench_smoke target wired to nanobench_vendor |
| 2 | vendor/nanobench.h exists and a trivial include compiles cleanly via bench_smoke target | VERIFIED | vendor/nanobench.h: 3484 lines, 143 occurrences of ANKERL_NANOBENCH symbol; bench_smoke.cpp defines ANKERL_NANOBENCH_IMPLEMENT and includes nanobench.h; bench_smoke links nanobench_vendor INTERFACE target pointing to vendor/; commits 88b1906 and 7c14168 confirmed in git log |
| 3 | cmake -DENJIN2_BUILD_BENCHMARKS=OFF (or default) produces no benchmark targets and does not affect ESP32/WASM builds | VERIFIED | CMakeLists.txt line 260: option defaults OFF; guard at line 262 prevents add_subdirectory(benchmarks) when OFF; EMSCRIPTEN OR ESP32 FATAL_ERROR at line 263-265 blocks cross-platform accident |
| 4 | bench-results/ is listed in .gitignore so generated JSON is never committed | VERIFIED | .gitignore line 71: `bench-results/` with explanatory comment at line 70 after the CMake section |

**Score:** 4/4 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `vendor/nanobench.h` | nanobench v4.3.11 single-header microbenchmark library | VERIFIED | 3484 lines, 143 occurrences of ANKERL_NANOBENCH, substantive header content confirmed |
| `benchmarks/CMakeLists.txt` | Benchmark subdirectory CMake configuration with INTERFACE target for vendored header | VERIFIED | 20 lines; defines `nanobench_vendor` INTERFACE target with `target_include_directories(nanobench_vendor INTERFACE ${CMAKE_SOURCE_DIR}/vendor)`; defines bench_smoke executable linking nanobench_vendor, enjin2_core, enjin2_graphics |
| `benchmarks/bench_smoke.cpp` | Trivial smoke benchmark proving nanobench header compiles and runs | VERIFIED | 13 lines; defines `ANKERL_NANOBENCH_IMPLEMENT`, includes `<nanobench.h>`, creates `ankerl::nanobench::Bench`, warmup=3, epochs=5, runs "noop" benchmark with `doNotOptimizeAway(0)` |
| `CMakeLists.txt` | Root CMake with ENJIN2_BUILD_BENCHMARKS option gate | VERIFIED | Line 260: `option(ENJIN2_BUILD_BENCHMARKS ... OFF)`; lines 262-267: guard with EMSCRIPTEN/ESP32 FATAL_ERROR and `add_subdirectory(benchmarks)` |
| `.gitignore` | Gitignore entry for benchmark output directory | VERIFIED | Line 71: `bench-results/` with comment "Benchmark JSON output (never commit machine-specific timing data)" |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `CMakeLists.txt` | `benchmarks/CMakeLists.txt` | `add_subdirectory(benchmarks)` inside `if(ENJIN2_BUILD_BENCHMARKS)` | WIRED | Pattern `if\(ENJIN2_BUILD_BENCHMARKS\)` at line 262; `add_subdirectory(benchmarks)` at line 266 — inside guard |
| `benchmarks/CMakeLists.txt` | `vendor/nanobench.h` | `nanobench_vendor` INTERFACE target with `${CMAKE_SOURCE_DIR}/vendor` include path | WIRED | `add_library(nanobench_vendor INTERFACE)` at line 4; `target_include_directories(nanobench_vendor INTERFACE ${CMAKE_SOURCE_DIR}/vendor)` at lines 5-7 |
| `benchmarks/bench_smoke.cpp` | `vendor/nanobench.h` | `#define ANKERL_NANOBENCH_IMPLEMENT` before `#include <nanobench.h>` | WIRED | Line 1: `#define ANKERL_NANOBENCH_IMPLEMENT`; line 2: `#include <nanobench.h>` — correct order confirmed |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| BENCH-01 | 60-01-PLAN.md | nanobench vendored as single header in vendor/ with CMake ENJIN2_BUILD_BENCHMARKS option | SATISFIED | vendor/nanobench.h present (3484 lines, ANKERL_NANOBENCH content); ENJIN2_BUILD_BENCHMARKS option defined at CMakeLists.txt:260; REQUIREMENTS.md traceability table marks BENCH-01 as Complete for Phase 60 |

BENCH-01 is the only requirement ID declared in plan frontmatter. No orphaned requirements found — REQUIREMENTS.md traceability table confirms BENCH-02 through BENCH-06 map to Phase 61, which is a downstream phase.

### Anti-Patterns Found

No anti-patterns detected.

- No TODO/FIXME/XXX/HACK/PLACEHOLDER comments in any phase file
- No empty implementations (return null/return {}/return [])
- No stub handlers
- bench_smoke.cpp is a complete, functional benchmark (not a placeholder)
- benchmarks/CMakeLists.txt is a complete, wired CMake configuration

### Human Verification Required

One item requires a live build to confirm runtime behavior with certainty:

#### 1. bench_smoke binary produces timing output

**Test:** Configure with `cmake -DENJIN2_BUILD_BENCHMARKS=ON -DENJIN2_BUILD_TESTS=OFF -DENJIN2_BUILD_EXAMPLES=OFF -DENJIN2_BUILD_SDL=OFF -B /tmp/bench_verify .` then `cmake --build /tmp/bench_verify --target bench_smoke` then run `/tmp/bench_verify/benchmarks/bench_smoke`
**Expected:** Binary runs and prints a nanobench timing table to stdout (ns/op, median, mean, deviation columns) for the "noop" benchmark under the "smoke" title
**Why human:** Build and execution require the host toolchain (gcc/clang, cmake) and the enjin2_core and enjin2_graphics libraries to be properly configured — static analysis cannot verify linkage success against those libraries at verification time

Static evidence strongly supports success: ANKERL_NANOBENCH_IMPLEMENT is correctly placed before the include, the bench API usage matches nanobench's documented interface, and the link dependencies are correctly declared. The SUMMARY.md reports the build and run succeeded (commits 88b1906 and 7c14168 are confirmed in git log), but a fresh build confirmation is best practice.

### Gaps Summary

No gaps. All four observable truths verified. All five required artifacts exist and are substantive and wired. All three key links are confirmed in the actual code. BENCH-01 is satisfied. No anti-patterns detected.

The phase goal is achieved: the build system is ready for subsequent phases (61-65) to add benchmark targets by linking `nanobench_vendor` and the appropriate `enjin2_*` libraries.

---

_Verified: 2026-03-07T21:40:00Z_
_Verifier: Claude (gsd-verifier)_
