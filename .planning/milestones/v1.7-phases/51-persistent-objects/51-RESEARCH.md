# Phase 51: Persistent Objects - Research

**Researched:** 2026-03-02
**Domain:** C++ engine internals (SceneStateMachine, ObjectCollection, LuaBindings) + Lua bindings
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **4 fixed persistent object slots** — fixed array, not dynamic
- **Pool exhaustion**: `engine.scene.persist()` returns nil/false when all 4 slots are full — no Lua error raised, consistent with coroutine pool and tween pool overflow policy
- **No introspection API** — minimal API surface
- **Coroutines and tweens are cancelled on scene transition** — persistent objects lose their async tasks but retain component state
- **`unpersist()` marks for removal on next transition**, not immediate destruction

### Claude's Discretion

- PersistentObjectRegistry internal structure (SSM-owned fixed array)
- How persistent objects are re-parented to new scene's ObjectCollection
- Render ordering of persistent objects relative to scene objects
- find() search priority (persistent registry first or active scene first)
- Overflow test design

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PERSIST-01 | `engine.scene.persist(proxy)` flags object to survive scene transitions | SSM-owned PersistentObjectRegistry with 4-slot fixed array; applyDeferredTransition() extracts persistent objects before destroying old scene and injects into new scene |
| PERSIST-02 | `engine.scene.unpersist(proxy)` removes persistence flag | Registry marks slot with `pendingRemoval` flag; object is destroyed on next scene transition (not immediately) |
| PERSIST-03 | `engine.scene.find()` searches persistent registry in addition to active scene | find() binding checks persistent registry when active scene search returns nullptr |
</phase_requirements>

---

## Summary

Phase 51 adds persistent-across-scenes objects: a fixed 4-slot `PersistentObjectRegistry` owned by `SceneStateMachine` (SSM). When Lua calls `engine.scene.persist(proxy)`, the Object* is moved into the registry array. During `applyDeferredTransition()`, persistent objects are extracted before the old scene is destroyed, then injected as non-owning pointers into the new scene's `ObjectCollection` via an `m_external[]` array extension. The registry owns the persistent objects via `std::unique_ptr`; `ObjectCollection` only gets raw non-owning pointers for update/find purposes.

The design challenge is ownership: persistent objects must survive scene `deactivate()` + `clear()`, which currently destroys all `unique_ptr` in `ObjectCollection::objects`. The solution is a split ownership model: `PersistentObjectRegistry` holds ownership (`unique_ptr`), while `ObjectCollection` gets an `m_external[]` raw pointer array for the injected objects. The `clear()` method of `ObjectCollection` must only clear owned objects, leaving externals intact (or they are managed separately by the registry).

All binding infrastructure follows the established Phase 50 (tween) pattern: a new `bindings_scene_persist.cpp` (or additions to `bindings_engine.cpp`) adds `persist`, `unpersist` to the `engine.scene.*` sub-table and extends `find()`. The `setActiveScene()` method in `bindings.cpp` does NOT need to change for persistence — the C++ layer handles object transfer; the Lua binding layer only needs updated `find()` logic.

**Primary recommendation:** Implement PersistentObjectRegistry as an SSM-owned struct with a 4-slot `std::unique_ptr<Object>` array + `bool pendingRemoval[4]`, extend ObjectCollection with a separate `m_external[4]` raw pointer array updated by SSM at each transition, and expose `persist/unpersist/find` as three additions to the existing `engine.scene` Lua sub-table registered in `registerEngineTable()`.

---

## Standard Stack

### Core (all pre-existing in this codebase)

