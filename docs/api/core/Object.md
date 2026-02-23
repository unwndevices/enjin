---
id: Object
title: Object
sidebar_label: Object
---

# Object

 base class for game entities. Object


The  class is the base class for all game entities in the Enjin system. It manages components using static allocation and provides lifecycle methods. Object

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/object.hpp`

## Public Methods

### ` Object()`

Constructor. 


        

---

### `virtual  ~Object()=default`

Virtual destructor. 


        

---

### `virtual void awake()`

Awake is called when object is created. 

Use this to ensure required components are present and initialize component relationships. 

---

### `virtual void start()`

Start is called before the first frame update. 

Use this for initialization that depends on other objects being fully set up. 

---

### `virtual void update(uint16_t deltaTime)`

Update is called once per frame. 

deltaTimeTime since last frame in milliseconds 

---

### `virtual void lateUpdate(uint16_t deltaTime)`

LateUpdate is called after all Update calls. 

deltaTimeTime since last frame in milliseconds 

---

### `bool isQueuedForRemoval() const const`

Check if object is queued for removal (matches original Enjin). 

True if object should be removed 

---

### `T * addComponent(Args &&... args)`

Add a component to this object. 

T type (must derive from ) ComponentComponentArgsConstructor argument types argsConstructor arguments Pointer to the created component or nullptr if failed 

---

### `T * getComponent()`

Get a component of specified type. 

T type ComponentPointer to component or nullptr if not found 

---

### `bool hasComponent() const const`

Check if object has a component of specified type. 

T type ComponentTrue if component exists 

---

### `bool removeComponent()`

Remove a component of specified type. 

T type ComponentTrue if component was removed 

---

### ` *C_Position getPosition() const const`

Get position component (cached for performance). 

Position component pointer or nullptr 

---

### `const  *const *C_Drawable getDrawables() const const`

Get all drawable components. 

Array of drawable component pointers 

---

### `size_t getDrawableCount() const const`

Get number of drawable components. 

Number of drawable components 

---

### ` *C_Drawable getDrawable(size_t index) const const`

Get drawable component by index. 

indexIndex of drawable component Pointer to drawable component or nullptr if invalid index 

---

### `bool isActive() const const`

Check if object is active. 

True if active 

---

### `void setActive(bool isActive)`

Set object active state. 

isActiveNew active state 

---

### `size_t getComponentCount() const const`

Get total number of components. 

 count Component

---

## Private Methods

### `std::enable_if&lt; std::is_same&lt; T,  &gt;::value &gt;::typeC_Position cachePositionIfType(T *componentPtr)`

Helper to cache position component only if T is  using SFINAE. C_Position


        

---

### `std::enable_if&lt;!std::is_same&lt; T,  &gt;::value &gt;::typeC_Position cachePositionIfType(T *componentPtr)`


        


        

---

### `void initializeComponentCache()`

Initialize cached component pointers. 


        

---

