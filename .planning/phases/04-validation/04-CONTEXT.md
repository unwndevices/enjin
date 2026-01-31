# Phase 4: Validation - Context

**Gathered:** 2026-01-31
**Status:** Ready for planning

<domain>
## Phase Boundary

Validate behavior through manual testing and shadow mode execution. Manual testing covers component lifecycle, rendering, scene transitions, and Lua scripting. Shadow mode runs enjin1 and enjin2 in parallel with output comparison for behavioral verification.

</domain>

<decisions>
## Implementation Decisions

### Test reporting format
- Terminal/console output (not HTML reports)
- Summary output with generated bmp image of the output buffer
- Minimal failure presentation — show failing test name and error code, reference bmp image for details
- Chronological organization — display tests in execution order

### Manual testing scope
- Critical paths only — happy path for each area (lifecycle, rendering, transitions, Lua)
- High-level guidance — broad objectives, tester decides specific approach
- Prioritize by risk — more test coverage on high-risk areas, less on stable ones
- Document as Markdown checklist

### Shadow mode comparison
- Compare output buffer only (final rendered pixels), not internal state
- Up to 3% pixel difference tolerance allowed
- Timing differences not problematic unless significant — warn on gaps
- Continue execution and summarize all differences at end (not fail-fast)

### Test execution workflow
- Single command per test type (e.g., `run-tests manual`, `run-tests shadow`)
- Hardcoded defaults — no config file or command-line flags needed
- Save all artifacts to disk — bmp images, logs, and reports in timestamped directory
- Clean state each run (default), with option to reuse state for faster repeated runs

### Claude's Discretion
- Exact threshold for "significant timing gaps" to warn on
- Test result formatting details (spacing, color coding, etc.)
- Artifact directory naming convention
- Environment cleanup strategy for clean state

</decisions>

<specifics>
## Specific Ideas

- "summary and generated bmp image of the output buffer" — user explicitly wants bmp images as part of summary output
- "i say up to 3% pixel difference. timing differences arent problematic unless big" — direct user specification on tolerances
- Minimal failure presentation — just the essentials, let the bmp image show the details
- Test execution should be simple: one command per test type, no configuration needed
- Save everything — complete artifact history for debugging

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-validation*
*Context gathered: 2026-01-31*
