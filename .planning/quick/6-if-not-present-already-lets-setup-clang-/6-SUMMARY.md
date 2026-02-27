---
phase: quick-06
plan: "01"
subsystem: tooling
tags: [clang-tidy, static-analysis, tooling, linting]
dependency_graph:
  requires: [compile_commands.json]
  provides: [.clang-tidy static analysis config]
  affects: [all C++ source files under src/]
tech_stack:
  added: [clang-tidy 21.1.8]
  patterns: [HeaderFilterRegex to scope vendor exclusion, YAML folding scalar for Checks]
key_files:
  created: [.clang-tidy]
  modified: []
decisions:
  - WarningsAsErrors left empty for informational first run; promote specific checks later
  - Disabled c-arrays and bounds checks for ESP32/Emscripten C-array compat
  - HeaderFilterRegex=include/enjin2/.* excludes luajit/SDL3/vendor headers from noise
  - LUA_GCSTEP pattern already reflected in GC decisions from Phase 35
metrics:
  duration: "~5 minutes"
  completed: "2026-02-27T16:26:12Z"
  tasks_completed: 2
  files_created: 1
  files_modified: 1
---

# Quick Task 6: clang-tidy Setup Summary

## One-liner

clang-tidy 21.1.8 configured for C++17 embedded engine with bugprone/cppcoreguidelines/modernize/performance/readability/clang-analyzer checks scoped to include/enjin2/.* headers.

## What Was Created

### `.clang-tidy` (repo root)

A YAML configuration file enabling six check categories:

| Category | Purpose |
|---|---|
| `bugprone-*` | Catches real bugs: use-after-move, suspicious includes, etc. |
| `cppcoreguidelines-*` | C++ best practices (exception-based sub-checks disabled) |
| `modernize-*` | C++17 modernisation (trailing-return-type excluded as noisy) |
| `performance-*` | Unnecessary copies, inefficient patterns |
| `readability-*` | Identifier naming, redundant expressions |
| `clang-analyzer-*` | Core static analysis: null deref, memory, logic |

**Disabled for embedded/ESP32 compat:**
- `cppcoreguidelines-pro-type-vararg` — printf intentional for ESP32
- `cppcoreguidelines-avoid-magic-numbers` / `readability-magic-numbers` — embedded has intentional constants
- `cppcoreguidelines-special-member-functions` — POD-style structs exist
- `cppcoreguidelines-pro-bounds-*` / `modernize-avoid-c-arrays` / `cppcoreguidelines-avoid-c-arrays` — C arrays used for ESP32/Wasm compat
- `modernize-use-trailing-return-type` / `readability-identifier-length` — noisy, no value

**Key config:**
- `HeaderFilterRegex: 'include/enjin2/.*'` — only project headers checked, not luajit/SDL3/vendor
- `WarningsAsErrors: ''` — informational mode; no hard failures on first run
- `FormatStyle: file` — reads `.clang-format` for fix formatting

**Identifier naming enforcement:**
- Private/protected/member prefix: `m_`
- Member/parameter/local/function case: `camelBack`
- Class/struct case: `CamelCase`

## Invocation

```bash
# Single file
clang-tidy -p build src/core/object.cpp

# All core source files
clang-tidy -p build src/core/*.cpp

# With automatic fixes applied
clang-tidy -p build --fix src/core/object.cpp

# List all enabled checks
clang-tidy --list-checks
```

The `compile_commands.json` symlink at repo root (`-> build/compile_commands.json`) is picked up automatically.

## First-Run Diagnostics (Informational)

Running on `src/core/object.cpp` produced 35,504 warnings (most suppressed as non-user-code). Representative genuine diagnostics found:

| File | Check | Issue |
|---|---|---|
| `include/enjin2/core/memory.hpp` | `readability-identifier-naming` | `storage`, `used`, `count` fields lack `m_` prefix |
| `include/enjin2/core/memory.hpp` | `cppcoreguidelines-use-default-member-init` | `count` should use `= 0` default member init |
| `include/enjin2/core/memory.hpp` | `cppcoreguidelines-pro-type-member-init` | Constructor doesn't initialise `storage`, `used` fields |
| `src/core/object.cpp` | `readability-braces-around-statements` | Single-line `if` guards without braces |
| `src/core/object.cpp` | `readability-implicit-bool-conversion` | `if (!position)` should be `if (position == nullptr)` |
| `src/core/object.cpp` | `readability-qualified-auto` | `auto drawable` should be `auto* drawable` |

These are genuine analysis results — **not config errors.** Fixing them is separate work (not part of this task).

## Verification Results

- `clang-tidy -p build src/core/object.cpp` — zero "unknown check" errors
- `clang-tidy -p build src/core/math.cpp` — zero "unknown check" errors
- `clang-tidy -p build src/core/scene.cpp` — zero "unknown check" errors
- All output is genuine static analysis, not config parse failures

## Commits

| Hash | Description |
|---|---|
| `c45a418` | `chore(quick-06): add .clang-tidy with pragmatic C++17 embedded engine checks` |
| `c143f2f` | `chore(quick-06): add usage comments to .clang-tidy after batch verification` |

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check

- [x] `.clang-tidy` exists at `/home/unwn/dev/enjin/.clang-tidy`
- [x] Zero "unknown check" errors on object.cpp, scene.cpp, math.cpp
- [x] `HeaderFilterRegex` set to `include/enjin2/.*`
- [x] Usage comments present in file header
- [x] Both commits exist: c45a418, c143f2f
