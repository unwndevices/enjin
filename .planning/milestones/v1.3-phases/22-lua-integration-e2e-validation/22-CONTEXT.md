# Phase 22: Lua Integration + E2E Validation - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire the input polling API (isButtonHeld, isButtonJustPressed, isButtonJustReleased, getAxis) and palette API into the shared Lua runtime so both are callable from scripts. Author a single E2E test script (`scripts/e2e_parity.lua`) that runs identically on SDL3 and WASM, confirming visual and behavioral parity via manual inspection.

ESP32 parity sign-off is deferred — this phase covers SDL3 + WASM only.

</domain>

<decisions>
## Implementation Decisions

### E2E test script format
- Visual demo running a continuous game loop — not assertion-based
- Draws a color grid covering all 15 palette indices (0–14) to make color correctness immediately obvious
- Color-coded indicator cell (not text) changes state when button 0 is held — proves real-time input polling works without requiring text rendering
- Script location: `scripts/e2e_parity.lua`

### Visual parity acceptance bar
- SDL3 and WASM: exact RGB match required (setPalette(0, 255, 0, 0) must render #FF0000)
- ESP32: best-effort — acceptable if colors are recognizably correct given hardware display limits
- Parity confirmed via manual eyeball (no screenshot diff or pixel hashing)
- Sign-off recorded as a written statement in VERIFICATION.md

### Input API surface
- Exact Lua function names (locked): `isButtonHeld(n)`, `isButtonJustPressed(n)`, `isButtonJustReleased(n)`, `getAxis(n)`
- Button and axis arguments are raw integers — no named Lua constants in this phase
- E2E test script exercises button index 0 and axis index 0 only

### Lua binding registration
- An existing binding file from Phase 19 (palette bindings) is already in place — input bindings extend it
- Binding file structure: open question. User noted a risk that a single consolidated file becomes hard to maintain as bindings grow. Planner should evaluate: extend existing file vs. introduce a per-module pattern (e.g., `input_lua.cpp` + `palette_lua.cpp`) that scales. Lean toward the approach that doesn't require revisiting this layout every phase.

### Lua error handling and failure reporting
- Engine host (C++ SDL3 runner) is responsible for catching Lua runtime errors — test script has no defensive pcall wrapping
- On error: log to stderr AND paint a visible on-canvas error signal (use a solid fill or error color — palette index 15 was suggested)
- This is permanent engine behavior, not phase-specific scaffolding

### Claude's Discretion
- Exact on-canvas error color/signal appearance
- How the color-coded input indicator cell is sized and positioned on the canvas
- Binding file structure decision (extend vs. split) — planner should choose based on what exists and what scales

</decisions>

<specifics>
## Specific Ideas

- The color grid should make all 15 palette colors visible at once so any wrong/missing color is immediately spotted
- On-canvas error signal: filling the canvas or a region with a "something broke" color was mentioned as the desired behavior
- The input indicator should respond to button 0 being held — the exact color change is up to Claude

</specifics>

<deferred>
## Deferred Ideas

- ESP32 parity sign-off — deferred to a later phase or hardware validation pass
- Named Lua constants (BTN_A = 0 etc.) exposed to Lua — future binding enhancement
- Text rendering for on-canvas debug output — not in scope; color-coded indicator used instead

</deferred>

---

*Phase: 22-lua-integration-e2e-validation*
*Context gathered: 2026-02-24*
