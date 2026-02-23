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

### `bool changeScene(uint32_t sceneId, TransitionType transition=TransitionType::IMMEDIATE, uint16_t duration=0)`

Change to a different scene. 

sceneIdTarget scene identifier transitionTransition type to use durationTransition duration in milliseconds (0 = use default) True if transition started successfully 

---

### `void update(uint16_t deltaTime)`

Update the state machine. 

deltaTimeTime since last frame in milliseconds 

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

### `void updateTransition(uint16_t deltaTime)`

Update transition state. 

deltaTimeTime since last frame 

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

