# Requirements: enjin2

**Defined:** 2026-02-23
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1.3 Requirements

Requirements for Tomodachi Readiness milestone. Each maps to roadmap phases.

### Palette

- [x] **PAL-01**: Canvas4 palette maps 16 indices to RGB colors at display time (not draw time)
- [x] **PAL-02**: Index 15 is transparent, indices 0-14 are user colors
- [x] **PAL-03**: Runtime palette swap via setPaletteColor(index, r, g, b) without canvas re-render
- [x] **PAL-04**: Lua API exposes setPalette() and getPalette() for scripts
- [x] **PAL-05**: WASM bindings expose getPaletteRGB() for JavaScript renderer

### SDL Runner

- [ ] **SDL-01**: CMake ENJIN2_BUILD_SDL=ON/OFF option with no impact on WASM or ESP32 builds
- [ ] **SDL-02**: SDL3 window with Canvas4-to-RGB texture blit via palette lookup
- [ ] **SDL-03**: Integer pixel scaling with nearest-neighbor filtering
- [ ] **SDL-04**: Game loop with event polling, delta time, and clean shutdown
- [ ] **SDL-05**: Lua scripting works in SDL3 runner (same scripts as WASM/ESP32)

### Input

- [ ] **INP-01**: Platform-agnostic input interface with zero platform types in headers
- [ ] **INP-02**: InputState with button bitmask and float analog axes
- [ ] **INP-03**: Edge detection (justPressed, held, justReleased) in shared layer
- [ ] **INP-04**: SDL3 keyboard-to-button default mapping (arrows, Z/X, Enter)
- [ ] **INP-05**: Lua input polling API (isButtonHeld, isButtonJustPressed, getAxis)

## Future Requirements

### Palette Extensions

- **PAL-06**: Two-stage draw palette (index remapping at draw time, PICO-8 model)
- **PAL-07**: Per-entry transparency flag in draw palette
- **PAL-08**: Canvas8 palette support (256-color indexed)

### Input Extensions

- **INP-06**: Lua input event callbacks (onButtonPressed / onButtonReleased)
- **INP-07**: Input device hot-plug (connect/disconnect gamepad at runtime)
- **INP-08**: Lua keyboard mapping table (input.setKeyMap)

### SDL Runner Extensions

- **SDL-06**: Lua hot-reload in SDL3 runner (F5 key or file mtime watch)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Multi-layer composition | Deferred to v1.4+ per project scope |
| MIDI/audio integration | Tomodachi-side, not enjin2 |
| SDL2 (legacy) | SDL3 is stable since Jan 2025; SDL2 receives no new features |
| Input libraries (Gainput, MPG) | Custom IInputProvider is simpler and fits zero-alloc constraint |
| Full alpha blending per pixel | Chroma-key transparency (index 15) sufficient for 4-bit canvas |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| PAL-01 | Phase 19 | Complete |
| PAL-02 | Phase 19 | Complete |
| PAL-03 | Phase 19 | Complete |
| PAL-04 | Phase 19 | Complete |
| PAL-05 | Phase 19 | Complete |
| SDL-01 | Phase 21 | Pending |
| SDL-02 | Phase 21 | Pending |
| SDL-03 | Phase 21 | Pending |
| SDL-04 | Phase 21 | Pending |
| SDL-05 | Phase 22 | Pending |
| INP-01 | Phase 20 | Pending |
| INP-02 | Phase 20 | Pending |
| INP-03 | Phase 20 | Pending |
| INP-04 | Phase 21 | Pending |
| INP-05 | Phase 22 | Pending |

**Coverage:**
- v1.3 requirements: 15 total
- Mapped to phases: 15
- Unmapped: 0

---
*Requirements defined: 2026-02-23*
*Last updated: 2026-02-23 after roadmap creation*
