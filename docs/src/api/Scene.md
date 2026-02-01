---
id: Scene
title: Scene
sidebar_label: Scene
---

# Scene

Base class for game scenes. 


Manages a collection of objects and provides lifecycle methods for scene initialization, updating, and cleanup. 

---

**Namespace:** enjin2

**Header:** include/enjin2/core/scene.hpp

## Public Methods

### `cpp
* Scene(uint32_t id)*
``

Constructor. 

paramidUnique scene identifier 

---

### `cpp
*virtual  ~Scene()*
``

Virtual destructor. 


        

---

### `cpp
*void initialize()*
``

Initialize the scene. 

Called once when the scene is first created. Override to set up initial objects and state. 

---

### `cpp
*void activate()*
``

Activate the scene. 

Called when the scene becomes the active scene. 

---

### `cpp
*void deactivate()*
``

Deactivate the scene. 

Called when the scene is no longer active. 

---

### `cpp
*void update(uint16_t deltaTime)*
``

Update the scene. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `cpp
*void render(ICanvas&lt; PixelType &gt; &canvas)*
``

Render the scene. 

paramcanvasTarget canvas for rendering 

---

### `cpp
*T * addObject(Args &&... args)*
``

Add an object to the scene. 

templateparamT type (must derive from ) Objectclassenjin2_1_1ObjectcompoundObjectclassenjin2_1_1ObjectcompoundArgsConstructor argument types paramargsConstructor arguments returnPointer to created object or nullptr if failed 

---

### `cpp
*bool removeObject(Object *object)*
``

Remove an object from the scene. 

paramobject to remove Objectclassenjin2_1_1ObjectcompoundreturnTrue if object was removed 

---

### `cpp
*T * findObject()*
``

Find first object of specified type. 

templateparamT type Objectclassenjin2_1_1ObjectcompoundreturnPointer to object or nullptr if not found 

---

### `cpp
* *Objectclassenjin2_1_1Objectcompound findObjectWithComponent()*
``

Find object with component of specified type. 

templateparamT type Componentclassenjin2_1_1ComponentcompoundreturnPointer to object or nullptr if not found 

---

### `cpp
*uint32_t getId() const const*
``

Get scene ID. 

return identifier Sceneclassenjin2_1_1Scenecompound

---

### `cpp
*bool isActive() const const*
``

Check if scene is active. 

returnTrue if scene is active 

---

### `cpp
*bool isInitialized() const const*
``

Check if scene is initialized. 

returnTrue if scene is initialized 

---

### `cpp
* &ObjectCollectionclassenjin2_1_1ObjectCollectioncompound getObjects()*
``

Get object collection. 

returnReference to object collection 

---

### `cpp
*const  &ObjectCollectionclassenjin2_1_1ObjectCollectioncompound getObjects() const const*
``

Get object collection (const). 

returnConst reference to object collection 

---

### `cpp
*&lt;  * &gt;SignalConnectionclassenjin2_1_1SignalConnectioncompoundSceneclassenjin2_1_1Scene_1a4a7dcc3e8b941246279ad1c3fda6ed09member connectOnCreate(std::function&lt; void(Scene *)&gt; callback)*
``

Connect to scene lifecycle events. 


        

---

### `cpp
*&lt;  * &gt;SignalConnectionclassenjin2_1_1SignalConnectioncompoundSceneclassenjin2_1_1Scene_1a4a7dcc3e8b941246279ad1c3fda6ed09member connectOnActivate(std::function&lt; void(Scene *)&gt; callback)*
``


        


        

---

### `cpp
*&lt;  * &gt;SignalConnectionclassenjin2_1_1SignalConnectioncompoundSceneclassenjin2_1_1Scene_1a4a7dcc3e8b941246279ad1c3fda6ed09member connectOnDeactivate(std::function&lt; void(Scene *)&gt; callback)*
``


        


        

---

### `cpp
*&lt;  * &gt;SignalConnectionclassenjin2_1_1SignalConnectioncompoundSceneclassenjin2_1_1Scene_1a4a7dcc3e8b941246279ad1c3fda6ed09member connectOnDestroy(std::function&lt; void(Scene *)&gt; callback)*
``


        


        

---

## Protected Methods

### `cpp
*virtual void onCreate()*
``

Called when scene is created (override in derived classes). 

Use this to initialize scene-specific data and create initial objects. 

---

### `cpp
*virtual void onActivate()*
``

Called when scene becomes active (override in derived classes). 

Use this to resume animations, start background processes, etc. 

---

### `cpp
*virtual void onDeactivate()*
``

Called when scene becomes inactive (override in derived classes). 

Use this to pause animations, stop background processes, etc. 

---

### `cpp
*virtual void onDestroy()*
``

Called when scene is destroyed (override in derived classes). 

Use this to clean up scene-specific resources. 

---

### `cpp
*virtual void onUpdate(uint16_t deltaTime)*
``

Called every frame (override in derived classes). 


Use this for scene-specific update logic that should happen before object updates. paramdeltaTimeTime since last frame in milliseconds

---

### `cpp
*virtual void onRender(ICanvas&lt; Pixel4 &gt; &canvas)*
``

Called during rendering for 4-bit canvas (override in derived classes). 


Use this for scene-specific rendering like backgrounds or UI overlays. paramcanvasTarget canvas for rendering

---

### `cpp
*virtual void onRender(ICanvas&lt; uint8_t &gt; &canvas)*
``

Called during rendering for 8-bit canvas (override in derived classes). 


Use this for scene-specific rendering like backgrounds or UI overlays. paramcanvasTarget canvas for rendering

---

## Private Methods

### `cpp
*void renderObjects(ICanvas&lt; PixelType &gt; &canvas)*
``

Render all objects in the scene. 

paramcanvasTarget canvas for rendering 

---

