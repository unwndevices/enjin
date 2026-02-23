# Phase 11: Documentation Tracking Improvements - Research

**Researched:** 2026-02-23
**Domain:** CI/CD, Doxygen warning verification, GitHub Actions
**Confidence:** HIGH

## Summary

Phase 11 adds automated Doxygen warning count verification to the existing CI/CD workflow (`.github/workflows/docs.yml`). The current workflow already runs Doxygen to generate XML for the documentation pipeline but does not inspect or enforce the warning count. The Doxyfile already configures `WARN_LOGFILE = doxygen-warnings.log`, so warnings are captured to a file during every Doxygen run.

The implementation is straightforward: add a CI step after the "Generate Doxygen XML" step that counts lines in the warning log file and fails the build if the count exceeds a threshold (20). No new tools, libraries, or complex infrastructure are needed — this is pure shell scripting within the existing GitHub Actions workflow.

**Primary recommendation:** Add a single GitHub Actions step after Doxygen generation that counts warning lines and exits non-zero if count exceeds 20.

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| GitHub Actions | v4 actions | CI/CD platform | Already in use in `.github/workflows/docs.yml` |
| Doxygen | apt package | Documentation generation with warning output | Already installed in CI workflow |
| Bash / `wc -l` | system | Count warning lines | Zero-dependency, standard Unix tooling |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `grep -c` | system | Alternative line counting with filtering | If warning log contains non-warning lines |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Line counting in shell | Doxygen `WARN_AS_ERROR = FAIL_ALL` | Too strict — fails on ANY warning, no threshold support. Would require 0 warnings first (currently 311). |
| Custom shell step | Dedicated Doxygen linting action | Over-engineered for a line count check |

## Architecture Patterns

### Current CI Workflow Structure
```
.github/workflows/docs.yml
├── Checkout
├── Setup Node.js
├── Install dependencies (npm)
├── Install Doxygen and Graphviz
├── Generate Doxygen XML          ← Doxygen runs here, writes doxygen-warnings.log
├── Generate API documentation    ← Node script processes XML
├── Build Docusaurus site
├── Setup Pages
├── Upload artifact
└── Deploy to GitHub Pages
```

### Pattern: Warning Threshold Gate
**What:** A CI step that reads the warning log, counts lines, compares against a threshold, and fails if exceeded.
**When to use:** After the Doxygen generation step, before proceeding to build docs.

**Key consideration — WARN_LOGFILE path:**
The Doxyfile sets `WARN_LOGFILE = doxygen-warnings.log` (relative path). The CI workflow runs Doxygen from the `build/` directory via CMake:
```yaml
- name: Generate Doxygen XML
  run: |
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DENJIN2_BUILD_LUA=OFF
    cmake --build . --target docs
```

The CMake `docs` target runs Doxygen with `WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}` (the repo root), so the warning log is written to the **repo root** (`doxygen-warnings.log`), NOT inside `build/`. This is confirmed by the existing 311-line warning log at repo root.

**Example implementation:**
```yaml
- name: Check Doxygen warning count
  run: |
    WARNING_FILE="doxygen-warnings.log"
    THRESHOLD=20
    if [ ! -f "$WARNING_FILE" ]; then
      echo "::error::Doxygen warning log not found at $WARNING_FILE"
      exit 1
    fi
    COUNT=$(wc -l < "$WARNING_FILE")
    echo "Doxygen warnings: $COUNT (threshold: $THRESHOLD)"
    if [ "$COUNT" -gt "$THRESHOLD" ]; then
      echo "::error::Doxygen warning count ($COUNT) exceeds threshold ($THRESHOLD)"
      echo "Top warnings:"
      head -20 "$WARNING_FILE"
      exit 1
    fi
    echo "::notice::Doxygen warning count ($COUNT) is within threshold ($THRESHOLD)"
```