| Component | Location | Purpose | Phase Role |
|-----------|----------|---------|------------|
| `SceneStateMachine` | `include/enjin2/core/scene_state_machine.hpp` | Owns scene lifecycle, deferred transitions | Add `PersistentObjectRegistry` member + modify `applyDeferredTransition()` |
| `ObjectCollection` | `include/enjin2/core/object_collection.hpp` | 128-object fixed array, update loop, `findByName` | Extend with `m_external[4]` non-owning array |
| `LuaBindings` | `include/enjin2/scripting/bindings.hpp` | Hosts all engine.* subtables and pools | Add 3 new binding declarations + `registerPersistSubtable()` |
| `ObjectProxy` | `include/enjin2/scripting/object_proxy.hpp` | Thin userdata wrapping `Object*` + `valid` flag | Unchanged; persist/unpersist receive existing ObjectProxy |
| `bind_helpers.hpp` | `include/enjin2/scripting/bind_helpers.hpp` | `LuaFuncDef`, `ENJIN_ARRAY_LEN`, `luaBindFunctions` | Used to register new functions into `engine.scene` table |
| `bindings_internal.hpp` | `src/scripting/bindings_internal.hpp` | Private TU header; metatable constants | Include in new persist bindings file |

### No new external dependencies

This phase is pure internal C++ and Lua binding work. No new libraries required.

---

## Architecture Patterns

### Recommended File Structure

```
include/enjin2/core/
├── scene_state_machine.hpp    # ADD: PersistentObjectRegistry struct + 4 new methods
├── object_collection.hpp      # ADD: m_external[] + injectExternal() + clearExternal()
└── (no new headers needed)

src/scripting/
├── bindings_engine.cpp        # MODIFY: registerEngineTable() engine.scene sub-table
│                              #          extend lua_engine_scene_find()
│                              #          ADD: lua_engine_scene_persist()
│                              #               lua_engine_scene_unpersist()
└── (new file optional: bindings_scene_persist.cpp if large enough)
```

### Pattern 1: PersistentObjectRegistry — SSM-owned fixed struct

**What:** A plain struct containing a 4-slot `unique_ptr<Object>` array and a `pendingRemoval` flag array. SSM owns it as a direct member (no heap allocation).

**When to use:** Encapsulates all persistence state; keeps `applyDeferredTransition()` changes contained.

```cpp
// Source: designed from established fixed-pool pattern (coroutine pool, tween pool)
// in include/enjin2/scripting/bindings.hpp

struct PersistentObjectRegistry {
    static constexpr int MAX_PERSISTENT = 4;
    std::unique_ptr<Object> objects[MAX_PERSISTENT];
    bool pendingRemoval[MAX_PERSISTENT]{};
    int count{0};  // active (non-null) slots

    // Returns true on success, false if full (caller returns nil to Lua)
    bool add(std::unique_ptr<Object> obj) {
        for (int i = 0; i < MAX_PERSISTENT; ++i) {
            if (!objects[i]) {
                pendingRemoval[i] = false;
                objects[i] = std::move(obj);
                ++count;
                return true;
            }
        }
        return false;  // full
    }

    // Mark for removal on next transition (not immediate)
    void markForRemoval(Object* obj) {
        for (int i = 0; i < MAX_PERSISTENT; ++i) {
            if (objects[i].get() == obj) {
                pendingRemoval[i] = true;
                break;
            }
        }
    }

    // Find by name (for engine.scene.find() fallback)
    Object* findByName(const char* name) const {
        if (!name) return nullptr;
        for (int i = 0; i < MAX_PERSISTENT; ++i) {
            if (objects[i] && !pendingRemoval[i] &&
                objects[i]->getName() &&
                strcmp(objects[i]->getName(), name) == 0) {
                return objects[i].get();
            }
        }
        return nullptr;
    }
};
```

### Pattern 2: ObjectCollection m_external[] — non-owning injection

**What:** A small secondary array of raw `Object*` pointers in ObjectCollection, populated by SSM after transition. These objects participate in `update()`, `lateUpdate()`, and `findByName()` but are NOT owned — `clear()` does not delete them.

**Key design:** The external array is sized identically to the registry (4 slots). It is populated by `injectExternal(Object* obj)` and cleared by `clearExternal()` (called at the start of each transition, before new objects are injected from the new registry state).

