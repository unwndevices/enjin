---
id: Scene
title: Scene
sidebar_label: Scene
---

# Scene

Base class for game scenes. 


Manages a collection of objects and provides lifecycle methods for scene initialization, updating, and cleanup. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/scene.hpp`

## Public Methods

### ` Scene(uint32_t id)`

Constructor. 

idUnique scene identifier 

---

### `virtual  ~Scene()`

Virtual destructor. 

---

### `void initialize()`

Initialize the scene. 

Called once when the scene is first created. Override to set up initial objects and state. 

---

### `void activate()`

Activate the scene. 

Called when the scene becomes the active scene. 

---

### `void deactivate()`

Deactivate the scene. 

Called when the scene is no longer active. 

---

### `void update(float dt)`

Update the scene. 

dtTime since last frame in seconds 

---

### `void render(ICanvas&lt; PixelType &gt; &canvas)`

Render the scene. 

canvasTarget canvas for rendering 

---

### `T * addObject(Args &&... args)`

Add an object to the scene. 

TObject type (must derive from Object) ArgsConstructor argument types argsConstructor arguments Pointer to created object or nullptr if failed 

---

### `bool removeObject(Object *object)`

Remove an object from the scene. 

objectObject to remove True if object was removed 

---

### `T * findObject()`

Find first object of specified type. 

TObject type Pointer to object or nullptr if not found 

---

### `Object * findObjectWithComponent()`

Find object with component of specified type. 

TComponent type Pointer to object or nullptr if not found 

---

### `Object * findByName(const char *name)`

Find first object with the given name. 

nameName to search for (string literal, case-sensitive) Pointer to matching Object or nullptr if not found 

---

### `size_t findAllWithTag(const char *tag, Object **results, size_t maxResults)`

Find all objects carrying the given tag. 

tagTag to search for (string literal, case-sensitive) resultsCaller-provided array to write matching Object pointers into maxResultsMaximum number of results to write Number of objects written into results 

---

### `uint32_t getId() const`

Get scene ID. 

Scene identifier 

---

### `bool isActive() const`

Check if scene is active. 

True if scene is active 

---

### `bool isInitialized() const`

Check if scene is initialized. 

True if scene is initialized 

---

### `void setStateMachine(SceneStateMachine *ssm)`

Inject non-owning SSM back-pointer. 

ssmOwning SceneStateMachine (called before activate()) 

---

### `void resetInitialized()`

Reset initialized guard to allow re-initialization (used for self-transitions). 

---

### `ObjectCollection & getObjects()`

Get object collection. 

Reference to object collection 

---

### `const ObjectCollection & getObjects() const`

Get object collection (const). 

Const reference to object collection 

---

### `SignalConnection&lt; Scene * &gt; connectOnCreate(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene create event. 

callbackFunction called when scene is created Signal connection handle 

---

### `SignalConnection&lt; Scene * &gt; connectOnActivate(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene activate event. 

callbackFunction called when scene becomes active Signal connection handle 

---

### `SignalConnection&lt; Scene * &gt; connectOnDeactivate(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene deactivate event. 

callbackFunction called when scene becomes inactive Signal connection handle 

---

### `SignalConnection&lt; Scene * &gt; connectOnDestroy(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene destroy event. 

callbackFunction called when scene is destroyed Signal connection handle 

---

## Protected Methods

### `virtual void onCreate()`

Called when scene is created (override in derived classes). 

Use this to initialize scene-specific data and create initial objects. 

---

### `virtual void onActivate()`

Called when scene becomes active (override in derived classes). 

Use this to resume animations, start background processes, etc. 

---

### `virtual void onDeactivate()`

Called when scene becomes inactive (override in derived classes). 

Use this to pause animations, stop background processes, etc. 

---

### `virtual void onDestroy()`

Called when scene is destroyed (override in derived classes). 

Use this to clean up scene-specific resources. 

---

### `virtual void onUpdate(float dt)`

Called every frame (override in derived classes). 

dtTime since last frame in seconds
Use this for scene-specific update logic that should happen before object updates. 

---

### `virtual void onRender(ICanvas&lt; Pixel4 &gt; &canvas)`

Called during rendering for 4-bit canvas (override in derived classes). 

canvasTarget canvas for rendering
Use this for scene-specific rendering like backgrounds or UI overlays. 

---

### `virtual void onRender(ICanvas&lt; uint8_t &gt; &canvas)`

Called during rendering for 8-bit canvas (override in derived classes). 

canvasTarget canvas for rendering
Use this for scene-specific rendering like backgrounds or UI overlays. 

---

## Private Methods

### `void renderObjects(ICanvas&lt; PixelType &gt; &canvas)`

Render all objects in the scene. 

canvasTarget canvas for rendering 

---

