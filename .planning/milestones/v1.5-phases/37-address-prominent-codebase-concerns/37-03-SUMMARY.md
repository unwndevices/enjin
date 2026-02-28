---
phase: 37-address-prominent-codebase-concerns
plan: 03
subsystem: infra, testing
tags: [cmake, clang-tidy, ctest, lua, scripting, error-policy, input-callbacks]

# Dependency graph
requires:
  - phase: 37-02
    provides: ObjectProxy userdata + Object destructor hook; object_proxy_test passing

provides:
  - "clang-tidy CMake lint target (option CLANG_TIDY OFF by default; CI: -DCLANG_TIDY=ON)"
  - "14 stale build_* directories removed from project root"
  - "ERR-SIBLING test: Disable policy on one C_LuaScript does not block independent sibling"
  - "INPUT-03-ORDER test: call_order=='PU' independently confirms on_button_pressed fires before update()"
  - "GC audit: zero LUA_GCCOLLECT in src/ (only LUA_GCSTEP used — no stop-the-world in hot paths)"
  - "Zero-alloc audit: zero std::string errorMessage references (Plan 01 char[256] conversion confirmed)"
  - "All 10 CONCERNS.md checklist items closeable — phase complete"

affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "clang-tidy CMake integration: option(CLANG_TIDY OFF) + find_program + add_custom_target(lint)"
    - "file(GLOB_RECURSE CONFIGURE_DEPENDS) for source globbing in custom targets"
    - "--warnings-as-errors=* on clang-tidy command line (not in .clang-tidy file) keeps local dev lenient"

key-files:
  created:
    - path: ".planning/phases/37-address-prominent-codebase-concerns/37-03-SUMMARY.md"
      provides: "This file"
  modified:
    - path: "CMakeLists.txt"
      provides: "option(CLANG_TIDY) + add_custom_target(lint) with --warnings-as-errors=*"
    - path: "tests/error_policy_test.cpp"
      provides: "ERR-SIBLING test verifying Disable policy only disables the erroring component"
    - path: "tests/input_event_callback_test.cpp"
      provides: "INPUT-03-ORDER test confirming on_button_pressed fires before update() via 'PU' string"

key-decisions:
  - "CLANG_TIDY option OFF by default — CI explicitly opts in with -DCLANG_TIDY=ON; no dev friction"
  - "--warnings-as-errors=* on CLI not in .clang-tidy WarningsAsErrors — local dev can run without hard failure"
  - "test_input03_order_call_sequence() named differently from existing test_input03_callbacks_fire_before_update() — avoids duplicate symbol linker error; both tests coexist providing complementary verification"

patterns-established:
  - "CMake lint target pattern: option(CLANG_TIDY OFF) -> find_program -> GLOB_RECURSE CONFIGURE_DEPENDS -> add_custom_target(lint)"

requirements-completed: []

# Metrics
duration: 13min
completed: 2026-02-27
---

# Phase 37 Plan 03: Address Prominent Codebase Concerns Summary

**clang-tidy CMake lint target (option CLANG_TIDY OFF, CI opt-in), 14 stale build dirs removed, ERR-SIBLING and INPUT-03-ORDER verification tests added — all 18 ctests pass, Phase 37 complete**

## Performance

- **Duration:** 13 min
- **Started:** 2026-02-27T18:21:34Z
- **Completed:** 2026-02-27T18:34:35Z
- **Tasks:** 2
- **Files modified:** 3 (CMakeLists.txt, error_policy_test.cpp, input_event_callback_test.cpp)

## Accomplishments

