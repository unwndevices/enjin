# Phase 30: Scene Self-Transitions - Research

**Researched:** 2026-02-27
**Domain:** C++ state machine deferred execution, re-entrancy prevention, lifecycle guard bypass
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

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

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SCENE-01 | Scene holds a non-owning `SceneStateMachine*` pointer injected at activation time | Add `protected SceneStateMachine* m_ssm = nullptr` to `Scene`; add `public setStateMachine(SceneStateMachine*)` called from `SceneStateMachine::activate()` before `onActivate()` fires |
| SCENE-02 | A derived scene can request a transition to another scene from its own `onUpdate()` | Add `switchTo(uint32_t id)` to `SceneStateMachine` that records a pending ID into `pendingSceneId` + `hasPendingTransition` flag; SSM `update()` dispatches deferred transitions after `currentScene->update(dt)` returns |
| SCENE-03 | Self-transitions (scene switching to itself) reset and reinitialize correctly via deferred execution | Deferred path in SSM `update()` must call `currentScene->deactivate()` + reset `initialized = false` + call `currentScene->activate()` (which re-runs `initialize()` → `onCreate()`); requires `Scene::resetInitialized()` protected helper or direct friend access |
</phase_requirements>

---

## Summary

Phase 30 adds two things to the engine: (1) a non-owning back-pointer from `Scene` to its owning `SceneStateMachine`, and (2) a deferred transition mechanism that allows scenes to request a switch from within their own `onUpdate()` callback, including switching to themselves.

The core challenge is that `SceneStateMachine::update()` currently calls `currentScene->update(dt)` synchronously (line 196–198 of `scene_state_machine.hpp`). If a derived scene calls `m_ssm->switchTo(id)` during `onUpdate()`, the transition must not execute immediately — doing so would destroy or re-initialize the current scene while it is still on the call stack. The solution is a pending-transition slot: `switchTo()` stores the target ID and a flag; the SSM processes the deferred transition after `currentScene->update(dt)` returns each frame.

The second challenge is the self-transition reset. `Scene::initialize()` contains the guard `if (initialized) return;` (line 57 of `scene.hpp`). `Scene::activate()` checks `if (!initialized) initialize()` before calling `onActivate()` (line 74–76). For a self-transition to re-run `onCreate()`, the `initialized` flag must be cleared before re-activation. This requires a way to reset it from the SSM without making `initialized` public. The cleanest approach is a `protected void resetForSelfTransition()` method on `Scene` (or friendship) that sets `initialized = false` and calls `deactivate()`, enabling the SSM to then call `activate()` which will re-run `initialize()` → `onCreate()`.

**Primary recommendation:** Add `pendingSceneId` + `hasPendingTransition` fields to `SceneStateMachine`; process them in `update()` after scene update returns; add `void Scene::resetForSelfTransition()` protected method to clear `initialized` before re-activation; inject `m_ssm` via `setStateMachine()` called from `SceneStateMachine`'s activation path.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++17 | project standard | `constexpr`, `std::array`, member defaults | `CMAKE_CXX_STANDARD 17` in `CMakeLists.txt` |
| `<cstdint>` | libc | `uint32_t` for scene IDs | Already used throughout `scene.hpp` and `scene_state_machine.hpp` |

### Supporting

None. This phase introduces zero new dependencies. All changes are in-tree modifications to two existing headers.

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Pending-transition slot (single deferred switch) | `std::queue<uint32_t>` of pending transitions | Queue costs heap allocation; single pending slot is sufficient for this problem (embedded constraint) |
| `Scene::resetForSelfTransition()` protected method | Making `initialized` public or SSM as `friend` | `friend` tightly couples the two classes and bleeds into all translation units; a named protected method is self-documenting and does not require `friend` if SSM downcast isn't needed |
| `pendingSceneId` + bool flag | `int pendingSceneId = -1` sentinel | Sentinel requires casting `uint32_t` through signed; separate bool flag is explicit and avoids type-pun issues |
| Full deactivate→destroy→onCreate→activate cycle for self-reset | Lightweight re-init only | Full cycle (deactivate → reset → activate) is the semantically correct behavior per SCENE-03 "full reset and re-initialization"; lighter paths skip `onDeactivate` which breaks user expectations |

**No installation required** — pure C++ header modifications.

---

## Architecture Patterns

### Files to Modify