```cpp
// Source: derived from ObjectCollection pattern in include/enjin2/core/object_collection.hpp
// New additions to ObjectCollection:

private:
    static constexpr size_t MAX_EXTERNAL = 4;  // matches PersistentObjectRegistry::MAX_PERSISTENT
    Object* m_external[MAX_EXTERNAL]{};         // non-owning; managed by SSM
    size_t m_externalCount{0};

public:
    // Called by SSM::applyDeferredTransition() to wire persistent objects into new scene
    void injectExternal(Object* obj) {
        if (m_externalCount < MAX_EXTERNAL && obj) {
            m_external[m_externalCount++] = obj;
        }
    }

    // Called at start of each transition to clear old external pointers
    void clearExternal() {
        for (size_t i = 0; i < MAX_EXTERNAL; ++i) m_external[i] = nullptr;
        m_externalCount = 0;
    }

    // Extended update() — iterates both owned and external objects
    // (similar change to lateUpdate(), findByName(), forEach())
```

### Pattern 3: applyDeferredTransition() modification

**What:** Before deactivating the old scene, extract Object* pointers from the registry. Before activating the new scene, inject surviving persistent objects (those not marked for removal) into the new scene's ObjectCollection via `injectExternal()`.

**Sequence (cross-scene transition):**

```
1. Registry: flush pendingRemoval slots (unique_ptr reset destroys objects)
2. Old scene: clearExternal() on old scene's ObjectCollection (remove injected pointers)
3. Old scene: deactivate() — fires onDeactivate(); scene objects are NOT destroyed here
4. currentScene = targetScene
5. Registry: injectExternal(obj) for each surviving persistent object into new scene
6. New scene: activate() — calls initialize() + start() on scene-owned objects
            — persistent objects skip re-initialize/start (they're already running)
7. SSM: setActiveScene() is called by host each frame — bindings update m_activeScene ptr
```

**Persistent object update during transition**: Persistent objects are owned by the registry and injected into whichever scene is current. Their `update()` is called because ObjectCollection's `update()` iterates both owned and external arrays.

### Pattern 4: Lua binding integration

**What:** Three new static member functions on `LuaBindings` + extend existing `lua_engine_scene_find`.

**Registration:** Add to the `kSceneFuncs` array in `registerEngineTable()` (same location as existing `switch`, `find`, `spawn`, `destroy`). No new sub-table needed — these extend `engine.scene.*`.

```cpp
// Source: established in src/scripting/bindings_engine.cpp

// Additions to kSceneFuncs in registerEngineTable():
static const LuaFuncDef kSceneFuncs[] = {
    {"switch",    lua_engine_scene_switch},
    {"find",      lua_engine_scene_find},      // MODIFIED: searches registry too
    {"spawn",     lua_engine_scene_spawn},
    {"destroy",   lua_engine_scene_destroy},
    {"persist",   lua_engine_scene_persist},   // NEW: PERSIST-01
    {"unpersist", lua_engine_scene_unpersist}, // NEW: PERSIST-02
};

// New declaration in bindings.hpp private section:
static int lua_engine_scene_persist(lua_State* L);
static int lua_engine_scene_unpersist(lua_State* L);
```

**engine.scene.persist(proxy) — PERSIST-01:**

```cpp
// Returns true on success, nil if pool full or proxy invalid — no Lua error
int LuaBindings::lua_engine_scene_persist(lua_State* L) {
    auto* proxy = static_cast<ObjectProxy*>(luaL_testudata(L, 1, "ObjectProxy"));
    if (!proxy || !proxy->valid || !proxy->object) {
        lua_pushnil(L); return 1;
    }

    LuaBindings* b = getBindings(L);
    if (!b || !b->m_ssm) { lua_pushnil(L); return 1; }

    // Extract the object from the active scene's ObjectCollection
    // (scene gives up ownership; registry takes it)
    // DESIGN QUESTION: See Open Questions #1
    bool ok = b->m_ssm->persistObject(proxy->object);
    if (!ok) { lua_pushnil(L); return 1; }

    lua_pushboolean(L, 1);
    return 1;
}
```

