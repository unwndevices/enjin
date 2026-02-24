# Phase 19: Palette Foundation - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Canvas4 pixels map to RGB colors at display time via a swappable palette. The palette is a runtime struct with up to 16 entries (indices 0–14 usable, index 15 always transparent). Supports variable-length named presets with index wrapping. Accessible from Lua and WASM. Single output canvas — multi-canvas compositing is a future phase.

</domain>

<decisions>
## Implementation Decisions

### Default palette
- PICO-8 palette minus `#94b0c2`, giving exactly 15 colors for indices 0–14
- Index 0 = `#1a1c2c` (dark navy) — serves as default background / canvas clear color
- Full index map: 0:`#1a1c2c`, 1:`#5d275d`, 2:`#b13e53`, 3:`#ef7d57`, 4:`#ffcd75`, 5:`#a7f070`, 6:`#38b764`, 7:`#257179`, 8:`#29366f`, 9:`#3b5dc9`, 10:`#41a6f6`, 11:`#73eff7`, 12:`#f4f4f4`, 13:`#566c86`, 14:`#333c57`
- Index 15 = transparent (always, regardless of palette size)

### Named presets
- Ship multiple named palettes (e.g., `default`, `gameboy`, etc.)
- `loadPalette('name')` replaces the entire active palette
- Palettes can be smaller than 16 colors — out-of-range indices wrap around (modulo palette size)
- Palette sizes are preset-defined — no runtime `setPaletteSize()` in this phase
- No `resetPalette()` shortcut — use `loadPalette('default')`

### Transparency behavior
- Index 15 = source skip ("don't write this pixel") — used for sprite/canvas compositing
- Not a color — the destination pixel is preserved when source is index 15
- Output canvas clears to index 0 at start of frame
- Optional debug mode: render index 15 pixels as bright magenta for development visibility

### Palette API (Lua)
- Explicit naming: `setPaletteColor(index, ...)`, `getPaletteColor(index)`, `loadPalette(name)`, `getPaletteSize()`
- `setPaletteColor` accepts hex string (`'#ff0000'`) or RGB components (`255, 0, 0`)
- Changes take effect immediately — already-drawn pixels update at next blit since palette is applied at display time
- Out-of-range index in `setPaletteColor` wraps silently (consistent with drawing behavior)

### Color precision
- Full 8-bit RGB per channel (no alpha component)
- No retro color space constraints — the 15-slot limit provides the retro feel
- Colors passed as-is to platform display drivers (no quantization)

### Claude's Discretion
- WASM `getPaletteRGB()` return format (flat array vs per-index — optimize for JS renderer consumption)
- `getPaletteColor` return format (likely always RGB for consistency)
- Which named presets to ship beyond `default` and `gameboy`
- Debug magenta mode toggle mechanism (flag, API call, or build-time)
- Internal storage layout of palette struct
- Exact preset color values for non-default palettes

</decisions>

<specifics>
## Specific Ideas

- Default palette is PICO-8 with `#94b0c2` removed — user specifically chose this set
- "The transparent color is used to draw over without covering what's under — especially useful with sprites and canvases"
- Color input inspired by FastLED's color conversion approach — offer the main color format conversions (hex, RGB)

</specifics>

<deferred>
## Deferred Ideas

- Multi-canvas compositing (sprite canvas onto background canvas) — future phase
- HSV color input format — could add later if needed for programmatic palette generation
- Runtime palette resizing (`setPaletteSize(n)`) — if needed, add in a future phase
- Per-entry alpha / RGBA support — not needed with index-based transparency model

</deferred>

---

*Phase: 19-palette-foundation*
*Context gathered: 2026-02-24*
