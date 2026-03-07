# Phase 60: CMake Foundation & Vendor - Research

**Researched:** 2026-03-07
**Domain:** CMake build system integration, nanobench single-header vendoring
**Confidence:** HIGH

## Summary

Phase 60 is a pure infrastructure phase: vendor nanobench as a single header, add a CMake option guard (`ENJIN2_BUILD_BENCHMARKS`), create a `benchmarks/` subdirectory with its own `CMakeLists.txt`, and add `bench-results/` to `.gitignore`. No benchmark code is written yet — only the scaffold that lets every subsequent phase (61–65) start immediately.

The project already uses the `vendor/` directory for `stb_image.h` and `stb_image_write.h`, making the vendor pattern established. The project's CMake style is plain `add_executable` / `target_link_libraries` / `target_include_directories` with `option()` guards — no exotic helpers. The pattern to follow is exactly how `ENJIN2_BUILD_TESTS` gates the `tests/` subdirectory in the root `CMakeLists.txt`.

**Primary recommendation:** Download `nanobench.h` v4.3.11 into `vendor/nanobench.h`, add an `INTERFACE` CMake target for it, add `option(ENJIN2_BUILD_BENCHMARKS ...)` to root `CMakeLists.txt`, and `add_subdirectory(benchmarks)` inside the guard. The benchmarks subdirectory creates one trivial smoke-test executable to verify the header compiles cleanly.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BENCH-01 | nanobench vendored as single header in vendor/ with CMake ENJIN2_BUILD_BENCHMARKS option | nanobench v4.3.11 is a single-header MIT library; vendor/ pattern is established in this project; CMake option guard mirrors ENJIN2_BUILD_TESTS |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| nanobench | v4.3.11 | Single-header C++11/17 microbenchmark library | MIT license, header-only, no dependencies, JSON output built-in via template API, lighter than Google Benchmark, explicitly chosen in REQUIREMENTS.md over Google Benchmark |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| CMake | >= 3.16 (already project minimum) | Build system; `option()`, `add_subdirectory()` | All build orchestration |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Vendored header | FetchContent | FetchContent adds network dependency at configure time; vendor avoids that; out-of-scope per REQUIREMENTS.md |
| Vendored header | Git submodule | Heavier VCS dependency; single header download is simpler for a stable release |
| nanobench | Google Benchmark | Heavier compile time, more boilerplate; explicitly excluded in REQUIREMENTS.md |

**Download:**
```bash
# One-time, run from project root
curl -L https://raw.githubusercontent.com/martinus/nanobench/v4.3.11/src/include/nanobench.h \
     -o vendor/nanobench.h
```

## Architecture Patterns

### Recommended Project Structure
```
vendor/
└── nanobench.h          # new — single-header, v4.3.11

benchmarks/
├── CMakeLists.txt       # new — benchmark targets (empty placeholder or smoke target)
└── bench_smoke.cpp      # new — trivial include test proving header compiles

bench-results/           # new — created at runtime by benchmarks; gitignored
```

Root `CMakeLists.txt` addition (after the `ENJIN2_BUILD_TESTS` block, before examples):
```cmake
option(ENJIN2_BUILD_BENCHMARKS "Build nanobench benchmark suite (desktop only)" OFF)

if(ENJIN2_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
```

### Pattern 1: INTERFACE target for the vendored header
**What:** Create a CMake `INTERFACE` library that exposes `vendor/` as an include path. Benchmark executables link against it to get `#include <nanobench.h>`.
**When to use:** Whenever a header-only library is vendored. Mirrors how `tests/CMakeLists.txt` uses `target_include_directories(... PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor)` for stb.

```cmake
# benchmarks/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# INTERFACE target for the vendored nanobench header
add_library(nanobench_vendor INTERFACE)
target_include_directories(nanobench_vendor INTERFACE
    ${CMAKE_SOURCE_DIR}/vendor
)

# Smoke target: proves vendor/nanobench.h is present and compiles
add_executable(bench_smoke bench_smoke.cpp)
target_link_libraries(bench_smoke PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
)
target_include_directories(bench_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
```

```cpp
// benchmarks/bench_smoke.cpp
// Source: nanobench v4.3.11 usage pattern (https://nanobench.ankerl.com/tutorial.html)
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.run("smoke", [&] {
        ankerl::nanobench::doNotOptimizeAway(1 + 1);
    });
    return 0;
}
```