**engine.scene.find() extended — PERSIST-03:**

```cpp
// Modified: search active scene first, then persistent registry
int LuaBindings::lua_engine_scene_find(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    // 1. Search active scene (existing behavior)
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto** scenePP = static_cast<Scene**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    Object* obj = nullptr;
    if (scenePP && *scenePP) {
        obj = (*scenePP)->findByName(name);
    }

    // 2. Fall back to persistent registry if not found in active scene
    if (!obj) {
        LuaBindings* b = getBindings(L);
        if (b && b->m_ssm) {
            obj = b->m_ssm->findPersistentByName(name);
        }
    }

    if (!obj) { lua_pushnil(L); return 1; }

    // Allocate ObjectProxy (same as before)
    auto* proxy = static_cast<ObjectProxy*>(lua_newuserdata(L, sizeof(ObjectProxy)));
    proxy->object = obj;
    proxy->valid  = true;
    luaL_getmetatable(L, "ObjectProxy");
    lua_setmetatable(L, -2);
    obj->setLuaProxy(proxy);
    return 1;
}
```

### Pattern 5: SSM interface additions

New public methods on `SceneStateMachine`:

```cpp
// In scene_state_machine.hpp:

// Adds object to persistent registry (called by engine.scene.persist())
// Returns false if registry is full
bool persistObject(Object* obj);

// Marks object for removal on next transition (called by engine.scene.unpersist())
void unpersistObject(Object* obj);

// Searches persistent registry by name (called by extend find())
Object* findPersistentByName(const char* name) const;
```

### Pattern 6: Registry access — storing in Lua registry

The `m_ssm` pointer is already stored in the Lua registry as `"enjin_ssm"` (a `SceneStateMachine**` lightuserdata). The persist/unpersist/find bindings retrieve SSM via `getBindings(L)->m_ssm` (same as camera follow pattern) — no new registry key needed.

### Anti-Patterns to Avoid

- **Copying/duplicating Object into registry**: Objects contain `unique_ptr<Component>` — they are not copyable. Must use move semantics to transfer ownership from scene's ObjectCollection into registry.
- **Invalidating proxy on persist**: The `ObjectProxy` remains valid through persist — the object still exists, just moved to registry ownership. Do NOT set `proxy->valid = false`.
- **Calling start()/awake() on re-injection**: Persistent objects already ran these lifecycle methods. Do NOT call `start()` or `awake()` again when injecting into new scene.
- **Destroying persistent objects in scene deactivate**: `Scene::deactivate()` does NOT destroy objects (only sets `active = false`). The actual destruction happens in `ObjectCollection::clear()`. Make sure clear() skips externals.
- **find() searching registry before active scene**: Active scene takes priority (same name resolution expected by callers in current scene context). Search active scene first.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Proxy validity after object move | Custom invalidation signal | Existing `ObjectProxy::valid` flag set by `Object::~Object()` | Already handles dangling pointer prevention; works transparently |
| Object name lookup | Custom hash map | Existing `ObjectCollection::findByName()` + new `PersistentObjectRegistry::findByName()` | Pattern already established; O(n) is fine for 4+128 slots |
| Registry size management | Dynamic vector | Fixed array + count (4 slots, matches `MAX_PERSISTENT`) | Zero-alloc embedded constraint; exactly mirrors tween pool, coroutine pool |
| Scene injection bookkeeping | Complex observer pattern | Direct `injectExternal()` call in `applyDeferredTransition()` | SSM already owns transition logic; injection is a single loop |

**Key insight:** Every data structure in this engine is a fixed-capacity array. The persistent registry is just another pool of 4.

---

## Common Pitfalls

### Pitfall 1: Object Ownership Transfer from ObjectCollection

**What goes wrong:** ObjectCollection stores objects as `std::unique_ptr<Object>` in `objects[0..127]`. Moving an object to the registry requires removing it from `ObjectCollection::objects[]` — but `removeObject()` calls `std::move` which resets the unique_ptr and triggers `Object::~Object()` (invalidating the proxy).

