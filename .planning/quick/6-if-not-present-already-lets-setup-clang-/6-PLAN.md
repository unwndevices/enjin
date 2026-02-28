---
phase: quick-06
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - .clang-tidy
autonomous: true
requirements: []
must_haves:
  truths:
    - "Running clang-tidy on any source file produces structured diagnostic output"
    - "Checks are scoped to project code — vendor/luajit/build dirs are excluded"
    - "The config reflects embedded/game-engine constraints (no exceptions, minimal STL)"
  artifacts:
    - path: ".clang-tidy"
      provides: "clang-tidy configuration with check selection and per-check options"
      contains: "Checks:"
  key_links:
    - from: ".clang-tidy"
      to: "compile_commands.json"
      via: "clang-tidy reads compile_commands.json automatically when present at repo root"
      pattern: "compile_commands.json -> build/compile_commands.json"
---

<objective>
Create a `.clang-tidy` configuration file that enables useful static analysis checks
for the enjin2 C++17 embedded/game-engine codebase.

Purpose: Surface real bugs and warnings via clang-tidy without noise from vendor code
or checks incompatible with embedded targets (no exceptions, ESP32/Emscripten targets).

Output: `.clang-tidy` at repo root. clang-tidy immediately usable via
`clang-tidy -p build src/core/object.cpp` (compile_commands.json symlink already
in place at repo root).
</objective>

<execution_context>
@/home/unwn/.claude/get-shit-done/workflows/execute-plan.md
@/home/unwn/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/STATE.md

Existing tooling context:
- clang 21.1.8 at /usr/bin/clang, clang-tidy at /usr/bin/clang-tidy
- .clang-format already present (format config exists)
- compile_commands.json is a symlink at repo root -> build/compile_commands.json
  (built with GCC; clang-tidy parses the compile DB for include paths, defines, flags)
- CMakeLists.txt already uses $&lt;CXX_COMPILER_ID:Clang,AppleClang&gt; guard for -Woverride
- Codebase: C++17, no exceptions, zero dynamic allocation in hot paths
- Targets: desktop (GCC/Clang), ESP32 (Xtensa), Emscripten (Wasm)
- Vendor dirs to exclude: luajit/, vendor/, _deps/ (SDL3 via FetchContent), build/
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create .clang-tidy with pragmatic C++17 embedded engine checks</name>
  <files>.clang-tidy</files>
  <action>
    Create `.clang-tidy` at repo root with the following configuration.

    Check selection rationale:
    - Enable `bugprone-*` — catches real bugs (use-after-move, suspicious includes, etc.)
    - Enable `cppcoreguidelines-*` — C++ best practices; disable exception-based sub-checks
    - Enable `modernize-*` — C++17 modernisation; disable modernize-use-trailing-return-type (noise)
    - Enable `performance-*` — catch unnecessary copies, inefficient patterns
    - Enable `readability-*` — identifier naming, redundant expressions; disable readability-magic-numbers (embedded code has intentional constants)
    - Enable `clang-analyzer-*` — core static analysis (null deref, memory, logic)
    - Disable checks incompatible with embedded/no-exception style:
      - cppcoreguidelines-pro-type-vararg (printf is used intentionally for ESP32 compat)
      - cppcoreguidelines-avoid-magic-numbers (alias of readability-magic-numbers)
      - cppcoreguidelines-special-member-functions (existing POD-style structs)

    Per-check options:
    - readability-identifier-naming: enforce m_ prefix for private members (already used),
      k prefix for constants (not enforced — set to warn-only via WarnOnly)
    - Set `HeaderFilterRegex` to `include/enjin2/.*` so only project headers are checked,
      not Lua/SDL/vendor headers

    Exact content to write:

    ```yaml
    ---
    # clang-tidy configuration for enjin2
    # Targets: desktop (Clang/GCC), ESP32 (Xtensa), Emscripten (Wasm)
    # Constraints: C++17, no exceptions, no dynamic allocation in hot paths
    # Run: clang-tidy -p build <source-file>

    Checks: >
      bugprone-*,
      cppcoreguidelines-*,
      modernize-*,
      performance-*,
      readability-*,
      clang-analyzer-*,
      -cppcoreguidelines-pro-type-vararg,
      -cppcoreguidelines-avoid-magic-numbers,
      -cppcoreguidelines-special-member-functions,
      -cppcoreguidelines-pro-bounds-pointer-arithmetic,
      -modernize-use-trailing-return-type,
      -readability-magic-numbers,
      -readability-identifier-length,
      -modernize-avoid-c-arrays,
      -cppcoreguidelines-avoid-c-arrays,
      -cppcoreguidelines-pro-bounds-array-to-pointer-decay,
      -cppcoreguidelines-pro-bounds-constant-array-index

    WarningsAsErrors: ''

    HeaderFilterRegex: 'include/enjin2/.*'

    FormatStyle: file

    CheckOptions:
      - key: readability-identifier-naming.MemberPrefix
        value: 'm_'
      - key: readability-identifier-naming.PrivateMemberPrefix
        value: 'm_'
      - key: readability-identifier-naming.ProtectedMemberPrefix
        value: 'm_'
      - key: readability-identifier-naming.MemberCase
        value: camelBack
      - key: readability-identifier-naming.ParameterCase
        value: camelBack
      - key: readability-identifier-naming.LocalVariableCase
        value: camelBack
      - key: readability-identifier-naming.FunctionCase
        value: camelBack
      - key: readability-identifier-naming.ClassCase
        value: CamelCase
      - key: readability-identifier-naming.StructCase
        value: CamelCase
      - key: modernize-use-nullptr.NullMacros
        value: 'NULL'
      - key: performance-move-const-arg.CheckTriviallyCopyableMove
        value: 'true'
    ```

    Note: `WarningsAsErrors: ''` means no checks are promoted to errors by default.
    This keeps the first run informational — user can promote specific checks later.
    The `Checks:` multi-line block uses YAML `>` folding scalar so line breaks become spaces.
  </action>
  <verify>
    Run clang-tidy on a representative source file:
    `clang-tidy -p /home/unwn/dev/enjin/build /home/unwn/dev/enjin/src/core/object.cpp 2>&1 | head -40`

    Expected: Output shows check results or "no relevant changes found" — NOT a config parse error.
    A clean file may show zero warnings; any warnings should be real diagnostics, not clang-tidy
    startup errors about bad config.
  </verify>
  <done>
    `.clang-tidy` exists at repo root. Running `clang-tidy -p build src/core/object.cpp`
    produces diagnostic output without YAML parse errors or "unknown check" fatal errors.
    At least one source file can be checked end-to-end.
  </done>
