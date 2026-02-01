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

### `` SceneStateMachine()``

Constructor. 


        

---

### `` ~SceneStateMachine()=default``

Destructor. 


        

---

### ``T * addScene(uint32_t sceneId, Args &&... args)``

Add a scene to the state machine. 

templateparamT type (must derive from ) Sceneclassenjin2_1_1ScenecompoundSceneclassenjin2_1_1ScenecompoundArgsConstructor argument types paramsceneIdUnique scene identifier argsConstructor arguments returnPointer to created scene or nullptr if failed 

---

### ``bool removeScene(uint32_t sceneId)``

Remove a scene from the state machine. 

paramsceneId identifier to remove Sceneclassenjin2_1_1ScenecompoundreturnTrue if scene was removed 

---

### ``bool changeScene(uint32_t sceneId, TransitionType transition=TransitionType::IMMEDIATE, uint16_t duration=0)``

Change to a different scene. 

paramsceneIdTarget scene identifier transitionTransition type to use durationTransition duration in milliseconds (0 = use default) returnTrue if transition started successfully 

---

### ``void update(uint16_t deltaTime)``

Update the state machine. 

paramdeltaTimeTime since last frame in milliseconds 

---

### ``void render(ICanvas&lt; PixelType &gt; &canvas)``

Render the current scene with transition effects. 

paramcanvasTarget canvas for rendering 

---

### `` *Sceneclassenjin2_1_1Scenecompound getCurrentScene()``

Get current scene. 

returnPointer to current scene or nullptr 

---

### `` *Sceneclassenjin2_1_1Scenecompound getScene(uint32_t sceneId)``

Get scene by ID. 

paramsceneId identifier Sceneclassenjin2_1_1ScenecompoundreturnPointer to scene or nullptr if not found 

---

### ``bool isTransitioning() const const``

Check if a transition is currently active. 

returnTrue if transitioning 

---

### ``float getTransitionProgress() const const``

Get current transition progress. 

returnProgress value from 0.0 to 1.0 

---

### ``<  *,  * >SignalConnectionclassenjin2_1_1SignalConnectioncompoundSceneclassenjin2_1_1ScenecompoundSceneclassenjin2_1_1Scenecompound connectOnSceneChangeStart(std::function&lt; void(Scene *, Scene *)&gt; callback)``

Connect to transition events. 


        

---

### ``<  *,  * >SignalConnectionclassenjin2_1_1SignalConnectioncompoundSceneclassenjin2_1_1ScenecompoundSceneclassenjin2_1_1Scenecompound connectOnSceneChangeComplete(std::function&lt; void(Scene *, Scene *)&gt; callback)``


        


        

---

### ``<  >SignalConnectionclassenjin2_1_1SignalConnectioncompoundTransitionTypeclassenjin2_1_1SceneStateMachine_1ac0494c42046b4b5aad9d77883aee936amember connectOnTransitionStart(std::function&lt; void(TransitionType)&gt; callback)``


        


        

---

### ``< float >SignalConnectionclassenjin2_1_1SignalConnectioncompound connectOnTransitionProgress(std::function&lt; void(float)&gt; callback)``


        


        

---

## Private Methods

### ``void startTransition()``

Start a transition. 


        

---

### ``void updateTransition(uint16_t deltaTime)``

Update transition state. 

paramdeltaTimeTime since last frame 

---

### ``void completeTransition()``

Complete the current transition. 


        

---

### ``void renderWithTransition(ICanvas&lt; PixelType &gt; &canvas)``

Render scenes with transition effects. 

paramcanvasTarget canvas 

---

### ``void renderFadeTransition(ICanvas&lt; PixelType &gt; &canvas)``

Render fade transition. 

paramcanvasTarget canvas 

---

### ``void renderSlideTransition(ICanvas&lt; PixelType &gt; &canvas)``

Render slide transition. 

paramcanvasTarget canvas 

---