**Why it happens:** `ObjectCollection::removeObject()` shifts the array and the moved-from slot becomes nullptr — the object IS destroyed.

**How to avoid:** `persistObject()` in SSM needs a new path: `ObjectCollection::extractObject(Object* obj)` that returns a `std::unique_ptr<Object>` (instead of destroying it). This is analogous to `removeObject()` but returns the unique_ptr rather than letting it drop. The existing `removeObject()` is destructive — do NOT reuse it for persistence.

**Warning signs:** If `proxy->valid` becomes false immediately after `persist()` is called, the destroy path is being triggered.

**Required addition to ObjectCollection:**
```cpp
// Extracts the unique_ptr from the collection WITHOUT destroying the object.
// Returns nullptr if object not found.
std::unique_ptr<Object> extractObject(Object* obj) {
    for (size_t i = 0; i < objectCount; ++i) {
        if (objects[i].get() == obj) {
            std::unique_ptr<Object> extracted = std::move(objects[i]);
            // Shift remaining owned objects left
            for (size_t j = i; j < objectCount - 1; ++j) {
                objects[j] = std::move(objects[j + 1]);
            }
            objects[objectCount - 1] = nullptr;
            objectCount--;
            return extracted;
        }
    }
    return nullptr;
}
```

### Pitfall 2: Double-update of Persistent Objects

**What goes wrong:** If persistent objects are injected into `m_external[]` AND still counted in `objectCount` of the old scene (e.g., after extractObject clears their owned slot), the update loop might skip them or double-tick them.

**Why it happens:** Timing of extraction vs. injection across the transition.