- Added `option(CLANG_TIDY "..." OFF)` + `add_custom_target(lint)` to CMakeLists.txt; `cmake -B build -DCLANG_TIDY=ON` confirms "clang-tidy lint target enabled: cmake --build build --target lint" and "/usr/bin/clang-tidy"
- Removed 14 stale `build_*` directories from project root; only `build/` and `build_wasm.sh` remain
- GC hot-path audit: zero `LUA_GCCOLLECT` calls in src/ or include/ — only `LUA_GCSTEP` used (incremental, no stop-the-world spikes; Phase 35-01 decision confirmed intact)
- Zero-alloc audit: zero `std::string errorMessage` references — Plan 01 `char[256]{}` conversion fully complete
- ERR-SIBLING: `test_err_sibling_not_blocked()` verifies two independent C_LuaScript instances; error+Disable on scriptA does not touch scriptB; scriptB reaches `update_count==2` over two frames
- INPUT-03-ORDER: `test_input03_order_call_sequence()` independently confirms `call_order=="PU"` — on_button_pressed fires before update() in same frame; complements existing INPUT-03 test
- All 18 ctests pass (zero regressions)

## Task Commits

Each task was committed atomically:

1. **Task 1: clang-tidy CMake lint target + build directory cleanup** - `b528fab` (chore)
2. **Task 2: Error policy sibling test + input frame-order test** - `f3d299c` (test)

**Plan metadata:** (docs commit — see final commit below)

## Files Created/Modified

- `CMakeLists.txt` - Added option(CLANG_TIDY OFF) block with find_program, file(GLOB_RECURSE), add_custom_target(lint), --warnings-as-errors=*
- `tests/error_policy_test.cpp` - Added test_err_sibling_not_blocked() + main() call
- `tests/input_event_callback_test.cpp` - Added test_input03_order_call_sequence() + main() call

## Decisions Made

- `CLANG_TIDY` option OFF by default so developers do not get clang-tidy analysis overhead on every configure; CI opts in explicitly with `-DCLANG_TIDY=ON`
- `--warnings-as-errors=*` placed on the `clang-tidy` command line (not in `.clang-tidy` WarningsAsErrors field) — keeps local dev runs advisory while CI enforces zero warnings
- `test_input03_order_call_sequence()` uses a different function name and different Lua global variable (`call_order` vs `callback_order`) from the existing `test_input03_callbacks_fire_before_update()` to avoid duplicate symbol linker error; both coexist providing complementary call-order verification

## Deviations from Plan

None - plan executed exactly as written.

## Audit Results

### GC Hot-Path Audit

Command: `grep -rn "LUA_GCCOLLECT" /home/unwn/dev/enjin/src/ /home/unwn/dev/enjin/include/ 2>/dev/null`

**Result: zero matches**

Only `LUA_GCSTEP` is used in hot paths (bindings_engine.cpp line ~319 includes a comment explaining this choice). No stop-the-world GC calls exist in any hot-path source.

### Zero-Alloc errorMessage Audit

Command: `grep -rn "std::string errorMessage\|errorMessage\.clear()\|errorMessage\.c_str" /home/unwn/dev/enjin/src/ /home/unwn/dev/enjin/include/ 2>/dev/null`

**Result: zero matches**

All `std::string errorMessage` usages were converted to `char[256]{}` in Plan 01. The `getErrorMessage()` accessor returns `const char*`. Zero heap allocation on all error paths confirmed.

## Issues Encountered

None — build succeeded on first attempt, all tests passed immediately.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Phase 37 is complete. All 10 CONCERNS.md checklist items are now closeable:

1. ObjectProxy userdata (Phase 37-01, 37-02) — done
2. char[256] errorMessage (Phase 37-01) — done; audit confirmed
3. Tag method dispatch (Phase 37-01) — done
4. clang-tidy CMake lint target (Phase 37-03) — done
5. Stale build directories (Phase 37-03) — done
6. ERR-SIBLING verification (Phase 37-03) — done
7. INPUT-03-ORDER verification (Phase 37-03) — done
8. GC LUA_GCCOLLECT audit (Phase 37-03) — zero matches confirmed
9. Zero-alloc errorMessage audit (Phase 37-03) — zero matches confirmed
10. All ctests pass (Phase 37-03) — 18/18 pass

v1.5 Lua Scripting Foundation milestone is complete.

---
*Phase: 37-address-prominent-codebase-concerns*
*Completed: 2026-02-27*
