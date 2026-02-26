---
phase: quick-4
plan: 01
type: execute
wave: 1
depends_on: []
files_modified: [.gitignore]
autonomous: true
requirements: [QUICK-4]

must_haves:
  truths:
    - "All build_* directories are ignored by git"
    - ".cache/ directory is ignored by git"
    - "compile_commands.json is ignored by git"
    - "Previously tracked files remain tracked"
    - "git status shows only intentionally untracked files after update"
  artifacts:
    - path: ".gitignore"
      provides: "Comprehensive ignore rules for C++ CMake project"
      contains: "build_*"
  key_links: []
---

<objective>
Update .gitignore to cover all build artifact directories, tool caches, and generated files currently showing as untracked.

Purpose: Clean up git status noise from 12+ build directories (build_21_off, build_22_check, etc.), clangd cache, and CMake-generated compile_commands.json.
Output: Updated .gitignore
</objective>

<execution_context>
@/home/unwn/.claude/get-shit-done/workflows/execute-plan.md
@/home/unwn/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.gitignore
</context>

<tasks>

<task type="auto">
  <name>Task 1: Update .gitignore with missing patterns</name>
  <files>.gitignore</files>
  <action>
Read the current .gitignore and add the following missing sections/patterns. Preserve all existing content and ordering. Append new sections at the end, organized by category:

1. **Build directories** section — Add a glob pattern to catch all build variant directories:
   - `build_*/` — catches build_21_off, build_21_on, build_22_check, build_22_sdl_lua, build_22_sdl_nolua, build_24_check, build_24_lua, build_off, build_sdl_test, build_test_20, build_test_sprite, and any future build_* dirs
   - Note: `/build` and `/build_wasm` are already covered by existing rules

2. **Tool caches** section:
   - `.cache/` — clangd index cache

3. **CMake generated** section:
   - `compile_commands.json` — CMake compilation database (generated with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

Do NOT add patterns for:
- `project/` directory — contains design documents that may be committed later
- `tests/pikachu.h` — test asset header that may be committed later
- `tools/aseprite/enjin-export.lua` — already tracked, just modified

Use comment headers consistent with the existing .gitignore style (e.g., "# Build variant directories").
  </action>
  <verify>
Run `git status --short` and confirm that build_*, .cache/, and compile_commands.json no longer appear as untracked. Only project/, tests/pikachu.h, and the modified tools/aseprite/enjin-export.lua should remain.
  </verify>
  <done>
git status no longer shows build_21_off/, build_21_on/, build_22_check/, build_22_sdl_lua/, build_22_sdl_nolua/, build_24_check/, build_24_lua/, build_off/, build_sdl_test/, build_test_20/, build_test_sprite/, .cache/, or compile_commands.json as untracked files.
  </done>
</task>

</tasks>

<verification>
`git status --short` output contains at most: M tools/aseprite/enjin-export.lua, ?? project/, ?? tests/pikachu.h — no build_* directories, no .cache/, no compile_commands.json.
</verification>

<success_criteria>
- .gitignore updated with build_*, .cache/, and compile_commands.json patterns
- git status is clean of build artifact noise
- Existing ignore rules preserved unchanged
</success_criteria>

<output>
After completion, create `.planning/quick/4-update-gitignore/4-SUMMARY.md`
</output>