### Pattern 2: Desktop-only guard (CRITICAL)
**What:** The `ENJIN2_BUILD_BENCHMARKS` option must never activate on ESP32 or WASM. nanobench uses Linux perf counters and `<chrono>` — it will not compile under ESP-IDF or Emscripten.
**When to use:** Enforced by the option default (OFF) and by documentation in CMakeLists.txt comments. The EMSCRIPTEN and ESP32 guards already exist elsewhere in root CMakeLists.txt; follow the same pattern.

```cmake
# In root CMakeLists.txt — safe because EMSCRIPTEN/ESP32 toolchains never set
# ENJIN2_BUILD_BENCHMARKS=ON in their toolchain files
option(ENJIN2_BUILD_BENCHMARKS "Build nanobench benchmark suite (desktop only)" OFF)

if(ENJIN2_BUILD_BENCHMARKS)
    if(EMSCRIPTEN OR ESP32)
        message(FATAL_ERROR "ENJIN2_BUILD_BENCHMARKS is not supported on WASM or ESP32 targets")
    endif()
    add_subdirectory(benchmarks)
endif()
```

### Pattern 3: nanobench implement macro per translation unit
**What:** nanobench is header-only but the implementation (template instantiations, statics) must be compiled exactly once. Define `ANKERL_NANOBENCH_IMPLEMENT` in exactly one `.cpp` file per binary.
**When to use:** Each benchmark binary (`bench_smoke.cpp`, later `bench_canvas.cpp`, etc.) defines the macro once in its own `main()` translation unit. Never define it in a shared `.cpp` compiled into a library.

```cpp
// Each bench_*.cpp starts with:
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>
```

### Anti-Patterns to Avoid
- **Defining `ANKERL_NANOBENCH_IMPLEMENT` in a header or shared file:** Causes multiply-defined symbol linker errors across benchmark executables. Define it in exactly one `.cpp` per binary.
- **Linking benchmark executables to `enjin2_lua` by default:** Lua is optional and brings Lua library dependency. Only include when benchmarking Lua (Phase 61 bench_lua target).
- **Putting `bench-results/` inside the build directory:** It needs to be at project root so scripts and CI can find JSON by conventional path.
- **Using `add_test()` for benchmark executables:** They are not pass/fail unit tests; they are measurement tools. Do not register them with CTest unless a smoke check is explicitly needed.
- **Enabling ENJIN2_BUILD_BENCHMARKS in CI on cross-compile jobs:** The option must remain OFF for all ESP32 and WASM workflows.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Timing hot code | Custom `chrono`-based loop | nanobench `Bench::run()` | nanobench handles warmup, epoch sampling, outlier detection, statistical reporting — a hand-rolled timer gets none of this |
| JSON output | Custom serializer | `ankerl::nanobench::templates::json()` | nanobench has a built-in mustache-like JSON template; Phase 61/62 will use it |
| Preventing dead-code elimination | `volatile` keyword | `ankerl::nanobench::doNotOptimizeAway()` | The library provides a portable, compiler-specific barrier; `volatile` has undefined behavior for this purpose in C++ |

**Key insight:** nanobench's single header already solves all measurement correctness problems (clock overhead subtraction, branch prediction warming, CPU frequency scaling detection). The only task in Phase 60 is making it available to subsequent phases.

## Common Pitfalls

### Pitfall 1: Header not found at compile time
**What goes wrong:** `fatal error: nanobench.h: No such file or directory` when building bench targets.
**Why it happens:** `target_include_directories` points to wrong path, or vendor/ was gitignored, or the file download step was skipped.
**How to avoid:** Verify `vendor/nanobench.h` exists before committing. The `nanobench_vendor` INTERFACE target encapsulates the include path so all benchmark executables inherit it automatically.
**Warning signs:** CMake configure succeeds but compile fails immediately on the first benchmark cpp.

### Pitfall 2: `ANKERL_NANOBENCH_IMPLEMENT` defined in multiple TUs
**What goes wrong:** Linker error `multiple definition of ankerl::nanobench::...` when building a benchmark binary with more than one `.cpp` file.
**Why it happens:** The IMPLEMENT macro instantiates static data and function bodies. If two `.cpp` files both define it before including the header, the linker sees two copies.
**How to avoid:** Each benchmark binary is a single `.cpp` file (one file, one main). This is the pattern used in all nanobench examples.
**Warning signs:** Linker error mentioning `ankerl::nanobench` symbols during benchmark target link step.