### Anti-Patterns to Avoid
- **Using `WARN_AS_ERROR`:** Doxygen supports `WARN_AS_ERROR = YES` (stops at first warning) and `FAIL_ALL` (reports all then fails). Neither allows a threshold — they require zero warnings. Since the codebase currently has 311 warnings, this would break CI until Phase 12 fixes them.
- **Checking warning count in the CMake target:** Would couple the gate to the build system rather than CI, making it harder to adjust thresholds and losing GitHub Actions annotation support (`::error::` syntax).
- **Parsing warning content instead of counting:** Over-engineering. Line count is sufficient for threshold enforcement. Warning content analysis belongs in Phase 12 (fixing warnings).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Warning counting | Custom parser | `wc -l` | Warning log has one warning per line; simple line count is accurate |
| CI step failure | Custom exit codes | `exit 1` with `::error::` | GitHub Actions native annotation support |
| Warning log creation | Custom log capture | Doxygen's `WARN_LOGFILE` | Already configured in Doxyfile |

**Key insight:** The Doxyfile already does all the heavy lifting. The CI step is purely a gate — read file, count lines, compare number.

## Common Pitfalls

### Pitfall 1: Warning Log Not Created
**What goes wrong:** The CI step fails because `doxygen-warnings.log` doesn't exist.
**Why it happens:** Doxygen might not run (CMake target not built, Doxygen not found), or the working directory assumption is wrong.
**How to avoid:** Check file existence before counting. Also verify the file path matches where Doxygen actually writes (repo root, per CMake `WORKING_DIRECTORY`).
**Warning signs:** CI step exits with "file not found" instead of a count.

### Pitfall 2: Warning Log Path Mismatch
**What goes wrong:** Counting warnings in the wrong file or location.
**Why it happens:** The Doxyfile uses a relative path (`doxygen-warnings.log`). If Doxygen's working directory changes, the file lands elsewhere.
**How to avoid:** The CMake target sets `WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}`, so the file is at repo root. The CI step must also reference it from repo root (the default working directory in GitHub Actions).
**Warning signs:** Warning count is 0 when it shouldn't be, or file not found.

### Pitfall 3: Stale Warning Log
**What goes wrong:** The CI reads a cached/stale warning log from a previous run.
**Why it happens:** If the Doxygen step is skipped (e.g., cached build), the old log file might remain.
**How to avoid:** Doxygen overwrites the log file on each run (it truncates/recreates). As long as Doxygen runs, the log is fresh. If adding caching for the build directory, ensure the warning log is excluded.
**Warning signs:** Warning count doesn't change across commits.

### Pitfall 4: Multi-Line Warnings
**What goes wrong:** Some Doxygen warnings span multiple lines, making `wc -l` count inaccurate.
**Why it happens:** Warnings about undocumented parameters can include continuation lines (e.g., "parameter 'arg'" on the next line).
**How to avoid:** Count lines starting with a file path pattern (`grep -c "^/"`) or count lines containing `: warning:` specifically. Looking at the actual log, continuation lines like `  parameter 'arg'` do exist, so `grep -c ": warning:"` is more accurate than `wc -l`.
**Warning signs:** Line count (311) differs from actual warning count.

**IMPORTANT:** The current log has 311 lines but includes continuation lines. Using `grep -c ": warning:"` would give the true warning count. This is the recommended approach.

### Pitfall 5: Threshold Too Strict for Current State
**What goes wrong:** CI immediately fails on every build because warning count (currently ~300+) far exceeds threshold (20).
**Why it happens:** Phase 12 (Fix Doxygen Warning Regression) hasn't executed yet. Phase 11 only adds the CI gate.
**How to avoid:** Two options: (1) Set a temporary higher threshold that decreases as Phase 12 progresses, or (2) make the step a warning (non-blocking) until Phase 12 is complete. Option 1 is simpler and self-documenting.
**Warning signs:** Every CI run fails immediately after this phase ships.

## Code Examples

### Verified: Current Doxyfile Warning Configuration
```
# From docs/Doxyfile (verified at HEAD)
WARN_IF_UNDOCUMENTED   = YES
WARN_IF_DOC_ERROR      = YES
WARN_NO_PARAMDOC       = YES
QUIET                  = NO
WARN_LOGFILE           = doxygen-warnings.log
```

### Verified: CMake Doxygen Target
```cmake
# From CMakeLists.txt (verified at HEAD)
add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
    COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-api-docs.js
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}  # <-- warns log written to repo root
    COMMENT "Generating API documentation with Doxygen and Docusaurus"
    BYPRODUCTS ${DOXYGEN_OUT}/index.xml
)
```

