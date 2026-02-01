---
id: ObjectCollection
title: ObjectCollection
sidebar_label: ObjectCollection
---

# ObjectCollection

Collection for managing multiple objects. 


Provides static allocation-based management of objects with lifecycle control and organized updates. 

---

**Namespace:** enjin2

**Header:** include/enjin2/core/object_collection.hpp

## Public Methods

### `cpp
* ObjectCollection()*
``

Constructor. 


        

---

### `cpp
* ~ObjectCollection()=default*
``

Destructor. 


        

---

### `cpp
*void initialize()*
``

Initialize the collection. 

Calls awake() on all objects in the collection. 

---

### `cpp
*void start()*
``

Start the collection. 

Calls  on all objects in the collection. start()classenjin2_1_1ObjectCollection_1a521f62105480089464d8bc7c13c7911emember

---

### `cpp
*void update(uint16_t deltaTime)*
``

Update all objects in the collection. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `cpp
*void lateUpdate(uint16_t deltaTime)*
``

Late update all objects in the collection. 

paramdeltaTimeTime since last frame in milliseconds 

---

### `cpp
*T * addObject(Args &&... args)*
``

Add an object to the collection. 

templateparamT type (must derive from ) Objectclassenjin2_1_1ObjectcompoundObjectclassenjin2_1_1ObjectcompoundArgsConstructor argument types paramargsConstructor arguments returnPointer to created object or nullptr if failed 

---

### `cpp
*bool removeObject(Object *object)*
``

Remove an object from the collection. 

paramobject to remove Objectclassenjin2_1_1ObjectcompoundreturnTrue if object was removed 

---

### `cpp
*T * findObject()*
``

Find first object of specified type. 

templateparamT type Objectclassenjin2_1_1ObjectcompoundreturnPointer to object or nullptr if not found 

---

### `cpp
*size_t findObjects(T **results, size_t maxResults)*
``

Find all objects of specified type. 

templateparamT type Objectclassenjin2_1_1ObjectcompoundparamresultsArray to store results maxResultsMaximum number of results returnNumber of objects found 

---

### `cpp
* *Objectclassenjin2_1_1Objectcompound findObjectWithComponent()*
``

Find object with component of specified type. 

templateparamT type Componentclassenjin2_1_1ComponentcompoundreturnPointer to object or nullptr if not found 

---

### `cpp
*void forEach(std::function&lt; void(Object *)&gt; func)*
``

Apply function to all objects. 

paramfuncFunction to apply (takes Object* parameter) 

---

### `cpp
*void forEachActive(std::function&lt; void(Object *)&gt; func)*
``

Apply function to all active objects. 

paramfuncFunction to apply (takes Object* parameter) 

---

### `cpp
*void clear()*
``

Clear all objects from the collection. 


        

---

### `cpp
*size_t size() const const*
``

Get number of objects in collection. 

return count Objectclassenjin2_1_1Objectcompound

---

### `cpp
*bool empty() const const*
``

Check if collection is empty. 

returnTrue if empty 

---

### `cpp
* *Objectclassenjin2_1_1Objectcompound getObject(size_t index)*
``

Get object at index. 

paramindex index Objectclassenjin2_1_1ObjectcompoundreturnPointer to object or nullptr if invalid index 

---

### `cpp
*const  *Objectclassenjin2_1_1Objectcompound getObject(size_t index) const const*
``

Get const object at index. 

paramindex index Objectclassenjin2_1_1ObjectcompoundreturnPointer to object or nullptr if invalid index 

---

### `cpp
*void removeInactiveObjects()*
``

Remove inactive objects from the collection. 

This compacts the array by removing objects that are not active, which can help with performance. 

---

