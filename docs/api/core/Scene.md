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

### `void update(uint16_t deltaTime)`

Update the scene. 

deltaTimeTime since last frame in milliseconds 

---

### `void render(ICanvas&lt; PixelType &gt; &canvas)`

Render the scene. 

canvasTarget canvas for rendering 

---

### `T * addObject(Args &&... args)`

Add an object to the scene. 

T type (must derive from ) ObjectObjectArgsConstructor argument types argsConstructor arguments Pointer to created object or nullptr if failed 

---

### `bool removeObject(Object *object)`

Remove an object from the scene. 

object to remove ObjectTrue if object was removed 

---

### `T * findObject()`

Find first object of specified type. 

T type ObjectPointer to object or nullptr if not found 

---

### ` *Object findObjectWithComponent()`

Find object with component of specified type. 

T type ComponentPointer to object or nullptr if not found 

---

### `uint32_t getId() const const`

Get scene ID. 

 identifier Scene

---

### `bool isActive() const const`

Check if scene is active. 

True if scene is active 

---

### `bool isInitialized() const const`

Check if scene is initialized. 

True if scene is initialized 

---

### ` &ObjectCollection getObjects()`

Get object collection. 

Reference to object collection 

---

### `const  &ObjectCollection getObjects() const const`

Get object collection (const). 

Const reference to object collection 

---

### `&lt;  * &gt;SignalConnectionScene connectOnCreate(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene create event. 

callbackFunction called when scene is created  connection handle Signal

---

### `&lt;  * &gt;SignalConnectionScene connectOnActivate(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene activate event. 

callbackFunction called when scene becomes active  connection handle Signal

---

### `&lt;  * &gt;SignalConnectionScene connectOnDeactivate(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene deactivate event. 

callbackFunction called when scene becomes inactive  connection handle Signal

---

### `&lt;  * &gt;SignalConnectionScene connectOnDestroy(std::function&lt; void(Scene *)&gt; callback)`

Connect to scene destroy event. 

callbackFunction called when scene is destroyed  connection handle Signal

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

### `virtual void onUpdate(uint16_t deltaTime)`

Called every frame (override in derived classes). 


Use this for scene-specific update logic that should happen before object updates. deltaTimeTime since last frame in milliseconds

---

### `virtual void onRender(ICanvas&lt; Pixel4 &gt; &canvas)`

Called during rendering for 4-bit canvas (override in derived classes). 


Use this for scene-specific rendering like backgrounds or UI overlays. canvasTarget canvas for rendering

---

### `virtual void onRender(ICanvas&lt; uint8_t &gt; &canvas)`

Called during rendering for 8-bit canvas (override in derived classes). 


Use this for scene-specific rendering like backgrounds or UI overlays. canvasTarget canvas for rendering

---

## Private Methods

### `void renderObjects(ICanvas&lt; PixelType &gt; &canvas)`

Render all objects in the scene. 

canvasTarget canvas for rendering 

---

