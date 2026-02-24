# Phase 21: SDL3 CMake + Runner - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

An SDL3 desktop runner builds as an opt-in CMake executable target (`ENJIN2_BUILD_SDL`), displaying a Canvas4 scene via palette lookup with integer scaling, running a fixed-rate game loop, and mapping SDL3 keyboard input to the Phase 20 InputState abstraction. Canvas rendering, palette system, and input abstraction are already complete from prior phases.

</domain>

<decisions>
## Implementation Decisions

### SDL3 dependency acquisition
- Use CMake FetchContent to auto-download SDL3 — zero contributor setup required
- Pin to the latest stable SDL3 release tag at implementation time (reproducible builds)
- If FetchContent fails when `ENJIN2_BUILD_SDL=ON`, issue a hard CMake error and stop configure — no silent fallbacks
- SDL3 is a build-only dependency; it must NOT appear in install/package targets

### Window & integer scaling
- Default scale factor: **4x** (e.g. a 128×128 canvas renders as a 512×512 window)
- Window is **fixed size, not resizable** — pixel-perfect integer scaling guaranteed always
- Window title: **"Enjin2"**
- No fullscreen toggle — windowed only for this phase

### Keyboard mapping
- **WASD mirrors arrow keys** — both sets write the same directional button bits in InputState
- **Escape = quit only** — closes the runner cleanly, not mapped to any InputState button
- Mapping is **hardcoded**, not configurable
- Button indices follow **whatever Phase 20's InputState defines** — SDL mapping aligns to existing indices (Up/Down/Left/Right/A/B/Start as defined there)
- Z = A button, X = B button, Enter = Start (per phase spec)

### Game loop & timing
- Target frame rate is **configurable via `--fps N` flag**, default **30fps** (matches embedded system capability)
- Delta-time is clamped to a **4-frame ceiling** (at 30fps ≈ 133ms max dt) — prevents spiral-of-death on stall
- Loop shuts down cleanly on window close or Escape
- Before any script is loaded, display a **blank canvas with the default palette** (no test pattern, no fake content)

### Claude's Discretion
- Exact timing source (SDL_GetTicks64 vs high-resolution clock)
- Sleep/yield strategy for frame pacing (SDL_Delay vs busy-wait)
- Canvas-to-SDL texture format and upload path

</decisions>

<specifics>
## Specific Ideas

- 30fps default is intentional — matches the embedded (ESP32) platform's stable frame rate so cross-platform behavior is comparable
- The runner is a dev/debug tool at this stage; no fancy UI or overlays needed

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 21-sdl3-cmake-runner*
*Context gathered: 2026-02-24*
