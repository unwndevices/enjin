---
id: Object
title: Object
sidebar_label: Object
---

# Object

 base class for game entities. Objectclassenjin2_1_1Objectcompound


The  class is the base class for all game entities in the Enjin system. It manages components using static allocation and provides lifecycle methods. Objectclassenjin2_1_1Objectcompound

---

**Namespace:** enjin2

**Header:** include/enjin2/core/object.hpp

## Public Methods

### `cpp
* Object()*
``

Constructor. 


        

---

### `cpp
*virtual  ~Object()=default*
``

Virtual destructor. 


        

---

### `cpp
*virtual void awake()*
``

Awake is called when object is created. 

Use this to ensure required components are present and initialize component relationships. 

---

### `cpp
*virtual void start()*
``

Start is called before the first frame update. 

Use this for initialization that depends on other objects being fully set up. 

---

### `cpp
*virtual void update(uint16_t deltaTime)*
``

Update is called once per frame. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `cpp
*virtual void lateUpdate(uint16_t deltaTime)*
``

LateUpdate is called after all Update calls. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `cpp
*bool isQueuedForRemoval() const const*
``

Check if object is queued for removal (matches original Enjin). 

returnTrue if object should be removed 

---

### `cpp
*T * addComponent(Args &&... args)*
``

Add a component to this object. 

templateparamT type (must derive from ) Componentclassenjin2_1_1ComponentcompoundComponentclassenjin2_1_1ComponentcompoundArgsConstructor argument types paramargsConstructor arguments returnPointer to the created component or nullptr if failed 

---

### `cpp
*T * getComponent()*
``

Get a component of specified type. 

templateparamT type Componentclassenjin2_1_1ComponentcompoundreturnPointer to component or nullptr if not found 

---

### `cpp
*bool hasComponent() const const*
``

Check if object has a component of specified type. 

templateparamT type Componentclassenjin2_1_1ComponentcompoundreturnTrue if component exists 

---

### `cpp
*bool removeComponent()*
``

Remove a component of specified type. 

templateparamT type Componentclassenjin2_1_1ComponentcompoundreturnTrue if component was removed 

---

### `cpp
* *C_Positionclassenjin2_1_1C__Positioncompound getPosition() const const*
``

Get position component (cached for performance). 

returnPosition component pointer or nullptr 

---

### `cpp
*const  *const *C_Drawableclassenjin2_1_1C__Drawablecompound getDrawables() const const*
``

Get all drawable components. 

returnArray of drawable component pointers 

---

### `cpp
*size_t getDrawableCount() const const*
``

Get number of drawable components. 

returnNumber of drawable components 

---

### `cpp
* *C_Drawableclassenjin2_1_1C__Drawablecompound getDrawable(size_t index) const const*
``

Get drawable component by index. 

paramindexIndex of drawable component returnPointer to drawable component or nullptr if invalid index 

---

### `cpp
*bool isActive() const const*
``

Check if object is active. 

returnTrue if active 

---

### `cpp
*void setActive(bool isActive)*
``

Set object active state. 

paramisActiveclassenjin2_1_1Object_1a49d28edf30e20db1c42d10851fe4d63amemberNew active state 

---

### `cpp
*size_t getComponentCount() const const*
``

Get total number of components. 

return count Componentclassenjin2_1_1Componentcompound

---

## Private Methods

### `cpp
*std::enable_if&lt; std::is_same&lt; T,  &gt;::value &gt;::typeC_Positionclassenjin2_1_1C__Positioncompound cachePositionIfType(T *componentPtr)*
``

Helper to cache position component only if T is  using SFINAE. C_Positionclassenjin2_1_1C__Positioncompound


        

---

### `cpp
*std::enable_if&lt;!std::is_same&lt; T,  &gt;::value &gt;::typeC_Positionclassenjin2_1_1C__Positioncompound cachePositionIfType(T *componentPtr)*
``


        


        

---

### `cpp
*void initializeComponentCache()*
``

Initialize cached component pointers. 


        

---

