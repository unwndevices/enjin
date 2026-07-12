---
id: SceneStateMachine
title: SceneStateMachine
sidebar_label: SceneStateMachine
---

# SceneStateMachine

State machine for managing scene transitions. 


Provides a centralized system for managing multiple scenes, handling transitions between them, and maintaining scene state. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/scene_state_machine.hpp`

## Public Methods

### ` SceneStateMachine()`

Constructor. 

---

### ` ~SceneStateMachine()=default`

Destructor. 

---

### `T * addScene(uint32_t sceneId, Args &&... args)`

Add a scene to the state machine. 

TScene type (must derive from Scene) ArgsConstructor argument types sceneIdUnique scene identifier argsConstructor arguments Pointer to created scene or nullptr if failed 

---

### `bool removeScene(uint32_t sceneId)`

Remove a scene from the state machine. 

sceneIdScene identifier to remove True if scene was removed 

---

### `bool changeScene(uint32_t sceneId, TransitionType transition=TransitionType::IMMEDIATE, float duration=0.0f)`

Change to a different scene. 

sceneIdTarget scene identifier transitionTransition type to use durationTransition duration in seconds (0 = use default) True if transition started successfully 

---

### `void switchTo(uint32_t sceneId)`

Request a deferred scene transition (safe to call from onUpdate()). 

Unlike changeScene(), switchTo() queues the transition for execution AFTER the current scene's update() returns, preventing re-entrant corruption. Calling switchTo(currentId) triggers a full reset: onDeactivate, onCreate, onActivate.Last-wins: multiple switchTo() calls in the same frame overwrite each other. Re-entrancy from onDeactivate(): safely queued for the next frame.sceneIdTarget scene identifier (must already be registered via addScene) 

---

### `void update(float dt)`

Update the state machine. 

dtTime since last frame in seconds 

---

### `void render(ICanvas&lt; PixelType &gt; &canvas)`

Render the current scene with transition effects. 

canvasTarget canvas for rendering 

---

### `Scene * getCurrentScene()`

Get current scene. 

Pointer to current scene or nullptr 

---

### `Scene * getScene(uint32_t sceneId)`

Get scene by ID. 

sceneIdScene identifier Pointer to scene or nullptr if not found 

---

### `bool isTransitioning() const`

Check if a transition is currently active. 

True if transitioning 

---

### `float getTransitionProgress() const`

Get current transition progress. 

Progress value from 0.0 to 1.0 

---

### `SignalConnection&lt; Scene *, Scene * &gt; connectOnSceneChangeStart(std::function&lt; void(Scene *, Scene *)&gt; callback)`

Connect to scene change start event. 

callbackFunction called with (from_scene, to_scene) Signal connection handle 

---

### `SignalConnection&lt; Scene *, Scene * &gt; connectOnSceneChangeComplete(std::function&lt; void(Scene *, Scene *)&gt; callback)`

Connect to scene change complete event. 

callbackFunction called with (from_scene, to_scene) Signal connection handle 

---

### `bool persistObject(Object *obj)`

Extract an object from the current scene and register it as persistent. 

The object survives all subsequent scene transitions. It is immediately re-injected as a non-owning external into the current scene's ObjectCollection, so it continues to participate in update/lateUpdate/findByName/forEach during the current scene and all subsequent scenes. If the object is already persistent, returns true (silent no-op — no duplicate slot consumed).objRaw pointer to an owned object in the current scene true if now persistent, false if object not in current scene or registry full 

---

### `void unpersistObject(Object *obj)`

Schedule a persistent object for destruction on the next scene transition. 

The object remains alive until applyDeferredTransition() calls flushPendingRemovals(). Its proxy will be invalidated at that point.objRaw pointer of the persistent object to remove 

---

### `Object * findPersistentByName(const char *name) const`

Find a persistent object by name (skips pending-removal objects). 

nameName to search for Pointer to matching Object or nullptr 

---

### `SignalConnection&lt; TransitionType &gt; connectOnTransitionStart(std::function&lt; void(TransitionType)&gt; callback)`

Connect to transition start event. 

callbackFunction called with transition type Signal connection handle 

---

### `SignalConnection&lt; float &gt; connectOnTransitionProgress(std::function&lt; void(float)&gt; callback)`

Connect to transition progress event. 

callbackFunction called with progress value (0.0-1.0) Signal connection handle 

---

## Private Methods

### `void startTransition()`

Start a transition. 

---

### `void applyDeferredTransition(uint32_t targetId)`

Apply a deferred transition queued by switchTo(). 

Called from update() after currentScene-&gt;update() returns. Self-transitions trigger a full reset cycle: deactivate -&gt; resetInitialized -&gt; activate. Cross-scene transitions are equivalent to an immediate changeScene().targetIdScene ID to transition to 

---

### `void updateTransition(float dt)`

Update transition state. 

dtTime since last frame in seconds 

---

### `void completeTransition()`

Complete the current transition. 

---

### `void renderWithTransition(ICanvas&lt; PixelType &gt; &canvas)`

Render scenes with transition effects. 

canvasTarget canvas 

---

### `void renderFadeTransition(ICanvas&lt; PixelType &gt; &canvas)`

Render fade transition. 

canvasTarget canvas 

---

### `void renderSlideTransition(ICanvas&lt; PixelType &gt; &canvas)`

Render slide transition. 

canvasTarget canvas 

---

