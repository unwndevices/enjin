# Phase 24: Sprite System Rework - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Rebuild the sprite system with a clean, zero-alloc API targeting `ICanvas<Pixel4>`. Deliver: `SpriteSheet` loading by uniform grid (cell width, cell height, rows, cols), frame addressing by linear index or (row, col), frame animation with FPS rate and three loop modes (once, loop, ping-pong), an updated `C_Sprite` component, and a Lua sprite pool. The old `Sprite` API with public `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` fields and `ICanvas<uint8_t>` draw target is removed entirely. Multi-layer canvas composition is Phase 25.

</domain>

<decisions>
## Implementation Decisions

### Transparency / matte handling
- Index 15 is the transparent palette index — pixels with value 15 are skipped during sprite blit
- This is a compile-time constant baked into the draw logic, not a per-sprite or per-draw-call parameter
- Consistent with Phase 25's layer system, which also uses index 15 as the passthrough transparency index
- Pixels that are drawn are written as raw palette index values — no color remapping or tinting
- No mode concept: the only drawing behavior is blit-with-matte-skip (draw non-15, skip 15)

### Claude's Discretion
- Lua sprite pool size and handle representation
- Frame animation tick model (delta-time vs. game ticks)
- API naming conventions for new Sprite/SpriteSheet types
- Behavior at end of "once" animation mode (freeze on last frame vs. stop advancing)
- Ping-pong direction reversal implementation

</decisions>

<specifics>
## Specific Ideas

No specific requirements beyond what's in the phase goal — open to standard approaches for pool design, animation advancement, and Lua binding style.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 24-sprite-system-rework*
*Context gathered: 2026-02-24*