```
include/enjin2/core/
├── scene.hpp                     — add m_ssm field, setStateMachine(), resetForSelfTransition()
└── scene_state_machine.hpp       — add switchTo(), pendingSceneId, hasPendingTransition;
                                    update activate() to call setStateMachine(this);
                                    update update() to dispatch deferred transition

tests/
├── scene_transition_test.cpp     — new test executable (Wave 0 gap)
└── CMakeLists.txt                — register scene_transition_test
```

No `.cpp` files need modification. `scene.cpp` only contains template instantiations. All new logic is inline in the headers, consistent with the project pattern where `Scene` and `SceneStateMachine` are entirely header-defined.

### Pattern 1: SSM Back-Pointer Injection (SCENE-01)

**What:** Add `protected SceneStateMachine* m_ssm = nullptr` to `Scene`. Add `public void setStateMachine(SceneStateMachine* ssm)` to `Scene`. Call `scene->setStateMachine(this)` from `SceneStateMachine` at the point where a scene is activated (both in `activate()` called from `completeTransition()` and in `startTransition()`).

**When to use:** SSM calls `setStateMachine` before `scene->activate()` so that when `onActivate()` fires, `m_ssm` is already valid.

**Injection point in SceneStateMachine:** The call to `scene->activate()` appears in two places — `completeTransition()` (line 371–373) and `startTransition()` (lines 307–313). Both must call `setStateMachine(this)` before calling `activate()`. Alternatively, `activate()` in `Scene` can call `setStateMachine` if the SSM pointer is passed as a parameter — but the CONTEXT.md decision locks the injection mechanism as `setStateMachine()` called at activation time, so the SSM calls it explicitly.

**Example:**
```cpp
// In scene.hpp protected section — new field
protected:
    SceneStateMachine* m_ssm = nullptr;

// In scene.hpp public section — new setter
public:
    void setStateMachine(SceneStateMachine* ssm) { m_ssm = ssm; }
```

```cpp
// In scene_state_machine.hpp completeTransition() — before activate()
currentScene = nextScene;
if (currentScene) {
    currentScene->setStateMachine(this);   // inject before activate
}
if (currentScene && !currentScene->isInitialized()) {
    currentScene->initialize();
}
if (currentScene && !currentScene->isActive()) {
    currentScene->activate();
}
```

**Note on forward declaration:** `scene.hpp` includes `scene_state_machine.hpp` today (`#include "signal.hpp"`, not the SSM). But `scene_state_machine.hpp` includes `scene.hpp`. Adding `SceneStateMachine*` to `scene.hpp` creates a circular include. This MUST be resolved with a forward declaration in `scene.hpp`:

```cpp
// In scene.hpp — before class Scene definition
namespace enjin2 {
    class SceneStateMachine;  // forward declaration — breaks circular dependency
}
```

`scene.hpp` stores only a pointer, so the full type is not needed in the declaration. The setter implementation is inline and only assigns the pointer — no SSM methods are called from `scene.hpp`. This pattern is standard C++ and safe.

### Pattern 2: Deferred Transition (SCENE-02)

**What:** `SceneStateMachine::switchTo(uint32_t id)` records the target ID into a pending slot and sets a flag. The SSM `update()` method processes this pending transition after `currentScene->update(dt)` returns.

**Why deferred:** `currentScene->update(dt)` at SSM line 196–198 calls `Scene::update()` which calls `onUpdate()`. If `onUpdate()` calls `m_ssm->switchTo(id)`, we are still in the `currentScene->update()` call frame. Executing the transition immediately would call `currentScene->deactivate()` while `currentScene->update()` is on the call stack — undefined behavior via invalidated `this` pointer if the transition destroys or reinitializes scene state.

