# Requirements: enjin2

**Defined:** 2026-02-24
**Core Value:** enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation

## v1 Requirements (v1.4)

Requirements for v1.4 milestone. Each maps to roadmap phases.

### LAYER — Multi-layer Canvas Composition

- [x] **LAYER-01**: Engine renders up to 4 independent Canvas4 layers composited in draw order
- [x] **LAYER-02**: Each drawable is assigned to exactly one layer and renders only to that layer's buffer
- [x] **LAYER-03**: Layers are composited at blit time using index 15 as passthrough transparency
- [x] **LAYER-04**: Layer count is compile-time configurable (default 4)
- [ ] **LAYER-05**: SDL3 runner composites all layers before blitting to GPU texture
- [ ] **LAYER-06**: Lua API exposes layer selection for draw calls (`setLayer(n)`) and layer clear (`clearLayer(n, color)`)

### DOC — Docusaurus Navigation

- [x] **DOC-01**: API sidebar navigation renders correctly with all module and class pages accessible
- [x] **DOC-02**: `generate-api-docs.js` escapes angle brackets in prose text so future regenerations remain MDX-safe

### SPR — Sprites Rework

- [x] **SPR-01**: Sprite class redesigned with clean API (no legacy public members, consistent naming, targets `ICanvas<Pixel4>`)
- [x] **SPR-02**: Sprite sheet loaded as uniform grid defined by cell width, cell height, rows, and cols
- [x] **SPR-03**: Frame addressed by linear index or (row, col) grid position
- [x] **SPR-04**: Frame animation with FPS playback rate and loop modes (once, loop, ping-pong)
- [x] **SPR-05**: C_Sprite component updated to use new Sprite API
- [x] **SPR-06**: Lua API exposes sprite sheet draw and frame animation control via static sprite pool

### HOT — Lua Hot Reload

- [ ] **HOT-01**: F5 key in SDL3 runner triggers Lua script reload from disk
- [ ] **HOT-02**: Reload performs full reset (Lua state destroyed and recreated, all bindings re-registered)
- [ ] **HOT-03**: Reload error (syntax/runtime) displays error message without crashing the runner

## v2 Requirements

Deferred to future release.

### Engine

- **ENG-01**: ECS drawable pipeline supports `Canvas4` (`ICanvas<Pixel4>`) in addition to `Canvas8` — pre-existing limitation, deferred
- **ENG-02**: Sprite flip (horizontal/vertical) — useful for character facing direction
- **ENG-03**: Layer visibility toggle (`setLayerVisible(n, bool)`) — deferred until concrete use case

### Documentation

- **DOCS-01**: Getting started guide — explicitly deferred from PROJECT.md
- **DOCS-02**: Usage examples in API documentation — deferred from PROJECT.md

## Out of Scope

| Feature | Reason |
|---------|--------|
| Per-pixel alpha blending | Incompatible with 4-bit indexed palette; index-15 chroma-key is sufficient |
| Dynamic layer count at runtime | Violates zero-alloc constraint |
| Sprite rotation at blit time | No FPU on ESP32; pre-rotate frames in sheet |
| Non-uniform sprite sheet frames | Requires per-frame metadata, breaks grid math simplicity |
| File-watch auto-reload | Platform-specific OS APIs; F5 manual reload is sufficient |
| Partial Lua state hot-patch | Produces dangling references; full reset is the correct semantic |
| WASM/ESP32 hot reload | Developer tool for SDL3 runner only |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| DOC-01 | Phase 23 | Complete |
| DOC-02 | Phase 23 | Complete |
| SPR-01 | Phase 24 | Complete |
| SPR-02 | Phase 24 | Complete |
| SPR-03 | Phase 24 | Complete |
| SPR-04 | Phase 24 | Complete |
| SPR-05 | Phase 24 | Complete |
| SPR-06 | Phase 24 | Complete |
| LAYER-01 | Phase 25 | Complete |
| LAYER-02 | Phase 25 | Complete |
| LAYER-03 | Phase 25 | Complete |
| LAYER-04 | Phase 25 | Complete |
| LAYER-05 | Phase 25 | Pending |
| LAYER-06 | Phase 25 | Pending |
| HOT-01 | Phase 26 | Pending |
| HOT-02 | Phase 26 | Pending |
| HOT-03 | Phase 26 | Pending |

**Coverage:**
- v1.4 requirements: 17 total
- Mapped to phases: 17
- Unmapped: 0 ✓

---
*Requirements defined: 2026-02-24*
*Last updated: 2026-02-24 — traceability confirmed after roadmap creation*
