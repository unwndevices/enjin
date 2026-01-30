# Phase 3: Feature Support - Context

**Gathered:** 2026-01-30
**Status:** Ready for planning

## Phase Boundary

Enable feature migration with abstraction layers. This phase delivers compilation independence (enjin2 headers compile without enjin1), legacy seams for isolated testing at component and scene boundaries, and a Canvas abstraction layer enabling both enjin1 and enjin2 to target the same interface. The scope is designing abstraction layers that make gradual migration practical, not expanding feature capabilities.

## Implementation Decisions

### Abstraction interface design
- Define only what's needed for current migration work; extend later as features are migrated (minimal shim)
- Design minimal shims with forward compatibility in mind — method signatures accommodate extension even if not fully implemented now
- Use I-prefix for abstract types (ICanvas, IComponent, IScene) to clearly distinguish from concrete implementations
- Error handling from underlying implementations — Claude's discretion

### Component wiring pattern
- How components obtain references to abstraction interfaces — Claude's discretion
- Selection timing: Compile-time only (build flags), with scope of using only enjin2 in the end and deprecating enjin1
- How components discover which implementation to use at compile time — Claude's discretion
- Testing during migration: Separate builds only — build once with enjin1, build separately with enjin2; no runtime mixing

### Legacy seam behavior
- Managing both enjin1 and enjin2 paths: Compile-time only (one or the other) — seams compile to enjin1 in one build, enjin2 in another; only one present in binary
- Seam behavior in enjin1 vs enjin2 builds — Claude's discretion
- Error handling when bridging enjin1 to enjin2 abstractions — Claude's discretion
- When migration to enjin2 is complete: Remove seams completely — no legacy code in final build

### Canvas abstraction scope
- Operations to include: Full rendering API (all drawing operations including shapes, text, images, transformations, and clipping)
- State handling (colors, fonts, transformations, clipping) — Claude's discretion
- Resource handling (images, fonts) — Claude's discretion
- Performance overhead — Claude's discretion (zero overhead vs acceptable abstraction overhead)

### Claude's Discretion
- Error handling strategy in abstraction interfaces
- Dependency injection vs factory vs direct instantiation for component wiring
- How components discover which implementation to use at compile time
- Seam behavior differences between enjin1 and enjin2 builds
- Error translation when bridging enjin1 to enjin2 abstractions
- Canvas state management approach (stateful, stateless, hybrid)
- Canvas resource management approach (opaque handles, wrapped types)
- Whether canvas abstraction should be zero-optimized or accept overhead

## Specific Ideas

No specific requirements — open to standard approaches that minimize migration friction and support eventual enjin2-only builds.

## Deferred Ideas

None — discussion stayed within phase scope.

---

*Phase: 03-feature-support*
*Context gathered: 2026-01-30*