</task>

<task type="auto">
  <name>Task 2: Verify clang-tidy runs cleanly across core source files</name>
  <files></files>
  <action>
    Run clang-tidy against a batch of core source files to confirm the config is functional
    and identify any immediate fixable issues.

    Run:
    ```
    clang-tidy -p /home/unwn/dev/enjin/build \
      /home/unwn/dev/enjin/src/core/object.cpp \
      /home/unwn/dev/enjin/src/core/scene.cpp \
      /home/unwn/dev/enjin/src/core/math.cpp \
      2>&1 | tail -30
    ```

    If there are "unknown check" errors from the config, remove the offending check name
    from `.clang-tidy`. Common false positives for clang 21:
    - Some cppcoreguidelines sub-checks may be renamed — remove any that error with
      "unknown check name"

    If there are real diagnostics (actual code warnings), document them in the task output
    but do NOT fix the source code in this plan — that is separate work. The goal here is
    a functional config, not a clean codebase.

    After verification passes, add a usage comment block at the top of `.clang-tidy`:
    ```
    # Usage:
    #   Single file:  clang-tidy -p build src/core/object.cpp
    #   All core:     clang-tidy -p build src/core/*.cpp
    #   With fixes:   clang-tidy -p build --fix src/core/object.cpp
    #   List checks:  clang-tidy --list-checks
    ```
  </action>
  <verify>
    `clang-tidy -p /home/unwn/dev/enjin/build /home/unwn/dev/enjin/src/core/math.cpp 2>&1 | grep -c "error: unknown check"` returns `0`

    No "unknown check name" errors appear. Diagnostics (if any) are genuine code analysis
    results, not config errors.
  </verify>
  <done>
    clang-tidy runs without config errors on at least 3 core source files.
    `.clang-tidy` has usage comments at the top.
    Any genuine diagnostics are noted in SUMMARY (not fixed here).
  </done>
</task>

</tasks>

<verification>
clang-tidy -p /home/unwn/dev/enjin/build /home/unwn/dev/enjin/src/core/object.cpp 2>&1 | grep -v "^$" | head -20
# Must not contain lines starting with "error: unknown check"
# May contain real diagnostics — those are informational
</verification>

<success_criteria>
- `.clang-tidy` exists at `/home/unwn/dev/enjin/.clang-tidy`
- Running `clang-tidy -p build src/core/object.cpp` from repo root produces analysis output
  without YAML or config parse errors
- `HeaderFilterRegex` scopes output to `include/enjin2/.*` (not vendor/luajit headers)
- Usage instructions in the file header so future sessions can invoke it without lookup
</success_criteria>

<output>
After completion, create `.planning/quick/6-if-not-present-already-lets-setup-clang-/6-SUMMARY.md`
with: what was created, any diagnostics found on first run, and a note on how to invoke.
</output>
