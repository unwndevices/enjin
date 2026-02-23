---
id: ObjectCollection
title: ObjectCollection
sidebar_label: ObjectCollection
---

# ObjectCollection

Collection for managing multiple objects. 


Provides static allocation-based management of objects with lifecycle control and organized updates. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/core/object_collection.hpp`

## Public Methods

### ` ObjectCollection()`

Constructor. 


        

---

### ` ~ObjectCollection()=default`

Destructor. 


        

---

### `void initialize()`

Initialize the collection. 

Calls awake() on all objects in the collection. 

---

### `void start()`

Start the collection. 

Calls  on all objects in the collection. start()

---

### `void update(uint16_t deltaTime)`

Update all objects in the collection. 

deltaTimeTime since last frame in milliseconds 

---

### `void lateUpdate(uint16_t deltaTime)`

Late update all objects in the collection. 

deltaTimeTime since last frame in milliseconds 

---

### `T * addObject(Args &&... args)`

Add an object to the collection. 

T type (must derive from ) ObjectObjectArgsConstructor argument types argsConstructor arguments Pointer to created object or nullptr if failed 

---

### `bool removeObject(Object *object)`

Remove an object from the collection. 

object to remove ObjectTrue if object was removed 

---

### `T * findObject()`

Find first object of specified type. 

T type ObjectPointer to object or nullptr if not found 

---

### `size_t findObjects(T **results, size_t maxResults)`

Find all objects of specified type. 

T type ObjectresultsArray to store results maxResultsMaximum number of results Number of objects found 

---

### ` *Object findObjectWithComponent()`

Find object with component of specified type. 

T type ComponentPointer to object or nullptr if not found 

---

### `void forEach(std::function&lt; void(Object *)&gt; func)`

Apply function to all objects. 

funcFunction to apply (takes Object* parameter) 

---

### `void forEachActive(std::function&lt; void(Object *)&gt; func)`

Apply function to all active objects. 

funcFunction to apply (takes Object* parameter) 

---

### `void clear()`

Clear all objects from the collection. 


        

---

### `size_t size() const const`

Get number of objects in collection. 

 count Object

---

### `bool empty() const const`

Check if collection is empty. 

True if empty 

---

### ` *Object getObject(size_t index)`

Get object at index. 

index index ObjectPointer to object or nullptr if invalid index 

---

### `const  *Object getObject(size_t index) const const`

Get const object at index. 

index index ObjectPointer to object or nullptr if invalid index 

---

### `void removeInactiveObjects()`

Remove inactive objects from the collection. 

This compacts the array by removing objects that are not active, which can help with performance. 

---