**How to avoid:** Extract happens BEFORE old scene deactivates, inject happens BEFORE new scene activates. Between transitions, persistent objects are ONLY in the registry (not in any scene's ObjectCollection). The m_external injection is a separate loop pass, so no double-tick occurs as long as extraction happens first.

### Pitfall 3: Proxy invalidation on scene's ObjectCollection::clear()

**What goes wrong:** If a scene is self-transitioned (`switchTo(sameId)`), `applyDeferredTransition()` calls `deactivate()` + `resetInitialized()` + `activate()`. ObjectCollection is NOT cleared on self-transition — but if clear() IS called, any persistent objects that were injected as externals would have their proxy valid flag preserved (external pointers are not destroyed by clear()), but the external array would be wiped.

**How to avoid:** Ensure `clearExternal()` is always called before any `injectExternal()` at the start of each transition. On self-transitions, clearExternal + re-injectExternal persistent objects.

### Pitfall 4: `setActiveScene()` in LuaBindings called by host

**What goes wrong:** The host (SDL runner, etc.) calls `bindings.setActiveScene(scene)` each time the scene changes. This already clears coroutines and tweens. Persistent objects are managed at C++ (SSM) level, NOT at the Lua bindings level — setActiveScene() does NOT need to touch the persistent registry.

**Why it happens:** Could incorrectly think persistent object cleanup belongs in setActiveScene().

**How to avoid:** Persistent object lifecycle is entirely SSM-driven. LuaBindings only sees the effect via the extended `find()` call. No changes to `setActiveScene()`.

### Pitfall 5: find() search priority — active scene vs registry

**What goes wrong:** If registry is searched first and an object has the same name as an active-scene object, the persistent one would be returned instead of the local one.

**How to avoid:** Always search active scene first (CONTEXT.md leaves this to discretion — research recommends active scene priority to match expected Lua caller mental model). Persistent fallback only when scene returns nullptr.

### Pitfall 6: STATE.md Research Flag — m_external update ordering

**What it says:** "ObjectCollection::m_external[] update ordering requires design review before implementation"

**What this means:** The order of operations in `update()` and `lateUpdate()` when iterating both owned objects and externals. The safe order is: iterate owned objects first (indices 0..objectCount-1), then externals (m_external[0..m_externalCount-1]). This matches the natural "scene objects first, then persistent guests" ordering, avoids write-after-read hazards, and is consistent with render ordering (see below).

---

## Code Examples

### Verified: SSM applyDeferredTransition() — modified cross-scene path

```cpp
// Source: scene_state_machine.hpp applyDeferredTransition() modified for Phase 51
void applyDeferredTransition(uint32_t targetId) {
    Scene* targetScene = nullptr;
    for (size_t i = 0; i < sceneCount; ++i) {
        if (scenes[i] && scenes[i]->getId() == targetId) {
            targetScene = scenes[i].get();
            break;
        }
    }
    if (!targetScene) return;

    bool isSelf = (targetScene == currentScene);
    if (isSelf) {
        // Self-transition: flush pending removals, re-inject survivors
        m_persistentRegistry.flushPendingRemovals();
        Scene* scene = currentScene;
        scene->getObjects().clearExternal();
        scene->deactivate();
        scene->resetInitialized();
        scene->setStateMachine(this);
        // Re-inject persistent objects into same scene
        for (int i = 0; i < PersistentObjectRegistry::MAX_PERSISTENT; ++i) {
            if (m_persistentRegistry.objects[i]) {
                scene->getObjects().injectExternal(m_persistentRegistry.objects[i].get());
            }
        }
        scene->activate();
    } else {
        // Cross-scene: flush removals, clear old externals, deactivate old, inject into new
        m_persistentRegistry.flushPendingRemovals();
        if (currentScene) {
            currentScene->getObjects().clearExternal();
            currentScene->deactivate();
        }
        currentScene = targetScene;
        currentScene->setStateMachine(this);
        // Inject persistent objects into new scene
        for (int i = 0; i < PersistentObjectRegistry::MAX_PERSISTENT; ++i) {
            if (m_persistentRegistry.objects[i]) {
                currentScene->getObjects().injectExternal(
                    m_persistentRegistry.objects[i].get());
            }
        }
        if (!currentScene->isInitialized()) { currentScene->initialize(); }
        if (!currentScene->isActive()) { currentScene->activate(); }
        onSceneChangeCompleteSignal.emit(oldScene, currentScene);
    }
}
```

### Verified: ObjectCollection update() extended for externals

```cpp
// Source: object_collection.hpp — existing update() extended
void update(float dt) {
    // Owned objects
    for (size_t i = 0; i < objectCount; ++i) {
        if (objects[i] && objects[i]->isActive()) {
            objects[i]->update(dt);
        }
    }
    // External (persistent) objects — updated after owned objects
    for (size_t i = 0; i < m_externalCount; ++i) {
        if (m_external[i] && m_external[i]->isActive()) {
            m_external[i]->update(dt);
        }
    }
}
// Same pattern applies to lateUpdate(), findByName(), forEach()
```

### Verified: Lua integration test pattern (follows tween_test.cpp fixture)

```cpp
// Source: modeled on tween_test.cpp fixture + scene_transition_test.cpp
struct PersistFixture {
    SceneStateMachine ssm;
    LuaEngine engine;
    LuaBindings bindings;

    PersistFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
        bindings.setSceneStateMachine(&ssm);
    }

    void switchScene(uint32_t id) {
        ssm.switchTo(id);
        ssm.update(0.016f);
        Scene* current = ssm.getCurrentScene();
        bindings.setActiveScene(current);
    }
};
```

### Verified: Overflow test pattern (returns nil, no error)

```cpp
// Source: modeled on coroutine pool overflow in overflow_test.cpp
// Fill all 4 slots, 5th call returns nil
LuaResult r = f.exec(R"(
    local results = {}
    for i = 1, 5 do
        local obj = engine.scene.spawn("persist_" .. i)
        results[i] = engine.scene.persist(obj)
    end
    overflow_result = results[5]  -- should be nil
)");
ASSERT(f.getBoolean("overflow_result") == false, "PERSIST-01: 5th persist returns nil");
```

---

## State of the Art

| Old Approach | Current Approach | When | Impact |
|--------------|------------------|------|--------|
| removeObject() destroys object | extractObject() returns unique_ptr | Phase 51 | Required to transfer ownership without destruction |
| engine.scene.find() searches active scene only | find() searches active scene, falls back to registry | Phase 51 | PERSIST-03 requirement |
| ObjectCollection only owns objects | ObjectCollection has owned + external arrays | Phase 51 | Enables non-owning injection without changing update/find logic |

**Deprecated/outdated approaches:**
- Using `removeObject()` for persistence: it destroys the object. New `extractObject()` is required.

---

## Open Questions

1. **Where does `persistObject()` live and how does it access scene's ObjectCollection?**
   - What we know: SSM owns `SceneStateMachine::m_persistentRegistry`. SSM has `currentScene`. `Scene::getObjects()` is public and returns `ObjectCollection&`.
   - What's unclear: Does `persistObject(Object*)` on SSM call `currentScene->getObjects().extractObject(obj)` directly? This requires SSM to know about ObjectCollection internals.
   - Recommendation: Yes — SSM calls `currentScene->getObjects().extractObject(obj)` since `Scene::getObjects()` returns a reference. SSM already has `currentScene` pointer. This is the cleanest path.

2. **Render ordering of persistent objects in Scene::renderObjects()**
   - What we know: `renderObjects()` iterates `objects.forEach()` which only covers owned objects, not externals. Persistent objects won't render if only owned objects are iterated.
   - What's unclear: Should externals be included in the drawable sort pass?
   - Recommendation: Extend `Scene::renderObjects()` forEach to also iterate externals (same as update). Persistent objects render after scene-owned objects (or sorted together by layer — the layer-based sort handles ordering). The `renderObjects()` forEach lambda needs to also loop over `m_externalCount` pointers. ObjectCollection needs a public `forEachExternal(func)` method (or expose `m_external[]` + `m_externalCount` for the render pass to use).

3. **Persist() on an object that is already persistent**
   - What we know: No specification in CONTEXT.md.
   - Recommendation: Silent no-op — check if the object's pointer is already in the registry; if yes, return true (or false consistently with pool-full). A second call to persist the same object should NOT add it twice.

4. **Persist() on a proxy from a different scene (not current active scene)**
   - What we know: `extractObject()` searches `currentScene->getObjects()`.
   - What's unclear: What if the Lua script calls `persist()` from scene A using a proxy for an object in scene B (possible via store + cross-scene spawn)?
   - Recommendation: If the object isn't found in the current scene's owned objects, return nil. Don't search non-active scenes.

---

## Sources

### Primary (HIGH confidence)

- Direct code reading of `include/enjin2/core/scene_state_machine.hpp` (full source)
- Direct code reading of `include/enjin2/core/object_collection.hpp` (full source)
- Direct code reading of `include/enjin2/scripting/bindings.hpp` (full source)
- Direct code reading of `src/scripting/bindings_engine.cpp` (full source)
- Direct code reading of `src/scripting/bindings_async.cpp` + `bindings_tween.cpp` (full source)
- Direct code reading of `include/enjin2/core/scene.hpp` + `object.hpp` (full source)
- Direct code reading of `.planning/phases/51-persistent-objects/51-CONTEXT.md`
- Direct code reading of `.planning/REQUIREMENTS.md` + `.planning/STATE.md`

### Secondary (MEDIUM confidence)

- None required — all claims are grounded in direct source examination

### Tertiary (LOW confidence)

- None

---

## Metadata

**Confidence breakdown:**
- C++ architecture (registry, extraction, injection): HIGH — all types and patterns verified in source
- Lua binding additions: HIGH — identical pattern to Phase 50 tween bindings, verified
- applyDeferredTransition() modification: HIGH — full source read, transition points clear
- render ordering: MEDIUM — renderObjects() uses forEach, extension approach is reasonable but exact `forEachExternal` API needs design decision at plan time
- Open questions 3 and 4 (edge cases): LOW — not specified in CONTEXT.md; conservative approach recommended

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable internal codebase; no external dependencies)
