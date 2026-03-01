# Phase 52: UI Component Bindings - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Add engine.ui.* Lua sub-table with four stateless immediate-mode draw functions (progressBar, statBar, panel, label) implemented as LuaCanvas fillRect/drawRect/text calls — bypassing existing C++ Label/FillUpGauge components due to std::string incompatibility — plus an internal guide for building new engine.ui.* components.

</domain>

<decisions>
## Implementation Decisions

### Cross-phase pool policy (inherited)
- UI draws are stateless — no pool, no slots, no capacity limits
- Each call draws immediately to the canvas and returns — no retained state between frames

### Claude's Discretion
- Which canvas layer UI calls draw to (active layer vs dedicated UI layer)
- Whether UI calls are screen-space only or support camera-relative positioning
- statBar visual design (bar only, or bar with current/max label)
- Color model (palette indices consistent with existing draw calls)
- resetUIState() implementation in registerAll()
- Internal guide document structure and content

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- LuaCanvas (bindings.hpp:61-256): Type-erased canvas wrapper with fillRect(), drawRect(), drawText() — UI functions call these directly
- Existing draw bindings (bindings_draw.cpp): lua_rectangle(), lua_text() etc. — pattern for canvas access from binding functions
- Label component (label.hpp:47-375): Reference for text rendering with alignment — but bypassed due to std::string
- FillUpGauge component (fill_up_gauge.hpp:32-201): Reference for bar rendering with fill/dither — but bypassed

### Established Patterns
- Stateless draw calls: existing lua_rectangle(), lua_circle() etc. are already stateless per-frame calls
- Canvas access: LuaBindings::getActiveCanvas() provides the draw target
- Color via palette index: setColor(index) before draw, or color parameter

### Integration Points
- engine.ui.* sub-table registered in bindings_engine.cpp alongside other engine.* tables
- LuaCanvas methods for actual drawing (fillRect, drawRect, text)
- bindings_ui.cpp as new split binding file (Phase 46 pattern)

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 52-ui-component-bindings*
*Context gathered: 2026-03-01*