**Multi-call policy (Claude's Discretion):** Last-wins is the correct policy. If a scene calls `switchTo(A)` and then `switchTo(B)` in the same `onUpdate()`, `B` wins. This is predictable and does not require a log or assert for normal use. An in-range ID check (scene exists) should trigger a debug assert on unknown IDs. This avoids silent no-ops from typos while not crashing on valid last-wins scenarios.

**Example — SceneStateMachine fields:**
```cpp
// In scene_state_machine.hpp private section — new fields
uint32_t pendingSceneId = 0;
bool hasPendingTransition = false;
```

**Example — switchTo() method:**
```cpp
// In scene_state_machine.hpp public section
void switchTo(uint32_t sceneId) {
    // Validate: scene must exist
    // (optional: debug assert if not found)
    for (size_t i = 0; i < sceneCount; ++i) {
        if (scenes[i] && scenes[i]->getId() == sceneId) {
            pendingSceneId = sceneId;
            hasPendingTransition = true;
            return;
        }
    }
    // Scene ID not found — silently ignore (or debug assert)
}
```

**Example — update() with deferred dispatch:**
```cpp
// In scene_state_machine.hpp update() — modified
void update(float dt) {
    // 1. Update transition animations (unchanged)
    if (transitionState != TransitionState::IDLE) {
        updateTransition(dt);
    }

    // 2. Update current scene (unchanged)
    if (currentScene) {
        currentScene->update(dt);
    }

    // 3. Dispatch deferred transition — AFTER scene update returns
    if (hasPendingTransition) {
        hasPendingTransition = false;
        uint32_t targetId = pendingSceneId;
        pendingSceneId = 0;
        applyDeferredTransition(targetId);
    }
}
```

### Pattern 3: Self-Transition Reset (SCENE-03)

**What:** When `switchTo(id)` targets the currently active scene, the deferred dispatch must perform a full lifecycle reset: deactivate → clear initialized flag → activate (which triggers initialize() → onCreate()).

**The initialization guard problem:** `Scene::initialize()` has `if (initialized) return;` at line 57. `Scene::activate()` calls `initialize()` at line 74–76 only if `!initialized`. For a self-transition to re-run `onCreate()`, `initialized` must be `false` before calling `activate()`.

**Solution — `resetForSelfTransition()`:** Add a method to `Scene` that clears `initialized` to `false` after deactivating. The SSM calls this sequence:

```cpp
// Full self-transition lifecycle:
// 1. Deactivate the scene (runs onDeactivate, sets active = false)
scene->deactivate();
// 2. Clear initialized flag so next activate() runs initialize() again
scene->resetInitialized();     // sets initialized = false
// 3. Activate: runs initialize() → onCreate(), then onActivate()
scene->activate();
```

**Where to add `resetInitialized()`:** As a public method on `Scene` (SSM calls it from outside the class hierarchy). Named `resetInitialized()` rather than `resetForSelfTransition()` to be more general and match the existing naming style (`isInitialized()` at line 188). The method does one thing: `initialized = false`.

**Example — Scene addition:**
```cpp
// In scene.hpp public section
void resetInitialized() { initialized = false; }
```

**Example — applyDeferredTransition() in SceneStateMachine:**
```cpp
// In scene_state_machine.hpp private section
void applyDeferredTransition(uint32_t targetId) {
    Scene* targetScene = nullptr;
    for (size_t i = 0; i < sceneCount; ++i) {
        if (scenes[i] && scenes[i]->getId() == targetId) {
            targetScene = scenes[i].get();
            break;
        }
    }
    if (!targetScene) return;

    bool isSelfTransition = (targetScene == currentScene);

    if (isSelfTransition) {
        // Full reset: deactivate, clear initialized, re-activate
        currentScene->deactivate();          // calls onDeactivate()
        currentScene->resetInitialized();    // clears initialized flag
        currentScene->setStateMachine(this); // re-inject (already set, but explicit)
        currentScene->activate();            // calls initialize() → onCreate(), then onActivate()
    } else {
        // Normal cross-scene transition (immediate)
        Scene* oldScene = currentScene;
        if (currentScene) {
            currentScene->deactivate();
        }
        currentScene = targetScene;
        currentScene->setStateMachine(this);
        if (!currentScene->isInitialized()) {
            currentScene->initialize();
        }
        if (!currentScene->isActive()) {
            currentScene->activate();
        }
        onSceneChangeCompleteSignal.emit(oldScene, currentScene);
    }
}
```

### Pattern 4: Re-entrancy Defense (SCENE-04 — Claude's Discretion)

**What:** If `switchTo()` is called from within `onDeactivate()`, the deferred transition has already been consumed (`hasPendingTransition` was cleared before calling `applyDeferredTransition()`), so a new call to `switchTo()` during deactivation would set `hasPendingTransition = true` again. After `applyDeferredTransition()` returns, the next frame's `update()` would process this new pending transition.

**Analysis:** This is actually safe under the deferred model. The call to `switchTo()` from `onDeactivate()` queues a new deferred transition, which will be processed on the NEXT frame. No re-entrancy occurs because we are inside `applyDeferredTransition()` (called after `currentScene->update()` returns), not inside `currentScene->update()` itself. The `hasPendingTransition` flag being re-set in `onDeactivate()` is harmless.

**Recommendation:** Silent drop of `switchTo()` calls from within `onDeactivate()` is not needed — the deferred model handles it naturally. However, for diagnostic clarity, a comment in the code noting this behavior is valuable. No assert or log is required for the base case.

**Warning:** The one edge case that IS dangerous is if someone calls `switchTo()` from a signal handler connected to `onDeactivateSignal`. The signal fires at the end of `deactivate()` (line 96 of `scene.hpp`). If the signal handler calls `m_ssm->switchTo(id)`, the same safe-queueing behavior applies since we're still in `applyDeferredTransition()` context. This is safe.

### Pattern 5: switchTo() vs changeScene() Naming

**What:** The existing API is `changeScene(uint32_t sceneId, TransitionType, float duration)`. The new `switchTo(uint32_t id)` is the deferred variant with implicit IMMEDIATE transition type.

**Decision:** Add `switchTo()` as a new method alongside `changeScene()`. Do NOT rename or remove `changeScene()` — it is the external API (used by the SDL main loop and other non-scene callers). `switchTo()` is the in-scene call, designed to be called from `m_ssm->switchTo(id)`. The two methods are complementary: `changeScene()` is for external callers who control the transition type and timing; `switchTo()` is for scenes that want to trigger a transition during their own update.

### Pattern 6: Test Structure

**What:** Follow the established hand-rolled test pattern from `input_test.cpp` and `sprite_test.cpp`. No external test framework. ASSERT macro + pass/fail counters + exit code 1 on failure.

**Minimal concrete `Scene` subclass for testing:**
```cpp
// tests/scene_transition_test.cpp
#include <enjin2/core/scene_state_machine.hpp>
#include <cstdio>
using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

struct TestScene : Scene {
    explicit TestScene(uint32_t id) : Scene(id) {}
    int createCount = 0;
    int activateCount = 0;
    int deactivateCount = 0;
    bool requestSelfSwitchOnNextUpdate = false;

    void onCreate()     override { createCount++; }
    void onActivate()   override { activateCount++; }
    void onDeactivate() override { deactivateCount++; }
    void onUpdate(float dt) override {
        if (requestSelfSwitchOnNextUpdate) {
            requestSelfSwitchOnNextUpdate = false;
            m_ssm->switchTo(sceneId);
        }
    }
};
```

### Anti-Patterns to Avoid

- **Executing transitions inside `update()`:** Calling `completeTransition()` or `applyDeferredTransition()` from inside `currentScene->update(dt)` — this invalidates the `currentScene` pointer while the scene is on the call stack.
- **Forgetting to clear `hasPendingTransition` before calling `applyDeferredTransition()`:** If the flag is still true when `applyDeferredTransition()` calls `deactivate()` and that triggers another `switchTo()`, the outer call would re-enter `applyDeferredTransition()`. Always clear the flag first, then act on the captured `targetId`.
- **Skipping `onDeactivate()` for self-transitions:** The success criteria explicitly requires `deactivate()` to run before self-transition reset. Skipping it violates user expectations and SCENE-03.
- **Not clearing `initialized` before re-activation:** Calling `activate()` without first calling `resetInitialized()` means `initialize()` sees `initialized == true` and returns early — `onCreate()` never fires. The self-transition becomes a no-op.
- **Circular include between scene.hpp and scene_state_machine.hpp:** `scene_state_machine.hpp` already includes `scene.hpp`. Adding `#include "scene_state_machine.hpp"` to `scene.hpp` would create a circular dependency. Use a forward declaration of `SceneStateMachine` in `scene.hpp` instead.
- **Calling `setStateMachine()` after `activate()` instead of before:** If `setStateMachine()` is called after `activate()`, then `onActivate()` fires with `m_ssm == nullptr`. The pointer must be injected before `activate()` is called.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Deferred work queue | `std::queue` or ring buffer of pending transitions | Single `pendingSceneId` + `bool hasPendingTransition` flag | Only one deferred transition makes sense per frame (last-wins); a queue adds heap allocation and complexity with no benefit at this scale |
| Re-entrancy tracking | Separate re-entrancy depth counter | Deferred execution model eliminates re-entrancy by design | If transitions only execute after `currentScene->update()` returns, re-entrancy is structurally impossible for the scene-update path |
| Thread safety | Mutex or atomic flag | Not applicable | enjin2 is single-threaded; no synchronization primitives needed |

**Key insight:** The deferred-execution model eliminates the re-entrancy problem by construction — transitions only fire after the current scene's update stack has fully unwound.

---

## Common Pitfalls

### Pitfall 1: Circular Include Between scene.hpp and scene_state_machine.hpp

**What goes wrong:** Adding `SceneStateMachine* m_ssm` to `scene.hpp` and also having `scene_state_machine.hpp` include `scene.hpp` creates a circular `#include` that fails to compile.

**Why it happens:** `scene_state_machine.hpp` line 3 already includes `"scene.hpp"`. If `scene.hpp` then includes `"scene_state_machine.hpp"`, both files include each other — `#pragma once` prevents infinite recursion but leaves each file seeing only an incomplete type from the other.

**How to avoid:** Add a forward declaration in `scene.hpp` before the `Scene` class definition:
```cpp
namespace enjin2 { class SceneStateMachine; }
```
`Scene` only stores a pointer to `SceneStateMachine` — the incomplete type is sufficient for the pointer declaration and inline setter. No `SceneStateMachine` methods are called from `scene.hpp`.

**Warning signs:** Compiler error "incomplete type" or "use of undeclared identifier" on the `SceneStateMachine*` field in `Scene`.

### Pitfall 2: Transition Executes While Scene Is On the Call Stack

**What goes wrong:** `switchTo()` is called from `onUpdate()` and immediately calls `completeTransition()` or `applyDeferredTransition()`. This deactivates (and possibly reinitializes) `currentScene` while `Scene::update()` → `onUpdate()` is still executing. After `onUpdate()` returns, `Scene::update()` tries to call `objects.update(dt)` on a scene that has been replaced or reset — reads from invalid state.

**Why it happens:** `SceneStateMachine::update()` calls `currentScene->update(dt)` synchronously. `onUpdate()` is called from inside `Scene::update()`. Any immediate mutation of `currentScene` invalidates the execution context.

**How to avoid:** `switchTo()` only writes to `pendingSceneId` and `hasPendingTransition`. The actual transition is applied in `SceneStateMachine::update()` after `currentScene->update(dt)` returns (step 3 in the updated `update()` flow).

**Warning signs:** Crashes or corrupted scene state on the frame a self-transition fires; `objects.update()` being called on a freshly-constructed scene.

### Pitfall 3: Self-Transition onCreate() Guard Not Reset

**What goes wrong:** A scene calls `m_ssm->switchTo(sceneId)` (its own ID). The SSM calls `currentScene->deactivate()` but does not call `resetInitialized()`. It then calls `currentScene->activate()`. Inside `activate()`, `initialize()` is called — but `initialized == true`, so `if (initialized) return;` fires and `onCreate()` is skipped. The scene appears to "transition to itself" but its state is never reset.

**Why it happens:** The `initialized` guard in `Scene::initialize()` exists to prevent double-initialization on normal activation. For self-transitions it must be explicitly cleared before re-activation.

**How to avoid:** `applyDeferredTransition()` calls `currentScene->resetInitialized()` between `deactivate()` and `activate()` for self-transitions specifically.

**Warning signs:** `onCreate()` counter does not increment after a self-transition; scene state is not reset after self-switch.

### Pitfall 4: m_ssm Is nullptr During onActivate() / onUpdate()

**What goes wrong:** `setStateMachine()` is called after `activate()` instead of before. `onActivate()` fires with `m_ssm == nullptr`. If the scene tries to call `m_ssm->switchTo(id)` from `onActivate()`, it crashes.

**Why it happens:** In `completeTransition()` (current code), the sequence is:
1. `currentScene->initialize()` if needed
2. `currentScene->activate()`

If `setStateMachine(this)` is inserted after step 2, `m_ssm` is null during `onActivate()`.

**How to avoid:** Insert `currentScene->setStateMachine(this)` BEFORE step 1 (before `initialize()` and `activate()`). This way `m_ssm` is valid for both `onCreate()` and `onActivate()`.

**Warning signs:** Null pointer dereference crash in `onActivate()` when calling `m_ssm->switchTo()`.

### Pitfall 5: hasPendingTransition Not Cleared Before applyDeferredTransition()

**What goes wrong:** `update()` calls `applyDeferredTransition()` while `hasPendingTransition` is still `true`. Inside `applyDeferredTransition()`, the call to `currentScene->deactivate()` triggers `onDeactivate()`, which calls `m_ssm->switchTo(id)` again. This sets `hasPendingTransition = true` and `pendingSceneId`. After `applyDeferredTransition()` returns, `update()` checks `hasPendingTransition` again — it is true, so `applyDeferredTransition()` is called a second time in the same frame. This is the only re-entrancy scenario that can occur under the deferred model.

**How to avoid:** In `update()`, clear both `hasPendingTransition = false` and capture `targetId = pendingSceneId` BEFORE calling `applyDeferredTransition()`. Any `switchTo()` call inside `applyDeferredTransition()` will set `hasPendingTransition = true` again for the NEXT frame, not the current one.

**Warning signs:** Multiple transitions firing in one frame; `applyDeferredTransition()` called twice per update.

### Pitfall 6: Scene ID Type Mismatch

**What goes wrong:** Existing code uses `uint32_t` for scene IDs throughout. Adding `switchTo(int id)` or `switchTo(SceneID)` (a new enum) requires type conversions at call sites. The CONTEXT.md says "integer or enum" — `uint32_t` is already the established type.

**How to avoid:** Use `uint32_t` for `switchTo(uint32_t id)` consistent with `changeScene(uint32_t sceneId, ...)` and `Scene::sceneId`. This means derived scenes call `m_ssm->switchTo(1u)` or `m_ssm->switchTo(static_cast<uint32_t>(MyScenes::Menu))`.

**Warning signs:** Implicit conversion warnings (`-Wsign-conversion`) at call sites.

---

## Code Examples

Verified patterns from direct codebase inspection:

### Existing initialization guard in Scene::initialize() (scene.hpp:56-64)

```cpp
// Source: include/enjin2/core/scene.hpp lines 56-64
void initialize() {
    if (initialized) return;    // <-- this guard must be bypassable for self-transitions

    onCreate();
    objects.initialize();
    initialized = true;

    onCreateSignal.emit(this);
}
```

### Existing activate() sequence (scene.hpp:71-83)

```cpp
// Source: include/enjin2/core/scene.hpp lines 71-83
void activate() {
    if (active) return;

    if (!initialized) {
        initialize();           // only runs onCreate() if !initialized
    }

    onActivate();
    objects.start();
    active = true;

    onActivateSignal.emit(this);
}
```

Self-transition requires: `deactivate()` → `resetInitialized()` (new) → `activate()` (which now runs `initialize()` → `onCreate()`).

### Existing completeTransition() in SceneStateMachine (scene_state_machine.hpp:359-384)

```cpp
// Source: include/enjin2/core/scene_state_machine.hpp lines 359-384
void completeTransition() {
    Scene* oldScene = currentScene;

    if (currentScene && currentScene != nextScene) {
        currentScene->deactivate();          // skipped for self-transition in old code!
    }

    currentScene = nextScene;
    if (currentScene && !currentScene->isInitialized()) {
        currentScene->initialize();          // guard prevents re-run!
    }
    if (currentScene && !currentScene->isActive()) {
        currentScene->activate();
    }
    // ...
}
```

This code shows why existing `changeScene(selfId)` was a no-op: `currentScene == nextScene` skips `deactivate()`, and `isInitialized() == true` skips `initialize()`. The new `applyDeferredTransition()` must handle self-transitions explicitly.

### Existing update() loop (scene_state_machine.hpp:189-199)

```cpp
// Source: include/enjin2/core/scene_state_machine.hpp lines 189-199
void update(float dt) {
    if (transitionState != TransitionState::IDLE) {
        updateTransition(dt);
    }
    if (currentScene) {
        currentScene->update(dt);   // deferred transition must fire AFTER this returns
    }
    // NEW: dispatch deferred transition here (step 3)
}
```

### Full modified Scene additions

```cpp
// Source: new additions to include/enjin2/core/scene.hpp

// Forward declaration (before class Scene, after namespace enjin2 opens):
class SceneStateMachine;

// In Scene protected section (after existing protected members):
SceneStateMachine* m_ssm = nullptr;

// In Scene public section (after existing public methods):
void setStateMachine(SceneStateMachine* ssm) { m_ssm = ssm; }
void resetInitialized() { initialized = false; }
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `changeScene(selfId)` is a no-op (skips deactivate + skips initialize due to guard) | `switchTo(selfId)` deferred → full deactivate → resetInitialized → activate cycle | Phase 30 | Self-transitions correctly re-run `onCreate()` |
| No SSM back-pointer in Scene | `protected SceneStateMachine* m_ssm` injected at activation | Phase 30 | Derived scenes can call `m_ssm->switchTo()` from `onUpdate()` |
| Transitions execute immediately in SSM (synchronous) | `switchTo()` defers to after `currentScene->update()` returns | Phase 30 | No re-entrancy risk from scene-initiated transitions |

**Not deprecated:** `changeScene()` remains — it is the external API for non-scene callers. `switchTo()` is a new companion method for in-scene use.

---

## Open Questions

1. **Should `switchTo()` support non-IMMEDIATE transition types?**
   - What we know: The CONTEXT.md says "no templates" and the phase goal is deferred IMMEDIATE switches. The success criteria does not mention fade/slide transitions for deferred switches.
   - What's unclear: Whether `switchTo(id, TransitionType::FADE_OUT_IN)` is ever needed from inside a scene.
   - Recommendation: Phase 30 scope is IMMEDIATE only. Defer animated deferred transitions to a future phase. `switchTo(uint32_t id)` takes only an ID.

2. **Should `switchTo()` accept IDs that don't exist in the SSM?**
   - What we know: `changeScene()` returns `false` on unknown ID. `switchTo()` is `void` (no return value needed for deferred use).
   - What's unclear: Whether a debug assert, a log, or a silent drop is best for unknown IDs.
   - Recommendation: In debug builds, assert when the ID is not found. In release builds, silently ignore. This matches the embedded platform constraint (no abort on ESP32 in release).

3. **Does `setStateMachine()` need to be called on deactivation (set back to nullptr)?**
   - What we know: The pointer is non-owning. If a scene is deactivated and then re-activated by a different SSM (unlikely in this architecture), the stale pointer would be wrong.
   - What's unclear: Whether scenes ever migrate between SSMs.
   - Recommendation: Do NOT clear `m_ssm` on deactivation. The architecture has one SSM per application. Setting it to `nullptr` on deactivate adds code with no practical benefit. The CONTEXT.md says the SSM sets it "at activation time" — the deactivation path is out of scope.

---

## Sources

### Primary (HIGH confidence)

- Live codebase inspection (2026-02-27):
  - `include/enjin2/core/scene.hpp` — full class definition; `initialize()` guard at line 57; `activate()` sequence at lines 71–83; `deactivate()` at lines 90–97; `update()` at lines 103–109 confirmed
  - `include/enjin2/core/scene_state_machine.hpp` — `changeScene()` implementation at lines 142–183; `completeTransition()` at lines 359–384; `update()` at lines 189–199; existing `currentScene != nextScene` guard and `isInitialized()` check confirmed
  - `include/enjin2/core/component.hpp` — `protected Object* owner` pattern confirms project uses protected non-owning raw pointer back-references (directly analogous to `protected SceneStateMachine* m_ssm`)
  - `src/core/scene.cpp` — only contains template instantiations; no constructor or method bodies; confirms all Scene logic is header-inline
  - `tests/input_test.cpp` — ASSERT macro + pass/fail counter test structure confirmed
  - `tests/CMakeLists.txt` — `add_executable` + `add_test` registration pattern confirmed
  - `CMakeLists.txt` — `CMAKE_CXX_STANDARD 17` confirmed; `ENJIN2_BUILD_TESTS` option and `add_subdirectory(tests)` confirmed
  - Phase 29 RESEARCH.md — confirms naming conventions (bare member names in `object.hpp`/`scene.hpp` context, no leading underscore for scene-level fields)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; C++17 confirmed from project files; all changes are in-tree header modifications
- Architecture: HIGH — all patterns derived from direct codebase inspection; the initialization guard, activation sequence, and update loop are read directly from the source files
- Pitfalls: HIGH — all pitfalls traced to specific existing code behavior (circular include risk, initialization guard, call-stack re-entrancy); no speculation

**Research date:** 2026-02-27
**Valid until:** 90 days (stable C++ codebase, no fast-moving dependencies)
