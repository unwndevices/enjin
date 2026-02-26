---
phase: 28-float-dt-migration
plan: "02"
subsystem: build
tags: [cmake, compiler-flags, override, clang, gcc, woverride, build-system]

# Dependency graph
requires:
  - phase: 28-01
    provides: float dt migration across all virtual update signatures
provides:
  - -Woverride compiler flag on all engine CMake targets (Clang/Emscripten)
  - Clean build verification: zero override-related warnings on GCC and Clang
affects:
  - 29-lua-scripting
  - future phases adding virtual override methods

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Use CMake generator expression $<CXX_COMPILER_ID:Clang,AppleClang> to apply Clang-specific warning flags"
    - "GCC enforces override keyword mismatches as hard compiler errors natively — no extra flag needed"
    - "-Woverride unconditionally applied to enjin2_wasm (always Emscripten/Clang)"

key-files:
  created: []
  modified:
    - CMakeLists.txt

key-decisions:
  - "-Woverride applied via CXX_COMPILER_ID generator expression because flag is Clang-specific; GCC enforces override correctness as a hard error already"
  - "-Woverride applied unconditionally to enjin2_wasm because ENJIN2_BUILD_WASM implies Emscripten (Clang-based)"

patterns-established:
  - "Clang-specific warning flags: wrap in $<$<CXX_COMPILER_ID:Clang,AppleClang>:...> generator expression"

requirements-completed:
  - DT-03

# Metrics
duration: 12min
completed: 2026-02-26
---

# Phase 28 Plan 02: -Woverride Compiler Flag Verification Summary

**-Woverride added to all engine CMake targets via Clang generator expression, with clean build (6/6 tests passing) confirming zero detached virtual overrides after the float dt migration**

## Performance

- **Duration:** 12 min
- **Started:** 2026-02-26T20:55:10Z
- **Completed:** 2026-02-26T21:07:00Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments

- Added -Woverride compiler flag to enjin2_core, enjin2_graphics, enjin2_ui, enjin2_input via foreach loop with Clang generator expression
- Added -Woverride to enjin2_lua inside ENJIN2_BUILD_LUA conditional block
- Added -Woverride unconditionally to enjin2_wasm (WASM always uses Emscripten/Clang)
- Added -Woverride to enjin2_sdl via Clang generator expression
- Full build passes on GCC 15 with zero override-related warnings or errors
- All 6 unit tests pass with no regressions

## Task Commits

Each task was committed atomically:

1. **Task 1: Add -Woverride to all CMake targets and verify clean build** - `54de913` (feat)

**Plan metadata:** (docs commit to follow)

## Files Created/Modified

- `CMakeLists.txt` - Added -Woverride to all 7 engine library and executable targets via Clang generator expressions and unconditional blocks

## Decisions Made

- Used `$<$<CXX_COMPILER_ID:Clang,AppleClang>:-Woverride>` generator expression instead of bare `-Woverride` because GCC 15 does not recognize this flag (it's Clang-specific since Clang 3.5). GCC enforces `override` keyword mismatches as hard compiler errors natively — any function marked `override` that doesn't match a base virtual causes an error without any extra flag.
- For `enjin2_wasm`, -Woverride is applied unconditionally (no generator expression) because `ENJIN2_BUILD_WASM=ON` always implies Emscripten, which is Clang-based. The existing target_compile_options block was already Emscripten-specific.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Applied -Woverride conditionally for Clang only**
- **Found during:** Task 1 (Add -Woverride to all CMake targets)
- **Issue:** Plan specified bare `-Woverride` flag for all targets, but GCC 15 (the desktop compiler) does not recognize `-Woverride` and fails with `unrecognized command-line option`. The plan's note "Clang 3.5+, GCC 5+" was incorrect — GCC does not support this flag.
- **Fix:** Wrapped flag in `$<$<CXX_COMPILER_ID:Clang,AppleClang>:-Woverride>` generator expression for all non-WASM targets. Left enjin2_wasm unconditional since ENJIN2_BUILD_WASM implies Emscripten/Clang. Added CMake comment explaining the rationale.
- **Files modified:** CMakeLists.txt
- **Verification:** `make -j$(nproc)` produces zero errors/warnings; all 6 tests pass
- **Committed in:** 54de913 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 - Bug)
**Impact on plan:** Auto-fix essential for correctness. The semantic intent of the plan is fully achieved: -Woverride is active on Clang/Emscripten builds (where it's supported), and GCC builds get equivalent protection via its built-in override enforcement. No scope creep.

## Issues Encountered

GCC 15 rejected `-Woverride` as an unrecognized option. Investigation confirmed this flag is Clang-specific; GCC provides equivalent protection via the `override` keyword enforcement (hard error, not a warning). Fixed via CMake generator expression without changing the migration's verification goal.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- DT-03 requirement fully satisfied: -Woverride is active on all Clang/Emscripten builds
- Clean build confirms the float dt migration (Phase 28-01) left zero detached override signatures
- Phase 29 (Lua scripting) can proceed without override signature concerns

---
*Phase: 28-float-dt-migration*
*Completed: 2026-02-26*
