# Phase 26: Lua Hot Reload - Context

**Gathered:** 2026-02-26
**Status:** Ready for planning

<domain>
## Phase Boundary

F5 key in the SDL3 runner performs a full Lua state reset and reloads the script from disk. Graceful error display on syntax or runtime failure. WASM and ESP32 builds are unaffected. File-watching, auto-reload, and editor integration are out of scope.

</domain>

<decisions>
## Implementation Decisions

### Error display
- Errors print to console/stdout only — no in-window overlay or text rendering
- Format: file + line + message (e.g. `[reload error] game.lua:42: attempt to call nil value`)
- No timestamp or reload attempt number — keep it minimal
- Syntax vs runtime error distinction: Claude's discretion

### State reset scope
- Full clear: all canvas/layer contents wiped on reload — clean slate
- Layer count resets to default — destroy all layers, script re-creates what it needs
- Window size stays as-is — no resolution reset on reload
- Game-loop state fully resets — tick count back to 0, delta time accumulator reset

### Reload feedback
- Successful reload prints: `[reload] script.lua` — simple one-liner, no timing info
- No visual feedback in the window — canvas clears and script starts drawing immediately
- Always reload on F5 regardless of whether file changed — no file-modification-time checking
- No debounce — each F5 triggers a full reload (idempotent by design)

### Error recovery flow
- After failed reload: canvas is blank (cleared), not frozen on last frame
- Game loop pauses on error — stop calling update/draw, just poll for input (F5 to retry)
- Recovery via F5 is identical to normal reload — no special recovery path or messages
- Initial startup failure behaves the same as reload failure — window opens, error prints, loop pauses, F5 to retry

### Claude's Discretion
- Whether to label syntax vs runtime errors differently in console output
- Internal implementation of loop pause (SDL event polling details)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 26-lua-hot-reload*
*Context gathered: 2026-02-26*