### Recommended: CI Warning Check Step
```yaml
# Insert after "Generate Doxygen XML" step, before "Generate API documentation"
- name: Check Doxygen warning count
  run: |
    WARNING_FILE="doxygen-warnings.log"
    THRESHOLD=20
    if [ ! -f "$WARNING_FILE" ]; then
      echo "::error::Doxygen warning log not found at $WARNING_FILE"
      exit 1
    fi
    COUNT=$(grep -c ": warning:" "$WARNING_FILE" || true)
    echo "Doxygen warnings: $COUNT (threshold: $THRESHOLD)"
    if [ "$COUNT" -gt "$THRESHOLD" ]; then
      echo "::error::Doxygen warning count ($COUNT) exceeds threshold ($THRESHOLD)"
      echo "## Doxygen Warning Summary" >> $GITHUB_STEP_SUMMARY
      echo "Warning count: **$COUNT** (threshold: $THRESHOLD)" >> $GITHUB_STEP_SUMMARY
      echo '```' >> $GITHUB_STEP_SUMMARY
      head -30 "$WARNING_FILE" >> $GITHUB_STEP_SUMMARY
      echo '```' >> $GITHUB_STEP_SUMMARY
      exit 1
    fi
    echo "::notice::Doxygen warning count ($COUNT) is within threshold ($THRESHOLD)"
```

### Important: grep -c vs wc -l
```bash
# wc -l counts ALL lines including continuation lines
wc -l < doxygen-warnings.log   # Returns 311

# grep -c counts only actual warning lines
grep -c ": warning:" doxygen-warnings.log   # Returns ~304 (actual warnings)
```
Use `grep -c ": warning:"` for accurate counting. The `|| true` prevents grep from returning exit code 1 when count is 0.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual warning review | `WARN_LOGFILE` + manual check | Phase 9 (2026-02-03) | Warnings captured but not enforced |
| No CI gate | Phase 11 adds CI gate | This phase | Prevents warning regression |

**Doxygen `WARN_AS_ERROR` options (Doxygen 1.9.5+):**
- `NO` — warnings do not cause failure (current)
- `YES` — stop at first warning
- `FAIL_ALL` — report all warnings then fail

These are NOT suitable for threshold-based enforcement. Shell-based counting is the correct approach for a threshold gate.

## Open Questions

1. **Threshold during transition period**
   - What we know: Phase 12 will reduce warnings to < 20. Currently ~304 warnings exist.
   - What's unclear: Should Phase 11 set threshold=20 (will fail until Phase 12) or use a temporary higher threshold?
   - Recommendation: Set threshold=20 per success criteria. Phase 12 depends on Phase 9, not Phase 11, so both can proceed in parallel. If the CI gate needs to be non-blocking temporarily, use `continue-on-error: true` on the step and remove it after Phase 12.

2. **Warning log in .gitignore**
   - What we know: `doxygen-warnings.log` currently exists at repo root and is tracked (or at least present).
   - What's unclear: Should it be gitignored since it's a build artifact?
   - Recommendation: Add to `.gitignore` if not already there. It's a generated file.

## Sources

### Primary (HIGH confidence)
- `docs/Doxyfile` — verified WARN_LOGFILE configuration at HEAD
- `CMakeLists.txt` — verified WORKING_DIRECTORY for docs target
- `.github/workflows/docs.yml` — verified current CI workflow structure
- `doxygen-warnings.log` — verified 311 lines at HEAD, confirmed path at repo root
- `.planning/v1.1-MILESTONE-AUDIT.md` — verified 304 warnings finding, Phase 11 gap

### Secondary (MEDIUM confidence)
- GitHub Actions `::error::` and `$GITHUB_STEP_SUMMARY` syntax — standard GitHub Actions features, well-documented

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no external tools needed, all infrastructure exists
- Architecture: HIGH — single CI step insertion into existing workflow
- Pitfalls: HIGH — verified warning log path, line counting nuance, and threshold considerations against actual codebase state

**Research date:** 2026-02-23
**Valid until:** 2026-04-23 (stable domain — CI/Doxygen configuration rarely changes)
