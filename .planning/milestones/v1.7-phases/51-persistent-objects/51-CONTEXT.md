# Phase 51: Persistent Objects - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Implement engine.scene.persist/unpersist Lua API backed by a PersistentObjectRegistry owned by SceneStateMachine — objects flagged as persistent survive scene transitions via a fixed-size ownership array, and engine.scene.find() searches the persistent registry in addition to the active scene.

</domain>

<decisions>
## Implementation Decisions

### Pool exhaustion policy
- 4 fixed persistent object slots
- When all slots are full, engine.scene.persist() returns nil/false
- Scripts that care can check the return value
- No Lua error raised — consistent with coroutine pool, tween pool, and all other fixed pools
- No introspection API — keep API minimal

### Scene transition behavior
- Persistent objects survive scene transitions by design (that's the feature)
- Coroutines and tweens are still cancelled on scene transition — persistent objects lose their async tasks but retain their component state
- unpersist() marks for removal on next transition, not immediate destruction

### Claude's Discretion
- PersistentObjectRegistry internal structure (SSM-owned fixed array)
- How persistent objects are re-parented to new scene's ObjectCollection
- Render ordering of persistent objects relative to scene objects
- find() search priority (persistent registry first or active scene first)
- Overflow test design

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- SceneStateMachine (include/enjin2/core/scene_state_machine.hpp): Owns scene lifecycle, 32-slot array, deferred transitions — registry attaches here
- ObjectCollection (include/enjin2/core/object_collection.hpp): 128-object fixed array with findByName, findAllWithTag — persistent find extends this
- Scene activate/deactivate cycle (SSM lines 346-350): Where persistent objects get injected into new scene

### Established Patterns
- Fixed-capacity arrays (128 objects, 32 scenes, 16 stores)
- Deferred transitions: pendingSceneId + hasPendingTransition flag — registry check goes in applyDeferredTransition
- Object lifecycle: initialize() -> start() -> update(dt) -> lateUpdate(dt)

### Integration Points
- SceneStateMachine::applyDeferredTransition(): Extract persistent objects before destroying old scene, inject into new scene
- ObjectCollection: May need m_external[] non-owning pointer injection for persistent objects
- engine.scene.find() binding: Extend to search persistent registry
- engine.scene.persist/unpersist: New entries in engine.scene.* sub-table

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 51-persistent-objects*
*Context gathered: 2026-03-01*
