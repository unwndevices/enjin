# Phase 2: Core Migration - Context

**Gathered:** 2026-01-30
**Status:** Ready for planning

## Phase Boundary

Migrate core infrastructure with compatibility layer. Establish compatibility headers, map enjin1 memory patterns to enjin2, ensure lifecycle compatibility, and implement scene management system with Strangler Fig pattern for incremental replacement.

## Implementation Decisions

### Compatibility header strategy
- Use thin type aliases only — simple typedefs for identical types, no adaptation logic
- Per-component header organization — separate headers for each component (scene.hpp, component.hpp, etc.)
- Headers live in enjin2 source tree (not a top-level compat/ directory)
- **Claude's discretion:** Header lifetime (temporary vs permanent)

### Memory mapping approach
- Restructure ownership patterns to avoid shared ownership needs (no ref counting added to enjin2)
- Direct swap — replace shared_ptr with direct ownership pointers
- **Claude's discretion:** Lifecycle safety (scene-based vs explicit hierarchy)
- **Claude's discretion:** Null safety (assertions vs runtime checks)

### Lifecycle mapping scope
- Full coverage — map all enjin1 lifecycle methods to enjin2 equivalents
- Use enjin2 naming convention (awake(), start(), etc.) not enjin1 (Awake, Start)
- **Claude's discretion:** Custom lifecycle hooks (virtual overrides vs registration pattern)
- **Claude's discretion:** Missing methods (stub implementations vs explicit errors)

### Scene transition approach
- Exact enjin1 behavior — replicate all transition timing, sequencing, and effects precisely
- Direct port of SceneStateMachine from enjin1 with minimal changes
- Port all transition effects (fade, slide, etc.) from enjin1
- Preserve enjin1 behavior for scene state handling during transitions

### Claude's Discretion
Header lifetime, lifecycle safety (model choice), null safety approach, custom lifecycle hooks pattern, missing method handling strategy

## Specific Ideas

Goal stated: enjin2 should not depend on anything from enjin1 — complete independence
No separate compat/ directory needed

## Deferred Ideas

None — discussion stayed within phase scope

---

*Phase: 02-core-migration*
*Context gathered: 2026-01-30*