### Pitfall 3: Benchmark targets included in ESP32/WASM toolchain builds
**What goes wrong:** CMake configure error or cryptic compiler error when cross-compiling for ESP32 or WASM because nanobench uses POSIX headers (`<sys/syscall.h>`, `<linux/perf_event.h>`).
**Why it happens:** `ENJIN2_BUILD_BENCHMARKS` defaulted to OFF is not enough if a CI job explicitly passes `-DENJIN2_BUILD_BENCHMARKS=ON` without checking the toolchain.
**How to avoid:** Add the `EMSCRIPTEN OR ESP32` FATAL_ERROR guard in root CMakeLists.txt. Document in the option comment: "desktop only."
**Warning signs:** Build script passes `ENJIN2_BUILD_BENCHMARKS=ON` unconditionally and is later used in a cross-compile context.

### Pitfall 4: `bench-results/` accidentally committed
**What goes wrong:** JSON files with machine-specific timing data committed to the repo, causing spurious diffs.
**Why it happens:** Developer runs benchmarks and `git add .` picks up the results directory.
**How to avoid:** Add `bench-results/` to `.gitignore` as a BENCH-01 success criterion (already listed in phase success criteria).
**Warning signs:** `git status` shows `bench-results/` as an untracked directory after running benchmarks locally.

### Pitfall 5: CMake minimum_required version conflict in benchmarks/CMakeLists.txt
**What goes wrong:** CMake policy warnings or errors if the subdirectory declares a higher minimum than the root.
**Why it happens:** Forgetting that the project minimum is 3.16.
**How to avoid:** Use `cmake_minimum_required(VERSION 3.16)` in `benchmarks/CMakeLists.txt`, matching the root.

## Code Examples

Verified patterns from official sources and project conventions:

### Root CMakeLists.txt addition (option + guard)
```cmake
# Source: mirrors ENJIN2_BUILD_TESTS pattern already in root CMakeLists.txt
option(ENJIN2_BUILD_BENCHMARKS "Build nanobench benchmark suite (desktop only)" OFF)

if(ENJIN2_BUILD_BENCHMARKS)
    if(EMSCRIPTEN OR ESP32)
        message(FATAL_ERROR "ENJIN2_BUILD_BENCHMARKS is not supported on WASM or ESP32 targets")
    endif()
    add_subdirectory(benchmarks)
endif()
```

### benchmarks/CMakeLists.txt
```cmake
# Source: project convention from tests/CMakeLists.txt + nanobench tutorial
cmake_minimum_required(VERSION 3.16)

# Interface target wraps vendored nanobench header
add_library(nanobench_vendor INTERFACE)
target_include_directories(nanobench_vendor INTERFACE
    ${CMAKE_SOURCE_DIR}/vendor
)

# Smoke benchmark: verifies nanobench.h is present and compiles
add_executable(bench_smoke bench_smoke.cpp)
target_link_libraries(bench_smoke PRIVATE
    nanobench_vendor
    enjin2_core
    enjin2_graphics
)
target_include_directories(bench_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
```

### bench_smoke.cpp (trivial compile verification)
```cpp
// Source: nanobench v4.3.11 tutorial (https://nanobench.ankerl.com/tutorial.html)
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("smoke").warmup(3).epochs(5);
    bench.run("noop", [&] {
        ankerl::nanobench::doNotOptimizeAway(0);
    });
    return 0;
}
```

### .gitignore addition
```gitignore
# Benchmark JSON output (never commit machine-specific timing data)
bench-results/
```

### Verify configure without benchmarks (success criterion 3)
```bash
cmake -DENJIN2_BUILD_BENCHMARKS=OFF -B build_check .
# Must produce no "benchmarks" in output
```

### Verify configure with benchmarks (success criterion 1)
```bash
cmake -DENJIN2_BUILD_BENCHMARKS=ON -B build_bench .
cmake --build build_bench --target bench_smoke
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Google Benchmark (heavyweight) | nanobench single header | Ecosystem trend 2020+ | 10-50x faster compile, no external build dep, equivalent statistical quality |
| Hand-rolled chrono loops | `ankerl::nanobench::Bench::run()` | nanobench adoption | Warmup, epoch sampling, and outlier detection handled automatically |
| FetchContent for test deps | Vendor single headers | Project convention (stb_image already vendored) | No network dep at configure, reproducible offline builds |

**Deprecated/outdated:**
- Manually timing with `std::chrono::high_resolution_clock` directly: has unmeasured clock overhead and no statistical framework; replaced by nanobench's calibrated harness.
- Google Benchmark: explicitly out of scope per REQUIREMENTS.md (`## Out of Scope`).

## Open Questions

1. **Should `bench_smoke` be registered with `add_test()` for `ctest`?**
   - What we know: Existing tests use `add_test()`. Benchmark binaries are measurement tools, not pass/fail tests.
   - What's unclear: Whether CI should `ctest -R bench_smoke` as a compile check.
   - Recommendation: Do NOT add `add_test()` for benchmark executables. They are separate from the unit test suite. Phase 61 will define the build-and-run script (BENCH-06).

