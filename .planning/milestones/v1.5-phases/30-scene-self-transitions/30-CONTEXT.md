# Phase 30: Scene Self-Transitions - Context

**Gathered:** 2026-02-27
**Status:** Ready for planning

<domain>
## Phase Boundary

Inject a non-owning `SceneStateMachine*` into `Scene` and implement deferred self-transition support so derived scenes can call `m_ssm->switchTo(id)` from within `onUpdate()` — including switching to themselves — without re-entrancy or initialization guard bypass. Creating new scene types, serialization, or transition animations are out of scope.

</domain>

<decisions>
## Implementation Decisions

### API shape
- Call site in derived scenes: `m_ssm->switchTo(id)` — direct access to the injected pointer, no helper wrapper
- Scene ID type: integer or enum (`switchTo(SceneID)` / `switchTo(int)`) — no string lookup, no templates
- Injection mechanism: `SceneStateMachine` calls `scene->setStateMachine(this)` at activation time (matches SCENE-01 "injected at activation time")
- Member scope: `protected SceneStateMachine* m_ssm` — derived scenes can read it directly in `onUpdate()`
- Setter scope: `public setStateMachine(SceneStateMachine*)` — SSM can call it from outside the hierarchy

### Claude's Discretion
- Multi-call policy: what happens if `switchTo()` is called more than once in a single frame (last wins, first wins, or assert)
- Self-reset lifecycle depth: whether `onDeactivate()` runs before a self-transition, and whether it's a full deactivate→destroy→onCreate→activate cycle or a lighter re-init
- Re-entrancy defense: what happens when `switchTo()` is called from `onDeactivate()` (silent drop, log+drop, or debug assert)
- Exact member/method naming conventions (e.g. `m_ssm` vs `m_stateMachine`, `setStateMachine` vs `bind`)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches for the Claude's Discretion items above.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 30-scene-self-transitions*
*Context gathered: 2026-02-27*