2. **nanobench perf counter availability on CI runner**
   - What we know: nanobench auto-detects Linux perf events and falls back gracefully when not available (shared/container runners often restrict perf_event_open).
   - What's unclear: Whether GitHub Actions runners support `perf_event_open`.
   - Recommendation: Noted for Phase 64 (CI). For Phase 60 this is irrelevant — the smoke test only needs the header to compile.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None — this phase produces CMake infrastructure, not C++ logic under test |
| Config file | N/A |
| Quick run command | `cmake -DENJIN2_BUILD_BENCHMARKS=ON -B /tmp/bench_test_build /home/unwn/git/enjin && cmake --build /tmp/bench_test_build --target bench_smoke` |
| Full suite command | Same — single target in this phase |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BENCH-01a | `cmake -DENJIN2_BUILD_BENCHMARKS=ON ..` configures without error | smoke/build | `cmake -DENJIN2_BUILD_BENCHMARKS=ON -B /tmp/b /home/unwn/git/enjin` | ❌ Wave 0 |
| BENCH-01b | `vendor/nanobench.h` exists and trivial include compiles | smoke/build | `cmake --build /tmp/b --target bench_smoke` | ❌ Wave 0 |
| BENCH-01c | `cmake -DENJIN2_BUILD_BENCHMARKS=OFF ..` produces no benchmark targets | smoke/build | `cmake -DENJIN2_BUILD_BENCHMARKS=OFF -B /tmp/b2 /home/unwn/git/enjin && cmake --build /tmp/b2 2>&1 \| grep -v bench` | ❌ Wave 0 |
| BENCH-01d | `bench-results/` is listed in `.gitignore` | file check | `grep bench-results/ /home/unwn/git/enjin/.gitignore` | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** Build `bench_smoke` target
- **Per wave merge:** Full configure + build of both ON and OFF variants
- **Phase gate:** All 4 success criteria verified before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `vendor/nanobench.h` — download nanobench v4.3.11 single header
- [ ] `benchmarks/CMakeLists.txt` — benchmark subdirectory CMake file
- [ ] `benchmarks/bench_smoke.cpp` — trivial smoke benchmark for compile verification
- [ ] Root `CMakeLists.txt` patch — add `ENJIN2_BUILD_BENCHMARKS` option and `add_subdirectory(benchmarks)` guard
- [ ] `.gitignore` patch — add `bench-results/` entry

## Sources

### Primary (HIGH confidence)
- nanobench GitHub releases (https://github.com/martinus/nanobench/releases) — confirmed v4.3.11 is latest, released 2024
- nanobench tutorial (https://nanobench.ankerl.com/tutorial.html) — single-header install pattern, `ANKERL_NANOBENCH_IMPLEMENT` macro usage, FetchContent example
- nanobench CMakeLists.txt (https://raw.githubusercontent.com/martinus/nanobench/master/CMakeLists.txt) — confirmed CMake target name `nanobench::nanobench`, interface include path `src/include`
- Project root CMakeLists.txt (read directly) — confirmed `cmake_minimum_required(VERSION 3.16)`, option pattern for `ENJIN2_BUILD_TESTS`, `add_subdirectory(tests)` guard
- Project vendor/ directory (read directly) — confirmed existing vendor pattern with `stb_image.h`, `stb_image_write.h`
- Project tests/CMakeLists.txt (read directly) — confirmed `target_include_directories(... PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../vendor)` for stb headers
- Project .gitignore (read directly) — confirmed current entries, `bench-results/` absent

### Secondary (MEDIUM confidence)
- nanobench reference docs (https://nanobench.ankerl.com/) — JSON template API `ankerl::nanobench::templates::json()` described, C++11/14/17/20 support confirmed

### Tertiary (LOW confidence)
- WebSearch results on CMake INTERFACE library patterns — general CMake best practices, not nanobench-specific

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — nanobench version verified via official GitHub releases page; single-header pattern verified via official tutorial
- Architecture: HIGH — directly mirrors existing project CMake patterns (tests/, vendor/ stb headers); no novel patterns introduced
- Pitfalls: HIGH — `ANKERL_NANOBENCH_IMPLEMENT` pitfall documented in nanobench tutorial; others derived from project's existing ESP32/WASM guard patterns

**Research date:** 2026-03-07
**Valid until:** 2026-09-07 (nanobench is stable; CMake patterns are project-internal)
